/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WidgetTools.h"
#include "openkal/platform/File.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace openkal::client {

    bool WidgetTools::point_in_rect(float px, float py, float x, float y, float w, float h) {
        return px >= x && py >= y && px < x + w && py < y + h;
    }

    void
    WidgetTools::center_rect(float& x, float& y, float w, float h, float screen_w, float screen_h) {
        x = std::max(0.0f, (screen_w - w) * 0.5f);
        y = std::max(0.0f, (screen_h - h) * 0.5f);
    }

    bool WidgetTools::load_bmp_rgba(const std::string& path, terrain::ImageRgba& out) {
        auto bytes_opt = platform::read_file(path);
        if (!bytes_opt || bytes_opt->size() < 54) {
            return false;
        }
        const std::vector<std::uint8_t>& bytes = *bytes_opt;
        if (bytes[0] != 'B' || bytes[1] != 'M') {
            return false;
        }
        const auto u16 = [&](std::size_t o) {
            return static_cast<std::uint16_t>(bytes[o] | (bytes[o + 1] << 8));
        };
        const auto u32 = [&](std::size_t o) {
            return static_cast<std::uint32_t>(bytes[o] | (bytes[o + 1] << 8) |
                                              (bytes[o + 2] << 16) | (bytes[o + 3] << 24));
        };
        const auto i32 = [&](std::size_t o) { return static_cast<std::int32_t>(u32(o)); };

        const std::uint32_t pixel_offset = u32(10);
        const std::uint32_t dib = u32(14);
        if (dib < 40 || pixel_offset >= bytes.size()) {
            return false;
        }
        const int width = i32(18);
        int height = i32(22);
        const bool top_down = height < 0;
        if (top_down) {
            height = -height;
        }
        const std::uint16_t planes = u16(26);
        const std::uint16_t bpp = u16(28);
        const std::uint32_t compression = u32(30);
        if (planes != 1 || (bpp != 8 && bpp != 24 && bpp != 32) || compression != 0 || width <= 0 ||
            height <= 0) {
            return false;
        }

        std::array<std::uint8_t, 256 * 4> palette{};
        if (bpp == 8) {
            const std::uint32_t colors_used = u32(46);
            const std::uint32_t ncolors = colors_used == 0 ? 256u : std::min(256u, colors_used);
            const std::size_t pal_off = 14 + static_cast<std::size_t>(dib);
            if (pal_off + static_cast<std::size_t>(ncolors) * 4 > bytes.size()) {
                return false;
            }
            for (std::uint32_t i = 0; i < ncolors; ++i) {
                const std::size_t o = pal_off + static_cast<std::size_t>(i) * 4;
                palette[static_cast<std::size_t>(i) * 4 + 0] = bytes[o + 2];  // R
                palette[static_cast<std::size_t>(i) * 4 + 1] = bytes[o + 1];  // G
                palette[static_cast<std::size_t>(i) * 4 + 2] = bytes[o + 0];  // B
                palette[static_cast<std::size_t>(i) * 4 + 3] = 255;
            }
        }

        const int row_bytes = ((width * static_cast<int>(bpp) + 31) / 32) * 4;
        if (pixel_offset + static_cast<std::uint32_t>(row_bytes * height) > bytes.size()) {
            return false;
        }

        out.width = width;
        out.height = height;
        out.rgba.assign(static_cast<std::size_t>(width * height * 4), 0);
        const int bpp_bytes = bpp / 8;
        bool any_nonzero_alpha = false;
        for (int y = 0; y < height; ++y) {
            const int src_y = top_down ? y : (height - 1 - y);
            const std::uint8_t* row =
                bytes.data() + pixel_offset + static_cast<std::size_t>(src_y) * row_bytes;
            for (int x = 0; x < width; ++x) {
                const std::size_t o = static_cast<std::size_t>((y * width + x) * 4);
                if (bpp == 8) {
                    const std::uint8_t idx = row[x];
                    const std::size_t po = static_cast<std::size_t>(idx) * 4;
                    out.rgba[o + 0] = palette[po + 0];
                    out.rgba[o + 1] = palette[po + 1];
                    out.rgba[o + 2] = palette[po + 2];
                    out.rgba[o + 3] = 255;
                } else {
                    const std::uint8_t* p = row + x * bpp_bytes;
                    out.rgba[o + 0] = p[2];
                    out.rgba[o + 1] = p[1];
                    out.rgba[o + 2] = p[0];
                    if (bpp_bytes == 4) {
                        out.rgba[o + 3] = p[3];
                        any_nonzero_alpha = any_nonzero_alpha || (p[3] != 0);
                    } else {
                        out.rgba[o + 3] = 255;
                    }
                }
            }
        }
        // Some Kal 32-bit BI_RGB icons store an unused all-zero alpha plane (e.g.
        // action015.BMP). Treat that as fully opaque, then apply the usual color
        // keys.
        if (bpp_bytes == 4 && !any_nonzero_alpha) {
            for (std::size_t o = 3; o < out.rgba.size(); o += 4) {
                out.rgba[o] = 255;
            }
        }
        for (int i = 0; i < width * height; ++i) {
            const std::size_t o = static_cast<std::size_t>(i * 4);
            // Kal UI BMPs: magenta, or pure/near-black on 8/24-bit (not item fill).
            if (out.rgba[o + 0] > 250 && out.rgba[o + 1] < 5 && out.rgba[o + 2] > 250) {
                out.rgba[o + 3] = 0;
            } else if ((bpp == 8 || bpp_bytes == 3) &&
                       out.rgba[o + 0] + out.rgba[o + 1] + out.rgba[o + 2] <= 6) {
                out.rgba[o + 3] = 0;
            }
        }
        return true;
    }

}  // namespace openkal::client
