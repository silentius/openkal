/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AnimatedActor.h"

#include "openkal/client/Audio.h"
#include "openkal/engine/TransparentMesh.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/HarvestPlant.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

namespace openkal::client {
    namespace {

        void bind_arrow_key(
            const terrain::GbAnimClip& clip, float& time_ms, float& ox, float& oy, float& oz) {
            time_ms = -1.0f;
            ox = 0.0f;
            oy = 0.0f;
            oz = 0.0f;
            const std::size_t n = std::min(clip.key_times.size(), clip.key_options.size());
            for (std::size_t i = 0; i < n; ++i) {
                const std::string& opt = clip.key_options[i];
                if (opt.find("EFFECT_ARROW") == std::string::npos) {
                    continue;
                }
                time_ms = static_cast<float>(clip.key_times[i]);
                const auto off = opt.find("OFFSET");
                if (off != std::string::npos) {
                    std::sscanf(opt.c_str() + off, "OFFSET %f %f %f", &ox, &oy, &oz);
                }
                return;
            }
        }

        void arm_arrow(bool& await,
                       bool& ready,
                       float& pending_ms,
                       float& px,
                       float& py,
                       float& pz,
                       float time_ms,
                       float ox,
                       float oy,
                       float oz) {
            px = ox;
            py = oy;
            pz = oz;
            ready = false;
            await = true;
            pending_ms = time_ms < 0.0f ? 0.0f : time_ms;
        }

        void disarm_arrow(bool& await, bool& ready) {
            await = false;
            ready = false;
        }

        /** True when the GB mesh is a weapon FX socket (effect_00, …). */
        [[nodiscard]] bool is_effect_socket_name(const std::string& name) {
            if (name.size() < 6) {
                return false;
            }
            static constexpr char kPrefix[] = "effect";
            for (std::size_t i = 0; i < 6; ++i) {
                if (std::tolower(static_cast<unsigned char>(name[i])) != kPrefix[i]) {
                    return false;
                }
            }
            return true;
        }

        /** Resolves the class-local fishing-rod mesh for a fishing motion variant. */
        [[nodiscard]] std::string
        resolve_fishing_rod_gb(const std::string& data_root, Class clazz, int variant) {
            const std::string dir = platform::join_path(data_root, "Item/Weapon");
            char name[32];
            std::snprintf(name,
                          sizeof(name),
                          "P%c_C1_%02d.gb",
                          EquipmentPaths::species_letter(clazz),
                          variant);
            std::string path = EquipmentPaths::resolve_existing(dir, name);
            if (!path.empty()) {
                return path;
            }

            // The class-specific meshes contain the hand-local coordinate correction.
            // PG is a safe fallback for incomplete resource packs.
            std::snprintf(name, sizeof(name), "PG_C1_%02d.gb", variant);
            return EquipmentPaths::resolve_existing(dir, name);
        }

        void poll_arrow_release(bool& await, bool& ready, float anim_time_ms, float pending_ms) {
            if (await && anim_time_ms >= pending_ms) {
                await = false;
                ready = true;
            }
        }

        /** True when option spawns a melee impact model (effect_attack*). */
        [[nodiscard]] bool is_melee_hit_effect_model(std::string_view option) {
            const auto pos = option.find("(*EFFECT_MODEL");
            if (pos == std::string_view::npos) {
                return false;
            }
            auto rest = option.substr(pos + 14);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                rest.remove_prefix(1);
            }
            return rest.size() >= 13 && rest.compare(0, 13, "effect_attack") == 0;
        }

        /** Resolves `T*_7_A_{sub}.gb` (then B / no-letter) under Model/Motion. */
        [[nodiscard]] std::string resolve_go_skill_gb(
            const std::string& data_root, Class clazz, int sub, int metamo_type, int metamo_level) {
            const std::string motion_dir = platform::join_path(data_root, "Model/Motion");
            auto named = [&](const char* name) -> std::string {
                std::string p = platform::join_path(motion_dir, name);
                return platform::file_exists(p) ? p : std::string{};
            };
            if (metamo_type >= 0 && metamo_level >= 1) {
                const char* kind = (metamo_level >= 3) ? "TGD" : "TGB";
                char built[64];
                std::snprintf(built, sizeof(built), "%s%02d_7_A_%02d.gb", kind, metamo_type, sub);
                std::string found = named(built);
                if (found.empty()) {
                    std::snprintf(built, sizeof(built), "%s%02d_7_%02d.gb", kind, metamo_type, sub);
                    found = named(built);
                }
                return found;
            }
            const char job = LocalPlayer::job_letter(clazz);
            const char* fmts[] = {"T%c_7_A_%02d.gb", "T%c_7_B_%02d.gb", "T%c_7_%02d.gb"};
            char name[64];
            for (const char* fmt : fmts) {
                std::snprintf(name, sizeof(name), fmt, job, sub);
                std::string p = named(name);
                if (!p.empty()) {
                    return p;
                }
                for (char* q = name; *q; ++q) {
                    if (q[0] == '_' && (q[1] == 'A' || q[1] == 'B') && q[2] == '_') {
                        q[1] = static_cast<char>(std::tolower(static_cast<unsigned char>(q[1])));
                        break;
                    }
                }
                p = named(name);
                if (!p.empty()) {
                    return p;
                }
            }
            return {};
        }

