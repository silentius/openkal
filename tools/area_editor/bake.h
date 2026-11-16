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

    // One-shot orthographic top-down bake of KCM terrain + OPL props (2048² by
    // default).
    BakeResult bake_patch_topdown(engine::IEngine& engine,
                                  const BakePaths& paths,
                                  int patch_x,
                                  int patch_y,
                                  int resolution = 2048);

}  // namespace openkal::area_editor
