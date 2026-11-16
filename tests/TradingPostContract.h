/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>
#include <limits>

namespace openkal::test {
    /** Runs the same transaction and lossless-instance contract on each SQL backend.
     * @param store Open isolated persistence backend. */
    inline void trading_post_contract(server::IDataStore& store) {
        using namespace server;
        using namespace protocol;
        AccountRecord account;
        account.id = 900;
        account.name = "MarketContract";
        account.password = "owned";
        expect(store.upsert_account(account), "create market contract account");
        CharacterRecord seller, buyer;
        seller.id = 900;
        seller.account_id = 900;
        seller.name = "PrivateSeller";
        seller.money = 10000;
        buyer = seller;
        buyer.id = 901;
        buyer.name = "MarketBuyer";
        expect(store.save_character(seller) && store.save_character(buyer),
               "create market participants");
        ItemRecord item;
        item.iid = 90000;
        item.character_id = seller.id;
        item.index = 1;
        item.count = 12;
        item.prefix = 4;
        item.cur_end = 31;
        item.max_end = 52;
        item.x_attack = 11;
        item.x_magic = 12;
        item.x_hit = 13;
        item.x_defense = 14;
        item.x_dodge = 15;
        item.upgrade_level = 7;
        item.upgrade_rate = 8;
        item.mix_type = 9;
        item.bof = 1;
        item.qigong_perforated = 1;
        item.qigong_effect = 33;
        item.dragon_effect = 99;
        item.random_stat_type = 2;
        item.random_stat_value = 5;
        item.transform_level = 21;
        item.transform_exp = 12345;
        item.pet_expire_at = 1800000000;
        item.ride_expire_at = 1800000123;
        item.enchant_protected = true;
        expect(store.insert_item(item), "seed fully enhanced market instance");
        TradingPostCategory category;
        category.clazz = 0;
        category.type = TradingPostType::Weapon;
        category.grade = 50;
        auto stale = item;
        stale.x_attack = 10;
        expect(!store.trading_post_sell(stale, 8, 100, category).committed,
               "reject changed source attributes");
        expect(store.trading_post_sell(item, 8, 100, category, 990001).committed,
               "escrow only offered quantity");
        auto inventory = store.load_items(seller.id);
        expect(inventory.size() == 1 && inventory[0].count == 4 &&
                   inventory[0].account_id == seller.account_id,
               "source remainder stays in seller inventory");
        expect(!store.trading_post_sell(item, 8, 100, category, 990001).committed,
               "stale source replay cannot create duplicate offers");
        TradingPostRequest browse;
        browse.operation = 1;
        browse.filter.type = TradingPostType::Weapon;
        browse.filter.grade = 50;
        auto page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.size() == 1, "browse by equipment type and grade");
        if (!page || page->entries.empty()) {
            return;
        }
        const auto id = page->entries[0].id;
        const auto& actual = page->entries[0].item;
        expect(!actual.iid && actual.attribute(TradeAttribute::Attack) == 11 &&
                   actual.attribute(TradeAttribute::UpgradeRate) == 8 &&
                   actual.attribute(TradeAttribute::Dragon) == 99 &&
                   actual.attribute(TradeAttribute::Protected) == 1 &&
                   actual.pet_expire_at == 1800000000,
               "anonymous listing retains complete modifiers and expiry");
        expect(!store.trading_post_buy(seller, id, 1, 100).committed,
               "reject purchasing own offer");
        expect(!store.trading_post_buy(buyer, id, 3, 99).committed, "reject stale displayed price");
        auto poor = buyer;
        poor.money = 1;
        expect(!store.trading_post_buy(poor, id, 3, 100).committed,
               "reject insufficient funds without touching offer");
        const auto fill = store.trading_post_buy(buyer, id, 3, 100, 990002);
        expect(fill.committed && fill.quantity == 3 && fill.charged == 300 &&
                   fill.seller == seller.id,
               "buy requested partial quantity atomically");
        buyer.money -= fill.charged;
        expect(store.load_character(buyer.id)->money == buyer.money &&
                   store.load_items(buyer.id).empty(),
               "charge persisted while purchase waits outside inventory");
        auto collection = browse;
        collection.view = TradingPostView::Collection;
        auto receipts = store.trading_post_list(buyer.id, collection);
        expect(receipts && receipts->entries.size() == 1 && receipts->entries[0].item.count == 3 &&
                   receipts->entries[0].delivery == TradingPostDelivery::Purchase,
               "buyer sees durable purchased quantity");
        receipts = store.trading_post_list(seller.id, collection);
        expect(receipts && receipts->entries.size() == 1 &&
                   receipts->entries[0].delivery == TradingPostDelivery::Proceeds &&
                   receipts->entries[0].unit_price * receipts->entries[0].item.count == 300 &&
                   store.load_character(seller.id)->money == seller.money,
               "same-account seller receives only pending Geons, while buyer receives items");
        const auto short_fill = store.trading_post_buy(buyer, id, 10, 100);
        expect(short_fill.committed && short_fill.quantity == 5 && short_fill.charged == 500 &&
                   short_fill.status.find("Only 5 of 10") != std::string::npos,
               "fill available remainder and explain reduced quantity");
        buyer.money -= short_fill.charged;
        expect(!store.trading_post_buy(buyer, id, 1, 100).committed &&
                   store.load_character(buyer.id)->money == buyer.money,
               "sold-out offer rejects repeated purchase without charge");
        inventory = store.load_items(seller.id);
        item = inventory[0];
        expect(store.trading_post_sell(item, 4, 200, category).committed,
               "list remaining source inventory");
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.size() == 1, "sold offer removed from search");
        if (!page || page->entries.empty()) {
            return;
        }
        const auto second = page->entries[0].id;
        expect(!store.trading_post_cancel(buyer.id, second).committed,
               "only seller may withdraw listing");
        expect(store.trading_post_cancel(seller.id, second).committed,
               "withdraw remaining items into collection");
        expect(!store.trading_post_cancel(seller.id, second).committed,
               "withdrawal cannot duplicate returned items");
        receipts = store.trading_post_list(seller.id, collection);
        expect(receipts && receipts->entries.size() == 3 &&
                   receipts->entries[0].delivery == TradingPostDelivery::Returned &&
                   receipts->entries[0].item.count == 4 &&
                   receipts->entries[1].delivery == TradingPostDelivery::Proceeds &&
                   receipts->entries[2].delivery == TradingPostDelivery::Proceeds,
               "collection distinguishes returns and sale proceeds");
        receipts = store.trading_post_list(buyer.id, collection);
        expect(receipts && receipts->entries.size() == 2 &&
                   receipts->entries[0].delivery == TradingPostDelivery::Purchase &&
                   receipts->entries[1].delivery == TradingPostDelivery::Purchase &&
                   receipts->entries[0].item.count + receipts->entries[1].item.count == 8,
               "same-account buyer sees purchased items without seller proceeds or returns");
        browse.filter.grade = 55;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.empty() && page->page == 0 && page->total_pages == 1,
               "empty grade filter has one empty page");
        for (int n = 0; n < 64; ++n) {
            ItemRecord offer;
            offer.iid = 90100 + n;
            offer.character_id = seller.id;
            offer.index = 1;
            offer.count = 1;
            offer.prefix = n % 2;
            offer.upgrade_level = n % 4;
            expect(store.insert_item(offer) &&
                       store.trading_post_sell(offer, 1, n + 1, category).committed,
                   "seed indexed market page");
        }
        browse = {};
        browse.operation = 2;
        browse.filter.type = TradingPostType::Weapon;
        browse.filter.grade = 50;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.size() == 60 && page->more && page->total_pages == 2 &&
                   page->entries.front().unit_price == 1,
               "price ordering returns bounded first page");
        browse.filter.page = 1;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.size() == 4 && !page->more && page->page == 1 &&
                   page->total_pages == 2 && page->entries.front().unit_price == 61,
               "pagination preserves sorted continuation");
        browse.filter.page = 10;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->page == 1 && page->total_pages == 2 && page->entries.size() == 4,
               "out-of-range page returns the last populated page");
        auto own_page_request = browse;
        own_page_request.view = TradingPostView::Own;
        page = store.trading_post_list(buyer.id, own_page_request);
        expect(page && page->page == 0 && page->total_pages == 1 && page->entries.empty(),
               "own-offer page count excludes another seller's offers");
        page = store.trading_post_list(seller.id, own_page_request);
        expect(page && page->total_pages == 2 && page->entries.size() == 4,
               "seller page count includes all owned offers");
        browse.filter.page = 0;
        browse.filter.order = TradingPostOrder::PriceDescending;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.front().unit_price == 64, "descending unit-price order");
        browse.filter.order = TradingPostOrder::EnhancementDescending;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.front().item.attribute(TradeAttribute::UpgradeLevel) == 3,
               "descending enhancement order");
        browse.filter.order = TradingPostOrder::EnhancementAscending;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.front().item.attribute(TradeAttribute::UpgradeLevel) == 0,
               "ascending enhancement order");
        browse.filter.prefix = 1;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.size() == 32 && !page->more && page->total_pages == 1,
               "prefix filter uses stored instance prefix");
        browse.filter.page = 1;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->page == 0 && page->total_pages == 1 && page->entries.size() == 32,
               "smaller filtered result clamps the page to its populated range");
        browse.filter.page = 0;
        browse.filter.clazz = 1;
        page = store.trading_post_list(buyer.id, browse);
        expect(page && page->entries.empty(), "class filter excludes incompatible equipment");

        receipts = store.trading_post_list(buyer.id, collection);
        if (!receipts || receipts->entries.size() != 2) {
            return;
        }
        const auto bought_receipt = receipts->entries.back().id;
        expect(!store.trading_post_collect(seller, bought_receipt).committed,
               "seller cannot claim the buyer's purchased item");
        const auto pickup = store.trading_post_collect(buyer, bought_receipt);
        expect(pickup.committed && pickup.item && pickup.item->iid == 990002 &&
                   pickup.item->count == 3 && !pickup.geons &&
                   pickup.item->attribute(TradeAttribute::Attack) == 11 &&
                   pickup.item->attribute(TradeAttribute::AnimalExp) == 12345 &&
                   pickup.item->pet_expire_at == 1800000000,
               "item pickup preserves the purchased stack IID, quantity, modifiers and expiry");
        const auto restored = store.load_items(buyer.id);
        const auto found = std::find_if(
            restored.begin(), restored.end(), [](const auto& row) { return row.iid == 990002; });
        expect(found != restored.end() && found->character_id == buyer.id &&
                   found->account_id == buyer.account_id && found->x_attack == 11 &&
                   found->upgrade_rate == 8 && found->enchant_protected &&
                   found->ride_expire_at == 1800000123,
               "restored database item preserves attributes outside the public charm packet");
        expect(!store.trading_post_collect(buyer, bought_receipt).committed,
               "replaying item collection cannot duplicate its inventory instance");

        receipts = store.trading_post_list(seller.id, collection);
        if (!receipts || receipts->entries.size() != 3) {
            return;
        }
        const auto proceeds = receipts->entries.back().id;
        seller.money = kTradingPostMaxPrice - 150;
        expect(store.save_character(seller), "prepare a nearly full wallet");
        const auto paid = store.trading_post_collect(seller, proceeds);
        expect(paid.committed && !paid.item && paid.geons == 100 &&
                   store.load_character(seller.id)->money == seller.money + 100,
               "only proceeds fitting the wallet are paid, with the remainder retained");
        seller.money += paid.geons;
        expect(!store.trading_post_collect(seller, proceeds).committed,
               "a full wallet leaves remaining proceeds untouched");
        seller.money = 0;
        expect(store.save_character(seller), "make wallet room for remaining proceeds");
        const auto remainder = store.trading_post_collect(seller, proceeds);
        expect(remainder.committed && remainder.geons == 200 &&
                   !store.trading_post_collect(seller, proceeds).committed,
               "remaining proceeds are paid exactly once");

        ItemRecord mount_item;
        mount_item.iid = 990010;
        mount_item.character_id = seller.id;
        mount_item.index = 1;
        mount_item.count = 1;
        BattleMountRecord mount;
        mount.mount_iid = mount_item.iid;
        mount.satiety = 73;
        mount.revision = 91;
        mount.abilities[0] = 7;
        expect(store.insert_item(mount_item) && store.upsert_battle_mount(mount) &&
                   store.trading_post_sell(mount_item, 1, 100, category).committed,
               "escrow Battle Riding state with the item");
        TradingPostRequest own;
        own.view = TradingPostView::Own;
        own.filter.order = TradingPostOrder::PriceDescending;
        auto mounted_offers = store.trading_post_list(seller.id, own);
        if (!mounted_offers) {
            return;
        }
        const auto mounted_offer =
            std::find_if(mounted_offers->entries.begin(),
                         mounted_offers->entries.end(),
                         [](const auto& entry) { return entry.unit_price == 100; });
        expect(mounted_offer != mounted_offers->entries.end() &&
                   store.trading_post_cancel(seller.id, mounted_offer->id).committed,
               "withdraw mount to durable collection");
        receipts = store.trading_post_list(seller.id, collection);
        const auto mount_pickup = store.trading_post_collect(*store.load_character(seller.id),
                                                             receipts->entries.front().id);
        const auto restored_mount = store.load_battle_mount(mount_item.iid);
        expect(mount_pickup.committed && restored_mount && restored_mount->satiety == 73 &&
                   restored_mount->revision == 91 && restored_mount->abilities[0] == 7,
               "mount collection preserves the existing IID and Battle Riding state");
        ItemRecord target;
        target.iid = 995000;
        target.character_id = buyer.id;
        target.account_id = buyer.account_id;
        target.index = 23000;
        target.count = 10;
        ItemRecord delivery = target;
        delivery.iid = 995001;
        delivery.character_id = seller.id;
        delivery.account_id = seller.account_id;
        delivery.count = 4;
        expect(store.insert_item(target) && store.insert_item(delivery) &&
                   store.trading_post_sell(delivery, 4, 1, {}).committed,
               "seed stack merge and escrow source");
        TradingPostRequest merge_request;
        merge_request.view = TradingPostView::Own;
        const auto own_page = store.trading_post_list(seller.id, merge_request);
        expect(own_page && !own_page->entries.empty(), "find stack merge offer");
        if (!own_page || own_page->entries.empty()) {
            return;
        }
        auto current_buyer = *store.load_character(buyer.id);
        const auto merge_offer =
            std::find_if(own_page->entries.begin(), own_page->entries.end(), [](const auto& entry) {
                return entry.item.index == 23000;
            });
        expect(merge_offer != own_page->entries.end(), "locate the synthetic stack offer");
        if (merge_offer == own_page->entries.end()) {
            return;
        }
        expect(store.trading_post_buy(current_buyer, merge_offer->id, 4, 1).committed,
               "buy stack merge delivery");
        merge_request.view = TradingPostView::Collection;
        const auto merge_collection = store.trading_post_list(buyer.id, merge_request);
        expect(merge_collection && !merge_collection->entries.empty(), "find merge receipt");
        if (!merge_collection || merge_collection->entries.empty()) {
            return;
        }
        const auto merge_entry =
            std::find_if(merge_collection->entries.begin(),
                         merge_collection->entries.end(),
                         [](const auto& entry) { return entry.item.index == 23000; });
        expect(merge_entry != merge_collection->entries.end(),
               "locate the synthetic merge receipt");
        if (merge_entry == merge_collection->entries.end()) {
            return;
        }
        const auto merge_receipt = merge_entry->id;
        current_buyer = *store.load_character(buyer.id);
        auto incompatible = target;
        incompatible.prefix = 1;
        expect(!store.trading_post_collect(current_buyer, merge_receipt, &incompatible).committed,
               "incompatible instance properties leave receipt and both stacks unchanged");
        auto stale_target = target;
        --stale_target.count;
        expect(!store.trading_post_collect(current_buyer, merge_receipt, &stale_target).committed,
               "stale destination count rolls back source transfer");
        stale_target = target;
        stale_target.x_attack = 1;
        expect(!store.trading_post_collect(current_buyer, merge_receipt, &stale_target).committed,
               "changed destination attributes cannot overwrite a stack");
        auto changed_target = target;
        changed_target.x_attack = 1;
        expect(store.update_item(changed_target) &&
                   !store.trading_post_collect(current_buyer, merge_receipt, &target).committed &&
                   store.update_item(target),
               "persisted attribute changes invalidate an otherwise compatible destination");
        auto oversized = target;
        oversized.count = std::numeric_limits<int>::max();
        expect(
            store.update_item(oversized) &&
                !store.trading_post_collect(current_buyer, merge_receipt, &oversized).committed &&
                store.update_item(target),
            "overflow leaves delivery collectible and destination intact");
        const auto merged = store.trading_post_collect(current_buyer, merge_receipt, &target);
        expect(merged.committed && merged.item &&
                   merged.item->iid == static_cast<std::uint32_t>(target.iid) &&
                   merged.item->count == 14 && merged.received_count == 4,
               "atomic pickup keeps destination IID and reports only received units");
        expect(!store.trading_post_collect(current_buyer, merge_receipt, &target).committed,
               "replaying merged pickup never increases the stack again");
        auto merged_inventory = store.load_items(buyer.id);
        const auto destination =
            std::find_if(merged_inventory.begin(), merged_inventory.end(), [&](const auto& value) {
                return value.iid == target.iid;
            });
        expect(destination != merged_inventory.end() && destination->count == 14 &&
                   std::none_of(merged_inventory.begin(),
                                merged_inventory.end(),
                                [&](const auto& value) { return value.iid == delivery.iid; }),
               "merged source row is removed and total quantity survives");
    }
}  // namespace openkal::test
