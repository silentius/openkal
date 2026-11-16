/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/EssenceBox.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/server/CollectionEquipment.h"

#include <cmath>

namespace openkal::server {
    bool EssenceBox::load_file(const std::string& path) {
        *this = {};
        const auto bytes = platform::read_file(path);
        return bytes && load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool EssenceBox::load_json_text(std::string_view text) {
        *this = {};
        platform::Json root;
        if (!platform::Json::parse(text, root) || !root.is_object()) {
            return false;
        }
        const auto& count = root.at("essence_count");
        const auto& minimum = count.at("min");
        const auto& maximum = count.at("max");
        const auto valid_count = [](const platform::Json& value) {
            return value.is_number() && std::isfinite(value.number()) && value.number() >= 1 &&
                   value.number() <= 216 && std::floor(value.number()) == value.number();
        };
        if (!valid_count(minimum) || !valid_count(maximum) || minimum.number() > maximum.number()) {
            return false;
        }
        const auto& grades = root.at("grade_percent");
        const std::array<const char*, 3> names{{"low", "middle", "high"}};
        std::array<double, 3> chances{};
        double total = 0;
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto& chance = grades.at(names[i]);
            if (!chance.is_number() || !std::isfinite(chance.number()) || chance.number() < 0 ||
                chance.number() > 100) {
                return false;
            }
            chances[i] = chance.number();
            total += chances[i];
        }
        if (std::abs(total - 100.0) > 1e-9) {
            return false;
        }
        m_min_count = minimum.as_int();
        m_max_count = maximum.as_int();
        m_chances = chances;
        return true;
    }

    int EssenceBox::item_for_roll(double roll) const {
        if (m_max_count == 0 || !std::isfinite(roll) || roll < 0 || roll >= 1) {
            return 0;
        }
        double accumulated = 0;
        int last_positive = 0;
        for (std::size_t i = 0; i < m_chances.size(); ++i) {
            accumulated += m_chances[i];
            if (m_chances[i] > 0) {
                last_positive = kEssenceFirst + static_cast<int>(i);
            }
            if (roll * 100.0 < accumulated) {
                return kEssenceFirst + static_cast<int>(i);
            }
        }
        return last_positive;
    }
}  // namespace openkal::server
