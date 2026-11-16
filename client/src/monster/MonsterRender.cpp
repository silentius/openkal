/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MonsterRender.h"

#include "openkal/client/Audio.h"
#include "openkal/client/WorldPick.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>

namespace openkal::client {
    namespace {

        void arm_attack_fx(bool& bound,
                           bool& await,
                           bool& ready,
                           float& pending_ms,
                           MonsterAttackFx& pending,
                           const terrain::GbAnimClip& clip) {
            bound = true;
            await = false;
            ready = false;
            pending_ms = -1.0f;
            pending = MonsterAttackFx{};
            const std::size_t n = std::min(clip.key_times.size(), clip.key_options.size());
            for (std::size_t i = 0; i < n; ++i) {
                MonsterAttackFx fx;
                if (!MonsterAttackFx::parse_option(clip.key_options[i], fx)) {
                    continue;
                }
                const float t = static_cast<float>(clip.key_times[i]);
                if (pending_ms < 0.0f || t < pending_ms) {
                    pending_ms = t;
                    pending = fx;
                }
            }
            await = pending_ms >= 0.0f;
        }

        void poll_attack_fx(bool& await, bool& ready, float anim_time_ms, float pending_ms) {
            if (await && anim_time_ms >= pending_ms) {
                await = false;
                ready = true;
            }
        }

        void poll_action_damage(bool& latched,
                                bool& ready,
                                bool& bound,
                                float& key_ms,
                                float anim_time_ms,
                                const terrain::GbAnimClip* clip,
                                bool force) {
            if (!latched) {
                return;
            }
            if (!bound) {
                bound = true;
                key_ms =
                    (clip != nullptr)
                        ? MonsterInstance::effect_damage_time_ms(clip->key_times, clip->key_options)
                        : -1.0f;
            }
            if (MonsterInstance::action_damage_should_fire(latched, anim_time_ms, key_ms, force)) {
                ready = true;
                latched = false;
            }
        }

        void poll_idle_fx(std::vector<std::string>& pending,
                          float& last_time_ms,
                          float anim_time_ms,
                          const terrain::GbAnimClip& clip,
                          const std::vector<MonsterIdleFxCue>& cues) {
            if (cues.empty()) {
                last_time_ms = 0.0f;
                return;
            }
            const float dur = std::max(1.0f, clip.duration_ms());
            const float t = std::fmod(anim_time_ms, dur);
            const float prev = last_time_ms;
            last_time_ms = t;
            const bool wrapped = prev >= 0.0f && t + 0.001f < prev;
            for (const auto& cue : cues) {
                const float key = cue.time_ms();
                bool fire = false;
                if (prev < 0.0f) {
                    fire = key <= t + 0.001f;
                } else if (wrapped) {
                    fire = key > prev || key <= t + 0.001f;
                } else {
                    fire = key > prev && key <= t + 0.001f;
                }
                if (fire) {
                    pending.push_back(cue.name());
                }
            }
        }

    }  // namespace

    bool MonsterAttackFx::parse_option(std::string_view option, MonsterAttackFx& out) {
        out = MonsterAttackFx{};
        const auto proj = option.find("(*PROJECTILE");
        if (proj != std::string_view::npos) {
            const std::string tmp(option);
            int type = 0;
            int bone = 1;
            const int n = std::sscanf(tmp.c_str() + proj, "(*PROJECTILE %d %d", &type, &bone);
            if (n < 1) {
                return false;
            }
            out.set_kind(Kind::Projectile);
            out.set_type(type);
            out.set_bone(n >= 2 ? bone : 1);
            return true;
        }
        if (option.find("(*EFFECT_ARROW)") != std::string_view::npos ||
            option.find("(*EFFECT_ARROW ") != std::string_view::npos) {
            out.set_kind(Kind::Arrow);
            out.set_bone(0);
            return true;
        }
        if (option.find("(*EFFECT_STONE)") != std::string_view::npos) {
            out.set_kind(Kind::Stone);
            out.set_bone(0);
            return true;
        }
        return false;
    }

