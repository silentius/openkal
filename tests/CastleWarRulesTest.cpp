/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Json.h"
#include "openkal/server/CastleWarRules.h"
#include "test_util.h"

namespace {
    using openkal::platform::Json;
    using openkal::server::CastleWarPhase;
    using openkal::server::CastleWarRules;
    using openkal::test::expect;

    Json layout_fixture() {
        return Json::parse(R"({
          "schema": "openkal.castle_war/v1",
          "enabled": true,
          "layout": {
            "bounds": [10, 10, 100, 100],
            "guard_npc_index": 60101,
            "master_npc_index": 60102,
            "camp_npc_index": 60103,
            "manager_npc_index": 60104,
            "attack_camp": [7, 12, 14, 17],
            "defense_camp": [7, 97, 98, 17],
            "east_gate": [7, 99, 54, 17],
            "west_gate": [7, 10, 54, 17],
            "exit": [0, 36, 48, 17],
            "flags": [
              {"position": [7, 50, 42, 17], "monster_index": 60105},
              {"position": [7, 20, 35, 17], "monster_index": 60106},
              {"position": [7, 30, 58, 17], "monster_index": 60106},
              {"position": [7, 49, 77, 17], "monster_index": 60106}
            ]
          }
        })");
    }

    void test_schedule(const CastleWarRules& rules) {
        // 2026-11-15 01:00:00 at UTC+09:00.
        constexpr std::int64_t battle = 1794672000;
        constexpr std::int64_t preparation = battle - 24 * 3600;
        constexpr std::int64_t declaration = battle - 2 * 24 * 3600;
        constexpr std::int64_t admission = battle - 600;
        constexpr std::int64_t end = battle + 3600;
        constexpr std::int64_t week = 7 * 24 * 3600;
        const auto round = rules.round(declaration - 1);
        expect(round.declaration == declaration && round.preparation == preparation &&
                   round.admission == admission && round.battle == battle && round.end == end,
               "the default future round matches the international castle guide");
        expect(rules.phase(declaration - 1) == CastleWarPhase::Idle &&
                   rules.phase(declaration) == CastleWarPhase::Declaration &&
                   rules.phase(preparation - 1) == CastleWarPhase::Declaration &&
                   rules.phase(preparation) == CastleWarPhase::Preparation &&
                   rules.phase(admission - 1) == CastleWarPhase::Preparation &&
                   rules.phase(admission) == CastleWarPhase::Admission &&
                   rules.phase(battle - 1) == CastleWarPhase::Admission &&
                   rules.phase(battle) == CastleWarPhase::Battle &&
                   rules.phase(end - 1) == CastleWarPhase::Battle &&
                   rules.phase(end) == CastleWarPhase::Idle &&
                   rules.round(end).battle == battle + week,
               "all phases transition on their inclusive start and exclusive end");
        expect(rules.round(battle + 1).battle == battle && rules.round(battle - 1).battle == battle,
               "the same round spans the local end of the week");

        auto crossing = rules;
        crossing.battle_weekday = 6;
        crossing.battle_hour = 23;
        crossing.battle_seconds = 2 * 3600;
        expect(crossing.round(battle - 1).battle == battle - 7200 &&
                   crossing.phase(battle - 1) == CastleWarPhase::Battle &&
                   crossing.phase(battle) == CastleWarPhase::Idle,
               "a Saturday battle can end in the following local week");

        auto shifted = rules;
        shifted.utc_offset_minutes = -300;
        shifted.battle_weekday = 3;
        shifted.battle_hour = 17;
        shifted.battle_minute = 45;
        shifted.battle_second = 13;
        shifted.declaration_seconds = 1800;
        shifted.preparation_seconds = 1500;
        shifted.admission_seconds = 300;
        shifted.battle_seconds = 1200;
        // 2026-11-18 17:45:13 at UTC-05:00.
        constexpr std::int64_t shifted_battle = 1795041913;
        const auto custom_round = shifted.round(shifted_battle - 1);
        expect(custom_round.declaration == shifted_battle - 3300 &&
                   custom_round.preparation == shifted_battle - 1500 &&
                   custom_round.admission == shifted_battle - 300 &&
                   custom_round.battle == shifted_battle &&
                   custom_round.end == shifted_battle + 1200,
               "a fixed negative offset and custom weekday/time/phase lengths are honored");
    }
}  // namespace

