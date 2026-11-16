/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/engine/engine.h"
#include "openkal/map/Constants.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::area_editor {

    /** Height above an OPL instance origin used for automatic object footprints. */
    inline constexpr float kObjectMaskHeadHeight = 180.0f;

    /** One source mesh used to derive an object's top-down outlines. */
    struct ObjectMaskMesh {
        std::vector<engine::Vec3> positions;
        std::vector<std::uint32_t> indices;
        bool alpha_surface = false;
    };

    /** Point in editable subcell coordinates. */
    struct ObjectMaskPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    /** Pick outline and head-height footprint for one OPL object instance. */
    struct ObjectMask {
        std::string name;
        std::vector<std::uint32_t> footprint_cells;
        /** Display-silhouette area in subcells. Smaller areas win object picking. */
        float pick_area = 0.0f;
        /** Inclusive footprint bounds. `max_x < min_x` means the footprint is empty. */
        int min_x = 0;
        int min_y = 0;
        int max_x = -1;
        int max_y = -1;
        /** Closed collision loops in subcell coordinates. */
        std::vector<std::vector<ObjectMaskPoint>> collision_contours;
        /** Closed full-projection loops, including roofs and foliage. */
        std::vector<std::vector<ObjectMaskPoint>> display_contours;
    };

    /** Builds a top-down display outline and a filled head-height footprint.
     *
     * The footprint is the union of opaque surfaces at or below head height, including the
     * interior of vertical walls. It is not a convex hull, so courtyards and L-shaped buildings
     * keep their notches. Alpha foliage is omitted from the footprint when `foliage` is set, but
     * it still contributes to the display contour.
     *
     * @param name Display name of the OPL object.
     * @param meshes Model-space source geometry.
     * @param model Model-to-world transform for the OPL instance.
     * @param patch_origin_x World X coordinate at the patch's left edge.
     * @param patch_origin_z World Z coordinate at the patch's top edge.
     * @param patch_extent World-space width and height of the patch.
     * @param base_world_y Ground height of the OPL instance.
     * @param foliage Whether alpha-tested foliage surfaces should be ignored for collision.
     * @return Generated mask; its footprint is empty when no usable collision surface exists.
     */
    [[nodiscard]] ObjectMask build_object_mask(const std::string& name,
                                               const std::vector<ObjectMaskMesh>& meshes,
                                               const engine::Mat4& model,
                                               float patch_origin_x,
                                               float patch_origin_z,
                                               float patch_extent,
                                               float base_world_y,
                                               bool foliage);

    /** Finds the most specific object under a subcell.
     *
     * A hit is either a filled collision cell, including the interior of wall-only buildings, or a
     * point inside the display silhouette. The latter keeps tree crowns pickable. The smallest
     * display area wins when several objects overlap. Objects with an empty footprint are ignored.
     *
     * @param masks Object masks generated for the current patch.
     * @param subcell_x Horizontal editable subcell coordinate.
     * @param subcell_y Vertical editable subcell coordinate.
     * @return Matching mask, or nullptr when the cursor is not over an object.
     */
    [[nodiscard]] const ObjectMask*
    pick_object_mask(const std::vector<ObjectMask>& masks, int subcell_x, int subcell_y);

    /** Returns footprint subcells that lie inside an inclusive subcell rectangle.
     *
     * @param mask Object mask to clip.
     * @param min_x Left subcell, inclusive.
     * @param min_y Top subcell, inclusive.
     * @param max_x Right subcell, inclusive.
     * @param max_y Bottom subcell, inclusive.
     * @return Matching footprint subcells. Empty when the rectangle misses the mask.
     */
    [[nodiscard]] std::vector<std::uint32_t>
    clip_footprint_to_rect(const ObjectMask& mask, int min_x, int min_y, int max_x, int max_y);

    /** Lists every editable subcell inside an inclusive rectangle.
     *
     * @param min_x Left subcell, inclusive.
     * @param min_y Top subcell, inclusive.
     * @param max_x Right subcell, inclusive.
     * @param max_y Bottom subcell, inclusive.
     * @return Subcell indices clamped to the patch. Empty when the rectangle is invalid.
     */
    [[nodiscard]] std::vector<std::uint32_t>
    subcells_in_rect(int min_x, int min_y, int max_x, int max_y);

}  // namespace openkal::area_editor
