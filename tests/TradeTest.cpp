/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Trade.h"

#include "openkal/protocol/FrameParser.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>

using namespace openkal;
int main() {
    using test::expect;
    protocol::TradeRequest request;
    request.command = protocol::TradeCommand::Offer;
    request.token = 12;
    request.revision = 3;
    request.money = 125;
    request.items = {{42, 2, 0}, {43, 1, 11}};
    auto frame = protocol::frame_trade_request(request);
    std::size_t n = 0;
    std::uint8_t type = 0;
    std::vector<std::uint8_t> payload;
    expect(protocol::FrameParser::pop(frame.data(), frame.size(), n, type, payload) ==
               protocol::FrameParseStatus::Complete,
           "deframe owned trade request");
    protocol::TradeRequest parsed;
    expect(protocol::parse_trade_request(payload.data(), payload.size(), parsed) &&
               parsed.items == request.items && parsed.revision == 3,
           "round trip token, revision and complete offer");
    payload.push_back(0);
    expect(!protocol::parse_trade_request(payload.data(), payload.size(), parsed),
           "reject trailing trade request bytes");
    request.items = {{42, 1, 0}, {42, 1, 1}};
    frame = protocol::frame_trade_request(request);
    (void) protocol::FrameParser::pop(frame.data(), frame.size(), n, type, payload);
    expect(!protocol::parse_trade_request(payload.data(), payload.size(), parsed),
           "reject duplicated offered IID");
    request.money = -1;
    expect(protocol::frame_trade_request(request).empty(), "negative money cannot be encoded");
    protocol::TradeState state;
    state.phase = protocol::TradePhase::Open;
    state.token = 12;
    state.revision = 4;
    state.partner = "OwnedPeer";
    state.status = "Review the exchange";
    state.inventory_slots = protocol::kTradeInventorySlots;
    state.partner = std::string(64, 'p');
    state.status = std::string(256, 's');
    for (unsigned i = 0; i < protocol::kTradeItems; ++i) {
        protocol::TradeItem item;
        item.iid = 100 + i;
        item.index = 1;
        item.count = 2000000000;
        item.slot = static_cast<std::uint16_t>(i);
        item.attributes.fill(INT32_MAX);
        item.pet_expire_at = UINT64_MAX;
        state.own.push_back(item);
        item.iid = 200 + i;
        state.other.push_back(item);
    }
    for (unsigned i = 0; i < protocol::kTradeInventorySlots; ++i) {
        protocol::TradeItem item;
        item.iid = 300 + i;
        item.index = 3;
        item.count = 100;
        item.slot = static_cast<std::uint16_t>(i);
        state.available.push_back(item);
    }
    frame = protocol::frame_trade_state(state);
    expect(!frame.empty() && frame.size() < 8000,
           "both full twelve-slot offers and the entire inventory fit one packet");
    (void) protocol::FrameParser::pop(frame.data(), frame.size(), n, type, payload);
    protocol::TradeState received;
    expect(protocol::parse_trade_state(payload.data(), payload.size(), received) &&
               received.locked() && received.other.size() == protocol::kTradeItems &&
               received.own.back().slot == 11 &&
               received.own.back().attribute(protocol::TradeAttribute::Attack) == INT32_MAX &&
               received.available.size() == protocol::kTradeInventorySlots,
           "round trip modal trade state");
    expect(!protocol::parse_trade_state(payload.data(), payload.size() - 1, received),
           "reject truncated snapshot");
    state.phase = protocol::TradePhase::Completed;
    state.partner_money = 500;
    state.own.clear();
    state.available.clear();
    frame = protocol::frame_trade_state(state);
    (void) protocol::FrameParser::pop(frame.data(), frame.size(), n, type, payload);
    expect(protocol::parse_trade_state(payload.data(), payload.size(), received) &&
               received.phase == protocol::TradePhase::Completed && !received.locked() &&
               received.other.size() == protocol::kTradeItems && received.partner_money == 500,
           "committed receipt round trips and unlocks the client");

    server::SqliteStore store;
    expect(store.open(":memory:"), "open isolated trade store");
    server::AccountRecord account;
    account.id = 1;
    account.name = "trade";
    account.password = "owned";
    expect(store.upsert_account(account), "seed trade account");
    server::CharacterRecord a, b;
    a.id = 1;
    a.account_id = 1;
    a.name = "TraderOne";
    a.money = 100;
    b = a;
    b.id = 2;
    b.name = "TraderTwo";
    b.money = 200;
    expect(store.save_character(a) && store.save_character(b), "seed trade characters");
    server::ItemRecord item;
    item.iid = 42;
    item.character_id = 1;
    item.index = 1;
    item.count = 3;
    item.prefix = 4;
    item.x_attack = 5;
    item.enchant_protected = true;
    expect(store.insert_item(item), "seed complete transferable instance");
    auto final_a = a, final_b = b;
    final_a.money = 150;
    final_b.money = 150;
    expect(store.commit_trade(final_a, final_b, {{item, item.count, 0}}),
           "commit ownership and both balances together");
    auto bag = store.load_items(2);
    expect(store.load_items(1).empty() && bag.size() == 1 && bag[0].iid == 42 &&
               bag[0].count == 3 && bag[0].prefix == 4 && bag[0].x_attack == 5 &&
               bag[0].enchant_protected,
           "transfer preserves IID, enhancements and protection");
    expect(store.load_character(1)->money == 150 && store.load_character(2)->money == 150,
           "money sum conserved");
    final_a.money = 0;
    final_b.money = 300;
    expect(!store.commit_trade(final_a, final_b, {{item, item.count, 0}}),
           "replayed source ownership fails atomically");
    expect(store.load_character(1)->money == 150 && store.load_character(2)->money == 150,
           "failed replay rolls back both balances");
    item = bag.front();
    item.character_bound = true;
    expect(store.update_item(item), "bind received item");
    auto stale = item;
    stale.character_bound = false;
    expect(!store.commit_trade(final_a, final_b, {{stale, stale.count, 0}}),
           "database checks live binding even if caller supplies stale flags");
    expect(store.load_items(2).size() == 1 && store.load_character(1)->money == 150,
           "bound failure preserves all state");
    item.character_bound = false;
    expect(store.update_item(item), "restore transferable item");
    auto missing = item;
    missing.iid = 999;
    expect(
        !store.commit_trade(final_a, final_b, {{item, item.count, 0}, {missing, missing.count, 0}}),
        "late missing item rolls back earlier ownership update");
    expect(store.load_items(1).empty() && store.load_items(2).size() == 1 &&
               store.load_character(2)->money == 150,
           "rollback restores ownership and wallets after partial SQL work");
    expect(!store.commit_trade(final_a, final_b, {{item, item.count, 0}, {item, item.count, 0}}),
           "duplicate item transfer rejected");
    expect(!store.commit_trade(final_a, final_a, {}), "same character cannot trade with itself");
    expect(store.commit_trade(final_a, final_b, {}), "money-only trade is atomic");
    expect(store.commit_trade(final_a, final_b, {{item, 1, 55}}),
           "partial stack and both balances commit together");
    auto remaining = store.load_items(2), received_items = store.load_items(1);
    expect(remaining.size() == 1 && remaining[0].iid == 42 && remaining[0].count == 2 &&
               received_items.size() == 1 && received_items[0].iid == 55 &&
               received_items[0].count == 1 && received_items[0].prefix == 4 &&
               received_items[0].x_attack == 5 && received_items[0].enchant_protected,
           "split preserves source IID, total quantity and every instance modifier");
    expect(!store.commit_trade(final_a, final_b, {{item, 1, 56}}),
           "replayed stale split cannot duplicate quantity");
    item = remaining[0];
    expect(!store.commit_trade(final_a, final_b, {{item, 1, 55}}),
           "split cannot overwrite an existing IID");
    expect(!store.commit_trade(final_a, final_b, {{item, 3, 56}}),
           "split cannot exceed owned quantity");
    expect(!store.commit_trade(final_a, final_b, {{item, 1, 57}, {missing, 1, 58}}),
           "late failure rolls back a newly inserted split");
    expect(store.load_items(1).size() == 1 && store.load_items(2)[0].count == 2,
           "failed split leaves no destination row or lost source units");
    return test::g_failures ? 1 : 0;
}
