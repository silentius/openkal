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

#include "openkal/client/MonsterCatalog.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openkal::platform {
    class Json;
}

namespace openkal::client {

    /** One GenNPC record: display name, where an NPC stands, and which way it
     * looks. */
    class NpcSpawn {
      public:
        /** @return GenNPC index. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /**
         * Sets the GenNPC index.
         * @param v Index.
         */
        void set_index(int v) {
            m_index = v;
        }

        /** @return Display name from the NPC JSON record. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param v NPC name.
         */
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** @return Optional UI bitmap drawn above this NPC's overhead name. */
        const std::string& overhead_icon() const {
            return m_overhead_icon;
        }

        /** Sets the optional overhead symbol. @param file UI bitmap path, empty to disable. */
        void set_overhead_icon(std::string file) {
            m_overhead_icon = std::move(file);
        }

        /** Resolves the per-NPC name height above its head anchor.
         * @param fallback Rig offset when the NPC has no override.
         * @return Vertical offset in client world units.
         */
        float name_offset_y(float fallback) const {
            return m_name_offset_y.value_or(fallback);
        }

        /** Overrides the overhead name height for this NPC.
         * @param offset Vertical offset in client world units.
         */
        void set_name_offset_y(float offset) {
            m_name_offset_y = offset;
        }

        /** @return Upward screen offset for the name and symbol at native UI scale. */
        float name_offset_pixels() const {
            return m_name_offset_pixels;
        }

        /** Sets a screen-space adjustment after projecting the NPC head anchor.
         * @param pixels Upward offset in pixels at native UI scale.
         */
        void set_name_offset_pixels(float pixels) {
            m_name_offset_pixels = pixels;
        }

        /** @return Appearance shape, resolving Data/Npc/Clothes/N{shape:03d}_*.gb. */
        [[nodiscard]] int shape() const {
            return m_shape;
        }

        /**
         * Sets the appearance shape.
         * @param v Shape number.
         */
        void set_shape(int v) {
            m_shape = v;
        }

        /** @return Stand position X, in server pixels. */
        [[nodiscard]] int server_x() const {
            return m_server_x;
        }

        /**
         * Sets the stand position X.
         * @param v Server pixel X.
         */
        void set_server_x(int v) {
            m_server_x = v;
        }

        /** @return Stand position Y, in server pixels. */
        [[nodiscard]] int server_y() const {
            return m_server_y;
        }

        /**
         * Sets the stand position Y.
         * @param v Server pixel Y.
         */
        void set_server_y(int v) {
            m_server_y = v;
        }

        /** @return Stand height, in server units. */
        [[nodiscard]] int server_z() const {
            return m_server_z;
        }

        /**
         * Sets the stand height.
         * @param v Server height.
         */
        void set_server_z(int v) {
            m_server_z = v;
        }

        /** @return Look-at point X, in server pixels. */
        [[nodiscard]] int dir_x() const {
            return m_dir_x;
        }

        /**
         * Sets the look-at point X.
         * @param v Server pixel X.
         */
        void set_dir_x(int v) {
            m_dir_x = v;
        }

        /** @return Look-at point Y, in server pixels. */
        [[nodiscard]] int dir_y() const {
            return m_dir_y;
        }

        /**
         * Sets the look-at point Y.
         * @param v Server pixel Y.
         */
        void set_dir_y(int v) {
            m_dir_y = v;
        }

        /** @return NPC kind (3 = quest). */
        [[nodiscard]] int kind() const {
            return m_kind;
        }

        /**
         * Sets the NPC kind.
         * @param v Kind value.
         */
        void set_kind(int v) {
            m_kind = v;
        }

        /** @return Logical map containing this NPC. */
        [[nodiscard]] int map() const {
            return m_map;
        }

        /**
         * Sets the logical map containing this NPC.
         * @param v Logical map index.
         */
        void set_map(int v) {
            m_map = v;
        }

        /** @return Scenario slot, or -1 when this NPC is not a scenario object. */
        [[nodiscard]] int scenario_slot() const {
            return m_scenario_slot;
        }

        /**
         * Sets the optional scenario slot.
         * @param v Scenario slot, or -1 when absent.
         */
        void set_scenario_slot(int v) {
            m_scenario_slot = v;
        }

        /** @return True when this spawn has a quest key. */
        [[nodiscard]] bool has_quest() const {
            return m_has_quest;
        }

