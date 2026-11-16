/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/platform/ConfigArchive.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/NpcRegistry.h"
#include "openkal/server/world/QuestDb.h"
#include "openkal/server/world/QuestEngine.h"
#include "test_util.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef OPENKAL_TEST_CONFIG_ARCHIVE
#error OPENKAL_TEST_CONFIG_ARCHIVE required
#endif

int main() {
    using openkal::protocol::Quest;
    using openkal::server::NpcRegistry;
    using openkal::server::PlayerQuestMap;
    using openkal::server::QuestActionSink;
    using openkal::server::QuestDb;
    using openkal::server::QuestEngine;
    using openkal::test::expect;

    const std::string root = openkal::test::temp_dir("quest_packaged_resources");
    std::string archive_error;
    if (!openkal::platform::ConfigArchive::mount(
            root, OPENKAL_TEST_CONFIG_ARCHIVE, &archive_error)) {
        expect(false, archive_error.c_str());
        return 1;
    }
    const std::string quests_dir = root + "/config/quests";
    const std::string tasks_dir = root + "/config/tasks";
    const std::string npcs_dir = root + "/config/npcs";

    QuestDb db;
    expect(db.load_dir(quests_dir), "load quests dir");
    expect(db.load_task_levels_dir(tasks_dir), "load task level requirements");
    expect(db.find(1, 1) != nullptr, "quest 1 flag 1 present");
    expect(db.find(1, 1)->linked, "quest 1 flag 1 linked");
    expect(db.find(1, 2) != nullptr && !db.find(1, 2)->linked, "quest 1 flag 2 direct");
    expect(db.find(1, 3) != nullptr && !db.find(1, 3)->linked, "quest 1 flag 3 direct");
    expect(db.stage_required_level(9004, 1) == 30,
           "journal task level is available to the quest engine");
    expect(db.required_level(9004) == 30, "journal task level is available to quest markers");
    const auto* bound_weapon_reward = db.find(54, 6);
    expect(bound_weapon_reward != nullptr && !bound_weapon_reward->cases.empty() &&
               !bound_weapon_reward->cases.front().actions.empty() &&
               !bound_weapon_reward->cases.front().actions.front().item_in.empty() &&
               bound_weapon_reward->cases.front().actions.front().item_in.front().size() == 4 &&
               bound_weapon_reward->cases.front().actions.front().item_in.front()[3] == 1,
           "quest weapon reward preserves its account-bound marker");

    NpcRegistry npcs;
    expect(npcs.load_dir(npcs_dir), "load npcs");
    std::uint32_t id_gen = 1000;
    npcs.spawn_for_map(id_gen, 0);
    expect(!npcs.npcs().empty(), "map 0 NPCs spawned");

    const openkal::server::NpcEntity* gwang = nullptr;
    const openkal::server::NpcEntity* nurook = nullptr;
    const openkal::server::NpcEntity* exp_event_manager = nullptr;
    for (const auto& n : npcs.npcs()) {
        if (n.index == 72) {
            gwang = &n;
        }
        if (n.index == 77) {
            nurook = &n;
        }
        if (n.index == 5037) {
            exp_event_manager = &n;
        }
    }
    expect(gwang != nullptr, "Gwang-Chun spawned");
    expect(nurook != nullptr, "Nu-Rook spawned");
    expect(gwang->quest_key == Quest::make_key(0, 1), "Gwang-Chun hub key 0/1");
    expect(nurook->quest_key == Quest::make_key(1, 3), "Nu-Rook key 1/3");
    expect(exp_event_manager != nullptr, "Exp Event Manager spawned");
    expect(exp_event_manager->kind == 3 && exp_event_manager->quest_key == Quest::make_key(9500, 1),
           "Exp Event Manager opens its dialog through quest 9500/1");

    npcs.append_for_map(id_gen, 20);
    const openkal::server::NpcEntity* explorer = nullptr;
    for (const auto& n : npcs.npcs()) {
        if (n.index == 5036) {
            explorer = &n;
            break;
        }
    }
    expect(explorer != nullptr && explorer->map == 20, "Emok map explorer on map 20");

    npcs.append_for_map(id_gen, 3);
    const openkal::server::NpcEntity* fifth_monument = nullptr;
    const openkal::server::NpcEntity* throne = nullptr;
    const openkal::server::NpcEntity* teleport_monument = nullptr;
    for (const auto& n : npcs.npcs()) {
        if (n.index == 374) {
            fifth_monument = &n;
        }
        if (n.index == 383) {
            throne = &n;
        }
        if (n.index == 396) {
            teleport_monument = &n;
        }
    }
    expect(fifth_monument != nullptr && fifth_monument->has_scenario &&
               fifth_monument->scenario_slot == 0 && fifth_monument->scenario_required_item == 645,
           "Doggebi monument scenario is loaded from NPC JSON");
    expect(throne != nullptr && throne->has_scenario && throne->scenario_slot == 8 &&
               throne->scenario_required_item == 0,
           "Doggebi throne scenario is retained without a consumed item");
    expect(teleport_monument != nullptr && teleport_monument->teleports.size() == 1 &&
               teleport_monument->teleports.front().map == 3 &&
               teleport_monument->teleports.front().x == 57366 &&
               teleport_monument->teleports.front().y == 10034,
           "Doggebi teleport monument target is loaded from NPC JSON");

    QuestEngine engine(db);
    PlayerQuestMap quests;
    std::unordered_map<int, int> bag;
    std::vector<std::int32_t> htmls;
    std::vector<openkal::protocol::QuestFlagEntry> saves;

    QuestActionSink sink;
    sink.clazz = 0;
    sink.level = 1;
    sink.send_html = [&](std::int32_t id) { htmls.push_back(id); };
    sink.send_save_quest = [&](const openkal::protocol::QuestFlagEntry& e) { saves.push_back(e); };
    sink.count_item = [&](int index) {
        const auto it = bag.find(index);
        return it == bag.end() ? 0 : it->second;
    };
    bool bag_full = false;
    int skill_points = 0;
    int contribution = 0;
    int experience = 0;
    sink.add_exp = [&](int points) { experience += points; };
    sink.add_skill_points = [&](int points) { skill_points += points; };
    sink.add_contribution = [&](int points) { contribution += points; };
    sink.add_specialty = [&](int bit_index) { sink.specialty |= 1 << bit_index; };
    sink.exchange_items = [&](const std::vector<openkal::server::QuestAction>& actions) {
        if (bag_full) {
            return false;
        }
        auto staged = bag;
        for (const auto& action : actions) {
            if (action.op != "item") {
                continue;
            }
            for (const auto& row : action.item_out) {
                if (staged[row[0]] < row[1]) {
                    return false;
                }
                staged[row[0]] -= row[1];
                if (staged[row[0]] == 0) {
                    staged.erase(row[0]);
                }
            }
            for (const auto& row : action.item_in) {
                staged[row[0]] += row[2];
            }
        }
        bag = std::move(staged);
        return true;
    };

    int changed_gstate_bit = -1;
    bool changed_gstate_enabled = false;
    sink.set_gstate = [&](int bit_index, bool enabled) {
        changed_gstate_bit = bit_index;
        changed_gstate_enabled = enabled;
    };

    PlayerQuestMap level_gated_quests;
    level_gated_quests[9004] = {9004, 1, 0};
    sink.level = 29;
    expect(!engine.call_process(Quest::make_key(9004, 1), level_gated_quests, sink),
           "task stage cannot advance below its journal level requirement");
    expect(level_gated_quests[9004].flag == 1,
           "failed task level gate leaves quest progress unchanged");
    sink.level = 30;
    expect(engine.call_process(Quest::make_key(9004, 1), level_gated_quests, sink),
           "task stage advances at its journal level requirement");
    expect(level_gated_quests[9004].flag == 2, "task level gate advances the expected stage");
    sink.level = 1;

    PlayerQuestMap guard_quests;
    guard_quests[54] = {54, 1, 0};
    guard_quests[55] = {55, 1, 0};
    guard_quests[56] = {56, 1, 0};
    guard_quests[57] = {57, 1, 0};
    bag[396] = 1;
    bag[397] = 1;
    bag[398] = 1;
    bag[237] = 1;
    sink.clazz = 3;
    sink.level = 5;
    expect(engine.call_process(Quest::make_key(54, 1), guard_quests, sink),
           "thief can turn in the Demon Vulgar guard quest");
    expect(engine.call_process(Quest::make_key(55, 1), guard_quests, sink),
           "thief can turn in the Demon Scout guard quest");
    expect(engine.call_process(Quest::make_key(56, 1), guard_quests, sink),
           "thief can turn in the Demon Spear guard quest");
    expect(engine.call_process(Quest::make_key(57, 1), guard_quests, sink),
           "thief can turn in the Fish Trap guard quest");
    expect(guard_quests[54].clear == 1 && guard_quests[55].clear == 1 &&
               guard_quests[56].clear == 1 && guard_quests[57].clear == 1,
           "all completed thief guard quests are cleared");
    expect(bag.find(396) == bag.end() && bag.find(397) == bag.end() && bag.find(398) == bag.end() &&
               bag.find(237) == bag.end(),
           "all thief guard quest turn-in items are consumed");

    PlayerQuestMap class_reward_quests;
    class_reward_quests[9006] = {9006, 3, 0};
    bag[232] = 1;
    bag[233] = 1;
    sink.level = 30;
    expect(engine.call_process(Quest::make_key(9006, 1), class_reward_quests, sink),
           "thief can receive the Ban-go quest class reward");
    expect(class_reward_quests[9006].flag == 4 && bag[1595] == 1,
           "thief receives the apprentice thief trinket");
    expect(bag.find(232) == bag.end() && bag.find(233) == bag.end(),
           "Ban-go quest turn-in items are consumed");

    PlayerQuestMap thief_first_job_quests;
    thief_first_job_quests[9004] = {9004, 2, 0};
    bag[181] = 1;
    htmls.clear();
    sink.level = 30;
    sink.specialty = 1;
    expect(engine.call_process(Quest::make_key(9004, 1), thief_first_job_quests, sink),
           "thief can complete the first-job quest");
    expect(thief_first_job_quests[9004].clear == 1 && thief_first_job_quests[9005].flag == 1,
           "thief first-job quest opens the next quest");
    expect(!htmls.empty() && htmls.back() == 700080,
           "thief receives the first-job completion dialog");

    PlayerQuestMap thief_second_job_quests;
    thief_second_job_quests[9009] = {9009, 1, 0};
    htmls.clear();
    sink.level = 50;
    sink.specialty = 1 << 1;
    expect(engine.call_process(Quest::make_key(9006, 1), thief_second_job_quests, sink),
           "thief reaches the second-job dialog");
    expect(!htmls.empty() && htmls.back() == 500060,
           "thief receives the second-job selection dialog");
    sink.clazz = 0;
    sink.level = 1;
    sink.specialty = 1;

    htmls.clear();
    expect(engine.call_process(exp_event_manager->quest_key, quests, sink),
           "Exp Event Manager dialog quest runs");
    expect(htmls.size() == 1 && htmls[0] == 690, "Exp Event Manager dialog quest sends html 690");
    const auto* donate_step = db.find(9501, 1);
    const auto* hundred_step = db.find(9502, 1);
    expect(donate_step != nullptr && donate_step->cases.size() == 1 &&
               donate_step->cases.front().actions.empty(),
           "EXP coin donation quest is an event-handler marker without legacy raw actions");
    expect(hundred_step != nullptr && hundred_step->cases.size() == 1 &&
               hundred_step->cases.front().actions.empty(),
           "100-coin donation quest is an event-handler marker without legacy raw actions");

    // 1) Talk to Gwang-Chun hub → link into quest 1/1 → offer dialog 900002.
    htmls.clear();
    expect(engine.call_process(gwang->quest_key, quests, sink), "hub CallProcess");
    expect(htmls.size() == 1 && htmls[0] == 900002, "offer html 900002");
    expect(quests.empty(), "no flag yet");

    // Linked step must not run via CallProcess.
    expect(!engine.call_process(Quest::make_key(1, 1), quests, sink), "linked gated");

    // 2) Accept via dialog link quest 1/2 → item 132 + flag 1 + 900003.
    htmls.clear();
    saves.clear();
    expect(engine.call_process(Quest::make_key(1, 2), quests, sink), "accept CallProcess");
    expect(bag[132] == 1, "got letter item 132");
    expect(quests[1].flag == 1 && quests[1].clear == 0, "flag 1 saved");
    expect(!saves.empty() && saves.back().flag == 1, "SaveQuest sent");
    expect(htmls.size() == 1 && htmls[0] == 900003, "accept html 900003");

    expect(!engine.call_process(Quest::make_key(1, 2), quests, sink) && bag[132] == 1,
           "repeated acceptance cannot duplicate the delivery letter");

    // 3) Nu-Rook exchange 132 → 142, flag 2.
    htmls.clear();
    expect(engine.call_process(nurook->quest_key, quests, sink), "Nu-Rook CallProcess");
    expect(bag.find(132) == bag.end(), "letter removed");
    expect(bag[142] == 1, "got item 142");
    expect(quests[1].flag == 2, "flag 2");
    expect(htmls.size() == 1 && htmls[0] == 900006, "Nu-Rook html 900006");

    // 4) Return to Gwang-Chun → turn-in rewards + clear.
    htmls.clear();
    expect(engine.call_process(gwang->quest_key, quests, sink), "turn-in CallProcess");
    expect(bag.find(142) == bag.end(), "142 removed");
    expect(bag[31] == 100, "money reward");
    expect(bag[47] == 2, "potion reward");
    expect(quests[1].clear == 1, "quest cleared");
    expect(htmls.size() == 1 && htmls[0] == 900005, "complete html 900005");

    // Cleared hub no longer offers quest 1.
    htmls.clear();
    expect(engine.call_process(gwang->quest_key, quests, sink), "post-clear hub");
    expect(!htmls.empty() && htmls[0] != 900002, "no re-offer after clear");

    // Fish Trap quest consumes item 238 and enables compatibility behavior (gstate bit 5).
    bag.clear();
    bag[238] = 1;
    sink.gstate = 0;
    changed_gstate_bit = -1;
    changed_gstate_enabled = false;
    expect(engine.process(Quest::make_key(6001, 3), quests, sink), "start fishing quest");
    expect(bag.find(238) == bag.end(), "fish trap consumed");
    expect(changed_gstate_bit == 5 && changed_gstate_enabled,
           "fish trap invokes gstate start callback");
    expect((sink.gstate & (1u << 5u)) != 0, "fish gstate is reflected in the action sink");

    const openkal::server::NpcEntity* chun = nullptr;
    const openkal::server::NpcEntity* jin = nullptr;
    for (const auto& n : npcs.npcs()) {
        if (n.index == 93) {
            chun = &n;
        }
        if (n.index == 97) {
            jin = &n;
        }
    }
    expect(chun != nullptr, "Chun-Sooin spawned");
    expect(jin != nullptr, "Jin-Pyung spawned");
    expect(chun->quest_key == Quest::make_key(9001, 1), "Chun-Sooin key 9001/1");
    expect(jin->quest_key == Quest::make_key(9004, 1), "Jin-Pyung key 9004/1");

    // Without NewPlayer flag 9001/1 the talk falls through to the stirrup reminder.
    PlayerQuestMap event_quests;
    htmls.clear();
    expect(engine.call_process(chun->quest_key, event_quests, sink), "Chun-Sooin without 9001");
    expect(htmls.size() == 1 && htmls[0] == 500001, "stirrup html without starter flag");

    openkal::protocol::QuestFlagEntry start_9001;
    start_9001.quest = 9001;
    start_9001.flag = 1;
    start_9001.clear = 0;
    event_quests[9001] = start_9001;
    htmls.clear();
    saves.clear();
    expect(engine.call_process(chun->quest_key, event_quests, sink), "Chun-Sooin with 9001/1");
    expect(event_quests[9001].flag == 2, "9001 advanced to flag 2");
    expect(htmls.size() == 1 && htmls[0] == 700001, "job-start html 700001");

    // Jin-Pyung first-job grant: wandering bit 0 + (specialty 1) → bitmask 3.
    PlayerQuestMap job_quests;
    openkal::protocol::QuestFlagEntry start_9004;
    start_9004.quest = 9004;
    start_9004.flag = 2;
    start_9004.clear = 0;
    job_quests[9004] = start_9004;
    int specialty = 1;
    std::int64_t gained_exp = 0;
    sink.clazz = 0;
    sink.level = 30;
    sink.specialty = specialty;
    sink.add_specialty = [&](int bit_index) { specialty |= (1 << bit_index); };
    sink.add_exp = [&](std::int64_t exp) { gained_exp += exp; };
    htmls.clear();
    expect(engine.process(Quest::make_key(9004, 3), job_quests, sink), "9004/3 first job");
    expect(specialty == 3, "first job sets bits 0+1");
    expect(gained_exp == 10000, "first-job exp");
    expect(job_quests[9004].clear == 1, "9004 cleared");
    expect(job_quests[9005].flag == 1 && job_quests[9005].clear == 0, "9005 opened");
    expect(htmls.size() == 1 && htmls[0] == 700019, "knight first-job html");

    const openkal::server::NpcEntity* chang = nullptr;
    for (const auto& n : npcs.npcs()) {
        if (n.index == 186) {
            chang = &n;
        }
    }
    expect(chang != nullptr, "Chang spawned");
    expect(chang->quest_key == Quest::make_key(37, 1), "Chang key 37/1");
    expect(db.find(37, 1) != nullptr && !db.find(37, 1)->linked, "quest 37/1 talk");
    expect(db.find(37, 2) != nullptr && db.find(37, 2)->linked, "quest 37/2 linked");

    PlayerQuestMap animal_quests;
    bag.clear();
    htmls.clear();
    saves.clear();
    sink.clazz = 0;
    sink.level = 14;
    sink.specialty = 0;
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "Chang under level");
    expect(htmls.size() == 1 && htmls[0] == 900405, "need level 15 html");

    htmls.clear();
    sink.level = 15;
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "Chang intro");
    expect(htmls.size() == 1 && htmls[0] == 900389, "intro html 900389");

    htmls.clear();
    saves.clear();
    expect(engine.call_process(Quest::make_key(37, 5), animal_quests, sink), "accept talisman");
    expect(animal_quests[37].flag == 1 && animal_quests[37].clear == 0, "flag 1");
    expect(htmls.empty(), "accept has no html");

    htmls.clear();
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "Chang waiting keg");
    expect(htmls.size() == 1 && htmls[0] == 900396, "go get keg html");

    bag[336] = 1;
    htmls.clear();
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "turn in keg");
    expect(bag.find(336) == bag.end(), "keg removed");
    expect(animal_quests[37].flag == 2, "flag 2");
    expect(htmls.size() == 1 && htmls[0] == 900397, "reward soon html");

    htmls.clear();
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "pick animal");
    expect(htmls.size() == 1 && htmls[0] == 900398, "picker html 900398");

    htmls.clear();
    expect(engine.call_process(Quest::make_key(37, 7), animal_quests, sink), "choose tiger");
    expect(bag[337] == 1, "got tiger egg");
    expect(animal_quests[37].flag == 3 && animal_quests[37].clear == 1, "cleared as tiger");

    htmls.clear();
    expect(engine.call_process(chang->quest_key, animal_quests, sink), "Chang after tiger");
    expect(htmls.size() == 1 && htmls[0] == 900380, "tiger care html");

    // Restored normal quest: failed turn-ins must not consume items or award points.
    PlayerQuestMap normal_quests;
    bag.clear();
    sink.level = 12;
    const auto before_exp = gained_exp;
    expect(engine.call_process(Quest::make_key(8, 3), normal_quests, sink),
           "restored quest 8 can be accepted");
    bag[152] = 24;
    expect(engine.process(Quest::make_key(8, 1), normal_quests, sink), "incomplete item reminder");
    expect(bag[152] == 24 && normal_quests[8].flag == 1 && gained_exp == before_exp,
           "missing objective gives no reward and retains progress");
    bag[152] = 25;
    bag_full = true;
    expect(!engine.process(Quest::make_key(8, 1), normal_quests, sink),
           "failed exchange aborts quest");
    expect(bag[152] == 25 && normal_quests[8].clear == 0 && gained_exp == before_exp &&
               contribution == 0,
           "failed exchange has no side effects");
    bag_full = false;
    expect(engine.process(Quest::make_key(8, 1), normal_quests, sink),
           "restored quest 8 completes");
    expect(bag.count(152) == 0 && normal_quests[8].clear == 1 && gained_exp == before_exp + 2600 &&
               contribution == 1,
           "quest 8 consumes exact objectives and awards EXP and contribution");
    expect(!engine.call_process(Quest::make_key(8, 3), normal_quests, sink),
           "completed quest cannot be accepted again");
    expect(!engine.call_process(Quest::make_key(37, 8), animal_quests, sink),
           "old animal selection cannot grant a second egg");

    // Second-job choice, recommendation and third-job chain share the same progression.
    PlayerQuestMap advanced_quests;
    advanced_quests[9009] = {9009, 1, 0};
    sink.level = 70;
    sink.specialty = 3;
    specialty = 3;
    bag.clear();
    expect(engine.call_process(Quest::make_key(9006, 15), advanced_quests, sink),
           "second job can be selected once");
    expect(advanced_quests[9009].clear == 1, "second job is complete");
    const auto job_mask = specialty;
    (void) engine.call_process(Quest::make_key(9006, 16), advanced_quests, sink);
    expect(specialty == job_mask, "cannot select the other second-job branch");
    expect(engine.call_process(Quest::make_key(9006, 22), advanced_quests, sink),
           "third-job recommendation accepts completed second job");
    expect(bag[1471] == 1 && advanced_quests[9010].flag == 1,
           "recommendation and third-job journal are granted together");
    expect(engine.call_process(Quest::make_key(9006, 22), advanced_quests, sink) && bag[1471] == 1,
           "recommendation cannot be duplicated");

    // Exercise every website normal quest using real data and public entry points.
    // Objective supplies are injected: monster drop reachability is audited separately.
    const std::vector<int> normal_ids = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                         16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31,
                                         32, 33, 34, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                                         48, 49, 50, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64, 65};
    for (const int id : normal_ids) {
        PlayerQuestMap progress;
        for (int other = 1; other <= 65; ++other) {
            if (other != id) {
                progress[other] = {static_cast<std::uint16_t>(other), 255, 1};
            }
        }
        bag.clear();
        sink.level = 100;
        sink.clazz = 0;
        sink.specialty = 1;
        for (int pass = 0; pass < 20 && !progress[id].clear; ++pass) {
            for (int flag = 0; flag <= 255; ++flag) {
                if (const auto* step = db.find(id, flag)) {
                    for (const auto& qc : step->cases) {
                        for (const auto& condition : qc.conditions) {
                            for (const auto& item : condition.items) {
                                bag[item.first] = std::max(bag[item.first], item.second);
                            }
                        }
                        for (const auto& action : qc.actions) {
                            for (const auto& row : action.item_out) {
                                bag[row[0]] = std::max(bag[row[0]], row[1]);
                            }
                        }
                    }
                }
            }
            for (const int entry : {id, 0}) {
                for (int flag = 0; flag <= 255 && !progress[id].clear; ++flag) {
                    if (const auto* step = db.find(entry, flag); step && !step->linked) {
                        (void) engine.call_process(Quest::make_key(entry, flag), progress, sink);
                    }
                }
            }
        }
        expect(progress[id].clear != 0,
               ("normal quest " + std::to_string(id) + " reaches completion").c_str());
    }

    // Follow the complete event chain for each class documented by the website.
    for (int clazz = 0; clazz < 3; ++clazz) {
        PlayerQuestMap progress;
        progress[9001] = {9001, 1, 0};
        bag.clear();
        sink.clazz = clazz;
        sink.specialty = 1;
        sink.level = 100;
        const std::vector<int> entries = {9001,
                                          9002,
                                          9003,
                                          9004,
                                          9005,
                                          9006,
                                          9007,
                                          9008,
                                          9009,
                                          9010,
                                          10000,
                                          10001,
                                          10002,
                                          10003,
                                          10004,
                                          10005};
        std::unordered_set<int> scripted_items;
        for (const int id : entries) {
            for (int flag = 1; flag <= 30; ++flag) {
                if (const auto* step = db.find(id, flag)) {
                    for (const auto& qc : step->cases) {
                        for (const auto& action : qc.actions) {
                            for (const auto& row : action.item_in) {
                                scripted_items.insert(row[0]);
                            }
                        }
                    }
                }
            }
        }
        for (int pass = 0; pass < 30 && !progress[10005].clear; ++pass) {
            for (const int id : entries) {
                for (int flag = 1; flag <= 30; ++flag) {
                    const auto* step = db.find(id, flag);
                    if (!step) {
                        continue;
                    }
                    for (const auto& qc : step->cases) {
                        for (const auto& condition : qc.conditions) {
                            for (const auto& item : condition.items) {
                                if (scripted_items.count(item.first) == 0) {
                                    bag[item.first] = std::max(bag[item.first], item.second);
                                }
                            }
                        }
                    }
                }
            }
            for (const int id : entries) {
                for (int flag = 1; flag <= 30; ++flag) {
                    if (const auto* step = db.find(id, flag); step && !step->linked) {
                        (void) engine.call_process(Quest::make_key(id, flag), progress, sink);
                    }
                }
            }
        }
        for (const int id :
             {9001, 9002, 9003, 9004, 9005, 9007, 9008, 9009, 9010, 10001, 10002, 10003, 10005}) {
            expect(progress[id].clear != 0,
                   ("event quest " + std::to_string(id) + " completes for class " +
                    std::to_string(clazz))
                       .c_str());
        }
        expect((sink.specialty & (1 << 4)) != 0 && bag[1620] == 1,
               "event chain awards third job and Moving Trinket");
    }

    // Load actual monster data to catch flattened single-row quest drop definitions.
    openkal::server::GameWorld loot_world;
    expect(loot_world.load_monsters_json_dir(root + "/config/monsters", 0, 0, 0),
           "load quest monsters");
    expect(loot_world.load_drop_tables(root + "/config/groups", root + "/config/itemgroups"),
           "load quest drop tables");
    const auto* tomb_keeper = loot_world.find_def(82);
    expect(tomb_keeper && tomb_keeper->quest_drops.size() == 1 &&
               tomb_keeper->quest_drops.front().quest_index == 62,
           "single flat monster quest row remains usable");
    const auto water_heads = loot_world.roll_monster_quest_drops(21, 0, 0, 18, 1);
    expect(water_heads.size() == 1 && water_heads.front().item_index == 162,
           "water dragons drop the restored quest head");
    const auto wrong_stage = loot_world.roll_monster_quest_drops(21, 0, 0, 18, 0);
    expect(wrong_stage.empty(), "quest heads require the active collection stage");
    const auto mad_heads = loot_world.roll_monster_quest_drops(19, 0, 0, 23, 1);
    expect(mad_heads.size() == 1 && mad_heads.front().item_index == 163,
           "mad knights drop the restored quest head");
    const auto necklace = loot_world.roll_monster_quest_drops(126, 0, 0, 9010, 10);
    expect(necklace.size() == 1 && necklace.front().item_index == 1493,
           "third-job boss drops the required necklace");
    PlayerQuestMap relic_quests;
    bag.clear();
    expect(engine.call_process(Quest::make_key(26, 1), relic_quests, sink),
           "Goo-Ryung starts the relic quest");
    const std::vector<int> relic_monsters{2, 6, 8, 25, 10};
    for (int stage = 1; stage <= 5; ++stage) {
        const auto drops =
            loot_world.roll_monster_quest_drops(relic_monsters[stage - 1], 0, 0, 26, stage);
        expect(drops.size() == 1 && drops.front().item_index == 181 + stage,
               "active relic stage has its matching monster drop");
        for (const auto& drop : drops) {
            bag[drop.item_index] += drop.count;
        }
        expect(engine.call_process(Quest::make_key(26, 1), relic_quests, sink) &&
                   relic_quests[26].flag == stage + 1,
               "Goo-Ryung accepts the relic and advances the journal");
    }
    expect(relic_quests[26].clear == 1 && bag[47] == 100 && bag[48] == 20,
           "all relic stages complete with the potion rewards");
    expect(db.find(static_cast<std::uint16_t>(200699), 1) == nullptr,
           "oversized legacy quest IDs never wrap into unrelated protocol IDs");

    // Contribution must survive both insert and update through the real SQLite store.
    openkal::server::SqliteStore store;
    expect(store.open(":memory:"), "open quest persistence database");
    openkal::server::AccountRecord account;
    account.id = 1;
    account.name = "quest-test";
    expect(store.upsert_account(account), "create quest test account");
    openkal::server::CharacterRecord character;
    character.id = 1;
    character.account_id = 1;
    character.name = "QuestTester";
    character.contribution = 7;
    character.su_point = 3;
    expect(store.save_character(character), "save quest reward counters");
    auto loaded = store.load_character(1);
    expect(loaded && loaded->contribution == 7 && loaded->su_point == 3,
           "load persisted contribution and skill points");
    character.contribution = 9;
    expect(store.save_character(character), "update contribution");
    loaded = store.load_character(1);
    expect(loaded && loaded->contribution == 9, "contribution update survives reload");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
