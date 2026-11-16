/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Path.h"
#include "openkal/server/LevelMailRewards.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <filesystem>
#include <sqlite3.h>
#include <string>
#include <unistd.h>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main() {
    using openkal::server::LevelMailRewardCatalog;
    using openkal::test::expect;

    LevelMailRewardCatalog catalog;
    const std::string repo = OPENKAL_REPO_ROOT;
    expect(catalog.load_file(openkal::platform::join_path(repo, "config/level_mail_rewards.json")),
           "load level-mail reward configuration");
    const auto& configured_rows = catalog.rewards();
    for (const auto& row : configured_rows) {
        const auto configured = catalog.rewards_for_level(row.level);
        expect(configured.size() == 1 && configured.front().item_id == row.item_id &&
                   configured.front().count == row.count,
               "read each configured level's item and quantity");
    }
    expect(
        !catalog.load_json_text(
            R"({"schema":"openkal.level_mail_rewards/v1","rewards":[{"level":10,"itemId":237,"count":1},{"level":10,"itemId":238,"count":1}]})"),
        "reject duplicate level rows");

    openkal::server::SqliteStore store;
    const auto path = std::filesystem::temp_directory_path() /
                      ("openkal-mail-store-" + std::to_string(getpid()) + ".sqlite");
    std::filesystem::remove(path);
    expect(store.open(path.string()), "open system-mail persistence database");
    openkal::server::AccountRecord account;
    account.id = 1;
    account.name = "mail-test";
    expect(store.upsert_account(account), "create system-mail account");
    openkal::server::CharacterRecord character;
    character.id = 1;
    character.account_id = 1;
    character.name = "MailTester";
    expect(store.save_character(character), "create system-mail character");
    openkal::server::SystemMailRecord mail;
    mail.character_id = 1;
    mail.source_key = "level-reward:10";
    mail.subject = "Level Up Reward";
    mail.body = "Congratulations";
    mail.item_index = 237;
    mail.item_count = 3;
    mail.created_at = 1710000000;
    expect(store.ensure_system_mail(mail), "persist level reward mail");
    const auto inbox = store.list_system_mail(1);
    expect(inbox.size() == 1 && inbox.front().claimed_at == 0 && inbox.front().item_count == 3,
           "load unclaimed level reward mail");
    store.close();
    sqlite3* legacy = nullptr;
    expect(sqlite3_open(path.c_str(), &legacy) == SQLITE_OK, "open legacy mail fixture");
    if (legacy) {
        expect(sqlite3_exec(legacy,
                            "ALTER TABLE system_mail DROP COLUMN read_at;"
                            "ALTER TABLE system_mail DROP COLUMN deleted_at;"
                            "DELETE FROM schema_version WHERE version = 24;",
                            nullptr,
                            nullptr,
                            nullptr) == SQLITE_OK,
               "prepare existing pre-read-state inbox");
        sqlite3_close(legacy);
    }
    expect(store.open(path.string()) && store.list_system_mail(1).size() == 1 &&
               store.list_system_mail(1).front().read_at == 0,
           "legacy inbox migrates without losing rewards");
    expect(!store.update_system_mail(2, inbox.front().id, false, 1710000001),
           "another character cannot mark mail read");
    expect(!store.update_system_mail(1, inbox.front().id, true, 1710000001),
           "unclaimed attachment cannot be deleted");
    expect(store.update_system_mail(1, inbox.front().id, false, 1710000002),
           "mark owned message read");
    expect(store.update_system_mail(1, inbox.front().id, false, 1710000003) &&
               store.list_system_mail(1).front().read_at == 1710000002,
           "rereading preserves first read timestamp");
    store.close();
    expect(store.open(path.string()) && store.list_system_mail(1).front().read_at == 1710000002,
           "read state survives reopening and migration is repeatable");
    openkal::server::ItemRecord item;
    item.iid = 10;
    item.character_id = 1;
    expect(store.claim_system_mail(1, inbox.front().id, item, 1710000010) ==
               openkal::server::SystemMailClaimResult::Claimed,
           "claim level reward atomically");
    expect(store.claim_system_mail(1, inbox.front().id, item, 1710000011) ==
               openkal::server::SystemMailClaimResult::AlreadyClaimed,
           "reject a second claim for the same level reward");
    const auto items = store.load_items(1);
    expect(items.size() == 1 && items.front().iid == 10 && items.front().index == 237 &&
               items.front().count == 3,
           "claim creates the configured item attachment once");
    expect(!store.update_system_mail(2, inbox.front().id, true, 1710000020),
           "another character cannot delete claimed mail");
    expect(store.update_system_mail(1, inbox.front().id, true, 1710000020) &&
               store.list_system_mail(1).empty(),
           "claimed message can be deleted");
    expect(store.ensure_system_mail(mail) && store.list_system_mail(1).empty(),
           "deleted level reward is never issued again");
    expect(store.claim_system_mail(1, inbox.front().id, item, 1710000021) ==
               openkal::server::SystemMailClaimResult::NotFound,
           "deleted mail cannot be claimed through an old ID");
    store.close();
    expect(store.open(path.string()) && store.list_system_mail(1).empty(),
           "deletion survives reopening");
    store.close();
    std::filesystem::remove(path);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
