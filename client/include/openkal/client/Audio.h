/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace openkal::client {

    /** Parsed SFX name: WAV stem plus whether `_L` requested a loop. */
    class SoundCue {
      public:
        /** @return WAV stem without a trailing `_L` (e.g. "efs011"). */
        [[nodiscard]] const std::string& stem() const {
            return m_stem;
        }

        /**
         * Sets the WAV stem.
         * @param v Stem without directory or extension.
         */
        void set_stem(std::string v) {
            m_stem = std::move(v);
        }

        /** @return True when the name ended in `_L` (skill-cast loop). */
        [[nodiscard]] bool looping() const {
            return m_looping;
        }

        /**
         * Sets the looping flag.
         * @param v True to loop until stop().
         */
        void set_looping(bool v) {
            m_looping = v;
        }

        /** @return True when no stem was parsed. */
        [[nodiscard]] bool empty() const {
            return m_stem.empty();
        }

      private:
        /** WAV file stem under Sound/. */
        std::string m_stem;

        /** True when `_L` was present on the source name. */
        bool m_looping = false;
    };

    /** Opaque handle of one playing voice; 0 is invalid. */
    class SoundHandle {
      public:
        SoundHandle() = default;

        /**
         * Wraps a mixer-issued voice id.
         * @param id Non-zero id from Audio::play.
         */
        explicit SoundHandle(std::uint32_t id) : m_id(id) {}

        /** @return True when the handle refers to a mixer voice. */
        [[nodiscard]] bool valid() const {
            return m_id != 0;
        }

        /** @return Mixer voice id, or 0. */
        [[nodiscard]] std::uint32_t id() const {
            return m_id;
        }

        /** Clears the handle without stopping playback. */
        void clear() {
            m_id = 0;
        }

      private:
        /** Mixer voice id; 0 is invalid. */
        std::uint32_t m_id = 0;
    };

    /**
     * Client SFX mixer (KalOnline `XSoundSystem::PlaySound`).
     *
     * Loads `{data_root}/Sound/<stem>.wav`. Names ending in `_L` loop when played
     * through play() (skill casts). Animation keyframes always one-shot even if the
     * stem had `_L`. Missing device or files are silent — never fatal.
     */
    class Audio {
      public:
        /** World units per KalOnline METER (`XSoundSystem` listener scale). */
        static constexpr float kMeter = 10.0f;

        /** Software-3D cutoff in metres (`fMaxDistance` default 50). */
        static constexpr float kMaxDistanceM = 50.0f;

        /** Software-3D reference distance in metres (`fMinDistance` default 1). */
        static constexpr float kMinDistanceM = 1.0f;

        /** Concurrent one-shot / loop voices (`XSoundSystem::PLAY_BUFFER`). */
        static constexpr int kPlayBuffer = 8;

        Audio();

        Audio(const Audio&) = delete;

        Audio& operator=(const Audio&) = delete;

        ~Audio();

        /**
         * Parses a raw sound token (`efs011_L` or `efs011_L)`).
         * @param raw Token from skill definition, JSON, or a keyframe option.
         * @return Stem plus looping flag; empty stem when `raw` has no name.
         */
        [[nodiscard]] static SoundCue parse_name(std::string_view raw);

        /**
         * Extracts `(*SOUND name)` from a GB keyframe option string.
         * @param option `compatibility behavior` text.
         * @param out Receives the cue when a SOUND token is present.
         * @return True when a non-empty stem was found.
         */
        [[nodiscard]] static bool parse_option(std::string_view option, SoundCue& out);

        /**
         * Opens the playback device and remembers the client and packaged runtime-data roots.
         * @param data_root Client data root (holds Sound WAVs).
         * @param runtime_data_root OpenKal RuntimeData root (holds packaged sounds).
         * @return True when a device was opened. False is still a usable silent
         * mixer.
         */
        [[nodiscard]] bool init(const std::string& data_root,
                                const std::string& runtime_data_root = {});

        /** Stops every voice and closes the device. */
        void shutdown();

        /**
         * Sets the listener pose (`XSoundSystem::SetPosition` / `SetOrientation`).
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param fwd_x World-space look X (camera forward).
         * @param fwd_z World-space look Z (camera forward).
         */
        void set_listener(float x, float y, float z, float fwd_x, float fwd_z);

        /**
         * Sets the master SFX gain (no UI yet; default 1).
         * @param gain Linear gain in 0..1.
         */
        void set_effect_volume(float gain);

        /** @return Master SFX gain in 0..1. */
        [[nodiscard]] float effect_volume() const;

        /** Sets the dedicated background-music gain.
         * @param gain Linear gain in 0..1.
         */
        void set_music_volume(float gain);

        /** @return Background-music gain in 0..1. */
        [[nodiscard]] float music_volume() const;

        /** Starts or retains a looping, non-spatial background track.
         * @param stem WAV or MP3 stem below the Sound directory.
         * @return True when the track is playing or was already active.
         */
        [[nodiscard]] bool play_music(std::string_view stem);

        /** Stops the dedicated background-music voice. */
        void stop_music();

        /** @return Current background track stem, or an empty string. */
        [[nodiscard]] const std::string& music_track() const;

        /**
         * Plays a named WAV at a world position.
         * @param name Stem or `_L` token (`efs021`, `efs011_L`).
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @return Voice handle; invalid when silent, culled, or the file is missing.
         */
        [[nodiscard]] SoundHandle play(std::string_view name, float x, float y, float z);

        /**
         * Plays a cue, optionally forcing one-shot (animation auto-destroy path).
         * @param cue Parsed stem and loop flag.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         * @param one_shot True to ignore cue.looping() (GB keyframe sounds).
         * @return Voice handle; invalid when silent, culled, or the file is missing.
         */
        [[nodiscard]] SoundHandle
        play(const SoundCue& cue, float x, float y, float z, bool one_shot = false);

        /**
         * Plays a 2D UI sound at full volume, ignoring listener distance.
         * @param name Stem or `_L` token.
         * @return Voice handle; invalid when silent or the file is missing.
         */
        [[nodiscard]] SoundHandle play_ui(std::string_view name);

        /**
         * Stops a looping (or still-playing) voice.
         * @param handle Handle from play(); ignored when invalid.
         */
        void stop(SoundHandle handle);

        /**
         * @param handle Handle from play(); invalid handles are not playing.
         * @return True when the mixer still has this voice.
         */
        [[nodiscard]] bool playing(SoundHandle handle) const;

        /**
         * Reaps finished one-shots and updates spatial gain/pan of live voices.
         *
         * Call once per frame after set_listener().
         */
        void update();

        /** @return True when init() opened a playback device. */
        [[nodiscard]] bool ready() const;

        /** @return Data root passed to init(). */
        [[nodiscard]] const std::string& data_root() const;

        /**
         * Resolves packaged RuntimeData/sounds cues before compatible client audio.
         * @param data_root Client data root.
         * @param stem WAV or MP3 stem without extension.
         * @param runtime_data_root OpenKal RuntimeData root with optional packaged cues.
         * @return Resolved path, or empty when no matching audio exists.
         */
        [[nodiscard]] static std::string
        resolve_wav_path(const std::string& data_root,
                         std::string_view stem,
                         const std::string& runtime_data_root = {});

      private:
        /** Opaque miniaudio mixer state. */
        class Impl;

        /** Mixer implementation. */
        std::unique_ptr<Impl> m_impl;
    };

}  // namespace openkal::client
