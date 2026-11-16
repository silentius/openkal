/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerSelectorIndex.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <string_view>

#ifdef OPENKAL_TEST_WRAP_NEW
namespace {
    bool g_track_allocations = false;
    std::size_t g_allocation_count = 0;
    std::size_t g_allocation_bytes = 0;
}  // namespace
extern "C" void* __real__Znwm(std::size_t size);
extern "C" void* __wrap__Znwm(std::size_t size) {
    if (g_track_allocations) {
        ++g_allocation_count;
        g_allocation_bytes += size;
    }
    return __real__Znwm(size);
}
#endif

namespace {
    using namespace openkal;
    using test::expect;

    struct Objective {
        int quest = 20, flag = 1, item = 2, needed = 1, count = 0, level = 1;
        bool clear = false;
    };
    struct Work {
        std::size_t definitions = 0, spawns = 0, references = 0, items = 0;
    };

    // This is the nested predicate, independent of the index's sorted memberships.
    bool legacy_drops(const server::MonsterDef& definition,
                      const server::ItemGroupDb& tables,
                      const Objective& objective,
                      Work* work = nullptr) {
        const auto group_contains = [&](int group) {
            const auto* outer = tables.find_itemgroup(group);
            if (!outer) {
                return false;
            }
            for (const auto& reference : outer->entries) {
                if (work) {
                    ++work->references;
                }
                const auto* inner = tables.find_group(reference.group_index);
                if (!inner || reference.chance <= 0) {
                    continue;
                }
                for (const auto& item : inner->entries) {
                    if (work) {
                        ++work->items;
                    }
                    if (item.kind == server::GroupEntry::Kind::Item &&
                        item.item_index == objective.item && item.chance > 0) {
                        return true;
                    }
                }
            }
            return false;
        };
        for (const auto& drop : definition.quest_drops) {
            if (drop.quest_index == objective.quest && drop.flag == objective.flag &&
                group_contains(drop.itemgroup)) {
                return true;
            }
        }
        for (const auto& drop : definition.item_groups) {
            if (drop.second > 0 && group_contains(drop.first)) {
                return true;
            }
        }
        return false;
    }

    int select(const std::vector<server::MonsterDef>& definitions,
               const std::vector<server::MonsterSpawn>& spawns,
               const server::ItemGroupDb& tables,
               const std::vector<Objective>& objectives,
               int level,
               int map,
               const std::set<int>& reachable,
               const TestPlayerSelectorIndex* index,
               Work* work = nullptr,
               std::vector<int>* portal_calls = nullptr) {
        for (const auto& objective : objectives) {
            if (objective.clear || objective.level > level || objective.item <= 0 ||
                objective.count >= objective.needed) {
                continue;
            }
            for (std::size_t ordinal = 0; ordinal < definitions.size(); ++ordinal) {
                const auto& definition = definitions[ordinal];
                if (work) {
                    ++work->definitions;
                }
                if (definition.level > level + 1 ||
                    !protocol::MonsterRaceInfo::is_normal_combat(definition.race)) {
                    continue;
                }
                const auto reachable_map = [&](int target) {
                    if (target == map) {
                        return true;
                    }
                    if (portal_calls) {
                        portal_calls->push_back(target);
                    }
                    return reachable.count(target) != 0;
                };
                bool can_reach = false;
                if (index) {
                    for (int target : index->spawn_maps(definition.index)) {
                        if (work) {
                            ++work->spawns;
                        }
                        if (reachable_map(target)) {
                            can_reach = true;
                            break;
                        }
                    }
                } else {
                    for (const auto& spawn : spawns) {
                        if (work) {
                            ++work->spawns;
                        }
                        if (spawn.def_index == definition.index && reachable_map(spawn.map_index)) {
                            can_reach = true;
                            break;
                        }
                    }
                }
                if (can_reach &&
                    (index ? index->drops_item(
                                 ordinal, objective.quest, objective.flag, objective.item)
                           : legacy_drops(definition, tables, objective, work))) {
                    return definition.index;
                }
            }
        }
        return 0;
    }

    void compare(const std::vector<server::MonsterDef>& definitions,
                 const std::vector<server::MonsterSpawn>& spawns,
                 const server::ItemGroupDb& tables,
                 const std::vector<Objective>& objectives,
                 int level,
                 int map,
                 const std::set<int>& reachable) {
        const TestPlayerSelectorIndex index(definitions, spawns, tables);
        std::vector<int> expected_calls, actual_calls;
        const int expected = select(definitions,
                                    spawns,
                                    tables,
                                    objectives,
                                    level,
                                    map,
                                    reachable,
                                    nullptr,
                                    nullptr,
                                    &expected_calls);
        const int actual = select(definitions,
                                  spawns,
                                  tables,
                                  objectives,
                                  level,
                                  map,
                                  reachable,
                                  &index,
                                  nullptr,
                                  &actual_calls);
        expect(actual == expected && actual_calls == expected_calls,
               "indexed selection preserves the exact legacy result and ordered portal calls");
    }

