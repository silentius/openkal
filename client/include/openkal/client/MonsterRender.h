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
#include "openkal/client/MonsterCatalog.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GbModel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openkal::client {

    /** One drawable submesh of a monster instance with its render state. */
    class MonsterPart {
      public:
        /** GPU mesh holding the skinned vertices of this part.
         * @return Engine mesh handle.
         */
        [[nodiscard]] engine::MeshHandle mesh() const {
            return m_mesh;
        }

        /** Sets the GPU mesh of this part.
         * @param mesh Engine mesh handle.
         */
        void set_mesh(engine::MeshHandle mesh) {
            m_mesh = mesh;
        }

        /** Diffuse texture of this part.
         * @return Engine texture handle.
         */
        [[nodiscard]] engine::TextureHandle texture() const {
            return m_texture;
        }

        /** Sets the diffuse texture of this part.
         * @param texture Engine texture handle.
         */
        void set_texture(engine::TextureHandle texture) {
            m_texture = texture;
        }

        /** Whether the part is drawn in the alpha blended pass.
         * @return True when alpha blending is enabled.
         */
        [[nodiscard]] bool alpha_blend() const {
            return m_alpha_blend;
        }

        /** Sets the alpha blending flag.
         * @param enabled True to draw the part in the alpha blended pass.
         */
        void set_alpha_blend(bool enabled) {
            m_alpha_blend = enabled;
        }

        /** Whether the part uses alpha testing (cutout in the opaque pass).
         * @return True when alpha testing is enabled.
         */
        [[nodiscard]] bool alpha_test() const {
            return m_alpha_test;
        }

        /** Sets the alpha testing flag.
         * @param enabled True to enable alpha testing.
         */
        void set_alpha_test(bool enabled) {
            m_alpha_test = enabled;
        }

        /** Whether back face culling is disabled for this part.
         * @return True when the part is two sided.
         */
        [[nodiscard]] bool two_sided() const {
            return m_two_sided;
        }

        /** Sets the two sided flag.
         * @param enabled True to disable back face culling.
         */
        void set_two_sided(bool enabled) {
            m_two_sided = enabled;
        }

      private:
        /** GPU mesh of the part. */
        engine::MeshHandle m_mesh;

        /** Diffuse texture of the part. */
        engine::TextureHandle m_texture;

        /** Alpha blended pass flag. */
        bool m_alpha_blend = false;

        /** Alpha test (cutout) flag. */
        bool m_alpha_test = false;

        /** Two sided (no back face culling) flag. */
        bool m_two_sided = false;
    };

    /**
     * One attack-clip keyframe that spawns a projectile (`OnAnimationOption`
     * `(*PROJECTILE)` / `(*EFFECT_ARROW)` / `(*EFFECT_STONE)`).
     */
    class MonsterAttackFx {
      public:
        /** Kind of projectile authored on the attack GB. */
        enum class Kind { Projectile, Arrow, Stone };

        /** @return Keyframe kind. */
        [[nodiscard]] Kind kind() const {
            return m_kind;
        }

        /**
         * Sets the keyframe kind.
         * @param v Kind parsed from the option string.
         */
        void set_kind(Kind v) {
            m_kind = v;
        }

        /** `effectType_t` for `(*PROJECTILE type bone)`, else 0.
         * @return Effect type integer.
         */
        [[nodiscard]] int type() const {
            return m_type;
        }

        /**
         * Sets the projectile effect type.
         * @param v `effectType_t` value from the GB.
         */
        void set_type(int v) {
            m_type = v;
        }

        /** EFFECTBONE slot the projectile leaves from (`(*PROJECTILE type bone)`).
         * @return Effect-bone index, default EFFECTBONE_SPINE1 (1).
         */
        [[nodiscard]] int bone() const {
            return m_bone;
        }

        /**
         * Sets the origin EFFECTBONE slot.
         * @param v Effect-bone index.
         */
        void set_bone(int v) {
            m_bone = v;
        }

        /**
         * Parses a GB key option into an attack projectile.
         * @param option Keyframe `runtime state` string.
         * @param out Filled on success.
         * @return True when the option spawns a projectile.
         */
        [[nodiscard]] static bool parse_option(std::string_view option, MonsterAttackFx& out);

      private:
        /** Keyframe kind. */
        Kind m_kind = Kind::Projectile;

        /** `effectType_t` for PROJECTILE, else 0. */
        int m_type = 0;

        /** Origin EFFECTBONE slot. */
        int m_bone = 1;
    };

    /** One explicit companion-mesh source and its optional texture replacement. */
    struct PetMeshAsset {
        /** Full path to the bind-pose GB mesh. */
        std::string gb_path;

        /** Texture name relative to the mesh directory; empty uses the GB material texture. */
        std::string texture_override;
    };

    /** Shared rig data of one monster index: skeleton, clips and bind-pose clothes.
     */
    class MonsterRigGpu {
      public:
        /** Loads skeleton, motion clips and clothes meshes of a monster index.
         * @param engine Engine used to create the textures.
         * @param data_root Root directory of the client data files.
         * @param index Monster index.
         * @param macro Appearance rig describing bone set, clip letters and scale.
         * @param out Rig filled on success; reset to defaults on entry.
         * @param tex_cache Texture cache shared between rigs, keyed by gb path and
         * texture name.
         * @param bone_info Optional effect bones of the skeleton set.
         * @param beheaded True to load the detached-head (H2) clothes set.
         * @param resource_root Root containing owned RuntimeData models.
         * @return True when at least one bind-pose mesh was loaded.
         */
        [[nodiscard]] static bool
        load(engine::IEngine& engine,
             const std::string& data_root,
             int index,
             const MonsterRig& macro,
             MonsterRigGpu& out,
             std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
             const MonsterBoneInfo* bone_info = nullptr,
             bool beheaded = false,
             const std::string& resource_root = {});

        /** Loads a companion rig from explicit Pet asset paths.
         * @param engine Engine used to create textures.
         * @param index Pet item index used for diagnostics.
         * @param scale Uniform render scale.
         * @param bone_path Skeleton GB path.
         * @param idle_path Idle-motion GB path.
         * @param walk_path Walk-motion GB path, or empty when unavailable.
         * @param run_path Run-motion GB path, or empty when unavailable.
         * @param cloth_assets Bind-pose clothes GB paths and optional texture overrides.
         * @param out Rig filled on success.
         * @param tex_cache Shared texture cache.
         * @return True when the skeleton, idle clip and at least one mesh load.
         */
        [[nodiscard]] static bool
        load_pet(engine::IEngine& engine,
                 int index,
                 float scale,
                 const std::string& bone_path,
                 const std::string& idle_path,
                 const std::string& walk_path,
                 const std::string& run_path,
                 const std::vector<PetMeshAsset>& cloth_assets,
                 MonsterRigGpu& out,
                 std::unordered_map<std::string, engine::TextureHandle>& tex_cache);

        /** Monster index this rig was loaded for.
         * @return Monster index.
         */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /** Uniform model scale of the monster.
         * @return Scale factor, 1.0 when unscaled.
         */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /** EFFECTBONE_SPINE1 index taken from the monsterbone entry.
         * @return Bone index, or -1 when unknown.
         */
        [[nodiscard]] int spine_bone() const {
            return m_spine_bone;
        }

        /** Target height taken from the monsterbone target[3] value.
         * @return Height in world units.
         */
        [[nodiscard]] float target_height() const {
            return m_target_height;
        }

        /** EFFECTBONE_HEAD index taken from the monsterbone entry.
         * @return Bone index, or -1 when unknown.
         */
        [[nodiscard]] int head_bone() const {
            return m_head_bone;
        }

        /**
         * Maps an EFFECTBONE slot to a skeleton bone.
         * @param effect_bone EFFECTBONE_* index, or -1.
         * @return Skeleton bone index, or -1 when unknown / origin.
         */
        [[nodiscard]] int skeleton_bone(int effect_bone) const {
            if (effect_bone < 0 || effect_bone >= kEffectBoneCount) {
                return -1;
            }
            return m_effect_bones[static_cast<std::size_t>(effect_bone)];
        }

        /** Pick AABB XZ half-extent, already scaled.
         * @return World-space half-extent.
         */
        [[nodiscard]] float pick_half() const {
            return m_pick_half;
        }

        /** Pick AABB height, already scaled.
         * @return World-space height.
         */
        [[nodiscard]] float pick_height() const {
            return m_pick_height;
        }

        /** Looping particle attachments stored from the appearance rig.
         * @return Particle list.
         */
        [[nodiscard]] const std::vector<MonsterFxAttach>& particles() const {
            return m_particles;
        }

        /** Looping FX attachments stored from the appearance rig.
         * @return FX list.
         */
        [[nodiscard]] const std::vector<MonsterFxAttach>& fx() const {
            return m_fx;
        }

        /** Idle-clip `(*EFFECT_FX)` oneshots (OnAnimationOption).
         * @return Cue list in key order.
         */
        [[nodiscard]] const std::vector<MonsterIdleFxCue>& idle_fx_cues() const {
            return m_idle_fx_cues;
        }

        /** Monster level stored from the appearance rig.
         * @return Monster level, 1 when unknown.
         */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /** Y offset added above the head bone, monsterinfo scale[1].
         * @return World-space Y offset.
         */
        [[nodiscard]] float name_offset_y() const {
            return m_name_offset_y;
        }

        /** Skeleton of the rig.
         * @return Bones in skeleton order.
         */
        [[nodiscard]] const std::vector<terrain::GbBone>& bones() const {
            return m_bones;
        }

        /** Idle animation clip.
         * @return Idle clip, always present on a loaded rig.
         */
        [[nodiscard]] const terrain::GbAnimClip& idle() const {
            return m_idle;
        }

        /** Walk animation clip.
         * @return Walk clip, only valid when has_walk() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& walk() const {
            return m_walk;
        }

        /** Run animation clip (ActionType 2 / ModelChange(2)).
         * @return Run clip, only valid when has_run() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& run() const {
            return m_run;
        }

        /** Attack animation clip.
         * @return Attack clip, only valid when has_attack() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& attack() const {
            return m_attack;
        }

        /** Finds a Monster ModelSkill Action 7 clip by its macro motion number.
         * @param motion Skill motion number; 0 uses the `_01` fallback.
         * @return Matching clip, or nullptr when the asset is unavailable.
         */
        [[nodiscard]] const terrain::GbAnimClip* skill(int motion) const {
            const int resolved_motion = motion > 0 ? motion : 1;
            const auto it = m_skill.find(resolved_motion);
            return it != m_skill.end() ? &it->second : nullptr;
        }

        /** Authored one-shot FX cues for a Monster ModelSkill Action 7 clip.
         * @param motion Skill motion number; 0 uses the `_01` fallback.
         * @return FX cues for the clip, or an empty list when none are authored.
         */
        [[nodiscard]] const std::vector<MonsterIdleFxCue>& skill_fx_cues(int motion) const {
            static const std::vector<MonsterIdleFxCue> empty;
            const int resolved_motion = motion > 0 ? motion : 1;
            const auto it = m_skill_fx_cues.find(resolved_motion);
            return it != m_skill_fx_cues.end() ? it->second : empty;
        }

        /** Die / open animation clip (ActionType 3).
         * @return Die clip, only valid when has_die() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& die() const {
            return m_die;
        }

        /** Behead animation clip (ActionType 4).
         * @return Behead clip, only valid when has_behead() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& behead() const {
            return m_behead;
        }

        /** Kneel animation clip (ActionType 9).
         * @return Kneel clip, only valid when has_knee() is true.
         */
        [[nodiscard]] const terrain::GbAnimClip& knee() const {
            return m_knee;
        }

        /** Whether a walk clip was found for this rig.
         * @return True when the walk clip is usable.
         */
        [[nodiscard]] bool has_walk() const {
            return m_has_walk;
        }

        /** Whether a run clip was found for this rig.
         * @return True when the run clip is usable.
         */
        [[nodiscard]] bool has_run() const {
            return m_has_run;
        }

        /** Whether an attack clip was found for this rig.
         * @return True when the attack clip is usable.
         */
        [[nodiscard]] bool has_attack() const {
            return m_has_attack;
        }

        /** Whether a die clip was found for this rig.
         * @return True when the die clip is usable.
         */
        [[nodiscard]] bool has_die() const {
            return m_has_die;
        }

        /** Whether a behead clip was found for this rig.
         * @return True when the behead clip is usable.
         */
        [[nodiscard]] bool has_behead() const {
            return m_has_behead;
        }

        /** Whether a kneel clip was found for this rig.
         * @return True when the kneel clip is usable.
         */
        [[nodiscard]] bool has_knee() const {
            return m_has_knee;
        }

        /** Loads the Action 8 attack clip when the server requests an attack.
         * @param data_root Root directory of the client data files.
         * @param macro Appearance rig describing the action filename.
         * @return True when the attack clip is available.
         */
        [[nodiscard]] bool ensure_attack(const std::string& data_root, const MonsterRig& macro);

        /** Loads an Action 7 skill clip when the server requests that skill motion.
         * @param data_root Root directory of the client data files.
         * @param macro Appearance rig describing the action filename.
         * @param motion Requested monster skill motion; zero resolves to `_01`.
         * @return True when the skill clip is available.
         */
        [[nodiscard]] bool
        ensure_skill(const std::string& data_root, const MonsterRig& macro, int motion);

        /** Bind-pose clothes meshes shared by every instance of this rig.
         * @return Bind-pose meshes in load order.
         */
        [[nodiscard]] const std::vector<terrain::GbMeshCpu>& bind_meshes() const {
            return m_bind_meshes;
        }

        /** Diffuse textures, one per bind-pose mesh.
         * @return Texture handles in mesh order.
         */
        [[nodiscard]] const std::vector<engine::TextureHandle>& textures() const {
            return m_textures;
        }

        /** Alpha blending flags, one per bind-pose mesh.
         * @return Flags in mesh order.
         */
        [[nodiscard]] const std::vector<bool>& alpha_blend_flags() const {
            return m_alpha_blend;
        }

        /** Alpha testing flags, one per bind-pose mesh.
         * @return Flags in mesh order.
         */
        [[nodiscard]] const std::vector<bool>& alpha_test_flags() const {
            return m_alpha_test;
        }

        /** Two sided flags, one per bind-pose mesh.
         * @return Flags in mesh order.
         */
        [[nodiscard]] const std::vector<bool>& two_sided_flags() const {
            return m_two_sided;
        }

      private:
        /** Monster index of the rig. */
        int m_index = 0;

        /** Uniform model scale. */
        float m_scale = 1.0f;

        /** EFFECTBONE_SPINE1 index from monsterbone. */
        int m_spine_bone = -1;

        /** monsterbone target[3] height. */
        float m_target_height = 17.0f;

        /** EFFECTBONE_HEAD index from monsterbone. */
        int m_head_bone = -1;

        /** Skeleton indices keyed by EFFECTBONE_*. */
        std::array<int, kEffectBoneCount> m_effect_bones{};

        /** Scaled pick AABB XZ half-extent. */
        float m_pick_half = 16.0f;

        /** Scaled pick AABB height. */
        float m_pick_height = 32.0f;

        /** Looping particle attaches. */
        std::vector<MonsterFxAttach> m_particles;

        /** Looping FX attaches. */
        std::vector<MonsterFxAttach> m_fx;

        /** Idle-clip EFFECT_FX oneshots. */
        std::vector<MonsterIdleFxCue> m_idle_fx_cues;

        /** Monsterinfo levelno. */
        int m_level = 1;

        /** monsterinfo scale[1], added above the head bone. */
        float m_name_offset_y = 5.0f;

        /** Skeleton bones in skeleton order. */
        std::vector<terrain::GbBone> m_bones;

        /** Idle animation clip. */
        terrain::GbAnimClip m_idle;

        /** Walk animation clip. */
        terrain::GbAnimClip m_walk;

        /** Run animation clip (action 2). */
        terrain::GbAnimClip m_run;

        /** Attack animation clip. */
        terrain::GbAnimClip m_attack;

        /** Action 7 / GO_SKILL clips keyed by macro-skill motion number. */
        std::unordered_map<int, terrain::GbAnimClip> m_skill;

        /** Authored Action 7 EFFECT_FX cues keyed by macro-skill motion number. */
        std::unordered_map<int, std::vector<MonsterIdleFxCue>> m_skill_fx_cues;

        /** Die / open animation clip (action 3). */
        terrain::GbAnimClip m_die;

        /** Behead animation clip (action 4). */
        terrain::GbAnimClip m_behead;

        /** Kneel animation clip (action 9). */
        terrain::GbAnimClip m_knee;

        /** True when the walk clip was loaded. */
        bool m_has_walk = false;

        /** True when the run clip was loaded. */
        bool m_has_run = false;

        /** True when the attack clip was loaded. */
        bool m_has_attack = false;

        /** True when the die clip was loaded. */
        bool m_has_die = false;

        /** True when the behead clip was loaded. */
        bool m_has_behead = false;

        /** True when the kneel clip was loaded. */
        bool m_has_knee = false;

        /** Action numbers already attempted but unavailable. */
        std::unordered_set<int> m_missing_actions;

        /** Action 7 sub-indices already attempted but unavailable. */
        std::unordered_set<int> m_missing_skill_motions;

        /** Bind-pose clothes meshes. */
        std::vector<terrain::GbMeshCpu> m_bind_meshes;

        /** Diffuse textures per bind-pose mesh. */
        std::vector<engine::TextureHandle> m_textures;

        /** Alpha blending flags per bind-pose mesh. */
        std::vector<bool> m_alpha_blend;

        /** Alpha testing flags per bind-pose mesh. */
        std::vector<bool> m_alpha_test;

        /** Two sided flags per bind-pose mesh. */
        std::vector<bool> m_two_sided;
    };

    /** Latched S2C_ATTACK presentation delayed until `(*EFFECT_DAMAGE)` (ActionDamage). */
    class MonsterActionDamage {
      public:
        /** Packet damage shown as a combat float (0 on miss/block).
         * @return Damage amount.
         */
        [[nodiscard]] std::int32_t damage() const {
            return m_damage;
        }

        /**
         * Sets the packet damage.
         * @param v Damage amount.
         */
        void set_damage(std::int32_t v) {
            m_damage = v;
        }

        /** S2C_ATTACK flags (hit / fatal / block / explosive).
         * @return Flag bits.
         */
        [[nodiscard]] std::uint8_t flags() const {
            return m_flags;
        }

        /**
         * Sets the S2C_ATTACK flags.
         * @param v Flag bits.
         */
        void set_flags(std::uint8_t v) {
            m_flags = v;
        }

        /** Target entity id (local or remote player).
         * @return Entity id.
         */
        [[nodiscard]] std::uint32_t target_id() const {
            return m_target_id;
        }

        /**
         * Sets the target entity id.
         * @param v Entity id.
         */
        void set_target_id(std::uint32_t v) {
            m_target_id = v;
        }

      private:
        /** Packet damage. */
        std::int32_t m_damage = 0;

        /** S2C_ATTACK flags. */
        std::uint8_t m_flags = 0;

        /** Target entity id. */
        std::uint32_t m_target_id = 0;
    };

    /** Per-entity skinned GPU meshes of a monster plus its animation clock. */
    class MonsterInstance {
      public:
        /** Animation clip currently played by an instance. */
        enum class Clip : int { Idle, Walk, Run, Attack, Skill, Die, Knee, Behead };

        /** Releases the GPU meshes of the instance and resets the skinning cache.
         * @param engine Engine owning the meshes.
         */
        void destroy(engine::IEngine& engine);

        /** Builds the skinned GPU meshes of this instance from a shared rig.
         * @param engine Engine used to create the meshes.
         * @param rig Shared rig providing skeleton, bind-pose meshes and textures.
         * @param fade_in True for normal monster creation; false for an already-existing corpse.
         * @return True when meshes and/or field FX were created.
         */
        [[nodiscard]] bool
        create_from_rig(engine::IEngine& engine, const MonsterRigGpu& rig, bool fade_in = true);

        /** Plays the attack clip from the start (restarts if a swing is already playing). */
        void trigger_attack();

        /** Plays an Action 7 clip while keeping the server attack a basic attack.
         * @param motion Action 7 motion number; 0 uses the `_01` fallback.
         */
        void trigger_basic_attack_visual(int motion);

        /** Starts a Monster ModelSkill Action 7 clip.
         * @param motion Skill macro's motion number; 0 uses the `_01` fallback.
         */
        void trigger_skill(int motion);

        /**
         * Latches ActionDamage until `(*EFFECT_DAMAGE)` (or clip end / move interrupt).
         * @param hit Packet damage, flags, and target id.
         */
        void latch_action_damage(const MonsterActionDamage& hit);

        /**
         * Consumes a latched ActionDamage that has reached its keyframe.
         * @param out Receives the hit.
         * @return True when presentation should fire this frame.
         */
        [[nodiscard]] bool take_action_damage(MonsterActionDamage& out);

        /**
         * True when a GB key option is `(*EFFECT_DAMAGE)`.
         * @param option Keyframe `runtime state` string.
         * @return True when ActionDamage should fire at this key.
         */
        [[nodiscard]] static bool is_effect_damage_option(std::string_view option);

        /**
         * Earliest `(*EFFECT_DAMAGE)` key time in a clip.
         * @param key_times Keyframe times in milliseconds.
         * @param key_options Parallel option strings.
         * @return Time in milliseconds, or negative when the clip has no key.
         */
        [[nodiscard]] static float
        effect_damage_time_ms(const std::vector<std::uint16_t>& key_times,
                              const std::vector<std::string>& key_options);

        /**
         * True when a latched ActionDamage should present.
         * @param latched True while a hit is waiting for the keyframe.
         * @param anim_time_ms Current attack clip time.
         * @param key_ms EFFECT_DAMAGE time, or negative when missing.
         * @param force True at clip end or when a real MoveOn interrupts the swing.
         * @return True when presentation should fire.
         */
        [[nodiscard]] static bool
        action_damage_should_fire(bool latched, float anim_time_ms, float key_ms, bool force);

        /** CreateMonster `SetBlendInTimer` default (3 seconds). */
        static constexpr int kDefaultBlendInMs = 3000;

        /**
         * Opacity while fading in (compatibility behavior `runtime state`).
         * @param timer_ms Blend-in duration; 0 means fully opaque.
         * @param time_ms Elapsed blend-in time.
         * @return Opacity in [0, 1].
         */
        [[nodiscard]] static float blend_opacity_at(float timer_ms, float time_ms);

        /** @return Current spawn fade-in opacity in [0, 1]. */
        [[nodiscard]] float blend_opacity() const {
            return blend_opacity_at(m_blend_in_timer_ms, m_blend_in_time_ms);
        }

        /**
         * Consumes a projectile release latched from the attack clip keyframe.
         * @param out Receives the parsed key (`(*PROJECTILE)` / arrow / stone).
         * @return True when a projectile should spawn this frame.
         */
        [[nodiscard]] bool take_attack_fx(MonsterAttackFx& out);

        /**
         * Consumes one idle `(*EFFECT_FX)` cue that the idle clip just crossed.
         * @param out Receives the FX name.
         * @return True when an idle FX should spawn this frame.
         */
        [[nodiscard]] bool take_idle_fx(std::string& out);

        /** Starts the die / open clip (AT_DIE → action 3); plays once and holds. */
        void trigger_die();

        /** Starts the kneel clip (AT_KNEE → action 9); plays once and holds. */
        void trigger_knee();

        /** Starts the behead clip (AT_BEHEADED → action 4); plays once. */
        void trigger_behead();

        /** Starts a short white flash when the monster is hit.
         * @param critical True for critical / explosive hits (stronger peak).
         */
        void trigger_hit_flash(bool critical);

        /** @return True while a death / corpse clip is active or held. */
        [[nodiscard]] bool dying() const {
            return m_clip == Clip::Die || m_clip == Clip::Knee || m_clip == Clip::Behead ||
                   m_die_hold;
        }

        /** @return True while the Maguni kneel pose is held. */
        [[nodiscard]] bool kneeling() const {
            return m_clip == Clip::Knee || (m_die_hold && m_clip == Clip::Knee);
        }

        /** @return True while the behead clip is playing. */
        [[nodiscard]] bool beheading() const {
            return m_clip == Clip::Behead;
        }

        /** World Y offset applied after the die clip (sink into the ground). */
        [[nodiscard]] float sink_y() const {
            return m_sink_y;
        }

        /** @return True when the local die/behead sequence has finished. */
        [[nodiscard]] bool ready_to_despawn() const {
            return m_ready_to_despawn;
        }

        /** Advances the animation clock and optionally re-skins the GPU meshes.
         * @param engine Engine owning the meshes.
         * @param rig Shared rig providing skeleton and clips.
         * @param dt Frame time in seconds.
         * @param moving True while the monster is moving.
         * @param running True while the monster moves at aggro run speed.
         * @param audio SFX mixer for `(*SOUND)` keyframes; nullptr skips playback.
         * @param wx World X used as the sound origin.
         * @param wy World Y used as the sound origin.
         * @param wz World Z used as the sound origin.
         * @param skin_this_frame True to update deformable vertices this frame.
         * @return True when the GPU meshes were re-skinned and uploaded.
         */
        [[nodiscard]] bool update(engine::IEngine& engine,
                                  const MonsterRigGpu& rig,
                                  float dt,
                                  bool moving,
                                  bool running = false,
                                  Audio* audio = nullptr,
                                  float wx = 0.0f,
                                  float wy = 0.0f,
                                  float wz = 0.0f,
                                  bool skin_this_frame = true);

        /** Draws the parts belonging to one render pass.
         * @param engine Engine used to submit the draw calls.
         * @param model World matrix of the monster.
         * @param want_alpha True for the alpha blended pass, false for the opaque
         * pass.
         */
        void draw(engine::IEngine& engine, const engine::Mat4& model, bool want_alpha) const;

        /** Draws the current skinned silhouette projected onto the ground.
         * @param engine Engine used to submit shadow draws.
         * @param model Monster model matrix.
         * @param receiver Terrain height-map receiver, or an empty fallback.
         * @param ground_y Receiver-plane height.
         * @param opacity Shadow opacity in the range 0..1.
         */
        void draw_shadow(engine::IEngine& engine,
                         const engine::Mat4& model,
                         const engine::ShadowReceiver& receiver,
                         float ground_y,
                         float opacity) const;

        /**
         * Sets the Ice mastery cyan ambient (`ForceAmbient` while compatibility behavior).
         * @param v True while the slow is active.
         */
        void set_ice_slowed(bool v) {
            m_ice_slowed = v;
        }

        /** Computes the world-space translation of a skeleton bone, the KalOnline
         * s_vMatrix attach point.
         * @param rig Shared rig providing skeleton and clips.
         * @param model World matrix of the monster.
         * @param bone_index Index of the bone inside the skeleton.
         * @param ox Receives the world X position.
         * @param oy Receives the world Y position.
         * @param oz Receives the world Z position.
         * @return True when the bone could be evaluated.
         */
        [[nodiscard]] bool bone_world(const MonsterRigGpu& rig,
                                      const engine::Mat4& model,
                                      int bone_index,
                                      float& ox,
                                      float& oy,
                                      float& oz) const;

        /**
         * Computes the world matrix of a skeleton bone, the KalOnline s_vMatrix.
         * @param rig Shared rig providing skeleton and clips.
         * @param model World matrix of the monster.
         * @param bone_index Index of the bone inside the skeleton.
         * @param out Receives the column-major world matrix.
         * @return True when the bone could be evaluated.
         */
        [[nodiscard]] bool bone_world_matrix(const MonsterRigGpu& rig,
                                             const engine::Mat4& model,
                                             int bone_index,
                                             engine::Mat4& out) const;

        /** Monster index this instance was created for.
         * @return Monster index.
         */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /** Facing angle of the monster.
         * @return Yaw in radians.
         */
        [[nodiscard]] float yaw() const {
            return m_yaw;
        }

        /** Sets the facing angle of the monster.
         * @param yaw Yaw in radians.
         */
        void set_yaw(float yaw) {
            m_yaw = yaw;
        }

        /** Animation clip currently played.
         * @return Active clip.
         */
        [[nodiscard]] Clip clip() const {
            return m_clip;
        }

      private:
        /** Monster index of the instance. */
        int m_index = 0;

        /** Drawable parts, one per rig bind-pose mesh. */
        std::vector<MonsterPart> m_parts;

        /** compatibility behavior meshes reused as skinning target, allocated once per instance. */
        std::vector<terrain::GbMeshCpu> m_working_meshes;

        /** Bone-world matrices cached at the exact animation sample for skinning and attachments.
         */
        mutable std::vector<std::array<float, 16>> m_bone_matrices;

        /** Skin matrices reused by each animation evaluation. */
        mutable std::vector<std::array<float, 16>> m_skin_matrices;

        /** Identity and sample parameters of the cached pose; a null rig invalidates it. */
        mutable const MonsterRigGpu* m_pose_rig = nullptr;
        mutable const terrain::GbAnimClip* m_pose_clip = nullptr;
        mutable float m_pose_time_ms = 0.0f;
        mutable bool m_pose_loop = false;

        /** Evaluates an exact pose once for all consumers of that sample.
         * @param rig Skeleton owning the clip.
         * @param clip Active animation clip.
         * @param time_ms Exact playback time in milliseconds.
         * @param loop Whether the clip loops.
         * @return True when bone and skin matrices are available.
         */
        [[nodiscard]] bool evaluate_pose(const MonsterRigGpu& rig,
                                         const terrain::GbAnimClip& clip,
                                         float time_ms,
                                         bool loop) const;

        /** Interleaved vertices reused while uploading each skinned part. */
        std::vector<engine::VertexP3T2> m_upload_vertices;

        /** Playback position inside the active clip, in milliseconds. */
        float m_anim_time_ms = 0.0f;

        /** Clip time of the last skinning pass, in milliseconds. */
        float m_last_skinned_time_ms = -1000.0f;

        /** Cleared on each clip change; monster attacks do not ping-pong. */
        bool m_attack_reverse = false;

        /** True while the last die/knee frame is held. */
        bool m_die_hold = false;

        /** Y offset applied after the die clip (negative = into the ground). */
        float m_sink_y = 0.0f;

        /** Seconds spent sinking after the die clip held. */
        float m_sink_sec = 0.0f;

        /** True when the local die/behead sequence has finished. */
        bool m_ready_to_despawn = false;

        /** Active animation clip. */
        Clip m_clip = Clip::Idle;

        /** Action 7 clip number while Clip::Skill is active. */
        int m_skill_motion = 1;

        /** True when the active Action 7 clip presents a regular basic attack. */
        bool m_basic_attack_visual = false;

        /** Facing angle in radians. */
        float m_yaw = 0.0f;

        /** Keyframe SOUND tracker for the clip now playing. */
        AnimSound m_sounds;

        /** Clip whose sounds runtime state is tracking; reset when it changes. */
        Clip m_sound_clip = Clip::Idle;

        /** True once the current attack clip's projectile key has been latched. */
        bool m_attack_fx_bound = false;

        /** True while waiting for the attack clip to reach the projectile key. */
        bool m_await_attack_fx = false;

        /** True when take_attack_fx() should spawn the projectile. */
        bool m_attack_fx_ready = false;

        /** Clip time of the projectile key, in milliseconds. */
        float m_pending_attack_fx_ms = -1.0f;

        /** Parsed projectile key for the current attack. */
        MonsterAttackFx m_pending_attack_fx;

        /** Idle EFFECT_FX names waiting to spawn this clip cycle. */
        std::vector<std::string> m_pending_idle_fx;

        /** Last polled idle clip time in milliseconds; -1 after a clip change. */
        float m_last_idle_fx_time_ms = -1.0f;

        /** Remaining hit-flash duration in milliseconds; 0 when idle. */
        float m_hit_flash_ms = 0.0f;

        /** Total hit-flash duration for the current envelope in milliseconds. */
        float m_hit_flash_duration_ms = 0.0f;

        /** Blend toward white at flash peak (0.9 normal, 1.0 critical). */
        float m_hit_flash_peak = 0.0f;

        /** True while compatibility behavior ice slow tints the model cyan. */
        bool m_ice_slowed = false;

        /** Latched ActionDamage waiting for `(*EFFECT_DAMAGE)`. */
        MonsterActionDamage m_action_damage;

        /** True while ActionDamage has been latched and not yet presented. */
        bool m_action_damage_latched = false;

        /** True when take_action_damage() should present this frame. */
        bool m_action_damage_ready = false;

        /** True once the current attack clip's EFFECT_DAMAGE time has been scanned. */
        bool m_action_damage_bound = false;

        /** Clip time of EFFECT_DAMAGE, or negative when the GB has no key. */
        float m_action_damage_key_ms = -1.0f;

        /** `runtime state` in milliseconds; 0 means no fade. */
        float m_blend_in_timer_ms = 0.0f;

        /** Elapsed `runtime state` in milliseconds. */
        float m_blend_in_time_ms = 0.0f;
    };

    inline bool MonsterInstance::is_effect_damage_option(std::string_view option) {
        return option.find("(*EFFECT_DAMAGE)") != std::string_view::npos;
    }

    inline float
    MonsterInstance::effect_damage_time_ms(const std::vector<std::uint16_t>& key_times,
                                           const std::vector<std::string>& key_options) {
        float best = -1.0f;
        const std::size_t n = std::min(key_times.size(), key_options.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (!is_effect_damage_option(key_options[i])) {
                continue;
            }
            const float t = static_cast<float>(key_times[i]);
            if (best < 0.0f || t < best) {
                best = t;
            }
        }
        return best;
    }

    inline bool MonsterInstance::action_damage_should_fire(bool latched,
                                                           float anim_time_ms,
                                                           float key_ms,
                                                           bool force) {
        if (!latched) {
            return false;
        }
        if (force) {
            return true;
        }
        return key_ms >= 0.0f && anim_time_ms >= key_ms;
    }

    inline float MonsterInstance::blend_opacity_at(float timer_ms, float time_ms) {
        if (timer_ms <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(time_ms / timer_ms, 0.0f, 1.0f);
    }

}  // namespace openkal::client
