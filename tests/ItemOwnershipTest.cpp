/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <cstdio>
#include <filesystem>
#include <sqlite3.h>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;

    bool execute(sqlite3* db, const char* sql) {
        char* error = nullptr;
        const bool ok = sqlite3_exec(db, sql, nullptr, nullptr, &error) == SQLITE_OK;
        if (!ok) {
            std::fprintf(stderr, "%s\n", error ? error : sqlite3_errmsg(db));
        }
        sqlite3_free(error);
        return ok;
    }

    std::int64_t scalar(sqlite3* db, const char* sql) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
            return INT64_MIN;
        }
        const auto value =
            sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : INT64_MIN;
        sqlite3_finalize(statement);
        return value;
    }
}  // namespace

int main() {
    using namespace openkal;
    using test::expect;
    const auto path = (std::filesystem::temp_directory_path() /
                       ("openkal-item-owner-" + std::to_string(getpid()) + ".sqlite"))
                          .string();
    std::filesystem::remove(path);
    server::SqliteStore store;
    if (!store.open(path)) {
        return 1;
    }
    expect(store.upsert_account({101, "OwnerOne", "owned"}) &&
               store.upsert_account({102, "OwnerTwo", "owned"}),
           "seed separate accounts");
    expect(!store.find_account("!openkal-trading-post"), "system escrow account cannot log in");
    server::CharacterRecord first, second, sibling;
    first.id = 201;
    first.account_id = 101;
    first.name = "FirstOwner";
    first.money = 1000;
    second = first;
    second.id = 202;
    second.account_id = 102;
    second.name = "SecondOwner";
    sibling = first;
    sibling.id = 203;
    sibling.name = "SameAccount";
    expect(store.save_character(first) && store.save_character(second) &&
               store.save_character(sibling),
           "seed character ownership");
    server::ItemRecord item;
    item.iid = 301;
    item.character_id = first.id;
    item.index = 1;
    item.count = 8;
    item.x_attack = 17;
    item.enchant_protected = true;
    expect(store.insert_item(item), "infer account for newly created inventory item");
    auto rows = store.load_items(first.id);
    if (rows.empty()) {
        return 1;
    }
    item = rows.front();
    expect(item.account_id == first.account_id, "inventory item persists owning account");
    auto other = item;
    other.iid = 302;
    other.character_id = sibling.id;
    other.account_id = 102;
    expect(store.insert_item(other) && store.load_items(sibling.id).front().account_id == 101,
           "character account is authoritative and same-account bag slots may repeat");
    auto stored = item;
    stored.iid = 303;
    stored.character_id = 0;
    stored.slot = 0;
    expect(store.insert_item(stored) && store.load_storage_items(101).size() == 1,
           "storage shares account ownership while remaining separate from inventory");
    stored.iid = 304;
    expect(!store.insert_item(stored), "storage slots remain unique within an account");
    stored = store.load_storage_items(101).front();
    stored.character_id = sibling.id;
    stored.account_id = 0;
    expect(store.update_item(stored) && store.load_storage_items(101).empty(),
           "withdrawal retains account even for callers omitting it");
    expect(store.commit_trade(first, second, {{item, 3, 305}}),
           "split trade transfers across accounts");
    rows = store.load_items(first.id);
    expect(rows.size() == 1 && rows.front().account_id == 101 && rows.front().count == 5,
           "partial trade keeps existing account on remainder");
    rows = store.load_items(second.id);
    expect(rows.size() == 1 && rows.front().account_id == 102 && rows.front().count == 3,
           "partial trade assigns recipient account to split");
    expect(store.commit_trade(first, second, {{rows.front(), 3, 0}}),
           "full trade accepts inventory records with account IDs");
    rows = store.load_items(first.id);
    expect(rows.size() == 2 && rows.back().account_id == 101,
           "full trade updates account together with character");

    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        return 1;
    }
    item = rows.front();
    expect(!store.trading_post_sell(item, 4, 100, {}, 305).committed &&
               scalar(db, "SELECT count FROM items WHERE iid=301") == 5,
           "split cannot overwrite an existing IID or remove source quantity");
    expect(store.trading_post_sell(item, 4, 100, {}, 401).committed,
           "list account-owned inventory");
    expect(
        scalar(db,
               "SELECT i.account_id FROM trading_post_offers o JOIN items i ON i.iid=o.item_iid") ==
            server::kTradingPostAccountId,
        "listed quantity belongs to trading post account");
    expect(store.load_items(first.id).front().account_id == 101,
           "listing leaves remaining stack with seller account");
    protocol::TradingPostRequest request;
    auto offers = store.trading_post_list(second.id, request);
    if (!offers || offers->entries.empty()) {
        return 1;
    }
    const auto listing = offers->entries.front().id;
    expect(!store.trading_post_buy(second, listing, 2, 100, 305).committed &&
               store.load_character(second.id)->money == second.money &&
               scalar(db, "SELECT count FROM items WHERE iid=401") == 4,
           "failed split purchase preserves wallet and offered stack");
    expect(store.trading_post_buy(second, listing, 2, 100, 402).committed,
           "purchase transfers escrow quantity");
    second.money -= 200;
    expect(scalar(db,
                  "SELECT i.account_id FROM trading_post_deliveries d JOIN items i ON "
                  "i.iid=d.item_iid WHERE d.kind=0") == 102,
           "purchase receipt already belongs to buyer account before collection");
    expect(store.load_storage_items(102).empty(),
           "pending purchases never appear in account storage");
    expect(scalar(db, "SELECT count FROM items WHERE iid=402") == 2,
           "only partial stacks create a new IID");
    expect(
        scalar(db,
               "SELECT i.account_id FROM trading_post_offers o JOIN items i ON i.iid=o.item_iid") ==
            -1,
        "unsold quantity remains owned by trading post");
    expect(store.trading_post_cancel(first.id, listing).committed,
           "withdraw unsold quantity into collection");
    expect(scalar(db,
                  "SELECT i.account_id FROM trading_post_deliveries d JOIN items i ON "
                  "i.iid=d.item_iid WHERE d.kind=2") == 101,
           "returned quantity belongs to seller account");
    request.view = protocol::TradingPostView::Collection;
    auto receipts = store.trading_post_list(second.id, request);
    if (!receipts || receipts->entries.empty()) {
        return 1;
    }
    auto stale_owner = second;
    stale_owner.account_id = 101;
    expect(store.trading_post_collect(stale_owner, receipts->entries.front().id).committed &&
               store.load_items(second.id).front().account_id == 102,
           "collection uses persisted recipient account rather than stale caller account");
    receipts = store.trading_post_list(first.id, request);
    expect(receipts && store.trading_post_collect(first, receipts->entries.front().id).committed,
           "collect returned quantity into existing account");
    expect(scalar(db, "SELECT account_id FROM items WHERE iid=401") == 101,
           "returned inventory item keeps existing account and listing IID");

    auto whole = item;
    whole.iid = 600;
    whole.count = 1;
    server::BattleMountRecord whole_mount;
    whole_mount.mount_iid = whole.iid;
    whole_mount.satiety = 61;
    expect(store.insert_item(whole) && store.upsert_battle_mount(whole_mount),
           "seed stable full-item IID with dependent mount state");
    expect(
        execute(db,
                "CREATE TRIGGER preserve_original_delete BEFORE DELETE ON items WHEN OLD.iid=600 "
                "BEGIN SELECT RAISE(ABORT,'reference item must remain'); END;"
                "CREATE TRIGGER preserve_original_insert BEFORE INSERT ON items WHEN NEW.iid=600 "
                "BEGIN SELECT RAISE(ABORT,'reference item must not be recreated'); END;"),
        "reject replacement of the existing item row");
    expect(store.trading_post_sell(whole, 1, 25, {}).committed &&
               scalar(db, "SELECT account_id FROM items WHERE iid=600") == -1,
           "whole listing retains existing row and IID under system ownership");
    request.view = protocol::TradingPostView::Buy;
    offers = store.trading_post_list(second.id, request);
    expect(offers && store.trading_post_buy(second, offers->entries.front().id, 1, 25).committed &&
               scalar(db, "SELECT account_id FROM items WHERE iid=600") == 102,
           "whole sale changes account without replacing the item row");
    second.money -= 25;
    request.view = protocol::TradingPostView::Collection;
    receipts = store.trading_post_list(second.id, request);
    const auto whole_pickup = store.trading_post_collect(second, receipts->entries.front().id);
    expect(whole_pickup.committed && whole_pickup.item && whole_pickup.item->iid == 600 &&
               store.load_battle_mount(600)->satiety == 61,
           "collection preserves existing item row, IID and mount state without inserts");

    // Prepare persisted inventory, storage, mount and market state in the previous schema.
    stored = item;
    stored.iid = 308;
    stored.character_id = 0;
    stored.slot = 215;
    expect(store.insert_item(stored), "seed storage for migration");
    server::BattleMountRecord mount;
    mount.mount_iid = 305;
    mount.satiety = 73;
    expect(store.upsert_battle_mount(mount), "seed dependent mount state for migration");
    item = store.load_items(first.id).front();
    expect(store.trading_post_sell(item, 1, 100, {}).committed, "seed legacy active offer");
    store.close();
    expect(execute(db, R"SQL(
PRAGMA foreign_keys=OFF;
BEGIN;
CREATE TABLE items_legacy (
  iid INTEGER PRIMARY KEY,
  character_id INTEGER,
  account_id INTEGER,
  item_index INTEGER NOT NULL,
  prefix INTEGER NOT NULL DEFAULT 0,
  count INTEGER NOT NULL DEFAULT 1,
  slot INTEGER NOT NULL DEFAULT 0,
  cur_end INTEGER NOT NULL DEFAULT 0,
  max_end INTEGER NOT NULL DEFAULT 0,
  x_attack INTEGER NOT NULL DEFAULT 0,
  x_magic INTEGER NOT NULL DEFAULT 0,
  x_hit INTEGER NOT NULL DEFAULT 0,
  x_defense INTEGER NOT NULL DEFAULT 0,
  x_dodge INTEGER NOT NULL DEFAULT 0,
  upgrade_level INTEGER NOT NULL DEFAULT 0,
  upgrade_rate INTEGER NOT NULL DEFAULT 0,
  mix_type INTEGER NOT NULL DEFAULT 0,
  bof INTEGER NOT NULL DEFAULT 0,
  qigong_perforated INTEGER NOT NULL DEFAULT 0,
  qigong_effect INTEGER NOT NULL DEFAULT 0,
  dragon_effect INTEGER NOT NULL DEFAULT 0,
  random_stat_type INTEGER NOT NULL DEFAULT 0,
  random_stat_value INTEGER NOT NULL DEFAULT 0,
  transform_level INTEGER NOT NULL DEFAULT 1,
  transform_exp INTEGER NOT NULL DEFAULT 0,
  pet_expire_at INTEGER NOT NULL DEFAULT 0,
  ride_expire_at INTEGER NOT NULL DEFAULT 0,
  account_bound INTEGER NOT NULL DEFAULT 0 CHECK(account_bound IN (0, 1)),
  enchant_protected INTEGER NOT NULL DEFAULT 0,
  character_bound INTEGER NOT NULL DEFAULT 0,
  CHECK ((character_id IS NOT NULL AND account_id IS NULL) OR
         (character_id IS NULL AND account_id IS NOT NULL)),
  CHECK (account_id IS NULL OR slot BETWEEN 0 AND 215),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);
INSERT INTO items_legacy(iid,character_id,account_id,item_index,prefix,count,slot,cur_end,max_end,x_attack,x_magic,x_hit,x_defense,x_dodge,upgrade_level,upgrade_rate,mix_type,bof,qigong_perforated,qigong_effect,dragon_effect,random_stat_type,random_stat_value,transform_level,transform_exp,pet_expire_at,ride_expire_at,account_bound,enchant_protected,character_bound)
SELECT iid,character_id,CASE WHEN character_id IS NULL THEN account_id ELSE NULL END,item_index,prefix,count,slot,cur_end,max_end,x_attack,x_magic,x_hit,x_defense,x_dodge,upgrade_level,upgrade_rate,mix_type,bof,qigong_perforated,qigong_effect,dragon_effect,random_stat_type,random_stat_value,transform_level,transform_exp,pet_expire_at,ride_expire_at,account_bound,enchant_protected,character_bound FROM items WHERE location<>2;
DROP TABLE items;
ALTER TABLE items_legacy RENAME TO items;
CREATE UNIQUE INDEX ux_items_account_slot ON items(account_id,slot) WHERE account_id IS NOT NULL;
DROP TABLE trading_post_offers;
DROP TABLE trading_post_deliveries;
CREATE TABLE IF NOT EXISTS trading_post_offers(id INTEGER PRIMARY KEY AUTOINCREMENT, seller BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>=0),unit_price BIGINT NOT NULL CHECK(unit_price>0),attributes TEXT NOT NULL,clazz BIGINT NOT NULL,type BIGINT NOT NULL,grade BIGINT NOT NULL,prefix BIGINT NOT NULL,enhancement BIGINT NOT NULL,mount TEXT NOT NULL DEFAULT '');
CREATE TABLE IF NOT EXISTS trading_post_deliveries(id INTEGER PRIMARY KEY AUTOINCREMENT,character_id BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>0),unit_price BIGINT NOT NULL CHECK(unit_price>0),kind BIGINT NOT NULL,attributes TEXT NOT NULL,mount TEXT NOT NULL DEFAULT '');
INSERT INTO trading_post_offers(seller,item_index,count,unit_price,attributes,clazz,type,grade,prefix,enhancement)
 VALUES(201,1,2,100,'["0","0","0","17","0","0","0","0","0","0","0","0","0","0","0","0","0","1","0","1","0","0"]',-1,0,0,0,0);
