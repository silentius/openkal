/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/GuildRules.h"

#include "openkal/protocol/Guild.h"
#include "openkal/protocol/Party.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>

namespace openkal::server {
    namespace {
        using J = platform::Json;
        constexpr const char* kDefaults = R"JSON({
"schema":"openkal.guilds/v1","enabled":true,
"creation":{"minimum_level":30,"minimum_members":1,"require_party":false,"require_full_party":false,"require_consent":true,"consent_seconds":60,"fee_zamogeon":100000},
"membership":{"minimum_level":1,"application_fee_zamogeon":0,"admission_fee_zamogeon":0,"application_lifetime_seconds":604800,"rejoin_lock_seconds":86400,"temporary_member_limit":10,"count_temporary_members":false,"maximum_members":0},
"rank_quotas":{"sub_leader":{"maximum":1},"head_chief":{"members_per_slot":50,"maximum":0},"chief":{"members_per_slot":10,"maximum":0}},
"grades":{"minimum_members_by_grade":[1,26,40,54,70,90,110,130,130,130],"on_member_loss":"penalty"},
"features":{"name_display_minimum_grade":2,"flag_minimum_grade":3,"siege_minimum_grade":4,"tax_minimum_grade":4,"raid_minimum_grade":4},
"names":{"maximum_latin":14,"maximum_hangul":7,"maximum_text":512},
"time":{"day_offset_minutes":0,"ranking_refresh_seconds":86400},
"points":{"enabled":true,"low_level_end":19,"low_level_points":1,"level_offset":10,"connected_hour_points":10,"maximum":2000000000},
"buffs":{"enabled":true,"minimum_grade":2,"primary_per_grade":0,"hp_per_grade":0,"defense_per_grade":0},
"daily_reward":{"enabled":true,"minimum_grade":2,"zamogeon_per_grade":1000,"minimum_membership_seconds":86400},
"ranks":{"titles":["","Leader","Sub-Leader","HeadChief","Chief","OfficialMember"],"permissions":[0,31,0,0,0,0]}
})JSON";
        bool merge(J& base, const J& input, std::string prefix, std::string& error) {
            if (!input.is_object()) {
                error = prefix + " must be an object";
                return false;
            }
            for (const auto& [key, value] : input.object()) {
                auto it = base.object().find(key);
                const auto path = prefix.empty() ? key : prefix + "." + key;
                if (it == base.object().end()) {
                    error = "Unknown guild setting: " + path;
                    return false;
                }
                if (value.type() != it->second.type()) {
                    error = "Invalid type: " + path;
                    return false;
                }
                if (value.is_object()) {
                    if (!merge(it->second, value, path, error)) {
                        return false;
                    }
                } else {
                    it->second = value;
                }
            }
            return true;
        }
        bool integers(const J& value, std::string path, std::string& error) {
            if (value.is_number() &&
                (!std::isfinite(value.number()) || value.number() != std::floor(value.number()) ||
                 value.number() < 0 || value.number() > 2000000000)) {
                error = "Invalid nonnegative integer: " + path;
                return false;
            }
            if (value.is_object()) {
                for (const auto& [key, v] : value.object()) {
                    if (!integers(v, path + "." + key, error)) {
                        return false;
                    }
                }
            }
            if (value.is_array()) {
                for (const auto& v : value.array()) {
                    if (!integers(v, path, error)) {
                        return false;
                    }
                }
            }
            return true;
        }
    }  // namespace
    bool GuildRules::load(std::string_view path, std::string& error) {
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file) {
            error = "Cannot read guild rules: " + std::string(path);
            return false;
        }
        const std::string text{std::istreambuf_iterator<char>(file), {}};
        J document;
        return J::parse(text, document, &error) && parse(document, error);
    }
    bool GuildRules::parse(const J& document, std::string& error) {
        error.clear();
        J next = J::parse(kDefaults);
        if (!merge(next, document, "", error) || !integers(next, "guilds", error)) {
            return false;
        }
        const auto fail = [&](std::string e) {
            error = std::move(e);
            return false;
        };
        if (next.at("schema").as_string() != "openkal.guilds/v1") {
            return fail("Unsupported guild schema");
        }
        const auto& grades = next.at("grades").at("minimum_members_by_grade").array();
        if (grades.size() != 10 || grades.front().as_int() != 1) {
            return fail("grades.minimum_members_by_grade requires 10 thresholds beginning at 1");
        }
        int previous = 1;
        for (const auto& g : grades) {
            if (!g.is_number() || g.as_int() < previous) {
                return fail("grades thresholds must be positive and nondecreasing");
            }
            previous = g.as_int();
        }
        const auto loss = next.at("grades").at("on_member_loss").as_string();
        if (loss != "penalty" && loss != "downgrade") {
            return fail("grades.on_member_loss must be penalty or downgrade");
        }
        const auto get = [&](const char* g, const char* k) { return next.at(g).at(k).as_int(); };
        if (get("creation", "minimum_members") < 1 || get("creation", "minimum_level") < 1 ||
            get("creation", "minimum_level") > 255 || get("membership", "minimum_level") < 1 ||
            get("membership", "minimum_level") > 255) {
            return fail("Invalid creation/membership minimum");
        }
        const bool party = next.at("creation").at("require_party").boolean();
        if ((next.at("creation").at("require_full_party").boolean() && !party) ||
            (!party && get("creation", "minimum_members") != 1) ||
            (party &&
             get("creation", "minimum_members") > static_cast<int>(protocol::kPartyCapacity))) {
            return fail("Incompatible creation party requirements");
        }
        for (const auto* r : {"head_chief", "chief"}) {
            if (next.at("rank_quotas").at(r).at("members_per_slot").as_int() < 1) {
                return fail(std::string("Invalid quota divisor: ") + r);
            }
        }
        if (next.at("rank_quotas").at("sub_leader").at("maximum").as_int() > 1) {
            return fail("At most one sub-leader is supported");
        }
        for (const auto& [key, value] : next.at("features").object()) {
            if (value.as_int() < 1 || value.as_int() > 10) {
                return fail("Invalid feature grade: " + key);
            }
        }
        if (get("names", "maximum_latin") < 1 ||
            get("names", "maximum_latin") > static_cast<int>(protocol::kGuildLabelMaximum) ||
            get("names", "maximum_hangul") < 1 ||
            get("names", "maximum_hangul") > static_cast<int>(protocol::kGuildLabelMaximum) ||
            get("names", "maximum_text") > 1024) {
            return fail("Invalid name/text bounds");
        }
        if (get("time", "day_offset_minutes") > 1439 ||
            get("time", "ranking_refresh_seconds") < 1 || get("creation", "consent_seconds") < 1 ||
            get("membership", "application_lifetime_seconds") < 1 || get("points", "maximum") < 1) {
            return fail("Invalid time or point bound");
        }
        for (const auto* group : {"buffs", "daily_reward"}) {
            if (get(group, "minimum_grade") < 1 || get(group, "minimum_grade") > 10) {
                return fail(std::string("Invalid minimum grade: ") + group);
            }
        }
        if (next.at("ranks").at("titles").array().size() != 6 ||
            next.at("ranks").at("permissions").array().size() != 6) {
            return fail("ranks requires six titles and permission values including index zero");
        }
        const auto& titles = next.at("ranks").at("titles").array();
        for (std::size_t i = 0; i < titles.size(); ++i) {
            if (!titles[i].is_string() ||
                (i == 0 ? !titles[i].string().empty()
                        : !protocol::guild_label_valid(titles[i].string()))) {
                return fail("Rank titles require 1 to 14 characters without spaces");
            }
        }
        for (const auto& mask : next.at("ranks").at("permissions").array()) {
            if (!mask.is_number() || mask.as_int() > 31) {
                return fail("Invalid rank permission mask");
            }
        }
        if (next.at("ranks").at("permissions").array()[1].as_int() != 31) {
            return fail("Leader must retain every permission");
        }
        m_document = std::move(next);
        return true;
    }
    std::int64_t GuildRules::number(std::string_view group, std::string_view key) const {
        return static_cast<std::int64_t>(m_document.at(group).at(key).number());
    }
    bool GuildRules::flag(std::string_view group, std::string_view key) const {
        return m_document.at(group).at(key).boolean();
    }
    bool GuildRules::enabled() const {
        return m_document.at("enabled").boolean();
    }
    int GuildRules::grade(std::size_t count) const {
        int grade = 1, index = 0;
        for (const auto& v : m_document.at("grades").at("minimum_members_by_grade").array()) {
            ++index;
            if (count >= static_cast<std::size_t>(v.as_int())) {
                grade = index;
            }
        }
        return grade;
    }
    bool GuildRules::penalized(int grade, std::size_t count) const {
        return grade > 1 && grade <= 10 &&
               count < static_cast<std::size_t>(m_document.at("grades")
                                                    .at("minimum_members_by_grade")
                                                    .array()[grade - 1]
                                                    .as_int());
    }
    std::size_t GuildRules::quota(int rank, std::size_t count) const {
        if (rank == 1) {
            return 1;
        }
        if (rank == 5) {
            return std::numeric_limits<std::size_t>::max();
        }
        const auto& r = m_document.at("rank_quotas")
                            .at(rank == 2   ? "sub_leader"
                                : rank == 3 ? "head_chief"
                                            : "chief");
        if (rank == 2) {
            return static_cast<std::size_t>(r.at("maximum").as_int());
        }
        auto slots = count / static_cast<std::size_t>(r.at("members_per_slot").as_int());
        const auto maximum = r.at("maximum").as_int();
        return maximum ? std::min(slots, static_cast<std::size_t>(maximum)) : slots;
    }
    std::string GuildRules::name_key(std::string_view name) const {
        if (!protocol::guild_label_valid(name)) {
            return {};
        }
        std::string key;
        std::size_t count = 0;
        bool hangul = false, latin = false;
        for (std::size_t i = 0; i < name.size();) {
            auto c = static_cast<unsigned char>(name[i++]);
            ++count;
            if (c < 128) {
                latin = true;
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
                    return {};
                }
                key += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
            } else {
                hangul = true;
                if ((c & 0xF0) != 0xE0 || i + 1 >= name.size()) {
                    return {};
                }
                auto code = ((c & 15) << 12) | ((static_cast<unsigned char>(name[i]) & 63) << 6) |
                            (static_cast<unsigned char>(name[i + 1]) & 63);
                if (code < 0xAC00 || code > 0xD7A3) {
                    return {};
                }
                key.append(name.substr(i - 1, 3));
                i += 2;
            }
        }
        if ((hangul && latin) ||
            count > static_cast<std::size_t>(
                        number("names", hangul ? "maximum_hangul" : "maximum_latin"))) {
            return {};
        }
        return key;
    }
}  // namespace openkal::server
