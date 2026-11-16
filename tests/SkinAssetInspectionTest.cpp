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

#include "fixture_util.h"
#include "test_util.h"

#include <filesystem>
#include <limits>
#include <utility>

namespace {
    bool has_error(const openkal::platform::Json& result, const char* code) {
        for (const auto& error : result.at("errors").array()) {
            if (error.as_string() == code) {
                return true;
            }
        }
        return false;
    }
}  // namespace

int main() {
    using openkal::test::expect;
    const auto root = openkal::test::temp_dir("skin_asset_inspection");
    const auto texture = root + "/synthetic.dds";
    const auto model_path = root + "/synthetic.gb";
    expect(openkal::test::write_solid_dds(texture, 4, 4, 40, 80, 120),
           "write owned inspection texture");
    expect(openkal::test::write_rigid_gb_v10(model_path, "synthetic.dds"),
           "write owned inspection model");
    openkal::terrain::GbModel model;
    expect(openkal::terrain::load_gb(model_path, model), "read owned inspection model");
    const auto valid = openkal::tools::inspect_skin_asset(model, model_path);
    expect(valid.at("errors").array().empty() && valid.at("vertices").as_int() == 3,
           "inspect synthetic geometry and used texture");
    const auto fingerprint = valid.at("geometry_fingerprint").as_string();
    const auto shape = valid.at("attachment_shape_fingerprint").as_string();
    auto recolored = model;
    recolored.meshes[0].pos_uv[3] += 0.25f;
    recolored.meshes[0].two_sided = !recolored.meshes[0].two_sided;
    expect(openkal::tools::inspect_skin_asset(recolored, model_path)
                   .at("attachment_shape_fingerprint")
                   .as_string() == shape,
           "attachment shape ignores UV and material changes");
    auto moved = model;
    moved.meshes[0].pos_uv[0] += 0.25f;
    expect(openkal::tools::inspect_skin_asset(moved, model_path)
                   .at("attachment_shape_fingerprint")
                   .as_string() != shape,
           "attachment shape detects position changes");
    auto reversed = model;
    std::swap(reversed.meshes[0].indices[0], reversed.meshes[0].indices[1]);
    expect(openkal::tools::inspect_skin_asset(reversed, model_path)
                   .at("attachment_shape_fingerprint")
                   .as_string() != shape,
           "attachment shape preserves winding and triangle indices");
    auto effect = model.meshes[0];
    effect.name = "EfFeCt_socket";
    recolored = model;
    recolored.meshes.push_back(effect);
    expect(openkal::tools::inspect_skin_asset(recolored, model_path)
                   .at("attachment_shape_fingerprint")
                   .as_string() == shape,
           "attachment shape excludes effect sockets as the weapon renderer does");
    recolored = model;
    recolored.meshes.push_back(moved.meshes[0]);
    const auto two_shapes = openkal::tools::inspect_skin_asset(recolored, model_path)
                                .at("attachment_shape_fingerprint")
                                .as_string();
    expect(two_shapes != shape, "attachment shape detects additional rendered meshes");
    std::swap(recolored.meshes[0], recolored.meshes[1]);
    expect(openkal::tools::inspect_skin_asset(recolored, model_path)
                   .at("attachment_shape_fingerprint")
                   .as_string() == two_shapes,
           "attachment shape is independent of mesh order");
    recolored.meshes = {effect};
    expect(openkal::tools::inspect_skin_asset(recolored, model_path)
                   .at("attachment_shape_meshes")
                   .as_int() == 0,
           "effect-only models have no attachment shape to verify");
    model.meshes[0].name = "Different display name";
    model.materials[0].texture = "missing.dds";
    const auto missing = openkal::tools::inspect_skin_asset(model, model_path);
    expect(has_error(missing, "missing_or_invalid_texture"),
           "missing texture is reported instead of accepting runtime fallback");
    expect(missing.at("geometry_fingerprint").as_string() == fingerprint,
           "geometry fingerprint ignores display and texture names");
    model.materials[0].texture = "synthetic.dds";
    model.meshes[0].indices[0] = 99;
    model.meshes[0].pos_uv[0] = std::numeric_limits<float>::quiet_NaN();
    const auto broken = openkal::tools::inspect_skin_asset(model, model_path);
    expect(has_error(broken, "invalid_triangle_index") && has_error(broken, "nonfinite_vertex"),
           "invalid indices and coordinates are independent findings");
    model.meshes[0].bone_palette = {17};
    model.meshes[0].skin_verts.resize(3);
    for (auto& vertex : model.meshes[0].skin_verts) {
        vertex.influence_count = 1;
        vertex.idx[0] = 0;
    }
    auto skinned = openkal::tools::inspect_skin_asset(model, model_path);
    expect(skinned.at("bones").array().size() == 1 && skinned.at("bones").array()[0].as_int() == 17,
           "used bones are mapped through the mesh-local palette");
    model.meshes[0].skin_verts[0].idx[0] = 5;
    skinned = openkal::tools::inspect_skin_asset(model, model_path);
    expect(has_error(skinned, "invalid_bone_palette_index"),
           "invalid skin palette reference is reported");
    const auto empty = openkal::tools::inspect_skin_asset({}, model_path);
    expect(has_error(empty, "empty_model"), "empty model is not a valid skin");
    std::filesystem::remove_all(root);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
