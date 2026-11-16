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
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::skill_effect_view {

    /** Left-list mode: skills, .gb models, particle shaders, FX shaders, or mobs. */
    enum class BrowseMode {
        Skills = 0,
        Models = 1,
        Sprite = 2,
        Fx = 3,
        Mobs = 4,
    };

    /** Number of BrowseMode values. */
    inline constexpr int kBrowseModeCount = 5;

    /** One .gb discovered under data/Skill or data/Effect. */
    class AssetEntry {
      public:
        /** @return Folder name ("Skill" or "Effect"). */
        [[nodiscard]] const std::string& folder() const {
            return m_folder;
        }

        /**
         * Sets the source folder.
         * @param value Folder name.
         */
        void set_folder(std::string value);

        /** @return File stem (Skill_R_3_1). */
        [[nodiscard]] const std::string& stem() const {
            return m_stem;
        }

        /**
         * Sets the file stem.
         * @param value Stem without extension.
         */
        void set_stem(std::string value);

        /** @return Path relative to the folder, using forward slashes. */
        [[nodiscard]] const std::string& relative_path() const {
            return m_relative_path;
        }

        /**
         * Sets the relative path.
         * @param value Path relative to Skill/ or Effect/.
         */
        void set_relative_path(std::string value);

        /** @return Absolute .gb path. */
        [[nodiscard]] const std::string& absolute_path() const {
            return m_absolute_path;
        }

        /**
         * Sets the absolute path.
         * @param value Absolute .gb path.
         */
        void set_absolute_path(std::string value);

        /** @return Skill display names that reference this stem. */
        [[nodiscard]] const std::vector<std::string>& used_by() const {
            return m_used_by;
        }

        /**
         * Records a skill that names this stem in its effect array.
         * @param skill_name Skill display name.
         */
        void add_used_by(std::string skill_name);

        /** Clears reverse skill links before a reload. */
        void clear_used_by();

      private:
        /** Source folder under the client data root. */
        std::string m_folder;

        /** File stem shown in the list. */
        std::string m_stem;

        /** Path relative to the folder. */
        std::string m_relative_path;

        /** Absolute .gb path. */
        std::string m_absolute_path;

        /** Skills whose JSON `effect` array names this stem. */
        std::vector<std::string> m_used_by;
    };

    /** One skill JSON row plus the three gubun effect stems. */
    class SkillRow {
      public:
        /** @return Owning class (0 knight / 1 magician / 2 archer / 3 thief). */
        [[nodiscard]] int clazz() const {
            return m_clazz;
        }

        /**
         * Sets the owning class.
         * @param value Class index.
         */
        void set_clazz(int value) {
            m_clazz = value;
        }

        /** @return Skill index within the class. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /**
         * Sets the skill index.
         * @param value Skill index.
         */
        void set_index(int value) {
            m_index = value;
        }

        /** @return Display name from JSON. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param value Skill name.
         */
        void set_name(std::string value);

        /** @return jobsystem role ("mage" / "knight" / ...). */
        [[nodiscard]] const std::string& role() const {
            return m_role;
        }

        /**
         * Sets the role.
         * @param value Role string.
         */
        void set_role(std::string value);

        /** @return jobsystem key ("mage-3"). */
        [[nodiscard]] const std::string& key() const {
            return m_key;
        }

        /**
         * Sets the jobsystem key.
         * @param value Key string.
         */
        void set_key(std::string value);

        /**
         * Effect stem for one gubun slot.
         * @param gubun 0 prep / 1 fire / 2 hit.
         * @return Stem such as "Skill_R_3_1", or empty.
         */
        [[nodiscard]] const std::string& effect(int gubun) const;

        /**
         * Sets one gubun effect stem.
         * @param gubun 0 prep / 1 fire / 2 hit.
         * @param value Stem, or empty.
         */
        void set_effect(int gubun, std::string value);

        /** @return True when at least one gubun names a stem. */
        [[nodiscard]] bool has_named_effect() const;

        /** @return "role index  name" list label. */
        [[nodiscard]] std::string list_label() const;

      private:
        /** Owning class. */
        int m_clazz = 0;

        /** Skill index within the class. */
        int m_index = 0;

        /** Display name. */
        std::string m_name;

        /** jobsystem role. */
        std::string m_role;

        /** jobsystem key. */
        std::string m_key;

        /** Gubun stems from JSON `effect` (prep / fire / hit). */
        std::array<std::string, 3> m_effects{};
    };

    /** One looping attach from monster or NPC JSON. */
    class AttachRow {
      public:
        /** @return Particle or FX shader name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the shader name.
         * @param value Shader name.
         */
        void set_name(std::string value);

        /** @return EFFECTBONE index, or -1. */
        [[nodiscard]] int effect_bone() const {
            return m_effect_bone;
        }

        /**
         * Sets the EFFECTBONE index.
         * @param value Effect-bone slot, or -1.
         */
        void set_effect_bone(int value) {
            m_effect_bone = value;
        }

        /** @return Offset X. */
        [[nodiscard]] float ox() const {
            return m_ox;
        }

        /**
         * Sets offset X.
         * @param value Offset X.
         */
        void set_ox(float value) {
            m_ox = value;
        }

        /** @return Offset Y. */
        [[nodiscard]] float oy() const {
            return m_oy;
        }

        /**
         * Sets offset Y.
         * @param value Offset Y.
         */
        void set_oy(float value) {
            m_oy = value;
        }

        /** @return Offset Z. */
        [[nodiscard]] float oz() const {
            return m_oz;
        }

        /**
         * Sets offset Z.
         * @param value Offset Z.
         */
        void set_oz(float value) {
            m_oz = value;
        }

      private:
        /** Particle or FX shader name. */
        std::string m_name;

        /** EFFECTBONE index, or -1. */
        int m_effect_bone = -1;

        /** Offset X. */
        float m_ox = 0.0f;

        /** Offset Y. */
        float m_oy = 0.0f;

        /** Offset Z. */
        float m_oz = 0.0f;
    };

    /** One Effect/Sprite or Effect/Fx shader name. */
    class ScriptRow {
      public:
        /** @return Shader name as written in the script. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the shader name.
         * @param value Shader name.
         */
        void set_name(std::string value);

        /** @return Monster/NPC labels that attach this shader. */
        [[nodiscard]] const std::vector<std::string>& used_by() const {
            return m_used_by;
        }

        /**
         * Records a monster or NPC that attaches this shader.
         * @param label List label of the user.
         */
        void add_used_by(std::string label);

        /** Clears reverse links before a reload. */
        void clear_used_by();

        /** @return Name plus first user, for the left list. */
        [[nodiscard]] std::string list_label() const;

      private:
        /** Shader name. */
        std::string m_name;

        /** Monsters/NPCs that attach this shader. */
        std::vector<std::string> m_used_by;
    };

    /** One monster or NPC that attaches looping particles or FX. */
    class MobRow {
      public:
        /** @return True when this row is an NPC. */
        [[nodiscard]] bool npc() const {
            return m_npc;
        }

        /**
         * Sets whether this row is an NPC.
         * @param value True for NPCs.
         */
        void set_npc(bool value) {
            m_npc = value;
        }

        /** @return Monster or NPC index. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /**
         * Sets the index.
         * @param value Index.
         */
        void set_index(int value) {
            m_index = value;
        }

        /** @return Display name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param value Name.
         */
        void set_name(std::string value);

        /** @return Particle attaches. */
        [[nodiscard]] const std::vector<AttachRow>& particles() const {
            return m_particles;
        }

        /**
         * Stores particle attaches.
         * @param value Particle list.
         */
        void set_particles(std::vector<AttachRow> value);

        /** @return FX attaches. */
        [[nodiscard]] const std::vector<AttachRow>& fx() const {
            return m_fx;
        }

        /**
         * Stores FX attaches.
         * @param value FX list.
         */
        void set_fx(std::vector<AttachRow> value);

        /** @return True when at least one attach is present. */
        [[nodiscard]] bool has_attach() const;

        /** @return "m 29  Castoff Baby Ghost" list label. */
        [[nodiscard]] std::string list_label() const;

      private:
        /** True when this row is an NPC. */
        bool m_npc = false;

        /** Monster or NPC index. */
        int m_index = 0;

        /** Display name. */
        std::string m_name;

        /** Particle attaches. */
        std::vector<AttachRow> m_particles;

        /** FX attaches. */
        std::vector<AttachRow> m_fx;
    };

    /**
     * Indexes data/Skill + data/Effect .gb files and joins them to config/skills
     * `effect` names, plus Effect/Sprite, Effect/Fx, and monster/NPC attaches.
     */
    class EffectCatalog {
      public:
        /**
         * Scans Skill/ and Effect/ under the client data root.
         * @param data_root Client data directory.
         * @return Number of .gb files found.
         */
        std::size_t scan_assets(const std::string& data_root);

        /**
         * Loads skill JSON and attaches effect stems to matching assets.
         * @param skills_dir config/skills directory.
         * @return Number of skill rows with at least one named effect.
         */
        std::size_t load_skills(const std::string& skills_dir);

        /**
         * Replaces the Sprite and Fx lists with names from the client Effect loader.
         * @param sprites Particle shader names.
         * @param fxs FX shader names.
         */
        void set_scripts(std::vector<std::string> sprites, std::vector<std::string> fxs);

        /**
         * Loads monster and NPC JSON attaches and binds them onto Sprite/Fx rows.
         * @param monsters_dir config/monsters directory.
         * @param npcs_dir config/npcs directory.
         * @return Number of mob rows with at least one attach.
         */
        std::size_t load_users(const std::string& monsters_dir, const std::string& npcs_dir);

        /**
         * Restricts visible() to labels containing the substring (case-insensitive).
         * @param query Filter text; empty shows every row of the current mode.
         */
        void set_filter(std::string query);

        /**
         * Switches the left list between skills and raw assets.
         * @param mode List mode.
         */
        void set_mode(BrowseMode mode);

        /** @return Current list mode. */
        [[nodiscard]] BrowseMode mode() const {
            return m_mode;
        }

        /** @return Current filter text. */
        [[nodiscard]] const std::string& filter() const {
            return m_filter;
        }

        /** @return Indexed .gb files. */
        [[nodiscard]] const std::vector<AssetEntry>& assets() const {
            return m_assets;
        }

        /** @return Skills that name at least one effect stem. */
        [[nodiscard]] const std::vector<SkillRow>& skills() const {
            return m_skills;
        }

        /** @return Particle shaders from Effect/Sprite. */
        [[nodiscard]] const std::vector<ScriptRow>& sprites() const {
            return m_sprites;
        }

        /** @return FX shaders from Effect/Fx. */
        [[nodiscard]] const std::vector<ScriptRow>& fxs() const {
            return m_fxs;
        }

        /** @return Monsters and NPCs that attach looping effects. */
        [[nodiscard]] const std::vector<MobRow>& mobs() const {
            return m_mobs;
        }

        /** @return Indices into skills() or assets() matching the filter. */
        [[nodiscard]] const std::vector<int>& visible() const {
            return m_visible;
        }

        /** @return Number of rows matching the filter. */
        [[nodiscard]] std::size_t visible_count() const {
            return m_visible.size();
        }

        /**
         * Looks up a visible skill row.
         * @param visible_index Index into visible().
         * @return Matching skill.
         */
        [[nodiscard]] const SkillRow& visible_skill(int visible_index) const;

        /**
         * Looks up a visible asset row.
         * @param visible_index Index into visible().
         * @return Matching asset.
         */
        [[nodiscard]] const AssetEntry& visible_asset(int visible_index) const;

        /**
         * Looks up a visible Sprite or Fx row.
         * @param visible_index Index into visible().
         * @return Matching script.
         */
        [[nodiscard]] const ScriptRow& visible_script(int visible_index) const;

        /**
         * Looks up a visible monster/NPC row.
         * @param visible_index Index into visible().
         * @return Matching mob.
         */
        [[nodiscard]] const MobRow& visible_mob(int visible_index) const;

        /**
         * Resolves a JSON effect stem to a scanned .gb (Skill first, then Effect).
         * @param stem Effect name without extension.
         * @return Matching asset, or nullptr when the file is missing.
         */
        [[nodiscard]] const AssetEntry* find_asset(std::string_view stem) const;

        /**
         * Short label for a gubun slot.
         * @param gubun 0 prep / 1 fire / 2 hit.
         * @return Slot name.
         */
        [[nodiscard]] static const char* slot_name(int gubun);

        /**
         * Short label for a browse tab.
         * @param mode List mode.
         * @return Tab caption.
         */
        [[nodiscard]] static const char* mode_name(BrowseMode mode);

        /**
         * Number of rows in the active list before filtering.
         * @return Unfiltered count for the current mode.
         */
        [[nodiscard]] int mode_total() const;

      private:
        /** Rebuilds runtime state from runtime state and runtime state. */
        void rebuild_visible();

        /** Attaches skill names onto assets they reference. */
        void bind_skill_uses();

        /** Attaches monster/NPC labels onto Sprite and Fx rows. */
        void bind_script_uses();

        /**
         * Records a shader user on the Sprite or Fx list.
         * @param name Shader name.
         * @param fx True to look in Fx, false for Sprite.
         * @param label User list label.
         */
        void add_script_use(std::string_view name, bool fx, const std::string& label);

        /**
         * Loads attach-bearing JSON files from one config folder.
         * @param dir config/monsters or config/npcs.
         * @param npc True when loading NPCs.
         */
        void load_mob_dir(const std::string& dir, bool npc);

        /**
         * Indexes every .gb under one data folder.
         * @param folder_dir Absolute Skill/ or Effect/ path.
         * @param folder_name "Skill" or "Effect".
         */
        void scan_folder(const std::string& folder_dir, const std::string& folder_name);

        /** Indexed .gb files. */
        std::vector<AssetEntry> m_assets;

        /** Skills with at least one named effect. */
        std::vector<SkillRow> m_skills;

        /** Particle shaders from Effect/Sprite. */
        std::vector<ScriptRow> m_sprites;

        /** FX shaders from Effect/Fx. */
        std::vector<ScriptRow> m_fxs;

        /** Monsters and NPCs with looping attaches. */
        std::vector<MobRow> m_mobs;

        /** Lowercased stem → index into runtime state (first Skill, then Effect). */
        std::unordered_map<std::string, int> m_stem_index;

        /** Lowercased sprite name → index into runtime state. */
        std::unordered_map<std::string, int> m_sprite_index;

        /** Lowercased FX name → index into runtime state. */
        std::unordered_map<std::string, int> m_fx_index;

        /** Filtered indices into the active list. */
        std::vector<int> m_visible;

        /** Case-insensitive substring filter. */
        std::string m_filter;

        /** Current left-list mode. */
        BrowseMode m_mode = BrowseMode::Skills;
    };

}  // namespace openkal::skill_effect_view
