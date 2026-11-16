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
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Owner movement values needed by the independent pet controller. */
    struct PetOwnerState {
        /** Owner world X. */
        float x = 0.0f;

        /** Owner world Z. */
        float z = 0.0f;

        /** Owner facing angle in radians. */
        float yaw = 0.0f;

        /** Owner walk speed in world units per second. */
        float walk_speed = 40.0f;

        /** Owner run speed in world units per second. */
        float run_speed = 60.0f;
    };

    /** One visible ground item that an auto-pickup pet may chase. */
    struct PetPickupTarget {
        /** Ground-item world entity id. */
        std::uint32_t id = 0;

        /** Ground-item world X. */
        float x = 0.0f;

        /** Ground-item world Z. */
        float z = 0.0f;
    };

    /** Per-frame outcome of independent pet movement. */
    struct PetUpdateResult {
        /** True when the pet changed position this frame. */
        bool moving = false;

        /** True when the run animation should be used. */
        bool running = false;

        /** Item reached this frame; zero means no pickup request. */
        std::uint32_t pickup_item_id = 0;
    };

    /** Drives a pet as an independent follower and auto-pickup actor. */
    class PetController {
      public:
        /**
         * Places the pet behind its owner and clears movement targets.
         * @param owner Current owner pose and movement speeds.
         */
        void reset(const PetOwnerState& owner);

        /** Removes the active pet position and all pending targets. */
        void clear();

        /**
         * Advances following, pickup chasing, and turn smoothing.
         * @param dt Frame duration in seconds.
         * @param owner Current owner pose and movement speeds.
         * @param auto_pickup True when the equipped pet may chase ground items.
         * @param items Visible ground items available for target selection.
         * @return Movement and pickup action for this frame.
         */
        [[nodiscard]] PetUpdateResult update(float dt,
                                             const PetOwnerState& owner,
                                             bool auto_pickup,
                                             const std::vector<PetPickupTarget>& items);

        /** @return True after the controller has been placed in the world. */
        [[nodiscard]] bool has_position() const {
            return m_has_position;
        }

        /** @return Pet world X. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Pet world Z. */
        [[nodiscard]] float z() const {
            return m_z;
        }

        /** @return Smoothed pet facing angle in radians. */
        [[nodiscard]] float yaw() const {
            return m_yaw;
        }

        /** @return Ground-item id currently being chased, or zero. */
        [[nodiscard]] std::uint32_t pickup_target_id() const {
            return m_pickup_target_id;
        }

      private:
        void update_retry_cooldowns(float dt);
        [[nodiscard]] const PetPickupTarget*
        find_pickup_target(const PetOwnerState& owner,
                           const std::vector<PetPickupTarget>& items) const;
        [[nodiscard]] bool
        move_toward(float dt, float target_x, float target_z, float stop_distance, float speed);

        bool m_has_position = false;
        bool m_following = false;
        float m_x = 0.0f;
        float m_z = 0.0f;
        float m_yaw = 0.0f;
        std::uint32_t m_pickup_target_id = 0;
        std::unordered_map<std::uint32_t, float> m_pickup_retry_seconds;
    };

}  // namespace openkal::client
