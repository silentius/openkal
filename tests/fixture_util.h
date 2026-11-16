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

// Synthetic asset writers for unit tests — no external asset pack / symlink data
// required.

#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/Path.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace openkal::test {

    inline void mkdir_p(const std::string& dir) {
        std::string cur;
        for (std::size_t i = 0; i < dir.size(); ++i) {
            cur.push_back(dir[i]);
            if (dir[i] == '/' || i + 1 == dir.size()) {
                if (cur.size() > 1) {
                    ::mkdir(cur.c_str(), 0755);
                }
            }
        }
    }

    inline std::string temp_dir(const char* name) {
        const char* base = std::getenv("TMPDIR");
        if (!base || !*base) {
            base = "/tmp";
        }
        const std::string dir =
            openkal::platform::join_path(base, std::string("openkal_fixtures_") + name);
        mkdir_p(dir);
        return dir;
    }

    inline bool write_bytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
        const auto slash = path.find_last_of('/');
        if (slash != std::string::npos) {
            mkdir_p(path.substr(0, slash));
        }
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(out);
    }

    template <typename T> inline void append_pod(std::vector<std::uint8_t>& out, const T& v) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
        out.insert(out.end(), p, p + sizeof(T));
    }

    inline void append_bytes(std::vector<std::uint8_t>& out, const void* data, std::size_t n) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        out.insert(out.end(), p, p + n);
    }

    inline void append_str_bytes(std::vector<std::uint8_t>& out, const std::string& s) {
        append_bytes(out, s.data(), s.size());
    }

    // Minimal uncompressed A8R8G8B8 DDS (mip0 only).
    inline bool
    write_rgba_dds(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgba) {
        if (w <= 0 || h <= 0 ||
            rgba.size() != static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4) {
            return false;
        }
        std::vector<std::uint8_t> dds(128, 0);
        dds[0] = 'D';
        dds[1] = 'D';
        dds[2] = 'S';
        dds[3] = ' ';
        const std::uint32_t header_size = 124;
        const std::uint32_t flags =
            0x1007;  // compatibility behavior | HEIGHT | WIDTH | PIXELFORMAT
        const std::uint32_t pitch = static_cast<std::uint32_t>(w * 4);
        const std::uint32_t pf_size = 32;
        const std::uint32_t pf_flags = 0x41;  // ALPHAPIXELS | RGB
        const std::uint32_t bit_count = 32;
        const std::uint32_t r_mask = 0x00FF0000;
        const std::uint32_t g_mask = 0x0000FF00;
        const std::uint32_t b_mask = 0x000000FF;
        const std::uint32_t a_mask = 0xFF000000;
        std::memcpy(dds.data() + 4, &header_size, 4);
        std::memcpy(dds.data() + 8, &flags, 4);
        std::memcpy(dds.data() + 12, &h, 4);
        std::memcpy(dds.data() + 16, &w, 4);
        std::memcpy(dds.data() + 20, &pitch, 4);
        std::memcpy(dds.data() + 76, &pf_size, 4);
        std::memcpy(dds.data() + 80, &pf_flags, 4);
        std::memcpy(dds.data() + 88, &bit_count, 4);
        std::memcpy(dds.data() + 92, &r_mask, 4);
        std::memcpy(dds.data() + 96, &g_mask, 4);
        std::memcpy(dds.data() + 100, &b_mask, 4);
        std::memcpy(dds.data() + 104, &a_mask, 4);
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            dds.push_back(rgba[i + 2]);  // B
            dds.push_back(rgba[i + 1]);  // G
            dds.push_back(rgba[i + 0]);  // R
            dds.push_back(rgba[i + 3]);  // A
        }
        return write_bytes(path, dds);
    }

    inline bool write_solid_dds(const std::string& path,
                                int w,
                                int h,
                                std::uint8_t r,
                                std::uint8_t g,
                                std::uint8_t b,
                                std::uint8_t a = 255) {
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) *
                                       4);
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            rgba[i] = r;
            rgba[i + 1] = g;
            rgba[i + 2] = b;
            rgba[i + 3] = a;
        }
        return write_rgba_dds(path, w, h, rgba);
    }

    inline std::string area_fixture_path(std::string path) {
        const std::size_t extension = path.find_last_of('.');
        if (extension != std::string::npos) {
            path.resize(extension);
        }
        return path + ".akarea";
    }

    inline bool write_open_area(const std::string& path) {
        openkal::map::AreaCell grid;
        grid.set_version(openkal::map::kVersion);
        grid.assign_attrs(
            static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount, 0);
        grid.ensure_blocked_mask(false);
        grid.ensure_not_moveable_mask(false);
        return openkal::map::AreaMapCodec::save(area_fixture_path(path), grid);
    }

    inline bool write_blocked_area(const std::string& path) {
        openkal::map::AreaCell grid;
        grid.set_version(openkal::map::kVersion);
        grid.assign_attrs(
            static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount, 0xffffu);
        grid.import_legacy_blocked_masks();
        return openkal::map::AreaMapCodec::save(area_fixture_path(path), grid);
    }

    inline bool
    write_opl_v4(const std::string& path,
                 int patch_x,
                 int patch_y,
                 const std::vector<std::pair<std::string, std::array<float, 3>>>& items) {
        std::vector<std::uint8_t> bytes;
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, static_cast<std::uint32_t>(patch_x));
        append_pod(bytes, static_cast<std::uint32_t>(patch_y));
        for (int i = 0; i < 4; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }
        append_pod(bytes, std::uint32_t{4});
        append_pod(bytes, static_cast<std::int32_t>(items.size()));
        for (const auto& it : items) {
            append_pod(bytes, static_cast<std::int32_t>(it.first.size()));
            append_str_bytes(bytes, it.first);
            float trans[3] = {it.second[0], it.second[1], it.second[2]};
            float quat[4] = {0, 0, 0, 1};
            float scale[3] = {1, 1, 1};
            append_bytes(bytes, trans, sizeof(trans));
            append_bytes(bytes, quat, sizeof(quat));
            append_bytes(bytes, scale, sizeof(scale));
        }
        return write_bytes(path, bytes);
    }

    inline bool write_env_layers(const std::string& path,
                                 const std::vector<std::pair<std::string, int>>& layers,
                                 const std::vector<std::pair<int, std::string>>& decorations = {}) {
        std::vector<std::uint8_t> bytes(36, 0);
        const std::uint32_t version = 4;
        std::memcpy(bytes.data() + 32, &version, 4);
        append_pod(bytes, static_cast<std::int32_t>(decorations.size()));
        for (const auto& decoration : decorations) {
            append_pod(bytes, static_cast<std::int32_t>(decoration.first));
            append_pod(bytes, static_cast<std::int32_t>(decoration.second.size()));
            append_str_bytes(bytes, decoration.second);
        }
        bytes.resize(bytes.size() + 24 * 5 * 4, 0);
        append_pod(bytes, static_cast<std::uint32_t>(layers.size()));
        for (const auto& layer : layers) {
            append_pod(bytes, static_cast<std::int32_t>(layer.second));
            append_pod(bytes, static_cast<std::int32_t>(layer.second));
            append_pod(bytes, static_cast<std::int32_t>(layer.first.size()));
            append_str_bytes(bytes, layer.first);
        }
        return write_bytes(path, bytes);
    }

    inline bool write_kcm_v7(const std::string& path,
                             int patch_x,
                             int patch_y,
                             std::uint8_t layer0,
                             std::uint8_t layer1,
                             std::uint16_t height) {
        std::vector<std::uint8_t> bytes;
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, static_cast<std::uint32_t>(patch_x));
        append_pod(bytes, static_cast<std::uint32_t>(patch_y));
        for (int i = 0; i < 4; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }
        append_pod(bytes, std::uint32_t{7});
        std::uint8_t slots[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        slots[0] = layer0;
        slots[1] = layer1;
        append_bytes(bytes, slots, 8);
        std::uint8_t deco[8] = {3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        append_bytes(bytes, deco, 8);
        const std::size_t map2 =
            static_cast<std::size_t>(openkal::map::kMapSize) * openkal::map::kMapSize;
        bytes.resize(bytes.size() + map2, 128);
        const std::size_t hcount =
            static_cast<std::size_t>(openkal::map::kHeightSamples) * openkal::map::kHeightSamples;
        for (std::size_t i = 0; i < hcount; ++i) {
            append_pod(bytes, height);
        }
        bytes.resize(bytes.size() + map2 * 3, 255);
        bytes.resize(bytes.size() + map2, 0);
        bytes[bytes.size() - map2] = 1;
        return write_bytes(path, bytes);
    }

    // Minimal GB v7: v8-style header, 22-byte inline material, 1 rigid triangle
    // (face list).
    inline bool write_rigid_gb_v7(const std::string& path, const std::string& texture_name) {
        std::string strings;
        strings += texture_name;
        strings.push_back('\0');
        const std::uint16_t string_size = static_cast<std::uint16_t>(strings.size());

        std::vector<std::uint8_t> bytes;
        bytes.push_back(7);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(1);
        append_pod(bytes, std::uint32_t{0});  // szoption
        for (int i = 0; i < 6; ++i) {
            append_pod(bytes, std::uint16_t{0});
        }
        append_pod(bytes, std::uint16_t{3});  // index_count
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{1});  // string_count
        append_pod(bytes, string_size);
        append_pod(bytes, std::uint16_t{0});  // cls_size
        append_pod(bytes, std::uint16_t{0});  // anim_count
        bytes.push_back(0);                   // anim_file_count
        append_pod(bytes, std::uint16_t{1});  // material_count
        append_pod(bytes, std::uint16_t{0});  // material_frame_count

        append_pod(bytes, std::uint32_t{0});  // szTexture
        append_pod(bytes, std::uint32_t{0xFF959595});
        append_pod(bytes, std::uint32_t{0xFF959595});
        append_pod(bytes, std::uint32_t{0xFFE5E5E5});
        append_pod(bytes, 1.0f);
        append_pod(bytes, std::uint16_t{0});  // mapoption

        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::int32_t{0});
        bytes.push_back(0);  // Static
        bytes.push_back(0);  // List
        append_pod(bytes, std::uint16_t{3});
        append_pod(bytes, std::uint16_t{3});
        bytes.push_back(0);

        auto emit_vert = [&](float x, float y, float z, float u, float v) {
            float buf[8] = {x, y, z, 0, 1, 0, u, v};
            append_bytes(bytes, buf, sizeof(buf));
        };
        emit_vert(0, 0, 0, 0, 0);
        emit_vert(10, 0, 0, 1, 0);
        emit_vert(0, 10, 0, 0, 1);
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{1});
        append_pod(bytes, std::uint16_t{2});

        append_str_bytes(bytes, strings);
        return write_bytes(path, bytes);
    }

    // Minimal GB v10: 1 rigid triangle + 1 material. Texture file should sit next
    // to the GB or under tex/.
    inline bool write_rigid_gb_v10(const std::string& path, const std::string& texture_name) {
        std::string strings;
        strings += texture_name;
        strings.push_back('\0');
        const std::uint32_t string_size = static_cast<std::uint32_t>(strings.size());

        std::vector<std::uint8_t> bytes;
        bytes.push_back(10);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(1);
        append_pod(bytes, std::uint32_t{0});  // crc
        append_pod(bytes, std::uint32_t{0});  // szoption
        for (int i = 0; i < 12; ++i) {
            append_pod(bytes, std::uint16_t{0});
        }
        append_pod(bytes, std::uint16_t{3});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, string_size);
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint16_t{0});
        bytes.push_back(0);
        bytes.push_back(0);
        append_pod(bytes, std::uint16_t{1});
        append_pod(bytes, std::uint16_t{0});  // no material frames
        for (int i = 0; i < 4; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }

        append_pod(bytes, std::uint32_t{0});  // szTexture
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, 0.0f);
        append_pod(bytes, std::uint32_t{0});

        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::int32_t{0});
        bytes.push_back(0);  // Static/Rigid pre-v11 type 0
        bytes.push_back(0);  // List
        append_pod(bytes, std::uint16_t{3});
        append_pod(bytes, std::uint16_t{3});
        bytes.push_back(0);

        auto emit_vert = [&](float x, float y, float z, float u, float v) {
            float buf[8] = {x, y, z, 0, 1, 0, u, v};
            append_bytes(bytes, buf, sizeof(buf));
        };
        emit_vert(0, 0, 0, 0, 0);
        emit_vert(10, 0, 0, 1, 0);
        emit_vert(0, 10, 0, 0, 1);
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{1});
        append_pod(bytes, std::uint16_t{2});

        append_str_bytes(bytes, strings);
        return write_bytes(path, bytes);
    }

    /**
     * GB v10 lightmap-static mesh (on-disk vertex_type 6 → VT_RIGID_DOUBLE).
     * Indoor dungeon rooms (d_p01.gb and friends) use this encoding.
     */
    inline bool write_lm_static_gb_v10(const std::string& path,
                                       const std::string& texture_name,
                                       const std::string& lightmap_name = {}) {
        std::string strings = texture_name;
        strings.push_back('\0');
        if (!lightmap_name.empty()) {
            strings += lightmap_name;
            strings.push_back('\0');
        }
        const std::uint32_t string_size = static_cast<std::uint32_t>(strings.size());

        std::vector<std::uint8_t> bytes;
        bytes.push_back(10);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(1);
        append_pod(bytes, std::uint32_t{0});  // crc
        append_pod(bytes, std::uint32_t{0});  // szoption
        for (int i = 0; i < 12; ++i) {
            append_pod(bytes, static_cast<std::uint16_t>(i == 6 ? 3 : 0));
        }
        append_pod(bytes, std::uint16_t{3});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, string_size);
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint16_t{0});
        bytes.push_back(0);
        bytes.push_back(0);
        append_pod(bytes, std::uint16_t{1});
        append_pod(bytes, std::uint16_t{0});
        for (int i = 0; i < 4; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }

        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, 0.0f);
        append_pod(bytes, std::uint32_t{0});

        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::int32_t{0});
        bytes.push_back(6);  // VT_LM_STATIC (pre-v11)
        bytes.push_back(0);
        append_pod(bytes, std::uint16_t{3});
        append_pod(bytes, std::uint16_t{3});
        bytes.push_back(0);

        auto emit_vert = [&](float x, float y, float z, float u, float v) {
            float buf[10] = {x, y, z, 0, 1, 0, u, v, u, v};
            append_bytes(bytes, buf, sizeof(buf));
        };
        emit_vert(0, 0, 0, 0, 0);
        emit_vert(10, 0, 0, 1, 0);
        emit_vert(0, 10, 0, 0, 1);
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{1});
        append_pod(bytes, std::uint16_t{2});

        append_str_bytes(bytes, strings);
        return write_bytes(path, bytes);
    }

    /** Writes an owned GB v11 collision triangle using model-coordinate AABB quantization.
     * @param path Output file.
     * @param size Horizontal triangle extent.
     * @return True when the fixture was written.
     */
    inline bool write_cls_only_gb_v11(const std::string& path, float size = 10.0f) {
        constexpr std::uint32_t kClsSize = 64;
        std::vector<std::uint8_t> bytes;
        bytes.push_back(11);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(0);                   // mesh_count
        append_pod(bytes, std::uint32_t{0});  // crc
        append_pod(bytes, std::uint32_t{0});  // szoption
        for (int i = 0; i < 12; ++i) {
            append_pod(bytes, std::uint16_t{0});
        }
        append_pod(bytes, std::uint16_t{0});  // index_count
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});  // reserved0
        append_pod(bytes, std::uint32_t{0});  // string_size
        append_pod(bytes, kClsSize);
        append_pod(bytes, std::uint16_t{0});  // anim_count
        bytes.push_back(0);                   // anim_file_count
        bytes.push_back(0);                   // reserved1
        append_pod(bytes, std::uint16_t{0});  // material_count
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, 0.0f);
        append_pod(bytes, 0.0f);
        append_pod(bytes, 0.0f);
        append_pod(bytes, size);
        append_pod(bytes, 1.0f);
        append_pod(bytes, size);
        for (int i = 0; i < 4; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }

        append_pod(bytes, std::uint16_t{3});  // vertex_count
        append_pod(bytes, std::uint16_t{1});  // face_count
        for (int i = 0; i < 6; ++i) {
            append_pod(bytes, std::uint32_t{0});
        }
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{65535});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{65535});
        // Triangle from above: (0,0,0), (0,0,size), (size,0,0); indices are already
        // vertex*3.
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{6});
        append_pod(bytes, std::uint16_t{3});
        for (int i = 0; i < 12; ++i) {
            bytes.push_back(0);
        }
        return write_bytes(path, bytes);
    }

}  // namespace openkal::test
