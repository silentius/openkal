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

#include "openkal/terrain/ImageRgba.h"

#include <string>

namespace openkal::terrain {

    /** Loads GTX and DDS images. */
    class GtxLoader final {
      public:
        /** Loads mip level zero into RGBA8.
         * @param path Path to a GTX or DDS image.
         * @param out Destination image.
         * @return True when the image was loaded.
         */
        [[nodiscard]] static bool load(const std::string& path, ImageRgba& out);
    };

    /** Loads mip level zero into RGBA8.
     * @param path Path to a GTX or DDS image.
     * @param out Destination image.
     * @return True when the image was loaded.
     */
    [[nodiscard]] bool load_gtx_rgba(const std::string& path, ImageRgba& out);

    /** Loads a true-color TGA (uncompressed or RLE) into RGBA8.
     * @param path Path to a .tga file.
     * @param out Destination image.
     * @return True when the image was loaded.
     */
    [[nodiscard]] bool load_tga_rgba(const std::string& path, ImageRgba& out);

    /**
     * Loads an Effect/Tex (or any client) texture path into RGBA8.
     * Accepts bare names or full `Data\\Effect\\Tex\\…` paths, and falls back
     * across .gtx / .dds / .tga when the named extension is missing.
     * @param data_root Client data root.
     * @param client_path Texture path from a particle/FX/skill script.
     * @param out Destination image.
     * @return True when any candidate loaded.
     */
    [[nodiscard]] bool load_effect_tex_rgba(const std::string& data_root,
                                            const std::string& client_path,
                                            ImageRgba& out);

}  // namespace openkal::terrain
