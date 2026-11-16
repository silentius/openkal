/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/server/MixCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cctype>
#include <tuple>
#include <utility>

namespace openkal::server {
    namespace {

        [[nodiscard]] bool json_flag(const platform::Json& node, bool fallback) {
            if (node.type() == platform::Json::Type::Bool) {
                return node.boolean();
            }
            if (node.is_number()) {
                return node.as_int(fallback ? 1 : 0) != 0;
            }
            return fallback;
        }

        [[nodiscard]] std::string ascii_lower(std::string s) {
            for (char& ch : s) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return s;
        }

        [[nodiscard]] int class_from_name(std::string_view name) {
            const std::string lc = ascii_lower(std::string(name));
            if (lc == "knight") {
                return 0;
            }
            if (lc == "mage") {
                return 1;
            }
            if (lc == "archer") {
                return 2;
            }
            if (lc == "thief") {
                return 3;
            }
            return -1;
        }

        [[nodiscard]] bool grade_ok(int required, int actual) {
            return required < 0 || required == actual;
        }

        [[nodiscard]] bool ends_with_json(std::string_view path) {
            if (path.size() < 5) {
                return false;
            }
            const std::string lc = ascii_lower(std::string(path.substr(path.size() - 5)));
            return lc == ".json";
        }

    }  // namespace

    bool MixCatalog::load_dir(const std::string& dir) {
        m_recipes.clear();
        m_stones.clear();
        m_grades.clear();
        m_success_level1_percent = 66;
        m_success_level2_percent = 33;
        m_max_mix_level = 2;
        m_block_same_stone_at_max = true;
        if (dir.empty() || !platform::is_directory(dir)) {
            log::warn("MixCatalog: missing directory " + dir);
            return false;
        }
        auto files = platform::list_files_recursive(dir);
        std::sort(files.begin(), files.end());
        bool any = false;
        for (const std::string& rel : files) {
            if (!ends_with_json(rel)) {
                continue;
            }
            const std::string path = platform::File::join(dir, rel);
            const auto bytes = platform::File::read(path);
            if (!bytes) {
                log::warn("MixCatalog: failed to read " + path);
                continue;
            }
            std::string err;
            platform::Json root;
            if (!platform::Json::parse(std::string(bytes->begin(), bytes->end()), root, &err)) {
                log::warn("MixCatalog: parse failed " + path + ": " + err);
                continue;
            }
            if (!merge_root(root)) {
                log::warn("MixCatalog: ignored " + path);
                continue;
            }
            any = true;
        }
        if (!ready()) {
            log::warn("MixCatalog: no stones/recipes in " + dir);
            return false;
        }
        (void) any;
        return true;
    }

    bool MixCatalog::load_json_text(std::string_view text) {
        std::string err;
        platform::Json root;
        if (!platform::Json::parse(text, root, &err)) {
            log::warn("MixCatalog: parse failed: " + err);
            return false;
        }
        return merge_root(root);
    }

    bool MixCatalog::ready() const {
        return !m_stones.empty() && !m_recipes.empty() && !m_grades.empty();
    }

    bool MixCatalog::is_mixing_stone(int index) const {
        return m_stones.find(index) != m_stones.end();
    }

    int MixCatalog::stone_type(int index) const {
        const auto it = m_stones.find(index);
        if (it == m_stones.end()) {
            return 0;
        }
        return it->second.type();
    }

    int MixCatalog::stone_require_mix_level(int index) const {
        const auto it = m_stones.find(index);
        if (it == m_stones.end()) {
            return 0;
        }
        return it->second.require_mix_level();
    }