        /** @return Quest index from the NPC JSON `quest` field. */
        [[nodiscard]] std::uint16_t quest_index() const {
            return m_quest_index;
        }

        /** @return Quest flag from the NPC JSON `quest` field. */
        [[nodiscard]] std::uint8_t quest_flag() const {
            return m_quest_flag;
        }

        /**
         * Sets the optional quest key.
         * @param index Quest index.
         * @param flag Step flag.
         */
        void set_quest(std::uint16_t index, std::uint8_t flag) {
            m_has_quest = true;
            m_quest_index = index;
            m_quest_flag = flag;
        }

      private:
        /** GenNPC index. */
        int m_index = 0;

        /** Display name. */
        std::string m_name;
        /** Optional overhead symbol in the UI texture namespace. */
        std::string m_overhead_icon;
        /** Per-spawn override; NPCs sharing a shape may use different name heights. */
        std::optional<float> m_name_offset_y;
        /** Screen-space adjustment independent of camera distance. */
        float m_name_offset_pixels = 0.f;

        /** Appearance shape. */
        int m_shape = 0;

        /** Stand position X in server pixels. */
        int m_server_x = 0;

        /** Stand position Y in server pixels. */
        int m_server_y = 0;

        /** Stand height in server units. */
        int m_server_z = 0;

        /** Look-at point X in server pixels. */
        int m_dir_x = 0;

        /** Look-at point Y in server pixels. */
        int m_dir_y = 0;

        /** NPC kind (3 = quest). */
        int m_kind = 0;

        /** Logical map containing this NPC. */
        int m_map = 0;

        /** Scenario slot from NPC JSON, or -1 when absent. */
        int m_scenario_slot = -1;

        /** True when the JSON listed a quest key. */
        bool m_has_quest = false;

        /** Quest index from `quest`. */
        std::uint16_t m_quest_index = 0;

        /** Quest flag from `quest`. */
        std::uint8_t m_quest_flag = 0;
    };

    /** Skeleton and idle clip of one NPC shape. */
    class NpcRig {
      public:
        /** @return Bone set number used to resolve T%03d_bone.gb. */
        [[nodiscard]] int bone_no() const {
            return m_bone_no;
        }

        /**
         * Sets the bone set number.
         * @param v Bone set number.
         */
        void set_bone_no(int v) {
            m_bone_no = v;
        }

        /** @return Uppercase motion letter of the idle clip. */
        [[nodiscard]] char idle_letter() const {
            return m_idle_letter;
        }

        /**
         * Sets the motion letter of the idle clip.
         * @param v Uppercase clip letter.
         */
        void set_idle_letter(char v) {
            m_idle_letter = v;
        }

        /** Uniform model scale from npcinfo scale[0].
         * @return Scale factor, 1.0 when unscaled.
         */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the uniform model scale.
         * @param v Scale factor.
         */
        void set_scale(float v) {
            m_scale = v;
        }

        /** Y offset added above EFFECTBONE_HEAD, npcinfo scale[1].
         * @return World-space Y offset.
         */
        [[nodiscard]] float name_offset_y() const {
            return m_name_offset_y;
        }

        /**
         * Sets the name Y offset.
         * @param v World-space Y offset.
         */
        void set_name_offset_y(float v) {
            m_name_offset_y = v;
        }

        /** Whether npcinfo allows an overhead name (false when (name 0)).
         * @return True when the name should be shown.
         */
        [[nodiscard]] bool show_name() const {
            return m_show_name;
        }

        /**
         * Sets whether the overhead name is enabled.
         * @param v True to show the name.
         */
        void set_show_name(bool v) {
            m_show_name = v;
        }

        /** Looping particle attaches from npcinfo (particle …).
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

        /** Looping FX attaches from npcinfo (fx …).
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

      private:
        /** Bone set number (npcinfo bone). */
        int m_bone_no = 0;

        /** Idle clip letter (first character of the npcinfo motion string). */
        char m_idle_letter = 'A';

        /** npcinfo scale[0]. */
        float m_scale = 1.0f;

        /** npcinfo scale[1], added above the head bone. */
        float m_name_offset_y = 5.0f;

        /** False when npcinfo (name 0) hid the label. */
        bool m_show_name = true;

        /** npcinfo (particle name effectbone ox oy oz) list. */
        std::vector<MonsterFxAttach> m_particles;

