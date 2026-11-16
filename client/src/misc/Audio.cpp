/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Audio.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace openkal::client {
    namespace {

        constexpr float kDsHundredthsPerDb = 100.0f;
        constexpr float kDsVolumeMin = -10000.0f;
        constexpr float kAttenScale = -301.03f * 0.3f;
        constexpr float kPanScale = 6.0f * 301.03f;

        [[nodiscard]] std::string_view trim_view(std::string_view s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
                s.remove_prefix(1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == ')' ||
                                  s.back() == '\r' || s.back() == '\n')) {
                s.remove_suffix(1);
            }
            return s;
        }

        [[nodiscard]] float ds_hundredths_to_linear(float hundredths) {
            const float db = hundredths / kDsHundredthsPerDb;
            return std::pow(10.0f, db / 20.0f);
        }

    }  // namespace

    class Audio::Impl {
      public:
        struct Voice {
            ma_sound sound{};
            ma_decoder decoder{};
            std::vector<std::uint8_t> encoded_data;
            std::uint32_t handle = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            bool looping = false;
            bool spatial = true;
            bool used = false;
            bool decoder_used = false;
        };

        std::string data_root;
        std::string runtime_data_root;
        ma_engine engine{};
        bool engine_ok = false;
        float effect_gain = 1.0f;
        float music_gain = 1.0f;
        ma_sound music{};
        ma_decoder music_decoder{};
        std::vector<std::uint8_t> music_encoded_data;
        bool music_used = false;
        bool music_decoder_used = false;
        std::string music_track;
        float listen_x = 0.0f;
        float listen_y = 0.0f;
        float listen_z = 0.0f;
        float fwd_x = 0.0f;
        float fwd_z = 1.0f;
        std::uint32_t next_handle = 1;
        std::array<Voice, Audio::kPlayBuffer> voices{};

        void uninit_voice(Voice& v) {
            if (v.used) {
                ma_sound_uninit(&v.sound);
                v.used = false;
            }
            if (v.decoder_used) {
                ma_decoder_uninit(&v.decoder);
                v.decoder_used = false;
            }
            v.encoded_data.clear();
            v.handle = 0;
            v.looping = false;
        }

        [[nodiscard]] bool init_sound(ma_sound& sound,
                                      ma_decoder& decoder,
                                      bool& decoder_used,
                                      std::vector<std::uint8_t>& encoded_data,
                                      const std::string& path,
                                      ma_uint32 flags) {
            auto bytes = platform::File::read(path);
            if (!bytes) {
                return false;
            }
            encoded_data = std::move(*bytes);
            if (ma_decoder_init_memory(
                    encoded_data.data(), encoded_data.size(), nullptr, &decoder) != MA_SUCCESS) {
                encoded_data.clear();
                return false;
            }
            decoder_used = true;
            if (ma_sound_init_from_data_source(&engine, &decoder, flags, nullptr, &sound) !=
                MA_SUCCESS) {
                ma_decoder_uninit(&decoder);
                decoder_used = false;
                encoded_data.clear();
                return false;
            }
            return true;
        }

        void uninit_music() {
            if (music_used) {
                ma_sound_uninit(&music);
                music_used = false;
            }
            if (music_decoder_used) {
                ma_decoder_uninit(&music_decoder);
                music_decoder_used = false;
            }
            music_encoded_data.clear();
            music_track.clear();
        }

        void shutdown() {
            uninit_music();
            for (auto& v : voices) {
                uninit_voice(v);
            }
            if (engine_ok) {
                ma_engine_uninit(&engine);
                engine_ok = false;
            }
        }

        [[nodiscard]] Voice* find_handle(std::uint32_t id) {
            if (id == 0) {
                return nullptr;
            }
            for (auto& v : voices) {
                if (v.used && v.handle == id) {
                    return &v;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const Voice* find_handle(std::uint32_t id) const {
            if (id == 0) {
                return nullptr;
            }
            for (const auto& v : voices) {
                if (v.used && v.handle == id) {
                    return &v;
                }
            }
            return nullptr;
        }

        [[nodiscard]] Voice* acquire_slot() {
            for (auto& v : voices) {
                if (!v.used) {
                    return &v;
                }
                if (!v.looping && !ma_sound_is_playing(&v.sound)) {
                    uninit_voice(v);
                    return &v;
                }
            }
            // Steal the oldest one-shot; never steal a looping skill cast if a one-shot
            // exists.
            Voice* steal = nullptr;
            for (auto& v : voices) {
                if (v.used && !v.looping) {
                    steal = &v;
                    break;
                }
            }
            if (!steal) {
                steal = &voices.front();
            }
            uninit_voice(*steal);
            return steal;
        }

        void apply_spatial(Voice& v) const {
            if (!v.used) {
                return;
            }
            float gain = effect_gain;
            float pan = 0.0f;
            if (v.spatial) {
                const float mx = (v.x - listen_x) / Audio::kMeter;
                const float my = (v.y - listen_y) / Audio::kMeter;
                const float mz = (v.z - listen_z) / Audio::kMeter;
                const float dist_sq = mx * mx + my * my + mz * mz;
                const float max_sq = Audio::kMaxDistanceM * Audio::kMaxDistanceM;
                if (dist_sq > max_sq) {
                    ma_sound_set_volume(&v.sound, 0.0f);
                    return;
                }
                const float dist = std::sqrt(std::max(dist_sq, 0.0001f));
                if (dist > Audio::kMinDistanceM) {
                    const float hundredths = (dist / Audio::kMinDistanceM) * kAttenScale;
                    if (hundredths <= kDsVolumeMin) {
                        ma_sound_set_volume(&v.sound, 0.0f);
                        return;
                    }
                    gain *= ds_hundredths_to_linear(hundredths);
                    float ox = fwd_x;
                    float oz = fwd_z;
                    const float olen = std::sqrt(ox * ox + oz * oz);
                    if (olen > 1e-4f) {
                        ox /= olen;
                        oz /= olen;
                    } else {
                        ox = 0.0f;
                        oz = 1.0f;
                    }
                    const float dot = oz * mx - ox * mz;
                    const float pan_h = dot * kPanScale / dist;
                    pan = std::clamp(pan_h / 10000.0f, -1.0f, 1.0f);
                }
            }
            ma_sound_set_volume(&v.sound, std::max(0.0f, gain));
            ma_sound_set_pan(&v.sound, pan);
        }
    };

    SoundCue Audio::parse_name(std::string_view raw) {
        SoundCue cue;
        std::string buf(trim_view(raw));
        if (buf.empty()) {
            return cue;
        }
        // PlaySound strips a trailing ')' then tests the last two chars for
        // "_L".
        if (!buf.empty() && buf.back() == ')') {
            buf.pop_back();
        }
        if (buf.size() >= 2) {
            const char a =
                static_cast<char>(std::tolower(static_cast<unsigned char>(buf[buf.size() - 2])));
            const char b =
                static_cast<char>(std::tolower(static_cast<unsigned char>(buf[buf.size() - 1])));
            if (a == '_' && b == 'l') {
                buf.resize(buf.size() - 2);
                cue.set_looping(true);
            }
        }
        cue.set_stem(std::move(buf));
        return cue;
    }

    bool Audio::parse_option(std::string_view option, SoundCue& out) {
        out = SoundCue{};
        const auto pos = option.find("(*SOUND");
        if (pos == std::string_view::npos) {
            return false;
        }
        auto rest = option.substr(pos + 7);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.remove_prefix(1);
        }
        std::size_t n = 0;
        while (n < rest.size() && rest[n] != ')' && rest[n] != ' ' && rest[n] != '\t') {
            ++n;
        }
        out = parse_name(rest.substr(0, n));
        return !out.empty();
    }

    std::string Audio::resolve_wav_path(const std::string& data_root,
                                        std::string_view stem,
                                        const std::string& runtime_data_root) {
        if (stem.empty()) {
            return {};
        }
        auto try_resolve = [&](std::string_view folder, const char* ext) -> std::string {
            std::string rel;
            rel.reserve(folder.size() + stem.size() + 1);
            rel.append(folder.data(), folder.size());
            rel.append(stem.data(), stem.size());
            return platform::resolve_client_asset(data_root, rel, ext);
        };
        auto try_runtime = [&](const char* ext) -> std::string {
            if (runtime_data_root.empty()) {
                return {};
            }
            std::string rel = "sounds/";
            rel.append(stem.data(), stem.size());
            rel += ext;
            return platform::Path::resolve_path_case(runtime_data_root, rel);
        };
        // Package-owned sounds override compatible client assets, like RuntimeData/HyperText icons.
        std::string path = try_runtime(".wav");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        path = try_runtime(".mp3");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        // Prefer Sound/*.wav first, then accept lower-case folder and mp3.
        path = try_resolve("Sound/", ".wav");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        path = try_resolve("sounds/", ".wav");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        path = try_resolve("Sound/", ".mp3");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        path = try_resolve("sounds/", ".mp3");
        if (!path.empty() && platform::File::exists(path)) {
            return path;
        }
        return {};
    }

    Audio::Audio() : m_impl(std::make_unique<Impl>()) {}

    Audio::~Audio() {
        shutdown();
    }

    bool Audio::init(const std::string& data_root, const std::string& runtime_data_root) {
        shutdown();
        m_impl = std::make_unique<Impl>();
        m_impl->data_root = data_root;
        m_impl->runtime_data_root = runtime_data_root;
        ma_engine_config cfg = ma_engine_config_init();
        cfg.listenerCount = 1;
        const ma_result r = ma_engine_init(&cfg, &m_impl->engine);
        if (r != MA_SUCCESS) {
            log::warn("Audio: no playback device (SFX silent)");
            m_impl->engine_ok = false;
            return false;
        }
        m_impl->engine_ok = true;
        return true;
    }

    void Audio::shutdown() {
        if (!m_impl) {
            return;
        }
        m_impl->shutdown();
    }

    void Audio::set_listener(float x, float y, float z, float fwd_x, float fwd_z) {
        if (!m_impl) {
            return;
        }
        m_impl->listen_x = x;
        m_impl->listen_y = y;
        m_impl->listen_z = z;
        m_impl->fwd_x = fwd_x;
        m_impl->fwd_z = fwd_z;
    }

    void Audio::set_effect_volume(float gain) {
        if (!m_impl) {
            return;
        }
        m_impl->effect_gain = std::clamp(gain, 0.0f, 1.0f);
    }

    float Audio::effect_volume() const {
        return m_impl ? m_impl->effect_gain : 1.0f;
    }

    void Audio::set_music_volume(float gain) {
        if (!m_impl) {
            return;
        }
        m_impl->music_gain = std::clamp(gain, 0.0f, 1.0f);
        if (m_impl->music_used) {
            ma_sound_set_volume(&m_impl->music, m_impl->music_gain);
        }
    }

    float Audio::music_volume() const {
        return m_impl ? m_impl->music_gain : 1.0f;
    }

    bool Audio::play_music(std::string_view stem) {
        if (stem.empty() || !m_impl || !m_impl->engine_ok) {
            return false;
        }
        if (m_impl->music_used && m_impl->music_track == stem &&
            ma_sound_is_playing(&m_impl->music)) {
            return true;
        }
        const std::string path =
            resolve_wav_path(m_impl->data_root, stem, m_impl->runtime_data_root);
        if (path.empty()) {
            log::warn("Audio: missing BGM " + std::string(stem));
            return false;
        }
        stop_music();
        const ma_uint32 flags =
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (!m_impl->init_sound(m_impl->music,
                                m_impl->music_decoder,
                                m_impl->music_decoder_used,
                                m_impl->music_encoded_data,
                                path,
                                flags)) {
            log::warn("Audio: failed to load BGM " + path);
            return false;
        }
        ma_sound_set_looping(&m_impl->music, MA_TRUE);
        if (ma_data_source* source = ma_sound_get_data_source(&m_impl->music)) {
            (void) ma_data_source_set_looping(source, MA_TRUE);
        }
        ma_sound_set_spatialization_enabled(&m_impl->music, MA_FALSE);
        ma_sound_set_volume(&m_impl->music, m_impl->music_gain);
        if (ma_sound_start(&m_impl->music) != MA_SUCCESS) {
            m_impl->uninit_music();
            return false;
        }
        m_impl->music_used = true;
        m_impl->music_track = stem;
        return true;
    }

    void Audio::stop_music() {
        if (!m_impl) {
            return;
        }
        if (m_impl->music_used) {
            ma_sound_stop(&m_impl->music);
        }
        m_impl->uninit_music();
    }

    const std::string& Audio::music_track() const {
        static const std::string empty;
        return m_impl ? m_impl->music_track : empty;
    }

    bool Audio::ready() const {
        return m_impl && m_impl->engine_ok;
    }

    const std::string& Audio::data_root() const {
        static const std::string empty;
        return m_impl ? m_impl->data_root : empty;
    }

    SoundHandle Audio::play(std::string_view name, float x, float y, float z) {
        return play(parse_name(name), x, y, z, false);
    }

    SoundHandle Audio::play_ui(std::string_view name) {
        SoundCue cue = parse_name(name);
        if (cue.empty() || !m_impl || !m_impl->engine_ok) {
            return {};
        }
        const std::string path =
            resolve_wav_path(m_impl->data_root, cue.stem(), m_impl->runtime_data_root);
        if (path.empty() || !platform::File::exists(path)) {
            return {};
        }
        Impl::Voice* slot = m_impl->acquire_slot();
        const ma_uint32 flags =
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (!m_impl->init_sound(
                slot->sound, slot->decoder, slot->decoder_used, slot->encoded_data, path, flags)) {
            return {};
        }
        ma_sound_set_looping(&slot->sound, cue.looping() ? MA_TRUE : MA_FALSE);
        if (ma_data_source* ds = ma_sound_get_data_source(&slot->sound)) {
            (void) ma_data_source_set_looping(ds, cue.looping() ? MA_TRUE : MA_FALSE);
        }
        ma_sound_set_spatialization_enabled(&slot->sound, MA_FALSE);
        slot->used = true;
        slot->looping = cue.looping();
        slot->spatial = false;
        slot->x = m_impl->listen_x;
        slot->y = m_impl->listen_y;
        slot->z = m_impl->listen_z;
        slot->handle = m_impl->next_handle++;
        if (m_impl->next_handle == 0) {
            m_impl->next_handle = 1;
        }
        ma_sound_set_volume(&slot->sound, m_impl->effect_gain);
        ma_sound_set_pan(&slot->sound, 0.0f);
        if (ma_sound_start(&slot->sound) != MA_SUCCESS) {
            m_impl->uninit_voice(*slot);
            return {};
        }
        return SoundHandle{slot->handle};
    }

    SoundHandle Audio::play(const SoundCue& cue, float x, float y, float z, bool one_shot) {
        if (cue.empty() || !m_impl || !m_impl->engine_ok || m_impl->effect_gain <= 0.0f) {
            return {};
        }
        const bool looping = !one_shot && cue.looping();
        if (!looping) {
            const float mx = (x - m_impl->listen_x) / kMeter;
            const float my = (y - m_impl->listen_y) / kMeter;
            const float mz = (z - m_impl->listen_z) / kMeter;
            const float dist_sq = mx * mx + my * my + mz * mz;
            if (dist_sq > kMaxDistanceM * kMaxDistanceM) {
                return {};
            }
        }
        const std::string path =
            resolve_wav_path(m_impl->data_root, cue.stem(), m_impl->runtime_data_root);
        if (path.empty() || !platform::File::exists(path)) {
            return {};
        }
        Impl::Voice* slot = m_impl->acquire_slot();
        ma_uint32 flags =
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (!m_impl->init_sound(
                slot->sound, slot->decoder, slot->decoder_used, slot->encoded_data, path, flags)) {
            log::debug("Audio: failed to load " + path);
            return {};
        }
        ma_sound_set_looping(&slot->sound, looping ? MA_TRUE : MA_FALSE);
        if (ma_data_source* ds = ma_sound_get_data_source(&slot->sound)) {
            (void) ma_data_source_set_looping(ds, looping ? MA_TRUE : MA_FALSE);
        }
        ma_sound_set_spatialization_enabled(&slot->sound, MA_FALSE);
        slot->used = true;
        slot->looping = looping;
        slot->spatial = true;
        slot->x = x;
        slot->y = y;
        slot->z = z;
        slot->handle = m_impl->next_handle++;
        if (m_impl->next_handle == 0) {
            m_impl->next_handle = 1;
        }
        m_impl->apply_spatial(*slot);
        if (ma_sound_start(&slot->sound) != MA_SUCCESS) {
            m_impl->uninit_voice(*slot);
            return {};
        }
        return SoundHandle{slot->handle};
    }

    void Audio::stop(SoundHandle handle) {
        if (!m_impl || !handle.valid()) {
            return;
        }
        Impl::Voice* v = m_impl->find_handle(handle.id());
        if (v) {
            ma_sound_stop(&v->sound);
            m_impl->uninit_voice(*v);
        }
    }

    bool Audio::playing(SoundHandle handle) const {
        if (!m_impl || !handle.valid()) {
            return false;
        }
        return m_impl->find_handle(handle.id()) != nullptr;
    }

    void Audio::update() {
        if (!m_impl || !m_impl->engine_ok) {
            return;
        }
        for (auto& v : m_impl->voices) {
            if (!v.used) {
                continue;
            }
            if (!v.looping && !ma_sound_is_playing(&v.sound)) {
                m_impl->uninit_voice(v);
                continue;
            }
            m_impl->apply_spatial(v);
        }
    }

}  // namespace openkal::client