        /** Collects EFFECT_MODEL / EFFECT_FX cues crossed between prev and curr. */
        void poll_clip_effect_cues(const terrain::GbAnimClip& clip,
                                   float prev_ms,
                                   float curr_ms,
                                   std::vector<AnimatedActor::ClipEffectCue>& out) {
            const float lower = prev_ms < 0.0f ? -1.0f : prev_ms;
            const std::size_t n = std::min(clip.key_times.size(), clip.key_options.size());
            for (std::size_t i = 0; i < n; ++i) {
                const float t = static_cast<float>(clip.key_times[i]);
                if (!(lower < t && t <= curr_ms)) {
                    continue;
                }
                AnimatedActor::parse_clip_effect_cues(clip.key_options[i], out);
            }
        }

    }  // namespace

    void AnimatedActor::destroy(engine::IEngine& engine) {
        for (auto& p : m_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
                p.set_mesh({});
            }
        }
        for (auto& p : m_weapon_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
                p.set_mesh({});
            }
        }
        for (auto& p : m_shield_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
                p.set_mesh({});
            }
        }
        m_parts.clear();
        m_meshes.clear();
        m_weapon_parts.clear();
        m_weapon_bind.clear();
        for (auto& p : m_fishing_cast_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
                p.set_mesh({});
            }
        }
        for (auto& p : m_fishing_hold_parts) {
            if (p.mesh()) {
                engine.destroy_mesh(p.mesh());
                p.set_mesh({});
            }
        }
        m_fishing_cast_parts.clear();
        m_fishing_cast_bind.clear();
        m_fishing_hold_parts.clear();
        m_fishing_hold_bind.clear();
        m_shield_parts.clear();
        m_shield_bind.clear();
        m_have_weapon_bone_mat = false;
        m_have_shield_bone_mat = false;
        m_have_effect_bone_mat = false;
        m_last_skinned_time_ms = -1000.0f;
        m_skill_extra.clear();
        m_emotes.clear();
        m_emote_motion = 0;
        m_skill_motion_sub = 3;
        m_pending_clip_effects.clear();
        m_clip_effect_prev_ms = -1.0f;
        m_effect_clip = Clip::Idle;
    }

    bool AnimatedActor::load_meshes_from_paths(
        engine::IEngine& engine,
        const std::vector<std::string>& gb_paths,
        std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
        const terrain::GbAnimClip& pose_clip,
        std::vector<terrain::GbMeshCpu>& out_meshes,
        std::vector<AnimatedPart>& out_parts,
        bool skin_now) {
        for (const auto& gb_path : gb_paths) {
            terrain::GbModel cpu;
            if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
                continue;
            }
            for (auto& mesh : cpu.meshes) {
                AnimatedPart part;
                part.set_alpha_blend(mesh.alpha_blend);
                part.set_alpha_test(mesh.alpha_test);
                part.set_two_sided(mesh.two_sided);

                std::string tex_name;
                if (mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(cpu.materials.size())) {
                    tex_name = cpu.materials[mesh.material_index].texture;
                }
                const std::string tex_key = gb_path + "|" + tex_name;
                auto tit = tex_cache.find(tex_key);
                if (tit != tex_cache.end()) {
                    part.set_texture(tit->second);
                } else {
                    terrain::ImageRgba img;
                    if (!terrain::load_gb_texture(gb_path, tex_name, img) || img.rgba.empty()) {
                        img.width = img.height = 1;
                        img.rgba = {180, 160, 140, 255};
                    }
                    part.set_texture(
                        engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true));
                    tex_cache.emplace(tex_key, part.texture());
                }

                terrain::GbMeshCpu upload = mesh;
                if (skin_now) {
                    std::vector<std::array<float, 16>> skin;
                    if (terrain::eval_skin_matrices_at_time(m_bones, pose_clip, 0.0f, true, skin)) {
                        terrain::skin_mesh(upload, skin);
                    }
                }
                std::vector<engine::VertexP3T2> verts;
                verts.reserve(upload.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < upload.pos_uv.size(); i += 5) {
                    verts.push_back({upload.pos_uv[i],
                                     upload.pos_uv[i + 1],
                                     upload.pos_uv[i + 2],
                                     upload.pos_uv[i + 3],
                                     upload.pos_uv[i + 4]});
                }
                part.set_mesh(engine.create_mesh(
                    verts.data(), verts.size(), upload.indices.data(), upload.indices.size()));
                engine.set_mesh_specular(part.mesh(), upload.normals, upload.specular_power);
                out_meshes.push_back(std::move(mesh));
                out_parts.push_back(part);
            }
        }
        return !out_parts.empty();
    }

    void AnimatedActor::set_appearance(int face, int hair) {
        m_face = std::clamp(face, 1, 255);
        m_hair = std::clamp(hair, 1, 255);
    }

    bool AnimatedActor::load(engine::IEngine& engine,
                             const std::string& data_root_in,
                             Class clazz_in,
                             std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                             const Wear* wear,
                             const ItemDb* catalog) {
        destroy(engine);
        m_data_root = data_root_in;
        m_clazz = clazz_in;
        m_clip = Clip::Idle;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        reset_clip_effect_poll();
        m_die_hold = false;
        m_has_die = false;
        m_has_attack = false;
        m_has_attack2 = false;
        m_has_attack3 = false;
        m_has_skill = false;
        m_has_skill_charge = false;
        m_has_behead = false;
        m_skill_use_charge = false;
        m_skill_use_behead = false;
        m_skill_motion_sub = 3;
        m_skill_extra.clear();
        m_lock_skill_pose = false;
        m_has_open_box = false;
        m_harvest_once = false;
        m_has_fishing = false;
        m_fishing_reeling = false;
        m_has_sit = false;
        m_has_sit_consume = false;
        m_has_stand = false;
        m_has_evade = false;
        m_has_riding = false;
        m_weapon_bone = EquipmentPaths::weapon_bone_index(m_clazz);
        m_shield_bone = EquipmentPaths::shield_bone_index(m_clazz);
        m_effect_bone = EquipmentPaths::effect_bone_index(m_clazz);

        std::vector<std::string> cloth_paths;
        if (wear && catalog) {
            cloth_paths = EquipmentPaths::clothes_paths_for_wear(
                m_data_root, m_clazz, *wear, *catalog, m_face, m_hair);
        } else {
            cloth_paths = LocalPlayer::default_clothes_paths(m_data_root, m_clazz, m_face, m_hair);
        }

        const char job = LocalPlayer::job_letter(m_clazz);
        const std::string motion_dir = platform::join_path(m_data_root, "Model/Motion");
        std::string metamo_kind;
        if (m_metamo_type >= 0 && m_metamo_level >= 1) {
            metamo_kind = (m_metamo_level >= 3) ? std::string("TGD") : std::string("TGB");
        }

        auto resolve_named = [&](const std::string& name) -> std::string {
            std::string p = platform::join_path(motion_dir, name);
            return platform::file_exists(p) ? p : std::string{};
        };

        auto resolve_motion =
            [&](const char* fmt_a, const char* fmt_b, char name_out[64]) -> std::string {
            if (!metamo_kind.empty()) {
                int action = 0;
                int sub = 1;
                char letter = 'A';
                if (std::sscanf(fmt_a, "T%%c_%d_%c_%d.gb", &action, &letter, &sub) != 3) {
                    letter = 0;
                    if (std::sscanf(fmt_a, "T%%c_%d_%d.gb", &action, &sub) != 2) {
                        action = 0;
                        sub = 1;
                    }
                }
                char built[64];
                if (letter != 0) {
                    std::snprintf(built,
                                  sizeof(built),
                                  "%s%02d_%d_%c_%02d.gb",
                                  metamo_kind.c_str(),
                                  m_metamo_type,
                                  action,
                                  letter,
                                  sub);
                } else {
                    std::snprintf(built,
                                  sizeof(built),
                                  "%s%02d_%d_%02d.gb",
                                  metamo_kind.c_str(),
                                  m_metamo_type,
                                  action,
                                  sub);
                }
                std::string found = resolve_named(built);
                if (found.empty()) {
                    std::snprintf(built,
                                  sizeof(built),
                                  "%s%02d_%d_%02d.gb",
                                  metamo_kind.c_str(),
                                  m_metamo_type,
                                  action,
                                  sub);
                    found = resolve_named(built);
                }
                std::snprintf(name_out, 64, "%s", built);
                return found;
            }
            std::snprintf(name_out, 64, fmt_a, job);
            std::string p = platform::join_path(motion_dir, name_out);
            if (platform::file_exists(p)) {
                return p;
            }
            // Thief packs often use lowercase action letters (Tt_8_a_01.gb vs
            // Tw_8_A_01.gb).
            for (char* q = name_out; *q; ++q) {
                if (q[0] == '_' && (q[1] == 'A' || q[1] == 'B') && q[2] == '_') {
                    q[1] = static_cast<char>(std::tolower(static_cast<unsigned char>(q[1])));
                    break;
                }
            }
            p = platform::join_path(motion_dir, name_out);
            if (platform::file_exists(p)) {
                return p;
            }
            std::snprintf(name_out, 64, fmt_b, job);
            p = platform::join_path(motion_dir, name_out);
            return platform::file_exists(p) ? p : std::string{};
        };

        char tmp[64];
        std::string bone_path;
        if (!metamo_kind.empty()) {
            char bone_name[64];
            std::snprintf(
                bone_name, sizeof(bone_name), "%s%02d_Bone.gb", metamo_kind.c_str(), m_metamo_type);
            bone_path = resolve_named(bone_name);
            if (bone_path.empty()) {
                std::snprintf(bone_name,
                              sizeof(bone_name),
                              "%s%02d_bone.gb",
                              metamo_kind.c_str(),
                              m_metamo_type);
                bone_path = resolve_named(bone_name);
            }
            if (bone_path.empty()) {
                metamo_kind.clear();
            } else {
                auto shinsu_clothes = EquipmentPaths::shinsu_clothes_paths(
                    m_data_root, m_metamo_type, m_metamo_level);
                if (!shinsu_clothes.empty()) {
                    cloth_paths = std::move(shinsu_clothes);
                }
            }
        }
        if (cloth_paths.empty()) {
            return false;
        }
        if (bone_path.empty()) {
            std::snprintf(tmp, sizeof(tmp), "T%c_bone.gb", job);
            bone_path = platform::join_path(motion_dir, tmp);
            if (!platform::file_exists(bone_path)) {
                std::snprintf(tmp,
                              sizeof(tmp),
                              "T%c_Bone.gb",
                              static_cast<char>(std::toupper(static_cast<unsigned char>(job))));
                bone_path = platform::join_path(motion_dir, tmp);
            }
        }
        const std::string idle_path = resolve_motion("T%c_0_A_01.gb", "T%c_0_01.gb", tmp);
        const std::string walk_path = resolve_motion("T%c_1_A_01.gb", "T%c_1_01.gb", tmp);
        // Player action 2 = die (GameUser::ActionType AT_DIE → 2). Not run.
        const std::string die_path = resolve_motion("T%c_2_A_01.gb", "T%c_2_01.gb", tmp);
        const ItemDef* weapon =
            wear && catalog && wear->has_weapon() ? catalog->find(wear->weapon_index()) : nullptr;
        const bool two_handed_knight = m_metamo_type < 0 && m_clazz == Class::Knight && weapon &&
                                       weapon->class_small() == "sword2h";
        const std::string attack_path = two_handed_knight
                                            ? resolve_motion("T%c_8_B_01.gb", "T%c_8_A_01.gb", tmp)
                                            : resolve_motion("T%c_8_A_01.gb", "T%c_8_01.gb", tmp);
        const std::string attack2_path = two_handed_knight
                                             ? resolve_motion("T%c_8_B_02.gb", "T%c_8_A_02.gb", tmp)
                                             : resolve_motion("T%c_8_A_02.gb", "T%c_8_02.gb", tmp);
        const std::string attack3_path = two_handed_knight
                                             ? resolve_motion("T%c_8_B_03.gb", "T%c_8_A_03.gb", tmp)
                                             : std::string{};
        // GO_SKILL fire (ModelSkill gubun=1 → runtime state[1], compatibility behavior / mage use
        // sub 3).
        const std::string skill_path = resolve_motion("T%c_7_A_03.gb", "T%c_7_03.gb", tmp);
        // GO_SKILL charge (ModelSkill gubun=0 → runtime state[0], compatibility behavior uses sub
        // 2).
        const std::string skill_charge_path = resolve_motion("T%c_7_A_02.gb", "T%c_7_02.gb", tmp);
        // compatibility behavior execute (ModelSkill gubun=1 → runtime state[1]=1) → T*_7_A_01.
        const std::string behead_path = resolve_motion("T%c_7_A_01.gb", "T%c_7_01.gb", tmp);
        // compatibility behavior / BeginProductionbar ITEMBOX → ModelChange(9, 4).
        const std::string open_box_path = resolve_motion("T%c_9_A_04.gb", "T%c_9_04.gb", tmp);
        // Early clients map AT_GENERAL to motion 6. The later asset pack used by
        // OpenKal has no T*_6 fishing clips; it provides T*_12_A_01 instead.
        std::string fishing_path = resolve_motion("T%c_6_A_01.gb", "T%c_6_01.gb", tmp);
        if (fishing_path.empty()) {
            fishing_path = resolve_motion("T%c_12_A_01.gb", "T%c_12_01.gb", tmp);
        }
        if (fishing_path.empty() && metamo_kind.empty()) {
            // The later fishing assets capitalize the class letter (TM/TR/TT),
            // whereas the regular player clips are named Tm/Tr/Tt.
            std::snprintf(tmp,
                          sizeof(tmp),
                          "T%c_12_A_01.gb",
                          static_cast<char>(std::toupper(static_cast<unsigned char>(job))));
            fishing_path = resolve_named(tmp);
        }
        const std::string sit_path = resolve_motion("T%c_5_A_01.gb", "T%c_5_01.gb", tmp);
        const std::string sit_consume_path = resolve_motion("T%c_5_A_03.gb", "T%c_5_03.gb", tmp);
        const std::string stand_path = resolve_motion("T%c_5_A_02.gb", "T%c_5_02.gb", tmp);
        const std::string evade_path = resolve_motion("T%c_3_A_01.gb", "T%c_3_01.gb", tmp);
        const std::string riding_path = resolve_motion("T%c_0_A_04.gb", "T%c_0_04.gb", tmp);

        terrain::GbModel skeleton;
        terrain::GbModel idle_gb;
        terrain::GbModel walk_gb;
        terrain::GbModel die_gb;
        terrain::GbModel attack_gb;
        terrain::GbModel attack2_gb;
        terrain::GbModel attack3_gb;
        terrain::GbModel skill_gb;
        terrain::GbModel skill_charge_gb;
        terrain::GbModel behead_gb;
        terrain::GbModel open_box_gb;
        terrain::GbModel fishing_gb;
        terrain::GbModel sit_gb;
        terrain::GbModel sit_consume_gb;
        terrain::GbModel stand_gb;
        terrain::GbModel evade_gb;
        terrain::GbModel riding_gb;
        if (!platform::file_exists(bone_path) || !terrain::load_gb(bone_path, skeleton) ||
            skeleton.bones.empty()) {
            log::warn("AnimatedActor: missing skeleton " + bone_path);
            return false;
        }
        if (idle_path.empty() || !terrain::load_gb(idle_path, idle_gb) || idle_gb.anims.empty()) {
            log::warn("AnimatedActor: missing idle motion");
            return false;
        }
        if (walk_path.empty() || !terrain::load_gb(walk_path, walk_gb) || walk_gb.anims.empty()) {
            log::warn("AnimatedActor: missing walk motion " + walk_path);
            return false;
        }

        m_bones = skeleton.bones;
        m_idle = idle_gb.anims[0];
        m_walk = walk_gb.anims[0];
        if (!die_path.empty() && terrain::load_gb(die_path, die_gb) && !die_gb.anims.empty()) {
            m_die = die_gb.anims[0];
            m_has_die = true;
        } else {
            log::warn("AnimatedActor: missing die motion " + die_path);
        }
        if (!attack_path.empty() && terrain::load_gb(attack_path, attack_gb) &&
            !attack_gb.anims.empty()) {
            m_attack = attack_gb.anims[0];
            m_has_attack = true;
        } else {
            log::warn("AnimatedActor: missing attack motion " + attack_path);
        }
        if (!attack2_path.empty() && terrain::load_gb(attack2_path, attack2_gb) &&
            !attack2_gb.anims.empty()) {
            m_attack2 = attack2_gb.anims[0];
            m_has_attack2 = true;
        }
        if (!attack3_path.empty() && terrain::load_gb(attack3_path, attack3_gb) &&
            !attack3_gb.anims.empty()) {
            m_attack3 = attack3_gb.anims[0];
            m_has_attack3 = true;
        }
        if (!skill_path.empty() && terrain::load_gb(skill_path, skill_gb) &&
            !skill_gb.anims.empty()) {
            m_skill = skill_gb.anims[0];
            m_has_skill = true;
        } else {
            log::warn("AnimatedActor: missing skill motion " + skill_path);
        }
        if (!skill_charge_path.empty() && terrain::load_gb(skill_charge_path, skill_charge_gb) &&
            !skill_charge_gb.anims.empty()) {
            m_skill_charge = skill_charge_gb.anims[0];
            m_has_skill_charge = true;
        }
        if (!behead_path.empty() && terrain::load_gb(behead_path, behead_gb) &&
            !behead_gb.anims.empty()) {
            m_behead = behead_gb.anims[0];
            m_has_behead = true;
        } else {
            log::warn("AnimatedActor: missing behead motion " + behead_path);
        }
        if (m_has_attack) {
            bind_arrow_key(m_attack,
                           m_attack_arrow_ms,
                           m_attack_arrow_ox,
                           m_attack_arrow_oy,
                           m_attack_arrow_oz);
        }
        if (m_has_attack2) {
            bind_arrow_key(m_attack2,
                           m_attack2_arrow_ms,
                           m_attack2_arrow_ox,
                           m_attack2_arrow_oy,
                           m_attack2_arrow_oz);
        }
        if (m_has_attack3) {
            bind_arrow_key(m_attack3,
                           m_attack3_arrow_ms,
                           m_attack3_arrow_ox,
                           m_attack3_arrow_oy,
                           m_attack3_arrow_oz);
        }
        if (m_has_skill) {
            bind_arrow_key(
                m_skill, m_skill_arrow_ms, m_skill_arrow_ox, m_skill_arrow_oy, m_skill_arrow_oz);
        }
        if (!open_box_path.empty() && terrain::load_gb(open_box_path, open_box_gb) &&
            !open_box_gb.anims.empty()) {
            m_open_box = open_box_gb.anims[0];
            m_has_open_box = true;
        } else {
            log::warn("AnimatedActor: missing open-box motion " + open_box_path);
        }
        if (!fishing_path.empty() && terrain::load_gb(fishing_path, fishing_gb) &&
            !fishing_gb.anims.empty()) {
            m_fishing = fishing_gb.anims[0];
            m_has_fishing = true;
            const std::string cast_rod_path = resolve_fishing_rod_gb(m_data_root, m_clazz, 1);
            const std::string hold_rod_path = resolve_fishing_rod_gb(m_data_root, m_clazz, 2);
            if (!load_meshes_from_paths(engine,
                                        {cast_rod_path},
                                        tex_cache,
                                        m_fishing,
                                        m_fishing_cast_bind,
                                        m_fishing_cast_parts,
                                        false)) {
                log::warn("AnimatedActor: missing fishing cast rod " + cast_rod_path);
            }
            if (!load_meshes_from_paths(engine,
                                        {hold_rod_path},
                                        tex_cache,
                                        m_fishing,
                                        m_fishing_hold_bind,
                                        m_fishing_hold_parts,
                                        false)) {
                log::warn("AnimatedActor: missing held fishing rod " + hold_rod_path);
            }
        } else {
            log::warn("AnimatedActor: missing fishing motion " + fishing_path);
        }
        if (!sit_path.empty() && terrain::load_gb(sit_path, sit_gb) && !sit_gb.anims.empty()) {
            m_sit = sit_gb.anims[0];
            m_has_sit = true;
        } else {
            log::warn("AnimatedActor: missing sit motion " + sit_path);
        }
        if (!sit_consume_path.empty() && terrain::load_gb(sit_consume_path, sit_consume_gb) &&
            !sit_consume_gb.anims.empty()) {
            m_sit_consume = sit_consume_gb.anims[0];
            m_has_sit_consume = true;
        } else {
            log::warn("AnimatedActor: missing sit-consume motion " + sit_consume_path);
        }
        if (!stand_path.empty() && terrain::load_gb(stand_path, stand_gb) &&
            !stand_gb.anims.empty()) {
            m_stand = stand_gb.anims[0];
            m_has_stand = true;
        } else {
            log::warn("AnimatedActor: missing stand-up motion " + stand_path);
        }
        if (!evade_path.empty() && terrain::load_gb(evade_path, evade_gb) &&
            !evade_gb.anims.empty()) {
            m_evade = evade_gb.anims[0];
            m_has_evade = true;
        } else {
            log::warn("AnimatedActor: missing evade motion " + evade_path);
        }
        if (!riding_path.empty() && terrain::load_gb(riding_path, riding_gb) &&
            !riding_gb.anims.empty()) {
            m_riding = riding_gb.anims[0];
            m_has_riding = true;
        } else {
            log::warn("AnimatedActor: missing riding motion " + riding_path);
        }

        if (!load_meshes_from_paths(
                engine, cloth_paths, tex_cache, m_idle, m_meshes, m_parts, true)) {
            return false;
        }

        if (wear && catalog && wear->has_weapon() && m_metamo_type < 0) {
            const ItemDef* def = catalog->find(wear->weapon_index());
            if (def && def->action_class() > 0) {
                const auto* skin = catalog->find(wear->skin_index(protocol::SkinSlot::Weapon));
                std::string wpath = skin && skin->skin_matches_weapon(*def)
                                        ? EquipmentPaths::skin_gb_path(m_data_root, m_clazz, *skin)
                                        : std::string{};
                if (wpath.empty()) {
                    wpath = EquipmentPaths::weapon_gb_path(
                        m_data_root,
                        m_clazz,
                        def->action_class(),
                        def->action_index() > 0 ? def->action_index() : 1);
                }
                if (!wpath.empty()) {
                    if (!load_meshes_from_paths(engine,
                                                {wpath},
                                                tex_cache,
                                                m_idle,
                                                m_weapon_bind,
                                                m_weapon_parts,
                                                false)) {
                        log::warn("AnimatedActor: failed to load weapon meshes from " + wpath);
                    } else {
                        log::info("AnimatedActor weapon " + wpath +
                                  " meshes=" + std::to_string(m_weapon_parts.size()));
                    }
                } else {
                    log::warn("AnimatedActor: weapon mesh missing for item " +
                              std::to_string(wear->weapon_index()));
                }
            }
        }

        const bool thief_dual_wield = wear && m_clazz == Class::Thief && wear->has_weapon();
        if (wear && catalog && (wear->has_shield() || thief_dual_wield) && m_metamo_type < 0) {
            const int offhand_index =
                thief_dual_wield ? wear->weapon_index() : wear->shield_index();
            const ItemDef* def = catalog->find(offhand_index);
            if (def && def->action_class() > 0 &&
                (!thief_dual_wield || def->class_small() == "dagger")) {
                const int ac = def->action_class();
                const int ai = def->action_index() > 0 ? def->action_index() : 1;
                const auto* skin = catalog->find(wear->skin_index(
                    thief_dual_wield ? protocol::SkinSlot::Weapon : protocol::SkinSlot::Shield));
                std::string spath = skin && (!thief_dual_wield || skin->skin_matches_weapon(*def))
                                        ? EquipmentPaths::skin_gb_path(m_data_root, m_clazz, *skin)
                                        : std::string{};
                if (spath.empty()) {
                    spath = EquipmentPaths::shield_gb_path(m_data_root, m_clazz, ac, ai);
                }
                if (spath.empty() && ai > 1) {
                    spath = EquipmentPaths::shield_gb_path(m_data_root, m_clazz, ac, 1);
                }
                if (!spath.empty()) {
                    if (!load_meshes_from_paths(engine,
                                                {spath},
                                                tex_cache,
                                                m_idle,
                                                m_shield_bind,
                                                m_shield_parts,
                                                false)) {
                        log::warn("AnimatedActor: failed to load shield meshes from " + spath);
                    } else {
                        log::info("AnimatedActor shield " + spath +
                                  " meshes=" + std::to_string(m_shield_parts.size()));
                    }
                } else {
                    log::warn("AnimatedActor: off-hand mesh missing for item " +
                              std::to_string(offhand_index));
                }
            }
        }

        log::info(std::string("AnimatedActor ") + LocalPlayer::class_name(m_clazz) +
                  " parts=" + std::to_string(m_parts.size()) +
                  " weapon=" + std::to_string(m_weapon_parts.size()) +
                  " shield=" + std::to_string(m_shield_parts.size()) +
                  " clips idle=" + std::to_string(m_idle.keyframe_count) +
                  " walk=" + std::to_string(m_walk.keyframe_count) +
                  " die=" + std::to_string(m_has_die ? m_die.keyframe_count : 0) +
                  " attack=" + std::to_string(m_has_attack ? m_attack.keyframe_count : 0) +
                  " attack2=" + std::to_string(m_has_attack2 ? m_attack2.keyframe_count : 0) +
                  " attack3=" + std::to_string(m_has_attack3 ? m_attack3.keyframe_count : 0) +
                  " skill=" + std::to_string(m_has_skill ? m_skill.keyframe_count : 0) +
                  " behead=" + std::to_string(m_has_behead ? m_behead.keyframe_count : 0) +
                  " open_box=" + std::to_string(m_has_open_box ? m_open_box.keyframe_count : 0) +
                  " fishing=" + std::to_string(m_has_fishing ? m_fishing.keyframe_count : 0) +
                  " sit=" + std::to_string(m_has_sit ? m_sit.keyframe_count : 0) + " sit_consume=" +
                  std::to_string(m_has_sit_consume ? m_sit_consume.keyframe_count : 0) +
                  " stand=" + std::to_string(m_has_stand ? m_stand.keyframe_count : 0) +
                  " evade=" + std::to_string(m_has_evade ? m_evade.keyframe_count : 0) +
                  " riding=" + std::to_string(m_has_riding ? m_riding.keyframe_count : 0));
        return !m_parts.empty();
    }

    bool
    AnimatedActor::apply_metamo(engine::IEngine& engine,
                                std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                                int type,
                                int level,
                                float scale,
                                const Wear* wear,
                                const ItemDb* catalog) {
        m_metamo_type = type;
        m_metamo_level = type < 0 ? 0 : std::max(1, level);
        m_metamo_scale = type < 0 ? 1.0f : std::max(0.1f, scale);
        if (load(engine, m_data_root, m_clazz, tex_cache, wear, catalog)) {
            return true;
        }
        if (type < 0) {
            return false;
        }
        m_metamo_type = -1;
        m_metamo_level = 0;
        const bool human = load(engine, m_data_root, m_clazz, tex_cache, wear, catalog);
        m_metamo_type = type;
        m_metamo_level = std::max(1, level);
        m_metamo_scale = std::max(0.1f, scale);
        return human;
    }

    bool
    AnimatedActor::apply_wear(engine::IEngine& engine,
                              std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                              const Wear& wear,
                              const ItemDb& catalog) {
        return load(engine, m_data_root, m_clazz, tex_cache, &wear, &catalog);
    }

    void AnimatedActor::trigger_attack() {
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_parts.empty() || !m_has_attack) {
            return;
        }
        // Leave sit/stand without ModelChange(5,2) when attacking from rest.
        // ModelChange(GO_ATTACK) also replaces a GO_SKILL pose.
        if (m_clip == Clip::Sit || m_clip == Clip::SitConsume || m_clip == Clip::Stand) {
            clear_rest_pose();
        }
        m_skill_use_behead = false;
        m_skill_use_charge = false;
        m_lock_skill_pose = false;
        m_skill_looping = false;
        // GetRandAttack advances the two-handed knight through B_01..B_03.
        const int attack_count = m_has_attack3 ? 3 : (m_has_attack2 ? 2 : 1);
        m_attack_variant = m_next_attack_variant % attack_count;
        m_next_attack_variant = (m_attack_variant + 1) % attack_count;
        m_clip = Clip::Attack;
        m_anim_time_ms = 0.0f;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        m_last_skinned_time_ms = -1000.0f;
        m_melee_hit_confirmed = false;
        m_melee_hit_model_passed = false;
        m_melee_hit_sfx_played = false;
        m_melee_hit_prev_ms = -1.0f;
        reset_clip_effect_poll();
        if (m_attack_variant == 2 && m_has_attack3) {
            arm_arrow(m_await_arrow,
                      m_arrow_ready,
                      m_pending_arrow_ms,
                      m_pending_arrow_ox,
                      m_pending_arrow_oy,
                      m_pending_arrow_oz,
                      m_attack3_arrow_ms,
                      m_attack3_arrow_ox,
                      m_attack3_arrow_oy,
                      m_attack3_arrow_oz);
        } else if (m_attack_variant == 1 && m_has_attack2) {
            arm_arrow(m_await_arrow,
                      m_arrow_ready,
                      m_pending_arrow_ms,
                      m_pending_arrow_ox,
                      m_pending_arrow_oy,
                      m_pending_arrow_oz,
                      m_attack2_arrow_ms,
                      m_attack2_arrow_ox,
                      m_attack2_arrow_oy,
                      m_attack2_arrow_oz);
        } else {
            arm_arrow(m_await_arrow,
                      m_arrow_ready,
                      m_pending_arrow_ms,
                      m_pending_arrow_ox,
                      m_pending_arrow_oy,
                      m_pending_arrow_oz,
                      m_attack_arrow_ms,
                      m_attack_arrow_ox,
                      m_attack_arrow_oy,
                      m_attack_arrow_oz);
        }
    }

    void AnimatedActor::trigger_behead_swing() {
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_parts.empty()) {
            return;
        }
        // The close-range Archer skill action uses the T*_7_A_01 bow-swing clip.
        if (m_has_behead) {
            m_skill_use_behead = true;
            m_skill_use_charge = false;
            m_skill_motion_sub = 1;
            m_lock_skill_pose = true;
            m_lock_attack_pose = true;
            m_attack_play_once = true;
            m_clip = Clip::Skill;
            m_anim_time_ms = 0.0f;
            m_skill_looping = false;
            m_last_skinned_time_ms = -1000.0f;
            disarm_arrow(m_await_arrow, m_arrow_ready);
            reset_clip_effect_poll();
            return;
        }
        if (!m_has_attack) {
            return;
        }
        m_attack_variant = 0;
        m_next_attack_variant = (m_has_attack2 || m_has_attack3) ? 1 : 0;
        m_clip = Clip::Attack;
        m_anim_time_ms = 0.0f;
        m_lock_attack_pose = true;
        m_attack_play_once = true;
        m_skill_use_charge = false;
        m_skill_use_behead = false;
        m_lock_skill_pose = false;
        m_skill_looping = false;
        m_last_skinned_time_ms = -1000.0f;
        disarm_arrow(m_await_arrow, m_arrow_ready);
        reset_clip_effect_poll();
    }

    bool AnimatedActor::behead_swing_cancelable() const {
        if (!m_lock_attack_pose) {
            return false;
        }
        const auto& clip = m_clip == Clip::Skill ? current_skill_clip() : m_attack;
        return m_anim_time_ms >= std::max(1.0f, clip.duration_ms()) * 0.5f;
    }

    void AnimatedActor::cancel_attack() {
        disarm_arrow(m_await_arrow, m_arrow_ready);
        m_skill_use_charge = false;
        m_skill_use_behead = false;
        m_skill_motion_sub = 3;
        m_lock_skill_pose = false;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        m_melee_hit_confirmed = false;
        m_melee_hit_model_passed = false;
        m_melee_hit_sfx_played = false;
        m_melee_hit_prev_ms = -1.0f;
        m_pending_clip_effects.clear();
        if (m_clip != Clip::Attack && m_clip != Clip::Skill) {
            return;
        }
        m_clip = Clip::Idle;
        m_anim_time_ms = 0.0f;
        m_skill_looping = false;
        m_last_skinned_time_ms = -1000.0f;
        reset_clip_effect_poll();
    }

    void AnimatedActor::note_melee_hit() {
        m_melee_hit_confirmed = true;
    }

    bool AnimatedActor::trigger_emote(int motion) {
        const bool finished_skill = m_clip == Clip::Skill && !m_lock_skill_pose &&
                                    !m_skill_looping &&
                                    m_anim_time_ms >= current_skill_clip().duration_ms();
        if (motion < 1 || motion > protocol::kEmoteCount || m_parts.empty() || m_die_hold ||
            m_metamo_type >= 0 ||
            (m_clip != Clip::Idle && m_clip != Clip::Walk && m_clip != Clip::Emote &&
             !finished_skill)) {
            return false;
        }
        if (m_emotes.find(motion) == m_emotes.end()) {
            const auto directory = platform::join_path(m_data_root, "Model/Motion");
            char name[64];
            std::string path;
            for (const auto* format : {"T%c_10_A_%02d.gb", "T%c_10_a_%02d.gb", "T%c_10_%02d.gb"}) {
                std::snprintf(name, sizeof(name), format, LocalPlayer::job_letter(m_clazz), motion);
                path = EquipmentPaths::resolve_existing(directory, name);
                if (!path.empty()) {
                    break;
                }
            }
            terrain::GbModel model;
            if (path.empty() || !terrain::load_gb(path, model) || model.anims.empty() ||
                static_cast<std::size_t>(model.anims.front().bone_count) != m_bones.size()) {
                log::warn("AnimatedActor: unavailable emote motion=" + std::to_string(motion) +
                          " class=" + std::string(LocalPlayer::class_name(m_clazz)));
                return false;
            }
            m_emotes.emplace(motion, std::move(model.anims.front()));
        }
        m_emote_motion = motion;
        m_clip = Clip::Emote;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
        m_restart_sounds = true;
        disarm_arrow(m_await_arrow, m_arrow_ready);
        reset_clip_effect_poll();
        return true;
    }

    void AnimatedActor::trigger_skill() {
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_parts.empty()) {
            return;
        }
        m_skill_use_charge = false;
        m_skill_use_behead = false;
        m_skill_motion_sub = 3;
        m_lock_skill_pose = false;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        if (m_has_skill) {
            m_clip = Clip::Skill;
            m_anim_time_ms = 0.0f;
            m_skill_looping = false;
            m_last_skinned_time_ms = -1000.0f;
            arm_arrow(m_await_arrow,
                      m_arrow_ready,
                      m_pending_arrow_ms,
                      m_pending_arrow_ox,
                      m_pending_arrow_oy,
                      m_pending_arrow_oz,
                      m_skill_arrow_ms,
                      m_skill_arrow_ox,
                      m_skill_arrow_oy,
                      m_skill_arrow_oz);
            reset_clip_effect_poll();
        } else {
            trigger_attack();
        }
    }

    bool AnimatedActor::ensure_skill_sub(int sub) {
        if (sub <= 0) {
            return m_has_skill;
        }
        if (sub == 1) {
            return m_has_behead || m_has_skill;
        }
        if (sub == 2) {
            return m_has_skill_charge || m_has_skill;
        }
        if (sub == 3) {
            return m_has_skill;
        }
        if (m_skill_extra.find(sub) != m_skill_extra.end()) {
            return true;
        }
        if (m_data_root.empty()) {
            return false;
        }
        const std::string path =
            resolve_go_skill_gb(m_data_root, m_clazz, sub, m_metamo_type, m_metamo_level);
        if (path.empty()) {
            log::warn("AnimatedActor: missing GO_SKILL sub=" + std::to_string(sub) +
                      " class=" + std::string(LocalPlayer::class_name(m_clazz)));
            return false;
        }
        terrain::GbModel gb;
        if (!terrain::load_gb(path, gb) || gb.anims.empty()) {
            log::warn("AnimatedActor: failed to load GO_SKILL sub=" + std::to_string(sub) + " " +
                      path);
            return false;
        }
        m_skill_extra[sub] = gb.anims[0];
        return true;
    }

    void AnimatedActor::trigger_skill_motion(int sub, bool play_once) {
        if (sub <= 0) {
            trigger_skill();
            m_attack_play_once = play_once;
            return;
        }
        if (sub == 1) {
            trigger_behead_swing();
            return;
        }
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_parts.empty()) {
            return;
        }
        if (!ensure_skill_sub(sub)) {
            trigger_skill();
            m_attack_play_once = play_once;
            return;
        }
        m_skill_motion_sub = sub;
        m_skill_use_charge = (sub == 2 && m_has_skill_charge);
        m_skill_use_behead = false;
        m_lock_skill_pose = false;
        m_lock_attack_pose = false;
        m_attack_play_once = play_once;
        m_clip = Clip::Skill;
        m_anim_time_ms = 0.0f;
        m_skill_looping = false;
        m_last_skinned_time_ms = -1000.0f;
        disarm_arrow(m_await_arrow, m_arrow_ready);
        reset_clip_effect_poll();
    }

    void AnimatedActor::trigger_skill_charge() {
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_parts.empty()) {
            return;
        }
        if (!m_has_skill_charge && !m_has_skill) {
            trigger_attack();
            return;
        }
        m_skill_use_charge = m_has_skill_charge;
        m_skill_use_behead = false;
        m_skill_motion_sub = 2;
        m_lock_skill_pose = true;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        m_clip = Clip::Skill;
        m_anim_time_ms = 0.0f;
        m_skill_looping = false;
        m_last_skinned_time_ms = -1000.0f;
        disarm_arrow(m_await_arrow, m_arrow_ready);
        reset_clip_effect_poll();
    }

    void AnimatedActor::end_skill() {
        m_skill_use_charge = false;
        m_skill_use_behead = false;
        m_skill_motion_sub = 3;
        m_lock_skill_pose = false;
        m_lock_attack_pose = false;
        m_attack_play_once = false;
        m_skill_looping = false;
        disarm_arrow(m_await_arrow, m_arrow_ready);
        if (m_clip != Clip::Skill) {
            m_pending_clip_effects.clear();
            return;
        }
        m_clip = Clip::Idle;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
        reset_clip_effect_poll();
    }

    const terrain::GbAnimClip& AnimatedActor::current_skill_clip() const {
        if (m_skill_use_behead && m_has_behead) {
            return m_behead;
        }
        const auto extra = m_skill_extra.find(m_skill_motion_sub);
        if (extra != m_skill_extra.end()) {
            return extra->second;
        }
        if ((m_skill_use_charge || m_skill_motion_sub == 2) && m_has_skill_charge) {
            return m_skill_charge;
        }
        if (m_skill_motion_sub == 1 && m_has_behead) {
            return m_behead;
        }
        return m_skill;
    }

    void AnimatedActor::trigger_evade() {
        if (m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox || m_clip == Clip::Sit ||
            m_clip == Clip::SitConsume || m_clip == Clip::Stand) {
            return;
        }
        if (m_parts.empty() || !m_has_evade) {
            return;
        }
        // ActionDamage skips ModelChange(3) during attack/skill; walking may still
        // dodge.
        if (m_clip == Clip::Attack || m_clip == Clip::Skill) {
            return;
        }
        m_clip = Clip::Evade;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_die() {
        if (m_parts.empty() || m_clip == Clip::Die || m_die_hold) {
            return;
        }
        if (!m_has_die) {
            log::warn("AnimatedActor: die clip missing, cannot play death");
            return;
        }
        m_clip = Clip::Die;
        m_anim_time_ms = 0.0f;
        m_die_hold = false;
        m_last_skinned_time_ms = -1000.0f;
        disarm_arrow(m_await_arrow, m_arrow_ready);
    }

    bool AnimatedActor::death_animation_finished() const {
        return m_clip != Clip::Die || m_die_hold;
    }

    void AnimatedActor::clear_die() {
        m_die_hold = false;
        if (m_clip == Clip::Die) {
            m_clip = Clip::Idle;
            m_anim_time_ms = 0.0f;
            m_last_skinned_time_ms = -1000.0f;
        }
    }

    void AnimatedActor::trigger_open_box() {
        m_harvest_once = false;
        if (m_parts.empty() || m_die_hold || m_clip == Clip::Die) {
            return;
        }
        if (!m_has_open_box) {
            log::warn("AnimatedActor: open-box clip missing");
            return;
        }
        if (m_clip == Clip::OpenBox) {
            return;
        }
        m_clip = Clip::OpenBox;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_harvest() {
        trigger_open_box();
        m_harvest_once = m_clip == Clip::OpenBox;
    }

    void AnimatedActor::clear_open_box() {
        m_harvest_once = false;
        if (m_clip != Clip::OpenBox) {
            return;
        }
        m_clip = Clip::Idle;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_fishing() {
        if (m_parts.empty() || m_die_hold || m_clip == Clip::Die || !m_has_fishing) {
            return;
        }
        if (m_clip == Clip::Fishing) {
            return;
        }
        m_clip = Clip::Fishing;
        m_anim_time_ms = 0.0f;
        m_lock_attack_pose = false;
        m_lock_skill_pose = false;
        m_fishing_reeling = false;
        m_last_skinned_time_ms = -1000.0f;
        disarm_arrow(m_await_arrow, m_arrow_ready);
    }

    void AnimatedActor::restart_fishing_cycle() {
        if (m_clip != Clip::Fishing || !m_has_fishing) {
            trigger_fishing();
            return;
        }
        m_fishing_reeling = true;
        m_anim_time_ms = std::max(1.0f, m_fishing.duration_ms());
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::clear_fishing() {
        if (m_clip != Clip::Fishing) {
            return;
        }
        m_clip = Clip::Idle;
        m_fishing_reeling = false;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_sit() {
        if (m_parts.empty() || m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_clip == Clip::Sit) {
            return;
        }
        if (!m_has_sit) {
            return;
        }
        m_clip = Clip::Sit;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_sit_consume() {
        if (m_parts.empty() || m_die_hold || m_clip == Clip::Die || m_clip == Clip::OpenBox) {
            return;
        }
        if (m_clip == Clip::SitConsume) {
            return;
        }
        if (!m_has_sit_consume) {
            trigger_sit();
            return;
        }
        // Already sitting: Chk_MState ModelChange(5, 3) from sit pose — skip sit-down.
        const bool already_sitting = m_clip == Clip::Sit;
        m_clip = Clip::SitConsume;
        const float dur = std::max(1.0f, m_sit_consume.duration_ms());
        const float loop_start = m_sit_consume.loop_start_ms;
        if (already_sitting) {
            if (loop_start > 0.0f && loop_start < dur) {
                m_anim_time_ms = loop_start;
            } else {
                m_anim_time_ms = dur;
            }
        } else {
            m_anim_time_ms = 0.0f;
        }
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::trigger_stand() {
        if (m_clip != Clip::Sit && m_clip != Clip::SitConsume) {
            return;
        }
        if (!m_has_stand) {
            m_clip = Clip::Idle;
            m_anim_time_ms = 0.0f;
            m_last_skinned_time_ms = -1000.0f;
            return;
        }
        m_clip = Clip::Stand;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::clear_rest_pose() {
        if (m_clip != Clip::Sit && m_clip != Clip::SitConsume && m_clip != Clip::Stand) {
            return;
        }
        m_clip = Clip::Idle;
        m_anim_time_ms = 0.0f;
        m_last_skinned_time_ms = -1000.0f;
    }

    void AnimatedActor::update(engine::IEngine& engine,
                               float dt,
                               bool moving,
                               bool hasting,
                               bool resting,
                               bool consuming,
                               Audio* audio,
                               float wx,
                               float wy,
                               float wz,
                               bool fishing,
                               bool riding,
                               bool skin_geometry) {
        if (m_parts.empty()) {
            return;
        }
        riding = riding && m_has_riding;
        if (fishing) {
            trigger_fishing();
        } else if (m_clip == Clip::Fishing) {
            clear_fishing();
        }
        if (!fishing && resting && consuming) {
            if (m_clip != Clip::SitConsume && m_clip != Clip::Stand && m_clip != Clip::Die &&
                m_clip != Clip::OpenBox && m_clip != Clip::Evade && !m_die_hold) {
                trigger_sit_consume();
            }
        } else if (!fishing && resting) {
            if (m_clip == Clip::SitConsume) {
                trigger_sit();
            } else if (m_clip != Clip::Sit && m_clip != Clip::Stand && m_clip != Clip::Die &&
                       m_clip != Clip::OpenBox && m_clip != Clip::Evade && !m_die_hold) {
                trigger_sit();
            }
        } else if (!fishing && !resting &&
                   (m_clip == Clip::Sit || m_clip == Clip::SitConsume || m_clip == Clip::Stand)) {
            // AT_REST off: ModelChange(5, 2) only while ActionNo is still 5.
            // Move/attack already left action 5 → skip stand-up.
            if (moving) {
                clear_rest_pose();
            } else if (m_clip == Clip::Sit || m_clip == Clip::SitConsume) {
                trigger_stand();
            }
        }
        const float dt_ms = dt * 1000.0f;
        if (riding) {
            if (m_clip != Clip::Riding) {
                m_clip = Clip::Riding;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
            }
            m_anim_time_ms += dt_ms;
        } else if (m_clip == Clip::Die) {
            const float dur = std::max(1.0f, m_die.duration_ms());
            if (!m_die_hold) {
                m_anim_time_ms += dt_ms;
                if (m_anim_time_ms >= dur) {
                    m_anim_time_ms = dur;
                    m_die_hold = true;
                }
            }
        } else if (m_clip == Clip::OpenBox) {
            const float dur = std::max(1.0f, m_open_box.duration_ms());
            if (m_harvest_once) {
                m_anim_time_ms += dt_ms * dur / (protocol::kHarvestSeconds * 1000.0f);
                if (m_anim_time_ms >= dur) {
                    clear_open_box();
                }
            } else {
                const bool can_loop =
                    m_open_box.loop_start_ms > 0.0f && m_open_box.loop_start_ms < dur;
                // Repeat only an authored loop; otherwise hold the opening pose.
                m_anim_time_ms =
                    can_loop ? m_anim_time_ms + dt_ms : std::min(dur, m_anim_time_ms + dt_ms);
            }
        } else if (m_clip == Clip::Fishing) {
            const float dur = std::max(1.0f, m_fishing.duration_ms());
            if (m_fishing_reeling) {
                m_anim_time_ms -= dt_ms;
                if (m_anim_time_ms <= 0.0f) {
                    m_anim_time_ms = 0.0f;
                    m_fishing_reeling = false;
                }
            } else if (m_anim_time_ms < dur) {
                m_anim_time_ms = std::min(dur, m_anim_time_ms + dt_ms);
            }
        } else if (m_clip == Clip::Sit) {
            // Sit-down plays once; hold the last frame while resting (no AniLoop).
            const float dur = std::max(1.0f, m_sit.duration_ms());
            if (m_anim_time_ms < dur) {
                m_anim_time_ms += dt_ms;
                if (m_anim_time_ms > dur) {
                    m_anim_time_ms = dur;
                }
            }
        } else if (m_clip == Clip::SitConsume) {
            // Repeat from (*LOOP) when present; otherwise hold the last frame.
            // Restarting from t=0 every clip length looked like sitting down each pulse.
            const float dur = std::max(1.0f, m_sit_consume.duration_ms());
            const float loop_start = m_sit_consume.loop_start_ms;
            const bool can_loop = loop_start > 0.0f && loop_start < dur;
            if (m_anim_time_ms < dur || can_loop) {
                m_anim_time_ms += dt_ms;
            }
            if (m_anim_time_ms >= dur) {
                if (can_loop) {
                    const float span = dur - loop_start;
                    m_anim_time_ms = loop_start + std::fmod(m_anim_time_ms - loop_start, span);
                    if (m_anim_time_ms < loop_start) {
                        m_anim_time_ms += span;
                    }
                } else {
                    m_anim_time_ms = dur;
                }
            }
        } else if (m_clip == Clip::Stand) {
            const float dur = std::max(1.0f, m_stand.duration_ms());
            m_anim_time_ms += dt_ms;
            if (m_anim_time_ms >= dur) {
                m_clip = Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
            }
        } else if (m_clip == Clip::Attack) {
            // Leave combat pose as soon as we start walking (auto-attack chase / out of
            // range), unless a locked behead swing must finish.
            if (moving && !m_lock_attack_pose) {
                m_clip = Clip::Walk;
                m_anim_time_ms = 0.0f;
                m_attack_play_once = false;
                m_last_skinned_time_ms = -1000.0f;
                disarm_arrow(m_await_arrow, m_arrow_ready);
            } else {
                const terrain::GbAnimClip& atk =
                    (m_attack_variant == 2 && m_has_attack3)
                        ? m_attack3
                        : ((m_attack_variant == 1 && m_has_attack2) ? m_attack2 : m_attack);
                const float dur = std::max(1.0f, atk.duration_ms());
                const float prev_ms = m_clip_effect_prev_ms;
                // Present the final attack pose once before returning to idle. The next
                // swing can then replace it without an idle frame between attacks.
                const bool finished = m_anim_time_ms >= dur;
                m_anim_time_ms = std::min(dur, m_anim_time_ms + dt_ms);
                poll_arrow_release(
                    m_await_arrow, m_arrow_ready, m_anim_time_ms, m_pending_arrow_ms);
                poll_clip_effect_cues(atk, prev_ms, m_anim_time_ms, m_pending_clip_effects);
                m_clip_effect_prev_ms = m_anim_time_ms;
                if (finished) {
                    m_clip = Clip::Idle;
                    m_anim_time_ms = 0.0f;
                    m_lock_attack_pose = false;
                    m_attack_play_once = false;
                    m_last_skinned_time_ms = -1000.0f;
                    if (audio) {
                        m_sounds.stop(*audio);
                    }
                }
            }
        } else if (m_clip == Clip::Skill) {
            // compatibility behavior: (*LOOP) PlayAniRepeat; otherwise
            // ModelChange(0) idle. play_once covers that idle path (behead, buff /
            // heal fire). Combat mage clips without play_once still hold the last
            // frame so a projectile landing after the clip does not look cancelled.
            if (moving && !m_lock_skill_pose) {
                m_clip = Clip::Walk;
                m_anim_time_ms = 0.0f;
                m_skill_looping = false;
                m_skill_use_charge = false;
                m_skill_use_behead = false;
                m_skill_motion_sub = 3;
                m_lock_attack_pose = false;
                m_attack_play_once = false;
                m_last_skinned_time_ms = -1000.0f;
                disarm_arrow(m_await_arrow, m_arrow_ready);
            } else {
                const terrain::GbAnimClip& skill = current_skill_clip();
                const float dur = std::max(1.0f, skill.duration_ms());
                const float loop_start = skill.loop_start_ms;
                const bool can_loop = loop_start > 0.0f && loop_start < dur;
                const float prev_ms = m_clip_effect_prev_ms;
                m_anim_time_ms += dt_ms;
                poll_arrow_release(
                    m_await_arrow, m_arrow_ready, m_anim_time_ms, m_pending_arrow_ms);
                if (m_anim_time_ms >= dur && m_attack_play_once) {
                    collect_looping_clip_effect_cues(
                        skill, prev_ms, m_anim_time_ms, m_pending_clip_effects);
                    m_clip = Clip::Idle;
                    m_anim_time_ms = 0.0f;
                    m_skill_looping = false;
                    m_skill_use_behead = false;
                    m_skill_use_charge = false;
                    m_lock_skill_pose = false;
                    m_lock_attack_pose = false;
                    m_attack_play_once = false;
                    m_last_skinned_time_ms = -1000.0f;
                    if (audio) {
                        m_sounds.stop(*audio);
                    }
                } else {
                    if (m_anim_time_ms >= dur) {
                        if (can_loop) {
                            m_skill_looping = true;
                            const float span = dur - loop_start;
                            m_anim_time_ms =
                                loop_start + std::fmod(m_anim_time_ms - loop_start, span);
                            if (m_anim_time_ms < loop_start) {
                                m_anim_time_ms += span;
                            }
                        } else {
                            m_anim_time_ms = dur;
                        }
                    }
                    collect_looping_clip_effect_cues(
                        skill, prev_ms, m_anim_time_ms, m_pending_clip_effects);
                    m_clip_effect_prev_ms = m_anim_time_ms;
                }
            }
        } else if (m_clip == Clip::Emote) {
            m_anim_time_ms += dt_ms;
            if (moving ||
                m_anim_time_ms >= std::max(1.0f, m_emotes.at(m_emote_motion).duration_ms())) {
                m_clip = moving ? Clip::Walk : Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
            }
        } else if (m_clip == Clip::Evade) {
            const float dur = std::max(1.0f, m_evade.duration_ms());
            m_anim_time_ms += dt_ms;
            if (m_anim_time_ms >= dur) {
                m_clip = Clip::Idle;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
            }
        } else if (resting) {
            m_anim_time_ms += dt_ms;
        } else {
            Clip want = moving ? Clip::Walk : Clip::Idle;
            if (want != m_clip) {
                m_clip = want;
                m_anim_time_ms = 0.0f;
                m_last_skinned_time_ms = -1000.0f;
            }
            const float anim_scale = (moving && hasting) ? kRunAnimSpeed : 1.0f;
            m_anim_time_ms += dt_ms * anim_scale;
        }
        if (m_await_arrow && m_clip != Clip::Attack && m_clip != Clip::Skill) {
            disarm_arrow(m_await_arrow, m_arrow_ready);
        }
        // Skin/upload every render frame so locomotion matches 60 FPS presentation.
        m_last_skinned_time_ms = m_anim_time_ms;

        const terrain::GbAnimClip* cur = &m_idle;
        if (m_clip == Clip::Walk) {
            cur = &m_walk;
        } else if (m_clip == Clip::Die) {
            cur = &m_die;
        } else if (m_clip == Clip::Attack) {
            cur = (m_attack_variant == 2 && m_has_attack3)
                      ? &m_attack3
                      : ((m_attack_variant == 1 && m_has_attack2) ? &m_attack2 : &m_attack);
        } else if (m_clip == Clip::Skill) {
            cur = &current_skill_clip();
        } else if (m_clip == Clip::OpenBox) {
            cur = &m_open_box;
        } else if (m_clip == Clip::Fishing) {
            cur = &m_fishing;
        } else if (m_clip == Clip::Sit) {
            cur = &m_sit;
        } else if (m_clip == Clip::SitConsume) {
            cur = &m_sit_consume;
        } else if (m_clip == Clip::Stand) {
            cur = &m_stand;
        } else if (m_clip == Clip::Evade) {
            cur = &m_evade;
        } else if (m_clip == Clip::Riding) {
            cur = &m_riding;
        } else if (m_clip == Clip::Emote) {
            cur = &m_emotes.at(m_emote_motion);
        }

        std::vector<std::array<float, 16>> skin;
        std::vector<std::array<float, 16>> bone_world;
        const bool skill_loops = m_clip == Clip::Skill && cur->loop_start_ms > 0.0f &&
                                 cur->loop_start_ms < std::max(1.0f, cur->duration_ms());
        const bool open_box_loops = m_clip == Clip::OpenBox && !m_harvest_once &&
                                    cur->loop_start_ms > 0.0f &&
                                    cur->loop_start_ms < std::max(1.0f, cur->duration_ms());
        const bool loop = m_clip == Clip::Idle || m_clip == Clip::Walk || open_box_loops ||
                          m_clip == Clip::SitConsume || m_clip == Clip::Riding || skill_loops;
        if (audio) {
            if (m_restart_sounds || m_sound_clip != m_clip) {
                m_restart_sounds = false;
                m_sounds.reset(audio);
                m_sound_clip = m_clip;
                if (m_clip == Clip::Attack) {
                    m_melee_hit_prev_ms = -1.0f;
                }
            }
            m_sounds.poll(*audio, *cur, m_anim_time_ms, loop, wx, wy, wz);

            // Melee hit SFX is on (*EFFECT_MODEL effect_attack*) after the whoosh SOUND.
            if (m_clip == Clip::Attack) {
                const float prev = m_melee_hit_prev_ms;
                const float curr = m_anim_time_ms;
                m_melee_hit_prev_ms = curr;
                if (!m_melee_hit_model_passed) {
                    const float lower = prev < 0.0f ? -1.0f : prev;
                    const std::size_t n = std::min(cur->key_times.size(), cur->key_options.size());
                    for (std::size_t i = 0; i < n; ++i) {
                        const float t = static_cast<float>(cur->key_times[i]);
                        if (!(lower < t && t <= curr)) {
                            continue;
                        }
                        if (is_melee_hit_effect_model(cur->key_options[i])) {
                            m_melee_hit_model_passed = true;
                            break;
                        }
                    }
                }
                if (m_melee_hit_confirmed && m_melee_hit_model_passed && !m_melee_hit_sfx_played) {
                    m_melee_hit_sfx_played = true;
                    (void) audio->play("efc006", wx, wy, wz);
                }
            }
        }
        const float sample_t =
            (m_clip == Clip::Attack || m_clip == Clip::Skill || m_clip == Clip::Die ||
             m_clip == Clip::Stand || m_clip == Clip::Evade || m_clip == Clip::Sit ||
             m_clip == Clip::SitConsume)
                ? std::max(0.0f, m_anim_time_ms)
                : m_anim_time_ms;
        if (!terrain::eval_bone_and_skin_matrices_at_time(
                m_bones, *cur, sample_t, loop, bone_world, skin)) {
            log::warn("AnimatedActor: pose evaluation failed class=" +
                      std::string(LocalPlayer::class_name(m_clazz)) +
                      " bones=" + std::to_string(m_bones.size()) +
                      " clip_bones=" + std::to_string(cur->bone_count) +
                      " keyframes=" + std::to_string(cur->keyframe_count));
            return;
        }
        // KalOnline attaches weapons with s_vMatrix (bone world), not s_vMatrixWorld
        // (skin).
        if (m_weapon_bone >= 0 && m_weapon_bone < static_cast<int>(bone_world.size())) {
            m_last_weapon_bone_mat = bone_world[static_cast<std::size_t>(m_weapon_bone)];
            m_have_weapon_bone_mat = true;
        }
        if (m_shield_bone > 0 && m_shield_bone < static_cast<int>(bone_world.size())) {
            m_last_shield_bone_mat = bone_world[static_cast<std::size_t>(m_shield_bone)];
            m_have_shield_bone_mat = true;
        }
        if (m_effect_bone >= 0 && m_effect_bone < static_cast<int>(bone_world.size())) {
            m_last_effect_bone_mat = bone_world[static_cast<std::size_t>(m_effect_bone)];
            m_have_effect_bone_mat = true;
        } else if (m_have_weapon_bone_mat) {
            // Fallback: weapon attach bone if effect socket missing.
            m_last_effect_bone_mat = m_last_weapon_bone_mat;
            m_have_effect_bone_mat = true;
        }
        m_last_bone_mats = std::move(bone_world);
        m_have_bone_mats = !m_last_bone_mats.empty();
        if (!skin_geometry) {
            return;
        }

        for (std::size_t i = 0; i < m_meshes.size() && i < m_parts.size(); ++i) {
            // Only specular materials consume normals in the player renderer.
            terrain::skin_mesh(m_meshes[i], skin, m_meshes[i].specular_power > 0.0f);
            std::vector<engine::VertexP3T2> verts;
            verts.reserve(m_meshes[i].pos_uv.size() / 5);
            for (std::size_t v = 0; v + 4 < m_meshes[i].pos_uv.size(); v += 5) {
                verts.push_back({m_meshes[i].pos_uv[v],
                                 m_meshes[i].pos_uv[v + 1],
                                 m_meshes[i].pos_uv[v + 2],
                                 m_meshes[i].pos_uv[v + 3],
                                 m_meshes[i].pos_uv[v + 4]});
            }
            if (m_meshes[i].specular_power > 0.0f) {
                engine.set_mesh_specular(
                    m_parts[i].mesh(), m_meshes[i].normals, m_meshes[i].specular_power);
            }
            if (!engine.update_mesh(m_parts[i].mesh(), verts.data(), verts.size())) {
                log::warn("AnimatedActor: mesh upload failed part=" + std::to_string(i));
            }
        }
    }

    bool AnimatedActor::effect_tip_world(const engine::Mat4& model,
                                         float& ox,
                                         float& oy,
                                         float& oz) const {
        if (!m_have_effect_bone_mat) {
            return false;
        }
        engine::Mat4 bone_mat;
        bone_mat.m = m_last_effect_bone_mat;
        const engine::Mat4 w = engine::Mat4::multiply(model, bone_mat);
        // Column-major translation (same layout as Mat4::translation writes).
        ox = w.m[12];
        oy = w.m[13];
        oz = w.m[14];
        return true;
    }

    bool AnimatedActor::bone_world(
        const engine::Mat4& model, int skeleton_bone, float& ox, float& oy, float& oz) const {
        if (!m_have_bone_mats || skeleton_bone < 0 ||
            skeleton_bone >= static_cast<int>(m_last_bone_mats.size())) {
            return false;
        }
        engine::Mat4 bone_mat;
        bone_mat.m = m_last_bone_mats[static_cast<std::size_t>(skeleton_bone)];
        const engine::Mat4 w = engine::Mat4::multiply(model, bone_mat);
        ox = w.m[12];
        oy = w.m[13];
        oz = w.m[14];
        return true;
    }

    bool AnimatedActor::weapon_bone_world(const engine::Mat4& model,
                                          float& ox,
                                          float& oy,
                                          float& oz) const {
        engine::Mat4 world;
        if (!weapon_world_matrix(model, world)) {
            return false;
        }
        ox = world.m[12];
        oy = world.m[13];
        oz = world.m[14];
        return true;
    }

    bool AnimatedActor::weapon_world_matrix(const engine::Mat4& model, engine::Mat4& out) const {
        if (!m_have_weapon_bone_mat) {
            return false;
        }
        engine::Mat4 bone_mat;
        bone_mat.m = m_last_weapon_bone_mat;
        out = engine::Mat4::multiply(model, bone_mat);
        return true;
    }

    bool AnimatedActor::weapon_effect_socket_world(const engine::Mat4& model,
                                                   engine::Mat4& out) const {
        engine::Mat4 weapon_world;
        if (!weapon_world_matrix(model, weapon_world)) {
            return false;
        }
        for (const auto& mesh : m_weapon_bind) {
            if (!is_effect_socket_name(mesh.name) || mesh.pos_uv.size() < 5) {
                continue;
            }
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            std::size_t count = 0;
            for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                x += mesh.pos_uv[i];
                y += mesh.pos_uv[i + 1];
                z += mesh.pos_uv[i + 2];
                ++count;
            }
            if (count == 0) {
                continue;
            }
            const float inv_count = 1.0f / static_cast<float>(count);
            out = engine::Mat4::multiply(
                weapon_world,
                engine::Mat4::translation(x * inv_count, y * inv_count, z * inv_count));
            return true;
        }
        return false;
    }

    bool AnimatedActor::bone_world_matrix(const engine::Mat4& model,
                                          int skeleton_bone,
                                          engine::Mat4& out) const {
        if (!m_have_bone_mats || skeleton_bone < 0 ||
            skeleton_bone >= static_cast<int>(m_last_bone_mats.size())) {
            return false;
        }
        engine::Mat4 bone_mat;
        bone_mat.m = m_last_bone_mats[static_cast<std::size_t>(skeleton_bone)];
        out = engine::Mat4::multiply(model, bone_mat);
        return true;
    }

    bool AnimatedActor::projectile_origin_world(const engine::Mat4& model,
                                                float& ox,
                                                float& oy,
                                                float& oz) const {
        if (!effect_tip_world(model, ox, oy, oz)) {
            return false;
        }
        // compatibility behavior initialization: runtime state += runtime state (world, from
        // (*EFFECT_ARROW) OFFSET).
        ox += m_pending_arrow_ox;
        oy += m_pending_arrow_oy;
        oz += m_pending_arrow_oz;
        return true;
    }

    bool AnimatedActor::take_arrow_release() {
        if (!m_arrow_ready) {
            return false;
        }
        m_arrow_ready = false;
        return true;
    }

    std::vector<AnimatedActor::ClipEffectCue> AnimatedActor::take_clip_effects() {
        std::vector<ClipEffectCue> out;
        out.swap(m_pending_clip_effects);
        return out;
    }

    void AnimatedActor::reset_clip_effect_poll() {
        m_restart_sounds = true;
        m_effect_clip = m_clip;
        m_clip_effect_prev_ms = -1.0f;
        m_pending_clip_effects.clear();
    }

    bool AnimatedActor::skill_playing() const {
        return m_clip == Clip::Skill;
    }

    bool AnimatedActor::attack_playing() const {
        return m_clip == Clip::Attack;
    }

    void AnimatedActor::draw(engine::IEngine& engine,
                             const engine::Mat4& model,
                             const engine::Mat4& view,
                             bool want_alpha,
                             float opacity) const {
        const bool fading = opacity < 0.999f;
        if (fading && !want_alpha) {
            return;
        }
        auto draw_part =
            [&](const AnimatedPart& part, const engine::Mat4& part_model, bool two_sided) {
                const bool use_blend = fading || part.alpha_blend();
                if (use_blend != want_alpha) {
                    return;
                }
                engine.draw_mesh_textured(part.mesh(),
                                          part.texture(),
                                          part_model,
                                          use_blend,
                                          two_sided || part.two_sided(),
                                          part.alpha_test() && !fading,
                                          false,
                                          0.0f,
                                          0.0f,
                                          1.0f,
                                          1.0f,
                                          1.0f,
                                          opacity);
            };
        const auto draw_weapon_part = [&](const AnimatedPart& part,
                                          const terrain::GbMeshCpu& mesh,
                                          const engine::Mat4& part_model,
                                          bool two_sided) {
            if (want_alpha && (fading || part.alpha_blend()) && (two_sided || part.two_sided()) &&
                mesh.specular_power <= 0.0f) {
                std::vector<engine::VertexP3T2> vertices;
                vertices.reserve(mesh.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    vertices.push_back({mesh.pos_uv[i],
                                        mesh.pos_uv[i + 1],
                                        mesh.pos_uv[i + 2],
                                        mesh.pos_uv[i + 3],
                                        mesh.pos_uv[i + 4]});
                }
                const auto batch = engine::build_transparent_mesh_batch(
                    vertices, mesh.indices, part_model, view, opacity);
                engine.draw_sprite_batch(batch, part.texture(), true, false);
            } else {
                draw_part(part, part_model, two_sided);
            }
        };
        for (const auto& part : m_parts) {
            draw_part(part, model, false);
        }
        if (m_clip == Clip::Emote) {
            return;
        }
        if (m_clip == Clip::Fishing) {
            if (m_have_weapon_bone_mat) {
                engine::Mat4 bone_mat;
                bone_mat.m = m_last_weapon_bone_mat;
                const engine::Mat4 rod_model = engine::Mat4::multiply(model, bone_mat);
                const bool casting =
                    m_fishing_reeling || m_anim_time_ms < std::max(1.0f, m_fishing.duration_ms());
                const bool use_hold_rod = !casting || m_fishing_cast_parts.empty();
                const auto& rod_parts = use_hold_rod ? m_fishing_hold_parts : m_fishing_cast_parts;
                const auto& rod_bind = use_hold_rod ? m_fishing_hold_bind : m_fishing_cast_bind;
                for (std::size_t i = 0; i < rod_parts.size(); ++i) {
                    if (i < rod_bind.size() && is_effect_socket_name(rod_bind[i].name)) {
                        continue;
                    }
                    draw_part(rod_parts[i], rod_model, false);
                }
            }
            return;
        }
        if (!m_weapon_parts.empty() && m_have_weapon_bone_mat) {
            engine::Mat4 bone_mat;
            bone_mat.m = m_last_weapon_bone_mat;
            const engine::Mat4 wmodel = engine::Mat4::multiply(model, bone_mat);
            for (std::size_t i = 0; i < m_weapon_parts.size(); ++i) {
                if (i < m_weapon_bind.size() && is_effect_socket_name(m_weapon_bind[i].name)) {
                    continue;
                }
                if (i < m_weapon_bind.size()) {
                    draw_weapon_part(m_weapon_parts[i], m_weapon_bind[i], wmodel, false);
                }
            }
        }
        if (m_shield_parts.empty() || !m_have_shield_bone_mat) {
            return;
        }
        engine::Mat4 shield_bone;
        shield_bone.m = m_last_shield_bone_mat;
        engine::Mat4 smodel = engine::Mat4::multiply(model, shield_bone);
        const float shield_dy = EquipmentPaths::shield_local_offset_y(m_clazz);
        if (shield_dy != 0.0f) {
            smodel =
                engine::Mat4::multiply(smodel, engine::Mat4::translation(0.0f, shield_dy, 0.0f));
            engine.set_polygon_offset(-4.0f, -64.0f);
            engine.set_depth_less_equal(true);
        }
        const bool shield_two_sided = shield_dy != 0.0f;
        for (std::size_t i = 0; i < m_shield_parts.size(); ++i) {
            if (i < m_shield_bind.size() && is_effect_socket_name(m_shield_bind[i].name)) {
                continue;
            }
            if (i < m_shield_bind.size()) {
                draw_weapon_part(m_shield_parts[i], m_shield_bind[i], smodel, shield_two_sided);
            }
        }
        if (shield_dy != 0.0f) {
            engine.set_depth_less_equal(false);
            engine.set_polygon_offset(0.0f, 0.0f);
        }
    }

    void AnimatedActor::draw_shadow(engine::IEngine& engine,
                                    const engine::Mat4& model,
                                    const engine::ShadowReceiver& receiver,
                                    float ground_y,
                                    float opacity) const {
        const auto draw_part = [&](const AnimatedPart& part, const engine::Mat4& part_model) {
            engine.draw_mesh_shadow(part.mesh(),
                                    part.texture(),
                                    part_model,
                                    receiver,
                                    ground_y,
                                    part.alpha_test() || part.alpha_blend(),
                                    opacity);
        };
        for (const auto& part : m_parts) {
            draw_part(part, model);
        }
        if (m_clip == Clip::Emote) {
            return;
        }
        if (m_clip == Clip::Fishing) {
            if (m_have_weapon_bone_mat) {
                engine::Mat4 bone_mat;
                bone_mat.m = m_last_weapon_bone_mat;
                const engine::Mat4 rod_model = engine::Mat4::multiply(model, bone_mat);
                const bool casting =
                    m_fishing_reeling || m_anim_time_ms < std::max(1.0f, m_fishing.duration_ms());
                const auto& rod_parts = (!casting || m_fishing_cast_parts.empty())
                                            ? m_fishing_hold_parts
                                            : m_fishing_cast_parts;
                for (const auto& part : rod_parts) {
                    draw_part(part, rod_model);
                }
            }
            return;
        }
        if (!m_weapon_parts.empty() && m_have_weapon_bone_mat) {
            engine::Mat4 bone_mat;
            bone_mat.m = m_last_weapon_bone_mat;
            const engine::Mat4 weapon_model = engine::Mat4::multiply(model, bone_mat);
            for (const auto& part : m_weapon_parts) {
                draw_part(part, weapon_model);
            }
        }
        if (!m_shield_parts.empty() && m_have_shield_bone_mat) {
            engine::Mat4 bone_mat;
            bone_mat.m = m_last_shield_bone_mat;
            engine::Mat4 shield_model = engine::Mat4::multiply(model, bone_mat);
            const float offset_y = EquipmentPaths::shield_local_offset_y(m_clazz);
            if (offset_y != 0.0f) {
                shield_model = engine::Mat4::multiply(
                    shield_model, engine::Mat4::translation(0.0f, offset_y, 0.0f));
            }
            for (const auto& part : m_shield_parts) {
                draw_part(part, shield_model);
            }
        }
    }

}  // namespace openkal::client
