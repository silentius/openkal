/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AnimSound.h"

#include "openkal/client/Audio.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    namespace {

        [[nodiscard]] float
        wrap_clip_time(const terrain::GbAnimClip& clip, float time_ms, bool looping) {
            const float dur = std::max(1.0f, clip.duration_ms());
            if (!looping) {
                return time_ms;
            }
            if (time_ms < dur) {
                return time_ms;
            }
            float start = clip.loop_start_ms;
            if (start < 0.0f || start >= dur) {
                start = 0.0f;
            }
            const float span = dur - start;
            if (span <= 1e-3f) {
                float t = std::fmod(time_ms, dur);
                if (t < 0.0f) {
                    t += dur;
                }
                return t;
            }
            float t = start + std::fmod(time_ms - start, span);
            if (t < start) {
                t += span;
            }
            return t;
        }

    }  // namespace

    void AnimSound::stop(Audio& audio) {
        for (auto& h : m_voices) {
            audio.stop(h);
        }
        m_voices.clear();
    }

    void AnimSound::reset(Audio* audio) {
        if (audio) {
            stop(*audio);
        }
        m_prev_ms = -1.0f;
        m_have_prev = false;
    }

    void AnimSound::track(Audio& audio, SoundHandle handle) {
        if (!handle.valid()) {
            return;
        }
        m_voices.erase(std::remove_if(m_voices.begin(),
                                      m_voices.end(),
                                      [&](SoundHandle h) { return !audio.playing(h); }),
                       m_voices.end());
        if (m_voices.size() >= static_cast<std::size_t>(Audio::kPlayBuffer)) {
            audio.stop(m_voices.front());
            m_voices.erase(m_voices.begin());
        }
        m_voices.push_back(handle);
    }

    void AnimSound::fire_range(Audio& audio,
                               const terrain::GbAnimClip& clip,
                               float prev_ms,
                               float curr_ms,
                               float x,
                               float y,
                               float z) {
        const std::size_t n = std::min(clip.key_times.size(), clip.key_options.size());
        for (std::size_t i = 0; i < n; ++i) {
            const float t = static_cast<float>(clip.key_times[i]);
            if (!(prev_ms < t && t <= curr_ms)) {
                continue;
            }
            SoundCue cue;
            if (!Audio::parse_option(clip.key_options[i], cue)) {
                continue;
            }
            track(audio, audio.play(cue, x, y, z, true));
        }
    }

    void AnimSound::poll(Audio& audio,
                         const terrain::GbAnimClip& clip,
                         float time_ms,
                         bool looping,
                         float x,
                         float y,
                         float z) {
        if (clip.key_times.empty() || clip.key_options.empty()) {
            m_prev_ms = time_ms;
            m_have_prev = true;
            return;
        }
        const float duration = std::max(1.0f, clip.duration_ms());
        float loop_start = 0.0f;
        if (looping) {
            loop_start = clip.loop_start_ms;
            if (loop_start < 0.0f || loop_start >= duration) {
                loop_start = 0.0f;
            }
        }
        const float curr = wrap_clip_time(clip, time_ms, looping);
        if (!m_have_prev) {
            fire_range(audio, clip, -1.0f, curr, x, y, z);
            m_prev_ms = curr;
            m_have_prev = true;
            return;
        }
        if (looping && curr + 0.5f < m_prev_ms) {
            fire_range(audio, clip, m_prev_ms, duration + 1.0f, x, y, z);
            fire_range(audio, clip, loop_start - 1.0f, curr, x, y, z);
        } else if (curr >= m_prev_ms) {
            fire_range(audio, clip, m_prev_ms, curr, x, y, z);
        }
        m_prev_ms = curr;
    }

}  // namespace openkal::client
