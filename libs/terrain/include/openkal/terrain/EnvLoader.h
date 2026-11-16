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

#include <string>
#include <vector>

namespace openkal::terrain {

    /** Terrain detail-layer metadata. */
    struct LayerInfo {
        /** Horizontal texture scale. */
        int scale_u = 0;

        /** Vertical texture scale. */
        int scale_v = 0;

        /** Texture name without extension. */
        std::string texture_name;
    };

    /** Terrain decoration model metadata. */
    struct DecoInfo {
        /** Decoration-table index referenced by KCM palette entries. */
        int index = 0;

        /** Client-relative GB model name. */
        std::string model_name;
    };

    /** Loads terrain environment layer tables. */
    class EnvLoader final {
      public:
        /** Loads the layer table from an environment file.
         * @param path Environment file path.
         * @param out Destination layer list.
         * @return True when the table was loaded.
         */
        [[nodiscard]] static bool load(const std::string& path, std::vector<LayerInfo>& out);

        /** Loads terrain layers and the decoration model table.
         * @param path Environment file path.
         * @param layers Destination layer list.
         * @param decorations Destination decoration list.
         * @return True when both tables were loaded.
         */
        [[nodiscard]] static bool load(const std::string& path,
                                       std::vector<LayerInfo>& layers,
                                       std::vector<DecoInfo>& decorations);
    };

    /** Loads the layer table from an environment file.
     * @param path Environment file path.
     * @param out Destination layer list.
     * @return True when the table was loaded.
     */
    [[nodiscard]] bool load_env_layers(const std::string& path, std::vector<LayerInfo>& out);

    /** Loads terrain layers and decorations from an environment file.
     * @param path Environment file path.
     * @param layers Destination layer list.
     * @param decorations Destination decoration list.
     * @return True when both tables were loaded.
     */
    [[nodiscard]] bool load_environment(const std::string& path,
                                        std::vector<LayerInfo>& layers,
                                        std::vector<DecoInfo>& decorations);

}  // namespace openkal::terrain
