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
#include "openkal/map/Constants.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/ExpEventState.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Skill.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

    /** Writes a minimal JSON monster fixture.
     * @param path Destination JSON file path.
     * @return True when the complete fixture was written.
     */
    bool write_monster_fixture(const std::string& path) {
        std::ofstream out(path);
        if (!out) {
            return false;
        }
        out << R"({
  "index": 1,
  "name": "TestMob",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 0, "area": 1, "max": 40, "cycle": 1, "rect": [10, 10, 20, 20] }
  ]
})";
        return static_cast<bool>(out);
    }

    /** Writes one item definition with max-vital bonuses for login persistence coverage. */
    bool write_item_fixture(const std::string& path) {
        std::ofstream out(path);
        if (!out) {
            return false;
        }
        out << R"({"index":1,"name":"Test Sword","class":["weapon","sword"],"specialty":{"hp":10000,"mp":10000}})";
        return static_cast<bool>(out);
    }

    /** Waits for one framed server packet while discarding unrelated packets.
     * @param timeout Optional overall deadline; zero retains the legacy retry count.
     */
    bool wait_for_frame(openkal::server::ISession& session,
                        std::vector<std::uint8_t>& rx,
                        std::uint8_t expected_type,
                        std::vector<std::uint8_t>& expected_payload,
                        int* mortal_points = nullptr,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
        const bool has_deadline = timeout > std::chrono::milliseconds::zero();
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        int attempts = 0;
        for (;;) {
            if (!session.recv_into(rx)) {
                return false;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                std::vector<std::uint8_t> payload;
                const auto status = openkal::protocol::FrameParser::pop(
                    rx.data(), rx.size(), consumed, type, payload);
                if (status == openkal::protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                if (status == openkal::protocol::FrameParseStatus::Invalid) {
                    return false;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (mortal_points &&
                    type == static_cast<std::uint8_t>(openkal::protocol::S2C::UpdateProperty)) {
                    openkal::protocol::UpdatePropertyFields property;
                    if (openkal::protocol::Property::parse_update_any(
                            payload.data(), payload.size(), property) &&
                        property.property == openkal::protocol::kPropertyMortal) {
                        *mortal_points = property.word_value;
                    }
                }
                if (type == expected_type) {
                    expected_payload = std::move(payload);
                    return true;
                }
            }
            if (has_deadline) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    return false;
                }
            } else if (++attempts >= 400) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    /** Waits until the server confirms the requested buff state. */
    bool wait_for_buff(openkal::server::ISession& session,
                       std::vector<std::uint8_t>& rx,
                       std::uint64_t expected_bstate) {
        for (int attempt = 0; attempt < 400; ++attempt) {
            std::vector<std::uint8_t> payload;
            if (!wait_for_frame(session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::MessageV),
                                payload)) {
                return false;
            }
            if (payload.empty() ||
                payload.front() !=
                    static_cast<std::uint8_t>(openkal::protocol::BStateMsg::AddBState)) {
                continue;
            }
            std::uint64_t bstate = 0;
            std::int32_t remain = 0;
            std::int32_t skill_index = 0;
            std::int32_t item_index = 0;
            if (openkal::protocol::BuffState::parse_add(payload.data() + 1,
                                                        payload.size() - 1,
                                                        bstate,
                                                        remain,
                                                        skill_index,
                                                        item_index) &&
                (bstate & expected_bstate) != 0) {
                return true;
            }
        }
        return false;
    }

}  // namespace