    bool MonsterRigGpu::load(engine::IEngine& engine,
                             const std::string& data_root,
                             int index,
                             const MonsterRig& macro,
                             MonsterRigGpu& out,
                             std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                             const MonsterBoneInfo* bone_info,
                             bool beheaded,
                             const std::string& resource_root) {
        out = {};
        out.m_index = index;
        out.m_scale = macro.scale() > 0.01f ? macro.scale() : 1.0f;
        out.m_effect_bones.fill(-1);
        out.m_spine_bone = -1;
        out.m_head_bone = -1;
        if (bone_info) {
            for (int i = 0; i < kEffectBoneCount; ++i) {
                out.m_effect_bones[static_cast<std::size_t>(i)] = bone_info->skeleton_bone(i);
            }
            out.m_spine_bone = bone_info->spine1();
            out.m_head_bone = bone_info->head();
            out.m_target_height =
                bone_info->target_height() > 0.01f ? bone_info->target_height() : 17.0f;
        }
        out.m_level = macro.level() > 0 ? macro.level() : 1;
        out.m_name_offset_y = macro.name_offset_y();
        out.m_particles = macro.particles();
        out.m_fx = macro.fx();

        if (!macro.owned_model().empty()) {
            const auto& asset = macro.owned_model();
            if (resource_root.empty() || asset.compare(0, 12, "RuntimeData/") != 0 ||
                asset.find("..") != std::string::npos) {
                return false;
            }
            const auto path = platform::join_path(resource_root, asset);
            terrain::GbModel cpu;
            if (!terrain::load_gb(path, cpu) || cpu.meshes.empty()) {
                return false;
            }
            terrain::GbBone root;
            root.inverse = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            out.m_bones.push_back(root);
            out.m_idle.bone_count = 1;
            out.m_idle.keyframe_count = 1;
            out.m_idle.key_times = {0};
            out.m_idle.samples.resize(1);
            float height = 0, half = 0;
            for (auto& mesh : cpu.meshes) {
                if (mesh.material_index < 0 ||
                    mesh.material_index >= static_cast<int>(cpu.materials.size())) {
                    return false;
                }
                const auto& name = cpu.materials[mesh.material_index].texture;
                const auto key = path + "|" + name;
                auto it = tex_cache.find(key);
                if (it == tex_cache.end()) {
                    terrain::ImageRgba image;
                    if (!terrain::load_gb_texture(path, name, image) || image.rgba.empty()) {
                        return false;
                    }
                    it = tex_cache
                             .emplace(key,
                                      engine.create_texture_rgba8(
                                          image.rgba.data(), image.width, image.height, true))
                             .first;
                }
                for (std::size_t v = 0; v + 4 < mesh.pos_uv.size(); v += 5) {
                    height = std::max(height, mesh.pos_uv[v + 1]);
                    half = std::max(
                        half, std::max(std::fabs(mesh.pos_uv[v]), std::fabs(mesh.pos_uv[v + 2])));
                }
                out.m_textures.push_back(it->second);
                out.m_alpha_blend.push_back(mesh.alpha_blend);
                out.m_alpha_test.push_back(mesh.alpha_test);
                out.m_two_sided.push_back(mesh.two_sided);
                out.m_bind_meshes.push_back(std::move(mesh));
            }
            out.m_pick_height = std::max(1.0f, height * out.m_scale);
            out.m_pick_half = std::max(1.0f, half * out.m_scale);
            out.m_target_height = height * out.m_scale;
            return true;
        }

        const int clothes = macro.clothes_index() > 0 ? macro.clothes_index() : index;
        const auto cloth_paths = MonsterCatalog::clothes_gb_paths(data_root, clothes, beheaded);
        if (cloth_paths.empty()) {
            log::warn("monster: no clothes for index " + std::to_string(index) + " (clothes " +
                      std::to_string(clothes) + ")");
        }
        const std::string bone_path = MonsterCatalog::bone_gb(data_root, macro.bone_no());
        const std::string idle_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 0, macro.idle_letter());
        const std::string walk_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 1, macro.walk_letter());
        const std::string run_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 2, macro.run_letter());
        const std::string die_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 3, macro.die_letter());
        const std::string behead_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 4, macro.behead_letter());
        const std::string knee_path =
            MonsterCatalog::motion_gb(data_root, macro.bone_no(), 9, macro.knee_letter());
        if (bone_path.empty() || idle_path.empty()) {
            log::warn("monster: missing bone/idle for index " + std::to_string(index));
            return false;
        }

        terrain::GbModel skeleton;
        terrain::GbModel idle_gb;
        terrain::GbModel walk_gb;
        terrain::GbModel run_gb;
        terrain::GbModel die_gb;
        terrain::GbModel behead_gb;
        terrain::GbModel knee_gb;
        if (!terrain::load_gb(bone_path, skeleton) || skeleton.bones.empty()) {
            return false;
        }
        if (!terrain::load_gb(idle_path, idle_gb) || idle_gb.anims.empty()) {
            return false;
        }
        out.m_bones = skeleton.bones;
        out.m_idle = idle_gb.anims[0];
        {
            const auto& clip = out.m_idle;
            MonsterIdleFxCue::append_idle_clip(
                clip.key_options, clip.key_times, out.m_fx, out.m_idle_fx_cues);
        }
        if (!walk_path.empty() && terrain::load_gb(walk_path, walk_gb) && !walk_gb.anims.empty()) {
            out.m_walk = walk_gb.anims[0];
            out.m_has_walk = true;
        }
        if (!run_path.empty() && terrain::load_gb(run_path, run_gb) && !run_gb.anims.empty()) {
            out.m_run = run_gb.anims[0];
            out.m_has_run = true;
        }
        if (!die_path.empty() && terrain::load_gb(die_path, die_gb) && !die_gb.anims.empty()) {
            out.m_die = die_gb.anims[0];
            out.m_has_die = true;
        }
        if (!behead_path.empty() && terrain::load_gb(behead_path, behead_gb) &&
            !behead_gb.anims.empty()) {
            out.m_behead = behead_gb.anims[0];
            out.m_has_behead = true;
        }
        if (!knee_path.empty() && terrain::load_gb(knee_path, knee_gb) && !knee_gb.anims.empty()) {
            out.m_knee = knee_gb.anims[0];
            out.m_has_knee = true;
        }

        for (const auto& gb_path : cloth_paths) {
            terrain::GbModel cpu;
            if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
                continue;
            }
            for (auto& mesh : cpu.meshes) {
                std::string tex_name;
                if (mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(cpu.materials.size())) {
                    tex_name = cpu.materials[mesh.material_index].texture;
                }
                const std::string tex_key = gb_path + "|" + tex_name;
                engine::TextureHandle tex;
                auto tit = tex_cache.find(tex_key);
                if (tit != tex_cache.end()) {
                    tex = tit->second;
                } else {
                    terrain::ImageRgba img;
                    if (!terrain::load_gb_texture(gb_path, tex_name, img) || img.rgba.empty()) {
                        img.width = img.height = 1;
                        img.rgba = {160, 140, 120, 255};
                    }
                    tex = engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
                    tex_cache.emplace(tex_key, tex);
                }
                out.m_textures.push_back(tex);
                // Trust GB MATERIAL_TWOSIDED only (KalOnline MapOptionPre) — do not force
                // two-sided on body parts.
                out.m_alpha_blend.push_back(mesh.alpha_blend);
                out.m_alpha_test.push_back(mesh.alpha_test);
                out.m_two_sided.push_back(mesh.two_sided);
                out.m_bind_meshes.push_back(std::move(mesh));
            }
        }
        constexpr float kSmallMeshHeight = 20.0f;
        constexpr float kSmallHitboxScale = 0.8f;
        bool have_aabb = false;
        float min_x = 0.0f;
        float min_y = 0.0f;
        float min_z = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        float max_z = 0.0f;
        for (const auto& mesh : out.m_bind_meshes) {
            for (std::size_t v = 0; v + 4 < mesh.pos_uv.size(); v += 5) {
                const float x = mesh.pos_uv[v];
                const float y = mesh.pos_uv[v + 1];
                const float z = mesh.pos_uv[v + 2];
                if (!have_aabb) {
                    min_x = max_x = x;
                    min_y = max_y = y;
                    min_z = max_z = z;
                    have_aabb = true;
                } else {
                    min_x = std::min(min_x, x);
                    min_y = std::min(min_y, y);
                    min_z = std::min(min_z, z);
                    max_x = std::max(max_x, x);
                    max_y = std::max(max_y, y);
                    max_z = std::max(max_z, z);
                }
            }
        }
        const float scale = out.m_scale;
        if (!have_aabb) {
            out.m_pick_half = WorldPick::kMonsterHalfExtent * kSmallHitboxScale * scale;
            out.m_pick_height = WorldPick::kMonsterBaseHeight * kSmallHitboxScale * scale;
        } else {
            const float mesh_h = max_y - std::min(min_y, 0.0f);
            const float mesh_half = 0.5f * std::max(max_x - min_x, max_z - min_z);
            if (mesh_h < kSmallMeshHeight) {
                out.m_pick_half = WorldPick::kMonsterHalfExtent * kSmallHitboxScale * scale;
                out.m_pick_height = WorldPick::kMonsterBaseHeight * kSmallHitboxScale * scale;
            } else {
                out.m_pick_half = std::max(mesh_half, 1.0f) * scale;
                out.m_pick_height = std::max(mesh_h, 1.0f) * scale;
            }
        }
        // Magic-field models (bones 900–903) ship empty clothes; the visual is
        // idle `(*EFFECT_FX …)` attached after load.
        return true;
    }

    bool
    MonsterRigGpu::load_pet(engine::IEngine& engine,
                            int index,
                            float scale,
                            const std::string& bone_path,
                            const std::string& idle_path,
                            const std::string& walk_path,
                            const std::string& run_path,
                            const std::vector<PetMeshAsset>& cloth_assets,
                            MonsterRigGpu& out,
                            std::unordered_map<std::string, engine::TextureHandle>& tex_cache) {
        out = {};
        out.m_index = index;
        out.m_scale = scale > 0.01f ? scale : 1.0f;
        out.m_effect_bones.fill(-1);

        terrain::GbModel skeleton;
        terrain::GbModel idle;
        terrain::GbModel walk;
        terrain::GbModel run;
        if (!terrain::load_gb(bone_path, skeleton) || skeleton.bones.empty() ||
            !terrain::load_gb(idle_path, idle) || idle.anims.empty()) {
            log::warn("pet: missing skeleton or idle motion for item " + std::to_string(index));
            return false;
        }
        out.m_bones = skeleton.bones;
        out.m_idle = idle.anims.front();
        if (!walk_path.empty() && terrain::load_gb(walk_path, walk) && !walk.anims.empty()) {
            out.m_walk = walk.anims.front();
            out.m_has_walk = true;
        }
        if (!run_path.empty() && terrain::load_gb(run_path, run) && !run.anims.empty()) {
            out.m_run = run.anims.front();
            out.m_has_run = true;
        }
        for (const PetMeshAsset& asset : cloth_assets) {
            const std::string& gb_path = asset.gb_path;
            terrain::GbModel model;
            if (!terrain::load_gb(gb_path, model) || model.meshes.empty()) {
                continue;
            }
            for (auto& mesh : model.meshes) {
                std::string texture_name;
                if (mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(model.materials.size())) {
                    texture_name = model.materials[mesh.material_index].texture;
                }
                const std::string& selected_texture =
                    asset.texture_override.empty() ? texture_name : asset.texture_override;
                const std::string texture_key = gb_path + "|" + selected_texture;
                engine::TextureHandle texture;
                const auto cached = tex_cache.find(texture_key);
                if (cached != tex_cache.end()) {
                    texture = cached->second;
                } else {
                    terrain::ImageRgba image;
                    if (!terrain::load_gb_texture(gb_path, selected_texture, image) ||
                        image.rgba.empty()) {
                        image.width = image.height = 1;
                        image.rgba = {160, 140, 120, 255};
                    }
                    texture = engine.create_texture_rgba8(
                        image.rgba.data(), image.width, image.height, true);
                    tex_cache.emplace(texture_key, texture);
                }
                out.m_textures.push_back(texture);
                out.m_alpha_blend.push_back(mesh.alpha_blend);
                out.m_alpha_test.push_back(mesh.alpha_test);
                out.m_two_sided.push_back(mesh.two_sided);
                out.m_bind_meshes.push_back(std::move(mesh));
            }
        }
        if (out.m_bind_meshes.empty()) {
            log::warn("pet: no clothes meshes for item " + std::to_string(index));
            return false;
        }
        return true;
    }

    bool MonsterRigGpu::ensure_attack(const std::string& data_root, const MonsterRig& macro) {
        constexpr int kAttackAction = 8;
        if (m_has_attack) {
            return true;
        }
        if (!m_missing_actions.insert(kAttackAction).second) {
            return false;
        }
        const std::string path = MonsterCatalog::motion_gb(
            data_root, macro.bone_no(), kAttackAction, macro.attack_letter());
        if (path.empty()) {
            log::warn("monster: requested attack unavailable for index " + std::to_string(m_index) +
                      " (bone " + std::to_string(macro.bone_no()) + ", action 8)");
            return false;
        }
        terrain::GbModel model;
        if (!terrain::load_gb(path, model) || model.anims.empty()) {
            log::warn("monster: requested attack could not load for index " +
                      std::to_string(m_index) + ": " + path);
            return false;
        }
        m_attack = model.anims[0];
        m_has_attack = true;
        return true;
    }

    bool
    MonsterRigGpu::ensure_skill(const std::string& data_root, const MonsterRig& macro, int motion) {
        constexpr int kSkillAction = 7;
        const int resolved_motion = motion > 0 ? motion : 1;
        if (m_skill.find(resolved_motion) != m_skill.end()) {
            return true;
        }
        if (!m_missing_skill_motions.insert(resolved_motion).second) {
            return false;
        }
        const std::string path = MonsterCatalog::motion_gb(
            data_root, macro.bone_no(), kSkillAction, macro.skill_letter(), resolved_motion);
        if (path.empty()) {
            log::warn("monster: requested skill unavailable for index " + std::to_string(m_index) +
                      " (bone " + std::to_string(macro.bone_no()) + ", action 7, motion " +
                      std::to_string(resolved_motion) + ")");
            return false;
        }
        terrain::GbModel model;
        if (!terrain::load_gb(path, model) || model.anims.empty()) {
            log::warn("monster: requested skill could not load for index " +
                      std::to_string(m_index) + ": " + path);
            return false;
        }
        const auto& clip = model.anims[0];
        std::vector<MonsterIdleFxCue> fx_cues;
        MonsterIdleFxCue::append_skill_clip(clip.key_options, clip.key_times, fx_cues);
        m_skill.emplace(resolved_motion, clip);
        if (!fx_cues.empty()) {
            m_skill_fx_cues.emplace(resolved_motion, std::move(fx_cues));
        }
        return true;
    }

    void MonsterInstance::destroy(engine::IEngine& engine) {
        for (auto& p : m_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
            }
        }
        m_parts.clear();
        m_working_meshes.clear();
        m_bone_matrices.clear();
        m_skin_matrices.clear();
        m_pose_rig = nullptr;
        m_pose_clip = nullptr;
        m_upload_vertices.clear();
        m_last_skinned_time_ms = -1000.0f;
        m_hit_flash_ms = 0.0f;
        m_hit_flash_duration_ms = 0.0f;
        m_hit_flash_peak = 0.0f;
        m_blend_in_timer_ms = 0.0f;
        m_blend_in_time_ms = 0.0f;
    }

    bool MonsterInstance::create_from_rig(engine::IEngine& engine,
                                          const MonsterRigGpu& rig,
                                          bool fade_in) {
        destroy(engine);
        m_index = rig.index();
        const float blend_in_timer_ms = fade_in ? static_cast<float>(kDefaultBlendInMs) : 0.0f;
        const bool skinned = evaluate_pose(rig, rig.idle(), 0.0f, true);
        if (!skinned) {
            m_blend_in_timer_ms = blend_in_timer_ms;
            m_blend_in_time_ms = 0.0f;
            return !rig.fx().empty() || !rig.particles().empty() || !rig.idle_fx_cues().empty();
        }
        m_working_meshes = rig.bind_meshes();  // Allocate/copy once, never per frame.
        for (std::size_t i = 0; i < rig.bind_meshes().size(); ++i) {
            terrain::GbMeshCpu& mesh = m_working_meshes[i];
            terrain::skin_mesh(mesh, m_skin_matrices);
            MonsterPart part;
            part.set_texture(rig.textures()[i]);
            part.set_alpha_blend(rig.alpha_blend_flags()[i]);
            part.set_alpha_test(rig.alpha_test_flags()[i]);
            part.set_two_sided(rig.two_sided_flags()[i]);
            m_upload_vertices.clear();
            m_upload_vertices.reserve(mesh.pos_uv.size() / 5);
            for (std::size_t v = 0; v + 4 < mesh.pos_uv.size(); v += 5) {
                m_upload_vertices.push_back({mesh.pos_uv[v],
                                             mesh.pos_uv[v + 1],
                                             mesh.pos_uv[v + 2],
                                             mesh.pos_uv[v + 3],
                                             mesh.pos_uv[v + 4]});
            }
            part.set_mesh(engine.create_mesh(m_upload_vertices.data(),
                                             m_upload_vertices.size(),
                                             mesh.indices.data(),
                                             mesh.indices.size()));
            engine.set_mesh_specular(part.mesh(), mesh.normals, mesh.specular_power);
            m_parts.push_back(part);
        }
        m_blend_in_timer_ms = blend_in_timer_ms;
        m_blend_in_time_ms = 0.0f;
        return !m_parts.empty() || !rig.fx().empty() || !rig.particles().empty() ||
               !rig.idle_fx_cues().empty();
    }

    void MonsterInstance::trigger_attack() {
        if (m_clip == Clip::Die || m_clip == Clip::Knee || m_clip == Clip::Behead || m_die_hold) {
            return;
        }
        m_clip = Clip::Attack;
        m_basic_attack_visual = false;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_last_skinned_time_ms = -1000.0f;
        m_attack_fx_bound = false;
        m_await_attack_fx = false;
        m_attack_fx_ready = false;
        m_pending_attack_fx_ms = -1.0f;
        m_pending_attack_fx = MonsterAttackFx{};
        m_action_damage = MonsterActionDamage{};
        m_action_damage_latched = false;
        m_action_damage_ready = false;
        m_action_damage_bound = false;
        m_action_damage_key_ms = -1.0f;
    }

    void MonsterInstance::trigger_basic_attack_visual(int motion) {
        if (m_clip == Clip::Die || m_clip == Clip::Knee || m_clip == Clip::Behead || m_die_hold) {
            return;
        }
        m_clip = Clip::Skill;
        m_skill_motion = motion > 0 ? motion : 1;
        m_basic_attack_visual = true;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_last_skinned_time_ms = -1000.0f;
        m_last_idle_fx_time_ms = -1.0f;
        m_pending_idle_fx.clear();
        m_action_damage = MonsterActionDamage{};
        m_action_damage_latched = false;
        m_action_damage_ready = false;
        m_action_damage_bound = false;
        m_action_damage_key_ms = -1.0f;
    }

    void MonsterInstance::trigger_skill(int motion) {
        if (m_clip == Clip::Die || m_clip == Clip::Knee || m_clip == Clip::Behead || m_die_hold) {
            return;
        }
        m_clip = Clip::Skill;
        m_skill_motion = motion > 0 ? motion : 1;
        m_basic_attack_visual = false;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_last_skinned_time_ms = -1000.0f;
        m_last_idle_fx_time_ms = -1.0f;
        m_pending_idle_fx.clear();
    }

    void MonsterInstance::latch_action_damage(const MonsterActionDamage& hit) {
        m_action_damage = hit;
        m_action_damage_latched = true;
        m_action_damage_ready = false;
        m_action_damage_bound = false;
        m_action_damage_key_ms = -1.0f;
    }

    bool MonsterInstance::take_action_damage(MonsterActionDamage& out) {
        if (!m_action_damage_ready) {
            return false;
        }
        m_action_damage_ready = false;
        out = m_action_damage;
        m_action_damage = MonsterActionDamage{};
        return true;
    }

    bool MonsterInstance::take_attack_fx(MonsterAttackFx& out) {
        if (!m_attack_fx_ready) {
            return false;
        }
        m_attack_fx_ready = false;
        out = m_pending_attack_fx;
        return true;
    }

    bool MonsterInstance::take_idle_fx(std::string& out) {
        if (m_pending_idle_fx.empty()) {
            return false;
        }
        out = std::move(m_pending_idle_fx.front());
        m_pending_idle_fx.erase(m_pending_idle_fx.begin());
        return true;
    }

    void MonsterInstance::trigger_die() {
        if (m_clip == Clip::Die || m_clip == Clip::Behead) {
            return;
        }
        m_clip = Clip::Die;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_die_hold = false;
        m_sink_y = 0.0f;
        m_sink_sec = 0.0f;
        m_ready_to_despawn = false;
        m_last_skinned_time_ms = -1000.0f;
    }

    void MonsterInstance::trigger_knee() {
        if (m_clip == Clip::Die || m_clip == Clip::Behead || m_clip == Clip::Knee) {
            return;
        }
        m_clip = Clip::Knee;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_die_hold = false;
        m_sink_y = 0.0f;
        m_sink_sec = 0.0f;
        m_ready_to_despawn = false;
        m_last_skinned_time_ms = -1000.0f;
        // A kneeling Maguni is a death-state transition, not a newly spawned monster.
        m_blend_in_timer_ms = 0.0f;
        m_blend_in_time_ms = 0.0f;
    }

    void MonsterInstance::trigger_behead() {
        m_clip = Clip::Behead;
        m_anim_time_ms = 0.0f;
        m_attack_reverse = false;
        m_die_hold = false;
        m_sink_y = 0.0f;
        m_sink_sec = 0.0f;
        m_ready_to_despawn = false;
        m_last_skinned_time_ms = -1000.0f;
    }

    void MonsterInstance::trigger_hit_flash(bool critical) {
        m_hit_flash_duration_ms = critical ? 120.0f : 90.0f;
        m_hit_flash_peak = critical ? 1.0f : 0.9f;
        m_hit_flash_ms = m_hit_flash_duration_ms;
    }

    bool MonsterInstance::update(engine::IEngine& engine,
                                 const MonsterRigGpu& rig,
                                 float dt,
                                 bool moving,
                                 bool running,
                                 Audio* audio,
                                 float wx,
                                 float wy,
                                 float wz,
                                 bool skin_this_frame) {
        const bool mesh_ok = !m_parts.empty() && rig.bind_meshes().size() == m_parts.size() &&
                             m_working_meshes.size() == m_parts.size();
        const bool fx_only = m_parts.empty() && (!rig.fx().empty() || !rig.particles().empty() ||
                                                 !rig.idle_fx_cues().empty());
        if (!mesh_ok && !fx_only) {
            return false;
        }
        if (fx_only) {
            moving = false;
            running = false;
        }
        const float dt_ms = dt * 1000.0f;
        if (m_blend_in_timer_ms > 0.0f && m_blend_in_time_ms < m_blend_in_timer_ms) {
            m_blend_in_time_ms = std::min(m_blend_in_timer_ms, m_blend_in_time_ms + dt_ms);
        }
        if (m_hit_flash_ms > 0.0f) {
            m_hit_flash_ms = std::max(0.0f, m_hit_flash_ms - dt_ms);
        }
        constexpr float kDieSinkSec = 0.8f;
        constexpr float kDieSinkDepth = 14.0f;
        if (m_clip == Clip::Die || m_clip == Clip::Knee || m_clip == Clip::Behead) {
            bool has = false;
            float dur = 1.0f;
            if (m_clip == Clip::Die && rig.has_die()) {
                has = true;
                dur = std::max(1.0f, rig.die().duration_ms());
            } else if (m_clip == Clip::Knee && rig.has_knee()) {
                has = true;
                dur = std::max(1.0f, rig.knee().duration_ms());
            } else if (m_clip == Clip::Behead && rig.has_behead()) {
                has = true;
                dur = std::max(1.0f, rig.behead().duration_ms());
            }
            if (has) {
                if (!m_die_hold) {
                    m_anim_time_ms += dt_ms;
                    if (m_anim_time_ms >= dur) {
                        m_anim_time_ms = dur;
                        m_die_hold = true;
                    }
                }
            } else {
                m_die_hold = true;
            }
            if (m_die_hold && m_clip == Clip::Die) {
                m_sink_sec += dt;
                const float t = std::min(1.0f, m_sink_sec / kDieSinkSec);
                m_sink_y = -kDieSinkDepth * t;
                if (t >= 1.0f) {
                    m_ready_to_despawn = true;
                }
            } else if (m_die_hold && m_clip == Clip::Behead) {
                m_ready_to_despawn = true;
            }
        } else if (m_clip == Clip::Attack && rig.has_attack()) {
            if (moving) {
                // Kal MoveMonster ModelChange(walk/run) interrupts GO_ATTACK. Playing
                // the clip backwards after aspeed looked like a second swing on the run.
                poll_action_damage(m_action_damage_latched,
                                   m_action_damage_ready,
                                   m_action_damage_bound,
                                   m_action_damage_key_ms,
                                   m_anim_time_ms,
                                   &rig.attack(),
                                   true);
                m_clip = (running && rig.has_run()) ? Clip::Run
                         : rig.has_walk()           ? Clip::Walk
                         : rig.has_run()            ? Clip::Run
                                                    : Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_attack_reverse = false;
                m_attack_fx_bound = false;
                m_await_attack_fx = false;
                m_last_skinned_time_ms = -1000.0f;
                if (audio) {
                    m_sounds.stop(*audio);
                }
            } else {
                const float dur = std::max(1.0f, rig.attack().duration_ms());
                if (!m_attack_fx_bound) {
                    arm_attack_fx(m_attack_fx_bound,
                                  m_await_attack_fx,
                                  m_attack_fx_ready,
                                  m_pending_attack_fx_ms,
                                  m_pending_attack_fx,
                                  rig.attack());
                }
                m_anim_time_ms += dt_ms;
                poll_attack_fx(
                    m_await_attack_fx, m_attack_fx_ready, m_anim_time_ms, m_pending_attack_fx_ms);
                poll_action_damage(m_action_damage_latched,
                                   m_action_damage_ready,
                                   m_action_damage_bound,
                                   m_action_damage_key_ms,
                                   m_anim_time_ms,
                                   &rig.attack(),
                                   m_anim_time_ms >= dur);
                if (m_anim_time_ms >= dur) {
                    // OnAnimationComplete: play once, then idle (no ping-pong).
                    m_clip = Clip::Idle;
                    m_anim_time_ms = 0.0f;
                    m_attack_reverse = false;
                    m_attack_fx_bound = false;
                    m_await_attack_fx = false;
                    m_last_idle_fx_time_ms = -1.0f;
                    m_pending_idle_fx.clear();
                    m_last_skinned_time_ms = -1000.0f;
                    if (audio) {
                        m_sounds.stop(*audio);
                    }
                }
            }
        } else if (m_clip == Clip::Skill) {
            const terrain::GbAnimClip* skill = rig.skill(m_skill_motion);
            if (skill == nullptr || moving) {
                poll_action_damage(m_action_damage_latched,
                                   m_action_damage_ready,
                                   m_action_damage_bound,
                                   m_action_damage_key_ms,
                                   m_anim_time_ms,
                                   skill,
                                   true);
                m_clip = (running && rig.has_run()) ? Clip::Run
                         : rig.has_walk()           ? Clip::Walk
                         : rig.has_run()            ? Clip::Run
                                                    : Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_last_idle_fx_time_ms = -1.0f;
                m_pending_idle_fx.clear();
                m_basic_attack_visual = false;
                m_last_skinned_time_ms = -1000.0f;
                if (audio) {
                    m_sounds.stop(*audio);
                }
            } else {
                m_anim_time_ms += dt_ms;
                poll_idle_fx(m_pending_idle_fx,
                             m_last_idle_fx_time_ms,
                             m_anim_time_ms,
                             *skill,
                             rig.skill_fx_cues(m_skill_motion));
                if (m_basic_attack_visual) {
                    poll_action_damage(m_action_damage_latched,
                                       m_action_damage_ready,
                                       m_action_damage_bound,
                                       m_action_damage_key_ms,
                                       m_anim_time_ms,
                                       skill,
                                       m_anim_time_ms >= std::max(1.0f, skill->duration_ms()));
                }
                if (m_anim_time_ms >= std::max(1.0f, skill->duration_ms())) {
                    m_clip = Clip::Idle;
                    m_anim_time_ms = 0.0f;
                    m_basic_attack_visual = false;
                    m_last_idle_fx_time_ms = -1.0f;
                    m_last_skinned_time_ms = -1000.0f;
                    if (audio) {
                        m_sounds.stop(*audio);
                    }
                }
            }
        } else {
            if (m_clip == Clip::Attack) {
                poll_action_damage(m_action_damage_latched,
                                   m_action_damage_ready,
                                   m_action_damage_bound,
                                   m_action_damage_key_ms,
                                   m_anim_time_ms,
                                   nullptr,
                                   true);
                m_clip = Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_attack_reverse = false;
                m_attack_fx_bound = false;
                m_await_attack_fx = false;
                m_last_idle_fx_time_ms = -1.0f;
                m_pending_idle_fx.clear();
            }
            Clip want = Clip::Idle;
            if (moving) {
                if (running && rig.has_run()) {
                    want = Clip::Run;
                } else if (rig.has_walk()) {
                    want = Clip::Walk;
                } else if (rig.has_run()) {
                    want = Clip::Run;
                }
            }
            if (want != m_clip) {
                m_clip = want;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
                m_last_idle_fx_time_ms = -1.0f;
                m_pending_idle_fx.clear();
            }
            // Only speed up walk when the run GB is missing (Kal ModelChange(2) clip).
            const float anim_scale =
                (m_clip == Clip::Walk && running && !rig.has_run()) ? 1.5f : 1.0f;
            m_anim_time_ms += dt_ms * anim_scale;
            if (m_clip == Clip::Idle) {
                poll_idle_fx(m_pending_idle_fx,
                             m_last_idle_fx_time_ms,
                             m_anim_time_ms,
                             rig.idle(),
                             rig.idle_fx_cues());
            }
        }
        // Resolve the active clip every frame so sound and authored cues keep their timing even
        // when a distant mesh skinning pass is deferred.
        const terrain::GbAnimClip* cur = &rig.idle();
        bool loop = true;
        if (m_clip == Clip::Walk) {
            cur = &rig.walk();
        } else if (m_clip == Clip::Run && rig.has_run()) {
            cur = &rig.run();
        } else if (m_clip == Clip::Attack && rig.has_attack()) {
            cur = &rig.attack();
            loop = false;
        } else if (m_clip == Clip::Skill && rig.skill(m_skill_motion) != nullptr) {
            cur = rig.skill(m_skill_motion);
            loop = false;
        } else if (m_clip == Clip::Die && rig.has_die()) {
            cur = &rig.die();
            loop = false;
        } else if (m_clip == Clip::Knee && rig.has_knee()) {
            cur = &rig.knee();
            loop = false;
        } else if (m_clip == Clip::Behead && rig.has_behead()) {
            cur = &rig.behead();
            loop = false;
        }
        if (audio) {
            if (m_sound_clip != m_clip) {
                m_sounds.reset(audio);
                m_sound_clip = m_clip;
            }
            m_sounds.poll(*audio, *cur, m_anim_time_ms, loop, wx, wy, wz);
        }
        if (!mesh_ok) {
            return false;
        }
        const bool action_requires_full_rate = m_clip == Clip::Attack || m_clip == Clip::Skill ||
                                               m_clip == Clip::Die || m_clip == Clip::Knee ||
                                               m_clip == Clip::Behead;
        if (!skin_this_frame && !action_requires_full_rate) {
            return false;
        }
        if (!evaluate_pose(rig, *cur, std::max(0.0f, m_anim_time_ms), loop)) {
            return false;
        }
        m_last_skinned_time_ms = m_anim_time_ms;
        for (std::size_t i = 0; i < m_parts.size(); ++i) {
            terrain::GbMeshCpu& mesh = m_working_meshes[i];
            terrain::skin_mesh(mesh, m_skin_matrices);
            m_upload_vertices.clear();
            m_upload_vertices.reserve(mesh.pos_uv.size() / 5);
            for (std::size_t v = 0; v + 4 < mesh.pos_uv.size(); v += 5) {
                m_upload_vertices.push_back({mesh.pos_uv[v],
                                             mesh.pos_uv[v + 1],
                                             mesh.pos_uv[v + 2],
                                             mesh.pos_uv[v + 3],
                                             mesh.pos_uv[v + 4]});
            }
            engine.update_mesh(
                m_parts[i].mesh(), m_upload_vertices.data(), m_upload_vertices.size());
            if (mesh.specular_power > 0.0f) {
                engine.set_mesh_specular(m_parts[i].mesh(), mesh.normals, mesh.specular_power);
            }
        }
        return true;
    }

    void MonsterInstance::draw(engine::IEngine& engine,
                               const engine::Mat4& model,
                               bool want_alpha) const {
        const float opacity = blend_opacity();
        const bool fading = opacity < 0.999f;
        if (fading && !want_alpha) {
            return;
        }
        float mul_r = 1.0f;
        float mul_g = 1.0f;
        float mul_b = 1.0f;
        if (m_ice_slowed) {
            mul_r = 0.0f;
            mul_g = 0.5f;
            mul_b = 0.5f;
        }
        float white_mix = 0.0f;
        if (m_hit_flash_ms > 0.0f && m_hit_flash_duration_ms > 0.0f) {
            const float t = m_hit_flash_ms / m_hit_flash_duration_ms;
            white_mix = t * t * m_hit_flash_peak;
        }
        for (const auto& part : m_parts) {
            const bool use_blend = fading || part.alpha_blend();
            if (use_blend != want_alpha) {
                continue;
            }
            engine.draw_mesh_textured(part.mesh(),
                                      part.texture(),
                                      model,
                                      use_blend,
                                      part.two_sided(),
                                      part.alpha_test() && !fading,
                                      false,
                                      0.0f,
                                      0.0f,
                                      mul_r,
                                      mul_g,
                                      mul_b,
                                      opacity,
                                      white_mix);
        }
    }

    void MonsterInstance::draw_shadow(engine::IEngine& engine,
                                      const engine::Mat4& model,
                                      const engine::ShadowReceiver& receiver,
                                      float ground_y,
                                      float opacity) const {
        for (const auto& part : m_parts) {
            engine.draw_mesh_shadow(part.mesh(),
                                    part.texture(),
                                    model,
                                    receiver,
                                    ground_y,
                                    part.alpha_test() || part.alpha_blend(),
                                    opacity);
        }
    }

    bool MonsterInstance::evaluate_pose(const MonsterRigGpu& rig,
                                        const terrain::GbAnimClip& clip,
                                        float time_ms,
                                        bool loop) const {
        if (m_pose_rig == &rig && m_pose_clip == &clip && m_pose_time_ms == time_ms &&
            m_pose_loop == loop) {
            return true;
        }
        m_pose_rig = nullptr;
        if (!terrain::eval_bone_and_skin_matrices_at_time(
                rig.bones(), clip, time_ms, loop, m_bone_matrices, m_skin_matrices)) {
            return false;
        }
        m_pose_rig = &rig;
        m_pose_clip = &clip;
        m_pose_time_ms = time_ms;
        m_pose_loop = loop;
        return true;
    }

    bool MonsterInstance::bone_world_matrix(const MonsterRigGpu& rig,
                                            const engine::Mat4& model,
                                            int bone_index,
                                            engine::Mat4& out) const {
        if (bone_index < 0 || bone_index >= static_cast<int>(rig.bones().size())) {
            return false;
        }
        const terrain::GbAnimClip* cur = &rig.idle();
        bool loop = true;
        float sample_t = m_anim_time_ms;
        if (m_clip == Clip::Walk && rig.has_walk()) {
            cur = &rig.walk();
        } else if (m_clip == Clip::Run && rig.has_run()) {
            cur = &rig.run();
        } else if (m_clip == Clip::Attack && rig.has_attack()) {
            cur = &rig.attack();
            loop = false;
            sample_t = std::max(0.0f, m_anim_time_ms);
        } else if (m_clip == Clip::Skill && rig.skill(m_skill_motion) != nullptr) {
            cur = rig.skill(m_skill_motion);
            loop = false;
            sample_t = std::max(0.0f, m_anim_time_ms);
        } else if (m_clip == Clip::Die && rig.has_die()) {
            cur = &rig.die();
            loop = false;
            sample_t = std::max(0.0f, m_anim_time_ms);
        } else if (m_clip == Clip::Knee && rig.has_knee()) {
            cur = &rig.knee();
            loop = false;
            sample_t = std::max(0.0f, m_anim_time_ms);
        } else if (m_clip == Clip::Behead && rig.has_behead()) {
            cur = &rig.behead();
            loop = false;
            sample_t = std::max(0.0f, m_anim_time_ms);
        }
        if (!evaluate_pose(rig, *cur, sample_t, loop)) {
            return false;
        }
        if (bone_index >= static_cast<int>(m_bone_matrices.size())) {
            return false;
        }
        engine::Mat4 bone_mat;
        bone_mat.m = m_bone_matrices[static_cast<std::size_t>(bone_index)];
        out = engine::Mat4::multiply(model, bone_mat);
        return true;
    }

    bool MonsterInstance::bone_world(const MonsterRigGpu& rig,
                                     const engine::Mat4& model,
                                     int bone_index,
                                     float& ox,
                                     float& oy,
                                     float& oz) const {
        engine::Mat4 w;
        if (!bone_world_matrix(rig, model, bone_index, w)) {
            return false;
        }
        ox = w.m[12];
        oy = w.m[13];
        oz = w.m[14];
        return true;
    }

}  // namespace openkal::client
