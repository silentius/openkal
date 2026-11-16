/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/client/WeaponThumbnailCamera.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace openkal::client {
    WeaponThumbnailCamera weapon_thumbnail_camera(engine::Vec3 lo,
                                                  engine::Vec3 hi,
                                                  protocol::WeaponSkinType weapon_type,
                                                  float aspect) {
        const engine::Vec3 center{(lo.x + hi.x) * .5f, (lo.y + hi.y) * .5f, (lo.z + hi.z) * .5f};
        const std::array<float, 3> extent{hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
        int normal =
            static_cast<int>(std::min_element(extent.begin(), extent.end()) - extent.begin());
        int vertical = (normal + 1) % 3;
        for (int axis = 0; axis < 3; ++axis) {
            if (axis != normal && extent[axis] > extent[vertical]) {
                vertical = axis;
            }
        }
        float up_sign = 1.f;
        if (weapon_type == protocol::WeaponSkinType::Wand) {
            // Rigid staff heads point toward -X, regardless of ornament thickness.
            vertical = 0;
            normal = extent[1] < extent[2] ? 1 : 2;
            up_sign = -1.f;
        } else if (weapon_type == protocol::WeaponSkinType::Bow) {
            // Bows span X/Z; Y ornaments must not turn the thumbnail edge-on.
            normal = 1;
            vertical = 2;
        }
        const int horizontal = 3 - normal - vertical;
        const float radius = std::max(extent[vertical], extent[horizontal] / aspect) * .5f;
        const float distance =
            std::max(1.f, (radius / std::tan(.55f * .5f) + extent[normal] * .5f) * 1.15f);
        std::array<float, 3> eye_coords{center.x, center.y, center.z}, up_coords{};
        eye_coords[normal] += distance;
        up_coords[vertical] = up_sign;
        const engine::Vec3 eye{eye_coords[0], eye_coords[1], eye_coords[2]};
        const engine::Vec3 up{up_coords[0], up_coords[1], up_coords[2]};
        return {engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                       engine::Mat4::look_at(eye, center, up)),
                engine::Mat4::perspective(.55f, aspect, .01f, distance + extent[normal] * 2 + 10)};
    }
}  // namespace openkal::client
