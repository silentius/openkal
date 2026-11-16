/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/GtxLoader.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/Decode4.h"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::terrain {
    namespace {

        constexpr std::uint32_t fourcc(char a, char b, char c, char d) {
            return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 8) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 16) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(d)) << 24);
        }

        void decode_dxt1_block(const std::uint8_t block[8], std::uint8_t out_rgba[16 * 4]) {
            const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
            const std::uint16_t c1 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
            auto expand = [](std::uint16_t c, std::uint8_t rgb[3]) {
                const int r = (c >> 11) & 31;
                const int g = (c >> 5) & 63;
                const int b = c & 31;
                rgb[0] = static_cast<std::uint8_t>((r * 255 + 15) / 31);
                rgb[1] = static_cast<std::uint8_t>((g * 255 + 31) / 63);
                rgb[2] = static_cast<std::uint8_t>((b * 255 + 15) / 31);
            };
            std::uint8_t colors[4][4]{};
            expand(c0, colors[0]);
            colors[0][3] = 255;
            expand(c1, colors[1]);
            colors[1][3] = 255;
            if (c0 > c1) {
                for (int i = 0; i < 3; ++i) {
                    colors[2][i] = static_cast<std::uint8_t>((2 * colors[0][i] + colors[1][i]) / 3);
                    colors[3][i] = static_cast<std::uint8_t>((colors[0][i] + 2 * colors[1][i]) / 3);
                }
                colors[2][3] = colors[3][3] = 255;
            } else {
                for (int i = 0; i < 3; ++i) {
                    colors[2][i] = static_cast<std::uint8_t>((colors[0][i] + colors[1][i]) / 2);
                    colors[3][i] = 0;
                }
                colors[2][3] = 255;
                colors[3][3] = 0;
            }
            const std::uint32_t idx = static_cast<std::uint32_t>(
                block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24));
            for (int i = 0; i < 16; ++i) {
                const int ci = (idx >> (2 * i)) & 3;
                out_rgba[i * 4 + 0] = colors[ci][0];
                out_rgba[i * 4 + 1] = colors[ci][1];
                out_rgba[i * 4 + 2] = colors[ci][2];
                out_rgba[i * 4 + 3] = colors[ci][3];
            }
        }

        void decode_dxt5_block(const std::uint8_t block[16], std::uint8_t out_rgba[16 * 4]) {
            // Color is second 8 bytes (DXT1-like).
            decode_dxt1_block(block + 8, out_rgba);
            const std::uint8_t a0 = block[0];
            const std::uint8_t a1 = block[1];
            std::uint8_t alphas[8];
            alphas[0] = a0;
            alphas[1] = a1;
            if (a0 > a1) {
                for (int i = 1; i <= 6; ++i) {
                    alphas[i + 1] = static_cast<std::uint8_t>(((7 - i) * a0 + i * a1) / 7);
                }
            } else {
                for (int i = 1; i <= 4; ++i) {
                    alphas[i + 1] = static_cast<std::uint8_t>(((5 - i) * a0 + i * a1) / 5);
                }
                alphas[6] = 0;
                alphas[7] = 255;
            }
            std::uint64_t bits = 0;
            for (int i = 0; i < 6; ++i) {
                bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
            }
            for (int i = 0; i < 16; ++i) {
                const int ai = static_cast<int>((bits >> (3 * i)) & 7u);
                out_rgba[i * 4 + 3] = alphas[ai];
            }
        }

        int mask_shift(std::uint32_t mask) {
            if (mask == 0) {
                return -1;
            }
            int shift = 0;
            while ((mask & 1u) == 0u) {
                mask >>= 1;
                ++shift;
            }
            return shift;
        }

        int mask_bit_count(std::uint32_t mask) {
            int n = 0;
            while (mask) {
                n += static_cast<int>(mask & 1u);
                mask >>= 1;
            }
            return n;
        }

        std::uint8_t channel_from_pixel(std::uint32_t pixel, std::uint32_t mask, int shift) {
            if (shift < 0 || mask == 0) {
                return 0;
            }
            const std::uint32_t bits = (pixel & mask) >> static_cast<unsigned>(shift);
            const int nbits = mask_bit_count(mask >> static_cast<unsigned>(shift));
            if (nbits <= 0) {
                return 0;
            }
            if (nbits >= 8) {
                return static_cast<std::uint8_t>(bits & 0xFFu);
            }
            // Expand 4/5/6-bit channels to 8-bit (A4R4G4B4, R5G6B5, A1R5G5B5, …).
            const std::uint32_t maxv = (1u << nbits) - 1u;
            return static_cast<std::uint8_t>((bits * 255u + maxv / 2u) / maxv);
        }

        bool decode_dds_mip0(const std::uint8_t* data, std::size_t size, ImageRgba& out) {
            if (size < 128 || std::memcmp(data, "DDS ", 4) != 0) {
                return false;
            }
            std::uint32_t height = 0;
            std::uint32_t width = 0;
            std::uint32_t pf_flags = 0;
            std::uint32_t four_cc = 0;
            std::uint32_t bit_count = 0;
            std::uint32_t r_mask = 0;
            std::uint32_t g_mask = 0;
            std::uint32_t b_mask = 0;
            std::uint32_t a_mask = 0;
            std::memcpy(&height, data + 12, 4);
            std::memcpy(&width, data + 16, 4);
            // DDS_PIXELFORMAT starts at header offset 72 => file offset 76.
            std::memcpy(&pf_flags, data + 80, 4);
            std::memcpy(&four_cc, data + 84, 4);
            std::memcpy(&bit_count, data + 88, 4);
            std::memcpy(&r_mask, data + 92, 4);
            std::memcpy(&g_mask, data + 96, 4);
            std::memcpy(&b_mask, data + 100, 4);
            std::memcpy(&a_mask, data + 104, 4);
            if (width == 0 || height == 0 || width > 8192 || height > 8192) {
                return false;
            }
            const std::uint8_t* pixels = data + 128;
            const std::size_t avail = size - 128;
            out.width = static_cast<int>(width);
            out.height = static_cast<int>(height);
            out.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4,
                            0);

            const bool dxt1 = (pf_flags & 0x4) && four_cc == fourcc('D', 'X', 'T', '1');
            const bool dxt5 = (pf_flags & 0x4) && four_cc == fourcc('D', 'X', 'T', '5');
            const bool dxt3 = (pf_flags & 0x4) && four_cc == fourcc('D', 'X', 'T', '3');

            if (dxt1 || dxt5 || dxt3) {
                const int blocks_w = static_cast<int>((width + 3) / 4);
                const int blocks_h = static_cast<int>((height + 3) / 4);
                const int block_size = dxt1 ? 8 : 16;
                if (avail < static_cast<std::size_t>(blocks_w * blocks_h * block_size)) {
                    return false;
                }
                std::uint8_t decoded[16 * 4];
                for (int by = 0; by < blocks_h; ++by) {
                    for (int bx = 0; bx < blocks_w; ++bx) {
                        const std::uint8_t* block =
                            pixels + static_cast<std::size_t>((by * blocks_w + bx) * block_size);
                        if (dxt1) {
                            decode_dxt1_block(block, decoded);
                        } else if (dxt5) {
                            decode_dxt5_block(block, decoded);
                        } else {
                            // DXT3: 8-byte explicit alpha + DXT1 color
                            decode_dxt1_block(block + 8, decoded);
                            for (int i = 0; i < 16; ++i) {
                                const int nibble = (block[i / 2] >> ((i % 2) * 4)) & 0xF;
                                decoded[i * 4 + 3] = static_cast<std::uint8_t>(nibble * 17);
                            }
                        }
                        for (int py = 0; py < 4; ++py) {
                            for (int px = 0; px < 4; ++px) {
                                const int x = bx * 4 + px;
                                const int y = by * 4 + py;
                                if (x >= static_cast<int>(width) || y >= static_cast<int>(height)) {
                                    continue;
                                }
                                const std::size_t dst = (static_cast<std::size_t>(y) * width +
                                                         static_cast<std::size_t>(x)) *
                                                        4;
                                const int si = (py * 4 + px) * 4;
                                out.rgba[dst + 0] = decoded[si + 0];
                                out.rgba[dst + 1] = decoded[si + 1];
                                out.rgba[dst + 2] = decoded[si + 2];
                                out.rgba[dst + 3] = decoded[si + 3];
                            }
                        }
                    }
                }
                return true;
            }

            // Uncompressed RGB / RGBA (DDPF_RGB=0x40, optional DDPF_ALPHAPIXELS=0x1).
            // NPC clothes often use 16-bit A4R4G4B4 (e.g. N033_02.GTX); effects use
            // 24/32-bit.
            constexpr std::uint32_t kDdpfRgb = 0x40;
            if ((pf_flags & kDdpfRgb) != 0 &&
                (bit_count == 16 || bit_count == 24 || bit_count == 32)) {
                const std::size_t bpp = bit_count / 8;
                const std::size_t need =
                    static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp;
                if (avail < need) {
                    return false;
                }
                const int r_shift = mask_shift(r_mask);
                const int g_shift = mask_shift(g_mask);
                const int b_shift = mask_shift(b_mask);
                const int a_shift = mask_shift(a_mask);
                const bool has_alpha = (pf_flags & 0x1) != 0 && a_mask != 0;
                for (std::uint32_t y = 0; y < height; ++y) {
                    for (std::uint32_t x = 0; x < width; ++x) {
                        const std::size_t src =
                            (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) *
                            bpp;
                        std::uint32_t pixel = 0;
                        for (std::size_t i = 0; i < bpp; ++i) {
                            pixel |= static_cast<std::uint32_t>(pixels[src + i]) << (8 * i);
                        }
                        const std::size_t dst =
                            (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
                        out.rgba[dst + 0] = channel_from_pixel(pixel, r_mask, r_shift);
                        out.rgba[dst + 1] = channel_from_pixel(pixel, g_mask, g_shift);
                        out.rgba[dst + 2] = channel_from_pixel(pixel, b_mask, b_shift);
                        out.rgba[dst + 3] = has_alpha ? channel_from_pixel(pixel, a_mask, a_shift)
                                                      : static_cast<std::uint8_t>(255);
                    }
                }
                return true;
            }

            log::error("load_gtx_rgba: unsupported DDS format");
            return false;
        }

    }  // namespace

    bool GtxLoader::load(const std::string& path, ImageRgba& out) {
        auto bytes = platform::read_file(path);
        if (!bytes) {
            log::error("load_gtx_rgba: cannot read " + path);
            return false;
        }
        if (bytes->size() < 128) {
            return false;
        }

        // GTX uses "KAL " magic; plain DDS uses "DDS ".
        if (std::memcmp(bytes->data(), "KAL ", 4) == 0) {
            (*bytes)[0] = 'D';
            (*bytes)[1] = 'D';
            (*bytes)[2] = 'S';
            (*bytes)[3] = ' ';
            // Decode(4, header+4, 0x40) with header at file+4 => file offsets [8,
            // 8+0x40).
            for (std::size_t i = 8; i < 8 + 0x40; ++i) {
                (*bytes)[i] = kDecodeTable4[(*bytes)[i]];
            }
        } else if (std::memcmp(bytes->data(), "DDS ", 4) != 0) {
            log::error("load_gtx_rgba: not GTX/DDS: " + path);
            return false;
        }

        if (!decode_dds_mip0(bytes->data(), bytes->size(), out)) {
            log::error("load_gtx_rgba: DDS decode failed: " + path);
            return false;
        }
        return true;
    }

    bool load_gtx_rgba(const std::string& path, ImageRgba& out) {
        return GtxLoader::load(path, out);
    }

    namespace {

        [[nodiscard]] char ascii_lower(char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        [[nodiscard]] std::string ascii_lowered(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                c = ascii_lower(c);
            }
            return out;
        }

        [[nodiscard]] bool ends_with_ci(std::string_view name, std::string_view ext) {
            if (name.size() < ext.size()) {
                return false;
            }
            return ascii_lowered(name.substr(name.size() - ext.size())) == ascii_lowered(ext);
        }

        [[nodiscard]] bool load_path_rgba(const std::string& path, ImageRgba& out) {
            if (path.empty() || !platform::file_exists(path)) {
                return false;
            }
            if (ends_with_ci(path, ".tga")) {
                return load_tga_rgba(path, out);
            }
            return load_gtx_rgba(path, out);
        }

    }  // namespace

    bool load_tga_rgba(const std::string& path, ImageRgba& out) {
        out = {};
        auto bytes = platform::read_file(path);
        if (!bytes || bytes->size() < 18) {
            return false;
        }
        const std::uint8_t* h = bytes->data();
        const int id_len = h[0];
        const int cmap_type = h[1];
        const int image_type = h[2];
        const int width = h[12] | (h[13] << 8);
        const int height = h[14] | (h[15] << 8);
        const int bpp = h[16];
        const int descriptor = h[17];
        if (cmap_type != 0 || width <= 0 || height <= 0) {
            return false;
        }
        // Type 2 = uncompressed true-color; type 10 = RLE true-color. Mix FX
        // (fire_black.tga) and many Effect/Tex flames ship as type 10.
        if ((image_type != 2 && image_type != 10) || (bpp != 24 && bpp != 32)) {
            log::warn("load_tga_rgba: unsupported TGA " + path);
            return false;
        }
        const std::size_t header = 18u + static_cast<std::size_t>(id_len);
        const std::size_t bytes_pp = static_cast<std::size_t>(bpp / 8);
        constexpr std::size_t kMaxPixels = 64u * 1024u * 1024u;
        if (header > bytes->size() ||
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > kMaxPixels) {
            return false;
        }
        const std::size_t pixel_bytes =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bytes_pp;
        std::vector<std::uint8_t> raw(pixel_bytes);
        const std::uint8_t* src = bytes->data() + header;
        const std::uint8_t* const src_end = bytes->data() + bytes->size();
        if (image_type == 2) {
            if (bytes->size() < header + pixel_bytes) {
                return false;
            }
            std::memcpy(raw.data(), src, pixel_bytes);
        } else {
            std::size_t out_i = 0;
            while (out_i < pixel_bytes) {
                if (src >= src_end) {
                    return false;
                }
                const std::uint8_t packet = *src++;
                const int count = static_cast<int>(packet & 0x7Fu) + 1;
                const std::size_t run = static_cast<std::size_t>(count) * bytes_pp;
                if (out_i + run > pixel_bytes) {
                    return false;
                }
                if ((packet & 0x80u) != 0) {
                    if (static_cast<std::size_t>(src_end - src) < bytes_pp) {
                        return false;
                    }
                    for (int i = 0; i < count; ++i) {
                        std::memcpy(raw.data() + out_i, src, bytes_pp);
                        out_i += bytes_pp;
                    }
                    src += bytes_pp;
                } else {
                    if (static_cast<std::size_t>(src_end - src) < run) {
                        return false;
                    }
                    std::memcpy(raw.data() + out_i, src, run);
                    out_i += run;
                    src += run;
                }
            }
        }
        const bool origin_top = (descriptor & 0x20) != 0;
        const bool origin_right = (descriptor & 0x10) != 0;
        out.width = width;
        out.height = height;
        out.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
        for (int y = 0; y < height; ++y) {
            const int dy = origin_top ? y : (height - 1 - y);
            for (int x = 0; x < width; ++x) {
                const std::size_t si =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)) *
                    bytes_pp;
                const int dx = origin_right ? width - 1 - x : x;
                const std::size_t di =
                    (static_cast<std::size_t>(dy) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(dx)) *
                    4u;
                out.rgba[di + 0] = raw[si + 2];
                out.rgba[di + 1] = raw[si + 1];
                out.rgba[di + 2] = raw[si + 0];
                out.rgba[di + 3] = bpp == 32 ? raw[si + 3] : 255;
            }
        }
        return true;
    }

    bool load_effect_tex_rgba(const std::string& data_root,
                              const std::string& client_path,
                              ImageRgba& out) {
        if (data_root.empty() || client_path.empty()) {
            return false;
        }
        const std::string lowered = ascii_lowered(client_path);
        if (lowered == "notexture" || ends_with_ci(lowered, "/notexture.bmp") ||
            ends_with_ci(lowered, "\\notexture.bmp")) {
            return false;
        }

        const std::string resolved = platform::resolve_client_asset(data_root, client_path);
        if (load_path_rgba(resolved, out)) {
            return true;
        }

        std::string rel = platform::strip_client_data_prefix(client_path);
        rel = platform::normalize_separators(std::move(rel));
        std::string stem = rel;
        const auto slash = stem.find_last_of('/');
        if (slash != std::string::npos) {
            stem = stem.substr(slash + 1);
        }
        const auto dot = stem.find_last_of('.');
        if (dot != std::string::npos) {
            stem = stem.substr(0, dot);
        }
        if (stem.empty()) {
            return false;
        }

        static const char* kExts[] = {".GTX", ".gtx", ".dds", ".DDS", ".tga", ".TGA"};
        const std::string tex_dir = platform::join_path(data_root, "Effect/Tex");
        for (const char* ext : kExts) {
            const std::string candidate = platform::resolve_path_case(tex_dir, stem + ext);
            if (load_path_rgba(candidate, out)) {
                return true;
            }
        }
        // Also try the relative folder from the script (Objects/Common/Tex/…).
        if (slash != std::string::npos) {
            const std::string dir_rel = rel.substr(0, slash);
            for (const char* ext : kExts) {
                const std::string candidate =
                    platform::resolve_path_case(data_root, dir_rel + "/" + stem + ext);
                if (load_path_rgba(candidate, out)) {
                    return true;
                }
            }
        }
        return false;
    }

}  // namespace openkal::terrain
