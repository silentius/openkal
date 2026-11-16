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

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Loads per-class Government Position (G.P.) title tables from JSON files in config/jobs. */
    class JobTitles {
      public:
        /**
         * Loads knight/mage/archer/thief JSON from a jobs directory.
         * @param jobs_dir Directory containing class job JSON files.
         * @return True when at least one class file loaded.
         */
        [[nodiscard]] static bool load(const std::string& jobs_dir);

        /**
         * Class display name (Job field).
         * @param clazz Class index 0..3.
         * @return Class name, or empty when unknown.
         */
        [[nodiscard]] static std::string class_name(int clazz);

        /**
         * Government Position title for a class title index.
         * @param clazz Class index 0..3.
         * @param title_index Index into that class titles array (0 = default /
         * wandering).
         * @return Title string, or empty when missing.
         */
        [[nodiscard]] static std::string title(int clazz, int title_index);
    };

}  // namespace openkal::client