int main() {
    using openkal::server::GameWorld;
    using openkal::test::expect;

    const std::string fixture_dir = openkal::test::temp_dir("server_fast_json");
    const std::string monsters_dir = openkal::platform::join_path(fixture_dir, "monsters");
    const std::string maps_dir = openkal::platform::join_path(fixture_dir, "maps");
    openkal::test::mkdir_p(monsters_dir);
    openkal::test::mkdir_p(maps_dir);

    expect(write_monster_fixture(openkal::platform::join_path(monsters_dir, "0001.json")),
           "write isolated monster JSON fixture");
    expect(
        openkal::test::write_open_area(openkal::platform::join_path(maps_dir, "n_000_000.akarea")),
        "write isolated open map fixture");

    constexpr int kPosition = 15 * openkal::map::kTileSize;
    GameWorld world;
    expect(world.load_monsters_json_dir(monsters_dir, kPosition, kPosition, 0),
           "load monster JSON fixture");
    expect(world.load_maps_around(maps_dir, kPosition, kPosition, 1), "load map fixture");

    std::uint32_t next_entity_id = 1;
    expect(world.ensure_monsters_around(kPosition, kPosition, 1024, next_entity_id, 40) == 40,
           "spawn the configured monster count without an area cap");
    expect(world.monsters().size() == 40, "retain the configured monster count");
    world.spawn_from_config(next_entity_id, 40, kPosition, kPosition);
    expect(world.monsters().size() == 40,
           "initial config spawn retains the configured monster count");

    {
        const std::string emok_dir = openkal::test::temp_dir("server_fast_emok_map");
        const std::string json_path = openkal::platform::join_path(emok_dir, "0393.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write Emok map-20 monster json");
            out << R"({
  "index": 393,
  "name": "SealedGhost",
  "stats": { "race": 1, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 20, "area": 2601, "max": 1, "cycle": 5, "rect": [10631, 8115, 10698, 8405] }
  ]
})";
        }
        GameWorld emok_world;
        expect(emok_world.load_monsters_json_dir(emok_dir, 0, 0, 0),
               "load Emok map-20 monster json");
        openkal::map::MapCatalog emok_maps;
        expect(emok_maps.load(
                   openkal::platform::join_path(OPENKAL_TEST_RESOURCE_ROOT, "config/mapinfo.json")),
               "load complete map definition for Emok");
        const auto emok_cell = emok_maps.cell_index_at_pixel(340000, 260000);
        expect(emok_cell.has_value() && *emok_cell == 20,
               "Emok physical cell retains its logical map index");
        emok_world.bind_field_spawns_to_overworld(emok_maps);
        const openkal::server::MonsterSpawn* emok = emok_world.find_spawn_by_area(2601);
        expect(emok != nullptr && emok->map_index == 20,
               "Emok gen remains on map 20 after outdoor spawn binding");
    }

    const auto run_nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string save_dir =
        openkal::test::temp_dir((std::string("server_fast_persistence_") +
                                 std::to_string(::getpid()) + "_" + std::to_string(run_nonce))
                                    .c_str());
    auto store = openkal::server::create_data_store(openkal::server::StoreBackend::Sqlite);
    expect(store != nullptr && store->open(save_dir), "open SQLite persistence fixture");
    openkal::server::CharacterRecord character;
    character.name = "PersistenceHero";
    character.face = 3;
    character.hair = 6;
    character.hp = 80;
    character.mp = 40;
    character.rage = 777;
    character.revival_point = 7;
    expect(store->ensure_default_player("persistence", "persistence", character),
           "create persistence character");
    const auto account = store->find_account("persistence");
    expect(account.has_value(), "load persistence account");
    const auto characters = account ? store->list_characters(account->id)
                                    : std::vector<openkal::server::CharacterRecord>{};
    expect(characters.size() == 1, "load persistence character list");
    expect(characters.size() == 1 && characters.front().face == 3 && characters.front().hair == 6,
           "character list retains natural appearance");
    if (!characters.empty()) {
        auto loaded = store->load_character(characters.front().id);
        expect(loaded.has_value() && loaded->rage == 777 && loaded->revival_point == 7,
               "load initial persisted rage and revival point");
        if (loaded) {
            loaded->face = 4;
            loaded->hair = 2;
            loaded->hp = 0;
            loaded->mp = 0;
            loaded->rage = 5432;
            loaded->revival_point = 12;
            expect(store->save_character(*loaded), "save zero vitals and rage");
        }
        store->close();
        expect(store->open(save_dir), "reopen SQLite persistence fixture");
        const auto reloaded = store->load_character(characters.front().id);
        expect(reloaded.has_value() && reloaded->hp == 0 && reloaded->mp == 0 &&
                   reloaded->rage == 5432 && reloaded->revival_point == 12,
               "reload zero vitals, rage, and revival point exactly");
        expect(reloaded.has_value() && reloaded->face == 4 && reloaded->hair == 2,
               "appearance survives update and database reopen");
        const std::vector<openkal::server::InventoryUnlockRecord> unlocks = {
            {characters.front().id, 0, 2592000},
            {characters.front().id, 1, 604800},
        };
        expect(store->replace_inventory_unlocks(characters.front().id, unlocks),
               "persist timed inventory register unlocks");
        const auto persisted_unlocks = store->load_inventory_unlocks(characters.front().id);
        expect(persisted_unlocks.size() == 2 && persisted_unlocks[0].register_index == 0 &&
                   persisted_unlocks[0].remain == 2592000 &&
                   persisted_unlocks[1].register_index == 1 &&
                   persisted_unlocks[1].remain == 604800,
               "reload timed inventory register unlocks in register order");
        const std::vector<openkal::server::BuffRemainRecord> buff_remains = {
            {characters.front().id, 101, 43200, 0, 0, 2411},
        };
        expect(store->replace_buff_remains(characters.front().id, buff_remains),
               "persist pay-buff source item");
        const auto persisted_buff_remains = store->load_buff_remains(characters.front().id);
        expect(persisted_buff_remains.size() == 1 &&
                   persisted_buff_remains.front().source_item_index == 2411,
               "reload pay-buff source item");

        openkal::server::ItemRecord persisted_pet;
        persisted_pet.iid = 990000;
        persisted_pet.character_id = characters.front().id;
        persisted_pet.index = 1747;
        persisted_pet.count = 1;
        persisted_pet.slot = 4;
        persisted_pet.pet_expire_at = 1'234'567;
        expect(store->insert_item(persisted_pet), "persist pet expiry timestamp");
        const auto persisted_items = store->load_items(characters.front().id);
        const auto pet_item = std::find_if(persisted_items.begin(),
                                           persisted_items.end(),
                                           [](const auto& item) { return item.iid == 990000; });
        expect(pet_item != persisted_items.end() && pet_item->slot == 4 &&
                   pet_item->pet_expire_at == 1'234'567,
               "reload pet slot and expiry timestamp exactly");

        openkal::server::ItemRecord persisted_ride;
        persisted_ride.iid = 990001;
        persisted_ride.character_id = characters.front().id;
        persisted_ride.index = 30000;
        persisted_ride.count = 1;
        persisted_ride.slot = 5;
        persisted_ride.ride_expire_at = 7'654'321;
        persisted_ride.account_bound = true;
        expect(store->insert_item(persisted_ride), "persist riding expiry timestamp");
        const auto persisted_items_with_ride = store->load_items(characters.front().id);
        const auto ride_item = std::find_if(persisted_items_with_ride.begin(),
                                            persisted_items_with_ride.end(),
                                            [](const auto& item) { return item.iid == 990001; });
        expect(ride_item != persisted_items_with_ride.end() && ride_item->slot == 5 &&
                   ride_item->ride_expire_at == 7'654'321 && ride_item->account_bound,
               "reload riding slot, expiry timestamp, and account binding exactly");

        openkal::server::BattleMountRecord battle_mount;
        battle_mount.mount_iid = persisted_ride.iid;
        battle_mount.grade = openkal::protocol::BattleRidingGrade::Rare;
        battle_mount.satiety = 83;
        battle_mount.revision = 9;
        battle_mount.abilities[0] = 4;
        battle_mount.equipment_iids[2] = 880001;
        expect(store->upsert_battle_mount(battle_mount),
               "persist per-IID Battle Riding progression");
        const auto persisted_battle_mount = store->load_battle_mount(persisted_ride.iid);
        expect(persisted_battle_mount &&
                   persisted_battle_mount->grade == openkal::protocol::BattleRidingGrade::Rare &&
                   persisted_battle_mount->satiety == 83 && persisted_battle_mount->revision == 9 &&
                   persisted_battle_mount->abilities[0] == 4 &&
                   persisted_battle_mount->equipment_iids[2] == 880001,
               "reload Battle Riding grade, satiety, abilities, gear and revision");

        openkal::server::ShortcutRecord saved_bar;
        saved_bar.character_id = characters.front().id;
        saved_bar.active_index = 4;
        saved_bar.slots[0] = 4;
        saved_bar.slots[10] = 12;
        saved_bar.slots[19] = 0x802f;
        expect(store->upsert_shortcuts(saved_bar), "persist both shortcut rows");
        const auto full_bar = store->load_shortcuts(saved_bar.character_id);
        expect(full_bar && full_bar->slots == saved_bar.slots, "twenty shortcuts reload exactly");
        store->close();
        sqlite3* legacy_db = nullptr;
        const std::string db_path =
            openkal::platform::join_path(save_dir, openkal::server::kSqliteFileName);
        expect(sqlite3_open(db_path.c_str(), &legacy_db) == SQLITE_OK,
               "open legacy migration fixture");
        if (legacy_db) {
            const int legacy_rc =
                sqlite3_exec(legacy_db,
                             "ALTER TABLE characters DROP COLUMN face;"
                             "ALTER TABLE characters DROP COLUMN hair;"
                             "ALTER TABLE characters DROP COLUMN rage;"
                             "ALTER TABLE characters DROP COLUMN revival_point;"
                             "DELETE FROM schema_version WHERE version IN (7, 8);",
                             nullptr,
                             nullptr,
                             nullptr);
            expect(legacy_rc == SQLITE_OK, "downgrade fixture to schema v6");
            for (int slot = 10; slot < 20; ++slot) {
                const std::string sql =
                    "ALTER TABLE shortcuts DROP COLUMN slot" + std::to_string(slot);
                expect(sqlite3_exec(legacy_db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
                       "restore legacy ten-slot fixture");
            }
            sqlite3_close(legacy_db);
        }
        expect(store->open(save_dir), "migrate SQLite persistence fixture to v8");
        const auto legacy_bar = store->load_shortcuts(saved_bar.character_id);
        expect(legacy_bar && legacy_bar->active_index == 4 && legacy_bar->slots[0] == 4 &&
                   legacy_bar->slots[10] == -1 && legacy_bar->slots[19] == -1,
               "migration preserves old shortcuts and initializes Shift row empty");
        expect(store->upsert_shortcuts(saved_bar), "save upper row after migration");
        store->close();
        expect(store->open(save_dir), "reopen migrated shortcut database");
        const auto reopened_bar = store->load_shortcuts(saved_bar.character_id);
        expect(reopened_bar && reopened_bar->slots == saved_bar.slots,
               "upper row survives server restart and repeated migration");
        const auto migrated = store->load_character(characters.front().id);
        expect(migrated.has_value() && migrated->hp == 0 && migrated->mp == 0 &&
                   migrated->rage == 0 && migrated->revival_point == 0,
               "schema v8 migration adds rage and revival point without changing zero vitals");
        expect(migrated.has_value() && migrated->face == 1 && migrated->hair == 1,
               "appearance migration defaults old characters even with a newer version marker");

        constexpr std::int64_t deletion_requested_at = 1'000'000;
        const std::int32_t deletion_id = characters.front().id;
        openkal::server::ItemRecord deleted_item;
        deleted_item.iid = 990001;
        deleted_item.character_id = deletion_id;
        deleted_item.index = 1;
        deleted_item.count = 1;
        expect(store->insert_item(deleted_item), "prepare deleted-character item");
        openkal::server::SkillRecord deleted_skill;
        deleted_skill.character_id = deletion_id;
        deleted_skill.skill_index = 77;
        deleted_skill.level = 1;
        expect(store->upsert_skill(deleted_skill), "prepare deleted-character skill");
        openkal::server::ShortcutRecord deleted_shortcuts;
        deleted_shortcuts.character_id = deletion_id;
        deleted_shortcuts.active_index = 0;
        deleted_shortcuts.slots[0] = 77;
        expect(store->upsert_shortcuts(deleted_shortcuts), "prepare deleted-character shortcuts");
        openkal::server::QuestFlagRecord deleted_quest;
        deleted_quest.character_id = deletion_id;
        deleted_quest.quest = 44;
        deleted_quest.flag = 2;
        expect(store->upsert_quest_flag(deleted_quest), "prepare deleted-character quest");

        expect(store->schedule_character_deletion(deletion_id, account->id, deletion_requested_at),
               "soft-delete character");
        expect(store->list_characters(account->id).empty(),
               "soft-deleted character leaves active list");
        expect(!store->load_character(deletion_id).has_value(),
               "soft-deleted character cannot enter the world");
        const auto deleted = store->list_deleted_characters(account->id, deletion_requested_at + 1);
        expect(deleted.size() == 1 &&
                   deleted.front().delete_after ==
                       deletion_requested_at + openkal::server::kCharacterDeletionGraceSeconds,
               "soft-deleted character has a 30-day recovery deadline");
        expect(store->restore_character(deletion_id, account->id, deletion_requested_at + 1),
               "restore character during recovery window");
        expect(store->list_characters(account->id).size() == 1,
               "restored character returns to active list");

        constexpr std::int64_t final_deletion_at = 2'000'000;
        const std::int64_t purge_at =
            final_deletion_at + openkal::server::kCharacterDeletionGraceSeconds;
        expect(store->schedule_character_deletion(deletion_id, account->id, final_deletion_at),
               "soft-delete restored character again");
        expect(store->process_character_deletions(purge_at - 1, 1) ==
                   openkal::server::CharacterPurgeResult::Idle,
               "cleanup does not start before the recovery deadline");
        expect(store->process_character_deletions(purge_at, 1) ==
                   openkal::server::CharacterPurgeResult::WorkRemaining,
               "expired cleanup removes only a bounded dependent-row batch");
        expect(!store->restore_character(deletion_id, account->id, purge_at),
               "purging character cannot be restored");
        openkal::server::CharacterPurgeResult purge_result =
            openkal::server::CharacterPurgeResult::WorkRemaining;
        for (int step = 0;
             step < 20 && purge_result != openkal::server::CharacterPurgeResult::CharacterPurged;
             ++step) {
            purge_result = store->process_character_deletions(purge_at, 1);
        }
        expect(purge_result == openkal::server::CharacterPurgeResult::CharacterPurged,
               "character row is removed only after dependent cleanup completes");
        expect(store->load_items(deletion_id).empty() && store->load_skills(deletion_id).empty() &&
                   !store->load_shortcuts(deletion_id).has_value() &&
                   store->load_quest_flags(deletion_id).empty() &&
                   store->load_buff_remains(deletion_id).empty() &&
                   store->load_inventory_unlocks(deletion_id).empty(),
               "expired cleanup removes all character-owned persistence rows");
    }

    store->close();

    const auto exit_nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string exit_root =
        openkal::test::temp_dir((std::string("server_game_exit_") + std::to_string(::getpid()) +
                                 "_" + std::to_string(exit_nonce))
                                    .c_str());
    const std::string exit_maps = openkal::platform::join_path(exit_root, "maps");
    const std::string exit_monsters = openkal::platform::join_path(exit_root, "monsters");
    const std::string exit_items = openkal::platform::join_path(exit_root, "items");
    const std::string exit_save = openkal::platform::join_path(exit_root, "save");
    openkal::test::mkdir_p(exit_maps);
    openkal::test::mkdir_p(exit_monsters);
    openkal::test::mkdir_p(exit_items);
    expect(
        openkal::test::write_open_area(openkal::platform::join_path(exit_maps, "n_000_000.akarea")),
        "write game-exit map fixture");
    expect(write_monster_fixture(openkal::platform::join_path(exit_monsters, "0001.json")),
           "write game-exit monster fixture");
    expect(write_item_fixture(openkal::platform::join_path(exit_items, "0001.json")),
           "write game-exit item fixture");

    auto exit_store = openkal::server::create_data_store(openkal::server::StoreBackend::Sqlite);
    expect(exit_store != nullptr && exit_store->open(exit_save), "open game-exit store");
    openkal::server::CharacterRecord exit_character;
    exit_character.name = "ExitThief";
    exit_character.clazz = 3;
    exit_character.face = 6;
    exit_character.hair = 4;
    exit_character.map_x = kPosition;
    exit_character.map_y = kPosition;
    exit_character.level = 15;
    exit_character.hp = 20000;
    exit_character.max_hp = 20000;
    exit_character.mp = 20000;
    exit_character.max_mp = 20000;
    exit_character.str = 19;
    exit_character.hth = 17;
    exit_character.intl = 9;
    exit_character.wis = 9;
    exit_character.dex = 11;
    expect(exit_store->ensure_default_player("exit", "exit", exit_character),
           "create game-exit character");
    const auto exit_account = exit_store->find_account("exit");
    const auto exit_characters = exit_account ? exit_store->list_characters(exit_account->id)
                                              : std::vector<openkal::server::CharacterRecord>{};
    expect(exit_characters.size() == 1, "resolve game-exit character");
    const std::int32_t exit_character_id = exit_characters.empty() ? 0 : exit_characters.front().id;
    openkal::server::ItemRecord exit_weapon;
    exit_weapon.iid = 1;
    exit_weapon.character_id = exit_character_id;
    exit_weapon.index = 1;
    exit_weapon.count = 1;
    exit_weapon.slot = 1;
    expect(exit_store->insert_item(exit_weapon), "equip game-exit max-vitals item");
    openkal::server::SkillRecord hiding_skill;
    hiding_skill.character_id = exit_character_id;
    hiding_skill.skill_index = 6;
    hiding_skill.level = 1;
    expect(exit_store->upsert_skill(hiding_skill), "learn Hiding for movement fixture");
    openkal::server::ItemRecord event_coins;
    event_coins.iid = 2;
    event_coins.character_id = exit_character_id;
    event_coins.index = 23016;
    event_coins.count = 50;
    expect(exit_store->insert_item(event_coins), "seed first coin stack");
    event_coins.iid = 3;
    event_coins.count = 100;
    expect(exit_store->insert_item(event_coins), "seed second coin stack");
    {
        std::ofstream coin(openkal::platform::join_path(exit_items, "23016.json"));
        coin
            << R"({"index":23016,"name":"Exp Event Coin","class":["general","etc"],"stackable":1})";
    }
    const auto event_now = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    expect(exit_store->save_server_event({"exp_event", 437, 10, event_now + 4}),
           "seed active event with partial upgrade progress");
    exit_store->close();

    openkal::server::ServerConfig server_config;
    server_config.server_events_path = openkal::platform::join_path(exit_root, "events.json");
    {
        std::ofstream events(server_config.server_events_path);
        events << R"({"events":[{"id":"exp_event","managerNpcIndex":5037,
            "donateQuest":{"index":9501,"flag":1},"donateHundredQuest":{"index":9502,"flag":1},
            "donationItem":23016,"durationSeconds":2,"extensionSeconds":2,
            "requiredItems":500,"bonusStepPercent":10,"maxBonusPercent":50}]})";
    }
    server_config.npcs_dir = openkal::platform::join_path(exit_root, "npcs");
    openkal::test::mkdir_p(server_config.npcs_dir);
    {
        std::ofstream npc(openkal::platform::join_path(server_config.npcs_dir, "5037.json"));
        npc << R"({"index":5037,"kind":3,"shape":75,"map":0,"html":690,"xy":[)" << kPosition << ","
            << kPosition << R"(,0]})";
    }
    server_config.maps_dir = exit_maps;
    server_config.save_dir = exit_save;
    server_config.monsters_dir = exit_monsters;
    server_config.items_dir = exit_items;
    server_config.skills_dir = std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/skills";
    server_config.store = openkal::server::StoreBackend::Sqlite;
    server_config.character_id = exit_character_id;
    server_config.transport = openkal::server::TransportKind::Unix;
    server_config.unix_path = openkal::platform::join_path(exit_root, "game-exit.sock");
    server_config.spawn_x = kPosition;
    server_config.spawn_y = kPosition;
    server_config.map_radius_cells = 1;
    server_config.max_monsters = 4;

    openkal::server::ServerHost host;
    expect(host.start(server_config), "start game-exit server");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto session = openkal::server::connect_unix(server_config.unix_path);
    expect(session != nullptr, "connect game-exit client");
    std::vector<std::uint8_t> rx;
    std::vector<std::uint8_t> payload;
    if (session) {
        expect(session->send_all(openkal::protocol::Handshake::build_connect(0)),
               "send game-exit connect");
        expect(wait_for_frame(
                   *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
               "receive game-exit handshake code");
        openkal::protocol::S2CCodeFields code;
        expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse game-exit handshake code");
        expect(
            session->send_all(openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
            "send game-exit handshake answer");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::CreatePlayer),
                              payload),
               "enter game-exit world");
        openkal::protocol::ExpEventStateFields event_state;
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::ExpEventState),
                              payload) &&
                   openkal::protocol::ExpEventState::parse(
                       payload.data(), payload.size(), event_state) &&
                   event_state.enabled && event_state.contributed == 437 &&
                   event_state.target == 500 && event_state.active_bonus_percent == 10 &&
                   event_state.remaining_seconds > 0,
               "new player receives authoritative noticeboard snapshot at login");
        openkal::protocol::CreateNpcInfo manager;
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::CreateNpc),
                              payload) &&
                   openkal::protocol::Quest::parse_create_npc(
                       payload.data(), payload.size(), manager) &&
                   manager.index == 5037,
               "receive EXP donation manager");
        expect(session->send_all(openkal::protocol::Quest::frame_ask_npc(manager.id)) &&
                   session->send_all(openkal::protocol::Quest::frame_quest(
                       openkal::protocol::Quest::make_key(9502, 1))),
               "click exactly-100 donation while EXP event is active");
        const auto upgrade_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        bool received_upgrade = false;
        while (std::chrono::steady_clock::now() < upgrade_deadline) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::ExpEventState),
                                payload,
                                nullptr,
                                std::chrono::milliseconds(1500))) {
                break;
            }
            if (openkal::protocol::ExpEventState::parse(
                    payload.data(), payload.size(), event_state) &&
                event_state.active_bonus_percent == 20 && event_state.contributed == 37) {
                received_upgrade = true;
                break;
            }
        }
        expect(received_upgrade && event_state.remaining_seconds > 0,
               "100 coins upgrade the active bonus and broadcast the remaining progress");
        expect(exit_store->open(exit_save), "inspect donated inventory");
        const auto remaining_items = exit_store->load_items(exit_character_id);
        int remaining_coins = 0;
        for (const auto& item : remaining_items) {
            if (item.index == 23016) {
                remaining_coins += item.count;
            }
        }
        expect(
            remaining_coins == 50 &&
                std::any_of(remaining_items.begin(),
                            remaining_items.end(),
                            [](const auto& item) { return item.iid == 3 && item.count == 50; }) &&
                std::none_of(remaining_items.begin(),
                             remaining_items.end(),
                             [](const auto& item) { return item.iid == 2; }),
            "fixed donation preserves the correct remaining stack after bag compaction");
        exit_store->close();
        expect(session->send_all(openkal::protocol::Quest::frame_quest(
                   openkal::protocol::Quest::make_key(9502, 1))),
               "try fixed donation with only 50 coins remaining");
        bool rejected = false;
        const auto rejection_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < rejection_deadline) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::Chatting),
                                payload,
                                nullptr,
                                std::chrono::milliseconds(1000))) {
                break;
            }
            const std::string text(payload.begin(), payload.end());
            if (text.find("You need at least 100") != std::string::npos) {
                rejected = true;
                break;
            }
        }
        expect(rejected, "fixed donation rejects insufficient coins instead of donating fewer");
        expect(session->send_all(openkal::protocol::Quest::frame_quest(
                   openkal::protocol::Quest::make_key(9501, 1))),
               "donate-all accepts the remaining 50 coins during the event");
        const auto event_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        bool received_expiry = false;
        while (std::chrono::steady_clock::now() < event_deadline) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::ExpEventState),
                                payload,
                                nullptr,
                                std::chrono::milliseconds(1500))) {
                break;
            }
            if (openkal::protocol::ExpEventState::parse(
                    payload.data(), payload.size(), event_state) &&
                event_state.active_bonus_percent == 0 && event_state.remaining_seconds == 0) {
                received_expiry = true;
                break;
            }
        }
        expect(received_expiry && event_state.contributed == 87,
               "expiry retains all partial credit, including donate-all, without a client query");
        expect(session->send_all(openkal::protocol::Skill::frame_client_self(6)),
               "activate Hiding before movement");
        expect(wait_for_buff(*session, rx, openkal::protocol::kCbsSkillHiding),
               "server confirms Hiding");
        expect(session->send_all(openkal::protocol::Move::frame_client_on(1, 0, 0)),
               "move one server unit while hidden");
        expect(session->send_all(openkal::protocol::GameExit::frame_client_request()),
               "request graceful game exit");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::AnsGameExit),
                              payload),
               "receive game-exit persistence acknowledgement");
        bool exit_saved = false;
        expect(openkal::protocol::GameExit::parse_server_answer(
                   payload.data(), payload.size(), exit_saved) &&
                   exit_saved,
               "server acknowledged persisted game-exit state");
        session->close();
    }
    host.stop();

    expect(exit_store->open(exit_save), "reopen game-exit store");
    const auto donated_inventory = exit_store->load_items(exit_character_id);
    expect(std::none_of(donated_inventory.begin(),
                        donated_inventory.end(),
                        [](const auto& item) { return item.index == 23016; }),
           "both donation options persist the exact inventory removal");
    const auto persisted_exit_character = exit_store->load_character(exit_character_id);
    expect(persisted_exit_character.has_value(), "reload game-exit character");
    if (persisted_exit_character) {
        expect(persisted_exit_character->face == 6 && persisted_exit_character->hair == 4,
               "server login and exit preserve the created appearance");
        expect(persisted_exit_character->max_hp > 10000,
               "equipped item increased authoritative max hp");
        expect(persisted_exit_character->max_mp > 10000,
               "equipped item increased authoritative max mp");
        expect(persisted_exit_character->hp > 0 &&
                   persisted_exit_character->hp <= persisted_exit_character->max_hp,
               "game exit persisted valid current hp");
        expect(persisted_exit_character->mp == persisted_exit_character->max_mp - 20,
               "game exit persisted Hiding's Energy cost");
        expect(persisted_exit_character->map_x == kPosition + 1,
               "hidden movement advances the authoritative server position");
    }
    exit_store->close();

    // Exercise the actual Knight packet path and resource lifecycle.
    expect(exit_store->open(exit_save), "reopen store for Knight skill audit");
    auto knight = exit_store->load_character(exit_character_id);
    if (knight) {
        knight->clazz = 0;
        knight->level = 70;
        knight->specialty = openkal::protocol::kSpecialtySecondJobA;
        knight->map_x = kPosition;
        knight->map_y = kPosition;
        knight->hp = knight->mp = 20000;
        knight->dex = 200;
        expect(exit_store->save_character(*knight), "prepare Knight Vagabond fixture");
        for (const int index : {3, 16, 17, 21, 25, 38}) {
            openkal::server::SkillRecord skill;
            skill.character_id = exit_character_id;
            skill.skill_index = index;
            skill.level = 1;
            expect(exit_store->upsert_skill(skill), "learn Knight audit skills");
        }
        openkal::server::ItemRecord shield;
        shield.iid = 4;
        shield.character_id = exit_character_id;
        shield.index = 4;
        shield.count = 1;
        shield.slot = 2;
        expect(exit_store->insert_item(shield), "save legacy shield plus two-handed weapon");
        openkal::server::ItemRecord one_hand = shield;
        one_hand.iid = 5;
        one_hand.index = 5;
        one_hand.slot = 0;
        expect(exit_store->insert_item(one_hand), "add one-handed weapon for equipment checks");
    }
    exit_store->close();
    {
        std::ofstream monster(openkal::platform::join_path(exit_monsters, "0001.json"));
        monster << R"({"index":1,"name":"Skill Audit Target",
            "stats":{"race":0,"level":1,"hp":1000000,"attack":{"min":1,"max":1}},
            "spawns":[{"map":0,"area":1,"max":1,"cycle":1,"rect":[15,15,16,16]}]})";
    }
    {
        std::ofstream weapon(openkal::platform::join_path(exit_items, "0001.json"));
        weapon << R"({"index":1,"name":"Audit Two-Hand Sword","wear":1,"class":["weapon","sword2h"],
            "range":48,"specialty":{"hp":10000,"mp":10000}})";
    }
    for (const int index : {2, 3}) {
        std::ofstream monster(
            openkal::platform::join_path(exit_monsters, index == 2 ? "0002.json" : "0003.json"));
        const int tile_x = index == 2 ? 13 : 17;
        monster << "{\"index\":" << index
                << R"(,"name":"Spin Slash Area Target","stats":{"race":0,"level":1,
                    "hp":1000000,"attack":{"min":1,"max":1}},
                    "spawns":[{"map":0,"area":1,"max":1,"cycle":1,"rect":[)"
                << tile_x << ",15," << tile_x + 1 << ",16]}]}";
    }
    server_config.max_monsters = 3;
    {
        std::ofstream shield(openkal::platform::join_path(exit_items, "0004.json"));
        shield << R"({"index":4,"name":"Audit Shield","wear":1,"class":["defense","shield"]})";
        std::ofstream weapon(openkal::platform::join_path(exit_items, "0005.json"));
        weapon << R"({"index":5,"name":"Audit One-Hand Sword","wear":1,"class":["weapon","sword"],
            "range":48,"specialty":{"hp":10000,"mp":10000}})";
    }
    expect(host.start(server_config), "start Knight skill audit server");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session = openkal::server::connect_unix(server_config.unix_path);
    rx.clear();
    if (session) {
        expect(session->send_all(openkal::protocol::Handshake::build_connect(0)), "connect Knight");
        expect(wait_for_frame(
                   *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
               "receive Knight handshake");
        openkal::protocol::S2CCodeFields code;
        expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse Knight handshake");
        expect(
            session->send_all(openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
            "answer Knight handshake");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::CreateMonster),
                              payload),
               "receive Knight target");
        openkal::protocol::PacketReader reader(payload.data(), payload.size());
        std::uint16_t monster_index = 0;
        std::uint32_t monster_id = 0;
        expect(reader.read_u16(monster_index) && reader.read_u32(monster_id),
               "parse Knight target identity");
        while (monster_index != 1) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::CreateMonster),
                                payload)) {
                expect(false, "find the close-range Knight target");
                break;
            }
            openkal::protocol::PacketReader next(payload.data(), payload.size());
            expect(next.read_u16(monster_index) && next.read_u32(monster_id),
                   "parse the next Knight target identity");
        }
        // Let initial spawn protection expire before checking positive damage.
        std::this_thread::sleep_for(std::chrono::milliseconds(2200));
        expect(exit_store->open(exit_save), "inspect equipment after login repair");
        const auto repaired_items = exit_store->load_items(exit_character_id);
        const auto repaired_shield = std::find_if(repaired_items.begin(),
                                                  repaired_items.end(),
                                                  [](const auto& item) { return item.iid == 4; });
        expect(repaired_shield != repaired_items.end() && repaired_shield->slot == 0 &&
                   repaired_shield->count == 1,
               "login unequips and persists the conflicting shield without deleting it");
        exit_store->close();
        const auto cast = [&](int index) {
            return session->send_all(openkal::protocol::Skill::frame_client_target(
                static_cast<std::uint8_t>(index), openkal::protocol::kCkMonster, monster_id));
        };
        int mortal_points = -1;
        const auto read_skill = [&](openkal::protocol::S2CSkillFields& result) {
            for (int attempt = 0; attempt < 8; ++attempt) {
                if (!wait_for_frame(*session,
                                    rx,
                                    static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                    payload,
                                    &mortal_points) ||
                    !openkal::protocol::Skill::parse_server(
                        payload.data(), payload.size(), result)) {
                    return false;
                }
                if (result.has_damage) {
                    return true;
                }
            }
            return false;
        };
        expect(cast(17), "submit Brutal Attack with zero Deathblow points");
        expect(session->send_all(openkal::protocol::Attack::frame_client(
                   openkal::protocol::kCkMonster, monster_id)),
               "start normal attack before the charging skill");
        expect(cast(16), "submit Powerful Upward Slash during normal attack recovery");
        openkal::protocol::S2CSkillFields result;
        expect(read_skill(result) && result.skill == 16,
               "uncharged Brutal Attack is rejected before the next valid skill");
        for (int retry = 0; result.damage == 0 && retry < 3; ++retry) {
            std::this_thread::sleep_for(std::chrono::milliseconds(6000));
            expect(cast(16) && read_skill(result), "retry source accuracy roll for Upward Slash");
        }
        expect(result.damage > 0 && mortal_points == 1,
               "Upward Slash after a normal attack sends one Deathblow point to the client");
        expect(cast(17), "submit charged Brutal Attack");
        expect(read_skill(result) && result.skill == 17 && mortal_points == 0,
               "Brutal Attack consumes the point and sends the cleared counter");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        expect(cast(17) && cast(3), "submit spent finisher followed by Lightning Slash");
        expect(read_skill(result) && result.skill == 3,
               "Brutal Attack consumes its points even if its accuracy roll misses");
        const auto half_swing_started = std::chrono::steady_clock::now();
        expect(cast(25) && read_skill(result) && result.skill == 25 && result.damage > 0 &&
                   mortal_points == 1,
               "Two-hand Swing uses the shared area handler and hits without an accuracy roll");
        expect(cast(17) && read_skill(result) && result.skill == 17,
               "Two-hand Swing also charges the Deathblow finisher");
        const auto read_cast = [&](int expected) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                payload)) {
                return false;
            }
            openkal::protocol::S2CSkillFields skill;
            return openkal::protocol::Skill::parse_server(payload.data(), payload.size(), skill) &&
                   skill.skill == expected;
        };
        // A basic skill provides an ordered sentinel after a rejected Rush.
        expect(cast(21), "reject Rush without its approach phase");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        expect(cast(3) && read_cast(3), "unprepared Rush emits no cast");
        expect(session->send_all(openkal::protocol::Skill::frame_preskill(21, monster_id)) &&
                   cast(21),
               "attempt Rush inside its minimum starting distance");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        expect(cast(3) && read_cast(3),
               "near-target Rush is rejected without spending its cooldown");
        expect(session->send_all(openkal::protocol::Move::frame_client_on(-40, 0, 0)) &&
                   session->send_all(openkal::protocol::Move::frame_client_end(-40, 0, 0)),
               "move to Rush starting range");
        expect(session->send_all(openkal::protocol::Skill::frame_preskill(21, monster_id)),
               "prepare Rush at distance");
        expect(cast(21), "attempt Rush before reaching weapon range");
        expect(session->send_all(openkal::protocol::Move::frame_client_on(40, 0, 0)) &&
                   session->send_all(openkal::protocol::Move::frame_client_end(40, 0, 0)) &&
                   cast(21),
               "finish Rush after the approach");
        expect(read_cast(21), "Rush applies at close range after a valid approach");
        expect(cast(21), "attempt immediate Rush reuse");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        expect(cast(3) && read_cast(3), "Rush cannot reuse its consumed preparation");
        std::this_thread::sleep_until(half_swing_started + std::chrono::milliseconds(10100));
        expect(cast(25) && read_skill(result) && result.skill == 25 && result.damage > 0 &&
                   mortal_points == 1,
               "build a Deathblow point before Spin Slash");
        expect(session->send_all(openkal::protocol::Skill::frame_client_self(38)),
               "cast Spin Slash without a selected target during attack recovery");
        std::vector<std::uint32_t> spin_targets;
        for (int hit = 0; hit < 3; ++hit) {
            expect(read_skill(result) && result.skill == 38 && result.damage > 0,
                   "Spin Slash damages each surrounding enemy beyond weapon range");
            spin_targets.push_back(result.target);
        }
        std::sort(spin_targets.begin(), spin_targets.end());
        expect(std::unique(spin_targets.begin(), spin_targets.end()) == spin_targets.end() &&
                   mortal_points == 0,
               "Spin Slash hits three different enemies and consumes its Deathblow points");
        expect(session->send_all(openkal::protocol::Skill::frame_client_self(38)) && cast(3) &&
                   read_cast(3),
               "Spin Slash cooldown rejects an immediate second area attack");
        const auto read_wear = [&](openkal::protocol::S2C opcode, std::uint32_t expected_iid) {
            if (!wait_for_frame(*session, rx, static_cast<std::uint8_t>(opcode), payload)) {
                return false;
            }
            openkal::protocol::PacketReader wear(payload.data(), payload.size());
            std::uint32_t player = 0;
            std::uint32_t iid = 0;
            return wear.read_u32(player) && wear.read_u32(iid) && iid == expected_iid;
        };
        expect(session->send_all(openkal::protocol::ItemWear::frame_client_put_on(4)) &&
                   session->send_all(openkal::protocol::ItemWear::frame_client_put_on(5)) &&
                   read_wear(openkal::protocol::S2C::PutOnItem, 5),
               "server rejects shield over two-handed weapon before accepting one-handed swap");
        expect(session->send_all(openkal::protocol::ItemWear::frame_client_put_on(4)) &&
                   read_wear(openkal::protocol::S2C::PutOnItem, 4),
               "server accepts shield with one-handed weapon");
        expect(session->send_all(openkal::protocol::ItemWear::frame_client_put_on(1)) &&
                   session->send_all(openkal::protocol::ItemWear::frame_client_put_on(5)) &&
                   read_wear(openkal::protocol::S2C::PutOnItem, 5),
               "server rejects two-handed swap while shield remains equipped");
        expect(session->send_all(openkal::protocol::ItemWear::frame_client_put_off(4)) &&
                   read_wear(openkal::protocol::S2C::PutOffItem, 4) &&
                   session->send_all(openkal::protocol::ItemWear::frame_client_put_on(1)) &&
                   read_wear(openkal::protocol::S2C::PutOnItem, 1),
               "two-handed weapon can be equipped after removing the shield");
        session->close();
    } else {
        expect(false, "connect Knight skill audit client");
    }
    host.stop();

    std::filesystem::remove(openkal::platform::join_path(exit_monsters, "0002.json"));
    std::filesystem::remove(openkal::platform::join_path(exit_monsters, "0003.json"));
    server_config.max_monsters = 1;

    expect(exit_store->open(exit_save), "reopen store for later Mage audit");
    auto mage = exit_store->load_character(exit_character_id);
    if (mage) {
        mage->clazz = 1;
        mage->level = 90;
        mage->specialty = 23;
        mage->map_x = mage->map_y = kPosition;
        mage->hp = mage->mp = 20000;
        mage->intl = 200;
        expect(exit_store->save_character(*mage), "prepare Mage channel fixture");
        for (const int index : {4, 65, 66, 67}) {
            openkal::server::SkillRecord skill;
            skill.character_id = exit_character_id;
            skill.skill_index = index;
            skill.level = 1;
            expect(exit_store->upsert_skill(skill), "learn later Mage skills");
        }
    }
    exit_store->close();
    expect(host.start(server_config), "start later Mage audit server");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session = openkal::server::connect_unix(server_config.unix_path);
    rx.clear();
    expect(session != nullptr, "connect Mage audit client");
    if (session) {
        expect(session->send_all(openkal::protocol::Handshake::build_connect(0)), "connect Mage");
        expect(wait_for_frame(
                   *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
               "receive Mage handshake");
        openkal::protocol::S2CCodeFields code;
        expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse Mage handshake");
        expect(
            session->send_all(openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
            "answer Mage handshake");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::CreateMonster),
                              payload),
               "receive Mage target");
        openkal::protocol::PacketReader reader(payload.data(), payload.size());
        std::uint16_t monster_index = 0;
        std::uint32_t monster_id = 0;
        expect(reader.read_u16(monster_index) && reader.read_u32(monster_id), "parse Mage target");
        std::this_thread::sleep_for(std::chrono::milliseconds(2200));
        const auto cast = [&](int index) {
            return session->send_all(openkal::protocol::Skill::frame_client_target(
                static_cast<std::uint8_t>(index), openkal::protocol::kCkMonster, monster_id));
        };
        const auto read_damage = [&](int index) {
            for (int attempt = 0; attempt < 12; ++attempt) {
                if (!wait_for_frame(*session,
                                    rx,
                                    static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                    payload)) {
                    return false;
                }
                openkal::protocol::S2CSkillFields hit;
                if (openkal::protocol::Skill::parse_server(payload.data(), payload.size(), hit) &&
                    hit.has_damage && hit.skill == index && hit.target == monster_id) {
                    return hit.damage > 0;
                }
            }
            return false;
        };
        expect(cast(65), "cast Flame Injection with selected target");
        const auto channel_start = std::chrono::steady_clock::now();
        for (int tick = 0; tick < 6; ++tick) {
            expect(read_damage(65), "Flame Injection delivers every positive channel pulse");
        }
        const auto channel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - channel_start)
                                    .count();
        expect(channel_ms >= 9500 && channel_ms < 13000,
               "six Flame Injection pulses span ten seconds");
        expect(session->send_all(openkal::protocol::Skill::frame_client_self(66)),
               "cast Icicle without a target");
        expect(read_damage(66), "Icicle delivers its first pulse immediately");
        expect(session->send_all(openkal::protocol::Move::frame_client_on(1, 0, 0)),
               "move to cancel Icicle");
        // A following Lightning Arrow is a sentinel: the canceled channel must not precede it.
        std::this_thread::sleep_for(std::chrono::milliseconds(2300));
        expect(cast(67), "cast Lightning Arrow after moving");
        bool canceled = true;
        bool lightning_hit = false;
        for (int attempt = 0; attempt < 12 && !lightning_hit; ++attempt) {
            if (!wait_for_frame(*session,
                                rx,
                                static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                payload)) {
                break;
            }
            openkal::protocol::S2CSkillFields hit;
            if (openkal::protocol::Skill::parse_server(payload.data(), payload.size(), hit) &&
                hit.has_damage) {
                canceled = canceled && hit.skill != 66;
                lightning_hit = hit.skill == 67 && hit.damage > 0;
            }
        }
        expect(canceled && lightning_hit,
               "movement cancels Icicle and Lightning Arrow deals its initial hit");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        expect(cast(4), "trigger Lightning Arrow's mark with lightning magic");
        expect(read_damage(67), "lightning magic triggers the marked-target splash");
        session->close();
    }
    host.stop();

    expect(exit_store->open(exit_save), "reopen store for Archer area audit");
    auto archer = exit_store->load_character(exit_character_id);
    if (archer) {
        archer->clazz = 2;
        archer->level = 90;
        archer->specialty = 23;
        archer->map_x = archer->map_y = kPosition;
        archer->hp = archer->mp = 20000;
        expect(exit_store->save_character(*archer), "prepare Archer area fixture");
        for (const int index : {47, 49}) {
            openkal::server::SkillRecord skill;
            skill.character_id = exit_character_id;
            skill.skill_index = index;
            skill.level = 1;
            expect(exit_store->upsert_skill(skill), "learn Archer area skills");
        }
    }
    exit_store->close();
    for (const bool armored : {true, false}) {
        {
            std::ofstream weapon(openkal::platform::join_path(exit_items, "0001.json"));
            weapon << R"({"index":1,"name":"Audit Bow","class":["weapon","bow"],"range":300,
            "specialty":{"hp":10000,"mp":10000}})";
            std::ofstream monster(openkal::platform::join_path(exit_monsters, "0001.json"));
            monster << R"({"index":1,"name":"Armored Audit Target",
            "stats":{"race":0,"level":90,"hp":1000000,"defense":[)"
                    << (armored ? 1000000 : 0) << "," << (armored ? 1000000 : 0)
                    << R"(],"attack":{"min":1,"max":1}},
            "spawns":[{"map":0,"area":1,"max":1,"cycle":1,"rect":[15,15,16,16]}]})";
        }
        expect(host.start(server_config), "start Archer area audit server");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        session = openkal::server::connect_unix(server_config.unix_path);
        rx.clear();
        expect(session != nullptr, "connect Archer audit client");
        if (session) {
            expect(session->send_all(openkal::protocol::Handshake::build_connect(0)),
                   "connect Archer");
            expect(
                wait_for_frame(
                    *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
                "receive Archer handshake");
            openkal::protocol::S2CCodeFields code;
            expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                   "parse Archer handshake");
            expect(session->send_all(
                       openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
                   "answer Archer handshake");
            expect(wait_for_frame(*session,
                                  rx,
                                  static_cast<std::uint8_t>(openkal::protocol::S2C::CreateMonster),
                                  payload),
                   "receive armored target");
            openkal::protocol::PacketReader reader(payload.data(), payload.size());
            std::uint16_t index = 0;
            std::uint32_t target_id = 0;
            expect(reader.read_u16(index) && reader.read_u32(target_id), "parse armored target");
            std::this_thread::sleep_for(std::chrono::milliseconds(2200));
            expect(session->send_all(
                       openkal::protocol::Skill::frame_client_ground(47, kPosition, kPosition)),
                   "cast Arrow Rain");
            bool rain_mitigated = false;
            for (int attempt = 0; attempt < 8 && !rain_mitigated; ++attempt) {
                if (!wait_for_frame(*session,
                                    rx,
                                    static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                    payload)) {
                    break;
                }
                openkal::protocol::S2CSkillFields hit;
                if (openkal::protocol::Skill::parse_server(payload.data(), payload.size(), hit) &&
                    hit.has_damage && hit.skill == 47 && hit.target == target_id) {
                    // Million-point defense leaves only a small positive remainder.
                    rain_mitigated = armored ? hit.damage > 0 && hit.damage < 100 : hit.damage > 0;
                    break;
                }
            }
            expect(rain_mitigated,
                   "Arrow Rain applies target armor while retaining positive damage");
            expect(session->send_all(openkal::protocol::Move::frame_client_on(1, 0, 0)),
                   "cancel Arrow Rain by movement");
            const bool movement_confirmed =
                wait_for_frame(*session,
                               rx,
                               static_cast<std::uint8_t>(openkal::protocol::S2C::MovePlayerOn),
                               payload);
            expect(movement_confirmed, "server confirms Arrow Rain cancellation movement");
            expect(movement_confirmed &&
                       session->send_all(openkal::protocol::Skill::frame_client_target(
                           49, openkal::protocol::kCkMonster, target_id)),
                   "cast delayed Arrow Explosion");
            bool rain_canceled = true;
            int explosion_hits = 0;
            bool explosion_mitigated = true;
            const auto explosion_deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
            while (movement_confirmed && explosion_hits < 2 &&
                   std::chrono::steady_clock::now() < explosion_deadline) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    explosion_deadline - std::chrono::steady_clock::now());
                if (!wait_for_frame(*session,
                                    rx,
                                    static_cast<std::uint8_t>(openkal::protocol::S2C::Skill),
                                    payload,
                                    nullptr,
                                    remaining)) {
                    break;
                }
                openkal::protocol::S2CSkillFields hit;
                if (openkal::protocol::Skill::parse_server(payload.data(), payload.size(), hit) &&
                    hit.has_damage) {
                    rain_canceled = rain_canceled && hit.skill != 47;
                    if (hit.skill == 49 && hit.target == target_id) {
                        // A critical initial hit legitimately exceeds the noncritical 500–750
                        // range; critical scaling is covered by the combat tests.
                        const bool critical = (hit.flags & openkal::protocol::kAtkFatal) != 0;
                        const bool initial_damage =
                            critical ? hit.damage >= 750 : hit.damage >= 500 && hit.damage <= 750;
                        const bool expected_damage =
                            armored ? (explosion_hits == 0 ? hit.damage == 1
                                                           : hit.damage > 0 && hit.damage < 100)
                                    : (explosion_hits == 0 ? initial_damage : hit.damage >= 1000);
                        explosion_mitigated = explosion_mitigated && expected_damage;
                        ++explosion_hits;
                    }
                }
            }
            expect(rain_canceled, "movement ends Arrow Rain before Arrow Explosion damage");
            expect(explosion_hits == 2, "Arrow Explosion has initial and delayed hits");
            expect(explosion_mitigated,
                   "Arrow Explosion applies the expected armor mitigation and damage ranges");
            session->close();
        }
        host.stop();
    }

    {
        std::ofstream egg(openkal::platform::join_path(exit_items, "0006.json"));
        egg << R"({"index":6,"name":"Regression Tiger Egg","wear":1,
            "class":["transform","cocoon"],"extra":{"race":0}})";
    }
    expect(exit_store->open(exit_save), "reopen store for transform egg swap regression");
    auto transform_character = exit_store->load_character(exit_character_id);
    if (transform_character) {
        transform_character->clazz = 0;
        transform_character->level = 70;
        transform_character->map_x = transform_character->map_y = kPosition;
        transform_character->hp = transform_character->mp = 20000;
        expect(exit_store->save_character(*transform_character),
               "prepare transform egg swap character");

        openkal::server::ItemRecord veteran_egg;
        veteran_egg.iid = 600001;
        veteran_egg.character_id = exit_character_id;
        veteran_egg.index = 6;
        veteran_egg.count = 1;
        veteran_egg.slot = 3;
        veteran_egg.transform_level = 39;
        veteran_egg.transform_exp = 123456;
        expect(exit_store->insert_item(veteran_egg), "save worn veteran transform egg");

        openkal::server::ItemRecord fresh_egg = veteran_egg;
        fresh_egg.iid = 600002;
        fresh_egg.slot = 0;
        fresh_egg.transform_level = 1;
        fresh_egg.transform_exp = 0;
        expect(exit_store->insert_item(fresh_egg), "save fresh transform egg");
    }
    exit_store->close();

    expect(host.start(server_config), "start transform egg swap regression server");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session = openkal::server::connect_unix(server_config.unix_path);
    rx.clear();
    expect(session != nullptr, "connect transform egg swap regression client");
    if (session) {
        expect(session->send_all(openkal::protocol::Handshake::build_connect(0)),
               "connect transform egg swap regression client");
        expect(wait_for_frame(
                   *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
               "receive transform egg swap handshake");
        openkal::protocol::S2CCodeFields code;
        expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse transform egg swap handshake");
        expect(
            session->send_all(openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
            "answer transform egg swap handshake");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::CreateMonster),
                              payload),
               "enter transform egg swap world");
        expect(session->send_all(openkal::protocol::ItemWear::frame_client_put_on(600002)),
               "replace veteran transform egg with a fresh copy");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::PutOnItem),
                              payload),
               "receive fresh transform egg equip confirmation");
        openkal::protocol::PacketReader wear(payload.data(), payload.size());
        std::uint32_t player_id = 0;
        std::uint32_t iid = 0;
        expect(wear.read_u32(player_id) && wear.read_u32(iid) && iid == 600002,
               "server equips the selected fresh transform egg instance");
        session->close();
    }
    host.stop();

    expect(exit_store->open(exit_save), "reopen store after transform egg swap regression");
    const auto transform_items = exit_store->load_items(exit_character_id);
    const auto veteran_egg = std::find_if(transform_items.begin(),
                                          transform_items.end(),
                                          [](const auto& item) { return item.iid == 600001; });
    const auto fresh_egg = std::find_if(transform_items.begin(),
                                        transform_items.end(),
                                        [](const auto& item) { return item.iid == 600002; });
    expect(veteran_egg != transform_items.end() && veteran_egg->slot == 0 &&
               veteran_egg->transform_level == 39 && veteran_egg->transform_exp == 123456,
           "transform egg swap preserves the veteran egg progress");
    expect(fresh_egg != transform_items.end() && fresh_egg->slot == 3 &&
               fresh_egg->transform_level == 1 && fresh_egg->transform_exp == 0,
           "transform egg swap does not copy veteran progress to the fresh egg");
    exit_store->close();

    expect(exit_store->open(exit_save), "reopen store for mail notification regression");
    auto mail_character = exit_store->load_character(exit_character_id);
    expect(mail_character.has_value(), "load mail notification character");
    if (mail_character) {
        mail_character->level = 15;
        mail_character->exp = 0;
        expect(exit_store->save_character(*mail_character), "prepare mail reward level");
    }
    exit_store->close();

    server_config.level_mail_rewards_path =
        openkal::platform::join_path(exit_root, "mail-rewards.json");
    {
        std::ofstream rewards(server_config.level_mail_rewards_path);
        rewards
            << R"({"schema":"openkal.level_mail_rewards/v1","rewards":[{"level":16,"itemId":1,"count":2}]})";
        expect(rewards.good(), "write live mail reward fixture");
    }

    // Deliver mail on a live connection without ever requesting the inbox.
    expect(host.start(server_config), "start mail notification server");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session = openkal::server::connect_unix(server_config.unix_path);
    expect(session != nullptr, "connect mail notification client");
    rx.clear();
    if (session) {
        expect(session->send_all(openkal::protocol::Handshake::build_connect(0)),
               "send mail notification connect");
        expect(wait_for_frame(
                   *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Code), payload),
               "receive mail notification handshake code");
        openkal::protocol::S2CCodeFields code;
        expect(openkal::protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse mail notification handshake code");
        expect(
            session->send_all(openkal::protocol::Handshake::build_answer(code.protocol_test_value)),
            "send mail notification handshake answer");
        std::vector<openkal::protocol::MailEntry> inbox;
        expect(
            wait_for_frame(
                *session, rx, static_cast<std::uint8_t>(openkal::protocol::S2C::Mail), payload) &&
                openkal::protocol::Mail::parse_list_s2c(payload.data(), payload.size(), inbox) &&
                inbox.empty(),
            "login pushes the initial empty inbox without opening mail");
        expect(session->send_all(openkal::protocol::Chatting::frame_c2s("/level")),
               "trigger a level reward while the inbox stays closed");
        expect(wait_for_frame(*session,
                              rx,
                              static_cast<std::uint8_t>(openkal::protocol::S2C::Mail),
                              payload,
                              nullptr,
                              std::chrono::seconds(2)) &&
                   openkal::protocol::Mail::parse_list_s2c(payload.data(), payload.size(), inbox) &&
                   inbox.size() == 1 && !inbox.front().read && !inbox.front().claimed &&
                   inbox.front().item_index == 1 && inbox.front().item_count == 2,
               "new reward pushes the unread inbox immediately without a mail-list request");
        session->close();
    }
    host.stop();

    return openkal::test::g_failures == 0 ? 0 : 1;
}
