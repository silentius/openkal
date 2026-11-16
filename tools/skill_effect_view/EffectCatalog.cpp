/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "EffectCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace openkal::skill_effect_view {
    namespace {

        [[nodiscard]] char ascii_lower(char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        [[nodiscard]] std::string ascii_lowered(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                c = ascii_lower(c);
            }
            return out;
        }

        [[nodiscard]] bool ends_with_gb(std::string_view path) {
            if (path.size() < 3) {
                return false;
            }
            return ascii_lower(path[path.size() - 3]) == '.' &&
                   ascii_lower(path[path.size() - 2]) == 'g' &&
                   ascii_lower(path[path.size() - 1]) == 'b';
        }

        [[nodiscard]] std::string stem_of(std::string_view relative) {
            const auto slash = relative.find_last_of('/');
            std::string_view name =
                slash == std::string_view::npos ? relative : relative.substr(slash + 1);
            const auto dot = name.find_last_of('.');
            if (dot != std::string_view::npos) {
                name = name.substr(0, dot);
            }
            return std::string(name);
        }

        [[nodiscard]] int clazz_from_role(std::string_view role) {
            if (role == "knight") {
                return 0;
            }
            if (role == "mage" || role == "magician") {
                return 1;
            }
            if (role == "archer") {
                return 2;
            }
            if (role == "thief") {
                return 3;
            }
            return -1;
        }

    }  // namespace

    void AssetEntry::set_folder(std::string value) {
        m_folder = std::move(value);
    }

    void AssetEntry::set_stem(std::string value) {
        m_stem = std::move(value);
    }

    void AssetEntry::set_relative_path(std::string value) {
        m_relative_path = std::move(value);
    }

    void AssetEntry::set_absolute_path(std::string value) {
        m_absolute_path = std::move(value);
    }

    void AssetEntry::add_used_by(std::string skill_name) {
        if (skill_name.empty()) {
            return;
        }
        for (const auto& existing : m_used_by) {
            if (existing == skill_name) {
                return;
            }
        }
        m_used_by.push_back(std::move(skill_name));
    }

    void AssetEntry::clear_used_by() {
        m_used_by.clear();
    }

    void SkillRow::set_name(std::string value) {
        m_name = std::move(value);
    }

    void SkillRow::set_role(std::string value) {
        m_role = std::move(value);
    }

    void SkillRow::set_key(std::string value) {
        m_key = std::move(value);
    }

    const std::string& SkillRow::effect(int gubun) const {
        static const std::string empty;
        if (gubun < 0 || gubun >= static_cast<int>(m_effects.size())) {
            return empty;
        }
        return m_effects[static_cast<std::size_t>(gubun)];
    }

    void SkillRow::set_effect(int gubun, std::string value) {
        if (gubun < 0 || gubun >= static_cast<int>(m_effects.size())) {
            return;
        }
        m_effects[static_cast<std::size_t>(gubun)] = std::move(value);
    }

    bool SkillRow::has_named_effect() const {
        for (const auto& stem : m_effects) {
            if (!stem.empty()) {
                return true;
            }
        }
        return false;
    }

    std::string SkillRow::list_label() const {
        std::string out = m_role.empty() ? "?" : m_role;
        out += ' ';
        out += std::to_string(m_index);
        out += "  ";
        out += m_name.empty() ? m_key : m_name;
        return out;
    }

    void AttachRow::set_name(std::string value) {
        m_name = std::move(value);
    }

    void ScriptRow::set_name(std::string value) {
        m_name = std::move(value);
    }

    void ScriptRow::add_used_by(std::string label) {
        if (label.empty()) {
            return;
        }
        for (const auto& existing : m_used_by) {
            if (existing == label) {
                return;
            }
        }
        m_used_by.push_back(std::move(label));
    }

    void ScriptRow::clear_used_by() {
        m_used_by.clear();
    }

    std::string ScriptRow::list_label() const {
        if (m_used_by.empty()) {
            return m_name;
        }
        return m_name + "  (" + m_used_by.front() + ")";
    }

    void MobRow::set_name(std::string value) {
        m_name = std::move(value);
    }

    void MobRow::set_particles(std::vector<AttachRow> value) {
        m_particles = std::move(value);
    }

    void MobRow::set_fx(std::vector<AttachRow> value) {
        m_fx = std::move(value);
    }

    bool MobRow::has_attach() const {
        return !m_particles.empty() || !m_fx.empty();
    }

    std::string MobRow::list_label() const {
        std::string out = m_npc ? "n " : "m ";
        out += std::to_string(m_index);
        out += "  ";
        out += m_name;
        return out;
    }

    const char* EffectCatalog::slot_name(int gubun) {
        switch (gubun) {
        case 0:
            return "prep";
        case 1:
            return "fire";
        case 2:
            return "hit";
        default:
            return "?";
        }
    }

    const char* EffectCatalog::mode_name(BrowseMode mode) {
        switch (mode) {
        case BrowseMode::Skills:
            return "Skills";
        case BrowseMode::Models:
            return "Models";
        case BrowseMode::Sprite:
            return "Sprite";
        case BrowseMode::Fx:
            return "Fx";
        case BrowseMode::Mobs:
            return "Mobs";
        default:
            return "?";
        }
    }

    int EffectCatalog::mode_total() const {
        switch (m_mode) {
        case BrowseMode::Skills:
            return static_cast<int>(m_skills.size());
        case BrowseMode::Models:
            return static_cast<int>(m_assets.size());
        case BrowseMode::Sprite:
            return static_cast<int>(m_sprites.size());
        case BrowseMode::Fx:
            return static_cast<int>(m_fxs.size());
        case BrowseMode::Mobs:
            return static_cast<int>(m_mobs.size());
        default:
            return 0;
        }
    }

    void EffectCatalog::scan_folder(const std::string& folder_dir, const std::string& folder_name) {
        if (folder_dir.empty() || !platform::is_directory(folder_dir)) {
            return;
        }
        const auto files = platform::list_files_recursive(folder_dir);
        for (const auto& rel_raw : files) {
            std::string rel = platform::normalize_separators(rel_raw);
            if (!ends_with_gb(rel)) {
                continue;
            }
            AssetEntry entry;
            entry.set_folder(folder_name);
            entry.set_relative_path(rel);
            entry.set_absolute_path(platform::join_path(folder_dir, rel));
            entry.set_stem(stem_of(rel));
            const std::string key = ascii_lowered(entry.stem());
            if (m_stem_index.find(key) == m_stem_index.end()) {
                m_stem_index.emplace(key, static_cast<int>(m_assets.size()));
            }
            m_assets.push_back(std::move(entry));
        }
    }

    std::size_t EffectCatalog::scan_assets(const std::string& data_root) {
        m_assets.clear();
        m_stem_index.clear();
        m_visible.clear();
        if (data_root.empty()) {
            return 0;
        }
        scan_folder(platform::resolve_path_case(data_root, "Skill"), "Skill");
        scan_folder(platform::resolve_path_case(data_root, "Effect"), "Effect");
        std::sort(m_assets.begin(), m_assets.end(), [](const AssetEntry& a, const AssetEntry& b) {
            if (a.folder() != b.folder()) {
                // Skill first so the stem index prefers jobsystem models.
                return a.folder() > b.folder();
            }
            return a.relative_path() < b.relative_path();
        });
        m_stem_index.clear();
        for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
            const std::string key = ascii_lowered(m_assets[static_cast<std::size_t>(i)].stem());
            if (m_stem_index.find(key) == m_stem_index.end()) {
                m_stem_index.emplace(key, i);
            }
        }
        rebuild_visible();
        return m_assets.size();
    }

    std::size_t EffectCatalog::load_skills(const std::string& skills_dir) {
        m_skills.clear();
        if (skills_dir.empty() || !platform::is_directory(skills_dir)) {
            bind_skill_uses();
            rebuild_visible();
            return 0;
        }
        for (const auto& rel : platform::list_files_recursive(skills_dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(skills_dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            SkillRow row;
            row.set_role(root.at("role").as_string());
            row.set_key(root.at("key").as_string());
            row.set_name(root.at("name").as_string());
            if (row.name().empty()) {
                row.set_name(root.at("comment").as_string());
            }
            int clazz = root.at("class").as_int(-1);
            if (clazz < 0) {
                clazz = clazz_from_role(row.role());
            }
            row.set_clazz(clazz);
            row.set_index(root.at("index").as_int(0));
            const platform::Json& effect = root.at("effect");
            if (effect.is_array()) {
                const auto& arr = effect.array();
                for (std::size_t i = 0; i < arr.size() && i < 3; ++i) {
                    row.set_effect(static_cast<int>(i), arr[i].as_string());
                }
            } else if (effect.is_string()) {
                row.set_effect(0, effect.as_string());
            }
            if (!row.has_named_effect()) {
                continue;
            }
            m_skills.push_back(std::move(row));
        }
        std::sort(m_skills.begin(), m_skills.end(), [](const SkillRow& a, const SkillRow& b) {
            if (a.role() != b.role()) {
                return a.role() < b.role();
            }
            if (a.index() != b.index()) {
                return a.index() < b.index();
            }
            return a.name() < b.name();
        });
        bind_skill_uses();
        rebuild_visible();
        return m_skills.size();
    }

    void EffectCatalog::set_scripts(std::vector<std::string> sprites,
                                    std::vector<std::string> fxs) {
        m_sprites.clear();
        m_fxs.clear();
        m_sprite_index.clear();
        m_fx_index.clear();
        m_sprites.reserve(sprites.size());
        for (auto& name : sprites) {
            if (name.empty()) {
                continue;
            }
            const std::string key = ascii_lowered(name);
            if (m_sprite_index.find(key) != m_sprite_index.end()) {
                continue;
            }
            ScriptRow row;
            row.set_name(std::move(name));
            m_sprite_index.emplace(key, static_cast<int>(m_sprites.size()));
            m_sprites.push_back(std::move(row));
        }
        m_fxs.reserve(fxs.size());
        for (auto& name : fxs) {
            if (name.empty()) {
                continue;
            }
            const std::string key = ascii_lowered(name);
            if (m_fx_index.find(key) != m_fx_index.end()) {
                continue;
            }
            ScriptRow row;
            row.set_name(std::move(name));
            m_fx_index.emplace(key, static_cast<int>(m_fxs.size()));
            m_fxs.push_back(std::move(row));
        }
        bind_script_uses();
        rebuild_visible();
    }

    namespace {

        [[nodiscard]] std::vector<AttachRow> parse_attach_array(const platform::Json& node) {
            std::vector<AttachRow> out;
            if (!node.is_array()) {
                return out;
            }
            for (const auto& elem : node.array()) {
                if (!elem.is_array() || elem.array().empty()) {
                    continue;
                }
                const auto& arr = elem.array();
                AttachRow row;
                row.set_name(arr[0].as_string());
                if (row.name().empty()) {
                    continue;
                }
                if (arr.size() > 1) {
                    row.set_effect_bone(arr[1].as_int(-1));
                }
                if (arr.size() > 2) {
                    row.set_ox(arr[2].as_float());
                }
                if (arr.size() > 3) {
                    row.set_oy(arr[3].as_float());
                }
                if (arr.size() > 4) {
                    row.set_oz(arr[4].as_float());
                }
                out.push_back(std::move(row));
            }
            return out;
        }

    }  // namespace

    void EffectCatalog::load_mob_dir(const std::string& dir, bool npc) {
        if (dir.empty() || !platform::is_directory(dir)) {
            return;
        }
        for (const auto& rel : platform::list_files_recursive(dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            const platform::Json& appearance = root.at("appearance");
            if (!appearance.is_object()) {
                continue;
            }
            MobRow row;
            row.set_npc(npc);
            row.set_index(root.at("index").as_int(0));
            std::string name = root.at("name").as_string();
            if (name.empty()) {
                name = base;
            }
            row.set_name(std::move(name));
            row.set_particles(parse_attach_array(appearance.at("particles")));
            row.set_fx(parse_attach_array(appearance.at("fx")));
            if (!row.has_attach()) {
                continue;
            }
            m_mobs.push_back(std::move(row));
        }
    }

    std::size_t EffectCatalog::load_users(const std::string& monsters_dir,
                                          const std::string& npcs_dir) {
        m_mobs.clear();
        load_mob_dir(monsters_dir, false);
        load_mob_dir(npcs_dir, true);
        std::sort(m_mobs.begin(), m_mobs.end(), [](const MobRow& a, const MobRow& b) {
            if (a.npc() != b.npc()) {
                return !a.npc() && b.npc();
            }
            if (a.index() != b.index()) {
                return a.index() < b.index();
            }
            return a.name() < b.name();
        });
        bind_script_uses();
        rebuild_visible();
        return m_mobs.size();
    }

    void EffectCatalog::bind_script_uses() {
        for (auto& row : m_sprites) {
            row.clear_used_by();
        }
        for (auto& row : m_fxs) {
            row.clear_used_by();
        }
        for (const auto& mob : m_mobs) {
            const std::string label = mob.list_label();
            for (const auto& attach : mob.particles()) {
                add_script_use(attach.name(), false, label);
            }
            for (const auto& attach : mob.fx()) {
                add_script_use(attach.name(), true, label);
            }
        }
    }

    void EffectCatalog::add_script_use(std::string_view name, bool fx, const std::string& label) {
        if (name.empty()) {
            return;
        }
        auto& index = fx ? m_fx_index : m_sprite_index;
        auto& rows = fx ? m_fxs : m_sprites;
        auto it = index.find(ascii_lowered(name));
        if (it == index.end()) {
            return;
        }
        rows[static_cast<std::size_t>(it->second)].add_used_by(label);
    }

    void EffectCatalog::bind_skill_uses() {
        for (auto& asset : m_assets) {
            asset.clear_used_by();
        }
        for (const auto& skill : m_skills) {
            for (int g = 0; g < 3; ++g) {
                const std::string& stem = skill.effect(g);
                if (stem.empty()) {
                    continue;
                }
                auto it = m_stem_index.find(ascii_lowered(stem));
                if (it == m_stem_index.end()) {
                    continue;
                }
                m_assets[static_cast<std::size_t>(it->second)].add_used_by(skill.list_label());
            }
        }
    }

    void EffectCatalog::set_filter(std::string query) {
        if (query == m_filter) {
            return;
        }
        m_filter = std::move(query);
        rebuild_visible();
    }

    void EffectCatalog::set_mode(BrowseMode mode) {
        if (mode == m_mode) {
            return;
        }
        m_mode = mode;
        rebuild_visible();
    }

    const SkillRow& EffectCatalog::visible_skill(int visible_index) const {
        return m_skills[static_cast<std::size_t>(
            m_visible[static_cast<std::size_t>(visible_index)])];
    }

    const AssetEntry& EffectCatalog::visible_asset(int visible_index) const {
        return m_assets[static_cast<std::size_t>(
            m_visible[static_cast<std::size_t>(visible_index)])];
    }

    const ScriptRow& EffectCatalog::visible_script(int visible_index) const {
        const auto& rows = m_mode == BrowseMode::Fx ? m_fxs : m_sprites;
        return rows[static_cast<std::size_t>(m_visible[static_cast<std::size_t>(visible_index)])];
    }

    const MobRow& EffectCatalog::visible_mob(int visible_index) const {
        return m_mobs[static_cast<std::size_t>(m_visible[static_cast<std::size_t>(visible_index)])];
    }

    const AssetEntry* EffectCatalog::find_asset(std::string_view stem) const {
        if (stem.empty()) {
            return nullptr;
        }
        auto it = m_stem_index.find(ascii_lowered(stem));
        if (it == m_stem_index.end()) {
            return nullptr;
        }
        return &m_assets[static_cast<std::size_t>(it->second)];
    }

    void EffectCatalog::rebuild_visible() {
        m_visible.clear();
        const std::string needle = ascii_lowered(m_filter);
        auto matches = [&](const std::string& hay_raw) {
            if (needle.empty()) {
                return true;
            }
            return ascii_lowered(hay_raw).find(needle) != std::string::npos;
        };

        if (m_mode == BrowseMode::Skills) {
            m_visible.reserve(m_skills.size());
            for (int i = 0; i < static_cast<int>(m_skills.size()); ++i) {
                const SkillRow& row = m_skills[static_cast<std::size_t>(i)];
                std::string hay = row.list_label();
                hay += ' ';
                hay += row.key();
                for (int g = 0; g < 3; ++g) {
                    hay += ' ';
                    hay += row.effect(g);
                }
                if (matches(hay)) {
                    m_visible.push_back(i);
                }
            }
            return;
        }
        if (m_mode == BrowseMode::Models) {
            m_visible.reserve(m_assets.size());
            for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
                const AssetEntry& asset = m_assets[static_cast<std::size_t>(i)];
                std::string hay = asset.folder();
                hay += '/';
                hay += asset.relative_path();
                for (const auto& name : asset.used_by()) {
                    hay += ' ';
                    hay += name;
                }
                if (matches(hay)) {
                    m_visible.push_back(i);
                }
            }
            return;
        }
        if (m_mode == BrowseMode::Sprite || m_mode == BrowseMode::Fx) {
            const auto& rows = m_mode == BrowseMode::Fx ? m_fxs : m_sprites;
            m_visible.reserve(rows.size());
            for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
                const ScriptRow& row = rows[static_cast<std::size_t>(i)];
                std::string hay = row.name();
                for (const auto& name : row.used_by()) {
                    hay += ' ';
                    hay += name;
                }
                if (matches(hay)) {
                    m_visible.push_back(i);
                }
            }
            return;
        }
        m_visible.reserve(m_mobs.size());
        for (int i = 0; i < static_cast<int>(m_mobs.size()); ++i) {
            const MobRow& mob = m_mobs[static_cast<std::size_t>(i)];
            std::string hay = mob.list_label();
            for (const auto& attach : mob.particles()) {
                hay += ' ';
                hay += attach.name();
            }
            for (const auto& attach : mob.fx()) {
                hay += ' ';
                hay += attach.name();
            }
            if (matches(hay)) {
                m_visible.push_back(i);
            }
        }
    }

}  // namespace openkal::skill_effect_view
