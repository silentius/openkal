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

#include "openkal/client/Floor.h"
#include "openkal/client/Player.h"

namespace openkal::client {

    /** Ground pick helpers for click-to-move against FloorField. */
    class WorldPick {
      public:
        /** World-space height of the monster pick AABB at model scale 1. */
        static constexpr float kMonsterBaseHeight = 32.0f;

        /** World-space XZ half-extent of the monster pick AABB at model scale 1. */
        static constexpr float kMonsterHalfExtent = 16.0f;

        /** Fallback NPC pick height when the clothes mesh has no AABB. */
        static constexpr float kNpcBaseHeight = 18.0f;

        /** Fallback NPC pick XZ half-extent when the clothes mesh has no AABB. */
        static constexpr float kNpcHalfExtent = 4.5f;

        /**
         * Unprojects a screen point and marches against floor heights.
         * @param cam Follow camera.
         * @param pl Local player (for eye/fallback height).
         * @param floor Walkable floor.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param sw Screen width.
         * @param sh Screen height.
         * @param out_x World X hit.
         * @param out_z World Z hit.
         * @return True on a hit.
         */
        [[nodiscard]] static bool pick_ground(const FollowCamera& cam,
                                              const LocalPlayer& pl,
                                              const FloorField& floor,
                                              float mx,
                                              float my,
                                              float sw,
                                              float sh,
                                              float& out_x,
                                              float& out_z);

        /**
         * Projects a world point with the same view/proj as the GL renderer.
         * @param cam Follow camera.
         * @param pl Local player.
         * @param world_x World X.
         * @param world_y World Y.
         * @param world_z World Z.
         * @param screen_w Screen width.
         * @param screen_h Screen height.
         * @param screen_x Out screen X.
         * @param screen_y Out screen Y.
         * @return True when the point is in front of the camera.
         */
        [[nodiscard]] static bool project_world_to_screen(const FollowCamera& cam,
                                                          const LocalPlayer& pl,
                                                          float world_x,
                                                          float world_y,
                                                          float world_z,
                                                          float screen_w,
                                                          float screen_h,
                                                          float& screen_x,
                                                          float& screen_y);

        /**
         * Converts a model-space AABB into a feet-centered pick box.
         * Leaves half_extent and height unchanged when the AABB is degenerate.
         * @param min_x Model-space AABB minimum X.
         * @param min_y Model-space AABB minimum Y.
         * @param min_z Model-space AABB minimum Z.
         * @param max_x Model-space AABB maximum X.
         * @param max_y Model-space AABB maximum Y.
         * @param max_z Model-space AABB maximum Z.
         * @param scale Uniform draw scale.
         * @param half_extent Out XZ half-size.
         * @param height Out box height from feet.
         */
        static void size_from_aabb(float min_x,
                                   float min_y,
                                   float min_z,
                                   float max_x,
                                   float max_y,
                                   float max_z,
                                   float scale,
                                   float& half_extent,
                                   float& height);

        /**
         * Ray-tests a feet-centered monster pick AABB.
         * @param cam Follow camera.
         * @param pl Local player.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param screen_w Screen width.
         * @param screen_h Screen height.
         * @param world_x Monster world X.
         * @param base_y Monster feet world Y.
         * @param world_z Monster world Z.
         * @param half_extent XZ half-extent in world units.
         * @param height AABB height in world units.
         * @param out_t Distance along the view ray to the nearest hit (eye units).
         * @return True when the ray intersects the AABB.
         */
        [[nodiscard]] static bool pick_monster_box(const FollowCamera& cam,
                                                   const LocalPlayer& pl,
                                                   float mx,
                                                   float my,
                                                   float screen_w,
                                                   float screen_h,
                                                   float world_x,
                                                   float base_y,
                                                   float world_z,
                                                   float half_extent,
                                                   float height,
                                                   float& out_t);
    };

}  // namespace openkal::client
