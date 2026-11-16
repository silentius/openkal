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

#include "openkal/server/world/GameWorld.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** One skill index whose learned level changed. */
    struct SkillRankChange {
        /** Skill index within the class. */
        int index = 0;

        /** New learned level. */
        int level = 0;
    };

    /**
     * Highest auto-learn rank for a definition at the given player level.
     * Rank N requires player_level >= level_limit + N - 1, capped at max_level,
     * unless `auto_learn_levels` lists an explicit level per rank.
     * Rest (index 11) stays at rank 1; compatibility behavior only ranks up via job.
     * Skills with `manual_learn` stay at rank 0 until C2S LearnSkill / SkillUp.
     * @param def Skill definition.
     * @param player_level Current character level.
     * @return Target rank, or 0 when the skill cannot be learned yet.
     */
    [[nodiscard]] int target_skill_rank(const SkillDef& def, int player_level);

    /**
     * Spends one SU point to learn or raise a `manual_learn` skill one rank.
     * @param learned In/out map of skill index to learned level.
     * @param defs Loaded skill definitions.
     * @param clazz Player class index.
     * @param player_level Current character level.
     * @param specialty Job bitmask.
     * @param skill_index Skill index to learn or raise.
     * @param su_point In/out remaining SU points.
     * @return The new rank when the spend succeeds.
     */
    [[nodiscard]] std::optional<SkillRankChange>
    try_manual_skill_rank(std::unordered_map<int, int>& learned,
                          const std::vector<SkillDef>& defs,
                          int clazz,
                          int player_level,
                          int specialty,
                          int skill_index,
                          std::int32_t& su_point);

    /**
     * Grants and ranks up every eligible class skill for the current level.
     * A skill is eligible when every bit of grade_mask is set in specialty.
     * Prerequisite skills must already be learned. Repeats until no further grants
     * occur. Rest (index 11) ranks to 2 after the first-job bit at level 30.
     * @param learned In/out map of skill index to learned level.
     * @param defs Loaded skill definitions.
     * @param clazz Player class index.
     * @param player_level Current character level.
     * @param specialty Job bitmask (bit 0 = wandering). Grade N requires all bits.
     * @return Entries whose stored level increased (final rank per index).
     */
    [[nodiscard]] std::vector<SkillRankChange>
    apply_auto_learn(std::unordered_map<int, int>& learned,
                     const std::vector<SkillDef>& defs,
                     int clazz,
                     int player_level,
                     int specialty = 1);

    /**
     * Caps, forgets, and re-grants class skills so learned ranks match the
     * current level and specialty. Forgotten skills are reported with level 0.
     * `manual_learn` ranks bought with SU are kept while the skill stays eligible.
     * @param learned In/out map of skill index to learned level.
     * @param defs Loaded skill definitions.
     * @param clazz Player class index.
     * @param player_level Current character level.
     * @param specialty Job bitmask (bit 0 = wandering). Grade N requires all bits.
     * @return Entries whose stored level changed (0 = unlearned).
     */
    [[nodiscard]] std::vector<SkillRankChange>
    sync_learned_skills(std::unordered_map<int, int>& learned,
                        const std::vector<SkillDef>& defs,
                        int clazz,
                        int player_level,
                        int specialty = 1);

}  // namespace openkal::server
