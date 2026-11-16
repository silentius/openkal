/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/HeightField.h"

#include "openkal/map/Constants.h"

#include <cmath>

namespace openkal::terrain {

    const HeightPatch* HeightField::find_patch(int patch_x, int patch_y) const {
        for (const auto& p : patches) {
            if (p.patch_x == patch_x && p.patch_y == patch_y) {
                return &p;
            }
        }
        return nullptr;
    }

    const HeightPatch* HeightField::covering_patch(float world_x, float world_z) const {
        if (patches.empty()) {
            return nullptr;
        }
        const float cell_x = world_x / static_cast<float>(map::kTerrainCellSize);
        const float cell_z = world_z / static_cast<float>(map::kTerrainCellSize);
        const int patch_x =
            static_cast<int>(std::floor(cell_x / static_cast<float>(map::kMapSize))) +
            map::kPatchGrid / 2;
        const int patch_y =
            static_cast<int>(std::floor(cell_z / static_cast<float>(map::kMapSize))) +
            map::kPatchGrid / 2;
        const HeightPatch* patch = find_patch(patch_x, patch_y);
        if (!patch || patch->heights.size() !=
                          static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples) {
            return nullptr;
        }
        return patch;
    }

    bool HeightField::covers(float world_x, float world_z) const {
        return covering_patch(world_x, world_z) != nullptr;
    }

    float HeightField::sample_raw(const HeightPatch& p, int lx, int lz) const {
        const int n = map::kHeightSamples;
        if (lx < 0) {
            lx = 0;
        }
        if (lz < 0) {
            lz = 0;
        }
        if (lx >= n) {
            lx = n - 1;
        }
        if (lz >= n) {
            lz = n - 1;
        }
        return static_cast<float>(
            p.heights[static_cast<std::size_t>(lz) * n + static_cast<std::size_t>(lx)]);
    }

    float HeightField::sample(float world_x, float world_z, float fallback) const {
        const HeightPatch* patch = covering_patch(world_x, world_z);
        if (!patch) {
            return fallback;
        }
        const float cell_x = world_x / static_cast<float>(map::kTerrainCellSize);
        const float cell_z = world_z / static_cast<float>(map::kTerrainCellSize);
        const int patch_x = patch->patch_x;
        const int patch_y = patch->patch_y;

        float local_x =
            cell_x - static_cast<float>((patch_x - map::kPatchGrid / 2) * map::kMapSize);
        float local_z =
            cell_z - static_cast<float>((patch_y - map::kPatchGrid / 2) * map::kMapSize);
        int ix = static_cast<int>(std::floor(local_x));
        int iz = static_cast<int>(std::floor(local_z));
        float fx = local_x - static_cast<float>(ix);
        float fz = local_z - static_cast<float>(iz);

        // Triangle split matches roam Terrain::GetHeight ((ix+iz)&1).
        if ((ix + iz) & 1) {
            if (fx + fz <= 1.0f) {
                const float h00 = sample_raw(*patch, ix, iz);
                const float h10 = sample_raw(*patch, ix + 1, iz);
                const float h01 = sample_raw(*patch, ix, iz + 1);
                return h00 + (h10 - h00) * fx + (h01 - h00) * fz;
            }
            fx = 1.0f - fx;
            fz = 1.0f - fz;
            const float h11 = sample_raw(*patch, ix + 1, iz + 1);
            const float h10 = sample_raw(*patch, ix + 1, iz);
            const float h01 = sample_raw(*patch, ix, iz + 1);
            return h11 + (h01 - h11) * fx + (h10 - h11) * fz;
        }
        if (fz < fx) {
            fx = 1.0f - fx;
            const float h10 = sample_raw(*patch, ix + 1, iz);
            const float h00 = sample_raw(*patch, ix, iz);
            const float h11 = sample_raw(*patch, ix + 1, iz + 1);
            return h10 + (h00 - h10) * fx + (h11 - h10) * fz;
        }
        fz = 1.0f - fz;
        const float h01 = sample_raw(*patch, ix, iz + 1);
        const float h00 = sample_raw(*patch, ix, iz);
        const float h11 = sample_raw(*patch, ix + 1, iz + 1);
        return h01 + (h11 - h01) * fx + (h00 - h01) * fz;
    }

}  // namespace openkal::terrain
