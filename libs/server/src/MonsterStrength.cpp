/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/MonsterStrength.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

namespace openkal::server {
    namespace {
        constexpr std::string_view kStats[] = {
            "str", "hth", "int", "wis", "dex", "hp", "hit", "dodge", "absorb"};

        bool valid_percent(const platform::Json& value) {
            return value.is_number() && std::isfinite(value.number()) && value.number() >= -100.0;
        }

        bool valid_offset(const platform::Json& value) {
            return value.is_number() && std::isfinite(value.number()) &&
                   value.number() == std::trunc(value.number()) &&
                   value.number() >= std::numeric_limits<int>::min() &&
                   value.number() <= std::numeric_limits<int>::max();
        }

        long double rounded_stat(long double value) {
            // Decimal percentages can land a few binary floating-point bits below .5.
            const long double tolerance =
                4 * std::numeric_limits<double>::epsilon() * std::abs(value);
            return std::clamp(std::floor(value + 0.5L + tolerance),
                              0.0L,
                              static_cast<long double>(std::numeric_limits<int>::max()));
        }
    }  // namespace

    bool MonsterStrength::load_file(const std::string& path) {
        m_global_percent = 0.0;
        m_levels.clear();
        const auto fail = [&path](std::string_view reason) {
            log::error("MonsterStrength: " + path + ": " + std::string(reason));
            return false;
        };
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            return fail("cannot read configuration");
        }
        platform::Json root;
        std::string error;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
            !root.is_object()) {
            return fail("invalid JSON document");
        }
        if (root.at("schema").as_string() != "openkal.monster_strength/v1" ||
            !valid_percent(root.at("globalPercent")) || !root.at("levels").is_object()) {
            return fail("expected schema, globalPercent >= -100, and levels object");
        }
        for (const auto& entry : root.object()) {
            if (entry.first != "schema" && entry.first != "globalPercent" &&
                entry.first != "levels") {
                return fail("unknown configuration option: " + entry.first);
            }
        }
        std::map<int, LevelAdjustment> levels;
        for (const auto& entry : root.at("levels").object()) {
            int level = 0;
            const auto parsed =
                std::from_chars(entry.first.data(), entry.first.data() + entry.first.size(), level);
            if (parsed.ec != std::errc{} || parsed.ptr != entry.first.data() + entry.first.size() ||
                level < 1 || entry.first != std::to_string(level) || !entry.second.is_object()) {
                return fail("invalid level: " + entry.first);
            }
            LevelAdjustment adjustment;
            for (const auto& option : entry.second.object()) {
                if (option.first != "percent" && option.first != "stats") {
                    return fail("unknown level option: " + option.first);
                }
            }
            if (const auto* percent = entry.second.get("percent")) {
                if (!valid_percent(*percent)) {
                    return fail("level percent must be finite and >= -100");
                }
                adjustment.percent = percent->number();
            }
            if (const auto* stats = entry.second.get("stats")) {
                if (!stats->is_object()) {
                    return fail("level stats must be an object");
                }
                for (const auto& stat : stats->object()) {
                    if (std::find(std::begin(kStats), std::end(kStats), stat.first) ==
                        std::end(kStats)) {
                        return fail("unknown monster stat: " + stat.first);
                    }
                    if (!valid_offset(stat.second)) {
                        return fail("stat offsets must be signed 32-bit integers");
                    }
                    adjustment.offsets.emplace(stat.first, stat.second.as_int());
                }
            }
            levels.emplace(level, std::move(adjustment));
        }
        m_global_percent = root.at("globalPercent").number();
        m_levels = std::move(levels);
        return true;
    }

    int MonsterStrength::adjust(int value, int level, std::string_view stat) const {
        if (std::find(std::begin(kStats), std::end(kStats), stat) == std::end(kStats)) {
            return value;
        }
        const auto found = m_levels.find(level);
        const double level_percent = found == m_levels.end() ? 0.0 : found->second.percent;
        int offset = 0;
        if (found != m_levels.end()) {
            const auto offsets = found->second.offsets.find(stat);
            if (offsets != found->second.offsets.end()) {
                offset = offsets->second;
            }
        }
        // Preserve the loader behavior exactly for neutral configurations.
        if (m_global_percent == 0.0 && level_percent == 0.0 && offset == 0) {
            return value;
        }
        long double result = rounded_stat(value * (1.0L + m_global_percent / 100.0L));
        result = rounded_stat(result * (1.0L + level_percent / 100.0L));
        return static_cast<int>(rounded_stat(result + offset));
    }
}  // namespace openkal::server
