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

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Number of EFFECTBONE_* slots in a monsterbone (bone …) list. */
    inline constexpr int kEffectBoneCount = 8;

    /** One looping particle or FX attach from monsterinfo. */
    class MonsterFxAttach {
      public:
        /** @return Particle or FX shader name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the shader name.
         * @param v Shader name.
         */
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** EFFECTBONE index, or -1 to attach at the origin / feet.
         * @return Effect-bone slot.
         */
        [[nodiscard]] int effect_bone() const {
            return m_effect_bone;
        }

        /**
         * Sets the EFFECTBONE index.
         * @param v Effect-bone slot, or -1.
         */
        void set_effect_bone(int v) {
            m_effect_bone = v;
        }

        /** @return World-axis offset X. */
        [[nodiscard]] float ox() const {
            return m_ox;
        }

        /**
         * Sets the world-axis offset X.
         * @param v Offset X.
         */
        void set_ox(float v) {
            m_ox = v;
        }

        /** @return World-axis offset Y. */
        [[nodiscard]] float oy() const {
            return m_oy;
        }

        /**
         * Sets the world-axis offset Y.
         * @param v Offset Y.
         */
        void set_oy(float v) {
            m_oy = v;
        }

        /** @return World-axis offset Z. */
        [[nodiscard]] float oz() const {
            return m_oz;
        }

        /**
         * Sets the world-axis offset Z.
         * @param v Offset Z.
         */
        void set_oz(float v) {
            m_oz = v;
        }

        /**
         * Appends unique `(*EFFECT_FX name)` tokens from a GB key option.
         * `(*EFFECT_FX_NR …)` is skipped. Idle keys that also contain `(*LOOP`
         * use this list as a field attach (Thunder / Ice Storm).
         * @param option Keyframe `runtime state` text.
         * @param out Attach list; existing names are not duplicated.
         */
        static void append_looping_effect_fx(std::string_view option,
                                             std::vector<MonsterFxAttach>& out);

      private:
        /** Particle or FX shader name. */
        std::string m_name;

        /** EFFECTBONE index, or -1 for the origin. */
        int m_effect_bone = -1;

        /** World-axis offset X. */
        float m_ox = 0.0f;

        /** World-axis offset Y. */
        float m_oy = 0.0f;

        /** World-axis offset Z. */
        float m_oz = 0.0f;
    };

    /** One idle-clip `(*EFFECT_FX)` cue (OnAnimationOption oneshot). */
    class MonsterIdleFxCue {
      public:
        /** @return FX shader name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the FX shader name.
         * @param v Shader name.
         */
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** @return Clip time of this key in milliseconds. */
        [[nodiscard]] float time_ms() const {
            return m_time_ms;
        }

        /**
         * Sets the clip time of this key.
         * @param v Time in milliseconds.
         */
        void set_time_ms(float v) {
            m_time_ms = v;
        }

        /**
         * Appends timed idle `(*EFFECT_FX name)` cues from a GB key option.
         * `(*EFFECT_FX_NR …)` is skipped.
         * @param option Keyframe `runtime state` text.
         * @param time_ms Clip time of this key.
         * @param out Cue list.
         */
        static void append_from_option(std::string_view option,
                                       float time_ms,
                                       std::vector<MonsterIdleFxCue>& out);

        /**
         * Classifies one idle GB key. Options that contain `(*LOOP` contribute
         * unique looping field attaches; other `(*EFFECT_FX)` tokens become
         * timed oneshot cues (Fire Storm sequence).
         * @param option Keyframe `runtime state` text.
         * @param time_ms Clip time of this key.
         * @param looping_fx Unique looping attaches.
         * @param cues Timed oneshot cues.
         */
        static void append_idle_gb_key(std::string_view option,
                                       float time_ms,
                                       std::vector<MonsterFxAttach>& looping_fx,
                                       std::vector<MonsterIdleFxCue>& cues);

        /**
         * Classifies a whole idle clip. Bone 903 (Poison Cloud) stores
         * `(*LOOP 0)` and `(*EFFECT_FX …)` on separate keys; Thunder / Ice
         * Storm do the same. Any LOOP on the clip promotes every
         * `(*EFFECT_FX)` to a looping field attach. Clips without LOOP keep
         * timed oneshots (Fire Storm).
         * @param key_options Per-key `runtime state` text.
         * @param key_times Per-key clip times in milliseconds.
         * @param looping_fx Unique looping attaches.
         * @param cues Timed oneshot cues.
         */
        static void append_idle_clip(const std::vector<std::string>& key_options,
                                     const std::vector<std::uint16_t>& key_times,
                                     std::vector<MonsterFxAttach>& looping_fx,
                                     std::vector<MonsterIdleFxCue>& cues);

        /** Appends every one-shot FX cue from a Monster ModelSkill Action 7 clip.
         * Unlike idle clips, skills must also process `(*EFFECT_FX_NR …)`: the
         * suffix means non-repeating, not invisible.
         * @param key_options Per-key `runtime state` text.
         * @param key_times Per-key clip times in milliseconds.
         * @param cues Timed one-shot cues.
         */
        static void append_skill_clip(const std::vector<std::string>& key_options,
                                      const std::vector<std::uint16_t>& key_times,
                                      std::vector<MonsterIdleFxCue>& cues);

      private:
        /** FX shader name. */
        std::string m_name;

        /** Clip time of this key in milliseconds. */
        float m_time_ms = 0.0f;
    };

    /** Hunt-quest drop slot from monster definition / JSON stats.quests. */
    class MonsterQuestSlot {
      public:
        /** Quest index (high 16 bits of CheckQuestFlag key).
         * @return Quest index, 0 when unset.
         */
        [[nodiscard]] std::uint16_t quest_index() const {
            return m_quest_index;
        }

        /**
         * Sets the quest index.
         * @param v Quest index.
         */
        void set_quest_index(std::uint16_t v) {
            m_quest_index = v;
        }

        /** Required quest flag (compared equal; clear must be 0 for the marker).
         * @return Flag byte.
         */
        [[nodiscard]] std::uint8_t flag() const {
            return m_flag;
        }

        /**
         * Sets the required quest flag.
         * @param v Flag byte.
         */
        void set_flag(std::uint8_t v) {
            m_flag = v;
        }

      private:
        /** Quest index from stats.quests[0]. */
        std::uint16_t m_quest_index = 0;

        /** Required flag from stats.quests[1]. */
        std::uint8_t m_flag = 0;
    };

    /** Appearance and gait data of one monster index. */
    class MonsterRig {
      public:
        /** Owned static model path, relative to the resource root.
         * @return Asset path, or empty for animated compatibility rigs.
         */
        const std::string& owned_model() const {
            return m_owned_model;
        }

        /** Sets the owned static model.
         * @param path Resource-relative model path.
         */
        void set_owned_model(const std::string& path) {
            m_owned_model = path;
        }

        /** Skeleton set the monster animates with.
         * @return Bone set number used to resolve T%03d_Bone.gb.
         */
        [[nodiscard]] int bone_no() const {
            return m_bone_no;
        }

        /** Sets the skeleton set the monster animates with.
         * @param bone_no Bone set number.
         */
        void set_bone_no(int bone_no) {
            m_bone_no = bone_no;
        }

        /** Clothes / model index from monsterinfo (key N M). M is the shared
         * clothes GB set when later dungeon variants reuse an earlier model.
         * @return Clothes index, or 0 when the monster index itself is used.
         */
        [[nodiscard]] int clothes_index() const {
            return m_clothes_index;
        }

        /** Sets the clothes / model index.
         * @param clothes_index Shared clothes index, or 0 for the monster index.
         */
        void set_clothes_index(int clothes_index) {
            m_clothes_index = clothes_index;
        }

        /** Motion letter of the idle clip.
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char idle_letter() const {
            return m_idle_letter;
        }

        /** Sets the motion letter of the idle clip.
         * @param letter Uppercase clip letter.
         */
        void set_idle_letter(char letter) {
            m_idle_letter = letter;
        }

        /** Motion letter of the walk clip.
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char walk_letter() const {
            return m_walk_letter;
        }

        /** Sets the motion letter of the walk clip.
         * @param letter Uppercase clip letter.
         */
        void set_walk_letter(char letter) {
            m_walk_letter = letter;
        }

        /** Motion letter of the run clip.
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char run_letter() const {
            return m_run_letter;
        }

        /** Sets the motion letter of the run clip.
         * @param letter Uppercase clip letter.
         */
        void set_run_letter(char letter) {
            m_run_letter = letter;
        }

        /** Motion letter of the attack clip.
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char attack_letter() const {
            return m_attack_letter;
        }

        /** Sets the motion letter of the attack clip.
         * @param letter Uppercase clip letter.
         */
        void set_attack_letter(char letter) {
            m_attack_letter = letter;
        }

        /** Motion letter of Action 7 monster-skill clips.
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char skill_letter() const {
            return m_skill_letter;
        }

        /** Sets the motion letter of Action 7 monster-skill clips.
         * @param letter Uppercase clip letter.
         */
        void set_skill_letter(char letter) {
            m_skill_letter = letter;
        }

        /** Whether this monster has an Action 8 attack motion.
         * @return True when the renderer should load the attack motion.
         */
        [[nodiscard]] bool has_attack_motion() const {
            return m_has_attack_motion;
        }

        /** Sets whether this monster has an Action 8 attack motion.
         * @param has_attack_motion True when the renderer should load the attack motion.
         */
        void set_has_attack_motion(bool has_attack_motion) {
            m_has_attack_motion = has_attack_motion;
        }

        /** Optional Action 7 motion used only to present a basic server attack.
         * @return Motion number, or 0 when the regular Action 8 clip is used.
         */
        [[nodiscard]] int basic_attack_visual_motion() const {
            return m_basic_attack_visual_motion;
        }

        /**
         * Sets the cosmetic Action 7 motion for a regular basic attack.
         * @param motion Action 7 motion number; 0 disables the override.
         */
        void set_basic_attack_visual_motion(int motion) {
            m_basic_attack_visual_motion = motion > 0 ? motion : 0;
        }

        /** Motion letter of the die / open clip (ActionType AT_DIE → 3).
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char die_letter() const {
            return m_die_letter;
        }

        /** Sets the motion letter of the die clip.
         * @param letter Uppercase clip letter.
         */
        void set_die_letter(char letter) {
            m_die_letter = letter;
        }

        /** Motion letter of the behead clip (ActionType AT_BEHEADED → 4).
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char behead_letter() const {
            return m_behead_letter;
        }

        /** Sets the motion letter of the behead clip.
         * @param letter Uppercase clip letter.
         */
        void set_behead_letter(char letter) {
            m_behead_letter = letter;
        }

        /** Motion letter of the kneel clip (ActionType AT_KNEE → 9).
         * @return Uppercase clip letter.
         */
        [[nodiscard]] char knee_letter() const {
            return m_knee_letter;
        }

        /** Sets the motion letter of the kneel clip.
         * @param letter Uppercase clip letter.
         */
        void set_knee_letter(char letter) {
            m_knee_letter = letter;
        }

        /** Uniform model scale applied when drawing the monster.
         * @return Scale factor, 1.0 when unscaled.
         */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /** Sets the uniform model scale.
         * @param scale Scale factor.
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** Monsterinfo levelno used by GetMonsterNameColor.
         * @return Monster level, 1 when unknown.
         */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /** Sets the monsterinfo levelno.
         * @param level Monster level.
         */
        void set_level(int level) {
            m_level = level;
        }

        /** Y offset added above EFFECTBONE_HEAD, monsterinfo scale[1].
         * @return World-space Y offset.
         */
        [[nodiscard]] float name_offset_y() const {
            return m_name_offset_y;
        }

        /** Sets the name Y offset.
         * @param offset World-space Y offset.
         */
        void set_name_offset_y(float offset) {
            m_name_offset_y = offset;
        }

        /** Walk movement tick, the first macro mspeed value.
         * @return Milliseconds per movement step while walking.
         */
        [[nodiscard]] float walk_tick() const {
            return m_walk_tick;
        }

        /** Sets the walk movement tick.
         * @param tick Milliseconds per movement step while walking.
         */
        void set_walk_tick(float tick) {
            m_walk_tick = tick;
        }

        /** Run movement tick, the second macro mspeed value.
         * @return Milliseconds per movement step while running.
         */
        [[nodiscard]] float run_tick() const {
            return m_run_tick;
        }

        /** Sets the run movement tick.
         * @param tick Milliseconds per movement step while running.
         */
        void set_run_tick(float tick) {
            m_run_tick = tick;
        }

        /** Looping particle attaches from monsterinfo (particle …).
         * @return Particle list in script order.
         */
        [[nodiscard]] const std::vector<MonsterFxAttach>& particles() const {
            return m_particles;
        }

        /**
         * Replaces the looping particle list.
         * @param v Particle attaches.
         */
        void set_particles(std::vector<MonsterFxAttach> v) {
            m_particles = std::move(v);
        }

        /** Looping FX attaches from monsterinfo (fx …).
         * @return FX list in script order.
         */
        [[nodiscard]] const std::vector<MonsterFxAttach>& fx() const {
            return m_fx;
        }

        /**
         * Replaces the looping FX list.
         * @param v FX attaches.
         */
        void set_fx(std::vector<MonsterFxAttach> v) {
            m_fx = std::move(v);
        }

        /** Hunt-quest slots from JSON stats.quests (quest, flag, itemgroup, drop).
         * @return Slots in script order.
         */
        [[nodiscard]] const std::vector<MonsterQuestSlot>& quest_slots() const {
            return m_quest_slots;
        }

        /**
         * Replaces the hunt-quest slot list.
         * @param v Quest slots.
         */
        void set_quest_slots(std::vector<MonsterQuestSlot> v) {
            m_quest_slots = std::move(v);
        }

      private:
        std::string m_owned_model;

        /** Bone set number (monsterinfo bone). */
        int m_bone_no = 1;

        /** Shared clothes GB index from monsterinfo (key N M). 0 uses the monster
         * index. */
        int m_clothes_index = 0;

        /** Idle clip letter. */
        char m_idle_letter = 'A';

        /** Walk clip letter. */
        char m_walk_letter = 'A';

        /** Run clip letter. */
        char m_run_letter = 'A';

        /** Attack clip letter. */
        char m_attack_letter = 'A';

        /** Monster skill clip letter (Action 7 / GO_SKILL). */
        char m_skill_letter = 'A';

        /** Whether the monster supplies an Action 8 attack clip. */
        bool m_has_attack_motion = true;

        /** Optional Action 7 motion presented for a regular basic attack. */
        int m_basic_attack_visual_motion = 0;

        /** Die / open clip letter (action 3). */
        char m_die_letter = 'A';

        /** Behead clip letter (action 4). */
        char m_behead_letter = 'A';

        /** Kneel clip letter (action 9). */
        char m_knee_letter = 'A';

        /** Uniform model scale. */
        float m_scale = 1.0f;

        /** Monsterinfo levelno. */
        int m_level = 1;

        /** monsterinfo scale[1], added above the head bone. */
        float m_name_offset_y = 5.0f;

        /** Walk movement tick, macro mspeed first value. */
        float m_walk_tick = 1600.0f;

        /** Run movement tick, macro mspeed second value. */
        float m_run_tick = 800.0f;

        /** monsterinfo (particle name effectbone ox oy oz) list. */
        std::vector<MonsterFxAttach> m_particles;

        /** monsterinfo (fx name effectbone ox oy oz) list. */
        std::vector<MonsterFxAttach> m_fx;

        /** Hunt-quest drop slots from stats.quests. */
        std::vector<MonsterQuestSlot> m_quest_slots;
    };

    /** Effect attach points of one skeleton set.
     * Keyed by the monsterinfo bone number; EFFECTBONE_SPINE1 (bone[1]) is the
     * lightning / skill impact attach point.
     */
    class MonsterBoneInfo {
      public:
        /** EFFECTBONE_EFFECT bone index.
         * @return Bone index inside the skeleton.
         */
        [[nodiscard]] int effect() const {
            return skeleton_bone(0);
        }

        /** Sets the EFFECTBONE_EFFECT bone index.
         * @param effect Bone index inside the skeleton.
         */
        void set_effect(int effect) {
            set_skeleton_bone(0, effect);
        }

        /** EFFECTBONE_SPINE1 bone index used as skill impact attach point.
         * @return Bone index inside the skeleton.
         */
        [[nodiscard]] int spine1() const {
            return skeleton_bone(1);
        }

        /** Sets the EFFECTBONE_SPINE1 bone index.
         * @param spine1 Bone index inside the skeleton.
         */
        void set_spine1(int spine1) {
            set_skeleton_bone(1, spine1);
        }

        /** EFFECTBONE_HEAD bone index.
         * @return Bone index inside the skeleton.
         */
        [[nodiscard]] int head() const {
            return skeleton_bone(2);
        }

        /** Sets the EFFECTBONE_HEAD bone index.
         * @param head Bone index inside the skeleton.
         */
        void set_head(int head) {
            set_skeleton_bone(2, head);
        }

        /**
         * Maps an EFFECTBONE slot to a skeleton bone index.
         * @param effect_bone EFFECTBONE_* index, or -1.
         * @return Skeleton bone index, or -1 when unknown / origin.
         */
        [[nodiscard]] int skeleton_bone(int effect_bone) const {
            if (effect_bone < 0 || effect_bone >= kEffectBoneCount) {
                return -1;
            }
            return m_bones[static_cast<std::size_t>(effect_bone)];
        }

        /**
         * Sets one EFFECTBONE slot.
         * @param effect_bone EFFECTBONE_* index.
         * @param skeleton_index Bone index inside the skeleton.
         */
        void set_skeleton_bone(int effect_bone, int skeleton_index) {
            if (effect_bone < 0 || effect_bone >= kEffectBoneCount) {
                return;
            }
            m_bones[static_cast<std::size_t>(effect_bone)] = skeleton_index;
        }

        /** Target height, the macro target[3] value used as AddTarget scale
         * numerator.
         * @return Height in world units.
         */
        [[nodiscard]] float target_height() const {
            return m_target_height;
        }

        /** Sets the target height.
         * @param height Height in world units.
         */
        void set_target_height(float height) {
            m_target_height = height;
        }

      private:
        /** Skeleton indices keyed by EFFECTBONE_* (0 effect, 1 spine1, 2 head, …). */
        std::array<int, kEffectBoneCount> m_bones{};

        /** Macro target[3] height. */
        float m_target_height = 17.0f;
    };

    /** Lookup table of monster appearance rigs and skeleton effect bones. */
    class MonsterCatalog {
      public:
        /** Loads JSON files from config/monsters, reading appearance and
         * hunt-quest slots.
         * @param dir Directory holding the per-monster json files and bones.json.
         * @return True when at least one rig or bone entry was parsed.
         */
        [[nodiscard]] bool load_json_dir(const std::string& dir);

        /** Looks up the appearance rig of a monster index.
         * @param index Monster index.
         * @return Pointer to the rig, or nullptr when unknown.
         */
        [[nodiscard]] const MonsterRig* find_rig(int index) const;

        /** Looks up the effect bones of a skeleton set.
         * @param bone_no Bone set number.
         * @return Pointer to the bone info, or nullptr when unknown.
         */
        [[nodiscard]] const MonsterBoneInfo* find_bone(int bone_no) const;

        /** Collects the clothes .gb files of a monster. Body (_B*) parts B1–B8 are
         * included when present; heads are _H1 and _H3+ (H2 is the beheaded mesh).
         * @param data_root Root directory of the client data files.
         * @param index Monster index.
         * @param beheaded True to pick the detached head part instead of the normal
         * one.
         * @return Existing clothes file paths in load order.
         */
        [[nodiscard]] static std::vector<std::string>
        clothes_gb_paths(const std::string& data_root, int index, bool beheaded = false);

        /** Resolves the skeleton .gb file of a bone set.
         * @param data_root Root directory of the client data files.
         * @param bone_no Bone set number.
         * @return Existing file path, or an empty string when missing.
         */
        [[nodiscard]] static std::string bone_gb(const std::string& data_root, int bone_no);

        /** Resolves a motion .gb file, falling back to the 'A' variant of the action.
         * @param data_root Root directory of the client data files.
         * @param bone_no Bone set number.
         * @param action Macro action id (0 idle, 1 walk, 2 run, 7 skill, 8 attack).
         * @param letter Preferred clip letter.
         * @param variant One-based animation variant; 0 falls back to variant 1.
         * @return Existing file path, or an empty string when missing.
         */
        [[nodiscard]] static std::string motion_gb(
            const std::string& data_root, int bone_no, int action, char letter, int variant = 1);

      private:
        /** Appearance rigs keyed by monster index. */
        std::unordered_map<int, MonsterRig> m_rigs;

        /** Effect bones keyed by bone set number. */
        std::unordered_map<int, MonsterBoneInfo> m_bones;
    };

}  // namespace openkal::client