        /** npcinfo (fx name effectbone ox oy oz) list. */
        std::vector<MonsterFxAttach> m_fx;
    };

    /** Effect attach points of one NPC skeleton set (config/npcs/bones.json). */
    class NpcBoneInfo {
      public:
        /** EFFECTBONE_HEAD bone index.
         * @return Bone index inside the skeleton.
         */
        [[nodiscard]] int head() const {
            return m_head;
        }

        /**
         * Sets the EFFECTBONE_HEAD bone index.
         * @param head Bone index inside the skeleton.
         */
        void set_head(int head) {
            m_head = head;
        }

        /** Target height, the macro target[3] value.
         * @return Height in world units.
         */
        [[nodiscard]] float target_height() const {
            return m_target_height;
        }

        /**
         * Sets the target height.
         * @param height Height in world units.
         */
        void set_target_height(float height) {
            m_target_height = height;
        }

      private:
        /** EFFECTBONE_HEAD bone index. */
        int m_head = 0;

        /** Macro target[3] height. */
        float m_target_height = 17.0f;
    };

    /** Static NPC spawn list and appearance rigs, parsed from JSON.
     */
    class NpcCatalog {
      public:
        /** Reads a rig from a definition's appearance object.
         * @param appearance JSON appearance data.
         * @return The parsed rig with default values for omitted fields.
         */
        [[nodiscard]] static NpcRig read_appearance(const platform::Json& appearance);

        /** Loads JSON files from config/npcs (spawns, names, and appearance).
         * @param dir Directory holding the per-NPC json files.
         * @return True when at least one spawn was parsed.
         */
        [[nodiscard]] bool load_json_dir(const std::string& dir);

        /** @return Every parsed spawn. */
        [[nodiscard]] const std::vector<NpcSpawn>& spawns() const {
            return m_spawns;
        }

        /**
         * Looks up the rig of an NPC shape.
         * @param shape Appearance shape.
         * @return Pointer to the rig, or nullptr when unknown.
         */
        [[nodiscard]] const NpcRig* find_rig(int shape) const;

        /**
         * Looks up the effect bones of a skeleton set.
         * @param bone_no Bone set number.
         * @return Pointer to the bone info, or nullptr when unknown.
         */
        [[nodiscard]] const NpcBoneInfo* find_bone(int bone_no) const;

        /**
         * Computes the facing angle of a spawn, matching Kal
         * compatibility behavior and CalcRotateYawDir.
         * @param origin_patch Map origin patch used for server/world pixel
         * conversion.
         * @param server_x Stand position X in server pixels.
         * @param server_y Stand position Y in server pixels.
         * @param dir_x Look-at point X in server pixels.
         * @param dir_y Look-at point Y in server pixels.
         * @return Yaw in radians.
         */
        [[nodiscard]] static float
        yaw_from_dir(int origin_patch, int server_x, int server_y, int dir_x, int dir_y);

        /**
         * Collects the client clothes meshes of a shape under Data/Npc/Clothes
         * (N{shape:03d}_B*.gb / _P*.gb, any part index, either letter case).
         * @param data_root Client data root.
         * @param shape Appearance shape.
         * @return Existing .gb paths, in load order.
         */
        [[nodiscard]] static std::vector<std::string> clothes_gb_paths(const std::string& data_root,
                                                                       int shape);

        /**
         * Resolves the skeleton .gb of a bone set.
         * @param data_root Client data root.
         * @param bone_no Bone set number.
         * @return Existing file path, or an empty string when missing.
         */
        [[nodiscard]] static std::string bone_gb(const std::string& data_root, int bone_no);

        /**
         * Resolves an idle motion .gb, falling back to the 'A' variant.
         * @param data_root Client data root.
         * @param bone_no Bone set number.
         * @param letter Preferred clip letter.
         * @return Existing file path, or an empty string when missing.
         */
        [[nodiscard]] static std::string
        idle_gb(const std::string& data_root, int bone_no, char letter);

      private:
        /** Parsed spawns, in file order. */
        std::vector<NpcSpawn> m_spawns;

        /** Appearance rigs keyed by shape. */
        std::unordered_map<int, NpcRig> m_rigs;

        /** Effect bones keyed by bone set number. */
        std::unordered_map<int, NpcBoneInfo> m_bones;
    };

}  // namespace openkal::client
