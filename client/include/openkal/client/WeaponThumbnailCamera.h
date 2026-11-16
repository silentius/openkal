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
#include "openkal/protocol/Skin.h"

namespace openkal::client {
    /** Camera matrices for a detached weapon or shield thumbnail. */
    struct WeaponThumbnailCamera {
        engine::Mat4 view;
        engine::Mat4 projection;
    };

    /** Fits a thumbnail while preserving the weapon's authored orientation.
     * @param lo Minimum model coordinates, excluding effect meshes.
     * @param hi Maximum model coordinates, excluding effect meshes.
     * @param weapon_type Cosmetic weapon type, or None for shields.
     * @param aspect Positive thumbnail width divided by height.
     * @return View and perspective matrices enclosing the model bounds.
     */
    [[nodiscard]] WeaponThumbnailCamera weapon_thumbnail_camera(
        engine::Vec3 lo, engine::Vec3 hi, protocol::WeaponSkinType weapon_type, float aspect);
}  // namespace openkal::client
