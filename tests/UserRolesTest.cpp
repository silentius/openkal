/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/UserRoles.h"

#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include <unistd.h>

int main() {
    using namespace openkal;
    using server::UserRole;
    using test::expect;
    const auto root =
        std::filesystem::temp_directory_path() / ("openkal-user-roles-" + std::to_string(getpid()));
    std::filesystem::create_directories(root);
    const auto path = (root / "user_roles.json").string();
    const auto write = [&](const std::string& json) { std::ofstream(path) << json; };
    server::UserRoles roles;
    std::string error;
    expect(!roles.allows(UserRole::Admin, "getitemid"), "unconfigured roles have no grants");
    expect(
        roles.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/server/user_roles.json", error),
        "load shipped server role policy");
    expect(!roles.allows(UserRole::Player, "getitemid") &&
               !roles.allows(UserRole::Moderator, "getitemid") &&
               !roles.allows(UserRole::CoAdmin, "getitemid") &&
               roles.allows(UserRole::Admin, "getitemid"),
           "shipped policy grants getitemid only to admin");
    expect(!roles.allows(UserRole::Admin, "level") &&
               !roles.allows(static_cast<UserRole>(4), "getitemid") &&
               !roles.allows(static_cast<UserRole>(-1), "getitemid"),
           "other commands and invalid roles remain denied");
    for (
        const auto* json :
        {R"({"schema":"openkal.user_roles/v1","roles":{"0":{"commands":["getitemid"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"4":{"commands":["getitemid"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"commands":["*"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"commands":["/getitemid"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"commands":["getitemid","getitemid"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"commands":[true]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"command":["getitemid"]}}})",
         R"({"schema":"openkal.user_roles/v1","roles":{"3":{"commands":["getitemid"]}},"extra":true})",
         R"({"schema":"wrong","roles":{}})",
         "{"}) {
        write(json);
        expect(!roles.load_file(path, error) && !error.empty() &&
                   !roles.allows(UserRole::Admin, "getitemid"),
               "invalid policies clear all grants");
    }
    write(
        R"({"schema":"openkal.user_roles/v1","roles":{"1":{"commands":["time"]},"3":{"commands":[]}}})");
    expect(roles.load_file(path, error) && error.empty() &&
               roles.allows(UserRole::Moderator, "time") && !roles.allows(UserRole::Admin, "time"),
           "roles use independent explicit allowlists");
    expect(!roles.load_file(path + ".missing", error) && !roles.allows(UserRole::Moderator, "time"),
           "missing policy clears privileges");
    const auto database = (root / "roles.sqlite").string();
    server::SqliteStore store;
    if (!store.open(database)) {
        return 1;
    }
    expect(store.upsert_account({1, "Roles", "owned"}), "seed role account");
    server::CharacterRecord character;
    character.id = 1;
    character.account_id = 1;
    character.name = "RoleHero";
    character.role = UserRole::Admin;
    character.money = 1234;
    character.clazz = 2;
    character.map_x = 777;
    expect(store.save_character(character), "create character with server-owned admin role");
    auto loaded = store.load_character(character.id);
    expect(loaded && loaded->role == UserRole::Admin && loaded->clazz == 2 &&
               loaded->map_x == 777 && loaded->money == 1234,
           "role and adjacent persisted character fields reload exactly");
    character.role = UserRole::Player;
    character.money = 1235;
    expect(store.save_character(character) && store.load_character(1)->role == UserRole::Admin &&
               store.load_character(1)->money == 1235,
           "ordinary gameplay save preserves database role");
    character.id = 2;
    character.name = "DefaultPlayer";
    expect(store.save_character(character) && store.load_character(2)->role == UserRole::Player,
           "new characters default to player");
    sqlite3* db = nullptr;
    expect(sqlite3_open(database.c_str(), &db) == SQLITE_OK, "inspect character role schema");
    sqlite3_stmt* statement = nullptr;
    expect(sqlite3_prepare_v2(db,
                              "SELECT cid FROM pragma_table_info('characters') WHERE name='role'",
                              -1,
                              &statement,
                              nullptr) == SQLITE_OK &&
               sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 3,
           "new schema places role immediately after name");
    sqlite3_finalize(statement);
    expect(sqlite3_exec(db, "UPDATE characters SET role=4 WHERE id=1", nullptr, nullptr, nullptr) !=
               SQLITE_OK,
           "database rejects invalid roles");
    store.close();
    expect(sqlite3_exec(db,
                        "ALTER TABLE characters DROP COLUMN role; "
                        "ALTER TABLE accounts DROP COLUMN protocol_points; "
                        "ALTER TABLE accounts DROP COLUMN protocol_kicks; "
                        "ALTER TABLE accounts DROP COLUMN banned; "
                        "DELETE FROM schema_version WHERE version IN (29,30)",
                        nullptr,
                        nullptr,
                        nullptr) == SQLITE_OK,
           "prepare legacy database without roles or protocol discipline");
    sqlite3_close(db);
    expect(store.open(database) && store.load_character(1)->role == UserRole::Player &&
               store.load_character(1)->money == 1235 && store.load_character(1)->map_x == 777,
           "migration initializes existing characters as players without changing gameplay data");
    const auto discipline = store.load_account_discipline(1);
    expect(discipline && discipline->points == 0 && discipline->kicks == 0 && !discipline->banned,
           "migration initializes existing accounts without sanctions");
    using server::AccountDisciplineChange;
    const auto point = store.update_account_discipline(1, AccountDisciplineChange::Violation);
    expect(point && point->points == 1, "persist a protocol violation");
    const auto decay = store.update_account_discipline(1, AccountDisciplineChange::Decay);
    expect(decay && decay->points == 0 && decay->kicks == 0,
           "decay removes a point without changing kick history");
    for (int violation = 0; violation < 30; ++violation) {
        expect(store.update_account_discipline(1, AccountDisciplineChange::Violation).has_value(),
               "commit an account violation");
    }
    expect(store.upsert_account({1, "Roles", "updated"}), "update credentials after enforcement");
    store.close();
    expect(store.open(database) && store.load_character(1)->role == UserRole::Player,
           "role migration is restart safe");
    const auto banned = store.load_account_discipline(1);
    expect(banned && banned->points == 0 && banned->kicks == 3 && banned->banned &&
               !store.update_account_discipline(1, AccountDisciplineChange::Violation),
           "kick history and ban survive restart and credential updates");
    store.close();
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