int main() {
    CastleWarRules rules;
    std::string error;
    expect(rules.load(OPENKAL_REPO_ROOT "/server/castle_war.example.json", error) &&
               !rules.enabled && rules.phase(1794672000) == CastleWarPhase::Idle,
           "the private example parses and keeps the event disabled");
    expect(rules.max_allied_guilds == 5 && rules.minimum_level == 70 &&
               rules.alliance_min_days == 1 && rules.alliance_max_days == 15,
           "the private example uses five allies plus one leading guild");

    const auto fixture = layout_fixture();
    expect(rules.parse(fixture, error) && rules.enabled && error.empty(),
           "a complete synthetic layout enables Castle War");
    expect(rules.contains(7, 10, 100) && !rules.contains(7, 9, 100) &&
               !rules.contains(8, 10, 100) && rules.flags[0].points == 50 &&
               rules.flags[3].points == 30,
           "bounds are inclusive and standard weights default to 50 and 30");
    test_schedule(rules);

    auto invalid = fixture;
    invalid.object().erase("layout");
    expect(!rules.parse(invalid, error) && rules.enabled && rules.flags[0].points == 50,
           "invalid reload preserves the last rules");
    invalid = fixture;
    invalid.object()["layout"].object()["flags"].array()[1].object()["position"] =
        fixture.at("layout").at("flags").array()[0].at("position");
    expect(!rules.parse(invalid, error), "duplicate flag locations are refused");
    invalid = fixture;
    invalid.object()["layout"].object()["exit"] = fixture.at("layout").at("attack_camp");
    expect(!rules.parse(invalid, error), "the exit must be outside the arena");
    invalid = fixture;
    invalid.object()["layout"].object()["guard_npc_index"] =
        fixture.at("layout").at("master_npc_index");
    expect(!rules.parse(invalid, error), "NPC roles require separate IDs");
    invalid = fixture;
    invalid.object()["schedule"] = Json::parse(R"({"admission_seconds":86401})");
    expect(!rules.parse(invalid, error), "admission cannot exceed the preparation window");
    invalid.object()["schedule"] = Json::parse(R"({"declaration_seconds":600000})");
    expect(!rules.parse(invalid, error), "weekly rounds cannot overlap");
    invalid.object()["schedule"] = Json::parse(R"({"battle_seconds":600})");
    expect(!rules.parse(invalid, error), "capture must unlock during the battle");
    invalid.object()["schedule"] = Json::parse(R"({"battle_weekday":2.5})");
    expect(!rules.parse(invalid, error), "fractional scheduling fields are rejected");
    invalid.object()["schedule"] = Json::parse(R"({"battle_hours":2})");
    expect(!rules.parse(invalid, error), "a mistyped setting is rejected");
    invalid.object().erase("schedule");
    invalid.object()["alliances"] = Json::parse(R"({"alliance_min_days":16})");
    expect(!rules.parse(invalid, error), "minimum alliance duration cannot exceed its maximum");
    invalid.object()["alliances"] = Json::parse(R"({"max_allied_guilds":2})");
    expect(rules.parse(invalid, error) && rules.max_allied_guilds == 2,
           "an operator can change the number of partner guilds");
    invalid.object()["tax"] = Json::parse(R"({"merchant_npc_indices":[12, 12]})");
    expect(!rules.parse(invalid, error), "duplicate tax merchant indices are rejected");
    invalid.object()["tax"] = Json::parse(R"({"merchant_npc_indices":[12, 54]})");
    expect(rules.parse(invalid, error) && rules.merchant_npc_indices == std::vector<int>{12, 54},
           "adjusted territory shops come from the optional whitelist");
    invalid.object()["layout"].object()["bounds"] = Json::parse("[10, 10, 524288, 100]");
    expect(!rules.parse(invalid, error), "bounds must fit the map grid");

    return openkal::test::g_failures ? 1 : 0;
}
