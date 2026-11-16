/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/BofCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace openkal::server {
    namespace {

        [[nodiscard]] std::string ascii_lower(std::string_view text) {
            std::string out(text);
            for (char& ch : out) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return out;
        }

    }  // namespace

    bool BofCatalog::third_stat_is_intel(int clazz) {
        return clazz == 1;
    }

    bool BofCatalog::load_file(const std::string& path) {
        m_ready = false;
        m_tiers.clear();
        m_armor_parts.clear();
        m_fx_levels.clear();
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::warn("BofCatalog: missing " + path);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool BofCatalog::load_json_text(std::string_view text) {
        m_ready = false;
        m_tiers.clear();
        m_armor_parts.clear();
        m_fx_levels.clear();
        std::string err;
        platform::Json root;
        if (!platform::Json::parse(text, root, &err)) {
            log::warn("BofCatalog: parse failed: " + err);
            return false;
        }
        return merge_root(root);
    }

    bool BofCatalog::merge_root(const platform::Json& root) {
        if (!root.is_object()) {
            return false;
        }
        m_min_grade = root.at("minGrade").as_int(46);
        m_half_count = root.at("halfCount").as_int(3);
        m_full_count = root.at("fullCount").as_int(5);
        m_success_rate = std::clamp(root.at("successRate").as_int(100), 0, 100);
        m_half_defense_percent = root.at("halfDefensePercent").as_int(3);
        m_full_hp_mp_percent = root.at("fullHpMpPercent").as_int(10);
        m_full_resist_percent = root.at("fullResistPercent").as_int(10);

        const platform::Json& parts = root.at("armorParts");
        if (parts.is_array()) {
            for (const auto& j : parts.array()) {
                const std::string token = ascii_lower(j.as_string());
                if (!token.empty()) {
                    m_armor_parts.insert(token);
                }
            }
        }
        if (m_armor_parts.empty()) {
            m_armor_parts = {
                "helmet", "hat", "upperarmor", "lowerarmor", "pants", "gauntlet", "boots"};
        }

        const platform::Json& fx = root.at("fxLevels");
        if (fx.is_array()) {
            for (const auto& j : fx.array()) {
                const int level = j.as_int(0);
                if (level > 0) {
                    m_fx_levels.push_back(level);
                }
            }
        }
        if (m_fx_levels.empty()) {
            m_fx_levels = {46, 50, 55, 60, 65};
        }
        std::sort(m_fx_levels.begin(), m_fx_levels.end());

        const platform::Json& tiers = root.at("tiers");
        if (tiers.is_array()) {
            for (const auto& j : tiers.array()) {
                if (!j.is_object()) {
                    continue;
                }
                BofTier row;
                row.set_grade(j.at("grade").as_int(0));
                row.set_piece(j.at("piece").as_int(0));
                row.set_half_set(j.at("halfSet").as_int(0));
                row.set_full_set(j.at("fullSet").as_int(0));
                if (row.grade() <= 0) {
                    continue;
                }
                m_tiers[row.grade()] = row;
            }
        }

        m_ready = m_min_grade > 0;
        if (!m_ready) {
            log::warn("BofCatalog: ignored config (minGrade missing)");
        }
        return m_ready;
    }

    bool BofCatalog::is_bof_armor_part(std::string_view class_small) const {
        return m_armor_parts.find(ascii_lower(class_small)) != m_armor_parts.end();
    }

    BofTier BofCatalog::fallback_tier(int grade) const {
        BofTier row;
        if (grade < m_min_grade) {
            return row;
        }
        const int piece = std::max(1, grade / 5 - 8);
        row.set_grade(grade);
        row.set_piece(piece);
        row.set_half_set(piece + 4);
        row.set_full_set(piece + 9);
        return row;
    }

    BofTier BofCatalog::tier_for_grade(int grade) const {
        if (grade < m_min_grade) {
            return {};
        }
        const auto it = m_tiers.find(grade);
        if (it != m_tiers.end()) {
            return it->second;
        }
        return fallback_tier(grade);
    }

    int BofCatalog::fx_level_for_grade(int grade) const {
        int best = 0;
        for (int level : m_fx_levels) {
            if (level <= grade) {
                best = level;
            }
        }
        return best;
    }

    BofSetEval BofCatalog::evaluate_set(const std::vector<BofWornPiece>& pieces) const {
        BofSetEval out;
        std::unordered_map<int, int> counts;
        for (const auto& piece : pieces) {
            if (piece.grade() < m_min_grade) {
                continue;
            }
            ++counts[piece.grade()];
        }
        int best_grade = 0;
        int best_count = 0;
        for (const auto& [grade, count] : counts) {
            if (count > best_count || (count == best_count && grade > best_grade)) {
                best_count = count;
                best_grade = grade;
            }
        }
        out.set_grade(best_grade);
        out.set_count(best_count);
        if (best_count >= m_full_count) {
            out.set_completeness(2);
        } else if (best_count >= m_half_count) {
            out.set_completeness(1);
        }
        return out;
    }

    BofCombatBonus BofCatalog::compute_bonus(int clazz,
                                             const std::vector<BofWornPiece>& pieces) const {
        BofCombatBonus bonus;
        const bool intel = third_stat_is_intel(clazz);
        int str = 0;
        int hth = 0;
        int third = 0;
        for (const auto& piece : pieces) {
            const BofTier tier = tier_for_grade(piece.grade());
            str += tier.piece();
            hth += tier.piece();
            third += tier.piece();
        }
        const BofSetEval set = evaluate_set(pieces);
        bonus.set_completeness(set.completeness());
        bonus.set_set_grade(set.grade());
        if (set.completeness() > 0) {
            const BofTier set_tier = tier_for_grade(set.grade());
            const int extra = set.completeness() >= 2 ? set_tier.full_set() : set_tier.half_set();
            str += extra;
            hth += extra;
            third += extra;
            bonus.set_defense_percent(m_half_defense_percent);
            bonus.set_fx_level(fx_level_for_grade(set.grade()));
            if (set.completeness() >= 2) {
                bonus.set_hp_mp_percent(m_full_hp_mp_percent);
                bonus.set_resist_percent(m_full_resist_percent);
            }
        }
        bonus.set_str(str);
        bonus.set_hth(hth);
        if (intel) {
            bonus.set_intl(third);
        } else {
            bonus.set_dex(third);
        }
        return bonus;
    }

    BofApplyResult BofCatalog::can_apply(bool is_defense,
                                         std::string_view class_small,
                                         int grade,
                                         bool equipped,
                                         bool already_bof) const {
        if (equipped) {
            return BofApplyResult::Equipped;
        }
        if (already_bof) {
            return BofApplyResult::AlreadyApplied;
        }
        if (!is_defense || !is_bof_armor_part(class_small) || grade < m_min_grade) {
            return BofApplyResult::InvalidTarget;
        }
        return BofApplyResult::Ok;
    }

    bool BofCatalog::roll_success(int roll) const {
        if (m_success_rate >= 100) {
            return true;
        }
        if (m_success_rate <= 0) {
            return false;
        }
        return roll >= 1 && roll <= m_success_rate;
    }

}  // namespace openkal::server
