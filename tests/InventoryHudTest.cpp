/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BootShell.h"
#include "openkal/client/DragonHud.h"
#include "openkal/client/Inventory.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/SkillBar.h"
#include "openkal/client/SkillWindow.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/server/DragonFusing.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/Qigong.h"
#include "openkal/server/WeaponEnchantment.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/terrain/GbModel.h"
#include "test_util.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    ItemStack item(int index, int count, std::uint32_t iid) {
        ItemStack stack;
        stack.index = index;
        stack.count = count;
        stack.iid = iid;
        return stack;
    }

    void verify_cosmetic_skins() {
        using openkal::protocol::SkinSlot;
        const auto root =
            std::filesystem::temp_directory_path() / ("openkal_skin_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root / "items");
        std::filesystem::create_directories(root / "Model/Clothes");
        const char* definitions[] = {
            R"({"index":9010,"class":["weapon","sword2h"],"wear":1,"action":[1, 1]})",
            R"({"index":9011,"class":["defense","upperarmor"],"wear":1,"action":[1, 1]})",
            R"({"index":9012,"class":["skin","helmet"],"wear":1,"action":[1, 1],"limit":{"class":"knight"}})",
            R"({"index":9013,"class":["skin","boots"],"wear":1,"action":[14, 1]})",
            R"({"index":9014,"class":["skin","body"],"wear":1,"action":[7, 1]})",
            R"({"index":9015,"class":["skin","weapon"],"skin":{"weapon_type":"sword2h"},"wear":1,"action":[40, 1]})",
            R"({"index":9016,"class":["skin","helmet"],"skin":{"costume_mesh":true},"wear":1,"action":[6, 1]})",
            R"({"index":9017,"class":["defense","boots"],"wear":1,"action":[1, 1]})",
            R"({"index":9018,"class":["skin","face"],"wear":1,"action":[0, 2]})",
            R"({"index":9019,"class":["skin","body"],"skin":{"keep_hair":true},"wear":1,"action":[7, 1]})",
            R"({"index":9020,"class":["skin","body"],"skin":{"replace_face":true},"wear":1,"action":[7, 1]})",
            R"({"index":9021,"class":["weapon","sword"],"wear":1,"action":[1, 1]})",
            R"({"index":9022,"class":["defense","shield"],"wear":1,"action":[1, 1]})",
            R"({"index":9023,"class":["skin","shield"],"wear":1,"action":[6, 1]})"};
        for (int i = 0; i < 14; ++i) {
            std::ofstream(root / "items" / (std::to_string(i) + ".json")) << definitions[i];
        }
        ItemDb catalog;
        expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                            root.string(),
                            (root / "items").string()),
               "load project-defined cosmetic fixtures");
        const auto* two_hand_skin = catalog.find(9015);
        ItemDef one_hand = *catalog.find(9010);
        one_hand.set_class_small("sword");
        expect(two_hand_skin->skin_matches_weapon(*catalog.find(9010)) &&
                   !two_hand_skin->skin_matches_weapon(one_hand) &&
                   !two_hand_skin->skin_matches_weapon(*catalog.find(9011)),
               "weapon skins require matching actual weapon types");
        ItemDef unknown_skin = *two_hand_skin;
        unknown_skin.set_skin_weapon_type("");
        expect(!unknown_skin.skin_matches_weapon(one_hand),
               "unclassified Knight weapon skins fail closed");
        unknown_skin.set_skin_weapon_type("invalid");
        expect(!unknown_skin.skin_matches_weapon(one_hand), "invalid weapon type fails closed");
        for (const char* part : {"s", "g", "p", "a", "f", "h"}) {
            std::ofstream(root / "Model/Clothes" / (std::string("Cw_0_") + part + "01.gb"));
        }
        std::ofstream(root / "Model/Clothes/Cw_1_a01.gb");
        std::ofstream(root / "Model/Clothes/Cw_7_c01.gb");
        std::ofstream(root / "Model/Clothes/Cw_6_c01.gb");
        std::ofstream(root / "Model/Clothes/Cw_1_s01.gb");
        std::ofstream(root / "Model/Clothes/Cw_14_s01.gb");
        std::ofstream(root / "Model/Clothes/Cw_0_f02.gb");
        Wear wear;
        wear.apply_item(*catalog.find(9010));
        wear.apply_item(*catalog.find(9011));
        wear.apply_item(*catalog.find(9017));
        wear.apply_item(*catalog.find(9012));
        wear.apply_item(*catalog.find(9013));
        wear.apply_item(*catalog.find(9015));
        expect(wear.skin_index(SkinSlot::Helmet) == 9012 &&
                   wear.skin_index(SkinSlot::Boots) == 9013,
               "individual skins coexist independently");
        wear.apply_item(*catalog.find(9018));
        const auto individual_paths =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(individual_paths.size() == 6 &&
                   individual_paths[0].find("Cw_14_s01.gb") != std::string::npos &&
                   wear.cloth_index(ClothPart::Boots) == 9017,
               "boot skin replaces the visible model and preserves the actual boots");
        expect(individual_paths.size() == 6 &&
                   individual_paths[4].find("Cw_0_f02.gb") != std::string::npos,
               "face skin can select a normal character face from model group zero");
        wear.apply_item(*catalog.find(9014));
        expect(wear.has_full_body_skin() && wear.skin_index(SkinSlot::Helmet) == 0 &&
                   wear.skin_index(SkinSlot::Boots) == 0 &&
                   wear.skin_index(SkinSlot::Weapon) == 9015,
               "whole-body skin replaces body cosmetics and preserves weapon skin");
        expect(wear.weapon_index() == 9010 && wear.cloth_index(ClothPart::Upper) == 9011,
               "skin appearance never replaces combat equipment");
        const auto paths =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(paths.size() == 2 && paths.front().find("Cw_7_c01.gb") != std::string::npos &&
                   paths.back().find("Cw_0_f01.gb") != std::string::npos,
               "whole-body mesh supplies hair while retaining the normal face by default");
        wear.apply_item(*catalog.find(9019));
        const auto keep_hair =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(catalog.find(9019)->skin_keeps_hair() && keep_hair.size() == 3 &&
                   keep_hair.back().find("Cw_0_h01.gb") != std::string::npos,
               "explicit hair exception keeps normal hair for a custom costume");
        wear.apply_item(*catalog.find(9020));
        const auto own_head =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(catalog.find(9020)->skin_replaces_face() && own_head.size() == 1 &&
                   own_head.front().find("Cw_7_c01.gb") != std::string::npos,
               "a complete costume head replaces both normal hair and face");
        std::filesystem::remove(root / "Model/Clothes/Cw_7_c01.gb");
        const auto fallback =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(fallback.size() == 6 && fallback[3].find("Cw_1_a01.gb") != std::string::npos,
               "missing cosmetic restores the actual equipment appearance");
        wear.apply_item(*catalog.find(9012));
        expect(!wear.has_full_body_skin() && wear.skin_index(SkinSlot::Upper) == 0,
               "individual skin displaces the whole-body skin");
        const auto restored =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(restored.size() == 6 && restored[4].find("Cw_0_f01.gb") != std::string::npos &&
                   restored[5].find("Cw_0_h01.gb") != std::string::npos,
               "individual equipment restores the normal face instead of retaining body overrides");
        expect(!catalog.find(9012)->can_equip_by(1, 90), "skin class limit is enforced");
        wear.apply_item(*catalog.find(9014));
        wear.apply_item(*catalog.find(9016));
        const auto head_paths =
            EquipmentPaths::clothes_paths_for_wear(root.string(), Class::Knight, wear, catalog);
        expect(!wear.has_full_body_skin() && wear.skin_index(SkinSlot::Upper) == 0 &&
                   wear.skin_index(SkinSlot::Helmet) == 9016 && head_paths.size() == 6 &&
                   head_paths[3].find("Cw_1_a01.gb") != std::string::npos &&
                   head_paths[5].find("Cw_6_c01.gb") != std::string::npos,
               "head-only costume uses its costume mesh without hiding body clothing");
        wear.apply_item(*catalog.find(9013));
        expect(wear.skin_index(SkinSlot::Helmet) == 9016 &&
                   wear.skin_index(SkinSlot::Boots) == 9013,
               "head-only costume leaves the other body skin slots available");
        Bag bag;
        bag.clear();
        expect(bag.try_insert(9015, 1, catalog, 0, 11), "insert weapon skin fixture");
        Wear no_weapon;
        expect(!Bag::toggle_equip_slot(bag, 0, no_weapon, catalog, 0, 90).changed,
               "client rejects weapon skin without an actual weapon");
        no_weapon.apply_item(*catalog.find(9010));
        expect(Bag::toggle_equip_slot(bag, 0, no_weapon, catalog, 0, 90).changed,
               "client accepts the matching two-hand skin");
        no_weapon.apply_item(*two_hand_skin);
        bag.clear();
        expect(bag.try_insert(9021, 1, catalog, 0, 13), "insert actual one-hand sword");
        expect(!Bag::toggle_equip_slot(bag, 0, no_weapon, catalog, 0, 90).changed,
               "client rejects weapon type changes while an incompatible skin is worn");
        ItemDef one_hand_skin = *two_hand_skin;
        one_hand_skin.set_skin_weapon_type("sword");
        expect(!one_hand_skin.skin_matches_weapon(*catalog.find(9010)),
               "one-hand skin rejects an actual two-hand weapon");
        bag.clear();
        expect(bag.try_insert(9012, 1, catalog, 0, 12), "insert unequipped skin fixture");
        const auto request = Bag::toggle_equip_slot(bag, 0, wear, catalog, 0, 90);
        expect(request.changed && !bag.normal[0].equipped,
               "cosmetic equip waits for server acknowledgement");
        expect(bag.set_equipment_equipped(12, true, catalog) && bag.equipment[17].iid == 12 &&
                   bag.normal[0].count == 0,
               "cosmetic instance uses its dedicated storage cell");
        expect(bag.set_equipment_equipped(12, false, catalog) && bag.normal[0].iid == 12,
               "cosmetic removal preserves its exact instance");
        const std::uint16_t base[7]{};
        openkal::protocol::SkinIndices skins{};
        skins[3] = 9014;
        skins[0] = 9015;
        auto frame = openkal::protocol::CreatePlayer::frame(
            1, "Skin", 0, 1, 2, 3, 0, 0, base, 1, 1, skins, 9016);
        openkal::protocol::CreatePlayerFields decoded;
        expect(
            openkal::protocol::CreatePlayer::parse(frame.data() + 3, frame.size() - 3, decoded) &&
                decoded.skins == skins && decoded.shield == 9016,
            "appearance snapshot round-trips cosmetic slots");
        expect(!openkal::protocol::CreatePlayer::parse(frame.data() + 3, frame.size() - 4, decoded),
               "truncated cosmetic extension is rejected");
        frame = openkal::protocol::CreatePlayer::frame(1, "Skin", 0, 1, 2, 3, 0, 0, base, 1, 1);
        expect(
            openkal::protocol::CreatePlayer::parse(frame.data() + 3, frame.size() - 3, decoded) &&
                decoded.skins == openkal::protocol::SkinIndices{} && decoded.shield == 0,
            "legacy snapshot clears previous cosmetics");
        openkal::server::CharacterRecord character;
        character.id = 42;
        character.name = "Owned selection fixture";
        std::vector<openkal::server::ItemRecord> records;
        std::vector<std::uint16_t> expected_selection;
        for (int index = 9400; index < 9412; ++index) {
            openkal::server::ItemRecord row;
            row.index = index;
            row.count = 1;
            row.slot = 2;
            records.push_back(row);
            expected_selection.push_back(static_cast<std::uint16_t>(index));
        }
        for (const auto& entry : {std::pair<int, int>{9010, 1},
                                  {9011, 2},
                                  {9014, 6},
                                  {9015, 6},
                                  {9023, 6},
                                  {9022, 2}}) {
            openkal::server::ItemRecord row;
            row.index = entry.first;
            row.count = 1;
            row.slot = entry.second;
            records.push_back(row);
            expected_selection.push_back(static_cast<std::uint16_t>(row.index));
        }
        auto bag_row = records.back();
        bag_row.index = 9013;
        bag_row.slot = 0;
        records.insert(records.begin(), bag_row);
        auto empty_row = records.back();
        empty_row.count = 0;
        records.insert(records.begin(), empty_row);
        const auto selection = BootShell::char_from_record(character, records);
        expect(selection.wear == expected_selection,
               "character selection retains late equipment and skins beyond thirteen rows");
        openkal::protocol::PlayerInfoMsg selection_message;
        selection_message.characters.push_back(selection);
        openkal::protocol::PlayerInfoMsg decoded_selection;
        expect(openkal::protocol::PlayerInfo::parse(
                   openkal::protocol::PlayerInfo::frame(selection_message), decoded_selection) &&
                   decoded_selection.characters.size() == 1 &&
                   decoded_selection.characters.front().wear == expected_selection,
               "character selection protocol round-trips all worn definitions");
        if (!decoded_selection.characters.empty()) {
            const auto preview =
                Wear::from_item_indices(catalog, decoded_selection.characters.front().wear);
            expect(preview.shield_index() == 9022 && preview.weapon_index() == 9010 &&
                       preview.skin_index(SkinSlot::Shield) == 9023 &&
                       preview.skin_index(SkinSlot::Weapon) == 9015 && preview.has_full_body_skin(),
                   "selection preview keeps the actual shield with whole-body and weapon/shield "
                   "skins");
        }
        const auto repeated_row = records.back();
        records.assign(256, repeated_row);
        const auto bounded = BootShell::char_from_record(character, records);
        expect(bounded.wear.size() == 255,
               "selection wear remains within the protocol byte-count limit");
        selection_message.characters = {bounded};
        expect(!openkal::protocol::PlayerInfo::frame(selection_message).empty(),
               "maximum selection wear count can still be encoded");
        std::filesystem::remove_all(root);
    }

    void verify_item_definition_flags() {
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_item_flags_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        const char* definitions[] = {
            R"({"index":91001,"class":["general","etc"],"stackable":1,"pay":1})",
            R"({"index":91002,"class":["quest","etc"],"stackable":1})",
            R"({"index":91003,"class":["weapon","sword"],"wear":1,"stackable":1})",
            R"({"index":91004,"class":["general","etc"],"stackable":0})",
            R"({"index":91005,"class":["general","etc"]})"};
        for (int i = 0; i < 5; ++i) {
            std::ofstream out(root / (std::to_string(i) + ".json"));
            out << definitions[i];
            expect(static_cast<bool>(out), "write cleaned item definition fixture");
        }
        ItemDb catalog;
        expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                            OPENKAL_TEST_RESOURCE_ROOT,
                            root.string()),
               "client loads cleaned item definitions");
        openkal::server::GameWorld world;
        expect(world.load_items_json_dir(root.string()), "server loads cleaned item definitions");
        for (int index = 91001; index <= 91005; ++index) {
            const auto* client = catalog.find(index);
            const auto server = world.item_defs().find(index);
            expect(client && server != world.item_defs().end(), "both loaders retain each item");
            if (!client || server == world.item_defs().end()) {
                continue;
            }
            const bool stackable = index <= 91003;
            expect(client->stackable() == stackable && server->second.stackable == stackable,
                   "both loaders read stackable and default it to false");
            expect(client->is_stackable() == (index <= 91002),
                   "wearable items never merge even when marked stackable");
            expect(client->is_quest() == (index == 91002) &&
                       (server->second.class_big == "quest") == (index == 91002),
                   "item class selects the quest tab without country metadata");
            expect(client->can_equip() == (index == 91003) &&
                       server->second.wearable == (index == 91003),
                   "equipment classification works without country metadata");
            expect(server->second.pay == (index == 91001),
                   "server retains the pay flag used by trash protection");
        }
        std::filesystem::remove_all(root);
    }

    void verify_polishing_drop(Window& win, ItemDb& catalog) {
        const auto* stone = catalog.find(openkal::server::kPolishingStoneIndex);
        expect(stone && stone->name() == "Polishing Stone" && stone->stackable() &&
                   stone->image() == "shop015" &&
                   stone->description() == "When it used on an item,#nitem won't be destroyed even "
                                           "if it failed to enchant with talisman.",
               "Polishing Stone keeps its ID with the image and description of legacy item 481");
        Wear wear;
        win.set_wear(&wear);
        win.set_open(true);
        win.set_tab(Tab::Normal);
        win.set_register_page(0);
        win.set_scale(1);
        win.center_on_screen(1100, 1100);
        for (int target : {1, 7}) {
            Bag bag;
            bag.normal.resize(kBagSlots);
            bag.normal[0] = item(openkal::server::kPolishingStoneIndex, 2, 123);
            bag.normal[1] = item(target, 1, 124);
            float x, y, w, h;
            expect(win.grid_slot_rect(1, x, y, w, h), "polishing target geometry");
            win.set_drag_slot(0);
            expect(win.handle_release(x + w / 2, y + h / 2, bag, wear) == ClickResult::Enchanted &&
                       win.last_enchant_from_id() == 123 && win.last_enchant_to_id() == 124 &&
                       bag.normal[0].count == 2 && bag.normal[1].iid == 124,
                   "dragging a stone sends the two instance IDs without optimistic mutation");
        }
        win.set_wear(nullptr);
    }

    void verify_revision_hammer_drop(Window& win, ItemDb& catalog) {
        const auto* hammer = catalog.find(openkal::server::kRevisionHammerIndex);
        expect(hammer && hammer->stackable() && hammer->buy() == 750 && hammer->sell() == 400 &&
                   hammer->image() == "revisionHammer",
               "Revision Hammer catalog has its prices and dedicated icon");
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_hammer_drop_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        std::filesystem::copy_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                       "/config/items/general/etc/23021_Revision_Hammer.json",
                                   root / "23021.json",
                                   std::filesystem::copy_options::overwrite_existing);
        const char* targets[] = {
            R"({"index":91010,"class":["weapon","sword"],"wear":1,"level":40})",
            R"({"index":91011,"class":["defense","upperarmor"],"wear":1,"level":40})",
            R"({"index":91012,"class":["ornament","ring"],"wear":1})",
            R"({"index":91013,"class":["ornament","trinket"],"wear":1,"level":40})"};
        for (int i = 0; i < 4; ++i) {
            std::ofstream out(root / (std::to_string(91010 + i) + ".json"));
            out << targets[i];
        }
        ItemDb synthetic;
        expect(synthetic.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                              OPENKAL_TEST_RESOURCE_ROOT,
                              root.string()),
               "load independent conversion target fixtures");
        Wear wear;
        win.set_wear(&wear);
        win.set_catalog(&synthetic);
        win.set_open(true);
        win.set_tab(Tab::Normal);
        win.set_register_page(0);
        win.set_scale(1);
        win.center_on_screen(1100, 1100);
        for (int target = 91010; target <= 91013; ++target) {
            for (int state = 0; state < 3; ++state) {
                Bag bag;
                bag.normal.resize(kBagSlots);
                bag.normal[0] = item(openkal::server::kRevisionHammerIndex, 2, 123);
                bag.normal[1] = item(target, state == 2 ? 2 : 1, 124);
                bag.normal[1].equipped = state == 1;
                float x, y, w, h;
                expect(win.grid_slot_rect(1, x, y, w, h), "hammer target geometry");
                win.set_drag_slot(0);
                const auto result = win.handle_release(x + w / 2, y + h / 2, bag, wear);
                expect((result == ClickResult::Enchanted) == (state == 0),
                       "hammer drag only requests conversion for single unequipped equipment");
                if (state == 0) {
                    expect(win.last_enchant_from_id() == 123 && win.last_enchant_to_id() == 124 &&
                               bag.normal[0].count == 2 && bag.normal[1].iid == 124,
                           "conversion carries instance IDs without optimistic destruction");
                }
            }
        }
        win.set_wear(nullptr);
        win.set_catalog(&catalog);
        std::filesystem::remove_all(root);
    }

    void verify_enchantment_talisman_drop(Window& win, ItemDb& catalog) {
        const std::array<std::string, 5> numerals{"I", "II", "III", "IIII", "V"};
        for (int tier = 1; tier <= 5; ++tier) {
            const auto* talisman =
                catalog.find(openkal::server::kEnchantmentTalismanIndex + tier - 1);
            const auto& numeral = numerals[static_cast<std::size_t>(tier - 1)];
            expect(
                talisman && talisman->stackable() &&
                    talisman->name() == "Talisman of Enchantment " + numeral &&
                    talisman->image() == "talismanOfEnchantment" + numeral,
                "all five talismans have distinct IDs, Roman names and shared-art icon variants");
        }
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_enchantment_drop_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        for (int tier = 1; tier <= 5; ++tier) {
            const int index = openkal::server::kEnchantmentTalismanIndex + tier - 1;
            const auto suffix =
                tier == 1 ? std::string{} : "_" + numerals[static_cast<std::size_t>(tier - 1)];
            std::filesystem::copy_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                           "/config/items/general/etc/" + std::to_string(index) +
                                           "_Talisman_of_Enchantment" + suffix + ".json",
                                       root / (std::to_string(index) + ".json"),
                                       std::filesystem::copy_options::overwrite_existing);
        }
        const char* targets[] = {
            R"({"index":91100,"class":["weapon","sword"],"wear":1,"level":37})",
            R"({"index":91101,"class":["weapon","sword2h"],"wear":1,"level":40})",
            R"({"index":91102,"class":["weapon","wand"],"wear":1,"level":40})",
            R"({"index":91103,"class":["weapon","bow"],"wear":1,"level":40})",
            R"({"index":91104,"class":["weapon","dagger"],"wear":1,"level":40})",
            R"({"index":91105,"class":["defense","upperarmor"],"wear":1,"level":40})",
            R"({"index":91106,"class":["weapon","sword"],"wear":1})",
            R"({"index":91107,"class":["weapon","sword"],"wear":1,"level":40})",
            R"({"index":91108,"class":["weapon","sword"],"wear":1,"level":39})",
            R"({"index":91109,"class":["weapon","sword"],"wear":1,"level":49})",
            R"({"index":91110,"class":["weapon","sword"],"wear":1,"level":50})",
            R"({"index":91111,"class":["weapon","sword"],"wear":1,"level":59})",
            R"({"index":91112,"class":["weapon","sword"],"wear":1,"level":60})",
            R"({"index":91113,"class":["weapon","sword"],"wear":1,"level":69})",
            R"({"index":91114,"class":["weapon","sword"],"wear":1,"level":70})",
            R"({"index":91115,"class":["weapon","sword"],"wear":1,"level":79})",
            R"({"index":91116,"class":["weapon","sword"],"wear":1,"level":80})",
            R"({"index":91117,"class":["weapon","sword"],"wear":1,"level":90})"};
        for (int i = 0; i < 18; ++i) {
            std::ofstream out(root / (std::to_string(91100 + i) + ".json"));
            out << targets[i];
        }
        ItemDb synthetic;
        expect(synthetic.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                              OPENKAL_TEST_RESOURCE_ROOT,
                              root.string()),
               "load independent enchantment target fixtures");
        Wear wear;
        win.set_wear(&wear);
        win.set_catalog(&synthetic);
        win.set_open(true);
        win.set_tab(Tab::Normal);
        win.set_register_page(0);
        win.set_scale(1);
        win.center_on_screen(1100, 1100);
        constexpr std::array<int, 18> expected_tier{
            0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 2, 2, 3, 3, 4, 4, 5, 5};
        for (int tier = 1; tier <= 5; ++tier) {
            for (int target = 91100; target <= 91117; ++target) {
                for (int state = 0; state < 4; ++state) {
                    Bag bag;
                    bag.normal.resize(kBagSlots);
                    bag.normal[0] =
                        item(openkal::server::kEnchantmentTalismanIndex + tier - 1, 2, 323);
                    bag.normal[1] = item(target, state == 2 ? 2 : 1, 324);
                    bag.normal[0].equipped = state == 3;
                    bag.normal[1].equipped = state == 1;
                    bag.normal[1].x_attack = 20;
                    float x, y, w, h;
                    expect(win.grid_slot_rect(1, x, y, w, h), "enchantment target geometry");
                    win.set_drag_slot(0);
                    const auto result = win.handle_release(x + w / 2, y + h / 2, bag, wear);
                    const bool valid =
                        expected_tier[static_cast<std::size_t>(target - 91100)] == tier &&
                        state == 0;
                    expect((result == ClickResult::Enchanted) == valid,
                           "drag validates all five talisman bands, boundaries and target states");
                    if (valid) {
                        expect(win.last_enchant_from_id() == 323 &&
                                   win.last_enchant_to_id() == 324 && bag.normal[0].count == 2 &&
                                   bag.normal[1].x_attack == 20,
                               "drag sends instance IDs and waits for authoritative reroll values");
                    }
                }
            }
        }
        win.set_wear(nullptr);
        win.set_catalog(&catalog);
        std::filesystem::remove_all(root);
    }

    void verify_enchantment_catalog(ItemDb& catalog) {
        const auto* shot = catalog.find(openkal::server::Qigong::perforation_shot_index());
        const auto* scroll = catalog.find(openkal::server::DragonFusing::scroll_index());
        const auto* talisman = catalog.find(openkal::server::Qigong::talisman_index());
        expect(shot && shot->name() == "Perforation Shot" && shot->image() == "shop143" &&
                   shot->stackable(),
               "active Perforation Shot ID resolves to its stackable catalog item and old image");
        expect(scroll && scroll->name() == "Dragon Spirit Scroll" && scroll->image() == "shop134" &&
                   scroll->stackable(),
               "active Dragon Spirit Scroll ID resolves to its catalog item and old image");
        expect(talisman && talisman->name() == "Qigong Talisman" &&
                   talisman->image() == "shop142" && talisman->stackable(),
               "active Qigong Talisman ID resolves to its catalog item and old image");
    }

    void verify_medicine_catalog(ItemDb& catalog) {
        const int medicines[] = {23000, 23001, 23002, 23003};
        const int potions[] = {23004, 23005, 23006, 23007};
        const int hp[] = {100, 300, 500, 800};
        const int mp[] = {300, 500, 800, 1200};
        const char* sizes[] = {"Small", "Medium", "Large", "Huge"};
        for (int i = 0; i < 4; ++i) {
            const auto* medicine = catalog.find(medicines[i]);
            const auto* potion = catalog.find(potions[i]);
            expect(medicine && medicine->name() == std::string(sizes[i]) + " Medicine" &&
                       medicine->image() == std::string("medicine") + sizes[i] &&
                       medicine->refresh_hp() == hp[i] && medicine->refresh_mp() == 0 &&
                       medicine->is_consumable() && !medicine->auto_hp(),
                   "manual medicine sizes retain their IDs, names, images and healing");
            expect(potion && potion->name() == std::string(sizes[i]) + " Magic Potion" &&
                       potion->image() == std::string("magicPotion") + sizes[i] &&
                       potion->refresh_mp() == mp[i] && potion->refresh_hp() == 0 &&
                       potion->is_consumable() && !potion->auto_hp(),
                   "all four manual magic potion sizes have matching images and MP recovery");
        }
    }

    void verify_emotes() {
        using openkal::protocol::Emote;
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_emote_ui_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        for (int motion = 1; motion <= 20; ++motion) {
            const int shortcut = 2021 + motion;
            std::ofstream(root / (std::to_string(shortcut) + ".json"))
                << "{\"class\":-1,\"role\":\"action\",\"index\":" << shortcut
                << ",\"name\":\"Owned emote " << motion
                << "\",\"image\":\"owned\",\"type\":[\"active\",\"once\"]}";
        }
        Catalog catalog;
        expect(catalog.load(root.string()), "load owned emote UI definitions");
        UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                "/config/ui/skill_window.json"),
               "load skill window layout");
        for (int clazz = 0; clazz < 4; ++clazz) {
            PlayerSkills player;
            player.seed_for_class(clazz, 1, catalog);
            player.set_active(4);
            SkillBar bar;
            bar.set_catalog(&catalog);
            bar.set_player(&player);
            bar.set_player_clazz(clazz);
            bar.set_panel(0, 0, 792, 180, 1);
            float hx, hy, hw, hh, ax, ay, aw, ah;
            expect(bar.hotbar_rect(0, hx, hy, hw, hh) && bar.active_rect(ax, ay, aw, ah),
                   "resolve hotbar and active slot geometry");
            SkillWindow window;
            window.set_catalog(&catalog);
            window.set_player(&player);
            window.set_player_clazz(clazz);
            window.set_layout(&layout);
            window.sync_from_layout();
            window.set_pos(0, 0);
            window.set_open(true);
            window.set_tab(SkillTab::Action);
            for (int motion = 1; motion <= 20; ++motion) {
                const int shortcut = 2021 + motion;
                const auto* definition = catalog.find(clazz, shortcut);
                expect(definition && Catalog::is_castable(*definition) &&
                           Catalog::cast_kind_for(clazz, shortcut) == CastKind::Emote &&
                           Catalog::skill_bar_casts_on_click(clazz, shortcut) &&
                           Emote::motion_for_shortcut(shortcut) == motion,
                       "every shared emote is an immediate cast for each class");
                expect(bar.try_drop(hx + hw / 2, hy + hh / 2, shortcut) &&
                           player.hotbar_slot(0) == shortcut && player.active_index() == 4,
                       "binding an emote preserves the active combat skill");
                expect(!bar.try_drop(ax + aw / 2, ay + ah / 2, shortcut),
                       "direct emote drop onto the active slot is rejected");
                expect(bar.begin_press(hx + hw / 2, hy + hh / 2) &&
                           bar.handle_release(hx + hw / 2, hy + hh / 2) == BarResult::Cast &&
                           bar.last_cast_index() == shortcut && player.active_index() == 4,
                       "emote hotbar click requests playback without changing active skill");
                player.activate_hotbar_slot(0);
                expect(player.active_index() == 4,
                       "generic hotbar activation cannot select emotes");
                expect(bar.begin_press(hx + hw / 2, hy + hh / 2), "start emote hotbar drag");
                openkal::engine::FrameInput pointer;
                pointer.mouse_left = true;
                pointer.mouse_x = ax + aw / 2;
                pointer.mouse_y = ay + ah / 2;
                bar.update_pointer(pointer);
                (void) bar.handle_release(pointer.mouse_x, pointer.mouse_y);
                expect(player.active_index() == 4,
                       "hotbar drag cannot put an emote in active slot");
                const int slot = motion - 1;
                const float x = 34 + (slot % 6) * 68 + 16;
                const float y = 140 + (slot / 6) * 66 + 16;
                expect(window.handle_click(x, y) == SkillClickResult::DragBegin &&
                           window.click_cast_on_release(x, y) == shortcut,
                       "Action-tab click requests the matching emote");
                pointer.mouse_x = x + 20;
                pointer.mouse_y = y;
                window.update_pointer(pointer);
                expect(window.click_cast_on_release(x, y) == -1,
                       "dragging an emote out and back never casts it");
                window.set_drag_skill_index(-1);
            }
            auto saved = std::array<int, PlayerSkills::kHotbarSlots>{};
            saved.fill(-1);
            saved[0] = 2022;
            player.apply_shortcut(2022, saved);
            expect(player.active_index() == -1 && player.hotbar_slot(0) == 2022,
                   "old saved active emote is cleared while its hotbar binding survives");
            player.ensure_default_hotbar(catalog, clazz);
            expect(player.active_index() == -1, "default hotbar never selects an emote");
        }
        std::filesystem::remove_all(root);
    }

    void verify_social_actions() {
        using openkal::test::expect;
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_actions_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        const char* names[] = {"Run", "Rest", "Guild", "Party", "Trade", "Wisper", "T&S relation"};
        const int ids[] = {0, 11, 2001, 2002, 2003, 2004, 2005};
        for (int i = 0; i < 7; ++i) {
            std::ofstream(root / (std::to_string(ids[i]) + ".json"))
                << "{\"role\":\"action\",\"index\":" << ids[i] << ",\"name\":\"" << names[i]
                << "\",\"image\":\"owned.bmp\",\"key\":\"action-owned-" << ids[i] << "\"}";
        }
        Catalog catalog;
        expect(catalog.load(root.string()), "load owned action fixtures");
        PlayerSkills player;
        player.set_multiplayer(true);
        player.seed_for_class(0, 1, catalog);
        player.set_active(4);
        UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                "/config/ui/skill_window.json"),
               "load action layout");
        SkillWindow window;
        window.set_catalog(&catalog);
        window.set_player(&player);
        window.set_layout(&layout);
        window.sync_from_layout();
        window.set_pos(0, 0);
        window.set_open(true);
        window.set_tab(SkillTab::Action);
        for (bool multiplayer : {false, true}) {
            window.set_multiplayer(multiplayer);
            std::vector<int> visible;
            for (int slot = 0; slot < 30; ++slot) {
                const float x = 34 + (slot % 6) * 68 + 16, y = 140 + (slot / 6) * 66 + 16;
                if (window.handle_click(x, y) == SkillClickResult::DragBegin) {
                    const int id = window.drag_skill_index();
                    visible.push_back(id);
                    expect(window.click_cast_on_release(x, y) == id,
                           "action click executes without drag");
                    window.set_drag_skill_index(-1);
                }
            }
            expect(visible.size() == (multiplayer ? 6u : 2u),
                   (std::string(multiplayer ? "multiplayer visible action count: "
                                            : "single player visible action count: ") +
                    std::to_string(visible.size()))
                       .c_str());
        }
        SkillBar bar;
        bar.set_catalog(&catalog);
        bar.set_player(&player);
        bar.set_panel(0, 0, 792, 180, 1);
        float x, y, w, h, ax, ay, aw, ah;
        expect(bar.hotbar_rect(0, x, y, w, h) && bar.active_rect(ax, ay, aw, ah),
               "resolve action bar slots");
        for (int id : {0, 11, 2001, 2002, 2003, 2004}) {
            const auto* definition = catalog.find(0, id);
            const bool active_action = social_action_uses_active_slot(social_action(*definition));
            const int expected_active = active_action ? id : 4;
            expect(bar.try_drop(x + w / 2, y + h / 2, id) &&
                       player.active_index() == expected_active,
                   "only Party and Trade become active when bound to the hotbar");
            player.set_active(id);
            expect(player.active_index() == expected_active,
                   "Party and Trade support direct active selection");
            expect(bar.try_drop(ax + aw / 2, ay + ah / 2, id) == active_action,
                   "active-slot drop accepts Party and Trade and rejects immediate actions");
            expect(bar.begin_press(x + w / 2, y + h / 2) &&
                       bar.handle_release(x + w / 2, y + h / 2) ==
                           (active_action ? BarResult::SetActive : BarResult::Cast) &&
                       player.active_index() == expected_active,
                   "hotbar click selects Party and Trade instead of executing them");
            if (active_action) {
                player.set_active(4);
                expect(bar.begin_press(x + w / 2, y + h / 2), "start targeted social action drag");
                openkal::engine::FrameInput pointer;
                pointer.mouse_left = true;
                pointer.mouse_x = ax + aw / 2;
                pointer.mouse_y = ay + ah / 2;
                bar.update_pointer(pointer);
                (void) bar.handle_release(pointer.mouse_x, pointer.mouse_y);
                expect(player.active_index() == id,
                       "hotbar drag places Party and Trade in the active slot");
            }
            auto slots = std::array<int, PlayerSkills::kHotbarSlots>{};
            slots.fill(-1);
            slots[0] = id;
            player.apply_shortcut(id, slots);
            expect(player.active_index() == (active_action ? id : -1) &&
                       player.hotbar_slot(0) == id,
                   "saved Party and Trade active selections survive shortcut restoration");
            player.apply_server_info({}, catalog, 0);
            expect(player.active_index() == (active_action ? id : -1),
                   "skill synchronization preserves targeted social action selection");
            player.set_active(4);
        }
        player.set_multiplayer(false);
        player.seed_for_class(0, 1, catalog);
        for (int id : {2002, 2003}) {
            auto slots = std::array<int, PlayerSkills::kHotbarSlots>{};
            slots.fill(-1);
            slots[0] = id;
            player.apply_shortcut(id, slots);
            expect(
                !player.is_learned(id) && player.active_index() == -1 &&
                    player.hotbar_slot(0) == -1,
                "single player still removes Party and Trade from saved active and hotbar slots");
        }
        std::filesystem::remove_all(root);
    }

    void verify_auto_hp_persistence(ItemDb& catalog) {
        using openkal::test::expect;
        const auto path = std::filesystem::temp_directory_path() /
                          ("openkal-auto-hp-ui-" + std::to_string(::getpid()) + ".json");
        Catalog skills;
        PlayerSkills player;
        Bag bag;
        bag.normal.resize(kBagSlots);
        SkillBar bar;
        bar.set_catalog(&skills);
        bar.set_player(&player);
        bar.set_items(&catalog);
        bar.set_bag(&bag);
        bar.set_panel(0, 0, 792, 180, 1);
        float x, y, w, h;
        expect(bar.auto_hp_rect(x, y, w, h), "persistence slot geometry");
        const float mx = x + w / 2, my = y + h / 2;
        expect(bar.try_drop(mx, my, openkal::protocol::shortcut_pack_item(23010)) &&
                   bar.take_auto_hp_dirty() && !bar.take_auto_hp_dirty(),
               "binding marks preferences dirty once");
        expect(bar.begin_press(mx, my), "first persistence click");
        (void) bar.handle_release(mx, my);
        expect(!bar.take_auto_hp_dirty(), "single click does not request a save");
        expect(bar.begin_press(mx, my), "second persistence click");
        (void) bar.handle_release(mx, my);
        expect(bar.auto_hp_percent() == 50 && bar.take_auto_hp_dirty(),
               "threshold change requests a save");

        UiSettings saved;
        saved.set_auto_hp(42, {bar.auto_hp_item_index(), bar.auto_hp_percent()});
        saved.set_auto_hp(43, {23011, 25});
        expect(saved.save(path.string()), "persist auto hp preferences");
        UiSettings loaded;
        expect(loaded.load(path.string()), "reload auto hp preferences");
        const auto& binding = loaded.auto_hp(42);
        SkillBar restarted;
        restarted.set_items(&catalog);
        restarted.set_bag(&bag);
        restarted.restore_auto_hp(binding.item_index, binding.percent);
        expect(restarted.auto_hp_item_index() == 23010 && restarted.auto_hp_percent() == 50 &&
                   !restarted.auto_hp_available() && !restarted.take_auto_hp_dirty(),
               "restart restores the binding and threshold even with an empty bag");
        bag.normal[3] = item(23010, 2, 900);
        expect(restarted.auto_hp_needed(499, 1000),
               "a new stack instance reactivates the restored binding");
        expect(loaded.auto_hp(43).item_index == 23011 && loaded.auto_hp(43).percent == 25 &&
                   loaded.auto_hp(44).item_index == 0 && loaded.auto_hp(44).percent == 75,
               "auto hp preferences are independent per character");
        for (int percent : {25, 50, 75, 100}) {
            restarted.restore_auto_hp(23011, percent);
            expect(restarted.auto_hp_percent() == percent, "restore every valid threshold");
        }
        for (int invalid : {0, -1, 23000, 23004, 999999}) {
            restarted.restore_auto_hp(invalid, 60);
            expect(restarted.auto_hp_item_index() == 0 && restarted.auto_hp_percent() == 75,
                   "invalid preferences cannot equip manual or unknown medicine");
        }

        expect(bar.begin_press(mx, my), "capture persisted binding removal");
        openkal::engine::FrameInput pointer;
        pointer.mouse_left = true;
        pointer.mouse_x = pointer.mouse_y = 1000;
        bar.update_pointer(pointer);
        (void) bar.handle_release(1000, 1000, true);
        expect(bar.take_auto_hp_dirty(), "removal requests a save");
        loaded.set_auto_hp(42, {bar.auto_hp_item_index(), bar.auto_hp_percent()});
        expect(loaded.save(path.string()) && saved.load(path.string()) &&
                   saved.auto_hp(42).item_index == 0 && saved.auto_hp(42).percent == 50 &&
                   saved.auto_hp(43).item_index == 23011,
               "removal remains cleared after restart without changing another character");
        {
            std::ofstream out(path);
            out << R"({"characters":{"42":{},"43":{"auto_hp":{"item_index":1e100,"percent":50.5}},"44":{"auto_hp":{"item_index":23010.5,"percent":"25"}}}})";
        }
        expect(saved.load(path.string()) && saved.auto_hp(42).item_index == 0 &&
                   saved.auto_hp(42).percent == 75 && saved.auto_hp(43).item_index == 0 &&
                   saved.auto_hp(43).percent == 75 && saved.auto_hp(44).item_index == 0 &&
                   saved.auto_hp(44).percent == 75,
               "old and malformed settings use safe defaults");
        std::filesystem::remove(path);
    }

    void verify_auto_hp(ItemDb& catalog) {
        using namespace openkal;
        UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                "/config/ui/dragon_hud.json"),
               "load auto hp layout");
        Catalog skills;
        PlayerSkills player;
        Bag bag;
        bag.normal.resize(kBagSlots);
        SkillBar bar;
        bar.set_catalog(&skills);
        bar.set_player(&player);
        bar.set_items(&catalog);
        bar.set_bag(&bag);
        bar.set_layout(&layout);
        for (float scale : {0.75f, 1.0f, 1.5f}) {
            bar.set_panel(20, 40, 792 * scale, 180 * scale, scale);
            float x, y, w, h;
            expect(bar.auto_hp_rect(x, y, w, h), "auto hp slot geometry");
            const float mx = x + w / 2, my = y + h / 2;
            expect(bar.hit_auto_hp(x - 3 * scale, my), "auto hp frame accepts pointer");
            expect(!bar.try_drop(mx, my, 0) &&
                       !bar.try_drop(mx, my, protocol::shortcut_pack_item(23000)),
                   "auto hp rejects skills and manual medicine");
            const int originals[] = {23000, 23001, 23002, 23003};
            const int amounts[] = {100, 300, 500, 800};
            for (int i = 0; i < 4; ++i) {
                const auto* def = catalog.find(23008 + i);
                const auto* original = catalog.find(originals[i]);
                expect(def && original && def->auto_hp() && def->is_consumable() &&
                           def->refresh_hp() == amounts[i] &&
                           def->cooltime_ms() == original->cooltime_ms() &&
                           def->buy() == original->buy() && def->sell() == original->sell() &&
                           def->effect() == original->effect(),
                       "auto medicine preserves the healing and economic values");
                bag.normal[0] = item(23008 + i, 3, 100 + i);
                const int binding = protocol::shortcut_pack_item(23008 + i);
                expect(bar.try_drop(mx, my, binding) && bar.auto_hp_available(),
                       "all four auto medicines bind without moving their stack");
                float hx, hy, hw, hh;
                expect(bar.hotbar_rect(0, hx, hy, hw, hh) &&
                           !bar.try_drop(hx + hw / 2, hy + hh / 2, binding),
                       "auto medicine cannot bind a manual hotkey");
                expect(!bag.try_consume_one(23008 + i, catalog).ok && bag.normal[0].count == 3,
                       "manual bag consumption cannot spend auto medicine");
            }
            const auto click = [&] {
                expect(bar.begin_press(mx, my), "auto hp captures click");
                expect(bar.handle_release(mx, my) == BarResult::Consumed,
                       "auto hp click never manually consumes medicine");
            };
            for (int threshold : {50, 25, 100, 75}) {
                click();
                const int previous = bar.auto_hp_percent();
                expect(previous != threshold, "single click keeps threshold");
                click();
                expect(bar.auto_hp_percent() == threshold, "double click cycles threshold");
                expect(bar.auto_hp_needed(threshold * 10 - 1, 1000) &&
                           !bar.auto_hp_needed(threshold * 10, 1000),
                       "selected threshold controls automatic use");
            }
            click();
            engine::FrameInput pointer;
            pointer.dt = 0.5f;
            bar.update_pointer(pointer);
            click();
            expect(bar.auto_hp_percent() == 75, "separate single clicks do not cycle");
            bar.update_pointer(pointer);
            expect(!bar.auto_hp_needed(0, 1000), "dead player never takes medicine");
            bar.set_item_refresh_cooldown(0.01f, 23000);
            expect(!bar.auto_hp_needed(1, 1000), "manual medicine shares the entire cooldown");
            bar.set_item_refresh_cooldown(1.0f, 23004);
            expect(!bar.auto_hp_needed(1, 1000), "mana medicine also shares cooldown");
            bar.set_item_refresh_cooldown(1.0f, 447);
            expect(bar.auto_hp_needed(1, 1000), "unrelated items do not block auto hp");
            bar.set_item_refresh_cooldown(0.0f);
            bag.normal[0] = {};
            expect(!bar.auto_hp_available() && !bar.auto_hp_needed(1, 1000),
                   "depleted stack stops automatic use");
            bag.normal[4] = item(23011, 2, 700);
            expect(bar.auto_hp_available(), "restocking another instance resumes auto hp");
            expect(bar.begin_press(mx, my), "capture auto hp drag");
            pointer.mouse_left = true;
            pointer.mouse_x = 1000;
            pointer.mouse_y = 1000;
            bar.update_pointer(pointer);
            expect(bar.handle_release(1000, 1000, true) == BarResult::Consumed &&
                       bar.auto_hp_item_index() == 0 && bag.normal[4].count == 2,
                   "dragging back unbinds without consuming or dropping medicine");
            bag.normal[4] = {};
        }
    }

    void capture_polishing(ItemDb& catalog, const std::string& path) {
        using namespace openkal;
        auto engine = std::unique_ptr<engine::IEngine>(engine::create_engine());
        if (!engine->init("Polishing protection verification", 640, 300)) {
            expect(false, "initialize polishing preview");
            return;
        }
        WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, font.find_system_sans(), 14), "load polishing preview font");
        std::vector<std::uint8_t> first, early, halfway, late, repeated;
        const auto frame = [&](double seconds, std::vector<std::uint8_t>& pixels) {
            expect(engine->begin_offscreen(640, 300, .07f, .08f, .10f), "begin polishing preview");
            font.draw(
                *engine, 24, 28, "Polishing Stone - one protected attempt", .9f, .9f, .95f, 1);
            for (int i = 0; i < 4; ++i) {
                const float x = 40.0f + i * 150.0f;
                const int index = i == 0 ? server::kPolishingStoneIndex : (i == 2 ? 7 : 1);
                const auto icon = catalog.icon_filename(index);
                engine->draw_ui_rect(x - 4, 74, 72, 72, .15f, .17f, .20f, 1);
                ui.draw_item_tex(icon, x, 78, 64, 64);
                if (i == 1 || i == 2) {
                    UiView::draw_enchant_protection(*engine, x, 78, 64, 64, seconds);
                }
                font.draw(*engine,
                          x - 12,
                          178,
                          i == 0   ? "Stone"
                          : i == 1 ? "Protected weapon"
                          : i == 2 ? "Protected armor"
                                   : "After attempt",
                          .85f,
                          .88f,
                          .95f,
                          1);
            }
            font.draw(*engine,
                      24,
                      235,
                      "A full light band sweeps top left to bottom right every 2 seconds.",
                      .65f,
                      .7f,
                      .8f,
                      1);
            expect(engine->end_offscreen_rgba(pixels), "read polishing preview");
        };
        frame(0, first);
        frame(0.625, early);
        frame(1, halfway);
        frame(1.375, late);
        frame(2, repeated);
        expect(!first.empty() && first != halfway, "diagonal band crosses the icon interior");
        expect(first == repeated, "diagonal sweep repeats exactly after two seconds");
        const auto changed = [&](const std::vector<std::uint8_t>& frame, int x, int y) {
            const auto offset = static_cast<std::size_t>((y * 640 + x) * 4);
            return frame.size() == first.size() && offset + 3 < frame.size() &&
                   (frame[offset] != first[offset] || frame[offset + 1] != first[offset + 1] ||
                    frame[offset + 2] != first[offset + 2]);
        };
        expect(changed(early, 203, 91) && !changed(late, 203, 91) && changed(late, 241, 129) &&
                   !changed(early, 241, 129),
               "sweep travels from the upper left to the lower right");
        expect(changed(halfway, 252, 79) && changed(halfway, 191, 140) &&
                   changed(halfway, 222, 110),
               "mid-sweep band spans the entire icon diagonal through its center");
        bool clipped = true;
        for (int y = 0; y < 300; ++y) {
            for (int x = 0; x < 640; ++x) {
                const bool protected_icon =
                    y >= 78 && y < 142 && ((x >= 190 && x < 254) || (x >= 340 && x < 404));
                if (!protected_icon && changed(halfway, x, y)) {
                    clipped = false;
                }
            }
        }
        expect(clipped, "protection light stays inside each icon rectangle");
        expect(engine->begin_offscreen(64, 64, 0, 0, 0), "begin scan transparency probe");
        UiView::draw_enchant_protection(*engine, 0, 0, 64, 64, 1);
        std::vector<std::uint8_t> gradient;
        expect(engine->end_offscreen_rgba(gradient) && gradient.size() == 64 * 64 * 4,
               "read scan transparency probe");
        if (gradient.size() == 64 * 64 * 4) {
            const auto blue = [&](int x) { return gradient[(32 * 64 + x) * 4 + 2]; };
            expect(
                blue(30) > blue(24) && blue(24) > blue(16) && blue(16) > blue(8) && blue(8) > 0 &&
                    blue(52) == 0 && gradient[2] == 0 && blue(8) > blue(0),
                "scan has a bright leading line and a long rear gradient fading to transparency");
        }
        std::ofstream out(path, std::ios::binary);
        out << "P6\n640 300\n255\n";
        for (std::size_t i = 0; i + 3 < halfway.size(); i += 4) {
            out.write(reinterpret_cast<const char*>(halfway.data() + i), 3);
        }
        expect(out.good(), "save polishing preview");
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }

    void capture_auto_hp(ItemDb& catalog, const std::string& path) {
        using namespace openkal;
        auto engine = std::unique_ptr<engine::IEngine>(engine::create_engine());
        if (!engine->init("Auto HP verification", 1240, 1000)) {
            expect(false, "initialize auto hp preview");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font, bold;
        expect(font.load(*engine, font.find_system_sans(), 12), "load auto hp font");
        expect(bold.load(*engine, bold.find_system_sans_bold(), 12), "load auto hp bold font");
        UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                "/config/ui/dragon_hud.json"),
               "load auto hp preview layout");
        Catalog skills;
        PlayerSkills player;
        Bag bag;
        bag.normal.resize(kBagSlots);
        SkillBar bar;
        bar.set_catalog(&skills);
        bar.set_player(&player);
        bar.set_items(&catalog);
        bar.set_bag(&bag);
        bar.set_layout(&layout);
        StatusWidgetData status;
        status.set_hp(370, 1000);
        status.set_mp(180, 500);
        DragonHudData data;
        data.set_exp(420, 1000);
        expect(engine->begin_offscreen(1240, 2180, .10f, .12f, .14f), "begin auto hp preview");
        std::vector<std::pair<int, int>> marker_pixels;
        float y = 10;
        for (float scale : {1.0f, 1.5f}) {
            int index = 23008;
            for (int percent : {75, 50, 25, 100}) {
                bag.normal[0] = item(index, 42, 100);
                bar.set_panel(20, y, 792 * scale, 180 * scale, scale);
                float sx, sy, sw, sh;
                expect(
                    bar.auto_hp_rect(sx, sy, sw, sh) &&
                        bar.try_drop(sx + sw / 2, sy + sh / 2, protocol::shortcut_pack_item(index)),
                    "bind each auto hp icon in preview");
                data.set_auto_hp(percent);
                DragonHud::draw(ui, *engine, font, bold, 20, y, scale, status, data, &layout);
                bar.draw(ui, &font);
                const int marker_x = percent == 100 ? 80 : percent == 50 ? 43 : 50;
                const int marker_y = percent == 100 ? 54 : 137 - 84 * percent / 100;
                marker_pixels.emplace_back(static_cast<int>(20 + marker_x * scale),
                                           static_cast<int>(y + (marker_y + 0.5f) * scale));
                y += 180 * scale + 20;
                ++index;
            }
        }
        bag.normal[0] = {};
        bar.set_panel(20, y, 792, 180, 1);
        data.set_auto_hp(0);
        DragonHud::draw(ui, *engine, font, bold, 20, y, 1, status, data, &layout);
        bar.draw(ui, &font);
        std::vector<std::uint8_t> rgba;
        expect(engine->end_offscreen_rgba(rgba) && rgba.size() == 1240 * 2180 * 4,
               "read auto hp preview");
        for (const auto& [px, py] : marker_pixels) {
            const auto offset = static_cast<std::size_t>((py * 1240 + px) * 4);
            expect(offset + 2 < rgba.size() && rgba[offset] > 140 && rgba[offset + 1] > 110 &&
                       rgba[offset] > rgba[offset + 2] + 50,
                   "rendered auto hp line is yellow at the selected threshold");
        }
        std::ofstream out(path, std::ios::binary);
        out << "P6\n1240 2180\n255\n";
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
        }
        expect(out.good(), "save auto hp preview");
        font.destroy(*engine);
        bold.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }

    std::pair<float, float> control(const Window& win, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "control exists");
        float x, y, w, h;
        UiView::widget_rect(
            *widget, nullptr, win.x(), win.y(), win.w(), win.h(), win.scale(), x, y, w, h);
        return {x + w / 2, y + h / 2};
    }

    void scroll_to(Window& win, Bag& bag, const UiDocument& doc, int row) {
        const auto* viewport =
            doc.find_widget(win.tab() == Tab::Normal ? "general_viewport" : "bag_viewport");
        openkal::engine::FrameInput input;
        input.mouse_x = win.x() + (viewport->x() + 30) * win.scale();
        input.mouse_y = win.y() + (viewport->y() + 30) * win.scale();
        input.mouse_scroll = 1000;
        win.update_pointer(input, bag);
        if (row > 0) {
            input.mouse_scroll = -static_cast<float>(row);
            win.update_pointer(input, bag);
        }
    }

    std::pair<float, float>
    collection_control(const Window& win, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "Collection control exists");
        const float scale =
            std::max(.25f,
                     std::min({win.scale(),
                               (1920 - win.w() - 88 * win.scale()) / (doc.window_w() + 80),
                               1080 / (doc.window_h() + 140)}));
        const float x = win.x() - (doc.window_w() + 40) * scale - 48 * win.scale();
        const float y = std::clamp(
            win.y(), 84 * scale, std::max(84 * scale, 1080 - (doc.window_h() + 56) * scale));
        float sx, sy, sw, sh;
        UiView::widget_rect(*widget,
                            nullptr,
                            x,
                            y,
                            doc.window_w() * scale,
                            doc.window_h() * scale,
                            scale,
                            sx,
                            sy,
                            sw,
                            sh);
        return {sx + sw / 2, sy + sh / 2};
    }

    void verify_sorting() {
        Bag bag;
        bag.normal.resize(kBagSlots);
        bag.quest.resize(kOtherSlots);
        bag.money = 84250;
        bag.normal[8] = item(90, 1, 501);
        bag.normal[8].equipped = true;
        bag.normal[8].prefix = 12;
        bag.normal[8].cur_end = 17;
        bag.normal[8].random_stat_value = 4;
        bag.normal[41] = item(1, 1, 502);
        bag.normal[72 + 3] = item(22, 1, 503);
        bag.normal[72 + 3].account_bound = true;
        bag.normal[108 + 5] = item(1, 20, 504);
        bag.normal[108 + 5].transform_level = 11;
        bag.quest[9] = item(300, 7, 900);
        const bool enabled[kMaxRegisters]{true, true, false, true, false, false};
        bag.sort_items(nullptr, enabled);
        expect(bag.normal[0].iid == 504 && bag.normal[1].iid == 502 && bag.normal[2].iid == 501,
               "sort by index and count across writable pages");
        expect(bag.normal[2].equipped && bag.normal[2].prefix == 12 &&
                   bag.normal[2].cur_end == 17 && bag.normal[2].random_stat_value == 4 &&
                   bag.normal[0].transform_level == 11,
               "sorting preserves instance metadata");
        expect(bag.normal[75].iid == 503 && bag.normal[75].account_bound,
               "expired page items stay in their exact slots");
        expect(bag.quest[9].iid == 900 && bag.money == 84250,
               "sorting only changes the normal bag");
        bag.sort_items(nullptr, enabled);
        expect(bag.normal[0].iid == 504 && bag.normal[2].iid == 501, "sorting is stable");
        Bag full;
        full.normal.resize(kBagSlots);
        for (int i = 0; i < 100; ++i) {
            const int slot = i < 72 ? i : i + 36;
            full.normal[slot] = item(100 - i, 1, static_cast<std::uint32_t>(i + 1));
        }
        full.sort_items(nullptr, enabled);
        expect(full.normal[71].index == 72 && full.normal[108].index == 73 &&
                   full.normal[135].index == 100 && full.normal[72].index == 0,
               "packing skips locked holes without dropping overflow");
    }

    void verify_input(Window& win, const UiDocument& doc) {
        Wear wear;
        win.set_wear(&wear);
        for (float requested : {0.75f, 1.0f, 1.5f}) {
            Bag bag;
            bag.clear();
            bag.normal[0] = item(90, 1, 41);
            bag.normal[5] = item(1, 1, 42);
            bag.normal[75] = item(22, 1, 43);
            win.set_scale(requested);
            win.center_on_screen(1920, 1080);
            const float scale = win.scale();
            win.set_open(true);
            win.set_tab(Tab::Normal);
            expect(win.x() - 40 * scale >= -.01f && win.y() - 84 * scale >= -.01f &&
                       win.x() + win.w() + 40 * scale <= 1920.01f &&
                       win.y() + win.h() + 56 * scale <= 1080.01f,
                   "entire inventory fits the minimum viewport even at enlarged HUD zoom");
            std::array<bool, kBagSlots> visited{};
            for (int row = 0; row <= 35; ++row) {
                scroll_to(win, bag, doc, row);
                for (int i = 0; i < kBagSlots; ++i) {
                    float x, y, w, h;
                    if (!win.grid_slot_rect(i, x, y, w, h)) {
                        continue;
                    }
                    visited[i] = true;
                    expect(win.grid_slot_at(x + w / 2, y + h / 2) == i,
                           "visible scrolled cells retain their absolute item identity");
                    const auto* viewport = doc.find_widget(
                        win.tab() == Tab::Normal ? "general_viewport" : "bag_viewport");
                    expect(y - 5 * scale >= win.y() + viewport->y() * scale - .01f &&
                               y + h + 5 * scale <=
                                   win.y() + (viewport->y() + viewport->h()) * scale + .01f,
                           "complete slot frames stay inside the scrolling viewport");
                }
            }
            expect(std::all_of(visited.begin(), visited.end(), [](bool seen) { return seen; }),
                   "scrolling reaches every one of the 216 General cells");
            float hidden_x, hidden_y, hidden_w, hidden_h;
            expect(!win.grid_slot_rect(0, hidden_x, hidden_y, hidden_w, hidden_h),
                   "offscreen cells have no interactive rectangle");
            openkal::engine::FrameInput outside;
            outside.mouse_scroll = 1000;
            win.update_pointer(outside, bag);
            expect(!win.grid_slot_rect(0, hidden_x, hidden_y, hidden_w, hidden_h),
                   "wheel input outside the viewport leaves inventory scrolling unchanged");
            win.set_tab(Tab::Quest);
            scroll_to(win, bag, doc, 0);
            win.set_tab(Tab::Normal);
            expect(win.grid_slot_rect(215, hidden_x, hidden_y, hidden_w, hidden_h),
                   "General and Other retain independent vertical positions");
            const auto up = control(win, doc, "scroll_up");
            const auto down = control(win, doc, "scroll_down");
            (void) win.handle_click(up.first, up.second, bag, wear);
            expect(!win.grid_slot_rect(215, hidden_x, hidden_y, hidden_w, hidden_h),
                   "scroll up arrow moves away from the final row");
            (void) win.handle_click(down.first, down.second, bag, wear);
            expect(win.grid_slot_rect(215, hidden_x, hidden_y, hidden_w, hidden_h),
                   "scroll down arrow restores the final row");
            const auto* track = doc.find_widget("scroll_track");
            const float track_x = win.x() + (track->x() + track->w() / 2) * scale;
            const float track_y = win.y() + track->y() * scale;
            (void) win.handle_click(track_x, track_y, bag, wear);
            expect(win.capturing_pointer() &&
                       win.grid_slot_rect(0, hidden_x, hidden_y, hidden_w, hidden_h),
                   "scrollbar captures the pointer and reaches the first block");
            openkal::engine::FrameInput drag_track;
            drag_track.mouse_x = track_x;
            drag_track.mouse_y = track_y + track->h() * scale;
            drag_track.mouse_left = true;
            win.update_pointer(drag_track, bag);
            expect(win.grid_slot_rect(215, hidden_x, hidden_y, hidden_w, hidden_h),
                   "dragging the scrollbar reaches the final block");
            (void) win.handle_release(drag_track.mouse_x, drag_track.mouse_y, bag, wear);
            expect(!win.capturing_pointer(), "scrollbar releases pointer capture");
            scroll_to(win, bag, doc, 0);
            const auto source_iid = bag.normal[0].iid;
            win.set_drag_slot(0);
            scroll_to(win, bag, doc, 6);
            float target_x, target_y, target_w, target_h;
            (void) win.grid_slot_rect(36, target_x, target_y, target_w, target_h);
            (void) win.handle_release(target_x + target_w / 2, target_y + target_h / 2, bag, wear);
            expect(bag.normal[36].iid == source_iid && bag.normal[0].count == 0,
                   "scrolling during item drag preserves the absolute source slot");
            win.set_drag_slot(36);
            scroll_to(win, bag, doc, 0);
            (void) win.grid_slot_rect(0, target_x, target_y, target_w, target_h);
            (void) win.handle_release(target_x + target_w / 2, target_y + target_h / 2, bag, wear);
            expect(bag.normal[0].iid == source_iid, "item drag scrolls back to its block");
            float x, y, w, h;
            (void) win.grid_slot_rect(0, x, y, w, h);
            expect(win.grid_slot_at(x - 4 * scale, y - 4 * scale) == 0,
                   "the visible slot frame is interactive");
            expect(win.grid_slot_at(x + 38 * scale, y) == -1, "slot gaps are not targets");
            const auto sort = control(win, doc, "sort");
            (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
            (void) win.handle_click(sort.first, sort.second, bag, wear);
            expect(bag.normal[0].iid == 41, "sorting cannot move a captured instance");
            (void) win.handle_release(x, y, bag, wear);
            expect(
                win.handle_click(sort.first, sort.second, bag, wear) ==
                        ClickResult::SortRequested &&
                    bag.normal[0].iid == 41,
                "sorting requests a server merge and leaves presentation unchanged before acknowledgement");
            win.complete_sort(bag, false);
            expect(win.selected() == -1 && win.hover_slot() == -1 && bag.normal[75].iid == 43,
                   "sorting clears stale input state and preserves expired-page items");
            const auto moved = bag.normal[0].iid;
            win.set_drag_slot(0);
            float tx, ty, tw, th;
            scroll_to(win, bag, doc, 18);
            (void) win.grid_slot_rect(108, tx, ty, tw, th);
            (void) win.handle_release(tx + tw / 2, ty + th / 2, bag, wear);
            expect(bag.normal[0].iid == moved && bag.normal[108].count == 0,
                   "locked General cells refuse deposits");
            // Expired items remain immediately accessible without selecting an old page.
            scroll_to(win, bag, doc, 0);
            (void) win.grid_slot_rect(4, tx, ty, tw, th);
            win.set_drag_slot(75);
            (void) win.handle_release(tx + tw / 2, ty + th / 2, bag, wear);
            expect(bag.normal[4].iid == 43 && bag.normal[75].count == 0,
                   "items can be taken out of expired blocks into available cells");
            win.set_tab(Tab::Quest);
            bag.quest[3] = item(300, 1, 700);
            bag.quest[kOtherSectionSlots + 2] = item(301, 1, 701);
            for (int section = 0; section < 2; ++section) {
                scroll_to(win, bag, doc, section == 0 ? 0 : 7);
                for (int i = section * kOtherSectionSlots; i < (section + 1) * kOtherSectionSlots;
                     ++i) {
                    expect(win.grid_slot_rect(i, x, y, w, h) &&
                               win.grid_slot_at(x + w / 2, y + h / 2) == i,
                           "Quest Items above Task Items are reachable by scrolling");
                }
            }
            (void) win.grid_slot_rect(kOtherSectionSlots + 2, x, y, w, h);
            expect(win.handle_right_click(x + w / 2, y + h / 2, bag) == ClickResult::Handled,
                   "Task Items reject manual use");
            (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
            expect(!win.capturing_pointer(), "Task Items never start a pointer drag");
            win.set_drag_slot(kOtherSectionSlots + 2);
            const auto protected_trash = control(win, doc, "destroy");
            expect(win.handle_release(protected_trash.first, protected_trash.second, bag, wear) ==
                           ClickResult::Handled &&
                       bag.quest[kOtherSectionSlots + 2].iid == 701,
                   "even a forced Task drag cannot request deletion");
            expect(win.handle_click(sort.first, sort.second, bag, wear) ==
                           ClickResult::SortRequested &&
                       bag.quest[3].iid == 700,
                   "quest sorting also waits for the server");
            win.complete_sort(bag, true);
            expect(bag.quest[0].iid == 700 && bag.quest[kOtherSectionSlots + 2].iid == 701,
                   "Other sorting leaves Task Item positions unchanged");
            win.set_drag_slot(kOtherSectionSlots + 2);
            (void) win.grid_slot_rect(5, tx, ty, tw, th);
            (void) win.handle_release(tx + tw / 2, ty + th / 2, bag, wear);
            expect(bag.quest[5].count == 0 && bag.quest[kOtherSectionSlots + 2].iid == 701,
                   "Task Items cannot move into Quest Items");
            bag.normal[6] = item(23013, 1, 702);
            bag.normal[7] = item(23014, 1, 703);
            win.set_tab(Tab::Normal);
            scroll_to(win, bag, doc, 0);
            const auto expand = control(win, doc, "expand");
            expect(win.handle_click(expand.first, expand.second, bag, wear) ==
                           ClickResult::Consumed &&
                       win.last_consumed_index() == 23013 && bag.find_instance(702),
                   "Expand selects the shortest available writ and awaits server acknowledgement");
            const auto trash = control(win, doc, "destroy");
            win.set_drag_slot(0);
            const auto iid = bag.normal[0].iid;
            expect(win.handle_release(trash.first, trash.second, bag, wear) ==
                           ClickResult::Destroyed &&
                       win.last_destroyed_iid() == iid && bag.normal[0].iid == iid,
                   "destroy drop preserves the confirmation flow");
            const auto close = control(win, doc, "close");
            (void) win.handle_click(close.first, close.second, bag, wear);
            expect(!win.open(), "footer close works at every scale");
        }
        win.set_wear(nullptr);
    }

    void
    verify_filters(const UiLayout& layout, const UiDocument& doc, const ItemDb& runtime_catalog) {
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_inventory_filters_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root / "items");
        const char* definitions[] = {
            R"({"index":91040,"name":"Filter medicine","class":["general","refresh"],"use":1,"specialty":{"refresh":{"stat":"hp","amount":5}}})",
            R"({"index":91041,"name":"Filter weapon","class":["weapon","sword"],"wear":1})",
            R"({"index":91042,"name":"Filter armor","class":["defense","upperarmor"],"wear":1})",
            R"({"index":91043,"name":"Filter charm","class":["general","etc"],"specialty":{"charming":"xattack"}})",
            R"({"index":91044,"name":"Filter pet","class":["pet","normal"],"wear":1})",
            R"({"index":91045,"name":"Filter remainder","class":["general","etc"]})",
            R"({"index":91046,"name":"Filter mask","class":["defense","mask"],"wear":1})",
            R"({"index":91047,"name":"Filter necklace","class":["ornament","necklace"],"wear":1})",
            R"({"index":91048,"name":"Filter animal","class":["transform","egg"],"wear":1})",
            R"({"index":91049,"name":"Filter mount","class":["ride","normal"],"wear":1})",
            R"({"index":91050,"name":"Filter limited pet","class":["limitedpet","normal"],"wear":1})",
            R"({"index":91051,"name":"Filter prefix","class":["general","etc"],"specialty":{"changeprefix":["weapon", 100, 1, 100, 2]}})",
            R"({"index":91052,"name":"Filter bead","class":["general","etc"],"specialty":{"bof":1}})"};
        for (int i = 0; i < 13; ++i) {
            std::ofstream out(root / "items" / (std::to_string(91040 + i) + ".json"));
            out << definitions[i];
        }
        ItemDb catalog;
        expect(catalog.load(root.string(), root.string()), "load owned category fixtures");
        Window win;
        win.set_layout(&layout);
        win.set_catalog(&catalog);
        win.set_open(true);
        win.center_on_screen(1920, 1080);
        Bag bag;
        bag.clear();
        Wear wear;
        win.set_wear(&wear);
        const char* names[] = {
            "consumables", "weapons", "armors", "enchantment", "skins_pets", "other"};
        auto select = [&](unsigned mask) {
            for (int i = 0; i < 6; ++i) {
                if (win.filter_active(i) != ((mask & (1u << i)) != 0)) {
                    const auto button =
                        control(win, doc, (std::string("filter.") + names[i]).c_str());
                    expect(win.handle_click(button.first, button.second, bag, wear) ==
                               ClickResult::Handled,
                           "filter toggle consumes its click");
                }
            }
            openkal::engine::FrameInput input;
            win.update_pointer(input, bag);
        };
        for (int i = 0; i < 6; ++i) {
            bag.normal[10 + i] = item(91040 + i, i + 1, 8000 + i);
            bag.normal[10 + i].prefix = i + 4;
        }
        for (float scale : {.75f, 1.f, 1.5f}) {
            win.set_scale(scale);
            win.center_on_screen(1920, 1080);
            for (unsigned mask = 0; mask < 64; ++mask) {
                select(mask);
                int display = 0;
                for (int i = 0; i < 6; ++i) {
                    float x, y, w, h;
                    const bool selected = mask == 0 || (mask & (1u << i)) != 0;
                    expect(win.grid_slot_rect(10 + i, x, y, w, h) == selected,
                           "every filter combination hides exactly its excluded categories");
                    if (selected) {
                        const auto* first = doc.find_widget("bag.0");
                        if (mask != 0) {
                            expect(win.general_source_slot(display) == 10 + i &&
                                       std::abs(x - win.x() -
                                                (first->x() + display * first->pitch_x()) *
                                                    win.scale()) < .01f,
                                   "combined categories pack together in source order");
                            ++display;
                        }
                        expect(win.grid_slot_at(x + w / 2, y + h / 2) == 10 + i,
                               "filtered hit tests preserve input slots");
                    }
                    expect(bag.normal[10 + i].iid == static_cast<unsigned>(8000 + i) &&
                               bag.normal[10 + i].count == i + 1 &&
                               bag.normal[10 + i].prefix == i + 4,
                           "filtering never mutates item instances or metadata");
                }
            }
        }
        // Additional supported item classes and unknown definitions partition correctly.
        for (const auto& entry : {std::pair<int, int>{91046, 4},
                                  {91047, 2},
                                  {91048, 4},
                                  {91049, 4},
                                  {91050, 4},
                                  {91051, 3},
                                  {91052, 3},
                                  {91999, 5}}) {
            bag.normal[16] = item(entry.first, 1, 8100);
            select(1u << entry.second);
            expect(win.general_source_slot(1) == 16,
                   "mask, animals, jewelry, charms and unknown items reach their category");
            bag.normal[16] = {};
        }
        select(1);
        float x, y, w, h;
        expect(win.grid_slot_rect(10, x, y, w, h), "filtered consumable is visible");
        expect(win.handle_right_click(x + w / 2, y + h / 2, bag) == ClickResult::Consumed &&
                   win.last_consumed_slot() == 10,
               "right-click use addresses the filtered instance");
        const auto filter = control(win, doc, "filter.weapons");
        win.set_drag_slot(10);
        (void) win.handle_click(filter.first, filter.second, bag, wear);
        expect(!win.filter_active(1), "category toggles cannot invalidate a captured drag");
        const auto* grid = doc.find_widget("bag.0");
        // Cell one resolves to the first genuinely empty source cell, not hidden gear.
        const float empty_x = win.x() + (grid->x() + grid->pitch_x() + 16) * win.scale();
        const float empty_y = win.y() + (grid->y() + 16) * win.scale();
        expect(win.general_source_slot(1) == 0, "filtered blank cells use genuine empty storage");
        (void) win.handle_release(empty_x, empty_y, bag, wear);
        expect(bag.normal[0].iid == 8000 && bag.normal[11].iid == 8001,
               "dragging into a filtered empty cell preserves excluded items");
        win.set_drag_slot(0);
        const auto destroy = control(win, doc, "destroy");
        expect(win.handle_release(destroy.first, destroy.second, bag, wear) ==
                       ClickResult::Destroyed &&
                   win.last_destroyed_iid() == 8000,
               "filtered destroy resolves the exact instance");
        // Expired storage is packed only within its own page and remains take-out-only.
        bag.normal[79] = item(91041, 1, 8200);
        select(2);
        expect(win.general_source_slot(0) == 11 && win.general_source_slot(72) == 79,
               "expired matches stay outside unlocked cells");
        scroll_to(win, bag, doc, 12);
        expect(win.grid_slot_rect(79, x, y, w, h), "expired filtered item scrolls into view");
        win.set_drag_slot(11);
        (void) win.handle_release(x + w / 2, y + h / 2, bag, wear);
        expect(bag.normal[11].iid == 8001 && bag.normal[79].iid == 8200,
               "filtering cannot enable swaps into expired storage");
        // Content updates rebuild the projection without requiring another toggle.
        bag.normal[11] = {};
        openkal::engine::FrameInput update;
        win.update_pointer(update, bag);
        expect(win.general_source_slot(0) != 11 && win.general_source_slot(72) == 79,
               "authoritative removal refreshes the filtered projection");
        win.set_tab(Tab::Quest);
        bag.quest[3] = item(91045, 1, 8300);
        scroll_to(win, bag, doc, 0);
        expect(win.grid_slot_rect(3, x, y, w, h) && win.grid_slot_at(x + w / 2, y + h / 2) == 3,
               "General filters do not affect Other");
        (void) win.handle_click(filter.first, filter.second, bag, wear);
        expect(win.filter_active(1), "hidden General buttons do not change state in Other");
        win.set_tab(Tab::Normal);
        select(0);
        expect(win.general_source_slot(79) == 79,
               "turning the last category off restores the complete inventory");
        // Exercise production metadata for every supported special upgrade family.
        bag.clear();
        win.set_catalog(&runtime_catalog);
        for (const auto& entry : {std::pair<int, int>{23000, 0},
                                  {23008, 0},
                                  {23012, 0},
                                  {23013, 0},
                                  {23017, 3},
                                  {23018, 3},
                                  {23019, 3},
                                  {23020, 3},
                                  {23021, 3},
                                  {23022, 3},
                                  {23026, 3},
                                  {24057, 3},
                                  {24062, 3},
                                  {23016, 5}}) {
            expect(runtime_catalog.find(entry.first) != nullptr,
                   "owned production definition exists");
            bag.normal[20] = item(entry.first, 1, 8400);
            select(1u << entry.second);
            expect(win.general_source_slot(0) == 20,
                   ("production category for " + std::to_string(entry.first)).c_str());
            bag.normal[20] = {};
        }
        win.set_catalog(&catalog);
        bag.normal[20] = item(91041, 1, 8500);
        select(2);
        scroll_to(win, bag, doc, 0);
        expect(win.grid_slot_rect(20, x, y, w, h), "filtered weapon is visible");
        (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
        (void) win.handle_release(x + w / 2, y + h / 2, bag, wear);
        (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
        expect(win.handle_release(x + w / 2, y + h / 2, bag, wear) == ClickResult::EquipChanged &&
                   win.last_equip_iid() == 8500 && bag.normal[20].iid == 8500,
               "filtered double-click equips the exact source instance after acknowledgement");
        // A completely occupied bag has no writable blank cells, even when all
        // its contents are excluded by the filter.
        bag.clear();
        for (int i = 0; i < 72; ++i) {
            bag.normal[i] = item(91045, 1, 8600 + i);
        }
        select(2);
        expect(win.general_source_slot(0) == -1 && win.general_source_slot(71) == -1,
               "hidden occupied cells never become empty drop targets");
        std::filesystem::remove_all(root);
    }

    void verify_scroll_input(UiLayout& layout, const UiDocument& doc) {
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_scroll_input_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root / "items");
        std::ofstream(root / "items/192.json")
            << R"({"index":192,"class":["general","teleport"],"stackable":1,"use":1,"specialty":{"teleport":{"loc":0}}})";
        ItemDb catalog;
        expect(catalog.load(root.string(), root.string()), "load owned city scroll definition");
        Window win;
        win.set_layout(&layout);
        Bag bag;
        bag.clear();
        bag.normal[0] = item(192, 5, 901);
        bag.normal[0].character_bound = true;
        bag.normal[1] = item(192, 3, 902);
        Wear wear;
        win.set_catalog(&catalog);
        win.set_wear(&wear);
        win.set_open(true);
        win.set_tab(Tab::Normal);
        win.center_on_screen(1920, 1080);
        scroll_to(win, bag, doc, 0);
        for (int cell : {0, 1}) {
            float x = 0, y = 0, w = 0, h = 0;
            expect(win.grid_slot_rect(cell, x, y, w, h), "locate each scroll instance");
            expect(win.handle_right_click(x + w / 2, y + h / 2, bag) == ClickResult::Consumed &&
                       win.last_consumed_iid() == static_cast<std::uint32_t>(901 + cell) &&
                       win.last_teleport_loc() == 0,
                   "scroll right-click requests the exact bound or ordinary instance");
            expect(bag.normal[0].count == 5 && bag.normal[1].count == 3,
                   "scroll input leaves both stacks unchanged until the server responds");
        }
        win.set_wear(nullptr);
        std::filesystem::remove_all(root);
    }

    void verify_other_capacity(ItemDb&) {
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_task_items_" + std::to_string(::getpid()));
        for (const auto* dir : {"items", "quests", "groups"}) {
            std::filesystem::create_directories(root / dir);
        }
        const char* definitions[] = {
            R"({"index":91020,"name":"General fixture","class":["general","etc"]})",
            R"({"index":91021,"name":"Quest drop fixture","class":["quest","etc"]})",
            R"({"index":91022,"name":"Quest document fixture","class":["quest","etc"]})",
            R"({"index":91023,"name":"Shared source fixture","class":["quest","etc"]})",
            R"({"index":91024,"name":"Reward medicine fixture","class":["general","etc"],"stackable":1,"specialty":{"refresh":{"stat":"hp","amount":5}}})",
            R"({"index":91025,"name":"Reward weapon fixture","class":["weapon","sword"],"wear":1})"};
        for (int i = 0; i < 6; ++i) {
            std::ofstream out(root / "items" / (std::to_string(91020 + i) + ".json"));
            out << definitions[i];
        }
        {
            std::ofstream out(root / "quests" / "11.json");
            out << R"({"index":11,"flag":1,"cases":[{"then":[{"op":"item","in":[[91022, 0, 1], [91023, 0, 1], [91024, 0, 1], [91025, 0, 1]]}]}]})";
        }
        {
            std::ofstream out(root / "groups" / "1.json");
            out << R"({"index":1,"entries":[{"item":91023,"chance":100,"count":1}]})";
        }
        ItemDb catalog;
        expect(catalog.load(root.string(), root.string()),
               "load independent Quest and Task source fixtures");
        expect(catalog.find(91022)->is_task() && !catalog.find(91021)->is_task() &&
                   !catalog.find(91023)->is_task() && !catalog.find(91024)->is_task() &&
                   !catalog.find(91025)->is_task(),
               "only special quest grants without loot sources become Task Items");
        Bag bag;
        bag.clear();
        for (int i = 0; i < 72; ++i) {
            expect(bag.try_insert(91020, 1, catalog, 0, 1000 + i), "fill base General capacity");
        }
        expect(!bag.try_insert(91024, 1, catalog),
               "normal reward medicine consumes General capacity");
        for (int i = 0; i < kOtherSectionSlots; ++i) {
            expect(bag.try_insert(91021, 1, catalog, 0, 2000 + i),
                   "Quest Items have independent capacity");
            expect(bag.try_insert(91022, 1, catalog, 0, 3000 + i),
                   "Task Items have independent capacity");
        }
        expect(!bag.try_insert(91021, 1, catalog) && !bag.try_insert(91022, 1, catalog),
               "both Other sections retain their independent 36-cell limit");
        expect(bag.quest[0].index == 91021 && bag.quest[36].index == 91022,
               "quest and task predicates route to their respective sections");
        expect(bag.try_remove_iid(1000) && bag.try_insert(91024, 5, catalog, 0, 4000) &&
                   bag.try_insert(91024, 7, catalog, 0, 4000),
               "normal consumable rewards stack in General");
        expect(bag.find_instance(4000)->count == 12 && bag.try_consume_one(91024, catalog).ok &&
                   bag.find_instance(4000)->count == 11,
               "General consumable rewards retain normal use");
        expect(bag.try_remove_iid(3000) && bag.count_item(91022) == 35,
               "quest script consumption can still remove protected Task Items authoritatively");
        expect(bag.try_remove_iid(1001) && bag.try_remove_iid(1002),
               "free cells for distinct authoritative medicine stacks");
        for (auto& item : bag.normal) {
            if (item.iid == 4000) {
                item.character_bound = true;
            }
        }
        expect(bag.try_insert(91024, 3, catalog, 0, 4001) && bag.find_instance(4000)->count == 11 &&
                   bag.find_instance(4001)->count == 3,
               "different authoritative iids keep bound and ordinary medicine separate");
        bool prefix_changed = false;
        expect(bag.try_update_instance(4000, 91024, 0, 10, true, prefix_changed) &&
                   bag.find_instance(4000)->count == 10 &&
                   bag.find_instance(4000)->character_bound && bag.find_instance(4001)->count == 3,
               "consumption updates only the selected instance and preserves its binding");
        ItemTipExtras bound_extras;
        bound_extras.character_bound = true;
        const auto bound_tip =
            ItemTooltip::make_tip(*catalog.find(91024), catalog, 10, 0, 1, 0, -1, -1, bound_extras);
        expect(bound_tip.find("Character Bound") != std::string::npos &&
                   bound_tip.find("Account Bound") == std::string::npos,
               "starter tooltip identifies character binding explicitly");
        std::filesystem::remove_all(root);
    }

    void verify_collection(Window& win, const UiDocument& doc) {
        UiLayout collection_layout;
        expect(collection_layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                           "/config/ui/inventory_collection.json"),
               "load Collection layout");
        UiDocument collection_doc;
        expect(collection_doc.load(collection_layout.root()), "parse Collection layout");
        Bag bag;
        Wear wear;
        win.set_wear(&wear);
        win.set_player_level(90);
        for (float scale : {1.0f, 1.5f}) {
            bag.clear();
            bag.normal[0] = item(24056, 1, 100);
            bag.normal[1] = item(24059, 1, 101);
            bag.normal[1].random_stat_type = 112;
            bag.normal[1].random_stat_value = 30;
            bag.normal[2] = item(24059, 1, 102);
            bag.normal[2].random_stat_type = 100;
            bag.normal[2].random_stat_value = 50;
            win.set_scale(scale);
            win.center_on_screen(1920, 1080);
            win.set_open(true);
            win.set_tab(Tab::Normal);
            float x, y, w, h;
            (void) win.grid_slot_rect(0, x, y, w, h);
            (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
            openkal::engine::FrameInput pointer;
            const auto handle = control(win, doc, "frame_handle");
            pointer.mouse_x = handle.first;
            pointer.mouse_y = handle.second;
            pointer.mouse_left = true;
            win.update_pointer(pointer, bag);
            std::this_thread::sleep_for(std::chrono::milliseconds(370));
            win.update_pointer(pointer, bag);
            expect(win.tab() == Tab::Normal && win.drag_slot() == 0,
                   "drag hover opens Collection while retaining the General source instance");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const auto mirror = collection_control(win, collection_doc, "collection.10");
            expect(win.handle_release(mirror.first, mirror.second, bag, wear) ==
                           ClickResult::EquipChanged &&
                       win.last_equip_iid() == 100 && !bag.normal[0].equipped,
                   "dragging to the Collection drawer awaits the server acknowledgement");
            wear = {};
            expect(bag.set_collection_equipped(100, true) &&
                       bag.set_collection_equipped(101, true) &&
                       bag.set_collection_equipped(102, true),
                   "move exact instances into Collection storage");
            const auto essence = collection_control(win, collection_doc, "collection.12");
            pointer.mouse_x = essence.first;
            pointer.mouse_y = essence.second;
            pointer.mouse_left = false;
            win.update_pointer(pointer, bag);
            expect(win.hover_slot() == 2,
                   "Equipment tooltip resolves the exact collection instance");
            (void) win.handle_click(essence.first, essence.second, bag, wear);
            expect(win.handle_click(essence.first, essence.second, bag, wear) ==
                           ClickResult::EquipChanged &&
                       win.last_equip_iid() == 102 && win.last_equip_op() == EquipNetOp::PutOff,
                   "Collection drawer double click removes the chosen duplicate");
            win.set_tab(Tab::Quest);
            win.update_pointer(pointer, bag);
            expect(win.hover_slot() == 2, "Collection remains accessible beside Other");
            win.set_tab(Tab::Normal);
            bag.normal[3] = item(24051, 1, 103);
            bag.normal[4] = item(24051, 1, 104);
            (void) win.grid_slot_rect(4, x, y, w, h);
            (void) win.handle_click(x + w / 2, y + h / 2, bag, wear);
            (void) win.handle_release(x + w / 2, y + h / 2, bag, wear);
            std::swap(bag.normal[3], bag.normal[4]);
            const auto absorb = collection_control(win, collection_doc, "absorb");
            expect(win.handle_click(absorb.first, absorb.second, bag, wear) ==
                           ClickResult::Enchanted &&
                       win.last_enchant_from_id() == 104 && win.last_enchant_to_id() == 104,
                   "absorption retains the selected Taegeuk IID across sorting and tab changes");
            const auto close_handle = control(win, doc, "frame_handle");
            (void) win.handle_click(close_handle.first, close_handle.second, bag, wear);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        win.set_tab(Tab::Normal);
        win.set_wear(nullptr);
    }

    void verify_collection_primary_bonuses() {
        Bag bag;
        bag.clear();
        bag.normal[0] = item(24054, 1, 100);
        const std::array<int, 5> options{100, 103, 102, 101, 104};
        const std::array<int, 5> values{7, 11, 13, 17, 19};
        for (std::size_t i = 0; i < options.size(); ++i) {
            auto essence = item(24059, 1, 101 + static_cast<std::uint32_t>(i));
            essence.random_stat_type = options[i];
            essence.random_stat_value = values[i];
            bag.normal[i + 1] = essence;
        }
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{},
               "unequipped essences do not add character info bonuses");
        for (std::uint32_t iid = 100; iid <= 105; ++iid) {
            expect(bag.set_collection_equipped(iid, true), "equip primary essence fixture");
        }
        expect(collection_primary_bonuses(bag) == values,
               "all five essence attributes map to character info order in separate storage");
        bag.collection[0].index = 24055;
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{7, 12, 14, 18, 20},
               "middle Hanin mirror amplifies displayed essence attributes by ten percent");
        bag.collection[0].index = 24056;
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{8, 13, 15, 20, 22},
               "senior Hanin mirror amplifies displayed essence attributes by twenty percent");
        expect(bag.set_collection_equipped(101, false), "remove strength essence");
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{0, 13, 15, 20, 22},
               "returning an essence to the bag removes its display bonus");
        expect(bag.set_collection_equipped(101, true), "restore strength essence");
        bag.collection[1].random_stat_value = 4;
        bag.collection[2].random_stat_type = 100;
        bag.collection[2].random_stat_value = 4;
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{9, 0, 15, 20, 22},
               "stacked essence totals are truncated after amplification as in server combat");
        bag.collection[1].count = 0;
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{4, 0, 15, 20, 22},
               "empty collection entries do not add display bonuses");
        bag.collection[0].equipped = false;
        expect(collection_primary_bonuses(bag) == std::array<int, 5>{},
               "essences without an equipped Hanin mirror have no display bonus");
    }

    void verify_collection_storage(const ItemDb& catalog) {
        Bag bag;
        bag.clear();
        for (int i = 0; i < kBagSlots; ++i) {
            bag.normal[i] = item(1, 1, 1000 + i);
        }
        expect(bag.try_insert(24059, 1, catalog, 0, 900, 0, 0, true),
               "login restores collection even with every bag cell occupied");
        bag.collection[0].random_stat_type = 112;
        bag.collection[0].random_stat_value = 30;
        bag.collection[0].upgrade_level = 4;
        bag.collection[0].account_bound = true;
        expect(!bag.set_collection_equipped(900, false) && bag.collection[0].iid == 900,
               "full bag leaves collection item untouched");
        expect(bag.try_remove_iid(1000) && bag.set_collection_equipped(900, false),
               "collection returns to a freed writable bag cell");
        const auto restored = std::find_if(bag.normal.begin(),
                                           bag.normal.end(),
                                           [](const auto& entry) { return entry.iid == 900; });
        expect(restored != bag.normal.end() && !restored->equipped && restored->account_bound &&
                   restored->random_stat_type == 112 && restored->random_stat_value == 30 &&
                   restored->upgrade_level == 4,
               "collection transfer preserves identity, options, level and binding");
        expect(bag.set_collection_equipped(900, true) && bag.set_collection_equipped(900, true),
               "repeated equipment acknowledgement does not duplicate an instance");
        expect(std::count_if(bag.normal.begin(),
                             bag.normal.end(),
                             [](const auto& entry) { return entry.count > 0; }) == kBagSlots - 1,
               "collection frees actual inventory capacity");
        bag.sort_items(&catalog, bag.compact_register_enabled.data());
        expect(bag.collection[0].iid == 900 && bag.collection[0].random_stat_value == 30,
               "inventory sorting leaves collection instances intact");
        bool changed = false;
        expect(bag.try_update_instance(900, 24059, 0, 1, true, changed),
               "absolute instance updates resolve collection storage");
        expect(bag.try_remove_iid(900) && bag.collection[0].iid == 0,
               "server removal clears the separate collection instance");
        bag.clear();
        expect(bag.collection.empty(), "character reset clears collection storage");
    }

    void
    verify_equipment_storage(Window& win, const UiDocument& doc, const ItemDb& original_catalog) {
        const auto root = std::filesystem::temp_directory_path() /
                          ("openkal_equipment_storage_" + std::to_string(::getpid()));
        std::filesystem::create_directories(root);
        const char* classes[] = {"weapon", "defense", "ornament", "defense", "defense", "defense"};
        const char* parts[] = {"sword", "upperarmor", "ring", "hat", "gloves", "mask"};
        for (int i = 0; i < 6; ++i) {
            std::ofstream out(root / (std::to_string(91000 + i) + ".json"));
            out << "{\"index\":" << 91000 + i << ",\"class\":[\"" << classes[i] << "\",\""
                << parts[i] << "\"],\"wear\":1}";
        }
        ItemDb catalog;
        expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                            OPENKAL_TEST_RESOURCE_ROOT,
                            root.string()),
               "load ordinary equipment fixtures");
        Bag bag;
        bag.clear();
        for (int i = 0; i < 72; ++i) {
            bag.normal[i] = item(91000, 1, 1000 + i);
        }
        const auto occupied = [&] {
            return std::count_if(bag.normal.begin(), bag.normal.end(), [](const ItemStack& s) {
                return s.count > 0;
            });
        };
        bag.normal[0].prefix = 7;
        bag.normal[0].x_attack = 12;
        bag.normal[0].random_stat_type = 3;
        bag.normal[0].random_stat_value = 4;
        bag.normal[0].cur_end = 23;
        bag.normal[0].max_end = 45;
        bag.normal[0].enchant_protected = true;
        Wear wear;
        win.set_catalog(&catalog);
        win.set_wear(&wear);
        win.set_open(true);
        win.set_tab(Tab::Normal);
        win.set_player_level(90);
        win.set_drag_slot(0);
        const auto permanent_weapon = control(win, doc, "wear.weapon");
        expect(win.handle_release(permanent_weapon.first, permanent_weapon.second, bag, wear) ==
                       ClickResult::EquipChanged &&
                   win.last_equip_iid() == 1000 && !bag.normal[0].equipped,
               "drag to permanently visible weapon slot requests the exact General instance");
        const auto request = Bag::toggle_equip_slot(bag, 0, wear, catalog, 0, 90);
        expect(request.changed && request.iid == 1000 && occupied() == 72 &&
                   wear.weapon_index() == 0,
               "ordinary equip request leaves inventory and wear unchanged before acknowledgement");
        expect(bag.set_equipment_equipped(1000, true, catalog) && occupied() == 71 &&
                   bag.equipment[0].iid == 1000 && bag.equipment[0].equipped,
               "equipping an exact instance releases its General cell");
        expect(bag.count_item(91000) == 72,
               "quest ownership counts still include ordinary worn equipment");
        wear.apply_item(*catalog.find(91000));
        win.set_catalog(&catalog);
        win.set_wear(&wear);
        win.set_tab(Tab::Normal);
        win.set_open(true);
        const auto weapon_cell = control(win, doc, "wear.weapon");
        (void) win.handle_click(weapon_cell.first, weapon_cell.second, bag, wear);
        win.layout_size(1920, 1080);
        expect(win.handle_click(weapon_cell.first, weapon_cell.second, bag, wear) ==
                       ClickResult::EquipChanged &&
                   win.last_equip_op() == EquipNetOp::PutOff && win.last_equip_iid() == 1000 &&
                   bag.equipment[0].iid == 1000,
               "double-clicking dedicated weapon cell requests exact-instance unequip without "
               "optimistic mutation");
        expect(bag.try_insert(91000, 1, catalog) && occupied() == 72,
               "released General cell accepts a new item");
        expect(!bag.set_equipment_equipped(1000, false, catalog) && bag.equipment[0].iid == 1000,
               "full General leaves worn item in its equipment slot");
        expect(bag.set_equipment_equipped(1001, true, catalog) && occupied() == 72 &&
                   bag.equipment[0].iid == 1001 && !bag.find_instance(1000)->equipped,
               "same-definition replacement swaps exact identities while General is full");
        const auto* previous = bag.find_instance(1000);
        expect(previous && previous->prefix == 7 && previous->x_attack == 12 &&
                   previous->cur_end == 23 && previous->max_end == 45 &&
                   previous->random_stat_value == 4 && previous->enchant_protected,
               "replacement preserves every instance property");
        expect(bag.try_remove_iid(1002) && bag.set_equipment_equipped(1001, false, catalog) &&
                   occupied() == 72 && bag.equipment[0].count == 0,
               "unequipping returns the exact instance to a free unlocked General cell");
        expect(bag.set_equipment_equipped(1000, true, catalog) &&
                   bag.set_equipment_equipped(1000, true, catalog),
               "duplicate acknowledgements are idempotent");
        const bool enabled[kMaxRegisters] = {true, true, false, false, false, false};
        bag.sort_items(&catalog, enabled);
        expect(bag.equipment[0].iid == 1000 && bag.item_at(kBagSlots)->iid == 1000,
               "sorting preserves equipment and its dedicated UI slot");
        bool changed = false;
        expect(bag.try_update_instance(1000, 91000, 8, 1, true, changed, 20, 45) &&
                   bag.equipment[0].prefix == 8 && bag.equipment[0].cur_end == 20,
               "absolute metadata and endurance updates reach worn storage");
        expect(bag.try_set_transform_progress(1000, 17, 123) &&
                   bag.equipment[0].transform_level == 17 && bag.equipment[0].transform_exp == 123,
               "instance progress updates reach dedicated equipment storage");
        // Login restores equipment even with all General cells occupied, including locked cells.
        bag.clear();
        for (int i = 0; i < kBagSlots; ++i) {
            bag.normal[i] = item(91000, 1, 2000 + i);
        }
        const int cells[] = {0, 3, 8, 2, 6, 7};
        for (int i = 0; i < 6; ++i) {
            expect(bag.try_insert(91000 + i, 1, catalog, 0, 3000 + i, 17, 30, true) &&
                       bag.equipment[cells[i]].iid == static_cast<std::uint32_t>(3000 + i) &&
                       occupied() == kBagSlots,
                   "login restores each ordinary equipment category independently of General "
                   "capacity");
        }
        // Overflow gear can be withdrawn, but a replacement cannot return into a locked cell.
        bag.normal[100] = item(91000, 1, 4000);
        expect(!bag.set_equipment_equipped(4000, true, catalog) && bag.equipment[0].iid == 3000,
               "full unlocked General rejects a swap that would deposit into an expired cell");
        expect(bag.try_remove_iid(2000) && bag.set_equipment_equipped(4000, true, catalog) &&
                   !bag.find_instance(3000)->equipped,
               "overflow equipment replacement returns old gear to a free unlocked cell");
        expect(bag.try_remove_iid(4000) && bag.equipment[0].count == 0,
               "authoritative removal clears the currently worn replacement");
        expect(bag.try_remove_iid(3000) && bag.equipment[0].count == 0,
               "authoritative removal clears equipment without creating a General entry");
        bag.clear();
        expect(bag.equipment.empty(), "character reset clears equipment storage");
        win.set_catalog(&original_catalog);
        win.set_wear(nullptr);
        std::filesystem::remove_all(root);
    }

    void capture_binding_icons(const ItemDb& catalog, const std::string& path) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Item binding verification", 520, 300)) {
            expect(false, "initialize binding icon capture engine");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans_bold(), 15),
               "load TrueType binding preview labels");
        expect(engine->begin_offscreen(520, 300, .055f, .06f, .08f), "begin binding icon preview");
        font.draw(*engine, 100, 12, "Account Bound", .9f, .9f, .95f);
        font.draw(*engine, 290, 12, "Character Bound", .9f, .9f, .95f);
        const auto file = catalog.icon_filename(23000);
        expect(ui.item_tex(file) && ui.tex("accBound.bmp"), "resolve owned icon and binding frame");
        for (int row = 0; row < 3; ++row) {
            const float size = 32 + row * 16;
            const float y = 48 + row * 82;
            font.draw(*engine,
                      18,
                      y + size * .5f - 8,
                      std::to_string(static_cast<int>(size)) + " px",
                      .7f,
                      .75f,
                      .85f);
            for (int column = 0; column < 2; ++column) {
                const float x = 140 + column * 190;
                ui.draw_item_tex(file, x, y, size, size);
                UiView::draw_item_binding(ui, x, y, size, size, column == 0, column == 1);
            }
        }
        std::vector<std::uint8_t> rgba;
        expect(engine->end_offscreen_rgba(rgba) && rgba.size() == 520 * 300 * 4,
               "read actual rendered binding locks");
        if (rgba.size() == 520 * 300 * 4) {
            for (int row = 0; row < 3; ++row) {
                const int size = 32 + row * 16;
                const int top = 48 + row * 82;
                bool left_differs = false, right_matches = true;
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        const auto account =
                            static_cast<std::size_t>(((top + y) * 520 + 140 + x) * 4);
                        const auto character =
                            static_cast<std::size_t>(((top + y) * 520 + 330 + x) * 4);
                        const bool same = std::equal(rgba.begin() + account,
                                                     rgba.begin() + account + 4,
                                                     rgba.begin() + character);
                        if (x < size / 2) {
                            left_differs = left_differs || !same;
                        } else {
                            right_matches = right_matches && same;
                        }
                    }
                }
                expect(left_differs && right_matches,
                       "character binding adds the left lock while preserving the right lock and "
                       "frame opacity");
            }
            std::ofstream out(path, std::ios::binary);
            out << "P6\n520 300\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "save binding icon preview");
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }

    void capture_enchantment_icons(const ItemDb& catalog, const std::string& path) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Enchantment talisman grade verification", 520, 280)) {
            expect(false, "initialize talisman icon capture engine");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans_bold(), 14),
               "load TrueType preview labels");
        expect(engine->begin_offscreen(520, 280, .055f, .06f, .08f),
               "begin talisman grade preview");
        for (int row = 0; row < 3; ++row) {
            const float size = 32.0f + row * 16.0f;
            const float y = 26.0f + row * 82.0f;
            font.draw(*engine,
                      12,
                      y + size / 2 - 8,
                      std::to_string(static_cast<int>(size)) + " px",
                      .7f,
                      .75f,
                      .85f);
            for (int grade = 0; grade < 5; ++grade) {
                const float x = 92.0f + grade * 82.0f;
                const auto file =
                    catalog.icon_filename(openkal::server::kEnchantmentTalismanIndex + grade);
                expect(!file.empty() && ui.item_tex(file), "resolve ranked talisman icon");
                ui.draw_item_tex(file, x, y, size, size);
            }
        }
        std::vector<std::uint8_t> rgba;
        expect(engine->end_offscreen_rgba(rgba) && rgba.size() == 520 * 280 * 4,
               "read actual rendered painted Roman numerals");
        if (rgba.size() == 520 * 280 * 4) {
            // Each painted rank must remain visually distinct at native inventory size.
            for (int rank = 1; rank < 5; ++rank) {
                bool differs = false;
                for (int y = 26; y < 58; ++y) {
                    for (int x = 0; x < 32; ++x) {
                        const auto first = static_cast<std::size_t>((y * 520 + 92 + x) * 4);
                        const auto other =
                            static_cast<std::size_t>((y * 520 + 92 + rank * 82 + x) * 4);
                        differs = differs || rgba[first] != rgba[other] ||
                                  rgba[first + 1] != rgba[other + 1] ||
                                  rgba[first + 2] != rgba[other + 2];
                    }
                }
                expect(differs, "painted Roman numerals distinguish scroll grades at 32 pixels");
            }
            std::ofstream out(path, std::ios::binary);
            out << "P6\n520 280\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "save talisman grade preview");
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }

    void capture(Window& win,
                 const UiDocument& doc,
                 const std::string& prefix,
                 const ItemDb* skin_catalog = nullptr) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Compact Inventory verification", 1920, 1080)) {
            expect(false, "initialize inventory capture engine");
            return;
        }
        WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        win.sync_fonts(*engine);
        Bag bag;
        bag.clear();
        bag.money = 12345678;
        // Project-owned icons keep captures useful without optional game data.
        bag.normal[0] = item(23016, 25, 401);
        bag.normal[1] = item(23000, 12, 402);
        bag.normal[2] = item(23004, 8, 403);
        bag.normal[1].account_bound = true;
        bag.normal[2].character_bound = true;
        bag.normal[3] = item(23013, 1, 404);
        bag.normal[4] = item(23023, 1, 405);
        bag.normal[5] = item(23022, 1, 406);
        Wear wear;
        if (skin_catalog) {
            for (int index = 25000; index <= 29999; ++index) {
                const auto* skin = skin_catalog->find(index);
                if (!skin || skin->class_limit() != 0) {
                    continue;
                }
                if (skin->full_body_skin() &&
                    wear.skin_index(openkal::protocol::SkinSlot::Upper) == 0) {
                    wear.apply_item(*skin);
                    (void) bag.try_insert(index, 1, *skin_catalog, 0, index, 0, 0, true);
                } else if (skin->skin_slot() == openkal::protocol::SkinSlot::Weapon &&
                           wear.skin_index(openkal::protocol::SkinSlot::Weapon) == 0) {
                    wear.apply_item(*skin);
                    (void) bag.try_insert(index, 1, *skin_catalog, 0, index, 0, 0, true);
                }
            }
        }
        win.set_wear(&wear);
        win.set_open(true);
        const auto now =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        std::array<std::uint64_t, kMaxRegisters> expiry{};
        expiry[2] = now + 7ull * 86400 * 1000;
        win.set_register_unlock_expire_ms(expiry);
        for (float scale : {1.0f, 1.5f}) {
            win.set_scale(scale);
            win.center_on_screen(1920, 1080);
            for (int state = 0; state < 8; ++state) {
                win.set_tab(state == 1 || state == 2 ? Tab::Quest : Tab::Normal);
                scroll_to(win, bag, doc, state == 2 ? 7 : state == 4 ? 12 : state == 5 ? 29 : 0);
                if (state == 3) {
                    const auto handle = control(win, doc, "frame_handle");
                    Wear unused;
                    (void) win.handle_click(handle.first, handle.second, bag, unused);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                for (const auto* name : {"consumables", "enchantment"}) {
                    const int category = std::string(name) == "consumables" ? 0 : 3;
                    const bool selected = state == 6 || (state == 7 && category == 3);
                    if (win.filter_active(category) != selected) {
                        const auto button =
                            control(win, doc, (std::string("filter.") + name).c_str());
                        (void) win.handle_click(button.first, button.second, bag, wear);
                    }
                }
                openkal::engine::FrameInput pointer;
                if (state == 7) {
                    const auto enchant = control(win, doc, "filter.enchantment");
                    pointer.mouse_x = enchant.first;
                    pointer.mouse_y = enchant.second;
                }
                if (state == 3) {
                    const auto expand = control(win, doc, "expand");
                    pointer.mouse_x = expand.first;
                    pointer.mouse_y = expand.second;
                }
                win.update_pointer(pointer, bag);
                expect(engine->begin_offscreen(1920, 1080, .06f, .07f, .08f),
                       "begin inventory capture");
                win.draw(ui, *engine, bag);
                if (state == 3 || state == 7) {
                    win.draw_tooltip(ui, *engine, bag, nullptr, 1920, 1080);
                }
                std::vector<std::uint8_t> rgba;
                expect(engine->end_offscreen_rgba(rgba), "read inventory capture");
                std::ofstream out(prefix + (scale == 1 ? "-native-" : "-scaled-") +
                                      std::to_string(state) + ".ppm",
                                  std::ios::binary);
                out << "P6\n1920 1080\n255\n";
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save complete inventory capture");
                if (state == 3) {
                    const auto handle = control(win, doc, "frame_handle");
                    Wear unused;
                    (void) win.handle_click(handle.first, handle.second, bag, unused);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
        }
        win.set_register_unlock_expire_ms({});
        win.set_tab(Tab::Normal);
        win.set_wear(nullptr);
        win.destroy_fonts(*engine);
        ui.destroy();
        engine->shutdown();
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--emotes-only") {
        verify_emotes();
        verify_social_actions();
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    verify_item_definition_flags();
    verify_cosmetic_skins();
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/ui/inventory.json"),
           "load inventory layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "load inventory document");
    ItemDb catalog;
    expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config",
                        OPENKAL_TEST_RESOURCE_ROOT),
           "load item definitions");
    const auto* box = catalog.find(23012);
    expect(box && box->is_essence_box() && box->is_consumable(),
           "essence box supports inventory use requests");
    Window win;
    win.set_layout(&layout);
    win.set_catalog(&catalog);
    if (argc == 3 && std::string(argv[1]) == "--verify-skin-assets") {
        int count = 0;
        for (int index = 25000; index <= 29999; ++index) {
            const auto* skin = catalog.find(index);
            if (!skin || !skin->is_skin()) {
                continue;
            }
            ++count;
            const auto path = EquipmentPaths::skin_gb_path(
                argv[2], static_cast<Class>(skin->class_limit()), *skin);
            openkal::terrain::GbModel model;
            const bool loaded =
                !path.empty() && openkal::terrain::load_gb(path, model) && !model.meshes.empty();
            if (!loaded) {
                std::fprintf(stderr, "Skin model failed: %d %s\n", index, path.c_str());
            }
            expect(loaded, "optional cosmetic model resolves and decodes");
        }
        expect(count == 784, "every active cosmetic item has been checked");
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-skins") {
        capture(win, doc, argv[2], &catalog);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-binding-icons") {
        capture_binding_icons(catalog, argv[2]);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-compact") {
        capture(win, doc, argv[2]);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 2 && std::string(argv[1]) == "--compact-inventory-only") {
        verify_filters(layout, doc, catalog);
        verify_input(win, doc);
        verify_collection(win, doc);
        verify_equipment_storage(win, doc, catalog);
        verify_other_capacity(catalog);
        verify_scroll_input(layout, doc);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-enchantment-icons") {
        verify_enchantment_talisman_drop(win, catalog);
        capture_enchantment_icons(catalog, argv[2]);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 2 && std::string(argv[1]) == "--weapon-enchantment-only") {
        verify_enchantment_talisman_drop(win, catalog);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc == 2 && std::string(argv[1]) == "--revision-hammer-only") {
        verify_revision_hammer_drop(win, catalog);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    verify_polishing_drop(win, catalog);
    verify_revision_hammer_drop(win, catalog);
    verify_enchantment_talisman_drop(win, catalog);
    verify_enchantment_catalog(catalog);
    verify_sorting();
    verify_medicine_catalog(catalog);
    verify_emotes();
    verify_social_actions();
    verify_auto_hp_persistence(catalog);
    verify_auto_hp(catalog);
    verify_filters(layout, doc, catalog);
    verify_input(win, doc);
    verify_collection(win, doc);
    verify_collection_primary_bonuses();
    verify_collection_storage(catalog);
    verify_equipment_storage(win, doc, catalog);
    verify_other_capacity(catalog);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(win, doc, argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-auto-hp") {
        capture_auto_hp(catalog, argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--capture-polishing") {
        capture_polishing(catalog, argv[2]);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
