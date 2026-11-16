/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/CastleWarRules.h"

#include "openkal/map/WorldGrid.h"
#include "openkal/platform/Json.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace openkal::server {
    namespace {
        using platform::Json;

        constexpr int kDaySeconds = 86400;
        constexpr int kWeekSeconds = 7 * kDaySeconds;
        constexpr int kMaximumCatalogIndex = std::numeric_limits<std::uint16_t>::max();
        constexpr int kMaximumMap = std::numeric_limits<std::uint8_t>::max();
        constexpr int kMaximumInteger = std::numeric_limits<int>::max();
        constexpr int kMinimumInteger = std::numeric_limits<int>::min();
        constexpr int kMaximumWorldPixel = map::kWorldCellSlots * map::kWorldSize - 1;

        struct IntegerField {
            std::string_view name;
            int& target;
            int min;
            int max;
        };

        bool read_integer(const Json& input, int min, int max, int& result) {
            if (!input.is_number()) {
                return false;
            }
            const double number = input.number();
            if (!std::isfinite(number) || std::floor(number) != number || number < min ||
                number > max) {
                return false;
            }
            result = static_cast<int>(number);
            return true;
        }

        bool check_object(const Json& input,
                          std::initializer_list<std::string_view> fields,
                          const std::string& path,
                          std::string& error) {
            if (!input.is_object()) {
                error = path + " must be an object.";
                return false;
            }
            for (const auto& entry : input.object()) {
                if (std::find(fields.begin(), fields.end(), entry.first) == fields.end()) {
                    error = "Unknown Castle War setting: " + path + "." + entry.first;
                    return false;
                }
            }
            return true;
        }

        bool read_fields(const Json& input,
                         std::initializer_list<IntegerField> fields,
                         const std::string& path,
                         std::string& error) {
            if (!input.is_object()) {
                error = path + " must be an object.";
                return false;
            }
            for (const auto& [name, value] : input.object()) {
                const auto* field =
                    std::find_if(fields.begin(), fields.end(), [&](const IntegerField& entry) {
                        return entry.name == name;
                    });
                if (field == fields.end()) {
                    error = "Unknown Castle War setting: " + path + "." + name;
                    return false;
                }
                if (!read_integer(value, field->min, field->max, field->target)) {
                    error = path + "." + name + " must be an integer between " +
                            std::to_string(field->min) + " and " + std::to_string(field->max) + ".";
                    return false;
                }
            }
            return true;
        }

        bool read_group(const Json& root,
                        const char* key,
                        std::initializer_list<IntegerField> fields,
                        std::string& error) {
            const auto* group = root.get(key);
            return !group || read_fields(*group, fields, key, error);
        }

        template <std::size_t Size>
        bool read_array(const Json& input,
                        std::array<int, Size>& result,
                        int min,
                        int max,
                        const std::string& path,
                        std::string& error) {
            if (!input.is_array() || input.array().size() != Size) {
                error = path + " must contain exactly " + std::to_string(Size) + " integers.";
                return false;
            }
            for (std::size_t i = 0; i < Size; ++i) {
                if (!read_integer(input.array()[i], min, max, result[i])) {
                    error = path + " contains an invalid integer.";
                    return false;
                }
            }
            return true;
        }

        bool read_position(const Json& input,
                           CastleWarPosition& position,
                           const std::string& path,
                           std::string& error) {
            std::array<int, 4> values{};
            if (!read_array(input, values, kMinimumInteger, kMaximumInteger, path, error)) {
                return false;
            }
            if (values[0] < 0 || values[0] > kMaximumMap || values[1] < 0 ||
                values[1] > kMaximumWorldPixel || values[2] < 0 || values[2] > kMaximumWorldPixel) {
                error = path + " requires a valid map and horizontal world coordinates.";
                return false;
            }
            position = {values[0], values[1], values[2], values[3]};
            return true;
        }

        bool read_layout(const Json& layout, CastleWarRules& rules, std::string& error) {
            if (!check_object(layout,
                              {"attack_camp",
                               "defense_camp",
                               "east_gate",
                               "west_gate",
                               "exit",
                               "bounds",
                               "guard_npc_index",
                               "master_npc_index",
                               "camp_npc_index",
                               "manager_npc_index",
                               "flags"},
                              "layout",
                              error)) {
                return false;
            }
            const std::pair<const char*, CastleWarPosition*> positions[] = {
                {"attack_camp", &rules.attack_camp},
                {"defense_camp", &rules.defense_camp},
                {"east_gate", &rules.east_gate},
                {"west_gate", &rules.west_gate},
                {"exit", &rules.exit}};
            for (const auto& [key, position] : positions) {
                if (const auto* input = layout.get(key)) {
                    if (!read_position(*input, *position, std::string("layout.") + key, error)) {
                        return false;
                    }
                } else if (rules.enabled) {
                    error = std::string("Missing Castle War position: layout.") + key;
                    return false;
                }
            }
            const std::pair<const char*, int*> npcs[] = {
                {"guard_npc_index", &rules.guard_npc_index},
                {"master_npc_index", &rules.master_npc_index},
                {"camp_npc_index", &rules.camp_npc_index},
                {"manager_npc_index", &rules.manager_npc_index}};
            std::set<int> npc_indices;
            for (const auto& [key, index] : npcs) {
                if (const auto* input = layout.get(key)) {
                    if (!read_integer(
                            *input, rules.enabled ? 1 : 0, kMaximumCatalogIndex, *index) ||
                        (*index != 0 && !npc_indices.insert(*index).second)) {
                        error =
                            std::string("Invalid or duplicate Castle War NPC index: layout.") + key;
                        return false;
                    }
                } else if (rules.enabled) {
                    error = std::string("Missing Castle War NPC index: layout.") + key;
                    return false;
                }
            }
            if (const auto* bounds = layout.get("bounds")) {
                if (!read_array(
                        *bounds, rules.bounds, 0, kMaximumWorldPixel, "layout.bounds", error)) {
                    return false;
                }
            }
            if (rules.enabled) {
                if (rules.bounds[0] >= rules.bounds[2] || rules.bounds[1] >= rules.bounds[3]) {
                    error =
                        "layout.bounds must specify a rectangle with positive width and height.";
                    return false;
                }
                for (const auto& [key, position] : positions) {
                    const bool inside = rules.contains(position->map, position->x, position->y);
                    if (inside == (position == &rules.exit)) {
                        error =
                            std::string("Position is on the wrong side of layout.bounds: layout.") +
                            key;
                        return false;
                    }
                }
            }
            const auto* flags = layout.get("flags");
            if (!flags && !rules.enabled) {
                return true;
            }
            if (!flags || !flags->is_array() || flags->array().size() != rules.flags.size()) {
                error = "layout.flags must contain one large and three small standards.";
                return false;
            }
            std::set<std::pair<int, int>> locations;
            for (std::size_t i = 0; i < rules.flags.size(); ++i) {
                const auto& input = flags->array()[i];
                auto& flag = rules.flags[i];
                const std::string path = "layout.flags[" + std::to_string(i) + "]";
                if (!check_object(input,
                                  {"position", "monster_index", "points", "hit_points"},
                                  path,
                                  error) ||
                    !read_position(
                        input.at("position"), flag.position, path + ".position", error)) {
                    return false;
                }
                if (!read_integer(input.at("monster_index"),
                                  rules.enabled ? 1 : 0,
                                  kMaximumCatalogIndex,
                                  flag.monster_index)) {
                    error = path + ".monster_index must identify a CastleStandard monster.";
                    return false;
                }
                for (const auto& setting :
                     {IntegerField{"points", flag.points, 1, kMaximumInteger / 4},
                      IntegerField{"hit_points", flag.hit_points, 1, kMaximumInteger}}) {
                    const auto* value = input.get(setting.name);
                    if (value && !read_integer(*value, setting.min, setting.max, setting.target)) {
                        error = path + "." + std::string(setting.name) + " is out of range.";
                        return false;
                    }
                }
                const auto& p = flag.position;
                if (rules.enabled &&
                    (!rules.contains(p.map, p.x, p.y) || !locations.emplace(p.x, p.y).second)) {
                    error = path + " must have a distinct position inside layout.bounds.";
                    return false;
                }
            }
            return true;
        }
    }  // namespace

    bool CastleWarRules::load(const std::string& path, std::string& error) {
        error.clear();
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            error = "Cannot read the private Castle War rules file.";
            return false;
        }
        const std::string text{std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>()};
        Json document;
        if (file.bad() || !Json::parse(text, document)) {
            error = "Cannot parse the Castle War JSON.";
            return false;
        }
        return parse(document, error);
    }

    bool CastleWarRules::parse(const Json& document, std::string& error) {
        error.clear();
        if (!check_object(document,
                          {"schema",
                           "enabled",
                           "minimum_level",
                           "schedule",
                           "alliances",
                           "combat",
                           "honor",
                           "tax",
                           "layout"},
                          "Castle War",
                          error)) {
            return false;
        }
        if (document.at("schema").as_string() != "openkal.castle_war/v1") {
            error = "Unsupported Castle War schema.";
            return false;
        }
        CastleWarRules rules;
        if (const auto* enabled = document.get("enabled")) {
            if (enabled->type() != Json::Type::Bool) {
                error = "Castle War enabled must be a boolean.";
                return false;
            }
            rules.enabled = enabled->boolean();
        }
        if (const auto* minimum = document.get("minimum_level")) {
            if (!read_integer(*minimum, 1, 255, rules.minimum_level)) {
                error = "minimum_level must be an integer between 1 and 255.";
                return false;
            }
        }
        if (!read_group(document,
                        "schedule",
                        {{"utc_offset_minutes", rules.utc_offset_minutes, -1439, 1439},
                         {"battle_weekday", rules.battle_weekday, 0, 6},
                         {"battle_hour", rules.battle_hour, 0, 23},
                         {"battle_minute", rules.battle_minute, 0, 59},
                         {"battle_second", rules.battle_second, 0, 59},
                         {"declaration_seconds", rules.declaration_seconds, 1, kWeekSeconds},
                         {"preparation_seconds", rules.preparation_seconds, 1, kWeekSeconds},
                         {"admission_seconds", rules.admission_seconds, 1, kWeekSeconds},
                         {"battle_seconds", rules.battle_seconds, 1, kWeekSeconds}},
                        error) ||
            !read_group(
                document,
                "alliances",
                {{"max_allied_guilds", rules.max_allied_guilds, 0, kMaximumInteger},
                 {"alliance_min_days", rules.alliance_min_days, 1, kMaximumInteger / kDaySeconds},
                 {"alliance_max_days", rules.alliance_max_days, 1, kMaximumInteger / kDaySeconds},
                 {"alliance_invitation_seconds",
                  rules.alliance_invitation_seconds,
                  1,
                  kMaximumInteger}},
                error) ||
            !read_group(
                document,
                "combat",
                {{"respawn_seconds", rules.respawn_seconds, 1, kWeekSeconds},
                 {"protection_seconds", rules.protection_seconds, 0, kWeekSeconds},
                 {"capture_unlock_seconds", rules.capture_unlock_seconds, 0, kWeekSeconds},
                 {"hold_seconds", rules.hold_seconds, 1, kWeekSeconds},
                 {"flag_regen_seconds", rules.flag_regen_seconds, 1, kWeekSeconds},
                 {"flag_regen_points", rules.flag_regen_points, 0, kMaximumInteger},
                 {"match_score_flush_seconds", rules.match_score_flush_seconds, 1, kWeekSeconds}},
                error) ||
            !read_group(document,
                        "honor",
                        {{"activity_minimum", rules.activity_minimum, 0, kMaximumInteger},
                         {"honor_winner", rules.honor_winner, 0, kMaximumInteger / 4},
                         {"honor_ally", rules.honor_ally, 0, kMaximumInteger / 4},
                         {"honor_loser", rules.honor_loser, 0, kMaximumInteger / 4},
                         {"honor_flag_max", rules.honor_flag_max, 0, kMaximumInteger / 4},
                         {"honor_kill_max", rules.honor_kill_max, 0, kMaximumInteger / 4},
                         {"honor_activity_max", rules.honor_activity_max, 0, kMaximumInteger / 4}},
                        error)) {
            return false;
        }
        if (rules.declaration_seconds + rules.preparation_seconds + rules.battle_seconds >
                kWeekSeconds ||
            rules.admission_seconds > rules.preparation_seconds) {
            error = "Castle War windows overlap or admission is longer than preparation.";
            return false;
        }
        if (rules.alliance_min_days > rules.alliance_max_days) {
            error = "Alliance minimum duration exceeds its maximum duration.";
            return false;
        }
        if (rules.capture_unlock_seconds >= rules.battle_seconds) {
            error = "Flag capture must unlock before the scheduled end of battle.";
            return false;
        }
        if (const auto* tax = document.get("tax")) {
            if (!check_object(
                    *tax, {"percent", "max_defenses", "merchant_npc_indices"}, "tax", error)) {
                return false;
            }
            if (const auto* input = tax->get("percent")) {
                if (!read_array(*input, rules.tax_percent, 1, 1000, "tax.percent", error) ||
                    !(rules.tax_percent[0] <= rules.tax_percent[1] &&
                      rules.tax_percent[1] <= rules.tax_percent[2])) {
                    if (error.empty()) {
                        error = "tax.percent must list low, middle and high values in order.";
                    }
                    return false;
                }
            }
            if (const auto* input = tax->get("max_defenses")) {
                if (!read_integer(*input, 1, kMaximumInteger, rules.max_defenses)) {
                    error = "tax.max_defenses must be a positive integer.";
                    return false;
                }
            }
            if (const auto* input = tax->get("merchant_npc_indices")) {
                if (!input->is_array()) {
                    error = "tax.merchant_npc_indices must be an array.";
                    return false;
                }
                std::set<int> indices;
                for (const auto& value : input->array()) {
                    int index = 0;
                    if (!read_integer(value, 1, kMaximumCatalogIndex, index) ||
                        !indices.insert(index).second) {
                        error = "tax.merchant_npc_indices requires unique positive NPC indices.";
                        return false;
                    }
                    rules.merchant_npc_indices.push_back(index);
                }
            }
        }
        if (const auto* layout = document.get("layout")) {
            if (!read_layout(*layout, rules, error)) {
                return false;
            }
        } else if (rules.enabled) {
            error = "An enabled Castle War requires a layout.";
            return false;
        }
        *this = std::move(rules);
        return true;
    }

    CastleWarRound CastleWarRules::round(std::int64_t now) const {
        // Unix time starts on a Thursday. Shift to the configured server's Sunday-based week.
        const std::int64_t shift = 4 * kDaySeconds + utc_offset_minutes * 60;
        const std::int64_t second_of_week =
            ((now % kWeekSeconds + shift) % kWeekSeconds + kWeekSeconds) % kWeekSeconds;
        const int local_start =
            battle_weekday * kDaySeconds + battle_hour * 3600 + battle_minute * 60 + battle_second;
        std::int64_t start = now + (local_start - second_of_week);
        if (now < start - declaration_seconds - preparation_seconds) {
            // An early-week date can still be inside the preceding week's battle.
            if (now < start - kWeekSeconds + battle_seconds) {
                start -= kWeekSeconds;
            }
        } else if (now >= start + battle_seconds) {
            start += kWeekSeconds;
        }
        return {start - preparation_seconds - declaration_seconds,
                start - preparation_seconds,
                start - admission_seconds,
                start,
                start + battle_seconds};
    }

    CastleWarPhase CastleWarRules::phase(std::int64_t now) const {
        if (!enabled) {
            return CastleWarPhase::Idle;
        }
        const auto current = round(now);
        if (now < current.declaration) {
            return CastleWarPhase::Idle;
        }
        if (now < current.preparation) {
            return CastleWarPhase::Declaration;
        }
        if (now < current.admission) {
            return CastleWarPhase::Preparation;
        }
        if (now < current.battle) {
            return CastleWarPhase::Admission;
        }
        return CastleWarPhase::Battle;
    }

    bool CastleWarRules::contains(int map, int x, int y) const {
        return map == attack_camp.map && x >= bounds[0] && y >= bounds[1] && x <= bounds[2] &&
               y <= bounds[3];
    }
}  // namespace openkal::server
