/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/client/MonsterRender.h"
#include "test_util.h"

#include <cmath>
#include <filesystem>
#include <memory>

namespace {
    // Project-owned two-bone fixture, encoded for OpenKal's GB decoder.
    bool write_motion(const std::string& path, float offset) {
        using openkal::test::append_pod;
        std::vector<std::uint8_t> bytes{7, 2, openkal::terrain::kModelBoneFlag, 0};
        append_pod(bytes, std::uint32_t{0});
        for (int i = 0; i < 8; ++i) {
            append_pod(bytes, std::uint16_t{0});
        }
        for (std::uint16_t value : {2, 1, 1, 0, 4}) {
            append_pod(bytes, value);
        }
        bytes.push_back(1);
        append_pod(bytes, std::uint16_t{0});
        append_pod(bytes, std::uint16_t{0});
        const auto identity = openkal::engine::Mat4::identity();
        for (int bone = 0; bone < 2; ++bone) {
            for (float value : identity.m) {
                append_pod(bytes, value);
            }
            bytes.push_back(bone == 0 ? 255 : 0);
        }
        append_pod(bytes, std::uint32_t{0});
        append_pod(bytes, std::uint16_t{2});
        for (std::uint16_t time : {0, 1000}) {
            append_pod(bytes, time);
            append_pod(bytes, std::uint32_t{0});
        }
        for (std::uint16_t sample = 0; sample < 4; ++sample) {
            append_pod(bytes, sample);
        }
        for (int frame = 0; frame < 2; ++frame) {
            for (int bone = 0; bone < 2; ++bone) {
                const float x = bone == 0 ? offset + frame * 10.0f : 2.0f;
                for (float value : {x, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f}) {
                    append_pod(bytes, value);
                }
            }
        }
        bytes.push_back(0);
        return openkal::test::write_bytes(path, bytes);
    }
}  // namespace

int main() {
    using openkal::test::expect;
    namespace test = openkal::test;
    namespace client = openkal::client;
    namespace engine = openkal::engine;
    const std::string root = openkal::test::temp_dir("monster_pose");
    expect(write_motion(root + "/idle.gb", 0.0f), "write idle fixture");
    expect(write_motion(root + "/walk.gb", 20.0f), "write walk fixture");
    expect(openkal::test::write_rigid_gb_v7(root + "/mesh.gb", ""), "write mesh fixture");
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    std::unordered_map<std::string, engine::TextureHandle> textures;
    client::MonsterRigGpu rig;
    if (!client::MonsterRigGpu::load_pet(*renderer,
                                         1,
                                         1.0f,
                                         root + "/idle.gb",
                                         root + "/idle.gb",
                                         root + "/walk.gb",
                                         "",
                                         {{root + "/mesh.gb", ""}},
                                         rig,
                                         textures)) {
        expect(false, "load synthetic rig");
        return 1;
    }
    client::MonsterInstance instance;
    expect(instance.create_from_rig(*renderer, rig, false), "create synthetic monster");
    auto model = engine::Mat4::identity();
    const auto check = [&](int bone, float expected) {
        engine::Mat4 world;
        expect(instance.bone_world_matrix(rig, model, bone, world), "resolve bone pose");
        expect(std::fabs(world.m[12] - expected) < 0.001f,
               "bone uses current clip, time and model");
    };
    check(0, 0.0f);
    check(1, 2.0f);
    expect(instance.update(*renderer, rig, 0.25f, false), "skin advancing idle pose");
    check(1, 4.5f);
    check(0, 2.5f);
    model.m[12] = 100.0f;
    check(1, 104.5f);
    expect(!instance.update(*renderer, rig, 0.25f, false, false, nullptr, 0, 0, 0, false),
           "defer mesh skinning");
    check(0, 105.0f);
    check(1, 107.0f);
    expect(instance.update(*renderer, rig, 0.5f, true), "switch clip at same sample time");
    check(0, 125.0f);
    check(1, 127.0f);
    expect(instance.update(*renderer, rig, 1.0f, true), "loop walk clip");
    check(0, 125.0f);
    instance.destroy(*renderer);
    expect(instance.create_from_rig(*renderer, rig, false), "recreate monster");
    // Rebuilding meshes preserves the active animation, even though it uploads the idle bind pose.
    check(0, 125.0f);
    check(1, 127.0f);
    instance.destroy(*renderer);
    test::mkdir_p(root + "/RuntimeData/models");
    expect(test::write_solid_dds(root + "/RuntimeData/models/leaf.dds", 4, 4, 50, 110, 40),
           "write owned static leaf texture");
    expect(test::write_rigid_gb_v10(root + "/RuntimeData/models/leaf.gb", "leaf.dds"),
           "write owned static leaf mesh");
    client::MonsterRig macro;
    macro.set_owned_model("RuntimeData/models/leaf.gb");
    macro.set_scale(0.185f);
    client::MonsterRigGpu leaf;
    expect(client::MonsterRigGpu::load(
               *renderer, "", 65000, macro, leaf, textures, nullptr, false, root),
           "owned static gathering model loads without optional client data");
    client::MonsterInstance plant;
    expect(plant.create_from_rig(*renderer, leaf, false), "create static gathering instance");
    expect(plant.update(*renderer, leaf, 1, false),
           "static gathering mesh survives animation update");
    expect(std::fabs(leaf.pick_height() - 1.85f) < 0.001f,
           "gathering hitbox follows the small model scale");
    expect(std::fabs(leaf.target_height() - 1.85f) < 0.001f,
           "gathering name anchor follows the displayed plant height");
    plant.destroy(*renderer);
    for (const auto& entry : textures) {
        renderer->destroy_texture(entry.second);
    }
    std::filesystem::remove_all(root);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