    void explicit_contract() {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        server::ItemGroupDb tables;
        tables.upsert_group({1,
                             "",
                             {{server::GroupEntry::Kind::Money, 1, 2},
                              {server::GroupEntry::Kind::Item, 0, 2},
                              {server::GroupEntry::Kind::Item, -1, 2},
                              {server::GroupEntry::Kind::Item, 1, 3}}});
        tables.upsert_group({2,
                             "",
                             {{server::GroupEntry::Kind::Item, 1, 2},
                              {server::GroupEntry::Kind::Item, nan, 4},
                              {server::GroupEntry::Kind::Item, 1, -7},
                              {server::GroupEntry::Kind::Item, 1, 0}}});
        tables.upsert_group({3, "", {{server::GroupEntry::Kind::Item, 1, 5}}});
        tables.upsert_itemgroup({1, "", {{1, 1}, {0, 2}, {-1, 2}, {1, 999}}});
        tables.upsert_itemgroup({2, "", {{nan, 2}}});
        tables.upsert_itemgroup({3, "", {{1, 3}}});
        std::vector<server::MonsterDef> definitions(6);
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            definitions[i].index = static_cast<int>(i) + 6;
        }
        definitions[0].index = definitions[1].index = 7;
        definitions[1].item_groups = {{1, 0}, {1, -1}, {2, 1}, {2, 1}};
        definitions[2].quest_drops = {{20, 1, 3, 0}, {20, 1, 999, -1}};
        definitions[2].item_groups = {{1, 1}};
        definitions[3].level = 99;
        definitions[4].race = 4;
        definitions[5].item_groups = {{2, 1}};
        std::vector<server::MonsterSpawn> spawns;
        for (const auto& [definition, map] :
             {std::pair{999, 9}, {7, 9}, {7, 9}, {8, 0}, {7, 0}, {9, 0}, {10, 0}}) {
            server::MonsterSpawn spawn;
            spawn.def_index = definition;
            spawn.map_index = map;
            spawn.count = 0;  // The selector historically does not filter spawn counts.
            spawns.push_back(spawn);
        }
        const TestPlayerSelectorIndex index(definitions, spawns, tables);
        expect(index.spawn_maps(7) == std::vector<int>({9, 9, 0}) && index.spawn_maps(-1).empty(),
               "spawn membership preserves authored duplicates and ignores missing ids");
        expect(!index.drops_item(0, 20, 1, 2) && index.drops_item(1, 20, 1, 2),
               "duplicate definition ids retain independent drop memberships by ordinal");
        expect(index.drops_item(2, 20, 1, 5) && !index.drops_item(2, 20, 2, 5),
               "quest roll counts are ignored while quest id and flag must match");
        expect(!index.drops_item(definitions.size(), 20, 1, 2) &&
                   !index.drops_item(std::numeric_limits<std::size_t>::max(), 20, 1, 2),
               "invalid definition ordinals safely have no membership");
        for (std::size_t ordinal = 0; ordinal < definitions.size(); ++ordinal) {
            for (int quest : {19, 20, 21}) {
                for (int flag : {0, 1, 2}) {
                    for (int item : {-7, 0, 2, 3, 4, 5, 999}) {
                        expect(index.drops_item(ordinal, quest, flag, item) ==
                                   legacy_drops(definitions[ordinal], tables, {quest, flag, item}),
                               "nested chance, missing-table and money semantics match the oracle");
                    }
                }
            }
        }
        for (int map : {0, 9, 100}) {
            for (const auto& reachable : {std::set<int>{}, std::set<int>{9}, std::set<int>{0, 9}}) {
                for (const auto& objectives :
                     {std::vector<Objective>{{20, 1, 2}},
                      std::vector<Objective>{{20, 2, 5}, {20, 1, 5}},
                      std::vector<Objective>{{20, 1, 2, 1, 1}, {20, 1, 3}},
                      std::vector<Objective>{{20, 1, 2, 1, 0, 99}, {20, 1, 5}},
                      std::vector<Objective>{{20, 1, 2, 1, 0, 1, true}}}) {
                    compare(definitions, spawns, tables, objectives, 5, map, reachable);
                }
            }
        }
        std::vector<int> trace;
        expect(
            select(definitions, spawns, tables, {{20, 1, 2}}, 5, 0, {}, &index, nullptr, &trace) ==
                    7 &&
                trace == std::vector<int>({9, 9, 9, 9}),
            "reachability still precedes drop membership for useless and duplicate definitions");
        definitions.clear();
        spawns.clear();
        tables.clear();
        expect(index.spawn_maps(7) == std::vector<int>({9, 9, 0}) &&
                   index.drops_item(1, 20, 1, 2) && index.drops_item(2, 20, 1, 5),
               "immutable membership owns its data independently of source lifetime");
        const TestPlayerSelectorIndex empty({}, {}, tables);
        expect(empty.spawn_maps(7).empty() && !empty.drops_item(0, 20, 1, 2),
               "an empty catalog has no spawn or drop candidates");
    }

    void randomized_contract() {
        std::mt19937 random(0x504c4159);
        for (int catalog = 0; catalog < 24; ++catalog) {
            server::ItemGroupDb tables;
            for (int id = 0; id < 6; ++id) {
                server::GroupDef inner;
                inner.index = id;
                for (int item = 0; item < 8; ++item) {
                    inner.entries.push_back({random() % 5 == 0 ? server::GroupEntry::Kind::Money
                                                               : server::GroupEntry::Kind::Item,
                                             static_cast<float>(int(random() % 3) - 1),
                                             int(random() % 8)});
                }
                tables.upsert_group(std::move(inner));
                server::ItemGroupDef outer;
                outer.index = id;
                for (int ref = 0; ref < 4; ++ref) {
                    outer.entries.push_back(
                        {static_cast<float>(int(random() % 3) - 1), int(random() % 8)});
                }
                tables.upsert_itemgroup(std::move(outer));
            }
            std::vector<server::MonsterDef> definitions(16);
            for (auto& definition : definitions) {
                definition.index = int(random() % 12);
                definition.level = int(random() % 9);
                definition.race = random() % 4 == 0 ? 4 : 0;
                for (int drop = 0; drop < 3; ++drop) {
                    definition.item_groups.emplace_back(int(random() % 8), int(random() % 3) - 1);
                    definition.quest_drops.push_back({std::uint16_t(20 + random() % 3),
                                                      std::uint8_t(random() % 3),
                                                      int(random() % 8),
                                                      int(random() % 3) - 1});
                }
            }
            std::vector<server::MonsterSpawn> spawns(40);
            for (auto& spawn : spawns) {
                spawn.def_index = int(random() % 14);
                spawn.map_index = int(random() % 4);
                spawn.count = int(random() % 3) - 1;
            }
            for (int state = 0; state < 12; ++state) {
                std::vector<Objective> objectives;
                for (int quest = 0; quest < 4; ++quest) {
                    objectives.push_back({20 + quest,
                                          int(random() % 3),
                                          int(random() % 9),
                                          2,
                                          int(random() % 3),
                                          int(random() % 9),
                                          random() % 4 == 0});
                }
                compare(definitions,
                        spawns,
                        tables,
                        objectives,
                        int(random() % 8),
                        int(random() % 4),
                        {int(random() % 4)});
            }
        }
    }

    double percentile(std::vector<double> samples, double fraction) {
        std::sort(samples.begin(), samples.end());
        return samples[static_cast<std::size_t>(std::ceil(fraction * samples.size())) - 1];
    }

    void benchmark() {
        using Clock = std::chrono::steady_clock;
        std::cout << "selector benchmark: 9 samples; Q objectives, D definitions, S spawns; "
                     "one shared index for A agents; no timing threshold\n";
        std::cout << "workload,Q,D,S,table_items,A,build_median_us,legacy_median_us,indexed_median_"
                     "us,legacy_p95_us,"
                     "indexed_p95_us,legacy_spawn_visits,indexed_spawn_visits,legacy_item_visits,"
                     "index_sizeof,build_requested_allocations,build_requested_bytes\n";
        struct Case {
            int q, d, s, items = 32;
            bool early_match = false;
        };
        const std::array<Case, 9> cases{{{4, 64, 64},
                                         {1, 64, 64},
                                         {16, 64, 64},
                                         {4, 16, 64},
                                         {4, 256, 64},
                                         {4, 64, 16},
                                         {4, 64, 256},
                                         {1, 1, 1, 32, true},
                                         {1, 256, 256, 512, true}}};
        std::uint64_t checksum = 0;
        for (const auto& dimensions : cases) {
            const auto [q, d, s, item_count, early_match] = dimensions;
            server::ItemGroupDb tables;
            server::GroupDef group;
            group.index = 1;
            for (int item = 1; item <= item_count; ++item) {
                group.entries.push_back({server::GroupEntry::Kind::Item, 1, item});
            }
            tables.upsert_group(std::move(group));
            tables.upsert_itemgroup({1, "", {{1, 1}}});
            std::vector<server::MonsterDef> definitions(d);
            for (int i = 0; i < d; ++i) {
                definitions[i].index = i + 1;
                definitions[i].item_groups = {{1, 1}};
            }
            std::vector<server::MonsterSpawn> spawns(s);
            for (int i = 0; i < s; ++i) {
                spawns[i].def_index = i % d + 1;
                spawns[i].map_index = 0;
            }
            std::vector<Objective> objectives(q);
            for (int i = 0; i < q; ++i) {
                objectives[i].quest = i + 20;
                objectives[i].item = early_match ? 1 : 9999;
            }
            std::vector<double> builds;
            for (int sample = 0; sample < 9; ++sample) {
                const auto begin = Clock::now();
                const TestPlayerSelectorIndex built(definitions, spawns, tables);
                const auto end = Clock::now();
                checksum += built.spawn_maps(1).size();
                builds.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
            }
            std::size_t allocation_count = 0, allocation_bytes = 0;
#ifdef OPENKAL_TEST_WRAP_NEW
            g_allocation_count = g_allocation_bytes = 0;
            g_track_allocations = true;
#endif
            const TestPlayerSelectorIndex index(definitions, spawns, tables);
#ifdef OPENKAL_TEST_WRAP_NEW
            g_track_allocations = false;
            allocation_count = g_allocation_count;
            allocation_bytes = g_allocation_bytes;
#endif
            Work old_work, new_work;
            const std::set<int> reachable;
            expect(
                select(
                    definitions, spawns, tables, objectives, 5, 0, reachable, nullptr, &old_work) ==
                        (early_match ? 1 : 0) &&
                    select(definitions,
                           spawns,
                           tables,
                           objectives,
                           5,
                           0,
                           reachable,
                           &index,
                           &new_work) == (early_match ? 1 : 0),
                "both selector drivers return the expected benchmark result");
            expect(old_work.definitions == std::size_t(early_match ? 1 : q * d) &&
                       new_work.definitions == old_work.definitions &&
                       new_work.spawns == std::size_t(early_match ? 1 : q * std::min(d, s)) &&
                       (early_match ? old_work.spawns == 1 && old_work.items == 1
                                    : old_work.spawns > new_work.spawns &&
                                          old_work.items == new_work.spawns * item_count),
                   "loop counts distinguish early matches from repeated no-match scans");
            for (int agents : {1, 16, 64}) {
                std::vector<double> old_samples, new_samples;
                const auto timed = [&](const TestPlayerSelectorIndex* selected) {
                    const auto begin = Clock::now();
                    for (int agent = 0; agent < agents; ++agent) {
                        checksum += select(
                            definitions, spawns, tables, objectives, 5, 0, reachable, selected);
                    }
                    return std::chrono::duration<double, std::micro>(Clock::now() - begin).count();
                };
                for (int sample = 0; sample < 9; ++sample) {
                    if (sample % 2 == 0) {
                        old_samples.push_back(timed(nullptr));
                        new_samples.push_back(timed(&index));
                    } else {
                        new_samples.push_back(timed(&index));
                        old_samples.push_back(timed(nullptr));
                    }
                }
                std::cout << (early_match ? "early_match" : "no_match") << ',' << q << ',' << d
                          << ',' << s << ',' << item_count << ',' << agents << ',' << std::fixed
                          << std::setprecision(3) << percentile(builds, .5) << ','
                          << percentile(old_samples, .5) << ',' << percentile(new_samples, .5)
                          << ',' << percentile(old_samples, .95) << ','
                          << percentile(new_samples, .95) << ',' << old_work.spawns * agents << ','
                          << new_work.spawns * agents << ',' << old_work.items * agents << ','
                          << sizeof(index) << ',' << allocation_count << ',' << allocation_bytes
                          << '\n';
            }
        }
        std::cout
            << "checksum=" << checksum
            << "; requested bytes include temporary construction storage, not retained heap\n";
    }
}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark();
    } else {
        explicit_contract();
        randomized_contract();
    }
    return test::g_failures == 0 ? 0 : 1;
}
