/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TradingPostContract.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/server/ItemStacking.h"

#include <atomic>
#include <filesystem>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>

using namespace openkal;
int main() {
    using test::expect;
    using namespace protocol;
    expect(trading_post_type("defense", "upperarmor", false) == TradingPostType::Chest &&
               trading_post_type("defense", "upper", false) == TradingPostType::Chest,
           "catalog chest armor and existing alias share the Chest category");
    expect(trading_post_type("defense", "lowerarmor", false) == TradingPostType::Shorts &&
               trading_post_type("defense", "pants", false) == TradingPostType::Shorts,
           "catalog lower armor and existing alias share the Shorts category");
    expect(trading_post_type("skin", "upper", false) == TradingPostType::Other &&
               trading_post_type("skin", "pants", false) == TradingPostType::Other,
           "armor skins remain outside equipment categories");
    for (const auto pickup : {TradingPostPickup{23000, 3, 0},
                              TradingPostPickup{23000, 1, 0},
                              TradingPostPickup{0, 0, 125},
                              TradingPostPickup{0, 0, kTradingPostMaxPrice}}) {
        auto receipt = frame_trading_post_pickup(pickup);
        TradingPostPickup parsed;
        expect(!receipt.empty() &&
                   parse_trading_post_pickup(receipt.data() + 3, receipt.size() - 3, parsed) &&
                   parsed.item_index == pickup.item_index && parsed.quantity == pickup.quantity &&
                   parsed.geons == pickup.geons,
               "committed pickup receipt preserves actual item quantity and Geons");
        expect(!parse_trading_post_pickup(receipt.data() + 3, receipt.size() - 4, parsed),
               "reject truncated pickup receipt");
        receipt.push_back(0);
        expect(!parse_trading_post_pickup(receipt.data() + 3, receipt.size() - 3, parsed),
               "reject trailing pickup receipt data");
    }
    for (const auto pickup : {TradingPostPickup{},
                              TradingPostPickup{1, 0, 0},
                              TradingPostPickup{0, 1, 0},
                              TradingPostPickup{-1, 1, 0},
                              TradingPostPickup{1, -1, 0},
                              TradingPostPickup{0, 0, -1},
                              TradingPostPickup{0, 0, kTradingPostMaxPrice + 1}}) {
        expect(frame_trading_post_pickup(pickup).empty(), "reject invalid pickup receipt");
    }
    server::ItemRecord stack;
    stack.index = 23000;
    stack.count = 2;
    stack.character_id = 1;
    auto incoming = stack;
    incoming.iid = 42;
    expect(server::can_stack_items(stack, incoming), "IID differences alone allow consolidation");
    for (int property = 0; property < 27; ++property) {
        auto different = incoming;
        int* fields[]{&different.index,
                      &different.prefix,
                      &different.cur_end,
                      &different.max_end,
                      &different.x_attack,
                      &different.x_magic,
                      &different.x_hit,
                      &different.x_defense,
                      &different.x_dodge,
                      &different.upgrade_level,
                      &different.upgrade_rate,
                      &different.mix_type,
                      &different.bof,
                      &different.qigong_perforated,
                      &different.qigong_effect,
                      &different.dragon_effect,
                      &different.random_stat_type,
                      &different.random_stat_value,
                      &different.transform_level,
                      &different.transform_exp};
        if (property < 20) {
            ++*fields[property];
        } else if (property == 20) {
            different.account_bound = true;
        } else if (property == 21) {
            different.character_bound = true;
        } else if (property == 22) {
            different.enchant_protected = true;
        } else if (property == 23) {
            different.pet_expire_at = 1;
        } else if (property == 24) {
            different.ride_expire_at = 1;
        } else if (property == 25) {
            different.slot = 1;
        } else {
            different.character_id = 2;
        }
        expect(!server::can_stack_items(stack, different),
               "distinct instance properties never merge");
    }
    for (const bool value : {false, true}) {
        const auto notification = frame_trading_post_collection(value);
        bool pending = !value;
        expect(notification.size() == 4 &&
                   parse_trading_post_collection(notification.data() + 3, 1, pending) &&
                   pending == value,
               "collection indicator round trip supports empty and pending states");
        expect(!parse_trading_post_collection(notification.data() + 3, 0, pending),
               "reject empty collection indicator");
    }
    const std::uint8_t invalid_indicator[] = {2, 0, 0};
    bool pending = false;
    expect(!parse_trading_post_collection(invalid_indicator, 1, pending) &&
               !parse_trading_post_collection(invalid_indicator + 1, 2, pending),
           "reject invalid collection flag and trailing bytes");
    TradingPostRequest request;
    request.operation = 42;
    request.command = TradingPostCommand::Buy;
    request.listing = 900;
    request.quantity = 100;
    request.unit_price = 123456;
    auto frame = frame_trading_post_request(request);
    TradingPostRequest parsed;
    expect(!frame.empty() &&
               parse_trading_post_request(frame.data() + 3, frame.size() - 3, parsed) &&
               parsed.quantity == 100 && parsed.unit_price == 123456 && parsed.operation == 42,
           "round trip trading post command");
    expect(!parse_trading_post_request(frame.data() + 3, frame.size() - 4, parsed),
           "reject truncated market command");
    frame.push_back(0);
    expect(!parse_trading_post_request(frame.data() + 3, frame.size() - 3, parsed),
           "reject trailing command data");
    request.quantity = -1;
    expect(frame_trading_post_request(request).empty(), "reject negative quantity");
    TradingPostRequest collect_request;
    collect_request.command = TradingPostCommand::Collect;
    collect_request.view = TradingPostView::Collection;
    collect_request.operation = 43;
    collect_request.listing = 900;
    expect(frame_trading_post_request(collect_request).empty(),
           "collection requires a runtime NPC");
    collect_request.npc_id = 123;
    frame = frame_trading_post_request(collect_request);
    expect(!frame.empty() &&
               parse_trading_post_request(frame.data() + 3, frame.size() - 3, parsed) &&
               parsed.command == TradingPostCommand::Collect && parsed.npc_id == 123 &&
               parsed.listing == 900,
           "collection command round trip preserves the selected receipt and NPC");
    collect_request.command = TradingPostCommand::CollectAll;
    collect_request.listing = 0;
    frame = frame_trading_post_request(collect_request);
    expect(!frame.empty() &&
               parse_trading_post_request(frame.data() + 3, frame.size() - 3, parsed) &&
               parsed.command == TradingPostCommand::CollectAll && parsed.npc_id == 123 &&
               parsed.listing == 0,
           "collect all needs no selected receipt and preserves the collector context");
    collect_request.npc_id = 0;
    expect(frame_trading_post_request(collect_request).empty(), "collect all requires an NPC");
    collect_request.npc_id = 123;
    collect_request.view = TradingPostView::Buy;
    expect(frame_trading_post_request(collect_request).empty(),
           "collect all is restricted to Collection");
    TradingPostState state;
    state.acknowledged = 42;
    state.page = 1;
    state.total_pages = 207;
    state.status = std::string(256, 's');
    for (unsigned n = 0; n < kTradingPostPageSize; ++n) {
        TradingPostEntry entry;
        entry.id = n + 1;
        entry.item.index = 1;
        entry.item.count = INT32_MAX;
        entry.item.attributes.fill(INT32_MAX);
        entry.item.pet_expire_at = UINT64_MAX;
        entry.unit_price = kTradingPostMaxPrice;
        state.entries.push_back(entry);
    }
    frame = frame_trading_post_state(state);
    TradingPostState received;
    expect(!frame.empty() && frame.size() < 8000 &&
               parse_trading_post_state(frame.data() + 3, frame.size() - 3, received) &&
               received.entries.size() == 60 && !received.entries[0].item.iid &&
               received.page == 1 && received.total_pages == 207,
           "full page preserves every attribute without revealing source IID");
    expect(!parse_trading_post_state(frame.data() + 3, frame.size() - 4, received),
           "reject truncated listing page");
    auto legacy = frame;
    legacy[3] = 1;
    legacy.erase(legacy.begin() + 17, legacy.begin() + 21);
    expect(parse_trading_post_state(legacy.data() + 3, legacy.size() - 3, received) &&
               received.page == 1 && received.total_pages == 0,
           "legacy pages retain an explicitly unknown total");
    auto invalid_total = frame;
    std::fill(invalid_total.begin() + 17, invalid_total.begin() + 21, 0);
    invalid_total[17] = 1;
    expect(!parse_trading_post_state(invalid_total.data() + 3, invalid_total.size() - 3, received),
           "reject a versioned page beyond its total");
    state.total_pages = 0;
    state.entries.clear();
    frame = frame_trading_post_state(state);
    expect(!frame.empty() &&
               parse_trading_post_state(frame.data() + 3, frame.size() - 3, received) &&
               received.page == 1 && received.total_pages == 0,
           "failed refresh can acknowledge a later page without inventing a total");
    state.total_pages = 1;
    expect(frame_trading_post_state(state).empty(), "reject current page beyond total pages");
    server::SqliteStore store;
    expect(store.open(":memory:"), "open isolated market database");
    test::trading_post_contract(store);
    // Independent connections race for the same escrow quantity.
    const auto file = (std::filesystem::temp_directory_path() /
                       ("openkal-market-" + std::to_string(getpid()) + ".sqlite"))
                          .string();
    std::filesystem::remove(file);
    server::SqliteStore first, second;
    expect(first.open(file), "open durable market database");
    server::AccountRecord account;
    account.id = 1;
    account.name = "OwnedMarketRace";
    account.password = "owned";
    expect(first.upsert_account(account), "seed race account");
    server::CharacterRecord seller, a, b;
    seller.id = 1;
    seller.account_id = 1;
    seller.name = "Seller";
    seller.money = 10000;
    a = seller;
    a.id = 2;
    a.name = "BuyerA";
    b = seller;
    b.id = 3;
    b.name = "BuyerB";
    expect(first.save_character(seller) && first.save_character(a) && first.save_character(b),
           "seed concurrent buyers");
    server::ItemRecord item;
    item.iid = 100;
    item.character_id = 1;
    item.index = 1;
    item.count = 5;
    expect(first.insert_item(item) && first.trading_post_sell(item, 5, 100, {}).committed,
           "seed contested offer");
    TradingPostRequest browse;
    browse.operation = 1;
    auto offers = first.trading_post_list(2, browse);
    if (!offers || offers->entries.empty()) {
        return 1;
    }
    const auto id = offers->entries[0].id;
    expect(second.open(file), "open independent buyer connection");
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    server::TradingPostResult x, y;
    auto run = [&](server::SqliteStore& db,
                   const server::CharacterRecord& buyer,
                   server::TradingPostResult& result) {
        ++ready;
        while (!go.load()) {
            std::this_thread::yield();
        }
        result = db.trading_post_buy(buyer, id, 4, 100, buyer.id == 2 ? 200001 : 200002);
    };
    std::thread one(run, std::ref(first), std::cref(a), std::ref(x));
    std::thread two(run, std::ref(second), std::cref(b), std::ref(y));
    while (ready.load() != 2) {
        std::this_thread::yield();
    }
    go = true;
    one.join();
    two.join();
    expect(x.committed && y.committed && x.quantity + y.quantity == 5 &&
               x.charged + y.charged == 500,
           "racing purchases conserve all escrow and charge only five items");
    expect((x.quantity == 1 || y.quantity == 1) &&
               first.load_character(2)->money + first.load_character(3)->money == 19500,
           "losing buyer gets reduced fill without double spending");
    first.close();
    expect(first.open(file), "reopen durable market store");
    browse.view = TradingPostView::Collection;
    auto receipts = first.trading_post_list(1, browse);
    expect(receipts && receipts->entries.size() == 2, "sale receipts survive database restart");
    // A failed receipt insert must roll back the offer and the debit together.
    item.iid = 101;
    item.count = 2;
    expect(first.insert_item(item) && first.trading_post_sell(item, 2, 200, {}).committed,
           "seed rollback offer");
    browse.view = TradingPostView::Buy;
    offers = first.trading_post_list(2, browse);
    if (!offers || offers->entries.empty()) {
        return 1;
    }
    sqlite3* db = nullptr;
    expect(sqlite3_open(file.c_str(), &db) == SQLITE_OK, "open fault injection connection");
    expect(
        sqlite3_exec(db,
                     "CREATE TRIGGER fail_market_delivery BEFORE INSERT ON trading_post_deliveries "
                     "BEGIN SELECT RAISE(ABORT,'owned test failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "inject receipt failure");
    auto current = first.load_character(2);
    expect(current.has_value(), "load buyer after race");
    expect(!first.trading_post_buy(*current, offers->entries[0].id, 2, 200).committed &&
               first.load_character(2)->money == current->money,
           "failed delivery rolls back buyer debit");
    auto same = first.trading_post_list(2, browse);
    expect(same && same->entries[0].item.count == 2, "failed delivery leaves escrow available");
    sqlite3_close(db);
    browse.view = TradingPostView::Collection;
    receipts = first.trading_post_list(a.id, browse);
    if (!receipts || receipts->entries.empty()) {
        return 1;
    }
    const auto receipt_id = receipts->entries.front().id;
    const auto owner = *first.load_character(a.id);
    expect(sqlite3_open(file.c_str(), &db) == SQLITE_OK,
           "open collection fault injection connection");
    expect(sqlite3_exec(
               db,
               "CREATE TRIGGER fail_collection_delete BEFORE DELETE ON trading_post_deliveries "
               "BEGIN SELECT RAISE(ABORT,'owned collection failure'); END",
               nullptr,
               nullptr,
               nullptr) == SQLITE_OK,
           "inject receipt removal failure");
    expect(!first.trading_post_collect(owner, receipt_id).committed &&
               first.load_items(a.id).empty() &&
               first.trading_post_list(a.id, browse)->entries.size() == receipts->entries.size(),
           "failed collection rolls back item placement and preserves its reference");
    expect(sqlite3_exec(db, "DROP TRIGGER fail_collection_delete", nullptr, nullptr, nullptr) ==
               SQLITE_OK,
           "remove collection fault injection");
    sqlite3_close(db);

    server::ItemRecord merge_target;
    merge_target.iid = 300000;
    merge_target.character_id = owner.id;
    merge_target.account_id = owner.account_id;
    merge_target.index = 1;
    merge_target.count = 10;
    expect(first.insert_item(merge_target), "seed merge target for pickup rollback and race");
    expect(sqlite3_open(file.c_str(), &db) == SQLITE_OK &&
               sqlite3_exec(
                   db,
                   "CREATE TRIGGER fail_merge_source BEFORE DELETE ON items "
                   "WHEN OLD.iid<>300000 BEGIN SELECT RAISE(ABORT,'owned source failure'); END",
                   nullptr,
                   nullptr,
                   nullptr) == SQLITE_OK,
           "inject source deletion failure after stack count and receipt change");
    expect(
        !first.trading_post_collect(owner, receipt_id, &merge_target).committed &&
            first.load_items(owner.id).size() == 1 && first.load_items(owner.id)[0].count == 10 &&
            first.trading_post_list(owner.id, browse)->entries.size() == receipts->entries.size(),
        "failed source deletion rolls back the increment, receipt removal and source transfer");
    expect(sqlite3_exec(db, "DROP TRIGGER fail_merge_source", nullptr, nullptr, nullptr) ==
               SQLITE_OK,
           "remove merge deletion failure");
    sqlite3_close(db);
    ready = 0;
    go = false;
    server::TradingPostCollectionResult left, right;
    auto claim = [&](server::SqliteStore& connection, server::TradingPostCollectionResult& result) {
        ++ready;
        while (!go.load()) {
            std::this_thread::yield();
        }
        result = connection.trading_post_collect(owner, receipt_id, &merge_target);
    };
    std::thread pickup_one(claim, std::ref(first), std::ref(left));
    std::thread pickup_two(claim, std::ref(second), std::ref(right));
    while (ready.load() != 2) {
        std::this_thread::yield();
    }
    go = true;
    pickup_one.join();
    pickup_two.join();
    expect(left.committed != right.committed && first.load_items(a.id).size() == 1 &&
               first.load_items(a.id)[0].count ==
                   10 + (left.committed ? left.received_count : right.received_count),
           "concurrent collection over separate connections restores the delivery exactly once");
    first.close();
    second.close();
    std::filesystem::remove(file);
    std::filesystem::remove(file + "-wal");
    std::filesystem::remove(file + "-shm");
    return test::g_failures ? 1 : 0;
}
