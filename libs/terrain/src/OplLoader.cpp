/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/OplLoader.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <cstring>

namespace openkal::terrain {
    namespace {

        struct OplHeader {
            std::uint32_t checksum = 0;
            std::uint32_t map_no = 0;
            std::uint32_t x = 0;
            std::uint32_t y = 0;
            std::uint32_t reserved[4]{};
            std::uint32_t version = 0;
        };

        bool read_pod(const std::vector<std::uint8_t>& src,
                      std::size_t& offset,
                      void* dst,
                      std::size_t n) {
            if (offset + n > src.size()) {
                return false;
            }
            std::memcpy(dst, src.data() + offset, n);
            offset += n;
            return true;
        }

    }  // namespace

    void opl_world_translation(
        const OplInstance& inst, int patch_x, int patch_y, float& wx, float& wy, float& wz) {
        // roam.cpp: matrix._41 = vTrans.x * c_patch_scale + c_patch_scale * i -
        // c_full_scale/2
        //           matrix._42 = vTrans.z
        //           matrix._43 = vTrans.y * c_patch_scale + c_patch_scale * j -
        //           c_full_scale/2
        wx = inst.tx * kOplPatchScale + kOplPatchScale * static_cast<float>(patch_x) -
             kOplFullScale * 0.5f;
        wy = inst.tz;
        wz = inst.ty * kOplPatchScale + kOplPatchScale * static_cast<float>(patch_y) -
             kOplFullScale * 0.5f;
    }

    bool OplLoader::load(const std::string& path, OplFile& out) {
        out = {};
        auto bytes = platform::read_file(path);
        if (!bytes) {
            // Try alternate casing (.OPL vs .opl).
            std::string alt = path;
            if (alt.size() >= 4) {
                const auto ext = alt.substr(alt.size() - 4);
                if (ext == ".opl") {
                    alt.replace(alt.size() - 4, 4, ".OPL");
                } else if (ext == ".OPL") {
                    alt.replace(alt.size() - 4, 4, ".opl");
                }
                bytes = platform::read_file(alt);
            }
        }
        if (!bytes) {
            log::error("load_opl: cannot read " + path);
            return false;
        }

        std::size_t offset = 0;
        OplHeader header{};
        if (!read_pod(*bytes, offset, &header, sizeof(header))) {
            log::error("load_opl: truncated header");
            return false;
        }

        out.version = header.version;
        out.checksum = header.checksum;
        out.patch_x = static_cast<int>(header.x);
        out.patch_y = static_cast<int>(header.y);

        std::int32_t count = 0;
        if (!read_pod(*bytes, offset, &count, sizeof(count)) || count < 0) {
            log::error("load_opl: bad model count");
            return false;
        }

        out.instances.reserve(static_cast<std::size_t>(count));
        const bool vec_scale = header.version >= 4;

        for (std::int32_t i = 0; i < count; ++i) {
            std::int32_t name_len = 0;
            if (!read_pod(*bytes, offset, &name_len, sizeof(name_len)) || name_len < 0 ||
                name_len > 512) {
                log::error("load_opl: bad name length at index " + std::to_string(i));
                return false;
            }
            if (offset + static_cast<std::size_t>(name_len) > bytes->size()) {
                return false;
            }
            OplInstance inst;
            inst.name.assign(reinterpret_cast<const char*>(bytes->data() + offset),
                             static_cast<std::size_t>(name_len));
            offset += static_cast<std::size_t>(name_len);

            float trans[3]{};
            float quat[4]{};
            if (!read_pod(*bytes, offset, trans, sizeof(trans)) ||
                !read_pod(*bytes, offset, quat, sizeof(quat))) {
                return false;
            }
            inst.tx = trans[0];
            inst.ty = trans[1];
            inst.tz = trans[2];
            inst.qx = quat[0];
            inst.qy = quat[1];
            inst.qz = quat[2];
            inst.qw = quat[3];

            if (vec_scale) {
                float scale[3]{};
                if (!read_pod(*bytes, offset, scale, sizeof(scale))) {
                    return false;
                }
                inst.sx = scale[0];
                inst.sy = scale[1];
                inst.sz = scale[2];
            } else {
                float s = 1.0f;
                if (!read_pod(*bytes, offset, &s, sizeof(s))) {
                    return false;
                }
                inst.sx = inst.sy = inst.sz = s;
            }
            out.instances.push_back(std::move(inst));
        }
        return true;
    }

    bool load_opl(const std::string& path, OplFile& out) {
        return OplLoader::load(path, out);
    }

}  // namespace openkal::terrain
