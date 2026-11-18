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

#include "ObjectMask.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GtxLoader.h"

#include <functional>
#include <string>
#include <vector>

namespace openkal::area_editor {

    struct BakePaths {
        std::string data_root;  // external asset pack/data (OBJECTS, …)
        std::string maps_dir;   // directory with n_XXX_YYY.kcm/.opl and Tex/
    };

    struct BakeResult {
        terrain::ImageRgba image;
        int patch_x = 0;
        int patch_y = 0;
        int prop_count = 0;
        int missing_gb = 0;
        std::vector<ObjectMask> object_masks;
        std::string error;
    };

    // Resolve maps dir from --data and/or the area path directory.
    BakePaths resolve_bake_paths(const std::string& area_path, const std::string& data_root_arg);

    /** Stages of a top-down map bake. */
    enum class BakeStage { Terrain, Objects, MovementClearance, Render };

    /** Receives the current bake stage and its completed fraction between zero and one. */
    using BakeProgress = std::function<void(BakeStage, double)>;

    /** Bakes an orthographic view of the terrain and placed objects.
     * @param engine OpenGL engine used on the calling thread.
     * @param paths Terrain and asset directories.
     * @param patch_x World patch X.
     * @param patch_y World patch Y.
     * @param resolution Length of each image edge in pixels.
     * @param progress Optional observer called on the calling thread.
     * @return Image and object masks, or an error explaining why the bake failed.
     */
    BakeResult bake_patch_topdown(engine::IEngine& engine,
                                  const BakePaths& paths,
                                  int patch_x,
                                  int patch_y,
                                  int resolution = 2048,
                                  const BakeProgress& progress = {});

}  // namespace openkal::area_editor
