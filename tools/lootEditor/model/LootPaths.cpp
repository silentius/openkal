/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "model/LootPaths.h"

#include "openkal/platform/Path.h"

namespace {

    [[nodiscard]] bool has_hypertext_bmps(const std::string& dir) {
        if (!openkal::platform::is_directory(dir)) {
            return false;
        }
        for (const auto& name : openkal::platform::list_directory(dir)) {
            if (name.size() >= 4) {
                const std::string ext = name.substr(name.size() - 4);
                if (ext == ".bmp" || ext == ".BMP") {
                    return true;
                }
            }
        }
        return false;
    }

}  // namespace

std::string hypertext_dir_for(const std::string& data_root) {
    const std::string nested = openkal::platform::join_path(data_root, "HyperText");
    if (has_hypertext_bmps(nested)) {
        return nested;
    }
    if (has_hypertext_bmps(data_root)) {
        return data_root;
    }
    return {};
}
