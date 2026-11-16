/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/GbModel.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GtxLoader.h"
#include "openkal/terrain/PngLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace openkal::terrain {
    namespace {

        constexpr int kVertexSizeV11[] = {
            32,  // Rigid
            36,  // Blend1
            40,  // Blend2
            44,  // Blend3
            48,  // Blend4
            40,  // RigidDouble / lightmap (two UV sets)
        };

        // compatibility behavior: pre-v11 on-disk enum inserts VT_STATIC at 0 and VT_LM_STATIC
        // at 6. When file type > 0 the client subtracts 1 so LM_STATIC becomes
        // VT_RIGID_DOUBLE and sets F_LIGHTMAP.
        int logical_vertex_type(int version, int file_vtype) {
            if (version < 11 && file_vtype > 0) {
                return file_vtype - 1;
            }
            return file_vtype;
        }

        int vertex_stride(int version, int file_vtype) {
            const int vtype = logical_vertex_type(version, file_vtype);
            if (vtype < 0 || vtype > 5) {
                return -1;
            }
            return kVertexSizeV11[vtype];
        }

        // Byte offset of first UV pair inside a vertex (logical type).
        int vertex_uv_offset(int version, int file_vtype) {
            const int vtype = logical_vertex_type(version, file_vtype);
            if (vtype < 0 || vtype > 5) {
                return -1;
            }
            static const int kOff[] = {24, 28, 32, 36, 40, 24};
            return kOff[vtype];
        }

        // Hard caps so a misaligned parse cannot OOM the process.
        constexpr std::uint32_t kMaxStringBytes = 16u * 1024u * 1024u;
        constexpr std::uint32_t kMaxClsBytes = 64u * 1024u * 1024u;
        // vertex_count / index_count are WORD in the file; still guard before size_t
        // multiply.
        constexpr std::size_t kMaxVertexBytesPerMesh = 48u * 65535u;
        constexpr std::size_t kMaxIndexBytesPerMesh = sizeof(std::uint16_t) * 65535u;
        constexpr std::size_t kGbBoneBytes = 65;  // D3DXMATRIX (64) + parent (1), pack(1)
        constexpr std::size_t kGbAnimBytes = 40;  // pos3 + quat4 + scale3
        constexpr std::size_t kGbKeyframeBytes = 6;

#pragma pack(push, 1)
        // Normalized view of the version-dependent file header.
        struct ModelFileHeader {
            std::uint8_t formatVersion = 0;
            std::uint8_t boneTotal = 0;
            std::uint8_t featureFlags = 0;
            std::uint8_t meshTotal = 0;
            std::uint32_t textTableBytes = 0;
            std::uint32_t collisionBytes = 0;
            std::uint16_t animationSampleTotal = 0;
            std::uint8_t clipTotal = 0;
            std::uint16_t materialTotal = 0;
            std::uint16_t materialFrameTotal = 0;
            std::uint16_t keyframeTotal = 0;
            std::uint32_t optionTextOffset = 0;
            float lowerBounds[3]{};
            float upperBounds[3]{};
        };

        struct MaterialFileRecord {
            std::uint32_t textureTextOffset = 0;
            std::uint16_t mappingFlags = 0;
            std::uint32_t optionTextOffset = 0;
            float specularPower = 0;
            std::uint32_t frameDataOffset = 0;
        };

        /** v7 first material: texture offset + inline D3DCOLOR/opacity (no string-table
         * frame). */
        struct LegacyMaterialRecord {
            std::uint32_t textureTextOffset = 0;
            std::uint32_t ambient = 0;
            std::uint32_t diffuse = 0;
            std::uint32_t specular = 0;
            float opacity = 1.0f;
            std::uint16_t mappingFlags = 0;
        };

        /** v7 materials after the first insert a DWORD (name / option offset) before
         * the colors. */
        struct LegacyNamedMaterialRecord {
            std::uint32_t textureTextOffset = 0;
            std::uint32_t optionTextOffset = 0;
            std::uint32_t ambient = 0;
            std::uint32_t diffuse = 0;
            std::uint32_t specular = 0;
            float opacity = 1.0f;
            std::uint16_t mappingFlags = 0;
        };

        struct MeshFileHeader {
            std::uint32_t nameTextOffset = 0;
            std::int32_t materialIndex = 0;
            std::uint8_t vertexLayout = 0;
            std::uint8_t indexLayout = 0;
            std::uint16_t vertexTotal = 0;
            std::uint16_t indexTotal = 0;
            std::uint8_t paletteSize = 0;
        };
#pragma pack(pop)
        static_assert(sizeof(LegacyMaterialRecord) == 22, "v7 first material size");
        static_assert(sizeof(LegacyNamedMaterialRecord) == 26, "v7 later material size");

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

        std::uint16_t read_u16_le(const std::uint8_t* p) {
            std::uint16_t v = 0;
            std::memcpy(&v, p, 2);
            return v;
        }

        // compatibility behavior vertices are uint16 XYZ quantized into the model AABB
        // (compatibility behavior). Face indices are already vertex*3 offsets into
        // that packed WORD array. v>=11 stores the AABB on the GB header; older files
        // keep min/max in the compatibility behavior header.
        void parse_cls_tris(const std::uint8_t* blob,
                            std::uint32_t cls_size,
                            std::uint8_t version,
                            GbModel& out) {
            constexpr std::size_t kClsHeaderBytes = 28;
            if (blob == nullptr || cls_size < kClsHeaderBytes) {
                return;
            }
            const std::uint16_t vertex_count = read_u16_le(blob);
            const std::uint16_t face_count = read_u16_le(blob + 2);
            float min_x = out.min_x;
            float min_y = out.min_y;
            float min_z = out.min_z;
            float max_x = out.max_x;
            float max_y = out.max_y;
            float max_z = out.max_z;
            if (version < 11) {
                std::memcpy(&min_x, blob + 4, 4);
                std::memcpy(&min_y, blob + 8, 4);
                std::memcpy(&min_z, blob + 12, 4);
                std::memcpy(&max_x, blob + 16, 4);
                std::memcpy(&max_y, blob + 20, 4);
                std::memcpy(&max_z, blob + 24, 4);
                out.min_x = min_x;
                out.min_y = min_y;
                out.min_z = min_z;
                out.max_x = max_x;
                out.max_y = max_y;
                out.max_z = max_z;
            }
            const std::size_t vert_bytes = static_cast<std::size_t>(vertex_count) * 6u;
            const std::size_t face_bytes = static_cast<std::size_t>(face_count) * 6u;
            if (kClsHeaderBytes + vert_bytes + face_bytes > cls_size) {
                return;
            }
            const float sx = (max_x - min_x) * (1.0f / 65535.0f);
            const float sy = (max_y - min_y) * (1.0f / 65535.0f);
            const float sz = (max_z - min_z) * (1.0f / 65535.0f);
            const std::uint8_t* vert_bytes_ptr = blob + kClsHeaderBytes;
            const std::uint8_t* face_bytes_ptr = vert_bytes_ptr + vert_bytes;
            const std::size_t word_count = static_cast<std::size_t>(vertex_count) * 3u;
            out.cls_tris.reserve(out.cls_tris.size() + static_cast<std::size_t>(face_count) * 9u);
            for (std::uint16_t fi = 0; fi < face_count; ++fi) {
                for (int k = 0; k < 3; ++k) {
                    const std::uint16_t idx =
                        read_u16_le(face_bytes_ptr + (static_cast<std::size_t>(fi) * 6u +
                                                      static_cast<std::size_t>(k) * 2u));
                    if (static_cast<std::size_t>(idx) + 2u >= word_count) {
                        out.cls_tris.insert(out.cls_tris.end(), {0.0f, 0.0f, 0.0f});
                        continue;
                    }
                    const std::size_t vo = static_cast<std::size_t>(idx) * 2u;
                    out.cls_tris.push_back(
                        min_x + static_cast<float>(read_u16_le(vert_bytes_ptr + vo)) * sx);
                    out.cls_tris.push_back(
                        min_y + static_cast<float>(read_u16_le(vert_bytes_ptr + vo + 2u)) * sy);
                    out.cls_tris.push_back(
                        min_z + static_cast<float>(read_u16_le(vert_bytes_ptr + vo + 4u)) * sz);
                }
            }
        }

        // Mirrors compatibility behavior version branches for GB versions 8-12.
        // Version 7 uses the same header as v8 (WORD string/cls sizes) but inlines
        // materials; the compatible client rejects < 8, yet OBJECTS still ships v7 props.
        bool read_gb_header(const std::vector<std::uint8_t>& bytes,
                            std::size_t& offset,
                            ModelFileHeader& h) {
            if (bytes.size() < 4) {
                return false;
            }
            h = {};
            h.formatVersion = bytes[0];
            h.boneTotal = bytes[1];
            h.featureFlags = bytes[2];
            h.meshTotal = bytes[3];
            offset = 4;
            if (h.formatVersion < 7 || h.formatVersion > 12) {
                return false;
            }
            // v12 packed header is crc then name; older (>=8) branch has no name field.
            if (h.formatVersion >= 10) {
                std::uint32_t crc = 0;
                if (!read_pod(bytes, offset, &crc, 4)) {
                    return false;
                }
                (void) crc;
            }
            if (h.formatVersion >= 12) {
                char name[64]{};
                if (!read_pod(bytes, offset, name, 64)) {
                    return false;
                }
            }
            if (!read_pod(bytes, offset, &h.optionTextOffset, 4)) {
                return false;
            }
            if (h.formatVersion >= 9) {
                std::uint16_t vertex_count[12]{};
                if (!read_pod(bytes, offset, vertex_count, sizeof(vertex_count))) {
                    return false;
                }
                (void) vertex_count;
            } else {
                std::uint16_t vertex_count[6]{};
                if (!read_pod(bytes, offset, vertex_count, sizeof(vertex_count))) {
                    return false;
                }
                (void) vertex_count;
            }
            std::uint16_t index_count = 0;
            std::uint16_t bone_index_count = 0;
            if (!read_pod(bytes, offset, &index_count, 2) ||
                !read_pod(bytes, offset, &bone_index_count, 2) ||
                !read_pod(bytes, offset, &h.keyframeTotal, 2)) {
                return false;
            }
            (void) index_count;
            (void) bone_index_count;
            if (h.formatVersion >= 9) {
                std::uint16_t reserved0 = 0;
                if (!read_pod(bytes, offset, &reserved0, 2) ||
                    !read_pod(bytes, offset, &h.textTableBytes, 4) ||
                    !read_pod(bytes, offset, &h.collisionBytes, 4)) {
                    return false;
                }
            } else {
                std::uint16_t string_count = 0;
                std::uint16_t string_size16 = 0;
                std::uint16_t cls_size16 = 0;
                if (!read_pod(bytes, offset, &string_count, 2) ||
                    !read_pod(bytes, offset, &string_size16, 2) ||
                    !read_pod(bytes, offset, &cls_size16, 2)) {
                    return false;
                }
                (void) string_count;
                h.textTableBytes = string_size16;
                h.collisionBytes = cls_size16;
            }
            if (!read_pod(bytes, offset, &h.animationSampleTotal, 2) ||
                !read_pod(bytes, offset, &h.clipTotal, 1)) {
                return false;
            }
            if (h.formatVersion >= 9) {
                std::uint8_t reserved1 = 0;
                if (!read_pod(bytes, offset, &reserved1, 1)) {
                    return false;
                }
            }
            if (!read_pod(bytes, offset, &h.materialTotal, 2) ||
                !read_pod(bytes, offset, &h.materialFrameTotal, 2)) {
                return false;
            }
            if (h.formatVersion >= 11) {
                if (!read_pod(bytes, offset, h.lowerBounds, 12) ||
                    !read_pod(bytes, offset, h.upperBounds, 12)) {
                    return false;
                }
            }
            if (h.formatVersion >= 9) {
                std::uint32_t reserved2[4]{};
                if (!read_pod(bytes, offset, reserved2, sizeof(reserved2))) {
                    return false;
                }
            }
            return true;
        }

        std::string to_lower_ascii(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool is_water_texture_name(const std::string& name) {
            const std::string lower = to_lower_ascii(name);
            if (lower.find("waterfall") != std::string::npos) {
                return false;
            }
            return lower.find("water") != std::string::npos;
        }

        bool looks_like_texture_name(const std::string& name) {
            const std::string lower = to_lower_ascii(name);
            return lower.size() >= 5 && (lower.find(".dds") != std::string::npos ||
                                         lower.find(".tga") != std::string::npos ||
                                         lower.find(".jpg") != std::string::npos ||
                                         lower.find(".bmp") != std::string::npos ||
                                         lower.find(".gtx") != std::string::npos);
        }

#pragma pack(push, 1)
        struct GbMaterialFrameRaw {
            std::uint32_t ambient = 0;
            std::uint32_t diffuse = 0;
            std::uint32_t specular = 0;
            float opacity = 1.0f;
            float offset_u = 0.0f;
            float offset_v = 0.0f;
            float angle_x = 0.0f;
            float angle_y = 0.0f;
            float angle_z = 0.0f;
        };
#pragma pack(pop)
        static_assert(sizeof(GbMaterialFrameRaw) == 36, "GB_MATERIAL_FRAME size");

        std::string cstr_at(const std::vector<std::uint8_t>& strings, std::uint32_t off) {
            if (off >= strings.size()) {
                return {};
            }
            const char* p = reinterpret_cast<const char*>(strings.data() + off);
            const char* end = reinterpret_cast<const char*>(strings.data() + strings.size());
            const char* e = p;
            while (e < end && *e) {
                ++e;
            }
            return std::string(p, e);
        }

        std::string cstr_after(const std::vector<std::uint8_t>& strings, std::uint32_t off) {
            const std::string first = cstr_at(strings, off);
            const std::uint32_t next = off + static_cast<std::uint32_t>(first.size()) + 1u;
            return cstr_at(strings, next);
        }

        void expand_strip(const std::uint16_t* idx, int count, std::vector<std::uint32_t>& out) {
            if (count < 3) {
                return;
            }
            for (int i = 0; i + 2 < count; ++i) {
                const std::uint32_t a = idx[i];
                const std::uint32_t b = idx[i + 1];
                const std::uint32_t c = idx[i + 2];
                if (a == b || b == c || a == c) {
                    continue;  // degenerate
                }
                if ((i & 1) == 0) {
                    out.push_back(a);
                    out.push_back(b);
                    out.push_back(c);
                } else {
                    out.push_back(a);
                    out.push_back(c);
                    out.push_back(b);
                }
            }
        }

        std::string replace_ext(std::string path, const char* ext) {
            const auto dot = path.find_last_of('.');
            if (dot != std::string::npos) {
                path.resize(dot);
            }
            path += ext;
            return path;
        }

        std::string parent_dir(const std::string& path) {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string::npos) {
                return ".";
            }
            return path.substr(0, slash);
        }

        // OBJECTS/.../model.gb ?? OBJECTS (case-insensitive component match).
        std::string objects_dir_from_model(const std::string& model_gb_path) {
            const std::string norm = platform::normalize_separators(model_gb_path);
            std::string cur = norm;
            while (true) {
                const auto slash = cur.find_last_of('/');
                if (slash == std::string::npos) {
                    break;
                }
                const std::string leaf = cur.substr(slash + 1);
                std::string lower = leaf;
                for (char& c : lower) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lower == "objects") {
                    return cur;
                }
                cur = cur.substr(0, slash);
            }
            return {};
        }

        bool try_load_tex_in_dir(const std::string& dir, const std::string& stem, ImageRgba& out) {
            static const char* kExts[] = {".gtx", ".dds", ".tga", ".png"};
            const auto load = [&](const std::string& path, std::string_view extension) {
                if (extension == ".png") {
                    return load_png_rgba(path, out);
                }
                if (extension == ".tga") {
                    return load_tga_rgba(path, out);
                }
                return load_gtx_rgba(path, out);
            };
            const auto entries = platform::list_directory(dir);
            std::string want = stem;
            for (char& c : want) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            for (const char* ext : kExts) {
                const std::string path = platform::join_path(dir, stem + ext);
                if (platform::file_exists(path) && load(path, ext)) {
                    return true;
                }
                // Keep legacy format precedence even for mixed-case filenames.
                for (const auto& ent : entries) {
                    std::string e = ent;
                    for (char& c : e) {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                    const auto dot = e.find_last_of('.');
                    if (dot == std::string::npos || e.substr(0, dot) != want ||
                        e.substr(dot) != ext) {
                        continue;
                    }
                    const std::string candidate = platform::join_path(dir, ent);
                    if (candidate != path && load(candidate, ext)) {
                        return true;
                    }
                }
            }
            return false;
        }

    }  // namespace

    bool is_waterfall_texture(const std::string& name) {
        std::string lower = name;
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return lower.find("waterfall") != std::string::npos;
    }

    bool is_water_surface_texture(const std::string& name) {
        std::string lower = name;
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lower.find("waterfall") != std::string::npos) {
            return false;
        }
        return lower.find("water") != std::string::npos;
    }

    bool sample_material_uv(const GbMaterial& mat,
                            const std::vector<std::uint16_t>& key_times,
                            float time_ms,
                            float& out_u,
                            float& out_v,
                            float& out_opacity) {
        out_u = 0.0f;
        out_v = 0.0f;
        out_opacity = 1.0f;
        if (mat.frames.size() < 2 || key_times.size() < 2 || mat.frames.size() < key_times.size()) {
            if (!mat.frames.empty()) {
                out_u = mat.frames[0].offset_u;
                out_v = mat.frames[0].offset_v;
                out_opacity = mat.frames[0].opacity;
            }
            return false;
        }
        const float duration = static_cast<float>(key_times.back());
        if (duration <= 0.0f) {
            out_u = mat.frames[0].offset_u;
            out_v = mat.frames[0].offset_v;
            out_opacity = mat.frames[0].opacity;
            return true;
        }
        float t = std::fmod(time_ms, duration);
        if (t < 0.0f) {
            t += duration;
        }
        std::size_t i0 = 0;
        for (std::size_t i = 0; i + 1 < key_times.size(); ++i) {
            if (t >= static_cast<float>(key_times[i]) &&
                t <= static_cast<float>(key_times[i + 1])) {
                i0 = i;
                break;
            }
            i0 = i;
        }
        const std::size_t i1 = std::min(i0 + 1, key_times.size() - 1);
        const float t0 = static_cast<float>(key_times[i0]);
        const float t1 = static_cast<float>(key_times[i1]);
        float s = 0.0f;
        if (t1 > t0) {
            s = (t - t0) / (t1 - t0);
        }
        const GbMaterialFrame& a = mat.frames[i0];
        const GbMaterialFrame& b = mat.frames[i1];
        out_u = a.offset_u + (b.offset_u - a.offset_u) * s;
        out_v = a.offset_v + (b.offset_v - a.offset_v) * s;
        out_opacity = a.opacity + (b.opacity - a.opacity) * s;
        return true;
    }

    bool GbLoader::load(const std::string& path, GbModel& out) {
        out = {};
        out.path = path;
        std::string read_error;
        const auto bytes = platform::read_file(path, &read_error);
        if (!bytes || bytes->empty()) {
            const std::string reason = bytes ? "file is empty" : read_error;
            log::error("load_gb: cannot read '" + path + "': " + reason);
            return false;
        }

        std::size_t offset = 0;
        ModelFileHeader header{};
        if (!read_gb_header(*bytes, offset, header)) {
            const int ver = bytes->empty() ? -1 : static_cast<int>((*bytes)[0]);
            log::warn("load_gb: unsupported or truncated version " + std::to_string(ver) + " in " +
                      path);
            return false;
        }
        if (header.textTableBytes > kMaxStringBytes || header.collisionBytes > kMaxClsBytes) {
            log::warn("load_gb: absurd string/cls size in " + path);
            return false;
        }

        out.min_x = header.lowerBounds[0];
        out.min_y = header.lowerBounds[1];
        out.min_z = header.lowerBounds[2];
        out.max_x = header.upperBounds[0];
        out.max_y = header.upperBounds[1];
        out.max_z = header.upperBounds[2];
        out.bone_count = header.boneTotal;

        if (header.featureFlags & kModelBoneFlag) {
            out.bones.resize(header.boneTotal);
            for (std::uint8_t bi = 0; bi < header.boneTotal; ++bi) {
                if (!read_pod(*bytes, offset, out.bones[bi].inverse.data(), 64) ||
                    !read_pod(*bytes, offset, &out.bones[bi].parent, 1)) {
                    return false;
                }
            }
        }

        std::vector<MaterialFileRecord> keys;
        std::vector<float> v7_opacity;
        std::vector<GbMaterialFrameRaw> v7_frames;
        struct MeshRaw {
            MeshFileHeader hdr{};
            std::vector<std::uint8_t> palette;
            std::vector<std::uint8_t> verts;
            std::vector<std::uint16_t> indices;
        };
        std::vector<MeshRaw> raw_meshes;
        raw_meshes.reserve(header.meshTotal);

        auto read_mesh = [&](MeshRaw& mesh) -> bool {
            if (!read_pod(*bytes, offset, &mesh.hdr, sizeof(MeshFileHeader))) {
                return false;
            }
            mesh.palette.resize(mesh.hdr.paletteSize);
            if (mesh.hdr.paletteSize) {
                if (!read_pod(*bytes, offset, mesh.palette.data(), mesh.hdr.paletteSize)) {
                    return false;
                }
            }
            const int vtype = mesh.hdr.vertexLayout;
            const int vsize = vertex_stride(header.formatVersion, vtype);
            if (vsize < 0) {
                log::warn("load_gb: bad vertex type " + std::to_string(vtype) + " (ver " +
                          std::to_string(header.formatVersion) + ") in " + path);
                return false;
            }
            const std::size_t vbytes =
                static_cast<std::size_t>(vsize) * static_cast<std::size_t>(mesh.hdr.vertexTotal);
            if (vbytes > kMaxVertexBytesPerMesh ||
                static_cast<std::size_t>(mesh.hdr.indexTotal) * sizeof(std::uint16_t) >
                    kMaxIndexBytesPerMesh) {
                log::warn("load_gb: mesh too large in " + path);
                return false;
            }
            mesh.verts.resize(vbytes);
            if (!read_pod(*bytes, offset, mesh.verts.data(), vbytes)) {
                return false;
            }
            mesh.indices.resize(mesh.hdr.indexTotal);
            return read_pod(
                *bytes, offset, mesh.indices.data(), mesh.indices.size() * sizeof(std::uint16_t));
        };

        if (header.formatVersion == 7) {
            // v7 stores one inline material immediately before each mesh (22 bytes, 26
            // after the first).
            keys.resize(header.meshTotal);
            v7_opacity.resize(header.meshTotal, 1.0f);
            v7_frames.resize(header.meshTotal);
            for (std::uint8_t mi = 0; mi < header.meshTotal; ++mi) {
                if (mi == 0) {
                    LegacyMaterialRecord mat{};
                    if (!read_pod(*bytes, offset, &mat, sizeof(mat))) {
                        return false;
                    }
                    keys[mi].textureTextOffset = mat.textureTextOffset;
                    keys[mi].mappingFlags = mat.mappingFlags;
                    v7_opacity[mi] = mat.opacity;
                    v7_frames[mi].ambient = mat.ambient;
                    v7_frames[mi].diffuse = mat.diffuse;
                    v7_frames[mi].specular = mat.specular;
                } else {
                    LegacyNamedMaterialRecord mat{};
                    if (!read_pod(*bytes, offset, &mat, sizeof(mat))) {
                        return false;
                    }
                    keys[mi].textureTextOffset = mat.textureTextOffset;
                    keys[mi].optionTextOffset = mat.optionTextOffset;
                    keys[mi].mappingFlags = mat.mappingFlags;
                    v7_opacity[mi] = mat.opacity;
                    v7_frames[mi].ambient = mat.ambient;
                    v7_frames[mi].diffuse = mat.diffuse;
                    v7_frames[mi].specular = mat.specular;
                }
                MeshRaw mesh;
                if (!read_mesh(mesh)) {
                    return false;
                }
                mesh.hdr.materialIndex = static_cast<std::int32_t>(mi);
                raw_meshes.push_back(std::move(mesh));
            }
        } else {
            keys.resize(header.materialTotal);
            for (std::uint16_t i = 0; i < header.materialTotal; ++i) {
                if (!read_pod(*bytes, offset, &keys[i], sizeof(MaterialFileRecord))) {
                    return false;
                }
            }
            for (std::uint8_t mi = 0; mi < header.meshTotal; ++mi) {
                MeshRaw mesh;
                if (!read_mesh(mesh)) {
                    return false;
                }
                raw_meshes.push_back(std::move(mesh));
            }
        }

        // Anim files then shared GB_ANIM pool.
        struct AnimFileRaw {
            std::uint32_t optionTextOffset = 0;
            std::uint16_t keyframeTotal = 0;
            std::vector<std::uint16_t> key_times;
            std::vector<std::uint32_t> key_opt_offs;
            std::vector<std::uint16_t> remap;  // kf * bone_count
        };
        std::vector<AnimFileRaw> anim_files(header.clipTotal);
        for (std::uint8_t i = 0; i < header.clipTotal; ++i) {
            if (!read_pod(*bytes, offset, &anim_files[i].optionTextOffset, 4) ||
                !read_pod(*bytes, offset, &anim_files[i].keyframeTotal, 2)) {
                return false;
            }
            const std::uint16_t kf = anim_files[i].keyframeTotal;
            const std::size_t key_bytes = static_cast<std::size_t>(kf) * kGbKeyframeBytes;
            const std::size_t index_bytes = static_cast<std::size_t>(kf) *
                                            static_cast<std::size_t>(header.boneTotal) *
                                            sizeof(std::uint16_t);
            if (offset + key_bytes + index_bytes > bytes->size()) {
                log::warn("load_gb: truncated anim data in " + path);
                return false;
            }
            anim_files[i].key_times.resize(kf);
            anim_files[i].key_opt_offs.resize(kf);
            for (std::uint16_t k = 0; k < kf; ++k) {
                std::uint16_t time = 0;
                std::uint32_t opt = 0;
                if (!read_pod(*bytes, offset, &time, 2) || !read_pod(*bytes, offset, &opt, 4)) {
                    return false;
                }
                anim_files[i].key_times[k] = time;
                anim_files[i].key_opt_offs[k] = opt;
            }
            anim_files[i].remap.resize(static_cast<std::size_t>(kf) * header.boneTotal);
            if (kf && header.boneTotal) {
                if (!read_pod(*bytes, offset, anim_files[i].remap.data(), index_bytes)) {
                    return false;
                }
            }
        }
        std::vector<GbAnimSample> anim_pool(header.animationSampleTotal);
        for (std::uint16_t ai = 0; ai < header.animationSampleTotal; ++ai) {
            if (!read_pod(*bytes, offset, &anim_pool[ai].px, 12) ||
                !read_pod(*bytes, offset, &anim_pool[ai].qx, 16) ||
                !read_pod(*bytes, offset, &anim_pool[ai].sx, 12)) {
                return false;
            }
        }

        // Collision blob. v7 stores cls_size as a WORD so values >= 65536 wrap;
        // recover from EOF.
        if (header.formatVersion == 7 && offset <= bytes->size() &&
            bytes->size() - offset >= header.textTableBytes) {
            const std::uint32_t actual_cls =
                static_cast<std::uint32_t>(bytes->size() - offset - header.textTableBytes);
            if ((actual_cls & 0xFFFFu) == (header.collisionBytes & 0xFFFFu)) {
                header.collisionBytes = actual_cls;
            }
        }
        if (header.collisionBytes > 0 && offset + header.collisionBytes <= bytes->size()) {
            parse_cls_tris(
                bytes->data() + offset, header.collisionBytes, header.formatVersion, out);
        }
        offset += header.collisionBytes;
        if (offset > bytes->size()) {
            return false;
        }

        if (offset + header.textTableBytes > bytes->size()) {
            log::warn("load_gb: string table past EOF in " + path);
            return false;
        }
        std::vector<std::uint8_t> strings(header.textTableBytes);
        if (!read_pod(*bytes, offset, strings.data(), header.textTableBytes)) {
            return false;
        }

        out.materials.resize(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) {
            out.materials[i].texture = cstr_at(strings, keys[i].textureTextOffset);
            out.materials[i].option = cstr_at(strings, keys[i].optionTextOffset);
            out.materials[i].mapping_flags = keys[i].mappingFlags;
            out.materials[i].specular_power =
                header.formatVersion == 7 ? 40.0f : keys[i].specularPower;
            if (header.formatVersion >= 11 && (keys[i].mappingFlags & kMatLightmap) != 0) {
                out.materials[i].lightmap = cstr_after(strings, keys[i].textureTextOffset);
            }
            if (header.formatVersion == 7) {
                if (i > 0 && !looks_like_texture_name(out.materials[i].texture) &&
                    looks_like_texture_name(out.materials[0].texture)) {
                    out.materials[i].texture = out.materials[0].texture;
                }
                GbMaterialFrame frame;
                frame.opacity = (i < v7_opacity.size()) ? v7_opacity[i] : 1.0f;
                frame.ambient = v7_frames[i].ambient;
                frame.diffuse = v7_frames[i].diffuse;
                frame.specular = v7_frames[i].specular;
                out.materials[i].frames.push_back(frame);
                continue;
            }
            if (header.materialFrameTotal > 0 &&
                keys[i].frameDataOffset + sizeof(GbMaterialFrameRaw) <= strings.size()) {
                out.materials[i].frames.resize(header.materialFrameTotal);
                for (std::uint16_t fi = 0; fi < header.materialFrameTotal; ++fi) {
                    const std::size_t off = static_cast<std::size_t>(keys[i].frameDataOffset) +
                                            fi * sizeof(GbMaterialFrameRaw);
                    if (off + sizeof(GbMaterialFrameRaw) > strings.size()) {
                        out.materials[i].frames.resize(fi);
                        break;
                    }
                    GbMaterialFrameRaw raw{};
                    std::memcpy(&raw, strings.data() + off, sizeof(raw));
                    out.materials[i].frames[fi].opacity = raw.opacity;
                    out.materials[i].frames[fi].offset_u = raw.offset_u;
                    out.materials[i].frames[fi].offset_v = raw.offset_v;
                    out.materials[i].frames[fi].ambient = raw.ambient;
                    out.materials[i].frames[fi].diffuse = raw.diffuse;
                    out.materials[i].frames[fi].specular = raw.specular;
                }
            }
        }

        out.option = cstr_at(strings, header.optionTextOffset);
        const bool model_is_water = out.option.find("Water") != std::string::npos ||
                                    out.option.find("water") != std::string::npos;

        for (auto& af : anim_files) {
            GbAnimClip clip;
            clip.keyframe_count = af.keyframeTotal;
            clip.bone_count = header.boneTotal;
            clip.key_times = std::move(af.key_times);
            clip.key_options.resize(af.key_opt_offs.size());
            for (std::size_t k = 0; k < af.key_opt_offs.size(); ++k) {
                clip.key_options[k] = cstr_at(strings, af.key_opt_offs[k]);
            }
            {
                const std::string opt = cstr_at(strings, af.optionTextOffset);
                const char* loop = std::strstr(opt.c_str(), "(*LOOP");
                if (!loop) {
                    // Some motions store the loop directive on a keyframe instead of the clip.
                    for (const auto& key_option : clip.key_options) {
                        loop = std::strstr(key_option.c_str(), "(*LOOP");
                        if (loop) {
                            break;
                        }
                    }
                }
                if (loop) {
                    loop += 6;
                    clip.loop_start_ms = static_cast<float>(std::atoi(loop));
                }
            }
            if (af.keyframeTotal > 0 && header.boneTotal > 0) {
                const std::size_t n = static_cast<std::size_t>(af.keyframeTotal) *
                                      static_cast<std::size_t>(header.boneTotal);
                clip.samples.resize(n);
                for (std::size_t si = 0; si < n; ++si) {
                    const std::uint16_t rem = af.remap[si];
                    if (rem < anim_pool.size()) {
                        clip.samples[si] = anim_pool[rem];
                    }
                }
                clip.bone0.resize(header.boneTotal);
                for (std::uint8_t bi = 0; bi < header.boneTotal; ++bi) {
                    clip.bone0[bi] = clip.samples[bi];
                }
            }
            out.anims.push_back(std::move(clip));
        }

        // Material UV anim shares the first anim clip's key times
        // (compatibility behavior).
        if (header.materialFrameTotal > 1 && !out.anims.empty() &&
            out.anims[0].keyframe_count == static_cast<int>(header.materialFrameTotal)) {
            out.material_key_times = out.anims[0].key_times;
        }

        for (const auto& raw : raw_meshes) {
            const int file_vtype = raw.hdr.vertexLayout;
            const int vsize = vertex_stride(header.formatVersion, file_vtype);
            const int uv_off = vertex_uv_offset(header.formatVersion, file_vtype);
            if (vsize < 0 || uv_off < 0) {
                continue;
            }
            const int logical = logical_vertex_type(header.formatVersion, file_vtype);

            GbMeshCpu mesh;
            mesh.name = cstr_at(strings, raw.hdr.nameTextOffset);
            mesh.material_index = raw.hdr.materialIndex;
            mesh.vertex_layout = static_cast<GbVertexType>(std::min(std::max(logical, 0), 5));
            mesh.bone_palette = raw.palette;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(out.materials.size())) {
                // Match KalOnline compatibility behavior: MAP_OPACITY alone is alpha *test*
                // (ALPHAREF 128, Z-write on). Soft blend + Z-write off only with
                // MAP_ALPHA_RGB.
                const auto opt = out.materials[mesh.material_index].mapping_flags;
                const float power = out.materials[mesh.material_index].specular_power;
                if ((opt & kMatSpecular) != 0 && std::isfinite(power) && power >= 0.0f) {
                    mesh.specular_power = std::clamp(power, 1.0f, 1024.0f);
                }
                mesh.two_sided = (opt & kMatTwoSided) != 0;
                const bool map_opacity = (opt & kMatMapOpacity) != 0;
                const bool map_alpha_rgb = (opt & kMatMapAlphaRgb) != 0;
                mesh.alpha_test = map_opacity && !map_alpha_rgb;
                mesh.alpha_blend = map_alpha_rgb;
                const std::string& tex = out.materials[mesh.material_index].texture;
                if (is_water_texture_name(tex) || model_is_water || is_waterfall_texture(tex)) {
                    mesh.alpha_test = false;
                    mesh.alpha_blend = true;
                    mesh.two_sided = true;
                }
                // Material opacity anim < 1 also needs blending (waterfall sheets).
                for (const auto& fr : out.materials[mesh.material_index].frames) {
                    if (fr.opacity < 0.999f) {
                        mesh.alpha_blend = true;
                        mesh.alpha_test = false;
                        break;
                    }
                }
            }

            const bool is_blend = logical >= 1 && logical <= 4;
            const bool is_lightmap = logical == 5;
            mesh.pos_uv.resize(static_cast<std::size_t>(raw.hdr.vertexTotal) * 5);
            mesh.normals.resize(raw.hdr.vertexTotal);
            if (is_lightmap) {
                mesh.lightmap_uv.resize(static_cast<std::size_t>(raw.hdr.vertexTotal) * 2);
            }
            if (is_blend) {
                mesh.skin_verts.resize(raw.hdr.vertexTotal);
            }
            for (std::uint16_t vi = 0; vi < raw.hdr.vertexTotal; ++vi) {
                const std::uint8_t* src = raw.verts.data() + static_cast<std::size_t>(vi) * vsize;
                float pos[3];
                float uv[2];
                std::memcpy(pos, src, 12);
                std::memcpy(uv, src + uv_off, 8);
                // Normals precede the first UV pair in every supported vertex layout.
                std::memcpy(mesh.normals[vi].data(), src + uv_off - 12, 12);
                const std::size_t o = static_cast<std::size_t>(vi) * 5;
                mesh.pos_uv[o + 0] = pos[0];
                mesh.pos_uv[o + 1] = pos[1];
                mesh.pos_uv[o + 2] = pos[2];
                mesh.pos_uv[o + 3] = uv[0];
                mesh.pos_uv[o + 4] = uv[1];
                if (is_lightmap && vsize >= 40) {
                    float lm[2];
                    std::memcpy(lm, src + 32, 8);
                    const std::size_t lo = static_cast<std::size_t>(vi) * 2;
                    mesh.lightmap_uv[lo] = lm[0];
                    mesh.lightmap_uv[lo + 1] = lm[1];
                }
                if (!is_blend) {
                    continue;
                }
                GbSkinVert& sv = mesh.skin_verts[vi];
                sv.px = pos[0];
                sv.py = pos[1];
                sv.pz = pos[2];
                sv.u = uv[0];
                sv.v = uv[1];
                sv.normal = mesh.normals[vi];
                // Layout after pos: Blend1 indices@12; BlendN weights then indices.
                int weight_count =
                    logical;  // Blend1??1 bone (no explicit weights), Blend2??1 weight, ??
                if (logical == 1) {
                    std::uint32_t packed = 0;
                    std::memcpy(&packed, src + 12, 4);
                    sv.idx[0] = static_cast<std::uint8_t>(packed & 0xff);
                    sv.idx[1] = static_cast<std::uint8_t>((packed >> 8) & 0xff);
                    sv.idx[2] = static_cast<std::uint8_t>((packed >> 16) & 0xff);
                    sv.idx[3] = static_cast<std::uint8_t>((packed >> 24) & 0xff);
                    sv.influence_count = 1;
                } else {
                    const int n_weights = logical - 1;  // Blend2??1, Blend3??2, Blend4??3
                    std::memcpy(sv.w, src + 12, static_cast<std::size_t>(n_weights) * 4);
                    std::uint32_t packed = 0;
                    std::memcpy(&packed, src + 12 + n_weights * 4, 4);
                    sv.idx[0] = static_cast<std::uint8_t>(packed & 0xff);
                    sv.idx[1] = static_cast<std::uint8_t>((packed >> 8) & 0xff);
                    sv.idx[2] = static_cast<std::uint8_t>((packed >> 16) & 0xff);
                    sv.idx[3] = static_cast<std::uint8_t>((packed >> 24) & 0xff);
                    sv.influence_count = logical;
                    (void) weight_count;
                }
            }

            // Rigid / RigidDouble clothes (e.g. knight face) parent to palette[0] ?
            // synthesize single-bone influences so compatibility behavior skin follows the head
            // bone like Blend meshes.
            if (!is_blend && !mesh.bone_palette.empty() && mesh.skin_verts.empty() &&
                raw.hdr.vertexTotal > 0) {
                mesh.skin_verts.resize(raw.hdr.vertexTotal);
                for (std::uint16_t vi = 0; vi < raw.hdr.vertexTotal; ++vi) {
                    const std::size_t o = static_cast<std::size_t>(vi) * 5;
                    GbSkinVert& sv = mesh.skin_verts[vi];
                    sv.px = mesh.pos_uv[o + 0];
                    sv.py = mesh.pos_uv[o + 1];
                    sv.pz = mesh.pos_uv[o + 2];
                    sv.u = mesh.pos_uv[o + 3];
                    sv.v = mesh.pos_uv[o + 4];
                    sv.normal = mesh.normals[vi];
                    sv.idx[0] = 0;  // first palette entry (head bone for faces)
                    sv.influence_count = 1;
                }
            }

            if (is_lightmap ||
                (mesh.material_index >= 0 &&
                 mesh.material_index < static_cast<int>(out.materials.size()) &&
                 (out.materials[mesh.material_index].mapping_flags & kMatLightmap) != 0)) {
                if (header.formatVersion >= 11 && mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(out.materials.size())) {
                    mesh.lightmap = out.materials[mesh.material_index].lightmap;
                    if (mesh.lightmap.empty() && mesh.material_index >= 0 &&
                        static_cast<std::size_t>(mesh.material_index) < keys.size()) {
                        mesh.lightmap = cstr_after(
                            strings,
                            keys[static_cast<std::size_t>(mesh.material_index)].textureTextOffset);
                    }
                } else {
                    mesh.lightmap = cstr_after(strings, raw.hdr.nameTextOffset);
                }
                if (mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(out.materials.size()) &&
                    out.materials[mesh.material_index].lightmap.empty()) {
                    out.materials[mesh.material_index].lightmap = mesh.lightmap;
                }
            }

            if (raw.hdr.indexLayout == static_cast<std::uint8_t>(GbFaceType::List)) {
                mesh.indices.reserve(raw.indices.size());
                for (std::uint16_t idx : raw.indices) {
                    mesh.indices.push_back(idx);
                }
            } else {
                expand_strip(
                    raw.indices.data(), static_cast<int>(raw.indices.size()), mesh.indices);
            }
            if (!mesh.indices.empty()) {
                out.meshes.push_back(std::move(mesh));
            }
        }

        if (!out.bones.empty() && !out.anims.empty() && !out.anims[0].bone0.empty()) {
            GbSkin::apply_embedded_idle(out);
        }

        return !out.materials.empty() || !out.meshes.empty() || !out.bones.empty() ||
               !out.anims.empty() || !out.cls_tris.empty();
    }

    namespace {

        void d3d_mul(float* out, const float* a, const float* b) {
            float t[16];
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    t[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] +
                                   a[r * 4 + 2] * b[2 * 4 + c] + a[r * 4 + 3] * b[3 * 4 + c];
                }
            }
            std::memcpy(out, t, sizeof(t));
        }

        void d3d_identity(float* m) {
            std::memset(m, 0, 64);
            m[0] = m[5] = m[10] = m[15] = 1.0f;
        }

        void trs_to_d3d(float* out, const GbAnimSample& s) {
            // D3DXMatrixRotationQuaternion
            const float x = s.qx, y = s.qy, z = s.qz, w = s.qw;
            const float xx = x * x, yy = y * y, zz = z * z;
            const float xy = x * y, xz = x * z, yz = y * z;
            const float wx = w * x, wy = w * y, wz = w * z;
            d3d_identity(out);
            out[0] = 1.0f - 2.0f * (yy + zz);
            out[1] = 2.0f * (xy + wz);
            out[2] = 2.0f * (xz - wy);
            out[4] = 2.0f * (xy - wz);
            out[5] = 1.0f - 2.0f * (xx + zz);
            out[6] = 2.0f * (yz + wx);
            out[8] = 2.0f * (xz + wy);
            out[9] = 2.0f * (yz - wx);
            out[10] = 1.0f - 2.0f * (xx + yy);
            // Scale rows (D3DX: scale axes of rotation matrix)
            out[0] *= s.sx;
            out[1] *= s.sx;
            out[2] *= s.sx;
            out[4] *= s.sy;
            out[5] *= s.sy;
            out[6] *= s.sy;
            out[8] *= s.sz;
            out[9] *= s.sz;
            out[10] *= s.sz;
            out[12] = s.px;
            out[13] = s.py;
            out[14] = s.pz;
        }

        void d3d_transform_point(
            const float* m, float x, float y, float z, float& ox, float& oy, float& oz) {
            ox = x * m[0] + y * m[4] + z * m[8] + m[12];
            oy = x * m[1] + y * m[5] + z * m[9] + m[13];
            oz = x * m[2] + y * m[6] + z * m[10] + m[14];
        }

        void lerp3(float& ox,
                   float& oy,
                   float& oz,
                   float ax,
                   float ay,
                   float az,
                   float bx,
                   float by,
                   float bz,
                   float t) {
            ox = ax + (bx - ax) * t;
            oy = ay + (by - ay) * t;
            oz = az + (bz - az) * t;
        }

        void quat_slerp(float& ox,
                        float& oy,
                        float& oz,
                        float& ow,
                        float ax,
                        float ay,
                        float az,
                        float aw,
                        float bx,
                        float by,
                        float bz,
                        float bw,
                        float t) {
            float cos_theta = ax * bx + ay * by + az * bz + aw * bw;
            if (cos_theta < 0.0f) {
                bx = -bx;
                by = -by;
                bz = -bz;
                bw = -bw;
                cos_theta = -cos_theta;
            }
            if (cos_theta > 0.9995f) {
                ox = ax + (bx - ax) * t;
                oy = ay + (by - ay) * t;
                oz = az + (bz - az) * t;
                ow = aw + (bw - aw) * t;
                const float len = std::sqrt(ox * ox + oy * oy + oz * oz + ow * ow);
                if (len > 1e-8f) {
                    const float inv = 1.0f / len;
                    ox *= inv;
                    oy *= inv;
                    oz *= inv;
                    ow *= inv;
                }
                return;
            }
            const float theta = std::acos(std::min(1.0f, cos_theta));
            const float sin_theta = std::sin(theta);
            const float w0 = std::sin((1.0f - t) * theta) / sin_theta;
            const float w1 = std::sin(t * theta) / sin_theta;
            ox = ax * w0 + bx * w1;
            oy = ay * w0 + by * w1;
            oz = az * w0 + bz * w1;
            ow = aw * w0 + bw * w1;
        }

        GbAnimSample lerp_sample(const GbAnimSample& a, const GbAnimSample& b, float t) {
            GbAnimSample o;
            lerp3(o.px, o.py, o.pz, a.px, a.py, a.pz, b.px, b.py, b.pz, t);
            quat_slerp(o.qx, o.qy, o.qz, o.qw, a.qx, a.qy, a.qz, a.qw, b.qx, b.qy, b.qz, b.qw, t);
            lerp3(o.sx, o.sy, o.sz, a.sx, a.sy, a.sz, b.sx, b.sy, b.sz, t);
            return o;
        }

        bool skin_from_local_samples(const std::vector<GbBone>& bones,
                                     const std::vector<GbAnimSample>& locals,
                                     std::vector<std::array<float, 16>>* bone_world_out,
                                     std::vector<std::array<float, 16>>& skin_out) {
            if (bones.empty() || locals.size() != bones.size()) {
                return false;
            }
            const int n = static_cast<int>(bones.size());
            std::vector<std::array<float, 16>> bone_world(static_cast<std::size_t>(n));
            float identity[16];
            d3d_identity(identity);

            for (int i = 0; i < n; ++i) {
                float local[16];
                trs_to_d3d(local, locals[static_cast<std::size_t>(i)]);
                const std::uint8_t parent = bones[static_cast<std::size_t>(i)].parent;
                if (parent < static_cast<std::uint8_t>(n)) {
                    d3d_mul(bone_world[static_cast<std::size_t>(i)].data(),
                            local,
                            bone_world[parent].data());
                } else {
                    d3d_mul(bone_world[static_cast<std::size_t>(i)].data(), local, identity);
                }
            }

            skin_out.resize(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                d3d_mul(skin_out[static_cast<std::size_t>(i)].data(),
                        bones[static_cast<std::size_t>(i)].inverse.data(),
                        bone_world[static_cast<std::size_t>(i)].data());
            }
            if (bone_world_out) {
                *bone_world_out = std::move(bone_world);
            }
            return true;
        }

        bool skin_from_local_samples(const std::vector<GbBone>& bones,
                                     const std::vector<GbAnimSample>& locals,
                                     std::vector<std::array<float, 16>>& skin_out) {
            return skin_from_local_samples(bones, locals, nullptr, skin_out);
        }

    }  // namespace

    bool
    GbSkin::eval_bone_and_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                                const GbAnimClip& clip,
                                                float time_ms,
                                                bool loop,
                                                std::vector<std::array<float, 16>>& bone_world_out,
                                                std::vector<std::array<float, 16>>& skin_out) {
        if (bones.empty() || clip.keyframe_count <= 0 ||
            clip.bone_count != static_cast<int>(bones.size()) ||
            clip.samples.size() != static_cast<std::size_t>(clip.keyframe_count) *
                                       static_cast<std::size_t>(clip.bone_count)) {
            if (!clip.bone0.empty() && clip.bone0.size() == bones.size()) {
                return skin_from_local_samples(bones, clip.bone0, &bone_world_out, skin_out);
            }
            return false;
        }

        const int n = clip.bone_count;
        const int kf = clip.keyframe_count;
        if (kf == 1) {
            std::vector<GbAnimSample> locals(clip.samples.begin(), clip.samples.begin() + n);
            return skin_from_local_samples(bones, locals, &bone_world_out, skin_out);
        }

        float t = time_ms;
        const float dur = clip.duration_ms();
        if (loop && dur > 0.0f) {
            if (t >= dur) {
                float start = clip.loop_start_ms;
                if (start < 0.0f) {
                    start = 0.0f;
                }
                if (start >= dur) {
                    start = 0.0f;
                }
                const float span = dur - start;
                if (span > 1e-3f) {
                    t = start + std::fmod(t - start, span);
                    if (t < start) {
                        t += span;
                    }
                } else {
                    t = std::fmod(t, dur);
                }
            }
            if (t < 0.0f) {
                t = 0.0f;
            }
        } else if (t < 0.0f) {
            t = 0.0f;
        } else if (t > dur) {
            t = dur;
        }

        int i0 = 0;
        for (int i = 0; i + 1 < kf; ++i) {
            if (t >= static_cast<float>(clip.key_times[static_cast<std::size_t>(i)]) &&
                t <= static_cast<float>(clip.key_times[static_cast<std::size_t>(i + 1)])) {
                i0 = i;
                break;
            }
            if (t > static_cast<float>(clip.key_times[static_cast<std::size_t>(i)])) {
                i0 = i;
            }
        }
        const int i1 = std::min(i0 + 1, kf - 1);
        const float t0 = static_cast<float>(clip.key_times[static_cast<std::size_t>(i0)]);
        const float t1 = static_cast<float>(clip.key_times[static_cast<std::size_t>(i1)]);
        float alpha = 0.0f;
        if (i1 != i0 && t1 > t0) {
            alpha = (t - t0) / (t1 - t0);
            if (alpha < 0.0f) {
                alpha = 0.0f;
            }
            if (alpha > 1.0f) {
                alpha = 1.0f;
            }
        }

        std::vector<GbAnimSample> locals(static_cast<std::size_t>(n));
        const std::size_t base0 = static_cast<std::size_t>(i0) * static_cast<std::size_t>(n);
        const std::size_t base1 = static_cast<std::size_t>(i1) * static_cast<std::size_t>(n);
        for (int bi = 0; bi < n; ++bi) {
            locals[static_cast<std::size_t>(bi)] =
                lerp_sample(clip.samples[base0 + static_cast<std::size_t>(bi)],
                            clip.samples[base1 + static_cast<std::size_t>(bi)],
                            alpha);
        }
        return skin_from_local_samples(bones, locals, &bone_world_out, skin_out);
    }

    bool GbSkin::eval_matrices_at_time(const std::vector<GbBone>& bones,
                                       const GbAnimClip& clip,
                                       float time_ms,
                                       bool loop,
                                       std::vector<std::array<float, 16>>& skin_out) {
        std::vector<std::array<float, 16>> unused_world;
        return eval_bone_and_skin_matrices_at_time(
            bones, clip, time_ms, loop, unused_world, skin_out);
    }

    bool GbSkin::eval_matrices_kf0(const GbModel& model,
                                   int anim_index,
                                   std::vector<std::array<float, 16>>& skin_out) {
        if (model.bones.empty() || anim_index < 0 ||
            anim_index >= static_cast<int>(model.anims.size())) {
            return false;
        }
        return eval_matrices_at_time(
            model.bones, model.anims[static_cast<std::size_t>(anim_index)], 0.0f, false, skin_out);
    }

    namespace {
        std::array<float, 9> normal_matrix(const std::array<float, 16>& m) {
            std::array<float, 9> cofactor{m[5] * m[10] - m[9] * m[6],
                                          m[9] * m[2] - m[1] * m[10],
                                          m[1] * m[6] - m[5] * m[2],
                                          m[8] * m[6] - m[4] * m[10],
                                          m[0] * m[10] - m[8] * m[2],
                                          m[4] * m[2] - m[0] * m[6],
                                          m[4] * m[9] - m[8] * m[5],
                                          m[8] * m[1] - m[0] * m[9],
                                          m[0] * m[5] - m[4] * m[1]};
            const float det = m[0] * cofactor[0] + m[4] * cofactor[1] + m[8] * cofactor[2];
            if (!std::isfinite(det) || std::fabs(det) < 1e-12f) {
                return {};
            }
            for (float& value : cofactor) {
                value /= det;
            }
            return cofactor;
        }

        std::array<float, 3> transform_normal(const std::array<float, 9>& m,
                                              const std::array<float, 3>& n) {
            return {m[0] * n[0] + m[1] * n[1] + m[2] * n[2],
                    m[3] * n[0] + m[4] * n[1] + m[5] * n[2],
                    m[6] * n[0] + m[7] * n[1] + m[8] * n[2]};
        }

        void normalize_normal(std::array<float, 3>& n) {
            const float length = std::hypot(n[0], n[1], n[2]);
            if (!std::isfinite(length) || length < 1e-12f) {
                n = {};
                return;
            }
            for (float& component : n) {
                component /= length;
            }
        }
    }  // namespace

    void transform_mesh_normals(GbMeshCpu& mesh, const std::array<float, 16>& transform) {
        const auto matrix = normal_matrix(transform);
        for (auto& normal : mesh.normals) {
            normal = transform_normal(matrix, normal);
            normalize_normal(normal);
        }
    }

    void GbSkin::skin_mesh(GbMeshCpu& mesh,
                           const std::vector<std::array<float, 16>>& skin,
                           bool skin_normals) {
        if (mesh.skin_verts.empty()) {
            return;
        }
        mesh.pos_uv.resize(mesh.skin_verts.size() * 5);
        std::vector<std::array<float, 9>> normal_matrices;
        if (skin_normals) {
            mesh.normals.resize(mesh.skin_verts.size());
            normal_matrices.reserve(skin.size());
            for (const auto& matrix : skin) {
                normal_matrices.push_back(normal_matrix(matrix));
            }
        }
        for (std::size_t vi = 0; vi < mesh.skin_verts.size(); ++vi) {
            const GbSkinVert& sv = mesh.skin_verts[vi];
            float ox = 0, oy = 0, oz = 0;
            std::array<float, 3> normal{};
            float weights[4] = {0, 0, 0, 0};
            const int n = sv.influence_count;
            if (n <= 0) {
                ox = sv.px;
                oy = sv.py;
                oz = sv.pz;
                normal = sv.normal;
            } else if (n == 1) {
                weights[0] = 1.0f;
            } else {
                float sum = 0;
                for (int i = 0; i < n - 1; ++i) {
                    weights[i] = sv.w[i];
                    sum += weights[i];
                }
                weights[n - 1] = 1.0f - sum;
            }
            if (n > 0) {
                for (int i = 0; i < n; ++i) {
                    const std::uint8_t pal = sv.idx[i];
                    if (pal >= mesh.bone_palette.size()) {
                        continue;
                    }
                    const std::uint8_t bone = mesh.bone_palette[pal];
                    if (bone >= skin.size()) {
                        continue;
                    }
                    float tx, ty, tz;
                    d3d_transform_point(skin[bone].data(), sv.px, sv.py, sv.pz, tx, ty, tz);
                    ox += tx * weights[i];
                    oy += ty * weights[i];
                    oz += tz * weights[i];
                    if (skin_normals) {
                        const auto transformed = transform_normal(normal_matrices[bone], sv.normal);
                        for (std::size_t axis = 0; axis < normal.size(); ++axis) {
                            normal[axis] += transformed[axis] * weights[i];
                        }
                    }
                }
            }
            mesh.pos_uv[vi * 5 + 0] = ox;
            mesh.pos_uv[vi * 5 + 1] = oy;
            mesh.pos_uv[vi * 5 + 2] = oz;
            mesh.pos_uv[vi * 5 + 3] = sv.u;
            mesh.pos_uv[vi * 5 + 4] = sv.v;
            if (skin_normals) {
                normalize_normal(normal);
                mesh.normals[vi] = normal;
            }
        }
    }

    void GbSkin::apply_embedded_idle(GbModel& model) {
        std::vector<std::array<float, 16>> skin;
        if (!eval_matrices_kf0(model, 0, skin)) {
            return;
        }
        for (auto& mesh : model.meshes) {
            skin_mesh(mesh, skin);
        }
    }

    bool GbSkin::skin_with_skeleton_anim(GbModel& clothes,
                                         const GbModel& skeleton,
                                         const GbModel& motion) {
        if (skeleton.bones.empty() || motion.anims.empty() || motion.anims[0].bone0.empty()) {
            return false;
        }
        if (skeleton.bones.size() != motion.anims[0].bone0.size()) {
            log::warn(
                "skin: bone count mismatch skeleton=" + std::to_string(skeleton.bones.size()) +
                " motion=" + std::to_string(motion.anims[0].bone0.size()));
            return false;
        }
        GbModel combined;
        combined.bones = skeleton.bones;
        combined.anims = motion.anims;
        std::vector<std::array<float, 16>> skin;
        if (!eval_matrices_kf0(combined, 0, skin)) {
            return false;
        }
        for (auto& mesh : clothes.meshes) {
            skin_mesh(mesh, skin);
        }
        return true;
    }

    bool GbLoader::load_texture(const std::string& model_gb_path,
                                const std::string& texture_name,
                                ImageRgba& out) {
        out = {};
        if (texture_name.empty()) {
            return false;
        }
        const std::string dir = parent_dir(model_gb_path);
        // Strip path components from texture name; client uses basename in TEX\.
        std::string base = texture_name;
        const auto slash = base.find_last_of("/\\");
        if (slash != std::string::npos) {
            base = base.substr(slash + 1);
        }

        const std::string stem = replace_ext(base, "");
        const std::string tex_dir = platform::join_path(dir, "tex");
        const std::string tex_dir_caps = platform::join_path(dir, "Tex");
        bool loaded = try_load_tex_in_dir(tex_dir, stem, out) ||
                      try_load_tex_in_dir(tex_dir_caps, stem, out) ||
                      try_load_tex_in_dir(dir, stem, out);
        if (!loaded) {
            // Shared pool used by many props (stone_*, watchtower_*, front_01, ...).
            const std::string objects = objects_dir_from_model(model_gb_path);
            if (!objects.empty()) {
                const std::string common_tex = platform::join_path(objects, "common/tex");
                loaded = try_load_tex_in_dir(common_tex, stem, out);
            }
        }
        if (!loaded) {
            // Skill GBs (Data/Skill/*.gb) pull textures from Data/Effect/Tex (KalOnline
            // compatibility behavior).
            const std::string norm = platform::normalize_separators(model_gb_path);
            std::string cur = norm;
            while (true) {
                const auto slash = cur.find_last_of('/');
                if (slash == std::string::npos) {
                    break;
                }
                const std::string leaf = cur.substr(slash + 1);
                std::string lower = leaf;
                for (char& c : lower) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lower == "data") {
                    const std::string effect_tex = platform::join_path(cur, "Effect/Tex");
                    loaded = try_load_tex_in_dir(effect_tex, stem, out);
                    break;
                }
                cur = cur.substr(0, slash);
            }
        }
        if (!loaded) {
            log::warn("load_gb_texture: missing " + base + " for " + model_gb_path);
            return false;
        }
        // Client draws water with ~40% constant alpha (TFACTOR), not DDS alpha.
        if (is_water_texture_name(base) && !out.rgba.empty()) {
            for (std::size_t i = 3; i < out.rgba.size(); i += 4) {
                out.rgba[i] = 102;
            }
        }
        return true;
    }

    bool load_gb(const std::string& path, GbModel& out) {
        return GbLoader::load(path, out);
    }

    bool load_gb_texture(const std::string& model_gb_path,
                         const std::string& texture_name,
                         ImageRgba& out) {
        return GbLoader::load_texture(model_gb_path, texture_name, out);
    }

    bool eval_skin_matrices_kf0(const GbModel& model,
                                int anim_index,
                                std::vector<std::array<float, 16>>& skin_out) {
        return GbSkin::eval_matrices_kf0(model, anim_index, skin_out);
    }

    bool eval_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                    const GbAnimClip& clip,
                                    float time_ms,
                                    bool loop,
                                    std::vector<std::array<float, 16>>& skin_out) {
        return GbSkin::eval_matrices_at_time(bones, clip, time_ms, loop, skin_out);
    }

    bool eval_bone_and_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                             const GbAnimClip& clip,
                                             float time_ms,
                                             bool loop,
                                             std::vector<std::array<float, 16>>& bone_world_out,
                                             std::vector<std::array<float, 16>>& skin_out) {
        return GbSkin::eval_bone_and_skin_matrices_at_time(
            bones, clip, time_ms, loop, bone_world_out, skin_out);
    }

    void
    skin_mesh(GbMeshCpu& mesh, const std::vector<std::array<float, 16>>& skin, bool skin_normals) {
        GbSkin::skin_mesh(mesh, skin, skin_normals);
    }

    void apply_embedded_idle(GbModel& model) {
        GbSkin::apply_embedded_idle(model);
    }

    bool skin_with_skeleton_anim(GbModel& clothes, const GbModel& skeleton, const GbModel& motion) {
        return GbSkin::skin_with_skeleton_anim(clothes, skeleton, motion);
    }

}  // namespace openkal::terrain
