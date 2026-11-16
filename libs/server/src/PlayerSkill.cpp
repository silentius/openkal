/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/PlayerSkill.h"

#include <algorithm>

namespace openkal::server {

    namespace {

        [[nodiscard]] int wanted_auto_rank(const SkillDef& def, int player_level, int specialty) {
            int want = target_skill_rank(def, player_level);
            // ChangeOccupationAutoLearnSkill: SkillUp(REST) at first job.
            if (def.index == 11 && (static_cast<unsigned>(std::max(0, specialty)) & 2u) != 0 &&
                player_level >= 30) {
                want = std::min(std::max(1, def.max_level), 2);
            }
            return want;
        }

        [[nodiscard]] bool skill_eligible(const SkillDef& def,
                                          int clazz,
                                          int specialty,
                                          const std::unordered_map<int, int>& learned) {
            if (def.clazz != clazz) {
                return false;
            }
            const unsigned spec = static_cast<unsigned>(std::max(0, specialty));
            const unsigned grade = static_cast<unsigned>(std::max(0, def.grade_mask));
            if (grade != 0 && (spec & grade) != grade) {
                return false;
            }
            if (def.require_skill_index != 0) {
                const auto pre = learned.find(def.require_skill_index);
                const int have = pre == learned.end() ? 0 : pre->second;
                const int need = def.require_skill_level > 0 ? def.require_skill_level : 1;
                if (have < need) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] const SkillDef*
        find_class_skill(const std::vector<SkillDef>& defs, int clazz, int index) {
            for (const SkillDef& def : defs) {
                if (def.clazz == clazz && def.index == index) {
                    return &def;
                }
            }
            return nullptr;
        }

    }  // namespace

    int target_skill_rank(const SkillDef& def, int player_level) {
        if (def.manual_learn) {
            return 0;
        }
        const int max_lv = std::max(1, def.max_level);
        if (!def.auto_learn_levels.empty()) {
            int rank = 0;
            for (std::size_t i = 0; i < def.auto_learn_levels.size() && rank < max_lv; ++i) {
                const int need = def.auto_learn_levels[i];
                if (need > 0 && player_level >= need) {
                    rank = static_cast<int>(i) + 1;
                }
            }
            return rank;
        }
        const int need = def.level_limit <= 0 ? 1 : def.level_limit;
        if (player_level < need) {
            return 0;
        }
        // Rest (PS_*_REST / compatibility behavior): sit/stand only. CanSkillUp allows 1→2
        // solely for a first-job specialty at character level 30 — not per-level ranks.
        if (def.index == 11) {
            return 1;
        }
        const int rank = player_level - need + 1;
        return std::min(max_lv, rank);
    }

    std::vector<SkillRankChange> apply_auto_learn(std::unordered_map<int, int>& learned,
                                                  const std::vector<SkillDef>& defs,
                                                  int clazz,
                                                  int player_level,
                                                  int specialty) {
        const std::unordered_map<int, int> before = learned;
        const unsigned spec = static_cast<unsigned>(std::max(0, specialty));
        bool progressed = true;
        while (progressed) {
            progressed = false;
            for (const SkillDef& def : defs) {
                if (def.clazz != clazz) {
                    continue;
                }
                const unsigned grade = static_cast<unsigned>(std::max(0, def.grade_mask));
                if (grade != 0 && (spec & grade) != grade) {
                    continue;
                }
                if (def.require_skill_index != 0) {
                    const auto pre = learned.find(def.require_skill_index);
                    const int have = pre == learned.end() ? 0 : pre->second;
                    const int need = def.require_skill_level > 0 ? def.require_skill_level : 1;
                    if (have < need) {
                        continue;
                    }
                }
                const int want = wanted_auto_rank(def, player_level, specialty);
                if (want < 1) {
                    continue;
                }
                const auto it = learned.find(def.index);
                const int cur = it == learned.end() ? 0 : it->second;
                if (want > cur) {
                    learned[def.index] = want;
                    progressed = true;
                }
            }
        }

        std::vector<SkillRankChange> changes;
        for (const auto& [index, level] : learned) {
            const auto it = before.find(index);
            if (it == before.end() || it->second != level) {
                SkillRankChange change;
                change.index = index;
                change.level = level;
                changes.push_back(change);
            }
        }
        return changes;
    }

    std::vector<SkillRankChange> sync_learned_skills(std::unordered_map<int, int>& learned,
                                                     const std::vector<SkillDef>& defs,
                                                     int clazz,
                                                     int player_level,
                                                     int specialty) {
        const std::unordered_map<int, int> before = learned;
        bool progressed = true;
        while (progressed) {
            progressed = false;
            for (auto it = learned.begin(); it != learned.end();) {
                const SkillDef* def = find_class_skill(defs, clazz, it->first);
                if (def && def->manual_learn) {
                    const int need = def->level_limit <= 0 ? 1 : def->level_limit;
                    if (!skill_eligible(*def, clazz, specialty, learned) || player_level < need) {
                        it = learned.erase(it);
                        progressed = true;
                        continue;
                    }
                    ++it;
                    continue;
                }
                int want = 0;
                if (def && skill_eligible(*def, clazz, specialty, learned)) {
                    want = wanted_auto_rank(*def, player_level, specialty);
                }
                if (want < 1) {
                    it = learned.erase(it);
                    progressed = true;
                    continue;
                }
                if (it->second > want) {
                    it->second = want;
                    progressed = true;
                }
                ++it;
            }
        }
        (void) apply_auto_learn(learned, defs, clazz, player_level, specialty);

        std::vector<SkillRankChange> changes;
        for (const auto& [index, level] : before) {
            const auto it = learned.find(index);
            const int now = it == learned.end() ? 0 : it->second;
            if (now != level) {
                SkillRankChange change;
                change.index = index;
                change.level = now;
                changes.push_back(change);
            }
        }
        for (const auto& [index, level] : learned) {
            if (before.find(index) == before.end()) {
                SkillRankChange change;
                change.index = index;
                change.level = level;
                changes.push_back(change);
            }
        }
        return changes;
    }

    std::optional<SkillRankChange> try_manual_skill_rank(std::unordered_map<int, int>& learned,
                                                         const std::vector<SkillDef>& defs,
                                                         int clazz,
                                                         int player_level,
                                                         int specialty,
                                                         int skill_index,
                                                         std::int32_t& su_point) {
        if (su_point <= 0) {
            return std::nullopt;
        }
        const SkillDef* def = find_class_skill(defs, clazz, skill_index);
        if (!def || !def->manual_learn || !skill_eligible(*def, clazz, specialty, learned)) {
            return std::nullopt;
        }
        const int max_lv = std::max(1, def->max_level);
        const int need = def->level_limit <= 0 ? 1 : def->level_limit;
        if (player_level < need) {
            return std::nullopt;
        }
        const auto it = learned.find(skill_index);
        const int cur = it == learned.end() ? 0 : it->second;
        if (cur >= max_lv) {
            return std::nullopt;
        }
        const int next = cur + 1;
        learned[skill_index] = next;
        --su_point;
        SkillRankChange change;
        change.index = skill_index;
        change.level = next;
        return change;
    }

}  // namespace openkal::server
