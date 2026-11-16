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
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/TradingPost.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

using namespace openkal;
namespace {
    bool wait_frame(server::ISession& session,
                    std::vector<std::uint8_t>& rx,
                    protocol::S2C expected,
                    std::vector<std::uint8_t>& payload,
                    std::vector<protocol::TradingPostPickup>* pickups = nullptr) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            if (!session.recv_into(rx)) {
                return false;
            }
            for (;;) {
                std::size_t used = 0;
                std::uint8_t type = 0;
                const auto parsed =
                    protocol::FrameParser::pop(rx.data(), rx.size(), used, type, payload);
                if (parsed == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                if (parsed == protocol::FrameParseStatus::Invalid) {
                    return false;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(used));
                if (type == static_cast<std::uint8_t>(protocol::S2C::TradingPostPickup) &&
                    pickups) {
                    protocol::TradingPostPickup pickup;
                    test::expect(
                        protocol::parse_trading_post_pickup(payload.data(), payload.size(), pickup),
                        "server emits a valid pickup receipt");
                    pickups->push_back(pickup);
                }
                if (type == static_cast<std::uint8_t>(expected)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < until);
        return false;
    }
}  // namespace
int main() {
    using test::expect;
    using namespace protocol;
    const auto root = (std::filesystem::temp_directory_path() /
                       ("openkal-market-server-" + std::to_string(getpid())))
                          .string();
    std::filesystem::remove_all(root);
    server::ServerConfig config;
    config.transport = server::TransportKind::Unix;
    config.store = server::StoreBackend::Sqlite;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.npcs_dir = root + "/npcs";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    config.character_id = 1;
    for (const auto& dir : {config.maps_dir,
                            config.monsters_dir,
                            config.items_dir,
                            config.save_dir,
                            config.npcs_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write owned market map");
    {
        std::ofstream f(config.monsters_dir + "/1.json");
        f << R"({"index":1,"stats":{"level":1,"hp":1},"spawns":[]})";
    }
    {
        std::ofstream f(config.items_dir + "/23000.json");
        f << R"({"index":23000,"name":"Owned medicine","class":["general","etc"],"stackable":1,"use":1})";
    }
    for (int n = 0; n < 3; ++n) {
        std::ofstream f(config.npcs_dir + "/" + std::to_string(60003 + n) + ".json");
        f << "{\"index\":" << 60003 + n
          << ",\"map\":0,\"shape\":75,\"kind\":3,\"trading_post_collector\":"
          << (n == 1 ? "false" : "true") << ",\"xy\":[" << config.spawn_x + (n == 2 ? 160 : 16)
          << ", " << config.spawn_y << ", 0]}";
    }
    server::SqliteStore store;
    expect(store.open(config.save_dir), "open market server store");
    server::AccountRecord account;
    account.id = 1;
    account.name = server::kDefaultAccountName;
    account.password = server::kDefaultAccountPassword;
    expect(store.upsert_account(account), "seed local market account");
    server::CharacterRecord buyer, seller;
    buyer.id = 1;
    buyer.account_id = 1;
    buyer.name = "MarketBuyer";
    buyer.money = 10000;
    buyer.map_x = buyer.map_y = config.spawn_x;
    buyer.map_z = 0;
    seller = buyer;
    seller.id = 2;
    seller.name = "OfflineSeller";
    expect(store.save_character(buyer) && store.save_character(seller) &&
               store.grant_starter_items(buyer, {}),
           "seed market characters with starter grant already complete");
    server::ItemRecord item;
    item.iid = 100;
    item.character_id = 2;
    item.index = 23000;
    item.count = 6;
    item.x_attack = 7;
    expect(store.insert_item(item) && store.trading_post_sell(item, 6, 100, {}).committed,
           "list offline seller's stack");
    item.iid = 101;
    item.character_id = 1;
    item.count = 5;
    item.x_attack = 0;
    expect(store.insert_item(item), "seed online seller's inventory");
    store.close();
    server::ServerHost host;
    expect(host.start(config), "start owned market server");
    for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto peer = server::connect_unix(config.unix_path);
    expect(peer != nullptr, "connect market client");
    if (!peer) {
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    expect(peer->send_all(Handshake::build_connect(0)) && wait_frame(*peer, rx, S2C::Code, payload),
           "market handshake");
    S2CCodeFields code;
    expect(Handshake::parse_code(payload.data(), payload.size(), code), "parse market handshake");
    expect(peer->send_all(Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*peer, rx, S2C::CreatePlayer, payload),
           "enter market world");
    bool pending = true;
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && !pending,
           "empty collection hides the indicator immediately on login");
    std::uint32_t collector = 0, ordinary_npc = 0, distant_collector = 0;
    const auto read_npcs = [&] {
        collector = ordinary_npc = distant_collector = 0;
        for (int n = 0; n < 3; ++n) {
            CreateNpcInfo npc;
            if (!wait_frame(*peer, rx, S2C::CreateNpc, payload) ||
                !Quest::parse_create_npc(payload.data(), payload.size(), npc)) {
                return false;
            }
            if (npc.index == 60003) {
                collector = npc.id;
            }
            if (npc.index == 60004) {
                ordinary_npc = npc.id;
            }
            if (npc.index == 60005) {
                distant_collector = npc.id;
            }
        }
        return collector && ordinary_npc && distant_collector;
    };
    expect(read_npcs(), "stream near, ordinary and out-of-range collection NPC fixtures");
    std::vector<TradingPostPickup> pickups;
    const auto command = [&](const TradingPostRequest& request) {
        TradingPostState state;
        pickups.clear();
        expect(peer->send_all(frame_trading_post_request(request)) &&
                   wait_frame(*peer, rx, S2C::TradingPost, payload, &pickups) &&
                   parse_trading_post_state(payload.data(), payload.size(), state),
               "exchange market command with real server");
        expect(state.acknowledged == request.operation, "server acknowledges requested operation");
        if (request.command != TradingPostCommand::Collect &&
            request.command != TradingPostCommand::CollectAll) {
            expect(pickups.empty(), "browsing or trading never announces rewards before pickup");
        }
        return state;
    };
    TradingPostRequest request;
    request.operation = 1;
    auto state = command(request);
    expect(state.entries.size() == 1 && !state.entries[0].item.iid,
           "browse anonymous offer from offline seller");
    if (state.entries.empty()) {
        return 1;
    }
    const auto offer = state.entries[0].id;
    request.command = TradingPostCommand::Buy;
    request.operation = 2;
    request.listing = offer;
    request.unit_price = 100;
    request.quantity = 3;
    state = command(request);
    expect(state.purchased == 3 && state.entries.size() == 1 && state.entries[0].item.count == 3,
           "server buys selected portion and refreshes remaining offer");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && pending,
           "committed purchase pushes the pending collection indicator");
    state = command(request);
    expect(state.purchased == 3, "replayed operation receives cached receipt");
    request = {};
    request.operation = 3;
    request.view = TradingPostView::Collection;
    state = command(request);
    expect(state.entries.size() == 1 && state.entries[0].item.count == 3 &&
               state.entries[0].delivery == TradingPostDelivery::Purchase,
           "replay creates no extra purchase receipt");
    request = {};
    request.operation = 4;
    request.command = TradingPostCommand::Sell;
    request.view = TradingPostView::Own;
    request.iid = 101;
    request.quantity = 2;
    request.unit_price = 55;
    state = command(request);
    expect(state.entries.size() == 1 && state.entries[0].item.count == 2,
           "server lists selected live inventory portion");
    if (state.entries.empty()) {
        return 1;
    }
    request = {};
    request.operation = 5;
    request.command = TradingPostCommand::Cancel;
    request.view = TradingPostView::Own;
    request.listing = state.entries[0].id;
    state = command(request);
    expect(state.entries.empty(), "server removes withdrawn own offer");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && pending,
           "withdrawn items also trigger the collection indicator");
    request = {};
    request.operation = 6;
    request.command = TradingPostCommand::Buy;
    request.listing = offer;
    request.unit_price = 100;
    request.quantity = 5;
    state = command(request);
    expect(state.purchased == 3 && state.entries.empty() &&
               state.status.find("Only 3 of 5") != std::string::npos,
           "server fills only currently available quantity and removes sold offer");
    request.operation = 7;
    state = command(request);
    expect(state.purchased == 0 && state.entries.empty() &&
               state.status.find("already") != std::string::npos,
           "server reports sold-out purchase");
    request = {};
    request.operation = 8;
    request.view = TradingPostView::Collection;
    state = command(request);
    expect(state.entries.size() == 3, "list buyer's purchases and withdrawn stack before pickup");
    if (state.entries.empty()) {
        return 1;
    }
    request.command = TradingPostCommand::Collect;
    request.listing = state.entries.front().id;
    for (const auto npc : {UINT32_MAX, ordinary_npc, distant_collector}) {
        ++request.operation;
        request.npc_id = npc;
        state = command(request);
        expect(state.entries.size() == 3 && state.status.find("Visit") != std::string::npos,
               "forged, ordinary and distant NPCs cannot authorize collection");
        expect(pickups.empty(), "rejected pickup emits no reward notice");
        expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload),
               "failed pickup refreshes the pending indicator");
    }
    request.npc_id = collector;
    ++request.operation;
    state = command(request);
    expect(state.entries.size() == 2 && state.status == "Items collected.",
           "nearby agent still supports a selected delivery");
    expect(pickups.size() == 1 && pickups[0].item_index == 23000 && pickups[0].quantity == 3 &&
               pickups[0].geons == 0,
           "selected pickup announces the actual received item stack");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload),
           "selected pickup sends the indicator");
    request.command = TradingPostCommand::CollectAll;
    request.listing = 0;
    ++request.operation;
    state = command(request);
    expect(state.entries.empty() && state.status == "Collected 2 deliveries.",
           "Collect All restores remaining purchases and returns without a selection");
    expect(pickups.size() == 2 && pickups[0].quantity + pickups[1].quantity == 5,
           "Collect All announces each committed item stack");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && !pending,
           "Collect All clears the indicator after the final delivery");
    state = command(request);
    expect(state.entries.empty(), "replayed pickup returns the cached result without another item");
    expect(pickups.empty(), "replayed pickup never repeats reward notices");
    peer->close();
    host.stop();
    expect(store.open(config.save_dir), "reopen server's durable market results");
    const auto saved = store.load_character(1);
    expect(saved && saved->money == 9400, "only actual purchased quantities charged once");
    const auto inventory = store.load_items(1);
    int total_items = 0, purchased_items = 0;
    for (const auto& row : inventory) {
        total_items += row.count;
        if (row.x_attack == 7) {
            purchased_items += row.count;
        }
    }
    expect(inventory.size() == 2 && total_items == 11 && purchased_items == 6,
           "collected purchases and returns persist without duplication and retain instance "
           "modifiers");
    request = {};
    request.operation = 8;
    request.view = TradingPostView::Collection;
    auto receipts = store.trading_post_list(1, request);
    expect(receipts && receipts->entries.empty(),
           "collected buyer receipts stay removed after restart");
    receipts = store.trading_post_list(2, request);
    expect(receipts && receipts->entries.size() == 2 &&
               receipts->entries[0].delivery == TradingPostDelivery::Proceeds &&
               receipts->entries[1].delivery == TradingPostDelivery::Proceeds &&
               receipts->entries[0].unit_price * receipts->entries[0].item.count +
                       receipts->entries[1].unit_price * receipts->entries[1].item.count ==
                   600,
           "offline seller on the same account receives Geons only, never buyer's items");
    // Put proceeds on older pages, behind more item receipts than the unlocked bag can hold.
    for (int n = 0; n < 63; ++n) {
        item.iid = 1000 + n;
        item.character_id = 2;
        item.count = 1;
        item.x_attack = 9;
        expect(store.insert_item(item) && store.trading_post_sell(item, 1, 10, {}).committed,
               "seed multi-page sale");
        TradingPostRequest offers;
        offers.view = TradingPostView::Own;
        offers.filter.order = TradingPostOrder::PriceDescending;
        const auto listing = store.trading_post_list(2, offers);
        auto customer = store.load_character(1);
        expect(listing && !listing->entries.empty() && customer &&
                   store.trading_post_buy(*customer, listing->entries.front().id, 1, 10).committed,
               "seed multi-page proceeds");
    }
    for (int n = 0; n < 75; ++n) {
        item.x_attack = 10 + n;
        item.iid = 2000 + n;
        expect(store.insert_item(item) && store.trading_post_sell(item, 1, 10, {}).committed,
               "seed more returns than the unlocked bag can hold");
        TradingPostRequest offers;
        offers.view = TradingPostView::Own;
        const auto listing = store.trading_post_list(2, offers);
        expect(listing && !listing->entries.empty() &&
                   store.trading_post_cancel(2, listing->entries.front().id).committed,
               "seed multi-page returned deliveries");
    }
    store.close();
    config.character_id = 2;
    expect(host.start(config), "restart with the seller selected");
    for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    peer = server::connect_unix(config.unix_path);
    rx.clear();
    expect(peer != nullptr, "connect returning seller");
    if (!peer) {
        return 1;
    }
    expect(peer->send_all(Handshake::build_connect(0)) &&
               wait_frame(*peer, rx, S2C::Code, payload) &&
               Handshake::parse_code(payload.data(), payload.size(), code) &&
               peer->send_all(Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*peer, rx, S2C::CreatePlayer, payload),
           "enter seller's world after server restart");
    pending = false;
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && pending,
           "offline sale proceeds restore seller's indicator without opening the market");
    expect(read_npcs(), "returning seller receives collection NPCs");
    request = {};
    request.operation = 1;
    request.view = TradingPostView::Collection;
    state = command(request);
    expect(state.entries.size() == kTradingPostPageSize && state.more,
           "seller Collection spans multiple pages");
    request.command = TradingPostCommand::CollectAll;
    request.npc_id = distant_collector;
    ++request.operation;
    state = command(request);
    expect(state.entries.size() == kTradingPostPageSize && state.more &&
               state.status.find("Visit") != std::string::npos,
           "Collect All requires a nearby collector");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload),
           "rejected Collect All refreshes the indicator");
    request.npc_id = collector;
    ++request.operation;
    state = command(request);
    expect(
        state.entries.size() == 3 && !state.more &&
            state.status.find("Collected 137 deliveries.") == 0 &&
            state.status.find("inventory") != std::string::npos,
        "Collect All spans all pages, fills only unlocked slots and continues to older proceeds");
    expect(pickups.size() == 73 && pickups.back().item_index == 0 && pickups.back().geons == 1230,
           "multi-page pickup reports collected items and one total for actual proceeds");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && pending,
           "uncollected items keep the indicator visible");
    state = command(request);
    expect(state.entries.size() == 3,
           "replayed Collect All never creates duplicate items or Geons");
    expect(pickups.empty(), "replayed Collect All does not repeat battle-chat rewards");
    ++request.operation;
    state = command(request);
    expect(state.entries.size() == 3 && state.status.find("Collected 0 deliveries.") == 0,
           "another Collect All leaves blocked receipts intact");
    expect(pickups.empty(), "blocked receipts emit no incoming-item notices");
    peer->close();
    host.stop();
    expect(store.open(config.save_dir) && store.load_character(2)->money == 11230 &&
               store.load_items(2).size() == 72,
           "all pages of proceeds and only available inventory slots persist exactly once");
    request = {};
    request.view = TradingPostView::Collection;
    receipts = store.trading_post_list(2, request);
    expect(receipts && receipts->entries.size() == 3,
           "items blocked by the full bag remain available after restart");
    auto nearly_full_wallet = store.load_character(2);
    if (!nearly_full_wallet) {
        return 1;
    }
    nearly_full_wallet->money = kTradingPostMaxPrice - 150;
    expect(store.save_character(*nearly_full_wallet), "seed limited wallet capacity");
    item.iid = 3000;
    item.count = 3;
    expect(store.insert_item(item) && store.trading_post_sell(item, 3, 100, {}).committed,
           "seed a proceeds receipt that only partly fits the wallet");
    request.view = TradingPostView::Own;
    const auto wallet_offer = store.trading_post_list(2, request);
    const auto customer = store.load_character(1);
    expect(
        wallet_offer && !wallet_offer->entries.empty() && customer &&
            store.trading_post_buy(*customer, wallet_offer->entries.front().id, 3, 100).committed,
        "create partially collectible sale proceeds");
    store.close();
    expect(host.start(config), "restart seller with limited inventory and wallet capacity");
    for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    peer = server::connect_unix(config.unix_path);
    rx.clear();
    if (!peer) {
        return 1;
    }
    expect(peer->send_all(Handshake::build_connect(0)) &&
               wait_frame(*peer, rx, S2C::Code, payload) &&
               Handshake::parse_code(payload.data(), payload.size(), code) &&
               peer->send_all(Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*peer, rx, S2C::CreatePlayer, payload) &&
               wait_frame(*peer, rx, S2C::TradingPostCollection, payload) && read_npcs(),
           "enter seller's world for partial bulk collection");
    request = {};
    request.operation = 1;
    request.command = TradingPostCommand::CollectAll;
    request.view = TradingPostView::Collection;
    request.npc_id = collector;
    state = command(request);
    expect(state.entries.size() == 4 && state.entries.front().item.count == 2 &&
               state.entries.front().delivery == TradingPostDelivery::Proceeds &&
               state.status.find("Collected 1 delivery.") == 0,
           "Collect All pays only whole sold units that fit and preserves the remaining proceeds");
    expect(pickups.size() == 1 && pickups[0].item_index == 0 && pickups[0].geons == 100,
           "partial proceeds announce only the amount paid into the wallet");
    expect(wait_frame(*peer, rx, S2C::TradingPostCollection, payload) &&
               parse_trading_post_collection(payload.data(), payload.size(), pending) && pending,
           "partial proceeds keep the collection indicator visible");
    state = command(request);
    expect(state.entries.size() == 4 && state.entries.front().item.count == 2,
           "replaying partial Collect All cannot pay the receipt twice");
    peer->close();
    host.stop();
    expect(store.open(config.save_dir) &&
               store.load_character(2)->money == kTradingPostMaxPrice - 50 &&
               store.load_items(2).size() == 72,
           "partial bulk collection preserves wallet and inventory limits durably");
    store.close();
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