INSERT INTO trading_post_deliveries(character_id,item_index,count,unit_price,kind,attributes)
 VALUES(202,1,3,100,0,'["0","0","0","17","0","0","0","0","0","0","0","0","0","0","0","0","0","1","0","1","0","0"]'),(201,1,2,100,1,'["0","0","0","17","0","0","0","0","0","0","0","0","0","0","0","0","0","1","0","1","0","0"]');
DELETE FROM schema_version WHERE version>=28;
COMMIT;
)SQL"),
           "restore legacy v27 ownership constraints and missing market account IDs");
    sqlite3_close(db);
    expect(store.open(path), "migrate existing ownership data");
    rows = store.load_items(first.id);
    expect(!rows.empty() && rows.front().account_id == 101 && rows.front().x_attack == 17 &&
               rows.front().enchant_protected,
           "migration backfills inventory accounts losslessly");
    expect(store.load_storage_items(101).size() == 1 &&
               store.load_storage_items(101).front().slot == 215,
           "migration preserves account storage");
    const auto migrated_mount = store.load_battle_mount(305);
    expect(migrated_mount && migrated_mount->satiety == 73,
           "item table rebuild preserves dependent Battle Riding state");
    expect(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "inspect migrated market data");
    expect(scalar(db,
                  "SELECT COUNT(*) FROM trading_post_offers o JOIN items i ON i.iid=o.item_iid "
                  "WHERE i.account_id<>-1") == 0 &&
               scalar(db,
                      "SELECT COUNT(*) FROM trading_post_deliveries d JOIN items i ON "
                      "i.iid=d.item_iid WHERE i.account_id<>102") == 0,
           "migration assigns system and recipient accounts to existing market snapshots");
    expect(scalar(db, "SELECT COUNT(*) FROM items WHERE account_id IS NULL") == 0,
           "all migrated items retain an account");
    sqlite3_close(db);
    store.close();
    expect(store.open(path) && store.load_battle_mount(305).has_value(),
           "migration is restart safe and idempotent");
    expect(store.schedule_character_deletion(first.id, first.account_id, 1000),
           "schedule seller cleanup with referenced market items");
    auto purged = server::CharacterPurgeResult::WorkRemaining;
    for (int n = 0; n < 30 && purged == server::CharacterPurgeResult::WorkRemaining; ++n) {
        purged =
            store.process_character_deletions(1000 + server::kCharacterDeletionGraceSeconds, 2);
    }
    expect(purged == server::CharacterPurgeResult::CharacterPurged &&
               store.load_storage_items(101).size() == 1,
           "bounded seller cleanup preserves account storage");
    request.view = protocol::TradingPostView::Collection;
    receipts = store.trading_post_list(second.id, request);
    expect(receipts && receipts->entries.size() == 1 && receipts->entries.front().item.count == 3 &&
               store.trading_post_collect(second, receipts->entries.front().id).committed,
           "seller cleanup retains purchased item rows referenced by another character");
    store.close();
    std::filesystem::remove(path);
    std::filesystem::remove(path + "-wal");
    std::filesystem::remove(path + "-shm");
    return test::g_failures ? 1 : 0;
}
