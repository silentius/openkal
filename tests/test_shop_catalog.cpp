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
#include "openkal/platform/Path.h"
#include "openkal/server/ShopCatalog.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <unordered_map>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::server::ItemCombatDef;
    using openkal::server::ShopCatalog;
    using openkal::test::expect;

    const std::string dir = openkal::test::temp_dir("shop_catalog");
    const std::string path = openkal::platform::join_path(dir, "0003.json");
    {
        std::ofstream output(path);
        output << R"({"schema":"openkal.shop/v1","npcIndex":3,"legacyStoreId":20,"items":[47,48]})";
        expect(static_cast<bool>(output), "write shop fixture");
    }
    std::unordered_map<int, ItemCombatDef> items;
    items[47].index = 47;
    items[47].buy = 20;
    items[48].index = 48;
    items[48].buy = 70;
    ShopCatalog catalog;
    expect(catalog.load_dir(dir, items), "load valid shop catalog");
    const auto* shop = catalog.find(3);
    expect(shop && shop->legacy_store_id == 20 && shop->items.size() == 2,
           "preserve NPC shop order and legacy id");
    expect(catalog.find(4) == nullptr, "unknown NPC has no shop");

    openkal::server::GameWorld world;
    const std::string repo = OPENKAL_TEST_RESOURCE_ROOT;
    expect(world.load_items_json_dir(openkal::platform::join_path(repo, "config/items")),
           "load repository item definitions");
    expect(world.load_pets_json_dir(openkal::platform::join_path(repo, "config/pets")),
           "load repository pet catalog without legacy fallback");
    expect(world.load_riding_json_dir(openkal::platform::join_path(repo, "config/riding")),
           "load repository common riding catalog");
    const auto blue_dragon = world.pet_defs().find(1747);
    const auto goblin = world.pet_defs().find(2004);
    expect(blue_dragon != world.pet_defs().end() &&
               blue_dragon->second.heal_per_two_seconds == 100 && !blue_dragon->second.auto_pickup,
           "Blue Dragon gameplay configuration is available");
    expect(goblin != world.pet_defs().end() && goblin->second.heal_per_two_seconds == 100 &&
               goblin->second.auto_pickup,
           "Goblin gameplay configuration enables automatic pickup");
    const auto blue_dragon_item = world.item_defs().find(1747);
    const auto goblin_item = world.item_defs().find(2004);
    expect(blue_dragon_item != world.item_defs().end() && blue_dragon_item->second.is_pet &&
               goblin_item != world.item_defs().end() && goblin_item->second.is_pet,
           "test pets are recognized as wearable pet items");
    expect(world.riding_defs().size() == 16, "load common and Battle Riding definitions");
    const auto white_tiger_ride = world.riding_defs().find(30000);
    const auto white_tiger_item = world.item_defs().find(30000);
    expect(white_tiger_ride != world.riding_defs().end() &&
               white_tiger_ride->second.duration_seconds == 604800 &&
               white_tiger_ride->second.movement_speed_percent == 75 &&
               white_tiger_item != world.item_defs().end() && white_tiger_item->second.is_ride &&
               openkal::server::wear_kind_of(white_tiger_item->second) ==
                   openkal::server::WearKind::Ride &&
               openkal::server::persist_slot_of(openkal::server::WearKind::Ride) == 5,
           "common riding item has seven-day duration and an exclusive slot");
    const auto battle_horse = world.riding_defs().find(30100);
    const auto battle_horse_item = world.item_defs().find(30100);
    expect(battle_horse != world.riding_defs().end() &&
               battle_horse->second.kind == openkal::protocol::RidingKind::Battle &&
               battle_horse->second.initial_grade == openkal::protocol::BattleRidingGrade::Normal &&
               battle_horse->second.duration_seconds == 604800 &&
               battle_horse_item != world.item_defs().end() && battle_horse_item->second.is_ride,
           "Battle Riding horse is distinct and reuses the ride registration slot");
    constexpr std::array<int, 13> kBattleRidingMaterials{
        30300, 30301, 30302, 30303, 30310, 30311, 30312, 30313, 30320, 41321, 41322, 41323, 30330};
    for (const int index : kBattleRidingMaterials) {
        const auto material = world.item_defs().find(index);
        expect(material != world.item_defs().end() && material->second.stackable &&
                   !material->second.wearable,
               "Battle Riding materials are stackable");
    }
    ShopCatalog repository_catalog;
    expect(repository_catalog.load_dir(openkal::platform::join_path(repo, "config/server/shops"),
                                       world.item_defs()),
           "validate repository shop definitions against item buy prices");
    const auto* gae_yoo = repository_catalog.find(3);
    expect(gae_yoo && gae_yoo->items.size() == 18 && gae_yoo->items.front() == 23000 &&
               gae_yoo->items.back() == 23021,
           "Gae-Yoo adds the Revision Hammer to the existing assortment");
    const auto hammer = world.item_defs().find(23021);
    const auto* goo_deuk = repository_catalog.find(1);
    expect(hammer != world.item_defs().end() && hammer->second.buy == 750 &&
               hammer->second.sell == 400 && hammer->second.stackable && goo_deuk &&
               goo_deuk->items.back() == 23021,
           "normal merchants sell stackable Revision Hammers for 750 with a resale value of 400");
    const auto* shaman_pet = repository_catalog.find(3022);
    expect(shaman_pet && shaman_pet->items.size() == 8 && shaman_pet->items.front() == 1747 &&
               shaman_pet->items.back() == 1754,
           "Shaman Pet offers all purchasable standard pets");
    const auto* artisan = repository_catalog.find(60002);
    expect(artisan && artisan->skin_catalog, "load the curated Skin Artisan catalog");
    if (artisan) {
        for (const int index : {25303, 25313, 25364}) {
            expect(world.item_defs().count(index) == 1,
                   "excluded skins retain their existing item definitions");
            expect(std::find(artisan->items.begin(), artisan->items.end(), index) ==
                           artisan->items.end() &&
                       artisan->prices.count(index) == 0,
                   "arrow, broken bow and duplicate Frost Feather Bow are not offered");
        }
        expect(std::count(artisan->items.begin(), artisan->items.end(), 25355) == 1 &&
                   artisan->prices.at(25355) == 2,
               "one Frost Feather Bow remains available for two tickets");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
