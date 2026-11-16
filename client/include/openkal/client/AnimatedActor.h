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

#include "openkal/client/AnimSound.h"
#include "openkal/client/Equipment.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/Player.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GbModel.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** One uploaded sub-mesh of a character, with its texture and render flags. */
    class AnimatedPart {
      public:
        /** @return GPU mesh handle. */
        [[nodiscard]] engine::MeshHandle mesh() const {
            return m_mesh;
        }

        /**
         * Sets the GPU mesh handle.
         * @param v Mesh handle.
         */
        void set_mesh(engine::MeshHandle v) {
            m_mesh = v;
        }

        /** @return GPU texture handle. */
        [[nodiscard]] engine::TextureHandle texture() const {
            return m_texture;
        }

        /**
         * Sets the GPU texture handle.
         * @param v Texture handle.
         */
        void set_texture(engine::TextureHandle v) {
            m_texture = v;
        }

        /** @return True when the part renders with alpha blending. */
        [[nodiscard]] bool alpha_blend() const {
            return m_alpha_blend;
        }

        /**
         * Sets the alpha blending flag.
         * @param v True to blend.
         */
        void set_alpha_blend(bool v) {
            m_alpha_blend = v;
        }

        /** @return True when the part renders with alpha testing. */
        [[nodiscard]] bool alpha_test() const {
            return m_alpha_test;
        }

        /**
         * Sets the alpha testing flag.
         * @param v True to alpha test.
         */
        void set_alpha_test(bool v) {
            m_alpha_test = v;
        }

        /** @return True when the part renders with backface culling disabled. */
        [[nodiscard]] bool two_sided() const {
            return m_two_sided;
        }

        /**
         * Sets the two-sided flag.
         * @param v True to disable backface culling.
         */
        void set_two_sided(bool v) {
            m_two_sided = v;
        }

      private:
        /** GPU mesh handle. */
        engine::MeshHandle m_mesh;

        /** GPU texture handle. */
        engine::TextureHandle m_texture;

        /** Render with alpha blending. */
        bool m_alpha_blend = false;

        /** Render with alpha testing. */
        bool m_alpha_test = false;

        /** Render without backface culling. */
        bool m_two_sided = false;
    };

    /**
     * Skinned player character with its motion clips.
     *
     * Player action indices (GameUser::ModelChange): 0 idle, 1 move, 2 die, 3
     * evade, 4 damage, 5 rest, 7 skill, 8 attack, 10 emote. Haste (skill 0 /
     * compatibility behavior) keeps action 1 (walk) and scales FrameMove by RUN_SPEED — T*_2
     * is death, not run (unlike monsters where action 2 = run). Common riding
     * uses the dedicated T*_0_A_04 pose.
     */
    class AnimatedActor {
      public:
        /** Motion clip currently driving the skeleton. */
        enum class Clip : int {
            /** T*_0 — idle, looping. */
            Idle = 0,

            /** T*_1 — walk/run, looping. */
            Walk = 1,

            /** T*_2 — death, played once and held. */
            Die = 2,

            /** T*_8 — physical swing. */
            Attack = 3,

            /** T*_7 — magic / GO_SKILL cast. */
            Skill = 4,

            /** T*_9_A_04 — treasure-box open kneel (ModelChange(9, 4) / compatibility behavior).
             */
            OpenBox = 5,

            /** Fishing pose; legacy assets use T*_6_A_01, later assets T*_12_A_01. */
            Fishing = 6,

            /** T*_5_A_01 — sit down, then loop (ModelChange(5, 1) / AT_REST). */
            Sit = 7,

            /** T*_5_A_02 — stand up once, then idle (ModelChange(5, 2)). */
            Stand = 8,

            /** T*_3_A_01 — evade / dodge (ModelChange(3, 1) when an attack misses). */
            Evade = 9,

            /** T*_5_A_03 — sit consume (ModelChange(5, 3) while related compatibility states). */
            SitConsume = 10,

            /** T*_0_A_04 — mounted rider pose, looping. */
            Riding = 11,

            /** T*_10_A_01..20 — one-shot character emote. */
            Emote = 12,
        };

        /** Starts a human emote, loading its motion on first use.
         * @param motion One-based action-10 motion (1..20).
         * @return True when playback starts; false for missing motions or incompatible poses.
         */
        [[nodiscard]] bool trigger_emote(int motion);

        /** Sets the natural appearance used on subsequent mesh loads.
         * @param face One-based face mesh index.
         * @param hair One-based hairstyle mesh index.
         */
        void set_appearance(int face, int hair);

        /** @return True when no character mesh is loaded. */
        [[nodiscard]] bool empty() const {
            return m_parts.empty();
        }

        /** @return True when a weapon mesh is attached. */
        [[nodiscard]] bool has_weapon_mesh() const {
            return !m_weapon_parts.empty();
        }

        /** @return True when a shield mesh is attached. */
        [[nodiscard]] bool has_shield_mesh() const {
            return !m_shield_parts.empty();
        }

        /** @return Class this actor was loaded for. */
        [[nodiscard]] Class clazz() const {
            return m_clazz;
        }

        /** @return Clip currently playing. */
        [[nodiscard]] Clip clip() const {
            return m_clip;
        }

        /** @return Client data root the meshes were loaded from. */
        [[nodiscard]] const std::string& data_root() const {
            return m_data_root;
        }

        /**
         * Releases every GPU mesh and clears the compatibility behavior-side state.
         * @param engine Engine owning the mesh handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Loads skeleton, motion clips, clothes and (optionally) worn weapon/shield.
         * @param engine Engine used to create meshes and textures.
         * @param data_root_in Client data root.
         * @param clazz_in Player class to load.
         * @param tex_cache Shared texture cache, keyed by "<gb path>|<texture name>".
         * @param wear Optional worn equipment; nullptr loads the default look.
         * @param catalog Item catalog, required when wear is given.
         * @return True when at least one character part was uploaded.
         */
        [[nodiscard]] bool load(engine::IEngine& engine,
                                const std::string& data_root_in,
                                Class clazz_in,
                                std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                                const Wear* wear = nullptr,
                                const ItemDb* catalog = nullptr);

        /**
         * Reloads motions/meshes for a Shinsu metamorphose.
         * Motions are `TGB{type:02d}_*` (baby/young) or `TGD{type:02d}_*` (adult);
         * clothes are `GB{type:02d}_B*` / `GD{type:02d}_B*`.
         * @param engine Engine used to create meshes.
         * @param tex_cache Shared texture cache.
         * @param type Race, or -1 to revert to human (METAMO_ORIGIN).
         * @param level Stage 1–3 (ignored when type < 0).
         * @param scale Model scale from shinsuinfo.
         * @param wear Worn equipment for the human fallback / revert.
         * @param catalog Item catalog.
         * @return True when at least one part was uploaded.
         */
        [[nodiscard]] bool
        apply_metamo(engine::IEngine& engine,
                     std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                     int type,
                     int level,
                     float scale,
                     const Wear* wear,
                     const ItemDb* catalog);

        /** @return Metamo race, or -1 for human. */
        [[nodiscard]] int metamo_type() const {
            return m_metamo_type;
        }

        /** @return Metamo stage, or 0. */
        [[nodiscard]] int metamo_level() const {
            return m_metamo_level;
        }

        /** @return shinsuinfo scale (1 = human). */
        [[nodiscard]] float metamo_scale() const {
            return m_metamo_scale;
        }

        /**
         * Reloads the character with a new worn set, keeping data root and class.
         * @param engine Engine used to create meshes and textures.
         * @param tex_cache Shared texture cache.
         * @param wear Worn equipment.
         * @param catalog Item catalog.
         * @return True when at least one character part was uploaded.
         */
        [[nodiscard]] bool
        apply_wear(engine::IEngine& engine,
                   std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                   const Wear& wear,
                   const ItemDb& catalog);

        /**
         * Starts a physical swing, alternating A_01 / A_02 when both clips exist.
         * Interrupts a GO_SKILL pose (`ModelChange(GO_ATTACK)`).
         */
        void trigger_attack();

        /**
         * Marks the current melee swing as a confirmed hit (S2C_ATTACK kAtkHit).
         *
         * Plays efc006 when the attack clip reaches `(*EFFECT_MODEL effect_attack*)`,
         * or immediately if that keyframe already passed (late packet).
         */
        void note_melee_hit();

        /**
         * Starts compatibility behavior presentation: ModelChange(GO_SKILL, runtime state[1]=1) →
         * T*_7_A_01. Plays once (no loop / no reverse); locked until finished or cancel_attack().
         */
        void trigger_behead_swing();

        /** Aborts a swing so walk/chase does not slide under an attack pose. */
        void cancel_attack();

        /** @return True while a locked behead swing is still playing. */
        [[nodiscard]] bool attack_locked() const {
            return m_lock_attack_pose;
        }

        /** @return True once the locked behead swing has reached half its duration. */
        [[nodiscard]] bool behead_swing_cancelable() const;

        /** Starts a magic / GO_SKILL cast, falling back to the swing when no cast
         * clip exists. Restarts when already casting (same-action ModelChange). */
        void trigger_skill();

        /**
         * Starts GO_SKILL fire (`ModelSkill` gubun 1 → `ModelChange(7, sub)`).
         * Sub 2 is Lightning Slash (`T*_7_A_02`); sub 3 is Mockery (`T*_7_A_03`).
         * @param sub Motion sub-index from jobsystem motion[1]; 0 uses the default
         * fire clip.
         * @param play_once True to return to idle after the clip unless `(*LOOP)`
         * repeats it. Melee blows and buff/heal casts use this to avoid holding
         * the last frame.
         */
        void trigger_skill_motion(int sub, bool play_once = false);

        /** Starts the archer PRESKILL draw (`ModelSkill` gubun 0, motion 2 / A_02)
         * without arming `(*EFFECT_ARROW)`. Holds the last frame until end_skill(). */
        void trigger_skill_charge();

        /** Leaves a GO_SKILL pose and returns to idle (fire / cancel). */
        void end_skill();

        /** Starts the evade clip (ModelChange(3, 1)) when a monster attack misses. */
        void trigger_evade();

        /** Starts the death clip (ModelChange(2) / AT_DIE); plays once and holds the
         * last frame. */
        void trigger_die();

        /** Leaves the death pose and returns to idle. */
        void clear_die();

        /** Starts the treasure-box open kneel (ModelChange(9, 4)); repeats an authored
         * loop or holds the last frame until clear_open_box(). */
        void trigger_open_box();

        /** Plays one complete gather motion and returns to standing. */
        void trigger_harvest();

        /** @return True during a single gathering motion. */
        bool harvesting() const {
            return m_harvest_once;
        }

        /** Leaves the open-box pose and returns to idle. */
        void clear_open_box();

        /** @return True while the open-box kneel is playing. */
        [[nodiscard]] bool opening_box() const {
            return m_clip == Clip::OpenBox;
        }

        /** Starts the fishing pose until clear_fishing(). */
        void trigger_fishing();

        /** Reels in the held line, then casts it again for the next fishing cycle. */
        void restart_fishing_cycle();

        /** Leaves the fishing pose and returns to idle. */
        void clear_fishing();

        /** @return True while the fishing pose is playing. */
        [[nodiscard]] bool fishing() const {
            return m_clip == Clip::Fishing;
        }

        /** Starts sit-down (ModelChange(5, 1)); plays once and holds the last frame. */
        void trigger_sit();

        /** Starts sit-consume (ModelChange(5, 3)). Plays once and holds, or loops
         * from (*LOOP). Already sitting skips sit-down. */
        void trigger_sit_consume();

        /** Starts stand-up (ModelChange(5, 2)); plays once then idle. */
        void trigger_stand();

        /**
         * Leaves sit/stand immediately without ModelChange(5, 2).
         * Used when movement or attack already replaced the rest action, so a
         * stand-up animation must not interrupt the new action.
         */
        void clear_rest_pose();

        /** @return True while sitting, consuming, or standing up. */
        [[nodiscard]] bool sitting() const {
            return m_clip == Clip::Sit || m_clip == Clip::SitConsume || m_clip == Clip::Stand;
        }

        /**
         * Advances the current clip and re-skins the character meshes.
         * @param engine Engine used to upload skinned vertices.
         * @param dt Frame delta in seconds.
         * @param moving True while the character is walking.
         * @param hasting True while compatibility behavior is active (walk clip at RUN_SPEED).
         * @param resting True while compatibility behavior is active (holds sit; does not overwrite
         * with walk).
         * @param consuming True while related compatibility states is set (T*_5_A_03).
         * @param audio SFX mixer for `(*SOUND)` keyframes; nullptr skips playback.
         * @param wx World X used as the sound origin.
         * @param wy World Y used as the sound origin.
         * @param wz World Z used as the sound origin.
         * @param fishing True while the authoritative compatibility behavior bit is active.
         * @param riding True while mounted on a common riding animal.
         * @param skin_geometry False to advance clips, sounds and bones without skinning hidden
         * meshes.
         */
        void update(engine::IEngine& engine,
                    float dt,
                    bool moving,
                    bool hasting,
                    bool resting = false,
                    bool consuming = false,
                    Audio* audio = nullptr,
                    float wx = 0.0f,
                    float wy = 0.0f,
                    float wz = 0.0f,
                    bool fishing = false,
                    bool riding = false,
                    bool skin_geometry = true);

        /**
         * Computes the world position of EFFECTBONE_EFFECT (cast origin / wand tip).
         * @param model Character model matrix.
         * @param ox Receives world X.
         * @param oy Receives world Y.
         * @param oz Receives world Z.
         * @return True when a pose has been evaluated and the socket is known.
         */
        [[nodiscard]] bool
        effect_tip_world(const engine::Mat4& model, float& ox, float& oy, float& oz) const;

        /**
         * Computes the world position of one skeleton bone from the last pose.
         * @param model Character model matrix.
         * @param skeleton_bone Skeleton bone index.
         * @param ox Receives world X.
         * @param oy Receives world Y.
         * @param oz Receives world Z.
         * @return True when the last pose contains that bone.
         */
        [[nodiscard]] bool bone_world(
            const engine::Mat4& model, int skeleton_bone, float& ox, float& oy, float& oz) const;

        /**
         * Computes the world position of the weapon bind bone (mix particle origin).
         * @param model Character model matrix.
         * @param ox Receives world X.
         * @param oy Receives world Y.
         * @param oz Receives world Z.
         * @return True when a pose has been evaluated and the socket is known.
         */
        [[nodiscard]] bool
        weapon_bone_world(const engine::Mat4& model, float& ox, float& oy, float& oz) const;

        /**
         * Computes the world matrix of the weapon bind bone.
         * @param model Character model matrix.
         * @param out Receives the column-major world matrix.
         * @return True when a pose has been evaluated and the socket is known.
         */
        [[nodiscard]] bool weapon_world_matrix(const engine::Mat4& model, engine::Mat4& out) const;

        /**
         * Resolves the first authored `effect*` weapon mesh as a mix-FX anchor.
         * @param model Actor world matrix.
         * @param out Receives the socket world matrix.
         * @return True when an effect socket and weapon bone are available.
         */
        [[nodiscard]] bool weapon_effect_socket_world(const engine::Mat4& model,
                                                      engine::Mat4& out) const;

        /**
         * Computes the world matrix of one skeleton bone from the last pose.
         * @param model Character model matrix.
         * @param skeleton_bone Skeleton bone index.
         * @param out Receives the column-major world matrix.
         * @return True when the last pose contains that bone.
         */
        [[nodiscard]] bool
        bone_world_matrix(const engine::Mat4& model, int skeleton_bone, engine::Mat4& out) const;

        /**
         * Projectile origin: EFFECTBONE_EFFECT plus the (*EFFECT_ARROW) (OFFSET) from
         * the clip.
         * @param model Character model matrix.
         * @param ox Receives world X.
         * @param oy Receives world Y.
         * @param oz Receives world Z.
         * @return True when the effect socket is known.
         */
        [[nodiscard]] bool
        projectile_origin_world(const engine::Mat4& model, float& ox, float& oy, float& oz) const;

        /**
         * Consumes a latched (*EFFECT_ARROW) keyframe (compatibility behavior).
         * @return True once this clip should spawn the flying arrow.
         */
        [[nodiscard]] bool take_arrow_release();

        /** One `(*EFFECT_MODEL)` / `(*EFFECT_FX)` cue crossed this clip. */
        struct ClipEffectCue {
            /** Rendering path used by this keyframe. */
            enum class Kind : std::uint8_t {
                /** `(*EFFECT_MODEL name)` — Skill/Effect .gb via CombatFx. */
                Model = 0,

                /** `(*EFFECT_FX name)` — Effect/Fx script via SpriteFx. */
                Fx,
            };

            /** Renderer family. */
            Kind kind = Kind::Model;

            /** Asset stem from the keyframe (`skill_w_3_1`, `effect_strike01`). */
            std::string name;
        };

        /**
         * Consumes `(*EFFECT_MODEL)` / `(*EFFECT_FX)` cues crossed since the last
         * take (compatibility behavior AddTarget / AddFxToTarget).
         * @return Cues in clip order.
         */
        [[nodiscard]] std::vector<ClipEffectCue> take_clip_effects();

        /**
         * Parses `(*EFFECT_MODEL name)` / `(*EFFECT_FX name)` tokens from a keyframe
         * option string.
         * @param option compatibility behavior text.
         * @param out Receives parsed cues; existing entries are kept.
         */
        static void parse_clip_effect_cues(std::string_view option,
                                           std::vector<ClipEffectCue>& out);

        /**
         * Collects clip FX cues in (prev_ms, curr_ms]. When curr wraps below prev
         * (`PlayAniRepeat`), also fires the remainder of the cycle and the restart
         * from `loop_start`, including options at the loop boundary.
         * @param clip Skill clip whose key options are scanned.
         * @param prev_ms Previous poll cursor, or negative on the first sample.
         * @param curr_ms Current clip time in milliseconds (already wrapped).
         * @param out Receives parsed cues; existing entries are kept.
         */
        static void collect_looping_clip_effect_cues(const terrain::GbAnimClip& clip,
                                                     float prev_ms,
                                                     float curr_ms,
                                                     std::vector<ClipEffectCue>& out);

        /** @return True while the GO_SKILL clip is playing. */
        [[nodiscard]] bool skill_playing() const;

        /** @return True while a melee swing clip is playing. */
        [[nodiscard]] bool attack_playing() const;

        /**
         * Draws the character and its weapon for one blend pass.
         * @param engine Engine used to submit draws.
         * @param model Character model matrix.
         * @param view World-to-camera view used to order transparent weapon triangles.
         * @param want_alpha True to draw the alpha-blended parts, false for opaque
         * parts.
         * @param opacity Multiply alpha (related compatibility states uses 0.333 for self).
         */
        void draw(engine::IEngine& engine,
                  const engine::Mat4& model,
                  const engine::Mat4& view,
                  bool want_alpha,
                  float opacity = 1.0f) const;

        /** Draws the current skinned silhouette projected onto the ground.
         * @param engine Engine used to submit shadow draws.
         * @param model Character model matrix.
         * @param receiver Terrain height-map receiver, or an empty fallback.
         * @param ground_y Receiver-plane height.
         * @param opacity Shadow opacity in the range 0..1.
         */
        void draw_shadow(engine::IEngine& engine,
                         const engine::Mat4& model,
                         const engine::ShadowReceiver& receiver,
                         float ground_y,
                         float opacity) const;

      private:
        /** @return Charge clip while PRESKILL, otherwise the fire/cast clip. */
        [[nodiscard]] const terrain::GbAnimClip& current_skill_clip() const;

        /**
         * Loads `T*_7_A_{sub}` when the clip is not already cached.
         * @param sub GO_SKILL motion sub-index.
         * @return True when a clip for `sub` can be sampled.
         */
        [[nodiscard]] bool ensure_skill_sub(int sub);

        /** Clears latched clip-effect cues and restarts the keyframe cursor. */
        void reset_clip_effect_poll();

        /**
         * Loads .gb meshes into GPU parts, optionally pre-skinned to a bind pose.
         * @param engine Engine used to create meshes and textures.
         * @param gb_paths Mesh files to load.
         * @param tex_cache Shared texture cache.
         * @param pose_clip Clip sampled at t=0 when skin_now is set.
         * @param out_meshes Receives the compatibility behavior meshes, for later re-skinning.
         * @param out_parts Receives the GPU parts.
         * @param skin_now True to bake the bind pose before uploading.
         * @return True when at least one part was produced.
         */
        [[nodiscard]] bool
        load_meshes_from_paths(engine::IEngine& engine,
                               const std::vector<std::string>& gb_paths,
                               std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                               const terrain::GbAnimClip& pose_clip,
                               std::vector<terrain::GbMeshCpu>& out_meshes,
                               std::vector<AnimatedPart>& out_parts,
                               bool skin_now);

        /** Skeleton shared by every clip. */
        std::vector<terrain::GbBone> m_bones;

        /** T*_0 — idle clip. */
        terrain::GbAnimClip m_idle;

        /** T*_1 — walk clip. */
        terrain::GbAnimClip m_walk;

        /** T*_2 — death clip (AT_DIE → ActionType 2). */
        terrain::GbAnimClip m_die;

        /** T*_8_{A|B}_01 — primary swing. */
        terrain::GbAnimClip m_attack;

        /** T*_8_{A|B}_02 — alternate swing. */
        terrain::GbAnimClip m_attack2;

        /** T*_8_B_03 — third two-handed knight swing. */
        terrain::GbAnimClip m_attack3;

        /** T*_7_A_03 — GO_SKILL fire / mage cast. */
        terrain::GbAnimClip m_skill;

        /** T*_7_A_02 — GO_SKILL charge (compatibility behavior `runtime state[0]` = 2). */
        terrain::GbAnimClip m_skill_charge;

        /** T*_7_A_01 — compatibility behavior execute (`runtime state[1]` = 1). */
        terrain::GbAnimClip m_behead;

        /** Extra GO_SKILL subs loaded on demand (`T*_7_A_{NN}`). */
        std::unordered_map<int, terrain::GbAnimClip> m_skill_extra;

        /** Lazily loaded human action-10 motions. */
        std::unordered_map<int, terrain::GbAnimClip> m_emotes;

        /** Motion selected while Clip::Emote is active. */
        int m_emote_motion = 0;

        /** T*_9_A_04 — open treasure box kneel. */
        terrain::GbAnimClip m_open_box;

        /** T*_6_A_01 or T*_12_A_01 — fishing cast and held pose. */
        terrain::GbAnimClip m_fishing;

        /** T*_5_A_01 — sit down. */
        terrain::GbAnimClip m_sit;

        /** T*_5_A_03 — sit consume. */
        terrain::GbAnimClip m_sit_consume;

        /** T*_5_A_02 — stand up. */
        terrain::GbAnimClip m_stand;

        /** T*_3_A_01 — evade / dodge. */
        terrain::GbAnimClip m_evade;

        /** T*_0_A_04 — mounted rider pose. */
        terrain::GbAnimClip m_riding;

        /** True when the death clip loaded. */
        bool m_has_die = false;

        /** True when the primary swing loaded. */
        bool m_has_attack = false;

        /** True when the alternate swing loaded. */
        bool m_has_attack2 = false;

        /** True when the third two-handed knight swing loaded. */
        bool m_has_attack3 = false;

        /** True when the cast clip loaded. */
        bool m_has_skill = false;

        /** True when the charge clip (A_02) loaded. */
        bool m_has_skill_charge = false;

        /** True when the behead clip (A_01) loaded. */
        bool m_has_behead = false;

        /** True when the open-box clip loaded. */
        bool m_has_open_box = false;

        /** True while the open-box motion is used as a single gathering cycle. */
        bool m_harvest_once = false;

        /** True when the fishing clip loaded. */
        bool m_has_fishing = false;

        /** True while the fishing pose is rewinding to reel in a catch. */
        bool m_fishing_reeling = false;

        /** True when the sit-down clip loaded. */
        bool m_has_sit = false;

        /** True when the sit-consume clip loaded. */
        bool m_has_sit_consume = false;

        /** True when the stand-up clip loaded. */
        bool m_has_stand = false;

        /** True when the evade clip loaded. */
        bool m_has_evade = false;

        /** True when the currently selected rider pose loaded. */
        bool m_has_riding = false;

        /** Swing clip playing (0 = attack, 1 = attack2, 2 = attack3). */
        int m_attack_variant = 0;

        /** Swing clip for the next trigger. */
        int m_next_attack_variant = 0;

        /** compatibility behavior character meshes, re-skinned every frame. */
        std::vector<terrain::GbMeshCpu> m_meshes;

        /** GPU character parts, parallel to runtime state. */
        std::vector<AnimatedPart> m_parts;

        /** Bind-pose weapon meshes. */
        std::vector<terrain::GbMeshCpu> m_weapon_bind;

        /** GPU weapon parts. */
        std::vector<AnimatedPart> m_weapon_parts;

        /** Bind-pose meshes for the class-local C1_01 fishing cast rod. */
        std::vector<terrain::GbMeshCpu> m_fishing_cast_bind;

        /** GPU meshes for the class-local C1_01 fishing cast rod. */
        std::vector<AnimatedPart> m_fishing_cast_parts;

        /** Bind-pose meshes for the class-local C1_02 held fishing rod. */
        std::vector<terrain::GbMeshCpu> m_fishing_hold_bind;

        /** GPU meshes for the class-local C1_02 held fishing rod. */
        std::vector<AnimatedPart> m_fishing_hold_parts;

        /** Bind-pose shield meshes. */
        std::vector<terrain::GbMeshCpu> m_shield_bind;

        /** GPU shield parts. */
        std::vector<AnimatedPart> m_shield_parts;

        /** Weapon attach bone index. */
        int m_weapon_bone = 45;

        /** Shield / off-hand attach bone index (0 = none). */
        int m_shield_bone = 32;

        /** EFFECTBONE_EFFECT — cast origin (wand tip). */
        int m_effect_bone = 86;

        /** Last evaluated world matrix of the weapon bone. */
        std::array<float, 16> m_last_weapon_bone_mat{};

        /** Last evaluated world matrix of the shield bone. */
        std::array<float, 16> m_last_shield_bone_mat{};

        /** Last evaluated world matrix of the effect bone. */
        std::array<float, 16> m_last_effect_bone_mat{};

        /** Last evaluated bone-world matrices (skeleton order) for FX attach. */
        std::vector<std::array<float, 16>> m_last_bone_mats;

        /** True once runtime state holds a pose. */
        bool m_have_weapon_bone_mat = false;

        /** True once runtime state holds a pose. */
        bool m_have_shield_bone_mat = false;

        /** True once runtime state holds a pose. */
        bool m_have_effect_bone_mat = false;

        /** True once runtime state holds a pose. */
        bool m_have_bone_mats = false;

        /** Attack-clip (*EFFECT_ARROW) time in milliseconds; -1 if none. */
        float m_attack_arrow_ms = -1.0f;

        /** Attack-clip EFFECT_ARROW OFFSET X (world). */
        float m_attack_arrow_ox = 0.0f;

        /** Attack-clip EFFECT_ARROW OFFSET Y (world). */
        float m_attack_arrow_oy = 0.0f;

        /** Attack-clip EFFECT_ARROW OFFSET Z (world). */
        float m_attack_arrow_oz = 0.0f;

        /** Attack A_02 clip (*EFFECT_ARROW) time in milliseconds; -1 if none. */
        float m_attack2_arrow_ms = -1.0f;

        /** Attack A_02 EFFECT_ARROW OFFSET X (world). */
        float m_attack2_arrow_ox = 0.0f;

        /** Attack A_02 EFFECT_ARROW OFFSET Y (world). */
        float m_attack2_arrow_oy = 0.0f;

        /** Attack A_02 EFFECT_ARROW OFFSET Z (world). */
        float m_attack2_arrow_oz = 0.0f;

        /** Attack B_03 (*EFFECT_ARROW) time in milliseconds; -1 if none. */
        float m_attack3_arrow_ms = -1.0f;

        /** Attack B_03 EFFECT_ARROW OFFSET X (world). */
        float m_attack3_arrow_ox = 0.0f;

        /** Attack B_03 EFFECT_ARROW OFFSET Y (world). */
        float m_attack3_arrow_oy = 0.0f;

        /** Attack B_03 EFFECT_ARROW OFFSET Z (world). */
        float m_attack3_arrow_oz = 0.0f;

        /** Skill-clip (*EFFECT_ARROW) time in milliseconds; -1 if none. */
        float m_skill_arrow_ms = -1.0f;

        /** Skill-clip EFFECT_ARROW OFFSET X (world). */
        float m_skill_arrow_ox = 0.0f;

        /** Skill-clip EFFECT_ARROW OFFSET Y (world). */
        float m_skill_arrow_oy = 0.0f;

        /** Skill-clip EFFECT_ARROW OFFSET Z (world). */
        float m_skill_arrow_oz = 0.0f;

        /** True while waiting for the current clip to reach the EFFECT_ARROW key. */
        bool m_await_arrow = false;

        /** True when take_arrow_release() should spawn the projectile. */
        bool m_arrow_ready = false;

        /** Pending EFFECT_ARROW time for the clip now playing. */
        float m_pending_arrow_ms = -1.0f;

        /** Pending OFFSET applied at release (world, compatibility behavior). */
        float m_pending_arrow_ox = 0.0f;

        /** Pending OFFSET Y. */
        float m_pending_arrow_oy = 0.0f;

        /** Pending OFFSET Z. */
        float m_pending_arrow_oz = 0.0f;

        /** Playback cursor of the current clip, in milliseconds. */
        float m_anim_time_ms = 0.0f;

        /** Cursor at the last skinning pass, in milliseconds. */
        float m_last_skinned_time_ms = -1000.0f;

        /** True while the last death frame is held (runtime state >= 8). */
        bool m_die_hold = false;

        /** True after the GO_SKILL clip reached its loop region (PlayAniRepeat). */
        bool m_skill_looping = false;

        /** True while PRESKILL should sample A_02 instead of A_03. */
        bool m_skill_use_charge = false;

        /** True while compatibility behavior should sample T*_7_A_01. */
        bool m_skill_use_behead = false;

        /** GO_SKILL sub currently sampled (1 = A_01, 2 = A_02, 3 = A_03, …). */
        int m_skill_motion_sub = 3;

        /** True while the charge pose must not be cancelled by walk. */
        bool m_lock_skill_pose = false;

        /** True while a behead swing must finish (or be cancelled by a new target). */
        bool m_lock_attack_pose = false;

        /** True to play the current attack or skill once before returning to idle. */
        bool m_attack_play_once = false;

        /** Class this actor was loaded for. */
        Class m_clazz = Class::Knight;

        /** Natural face and hair, retained when equipment reloads. */
        int m_face = 1;
        int m_hair = 1;

        /** Metamo race, or -1 for human. */
        int m_metamo_type = -1;

        /** Metamo stage 1–3, or 0. */
        int m_metamo_level = 0;

        /** shinsuinfo scale. */
        float m_metamo_scale = 1.0f;

        /** Client data root the meshes were loaded from. */
        std::string m_data_root;

        /** Clip currently playing. */
        Clip m_clip = Clip::Idle;

        /** Keyframe SOUND tracker for the clip now playing. */
        AnimSound m_sounds;

        /** A restarted motion must reset sounds even if its clip category is unchanged. */
        bool m_restart_sounds = false;

        /** Clip whose sounds runtime state is tracking; reset when it changes. */
        Clip m_sound_clip = Clip::Idle;

        /** True after S2C confirms the current melee swing hit. */
        bool m_melee_hit_confirmed = false;

        /** True after EFFECT_MODEL effect_attack* was crossed this swing. */
        bool m_melee_hit_model_passed = false;

        /** True after efc006 was started for this swing. */
        bool m_melee_hit_sfx_played = false;

        /** Attack-clip cursor used to detect EFFECT_MODEL crossings. */
        float m_melee_hit_prev_ms = -1.0f;

        /** Clip whose EFFECT_MODEL / EFFECT_FX cues are being tracked. */
        Clip m_effect_clip = Clip::Idle;

        /** Cursor used to detect clip-effect keyframe crossings. */
        float m_clip_effect_prev_ms = -1.0f;

        /** Latched `(*EFFECT_MODEL)` / `(*EFFECT_FX)` cues for the current frame. */
        std::vector<ClipEffectCue> m_pending_clip_effects;
    };

    inline void AnimatedActor::parse_clip_effect_cues(std::string_view option,
                                                      std::vector<ClipEffectCue>& out) {
        auto parse_tag = [&](std::string_view tag, ClipEffectCue::Kind kind) {
            std::size_t search = 0;
            while (search < option.size()) {
                const auto pos = option.find(tag, search);
                if (pos == std::string_view::npos) {
                    return;
                }
                const auto after = option.substr(pos + tag.size());
                if (tag == "(*EFFECT_FX" && after.size() >= 3 && after.compare(0, 3, "_NR") == 0) {
                    search = pos + tag.size();
                    continue;
                }
                auto rest = after;
                while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                    rest.remove_prefix(1);
                }
                std::size_t n = 0;
                while (n < rest.size()) {
                    const char c = rest[n];
                    if (c == ')' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                        break;
                    }
                    ++n;
                }
                if (n > 0) {
                    ClipEffectCue cue;
                    cue.kind = kind;
                    cue.name.assign(rest.data(), n);
                    out.push_back(cue);
                }
                search = pos + tag.size();
            }
        };
        parse_tag("(*EFFECT_FX_NR", ClipEffectCue::Kind::Fx);
        parse_tag("(*EFFECT_FX", ClipEffectCue::Kind::Fx);
        parse_tag("(*EFFECT_MODEL", ClipEffectCue::Kind::Model);
    }

    inline void AnimatedActor::collect_looping_clip_effect_cues(const terrain::GbAnimClip& clip,
                                                                float prev_ms,
                                                                float curr_ms,
                                                                std::vector<ClipEffectCue>& out) {
        auto poll = [&](float from_ms, float to_ms) {
            const float lower = from_ms < 0.0f ? -1.0f : from_ms;
            const std::size_t n = clip.key_times.size() < clip.key_options.size()
                                      ? clip.key_times.size()
                                      : clip.key_options.size();
            for (std::size_t i = 0; i < n; ++i) {
                const float t = static_cast<float>(clip.key_times[i]);
                if (!(lower < t && t <= to_ms)) {
                    continue;
                }
                parse_clip_effect_cues(clip.key_options[i], out);
            }
        };
        float dur = clip.duration_ms();
        if (dur < 1.0f) {
            dur = 1.0f;
        }
        float loop_start = clip.loop_start_ms;
        if (loop_start < 0.0f || loop_start >= dur) {
            loop_start = 0.0f;
        }
        if (curr_ms + 0.5f < prev_ms) {
            poll(prev_ms, dur + 1.0f);
            poll(loop_start - 1.0f, curr_ms);
            return;
        }
        poll(prev_ms, curr_ms);
    }

}  // namespace openkal::client
