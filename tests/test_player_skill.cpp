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
#include "test_util.h"

#include <optional>
#include <unordered_map>
#include <vector>

int main() {
    using openkal::server::apply_auto_learn;
    using openkal::server::SkillDef;
    using openkal::server::sync_learned_skills;
    using openkal::server::target_skill_rank;
    using openkal::server::try_manual_skill_rank;
    using openkal::test::expect;

    SkillDef slash;
    slash.clazz = 0;
    slash.index = 3;
    slash.level_limit = 5;
    slash.max_level = 10;
    slash.grade_mask = 1;

    expect(target_skill_rank(slash, 4) == 0, "rank 0 below limit");
    expect(target_skill_rank(slash, 5) == 1, "rank 1 at limit");
    expect(target_skill_rank(slash, 6) == 2, "rank N is level - limit + 1");
    expect(target_skill_rank(slash, 14) == 10, "rank capped at maxLevel");
    expect(target_skill_rank(slash, 20) == 10, "rank stays capped");

    SkillDef rest;
    rest.clazz = 0;
    rest.index = 11;
    rest.level_limit = 0;
    rest.max_level = 3;
    rest.grade_mask = 1;
    expect(target_skill_rank(rest, 1) == 1, "rest rank 1 at level 1");
    expect(target_skill_rank(rest, 3) == 1, "rest does not rank with character level");
    expect(target_skill_rank(rest, 30) == 1, "rest job rank-up is not common auto-learn");

    SkillDef harmony;
    harmony.clazz = 0;
    harmony.index = 45;
    harmony.level_limit = 61;
    harmony.max_level = 2;
    harmony.grade_mask = 11;
    harmony.auto_learn_levels = {61, 70};
    expect(target_skill_rank(harmony, 60) == 0, "Call Harmony locked below 61");
    expect(target_skill_rank(harmony, 61) == 1, "Call Harmony rank 1 at 61");
    expect(target_skill_rank(harmony, 62) == 1, "Call Harmony stays rank 1 at 62");
    expect(target_skill_rank(harmony, 69) == 1, "Call Harmony stays rank 1 at 69");
    expect(target_skill_rank(harmony, 70) == 2, "Call Harmony rank 2 at 70");

    SkillDef perfect_defense;
    perfect_defense.clazz = 0;
    perfect_defense.index = 40;
    perfect_defense.level_limit = 70;
    perfect_defense.max_level = 6;
    perfect_defense.grade_mask = 43;
    perfect_defense.manual_learn = true;
    expect(target_skill_rank(perfect_defense, 70) == 0, "Perfect Defense does not auto-rank at 70");
    expect(target_skill_rank(perfect_defense, 100) == 0,
           "Perfect Defense stays unlearned without SU");

    SkillDef provocation;
    provocation.clazz = 0;
    provocation.index = 42;
    provocation.level_limit = 75;
    provocation.max_level = 5;
    provocation.grade_mask = 43;
    provocation.manual_learn = true;
    expect(target_skill_rank(provocation, 80) == 0, "Provocation of Blow does not auto-rank");

    SkillDef blessing;
    blessing.clazz = 0;
    blessing.index = 71;
    blessing.level_limit = 70;
    blessing.max_level = 7;
    blessing.grade_mask = 43;
    blessing.auto_learn_levels = {70, 75, 80, 85, 90, 95, 100};
    expect(target_skill_rank(blessing, 69) == 0, "Blessing locked below 70");
    expect(target_skill_rank(blessing, 70) == 1, "Blessing rank 1 at 70");
    expect(target_skill_rank(blessing, 75) == 2, "Blessing rank 2 at 75");
    expect(target_skill_rank(blessing, 100) == 7, "Blessing rank 7 at 100");

    SkillDef defense_str;
    defense_str.clazz = 0;
    defense_str.index = 39;
    defense_str.level_limit = 70;
    defense_str.max_level = 1;
    defense_str.grade_mask = 43;
    defense_str.auto_learn_levels = {70};
    expect(target_skill_rank(defense_str, 69) == 0, "Defense strengthening locked below 70");
    expect(target_skill_rank(defense_str, 70) == 1, "Defense strengthening auto-learns at 70");

    SkillDef mastery;
    mastery.clazz = 0;
    mastery.index = 2;
    mastery.level_limit = 1;
    mastery.max_level = 20;
    mastery.grade_mask = 1;

    SkillDef job;
    job.clazz = 0;
    job.index = 16;
    job.level_limit = 30;
    job.max_level = 1;
    job.grade_mask = 3;

    SkillDef gated;
    gated.clazz = 0;
    gated.index = 20;
    gated.level_limit = 10;
    gated.max_level = 1;
    gated.grade_mask = 1;
    gated.require_skill_index = 3;
    gated.require_skill_level = 1;

    const std::vector<SkillDef> defs{slash, mastery, job, gated};

    {
        std::unordered_map<int, int> learned;
        const auto changes = apply_auto_learn(learned, defs, 0, 5);
        expect(learned[3] == 1, "learn rank 1 at limit");
        expect(learned[2] == 5, "catch-up mastery to player level");
        expect(learned.find(16) == learned.end(), "grade 3 skipped at common");
        expect(learned.find(20) == learned.end(), "gated still below its level limit");
        bool saw_slash = false;
        bool saw_job = false;
        for (const auto& c : changes) {
            if (c.index == 3 && c.level == 1) {
                saw_slash = true;
            }
            if (c.index == 16) {
                saw_job = true;
            }
        }
        expect(saw_slash, "reports slash grant");
        expect(!saw_job, "does not report skipped job skill");
    }

    {
        std::unordered_map<int, int> learned;
        (void) apply_auto_learn(learned, defs, 0, 1);
        expect(learned.find(3) == learned.end(), "slash blocked below limit");
        expect(learned[2] == 1, "mastery rank 1 at level 1");
        expect(learned.find(20) == learned.end(), "gated blocked without prereq");
    }

    {
        std::unordered_map<int, int> learned;
        (void) apply_auto_learn(learned, defs, 0, 10);
        expect(learned[3] == 6, "slash rank at level 10");
        expect(learned[20] == 1, "prereq plus level limit unlocks gated skill");
        expect(learned.find(16) == learned.end(), "grade 3 still skipped");
    }

    {
        std::unordered_map<int, int> learned;
        learned[3] = 1;
        const auto changes = apply_auto_learn(learned, defs, 0, 8);
        expect(learned[3] == 4, "slash rank-up from existing");
        expect(learned[2] == 8, "mastery catch-up from start level");
        bool upgraded = false;
        for (const auto& c : changes) {
            if (c.index == 3 && c.level == 4) {
                upgraded = true;
            }
        }
        expect(upgraded, "reports slash rank-up");
        expect(changes.size() >= 1, "at least one change");
    }

    {
        std::unordered_map<int, int> learned;
        (void) apply_auto_learn(learned, defs, 1, 50);
        expect(learned.empty(), "other class learns nothing");
    }

    {
        std::unordered_map<int, int> learned;
        const std::vector<SkillDef> job_defs{slash, mastery, job, gated, rest};
        (void) apply_auto_learn(learned, job_defs, 0, 30, 1);
        expect(learned.find(16) == learned.end(), "grade 3 skipped at wandering");
        expect(learned[11] == 1, "rest stays rank 1 while wandering");
        const auto changes = apply_auto_learn(learned, job_defs, 0, 30, 3);
        expect(learned[16] == 1, "first-job skill at specialty 3");
        expect(learned[11] == 2, "rest ranks to 2 at first job");
        bool saw_job = false;
        for (const auto& c : changes) {
            if (c.index == 16 && c.level == 1) {
                saw_job = true;
            }
        }
        expect(saw_job, "reports first-job skill grant");
    }

    {
        std::unordered_map<int, int> learned;
        const std::vector<SkillDef> harmony_defs{harmony};
        (void) apply_auto_learn(learned, harmony_defs, 0, 61, 7);
        expect(learned.find(45) == learned.end(), "Call Harmony skipped on second-job A");
        (void) apply_auto_learn(learned, harmony_defs, 0, 61, 11);
        expect(learned[45] == 1, "Call Harmony rank 1 for Commander at 61");
        const auto changes = apply_auto_learn(learned, harmony_defs, 0, 70, 43);
        expect(learned[45] == 2, "Call Harmony rank 2 for General at 70");
        bool saw_rank2 = false;
        for (const auto& c : changes) {
            if (c.index == 45 && c.level == 2) {
                saw_rank2 = true;
            }
        }
        expect(saw_rank2, "reports Call Harmony rank 2");
    }

    {
        std::unordered_map<int, int> learned;
        const std::vector<SkillDef> general_defs{
            perfect_defense, provocation, blessing, defense_str};
        (void) apply_auto_learn(learned, general_defs, 0, 80, 43);
        expect(learned.find(40) == learned.end(), "Perfect Defense is not auto-learned");
        expect(learned.find(42) == learned.end(), "Provocation of Blow is not auto-learned");
        expect(learned[71] == 3, "Blessing auto-ranks at 80");
        expect(learned[39] == 1, "Defense strengthening auto-learns at 70+");
    }

    {
        std::unordered_map<int, int> learned;
        std::int32_t su = 2;
        const std::vector<SkillDef> defs{perfect_defense};
        expect(!try_manual_skill_rank(learned, defs, 0, 69, 43, 40, su),
               "Perfect Defense rejected below level 70");
        const auto first = try_manual_skill_rank(learned, defs, 0, 70, 43, 40, su);
        expect(first.has_value() && first->index == 40 && first->level == 1 && su == 1,
               "Perfect Defense first rank spends one SU");
        expect(learned[40] == 1, "Perfect Defense stored at rank 1");
        const auto second = try_manual_skill_rank(learned, defs, 0, 70, 43, 40, su);
        expect(second.has_value() && second->level == 2 && su == 0,
               "Perfect Defense rank 2 spends the last SU");
        expect(!try_manual_skill_rank(learned, defs, 0, 70, 43, 40, su),
               "Perfect Defense rank-up needs SU");
    }

    {
        std::unordered_map<int, int> learned;
        std::int32_t su = 3;
        const std::vector<SkillDef> defs{perfect_defense, provocation};
        expect(try_manual_skill_rank(learned, defs, 0, 80, 43, 40, su).has_value(),
               "Perfect Defense learns with SU");
        expect(try_manual_skill_rank(learned, defs, 0, 80, 43, 40, su).has_value(),
               "Perfect Defense ranks to 2 with SU");
        expect(try_manual_skill_rank(learned, defs, 0, 80, 43, 42, su).has_value(),
               "Provocation of Blow learns with SU");
        expect(learned[40] == 2 && learned[42] == 1, "SU ranks stored before sync");
        const auto keep = sync_learned_skills(learned, defs, 0, 80, 43);
        expect(learned[40] == 2, "sync keeps Perfect Defense SU rank");
        expect(learned[42] == 1, "sync keeps Provocation of Blow SU rank");
        bool changed_manual = false;
        for (const auto& c : keep) {
            if (c.index == 40 || c.index == 42) {
                changed_manual = true;
            }
        }
        expect(!changed_manual, "sync does not rewrite eligible SU ranks");
        const auto drop = sync_learned_skills(learned, defs, 0, 70, 43);
        expect(learned[40] == 2, "Perfect Defense stays at 70");
        expect(learned.find(42) == learned.end(), "Provocation of Blow forgotten below 75");
        bool saw_pob_forget = false;
        for (const auto& c : drop) {
            if (c.index == 42 && c.level == 0) {
                saw_pob_forget = true;
            }
        }
        expect(saw_pob_forget, "reports Provocation of Blow unlearn below limit");
        (void) sync_learned_skills(learned, defs, 0, 69, 43);
        expect(learned.find(40) == learned.end(), "Perfect Defense forgotten below 70");
    }

    {
        std::unordered_map<int, int> learned;
        (void) apply_auto_learn(learned, defs, 0, 14);
        expect(learned[3] == 10, "slash at cap before sync down");
        expect(learned[2] == 14, "mastery at level 14");
        expect(learned[20] == 1, "gated learned at 14");
        const auto changes = sync_learned_skills(learned, defs, 0, 6);
        expect(learned[3] == 2, "slash downgrades to level 6 rank");
        expect(learned[2] == 6, "mastery downgrades to player level");
        expect(learned.find(20) == learned.end(), "gated forgotten below its limit");
        bool saw_forget = false;
        bool saw_slash_down = false;
        for (const auto& c : changes) {
            if (c.index == 20 && c.level == 0) {
                saw_forget = true;
            }
            if (c.index == 3 && c.level == 2) {
                saw_slash_down = true;
            }
        }
        expect(saw_forget, "reports gated unlearn");
        expect(saw_slash_down, "reports slash rank-down");
    }

    {
        std::unordered_map<int, int> learned;
        const std::vector<SkillDef> job_defs{slash, mastery, job, gated, rest};
        (void) apply_auto_learn(learned, job_defs, 0, 30, 3);
        expect(learned[11] == 2, "rest rank 2 at first job");
        expect(learned[16] == 1, "job skill at first job");
        const auto changes = sync_learned_skills(learned, job_defs, 0, 20, 3);
        expect(learned[11] == 1, "rest downgrades below job rank-up level");
        expect(learned.find(16) == learned.end(), "job skill forgotten below 30");
        bool saw_job_forget = false;
        for (const auto& c : changes) {
            if (c.index == 16 && c.level == 0) {
                saw_job_forget = true;
            }
        }
        expect(saw_job_forget, "reports first-job skill unlearn");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