    std::vector<MixMaterialRow> MixCatalog::material_rows(std::uint8_t mode) const {
        std::vector<MixMaterialRow> rows;
        for (const Recipe& recipe : m_recipes) {
            if (recipe.mode() != mode) {
                continue;
            }
            auto append = [&](const GradeRow* grade) {
                MixMaterialRow row;
                row.recipe_id = recipe.id();
                row.center_grade = grade ? grade->center_grade() : recipe.center().grade();
                row.extra_grade =
                    recipe.extra_weapon().required()
                        ? (grade ? grade->extra_grade() : recipe.extra_weapon().grade())
                        : 0;
                row.armor_grade = recipe.armor().required()
                                      ? (grade ? grade->armor_grade() : recipe.armor().grade())
                                      : 0;
                row.upper_only = recipe.armor().upper_only() || (grade && grade->upper_only());
                row.stone_count = recipe.stone_count();
                row.same_stone_type = recipe.stones_same_type();
                row.grade3 = recipe.action() == MixResultAction::ApplyMix && row.stone_count == 2;
                row.gold = recipe.gold();
                rows.push_back(std::move(row));
            };
            if (recipe.action() == MixResultAction::ApplyMix) {
                for (const auto& [key, grade] : m_grades) {
                    (void) key;
                    if (recipe.extra_weapon().required() && recipe.armor().required() &&
                        grade_ok(recipe.center().grade(), grade.center_grade()) &&
                        grade_ok(recipe.extra_weapon().grade(), grade.extra_grade()) &&
                        grade_ok(recipe.armor().grade(), grade.armor_grade())) {
                        append(&grade);
                    }
                }
            } else {
                append(nullptr);
            }
        }
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return std::tie(a.stone_count, a.center_grade, a.recipe_id) <
                   std::tie(b.stone_count, b.center_grade, b.recipe_id);
        });
        return rows;
    }

    bool MixCatalog::merge_root(const platform::Json& root) {
        if (!root.is_object()) {
            return false;
        }
        if (const platform::Json* stones = root.get("stones")) {
            if (stones->is_array()) {
                for (const auto& j : stones->array()) {
                    if (!j.is_object()) {
                        continue;
                    }
                    StoneDef def;
                    def.set_index(j.at("index").as_int(0));
                    def.set_type(j.at("type").as_int(0));
                    def.set_require_mix_level(j.at("require_mix_level").as_int(0));
                    if (def.index() <= 0 || def.type() <= 0) {
                        continue;
                    }
                    m_stones[def.index()] = def;
                }
            }
        }
        if (const platform::Json* rules = root.get("rules")) {
            if (rules->is_object()) {
                m_success_level1_percent =
                    rules->at("success_level1_percent").as_int(m_success_level1_percent);
                m_success_level2_percent =
                    rules->at("success_level2_percent").as_int(m_success_level2_percent);
                m_max_mix_level = rules->at("max_mix_level").as_int(m_max_mix_level);
                if (const platform::Json* block = rules->get("block_same_stone_at_max")) {
                    m_block_same_stone_at_max = json_flag(*block, m_block_same_stone_at_max);
                }
            }
        }
        if (const platform::Json* grades = root.get("grades")) {
            if (grades->is_array()) {
                for (const auto& j : grades->array()) {
                    if (!j.is_object()) {
                        continue;
                    }
                    GradeRow row;
                    row.set_center_grade(j.at("center").as_int(0));
                    row.set_extra_grade(j.at("extra").as_int(0));
                    row.set_armor_grade(j.at("armor").as_int(0));
                    const std::string slot = ascii_lower(j.at("slot").as_string("any"));
                    row.set_upper_only(slot == "upperarmor" || slot == "upper");
                    if (row.center_grade() <= 0 || row.extra_grade() <= 0 ||
                        row.armor_grade() <= 0) {
                        continue;
                    }
                    m_grades[row.center_grade()] = row;
                }
            }
        }
        if (const platform::Json* recipes = root.get("recipes")) {
            if (!recipes->is_array()) {
                return true;
            }
            for (const auto& j : recipes->array()) {
                if (!j.is_object()) {
                    continue;
                }
                Recipe rec;
                rec.set_id(j.at("id").as_string());
                rec.set_mode(static_cast<std::uint8_t>(j.at("mode").as_int(0)));
                rec.set_gold(static_cast<std::int64_t>(j.at("gold").as_int(0)));
                if (const platform::Json* center = j.get("center")) {
                    rec.center().set_required(true);
                    rec.center().set_wearable(json_flag(center->at("wearable"), true));
                    rec.center().set_grade(center->at("grade").as_int(-1));
                    if (!center->get("grade")) {
                        rec.center().set_grade(-1);
                    }
                } else {
                    rec.center().set_required(true);
                    rec.center().set_wearable(true);
                    rec.center().set_grade(-1);
                }
                if (const platform::Json* extra = j.get("extra_weapon")) {
                    rec.extra_weapon().set_required(json_flag(extra->at("required"), false));
                    rec.extra_weapon().set_wearable(json_flag(extra->at("wearable"), true));
                    if (extra->get("grade")) {
                        rec.extra_weapon().set_grade(extra->at("grade").as_int(-1));
                    } else {
                        rec.extra_weapon().set_grade(-1);
                    }
                }
                if (const platform::Json* armor = j.get("armor")) {
                    rec.armor().set_required(json_flag(armor->at("required"), true));
                    if (armor->get("grade")) {
                        rec.armor().set_grade(armor->at("grade").as_int(-1));
                    } else {
                        rec.armor().set_grade(-1);
                    }
                    const std::string slot = ascii_lower(armor->at("slot").as_string("any"));
                    rec.armor().set_upper_only(slot == "upperarmor" || slot == "upper");
                }
                if (const platform::Json* stones_spec = j.get("stones")) {
                    rec.set_stone_count(stones_spec->at("count").as_int(1));
                    rec.set_stones_same_type(json_flag(stones_spec->at("same_type"), false));
                }
                if (const platform::Json* result = j.get("result")) {
                    const std::string action =
                        ascii_lower(result->at("action").as_string("apply_mix"));
                    rec.set_action(action == "replace_weapon" ? MixResultAction::ReplaceWeapon
                                                              : MixResultAction::ApplyMix);
                    if (const platform::Json* replace = result->get("replace")) {
                        if (replace->is_array()) {
                            for (const auto& row : replace->array()) {
                                if (!row.is_object()) {
                                    continue;
                                }
                                ReplaceEntry entry;
                                entry.set_player_class(
                                    class_from_name(row.at("class").as_string()));
                                entry.set_kind(ascii_lower(row.at("kind").as_string()));
                                entry.set_index(row.at("index").as_int(0));
                                if (entry.index() <= 0) {
                                    continue;
                                }
                                rec.replace().push_back(std::move(entry));
                            }
                        }
                    }
                }
                if (rec.id().empty()) {
                    continue;
                }
                m_recipes.push_back(std::move(rec));
            }
        }
        return true;
    }

    bool MixCatalog::spec_fits(const MaterialSpec& spec, const MixItemOffer& item, bool as_armor) {
        if (spec.wearable() && !item.wearable()) {
            return false;
        }
        if (!grade_ok(spec.grade(), item.grade())) {
            return false;
        }
        if (as_armor && spec.upper_only() && item.class_small() != "upperarmor") {
            return false;
        }
        return true;
    }

    bool MixCatalog::is_imperial_weapon(int index) const {
        for (const auto& recipe : m_recipes) {
            if (recipe.action() != MixResultAction::ReplaceWeapon) {
                continue;
            }
            for (const auto& entry : recipe.replace()) {
                if (entry.index() == index) {
                    return true;
                }
            }
        }
        return false;
    }

    int
    MixCatalog::pick_replacement(const Recipe& recipe, int player_class, std::string_view kind) {
        if (player_class < 0 || recipe.replace().empty()) {
            return 0;
        }
        const std::string kind_lc = ascii_lower(std::string(kind));
        int fallback = 0;
        for (const ReplaceEntry& entry : recipe.replace()) {
            if (entry.player_class() >= 0 && entry.player_class() != player_class) {
                continue;
            }
            if (fallback == 0) {
                fallback = entry.index();
            }
            if (!entry.kind().empty() && entry.kind() == kind_lc) {
                return entry.index();
            }
        }
        return fallback;
    }

    const MixCatalog::GradeRow* MixCatalog::find_grade(int center_grade) const {
        const auto it = m_grades.find(center_grade);
        if (it == m_grades.end()) {
            return nullptr;
        }
        return &it->second;
    }

    MixMatch MixCatalog::match(std::uint8_t mode,
                               const MixItemOffer& center,
                               const std::vector<MixItemOffer>& outer,
                               int player_class) const {
        MixMatch out;
        if (center.iid() == 0 || !center.is_weapon()) {
            return out;
        }
        for (const Recipe& recipe : m_recipes) {
            if (recipe.mode() != mode) {
                continue;
            }
            if (!spec_fits(recipe.center(), center, false)) {
                continue;
            }

            std::vector<const MixItemOffer*> stones;
            std::vector<const MixItemOffer*> weapons;
            std::vector<const MixItemOffer*> armors;
            bool unknown = false;
            for (const MixItemOffer& item : outer) {
                if (item.iid() == 0) {
                    continue;
                }
                if (is_mixing_stone(item.index())) {
                    stones.push_back(&item);
                    continue;
                }
                if (item.is_weapon()) {
                    weapons.push_back(&item);
                    continue;
                }
                if (item.is_defense()) {
                    armors.push_back(&item);
                    continue;
                }
                unknown = true;
                break;
            }
            if (unknown) {
                continue;
            }
            if (static_cast<int>(stones.size()) != recipe.stone_count()) {
                continue;
            }
            const int stone_req = stone_require_mix_level(stones[0]->index());
            // G1/G2 mix: one elemental stone. G3 Mystery/Blood: two same-type stones.
            if (recipe.action() == MixResultAction::ApplyMix) {
                if (recipe.stone_count() == 1 && stone_req > 0) {
                    continue;
                }
                if (recipe.stone_count() == 2 && stone_req < 2) {
                    continue;
                }
            }
            if (recipe.armor().required()) {
                if (armors.size() != 1 || !spec_fits(recipe.armor(), *armors[0], true)) {
                    continue;
                }
            } else if (!armors.empty()) {
                continue;
            }
            const MixItemOffer* extra = nullptr;
            if (recipe.extra_weapon().required()) {
                if (weapons.size() != 1 || !spec_fits(recipe.extra_weapon(), *weapons[0], false) ||
                    !weapons[0]->is_weapon()) {
                    continue;
                }
                extra = weapons[0];
            }
            if (recipe.action() == MixResultAction::ApplyMix) {
                const GradeRow* row = find_grade(center.grade());
                if (!row || !extra || extra->grade() != row->extra_grade() || !extra->is_weapon()) {
                    continue;
                }
                if (armors.size() != 1 || armors[0]->grade() != row->armor_grade()) {
                    continue;
                }
                if (row->upper_only() && armors[0]->class_small() != "upperarmor") {
                    continue;
                }
            }
            if (recipe.stones_same_type() && stones.size() >= 2) {
                const int t0 = stone_type(stones[0]->index());
                const int t1 = stone_type(stones[1]->index());
                if (t0 == 0 || t0 != t1) {
                    continue;
                }
            }
            if (stones.empty() || stone_type(stones[0]->index()) == 0) {
                continue;
            }
            // Making (replace_weapon) always starts the new imperial at G1. The two
            // stones are craft materials, not a second mix grade.
            const std::uint8_t roll_from = recipe.action() == MixResultAction::ReplaceWeapon
                                               ? std::uint8_t{0}
                                               : center.mix_type();
            if (!plan_roll(roll_from, stones[0]->index()).allowed()) {
                continue;
            }

            out.set_valid(true);
            out.set_recipe_id(recipe.id());
            out.set_mode(recipe.mode());
            out.set_gold(recipe.gold());
            out.set_weapon_iid(center.iid());
            out.set_extra_weapon_iid(extra ? extra->iid() : 0);
            out.set_armor_iid(recipe.armor().required() && !armors.empty() ? armors[0]->iid() : 0);
            out.set_stone1_iid(stones[0]->iid());
            out.set_stone2_iid(stones.size() >= 2 ? stones[1]->iid() : 0);
            out.set_action(recipe.action());
            out.set_replacement_index(pick_replacement(recipe, player_class, center.class_small()));
            return out;
        }
        return out;
    }

    MixRollPlan MixCatalog::plan_roll(std::uint8_t weapon_mix_type, int stone_index) const {
        MixRollPlan plan;
        const int new_stone_type = stone_type(stone_index);
        if (new_stone_type == 0) {
            return plan;
        }
        const int cur_stone_type = static_cast<int>(weapon_mix_type & 0x0F);
        const int cur_mix_level = static_cast<int>((weapon_mix_type >> 4) & 0x03);
        const int required = stone_require_mix_level(stone_index);
        if (required > 0 && cur_mix_level < required) {
            return plan;
        }
        if (m_block_same_stone_at_max && cur_stone_type == new_stone_type &&
            cur_mix_level >= m_max_mix_level) {
            return plan;
        }
        int target_level = 1;
        int chance = m_success_level1_percent;
        if (cur_mix_level >= 1) {
            target_level = 2;
            chance = m_success_level2_percent;
        }
        if (target_level > m_max_mix_level) {
            return plan;
        }
        plan.set_allowed(true);
        plan.set_target_level(target_level);
        plan.set_success_percent(chance);
        plan.set_new_mix_type((target_level << 4) | new_stone_type);
        return plan;
    }

}  // namespace openkal::server
