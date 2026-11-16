/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkinAssetInspection.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

namespace openkal::tools {
    namespace {
        using platform::Json;

        Json value(double number) {
            Json result;
            result.set_number(number);
            return result;
        }

        Json value(const std::string& text) {
            Json result;
            result.string() = text;
            result.set_type(Json::Type::String);
            return result;
        }

        Json array() {
            Json result;
            result.set_type(Json::Type::Array);
            return result;
        }

        void hash_word(std::uint64_t& hash, std::uint32_t word) {
            for (int i = 0; i < 4; ++i) {
                hash ^= (word >> (i * 8)) & 255u;
                hash *= 1099511628211ull;
            }
        }

        void hash_float(std::uint64_t& hash, float number) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &number, sizeof(bits));
            hash_word(hash, bits);
        }

        std::string fingerprint(std::uint64_t hash) {
            std::ostringstream out;
            out << std::hex << std::setfill('0') << std::setw(16) << hash;
            return out.str();
        }
    }  // namespace

    platform::Json inspect_skin_asset(const terrain::GbModel& model, const std::string& path) {
        Json result;
        result.set_type(Json::Type::Object);
        std::set<std::string> errors;
        std::set<int> bones;
        std::set<int> materials;
        std::array<float, 3> lo, hi;
        lo.fill(std::numeric_limits<float>::max());
        hi.fill(std::numeric_limits<float>::lowest());
        std::size_t vertices = 0;
        std::size_t triangles = 0;
        std::uint64_t geometry_hash = 14695981039346656037ull;
        std::uint64_t texture_hash = 14695981039346656037ull;
        std::vector<std::uint64_t> attachment_shapes;
        if (model.meshes.empty()) {
            errors.insert("empty_model");
        }
        for (const auto& mesh : model.meshes) {
            const auto count = mesh.pos_uv.size() / 5;
            vertices += count;
            triangles += mesh.indices.size() / 3;
            if (count == 0 || mesh.indices.empty()) {
                errors.insert("empty_mesh");
            }
            if (mesh.pos_uv.size() % 5 != 0 || mesh.indices.size() % 3 != 0) {
                errors.insert("invalid_geometry_layout");
            }
            std::string name = mesh.name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (name.compare(0, 6, "effect") != 0) {
                std::uint64_t shape = 14695981039346656037ull;
                hash_word(shape, static_cast<std::uint32_t>(count));
                hash_word(shape, static_cast<std::uint32_t>(mesh.indices.size()));
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    for (int axis = 0; axis < 3; ++axis) {
                        hash_float(shape, mesh.pos_uv[i + axis]);
                    }
                }
                for (const auto index : mesh.indices) {
                    hash_word(shape, index);
                }
                attachment_shapes.push_back(shape);
            }
            hash_word(geometry_hash, static_cast<std::uint32_t>(count));
            for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                for (int j = 0; j < 5; ++j) {
                    const float number = mesh.pos_uv[i + j];
                    if (!std::isfinite(number)) {
                        errors.insert("nonfinite_vertex");
                        continue;
                    }
                    hash_float(geometry_hash, number);
                    if (j < 3) {
                        lo[j] = std::min(lo[j], number);
                        hi[j] = std::max(hi[j], number);
                    }
                }
            }
            for (const auto index : mesh.indices) {
                if (index >= count) {
                    errors.insert("invalid_triangle_index");
                }
                hash_word(geometry_hash, index);
            }
            hash_word(geometry_hash, static_cast<std::uint32_t>(mesh.material_index));
            hash_word(geometry_hash,
                      mesh.alpha_blend | (mesh.alpha_test << 1) | (mesh.two_sided << 2));
            if (mesh.material_index < 0 ||
                mesh.material_index >= static_cast<int>(model.materials.size())) {
                errors.insert("invalid_material_index");
            } else {
                materials.insert(mesh.material_index);
            }
            for (const auto bone : mesh.bone_palette) {
                hash_word(geometry_hash, bone);
            }
            if (!mesh.skin_verts.empty() && mesh.skin_verts.size() != count) {
                errors.insert("invalid_skin_vertex_count");
            }
            for (const auto& vertex : mesh.skin_verts) {
                if (vertex.influence_count < 1 || vertex.influence_count > 4) {
                    errors.insert("invalid_influence_count");
                    continue;
                }
                float remaining = 1.0f;
                for (int i = 0; i < vertex.influence_count; ++i) {
                    const float weight = i + 1 == vertex.influence_count ? remaining : vertex.w[i];
                    if (i + 1 != vertex.influence_count) {
                        remaining -= weight;
                    }
                    if (!std::isfinite(weight) || weight < -0.001f || weight > 1.001f) {
                        errors.insert("invalid_skin_weight");
                    }
                    hash_float(geometry_hash, weight);
                    hash_word(geometry_hash, vertex.idx[i]);
                    if (weight <= 0.00001f) {
                        continue;
                    }
                    if (vertex.idx[i] >= mesh.bone_palette.size()) {
                        errors.insert("invalid_bone_palette_index");
                    } else {
                        bones.insert(mesh.bone_palette[vertex.idx[i]]);
                    }
                }
            }
        }
        auto textures = array();
        for (const auto index : materials) {
            const auto& material = model.materials[index];
            Json texture;
            texture.set_type(Json::Type::Object);
            texture.object()["name"] = value(material.texture);
            terrain::ImageRgba image;
            const bool loaded = terrain::load_gb_texture(path, material.texture, image) &&
                                image.width > 0 && image.height > 0 &&
                                image.rgba.size() == static_cast<std::size_t>(image.width) *
                                                         static_cast<std::size_t>(image.height) * 4;
            texture.object()["width"] = value(image.width);
            texture.object()["height"] = value(image.height);
            texture.object()["loaded"].set_boolean(loaded);
            if (!loaded) {
                errors.insert("missing_or_invalid_texture");
            }
            hash_word(texture_hash, static_cast<std::uint32_t>(index));
            hash_word(texture_hash, static_cast<std::uint32_t>(image.width));
            hash_word(texture_hash, static_cast<std::uint32_t>(image.height));
            for (const auto byte : image.rgba) {
                texture_hash ^= byte;
                texture_hash *= 1099511628211ull;
            }
            textures.array().push_back(std::move(texture));
        }
        result.object()["textures"] = std::move(textures);
        result.object()["vertices"] = value(static_cast<double>(vertices));
        result.object()["triangles"] = value(static_cast<double>(triangles));
        result.object()["meshes"] = value(static_cast<double>(model.meshes.size()));
        result.object()["geometry_fingerprint"] = value(fingerprint(geometry_hash));
        result.object()["texture_fingerprint"] = value(fingerprint(texture_hash));
        std::sort(attachment_shapes.begin(), attachment_shapes.end());
        std::uint64_t shape_hash = 14695981039346656037ull;
        hash_word(shape_hash, static_cast<std::uint32_t>(attachment_shapes.size()));
        for (const auto shape : attachment_shapes) {
            hash_word(shape_hash, static_cast<std::uint32_t>(shape));
            hash_word(shape_hash, static_cast<std::uint32_t>(shape >> 32));
        }
        result.object()["attachment_shape_fingerprint"] = value(fingerprint(shape_hash));
        result.object()["attachment_shape_meshes"] =
            value(static_cast<double>(attachment_shapes.size()));
        auto bounds_min = array();
        auto bounds_max = array();
        for (int i = 0; i < 3; ++i) {
            bounds_min.array().push_back(value(vertices == 0 || lo[i] > hi[i] ? 0 : lo[i]));
            bounds_max.array().push_back(value(vertices == 0 || lo[i] > hi[i] ? 0 : hi[i]));
        }
        result.object()["bounds_min"] = std::move(bounds_min);
        result.object()["bounds_max"] = std::move(bounds_max);
        auto bone_list = array();
        for (const auto bone : bones) {
            bone_list.array().push_back(value(bone));
        }
        result.object()["bones"] = std::move(bone_list);
        auto error_list = array();
        for (const auto& error : errors) {
            error_list.array().push_back(value(error));
        }
        result.object()["errors"] = std::move(error_list);
        return result;
    }
}  // namespace openkal::tools
