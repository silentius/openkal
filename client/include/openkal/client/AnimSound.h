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

#include "openkal/client/Audio.h"
#include "openkal/terrain/GbModel.h"

#include <vector>

namespace openkal::client {

    /**
     * Fires `(*SOUND)` cues as a GB clip's playback cursor crosses keyframes
     * (`compatibility behavior`).
     */
    class AnimSound {
      public:
        /**
         * Stops voices started by this tracker and forgets the playback cursor.
         * @param audio Mixer whose voices to stop; nullptr only resets the cursor.
         */
        void reset(Audio* audio = nullptr);

        /**
         * Stops voices started by this tracker without resetting the cursor.
         *
         * Used when an attack starts unwinding: the swing is over, but the clip has
         * not changed yet.
         *
         * @param audio Mixer whose voices to stop.
         */
        void stop(Audio& audio);

        /**
         * Plays every SOUND keyframe crossed since the previous poll.
         *
         * Animation sounds are always one-shot (`bAutoDestroy = TRUE`).
         * Looping clips wrap at `(*LOOP)` like the skinner; reverse playback (attack
         * unwind) is silent. Voices stop when the clip changes so a long WAV cannot
         * outlive the motion.
         *
         * @param audio Mixer used to play cues.
         * @param clip Clip whose `key_times` / `key_options` are scanned.
         * @param time_ms Playback cursor in milliseconds (may grow past duration when
         * looping).
         * @param looping True for idle/walk (and other looping clips).
         * @param x World X of the actor.
         * @param y World Y of the actor.
         * @param z World Z of the actor.
         */
        void poll(Audio& audio,
                  const terrain::GbAnimClip& clip,
                  float time_ms,
                  bool looping,
                  float x,
                  float y,
                  float z);

      private:
        /**
         * Plays SOUND keys with time in (prev_ms, curr_ms].
         * @param audio Mixer used to play cues.
         * @param clip Clip to scan.
         * @param prev_ms Exclusive lower bound.
         * @param curr_ms Inclusive upper bound.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void fire_range(Audio& audio,
                        const terrain::GbAnimClip& clip,
                        float prev_ms,
                        float curr_ms,
                        float x,
                        float y,
                        float z);

        /**
         * Records a voice, dropping finished handles so a live footstep is not
         * stolen.
         * @param audio Mixer used to query and stop voices.
         * @param handle Voice from play(); ignored when invalid.
         */
        void track(Audio& audio, SoundHandle handle);

        /** Wrapped cursor of the last poll, or -1 before the first sample. */
        float m_prev_ms = -1.0f;

        /** True after the first poll (or after reset until the next poll). */
        bool m_have_prev = false;

        /** Voices started for the clip now playing. */
        std::vector<SoundHandle> m_voices;
    };

}  // namespace openkal::client
