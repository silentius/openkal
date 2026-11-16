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
#include "openkal/client/AnimatedActor.h"
#include "test_util.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <unistd.h>

namespace {
    // Project-owned two-bone fixture, encoded for OpenKal's GB decoder.
    bool write_motion(const std::string& path,
                      const std::string& options,
                      bool keyframe_options = false,
                      float distance = 10.0f,
                      std::uint16_t duration_ms = 1000) {
        using openkal::test::append_pod;
        const std::string strings = std::string(1, '\0') + options + '\0';
        std::vector<std::uint8_t> bytes{7, 2, openkal::terrain::kModelBoneFlag, 0};
        append_pod(bytes, std::uint32_t{0});
        for (int i = 0; i < 8; ++i) {
            append_pod(bytes, std::uint16_t{0});
        }
        for (std::uint16_t value :
             std::array<std::uint16_t, 5>{2, 1, static_cast<std::uint16_t>(strings.size()), 0, 4}) {
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
        append_pod(bytes, std::uint32_t{keyframe_options ? 0u : 1u});
        append_pod(bytes, std::uint16_t{2});
        for (std::uint16_t time : std::array<std::uint16_t, 2>{0, duration_ms}) {
            append_pod(bytes, time);
            append_pod(bytes, std::uint32_t{keyframe_options && time == duration_ms ? 1u : 0u});
        }
        for (std::uint16_t sample = 0; sample < 4; ++sample) {
            append_pod(bytes, sample);
        }
        for (int frame = 0; frame < 2; ++frame) {
            for (int bone = 0; bone < 2; ++bone) {
                const float x = bone == 0 ? frame * distance : 2.0f;
                for (float value : {x, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f}) {
                    append_pod(bytes, value);
                }
            }
        }
        openkal::test::append_str_bytes(bytes, strings);
        return openkal::test::write_bytes(path, bytes);
    }
}  // namespace

int main() {
    using namespace openkal;
    using test::expect;
    const auto root = test::temp_dir(("player_chest_pose_" + std::to_string(getpid())).c_str());
    test::mkdir_p(root + "/Model/Motion");
    test::mkdir_p(root + "/Model/Clothes");
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    std::unordered_map<std::string, engine::TextureHandle> textures;
    for (const auto clazz : {client::Class::Thief,
                             client::Class::Knight,
                             client::Class::Archer,
                             client::Class::Magician}) {
        const std::string job(1, client::LocalPlayer::job_letter(clazz));
        const auto motion = root + "/Model/Motion/T" + job;
        expect(write_motion(motion + "_bone.gb", ""), "write owned player skeleton");
        expect(write_motion(motion + "_0_A_01.gb", ""), "write owned idle motion");
        expect(write_motion(motion + "_1_A_01.gb", ""), "write owned walk motion");
        expect(write_motion(motion + "_5_A_02.gb", ""), "write owned rest stand-up motion");
        expect(write_motion(motion + "_5_A_01.gb", ""), "write owned sitting motion");
        expect(write_motion(motion + "_7_A_03.gb", ""), "write owned skill motion");
        expect(test::write_rigid_gb_v7(root + "/Model/Clothes/C" + job + "_0_s01.gb", ""),
               "write owned player mesh");
        expect(write_motion(motion + "_8_A_01.gb", "", false, 10.0f, 2000),
               "write owned fallback attack motion");
        for (const bool has_behead : {false, true}) {
            if (has_behead) {
                expect(write_motion(motion + "_7_A_01.gb", ""), "write owned behead motion");
            }
            client::AnimatedActor actor;
            expect(actor.load(*renderer, root, clazz, textures), "load owned beheading actor");
            expect(!actor.behead_swing_cancelable(), "idle has no cancelable beheading swing");
            actor.trigger_behead_swing();
            const float duration_sec = has_behead ? 1.0f : 2.0f;
            actor.update(*renderer, duration_sec * 0.25f, false, false, false);
            expect(actor.attack_locked() && !actor.behead_swing_cancelable(),
                   "the first half of either beheading motion stays locked");
            actor.update(*renderer, duration_sec * 0.25f, false, false, false);
            expect(actor.attack_locked() && actor.behead_swing_cancelable(),
                   "beheading becomes cancelable at half the actual clip duration");
            actor.update(*renderer, duration_sec * 0.25f, false, false, false);
            expect(actor.attack_locked() && actor.behead_swing_cancelable(),
                   "beheading continues past halfway when no input cancels it");
            actor.cancel_attack();
            expect(actor.clip() == client::AnimatedActor::Clip::Idle && !actor.attack_locked() &&
                       !actor.behead_swing_cancelable(),
                   "cancelling the second half releases the animation and movement lock");
            actor.trigger_behead_swing();
            expect(actor.attack_locked() && !actor.behead_swing_cancelable(),
                   "a new beheading swing starts with its first-half lock again");
            actor.destroy(*renderer);
        }
        for (const bool keyframe_options : {false, true}) {
            for (const auto& options :
                 {std::string{}, std::string("(*LOOP 500)"), std::string("(*LOOP 1000)")}) {
                // Exercise the lowercase action-letter fallback used by Thief motions.
                const auto suffix = clazz == client::Class::Thief ? "_9_a_04.gb" : "_9_A_04.gb";
                expect(write_motion(motion + suffix, options, keyframe_options),
                       "write owned chest motion");
                terrain::GbModel decoded;
                expect(terrain::load_gb(motion + suffix, decoded) && !decoded.anims.empty(),
                       "decode owned chest motion");
                const float expected_loop =
                    options.empty() ? 0.0f : (options == "(*LOOP 500)" ? 500.0f : 1000.0f);
                expect(!decoded.anims.empty() &&
                           decoded.anims.front().loop_start_ms == expected_loop,
                       "decode loop marker from clip or keyframe options");
                client::AnimatedActor actor;
                expect(actor.load(*renderer, root, clazz, textures), "load owned player actor");
                actor.trigger_open_box();
                const auto advance = [&](float dt, float expected) {
                    actor.update(*renderer, dt, false, false, false);
                    engine::Mat4 pose;
                    expect(actor.bone_world_matrix(engine::Mat4::identity(), 0, pose),
                           "evaluate chest pose");
                    expect(std::fabs(pose.m[12] - expected) < 0.001f,
                           "chest pose preserves intro and authored loop semantics");
                    expect(actor.opening_box(), "opening remains active until server feedback");
                };
                advance(0.25f, 2.5f);
                const bool looping = options == "(*LOOP 500)";
                advance(1.0f, looping ? 7.5f : 10.0f);
                actor.trigger_open_box();
                advance(2.0f, looping ? 7.5f : 10.0f);
                advance(3.0f, looping ? 7.5f : 10.0f);
                actor.clear_open_box();
                expect(!actor.opening_box(), "completion or cancellation leaves the opening pose");
                actor.update(*renderer, 0.0f, false, false, false);
                engine::Mat4 idle;
                expect(actor.bone_world_matrix(engine::Mat4::identity(), 0, idle) &&
                           std::fabs(idle.m[12]) < 0.001f,
                       "idle pose resumes after opening");
                actor.trigger_harvest();
                actor.update(*renderer, 1.0f, false, false, false);
                engine::Mat4 gathering;
                expect(actor.harvesting() &&
                           actor.bone_world_matrix(engine::Mat4::identity(), 0, gathering) &&
                           std::fabs(gathering.m[12] - 5.0f) < 0.001f,
                       "gathering plays one full motion even when the clip has loop markers");
                actor.update(*renderer, 1.1f, false, false, false);
                expect(!actor.harvesting() && !actor.opening_box() && !actor.sitting() &&
                           actor.clip() == client::AnimatedActor::Clip::Idle,
                       "gathering returns directly to idle without the rest stand-up motion");
                actor.update(*renderer, 0.0f, false, false, false);
                expect(actor.bone_world_matrix(engine::Mat4::identity(), 0, idle) &&
                           std::fabs(idle.m[12]) < 0.001f,
                       "gathering restores the idle pose");
                actor.destroy(*renderer);
            }
        }
        client::AnimatedActor gatherer;
        expect(gatherer.load(*renderer, root, clazz, textures),
               "load gatherer with stand-up motion");
        gatherer.trigger_open_box();
        gatherer.update(*renderer, 1.0f, false, false, false);
        gatherer.clear_open_box();
        gatherer.update(*renderer, 0.0f, false, false, false);
        engine::Mat4 box_end;
        expect(gatherer.bone_world_matrix(engine::Mat4::identity(), 0, box_end),
               "sample the Wooden Box completion pose");
        gatherer.trigger_harvest();
        gatherer.update(*renderer, 2.1f, false, false, false);
        engine::Mat4 harvest_end;
        expect(gatherer.clip() == client::AnimatedActor::Clip::Idle && !gatherer.sitting() &&
                   !gatherer.opening_box() &&
                   gatherer.bone_world_matrix(engine::Mat4::identity(), 0, harvest_end),
               "gathering completes through the same idle transition as a Wooden Box");
        for (int i = 0; i < 16; ++i) {
            expect(std::fabs(harvest_end.m[i] - box_end.m[i]) < 0.001f,
                   "gathering and Wooden Box completion poses match");
        }
        gatherer.update(*renderer, 0.25f, false, false, false);
        expect(gatherer.clip() == client::AnimatedActor::Clip::Idle && !gatherer.sitting(),
               "ordinary idle continues without an intermediate rest transition");
        expect(!gatherer.trigger_emote(0) && !gatherer.trigger_emote(21),
               "reject out-of-range emote motions");
        expect(!gatherer.trigger_emote(20), "missing emote leaves the current pose unchanged");
        for (int emote = 1; emote <= 20; ++emote) {
            const auto suffix = std::string(clazz == client::Class::Thief ? "_10_a_" : "_10_A_") +
                                (emote < 10 ? "0" : "") + std::to_string(emote) + ".gb";
            expect(write_motion(motion + suffix, "(*LOOP 500)", false, emote * 10.0f),
                   "write distinct owned emote motion");
            expect(gatherer.trigger_emote(emote), "all twenty emotes start for every human class");
            gatherer.update(*renderer, 0.25f, false, false, false);
            engine::Mat4 pose;
            expect(gatherer.clip() == client::AnimatedActor::Clip::Emote &&
                       gatherer.bone_world_matrix(engine::Mat4::identity(), 0, pose) &&
                       std::fabs(pose.m[12] - emote * 2.5f) < 0.001f,
                   "emote selects its own motion and advances the skeleton");
            gatherer.update(*renderer, 0.85f, false, false, false);
            expect(gatherer.clip() == client::AnimatedActor::Clip::Idle,
                   "emote finishes once even when the motion contains a loop marker");
            expect(gatherer.trigger_emote(emote), "cached emote can restart");
            gatherer.update(*renderer, 0.0f, false, false, false);
            expect(gatherer.bone_world_matrix(engine::Mat4::identity(), 0, pose) &&
                       std::fabs(pose.m[12]) < 0.001f,
                   "repeated emote starts at its first frame");
            gatherer.update(*renderer, 0.1f, true, false, false);
            expect(gatherer.clip() == client::AnimatedActor::Clip::Walk,
                   "movement interrupts the emote immediately");
        }
        gatherer.trigger_open_box();
        expect(!gatherer.trigger_emote(1) && gatherer.opening_box(),
               "emote cannot interrupt an interaction");
        gatherer.clear_open_box();
        gatherer.trigger_sit();
        expect(!gatherer.trigger_emote(1), "emote cannot replace a sitting pose");
        gatherer.clear_rest_pose();
        gatherer.trigger_skill();
        gatherer.update(*renderer, 0.25f, false, false, false);
        expect(!gatherer.trigger_emote(1), "emote cannot interrupt an unfinished skill");
        gatherer.update(*renderer, 0.85f, false, false, false);
        expect(gatherer.trigger_emote(1), "emote can replace a completed skill's held final pose");
        gatherer.destroy(*renderer);
    }
    for (const auto& entry : textures) {
        renderer->destroy_texture(entry.second);
    }
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
