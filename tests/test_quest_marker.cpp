/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DialogCatalog.h"
#include "openkal/client/MonsterCatalog.h"
#include "openkal/client/QuestMarker.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/world/QuestDb.h"
#include "test_util.h"

#include <string>
#include <vector>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::client::QuestMarker;
    using openkal::client::QuestMarkerKind;
    using openkal::protocol::Quest;
    using openkal::protocol::QuestFlagEntry;
    using openkal::server::QuestDb;
    using openkal::test::expect;

    const std::string root = OPENKAL_TEST_RESOURCE_ROOT;
    QuestDb db;
    expect(db.load_dir(root + "/config/quests"), "load quests");
    expect(db.load_task_levels_dir(root + "/config/tasks"), "load task level requirements");
    expect(db.required_level(1) == 1, "quest 1 reads its journal level requirement");
    expect(db.required_level(6) == 10, "quest 6 starts at level 10");
    expect(db.required_level(9004) == 30,
           "quest marker reads the same task level requirement as the server");

    openkal::client::DialogCatalog dialog_catalog;
    expect(dialog_catalog.load_dir(root + "/config/dialogs"), "load dialogs");
    openkal::client::TaskCatalog tasks;
    expect(tasks.load_dir(root + "/config/tasks"), "load tasks");
    const auto* won_jung_task = tasks.find(7);
    expect(won_jung_task && !won_jung_task->flags.empty() &&
               won_jung_task->flags.front().level == 11,
           "task catalog retains stage level requirements");
    const auto* teleport_dialog = dialog_catalog.find(259);
    expect(teleport_dialog != nullptr && !teleport_dialog->links.empty() &&
               teleport_dialog->links.front().type == "npc_teleport",
           "Doggebi teleport confirmation dialog has an NPC teleport action");
    const auto* goo_deuk_dialog = dialog_catalog.find(1);
    expect(goo_deuk_dialog != nullptr && goo_deuk_dialog->links.size() == 3 &&
               goo_deuk_dialog->links[0].type == "shop" && goo_deuk_dialog->links[1].type == "2" &&
               goo_deuk_dialog->links[2].type == "close",
           "Goo-Deuk opens the weapon shop and can continue to his lore dialog");
    const auto* shaman_pet_dialog = dialog_catalog.find(800008);
    expect(shaman_pet_dialog != nullptr && !shaman_pet_dialog->links.empty() &&
               shaman_pet_dialog->links.front().type == "shop",
           "Shaman Pet dialog opens the pet shop");
    expect(dialog_catalog.find(2) != nullptr, "Goo-Deuk lore dialog is available");

    const std::uint32_t gwang = Quest::make_key(0, 1);
    const std::uint32_t nurook = Quest::make_key(1, 3);
    std::vector<QuestFlagEntry> flags;

    expect(QuestMarker::kind_for_npc(Quest::make_key(2, 1), 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Available,
           "quest offer remains visible through intermediate dialogue navigation");

    expect(QuestMarker::kind_for_npc(0, 1, 0, flags, db, dialog_catalog) == QuestMarkerKind::None,
           "no key means no marker");
    expect(QuestMarker::kind_for_npc(gwang, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Available,
           "Gwang-Chun offers quest 1 at level 1");
    expect(QuestMarker::kind_for_npc(nurook, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "Nu-Rook has no offer before accept");

    flags.push_back(QuestFlagEntry{1, 1, 0});
    expect(QuestMarker::kind_for_npc(gwang, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Accepted,
           "Gwang-Chun switches to ? after accept");
    expect(QuestMarker::kind_for_npc(nurook, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Accepted,
           "Nu-Rook shows ? while flag 1 is active");

    flags[0].flag = 2;
    expect(QuestMarker::kind_for_npc(gwang, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Accepted,
           "Gwang-Chun keeps a yellow ? until the journal requirement is fulfilled");
    auto has_gwang_chun_goods = [](int index, const void*) { return index == 142 ? 1 : 0; };
    expect(QuestMarker::kind_for_npc(
               gwang, 1, 0, flags, db, dialog_catalog, &tasks, has_gwang_chun_goods, nullptr) ==
               QuestMarkerKind::ReadyToTurnIn,
           "Gwang-Chun shows a white ? when the journal requirement is green");
    expect(QuestMarker::kind_for_npc(nurook, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "Nu-Rook drops marker after his step");

    flags[0].clear = 1;
    expect(QuestMarker::kind_for_npc(nurook, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "cleared quest removes Nu-Rook marker");

    flags.clear();
    flags.push_back(QuestFlagEntry{1, 3, 1});
    flags.push_back(QuestFlagEntry{5, 0, 1});
    expect(QuestMarker::kind_for_npc(gwang, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "quest 6 is hidden below level 10");
    expect(QuestMarker::kind_for_npc(gwang, 10, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "quest 6 becomes no marker when no quest button is available");

    const std::uint32_t chun = Quest::make_key(9001, 1);
    const std::uint32_t jin = Quest::make_key(9004, 1);
    flags.clear();
    expect(QuestMarker::kind_for_npc(chun, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Available,
           "Chun-Sooin offers the job starter before flag 9001 exists");
    expect(QuestMarker::kind_for_npc(jin, 1, 0, flags, db, dialog_catalog) == QuestMarkerKind::None,
           "Jin-Pyung stays quiet until quest 9004 is granted");

    flags.push_back(QuestFlagEntry{9001, 1, 0});
    expect(QuestMarker::kind_for_npc(chun, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Available,
           "Chun-Sooin shows ! while 9001/1 is the un-talked NewPlayer flag");

    flags[0].flag = 2;
    expect(QuestMarker::kind_for_npc(chun, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Accepted,
           "Chun-Sooin shows ? after the starter talk");
    auto has_starter_letter = [](int index, const void*) { return index == 169 ? 1 : 0; };
    expect(QuestMarker::kind_for_npc(
               chun, 1, 0, flags, db, dialog_catalog, &tasks, has_starter_letter, nullptr) ==
               QuestMarkerKind::ReadyToTurnIn,
           "Chun-Sooin shows a white ? when the journal requirement is green");

    flags[0].flag = 3;
    flags[0].clear = 1;
    expect(QuestMarker::kind_for_npc(chun, 1, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::None,
           "Chun-Sooin drops the marker after 9001 is cleared");

    flags.clear();
    flags.push_back(QuestFlagEntry{9004, 1, 0});
    expect(QuestMarker::kind_for_npc(jin, 30, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Available,
           "Jin-Pyung shows ! at first-job flag 9004/1");

    flags[0].flag = 2;
    expect(QuestMarker::kind_for_npc(jin, 30, 0, flags, db, dialog_catalog) ==
               QuestMarkerKind::Accepted,
           "Jin-Pyung shows ? after the first-job talk");

    const std::uint32_t ju_bong = Quest::make_key(7, 4);
    flags.clear();
    flags.push_back(QuestFlagEntry{7, 2, 0});
    expect(QuestMarker::kind_for_npc(ju_bong, 11, 0, flags, db, dialog_catalog, &tasks) ==
               QuestMarkerKind::Accepted,
           "Ju-Bong shows a white ? before the chain task requirement is fulfilled");
    auto has_milk_bottles = [](int index, const void*) { return index == 153 ? 10 : 0; };
    expect(QuestMarker::kind_for_npc(
               ju_bong, 11, 0, flags, db, dialog_catalog, &tasks, has_milk_bottles, nullptr) ==
               QuestMarkerKind::ReadyToTurnIn,
           "Ju-Bong shows a yellow ? when a chain task can advance");

    openkal::client::MonsterQuestSlot hunt;
    hunt.set_quest_index(2);
    hunt.set_flag(1);
    const std::vector<openkal::client::MonsterQuestSlot> hunt_slots{hunt};
    flags.clear();
    expect(QuestMarker::kind_for_monster(hunt_slots, flags) == QuestMarkerKind::None,
           "no flags means no monster marker");
    flags.push_back(QuestFlagEntry{2, 1, 0});
    expect(QuestMarker::kind_for_monster(hunt_slots, flags) == QuestMarkerKind::Available,
           "active hunt quest shows !");
    expect(QuestMarker::kind_for_monster(hunt_slots, flags) != QuestMarkerKind::Accepted,
           "monster never uses ?");
    flags[0].flag = 2;
    expect(QuestMarker::kind_for_monster(hunt_slots, flags) == QuestMarkerKind::None,
           "wrong flag hides monster marker");
    flags[0].flag = 1;
    flags[0].clear = 1;
    expect(QuestMarker::kind_for_monster(hunt_slots, flags) == QuestMarkerKind::None,
           "cleared hunt quest hides monster marker");

    const auto* relics = tasks.find(26);
    expect(relics && relics->name == "The Stolen Royal Tomb Relics" && relics->flags.size() == 5,
           "legacy quest 26 has a readable journal for all five stages");
    openkal::client::MonsterQuestSlot milk;
    milk.set_quest_index(7);
    milk.set_flag(2);
    const std::vector<openkal::client::MonsterQuestSlot> milk_slots{milk};
    flags.clear();
    flags.push_back(QuestFlagEntry{7, 2, 0});
    auto missing = [](int, const void*) { return 0; };
    expect(QuestMarker::kind_for_monster(milk_slots, flags, &tasks, missing, nullptr) ==
               QuestMarkerKind::Available,
           "active hunt still shows ! without items");
    auto have_quota = [](int index, const void*) { return index == 153 ? 10 : 0; };
    expect(QuestMarker::kind_for_monster(milk_slots, flags, &tasks, have_quota, nullptr) ==
               QuestMarkerKind::None,
           "hunt ! hidden when item quota is already in the bag");
    expect(QuestMarker::kind_for_monster(milk_slots, flags) == QuestMarkerKind::Available,
           "without task catalog, item quota is ignored");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
