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

#include "openkal/engine/engine.h"

namespace openkal::client {

    /** Interpolates remote player snapshots without interrupting locomotion between packets. */
    class RemotePlayerMotion {
      public:
        /** Places a new or recreated player immediately and clears pending movement.
         * @param position Initial world-space feet position.
         */
        void reset(const engine::Vec3& position);

        /** Accepts the latest server pose; unchanged snapshots do not restart interpolation.
         * @param position World-space feet position.
         * @param moving True until the server reports MovePlayerEnd.
         */
        void set_target(const engine::Vec3& position, bool moving);

        /** Faces a stationary combat target without moving the feet.
         * @param x Target world X.
         * @param z Target world Z.
         */
        void face_target(float x, float z);

        /** Advances position and facing using render-frame time.
         * @param dt Elapsed seconds.
         */
        void update(float dt);

        /** Returns the interpolated feet position.
         * @return World-space position.
         */
        const engine::Vec3& position() const;

        /** Returns the interpolated facing.
         * @return Yaw in radians.
         */
        float yaw() const;

        /** Keeps locomotion active until an explicit stop has reached its endpoint.
         * @return True while the server reports movement or the final segment is unfinished.
         */
        bool moving() const;

      private:
        /** Covers the 50 ms send interval plus modest packet scheduling jitter. */
        static constexpr float kInterpolationSeconds = 0.075f;
        /** Large corrections must not animate travel across the map. */
        static constexpr float kSnapDistance = 256.0f;

        engine::Vec3 m_position{};
        engine::Vec3 m_start{};
        engine::Vec3 m_target{};
        float m_elapsed = kInterpolationSeconds;
        float m_yaw = 0.0f;
        float m_target_yaw = 0.0f;
        bool m_network_moving = false;
    };

}  // namespace openkal::client
