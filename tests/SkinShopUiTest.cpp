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
#include "openkal/client/SkinShopWindow.h"
#include "openkal/client/TextWrap.h"
#include "openkal/client/WeaponThumbnailCamera.h"
#include "openkal/platform/NpcDefinitions.h"
#include "openkal/server/ShopCatalog.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <png.h>

namespace {
    void test_weapon_thumbnail_camera() {
        using namespace openkal;
        using protocol::WeaponSkinType;
        using test::expect;
        for (const auto hi : {engine::Vec3{9, 2, 1}, engine::Vec3{9, 1, 3}}) {
            const auto camera = client::weapon_thumbnail_camera(
                {-12, -hi.y, -hi.z}, hi, WeaponSkinType::Wand, 1.2f);
            const auto head = engine::Mat4::transform_point(camera.view, -12, 0, 0);
            const auto foot = engine::Mat4::transform_point(camera.view, 9, 0, 0);
            expect(head.y > foot.y && std::abs(head.x - foot.x) < 1e-5f,
                   "staff head stays above the foot for either ornament plane");
        }
        for (const auto hi : {engine::Vec3{3, 1, 10}, engine::Vec3{3, 12, 10}}) {
            const engine::Vec3 lo{-4, -hi.y, -10};
            for (const float aspect : {.6f, 1.2f}) {
                const auto camera =
                    client::weapon_thumbnail_camera(lo, hi, WeaponSkinType::Bow, aspect);
                const auto left = engine::Mat4::transform_point(camera.view, -4, 0, 0);
                const auto right = engine::Mat4::transform_point(camera.view, 3, 0, 0);
                const auto top = engine::Mat4::transform_point(camera.view, 0, 0, 10);
                const auto bottom = engine::Mat4::transform_point(camera.view, 0, 0, -10);
                expect(std::abs(right.x - left.x) > 6.9f && std::abs(right.z - left.z) < 1e-5f &&
                           top.y > bottom.y,
                       "bow keeps its full side profile and Z upright despite deep ornaments");
                const auto vp = engine::Mat4::multiply(camera.projection, camera.view);
                bool fits = true;
                for (const float x : {lo.x, hi.x}) {
                    for (const float y : {lo.y, hi.y}) {
                        for (const float z : {lo.z, hi.z}) {
                            const auto clip = engine::Mat4::transform_point(vp, x, y, z);
                            const float w = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
                            fits = fits && w > 0 && std::abs(clip.x) < w && std::abs(clip.y) < w &&
                                   std::abs(clip.z) < w;
                        }
                    }
                }
                expect(fits, "bow bounds fit inside the thumbnail including deep ornaments");
            }
        }
        for (const auto type : {WeaponSkinType::None,
                                WeaponSkinType::Sword,
                                WeaponSkinType::Sword2h,
                                WeaponSkinType::Dagger}) {
            const auto camera =
                client::weapon_thumbnail_camera({-2, -10, -.5f}, {2, 10, .5f}, type, 1.2f);
            const auto top = engine::Mat4::transform_point(camera.view, 0, 10, 0);
            const auto bottom = engine::Mat4::transform_point(camera.view, 0, -10, 0);
            expect(top.y > bottom.y && std::abs(top.x - bottom.x) < 1e-5f,
                   "swords, daggers and shields retain their bounds-based orientation");
        }
    }
}  // namespace

int main(int argc, char** argv) {
    using namespace openkal;
    using test::expect;
    test_weapon_thumbnail_camera();
    const std::string repo = OPENKAL_REPO_ROOT;
    const auto item_dir = test::temp_dir("skin_shop_ui_items");
    std::filesystem::remove_all(item_dir);
    std::filesystem::copy(
        repo + "/config/items", item_dir, std::filesystem::copy_options::recursive);
    for (int type = 0; type < 6; ++type) {
        static const char* names[] = {"sword", "sword2h", "wand", "bow", "dagger", "shield"};
        const int clazz = type < 2 || type == 5 ? 0 : type - 1;
        std::ofstream out(item_dir + "/preview_" + std::to_string(type) + ".json");
        out << "{\"index\":" << 100 + type
            << ",\"name\":\"Synthetic preview equipment\",\"class\":[\""
            << (type == 5 ? "defense" : "weapon") << "\",\"" << names[type]
            << "\"],\"action\":[1,1],\"limit\":{\"class\":\""
            << (clazz == 0   ? "knight"
                : clazz == 1 ? "magician"
                : clazz == 2 ? "archer"
                             : "thief")
            << "\"}}";
    }
    // An owned fixture only; no production item ID or catalog entry is allocated.
    const std::string label_name =
        u8"Owned … Étoile — ceremonial preview skin with a deliberately long title (Knight)";
    std::ofstream(item_dir + "/65000_owned_label_skin.json")
        << "{\"index\":65000,\"name\":\"" << label_name
        << "\",\"class\":[\"skin\",\"body\"],\"action\":[7, 1],\"wear\":1,"
           "\"limit\":{\"class\":\"knight\"}}";
    client::ItemDb items;
    expect(items.load(repo + "/config", "", item_dir), "load project skins without external data");
    client::UiLayout layout;
    expect(layout.load_file(repo + "/config/ui/skin_shop.json"), "load universal shop layout");
    client::SkinShopWindow window;
    window.set_layout(&layout);
    window.sync_from_layout();
    window.center_on_screen(1100, 900);
    protocol::ShopItemList list;
    list.npc_id = 7;
    list.currency_index = 23030;
    server::GameWorld world;
    expect(world.load_items_json_dir(repo + "/config/items"),
           "load authoritative skin definitions");
    const auto shop_dir = test::temp_dir("skin_shop_ui_catalog");
    std::filesystem::copy_file(repo + "/config/server/shops/60002_skin_artisan.json",
                               shop_dir + "/60002.json",
                               std::filesystem::copy_options::overwrite_existing);
    server::ShopCatalog shops;
    expect(shops.load_dir(shop_dir, world.item_defs()), "load authoritative skin offers");
    std::filesystem::remove_all(shop_dir);
    const auto* artisan = shops.find(60002);
    expect(artisan != nullptr, "find Skin Artisan");
    if (!artisan) {
        return 1;
    }
    for (const int index : artisan->items) {
        list.offers.push_back({static_cast<std::uint16_t>(index),
                               static_cast<std::uint32_t>(artisan->prices.at(index))});
    }
    for (int clazz = 0; clazz < 4; ++clazz) {
        window.show(
            list, client::SkinShopCategory::Weapons, static_cast<client::Class>(clazz), items);
        expect(window.groups().size() == (clazz == 0 ? 2u : 1u),
               "only Knight has separate one-hand and two-hand blocks");
        int shields = 0;
        int g56 = 0;
        int g59 = 0;
        int frost_feather = 0;
        for (const auto& group : window.groups()) {
            for (const auto& offer : group.offers) {
                const auto* item = items.find(offer.index);
                expect(offer.index != 25303 && offer.index != 25313 && offer.index != 25364,
                       "reported unusable and duplicate skins are absent from the shop UI");
                if (item && item->name() == "Frost Feather Bow Skin (Archer)") {
                    ++frost_feather;
                }
                expect(item && item->class_limit() == clazz, "foreign-class offers never appear");
                if (offer.index >= 25771 && offer.index <= 25807) {
                    if (offer.index <= 25786) {
                        ++g56;
                    } else {
                        ++g59;
                    }
                    expect(offer.buy == 2, "G56 and G59 weapon skins cost two tickets");
                }
                if (clazz == 0 && item && item->skin_slot() == protocol::SkinSlot::Weapon) {
                    expect((group.label == "Two-handed weapons") ==
                               (item->weapon_skin_type() == protocol::WeaponSkinType::Sword2h),
                           "normal, costume and Darkness swords use their explicit hand type");
                }
                if (item && item->skin_slot() == protocol::SkinSlot::Shield) {
                    ++shields;
                    expect(group.label != "Two-handed weapons", "shield belongs to one-hand block");
                }
            }
        }
        expect(shields == (clazz == 0 ? 13 : 0),
               "Knight shields appear with weapons and the Archer arrow is not offered");
        expect(frost_feather == (clazz == 2 ? 1 : 0),
               "Frost Feather Bow appears once in the Archer catalog");
        expect(g59 == (clazz == 0   ? 8
                       : clazz == 1 ? 6
                       : clazz == 3 ? 3
                                    : 4),
               "every class can browse all of its G59 weapon variants");
        expect(g56 == (clazz == 0   ? 8
                       : clazz == 3 ? 0
                                    : 4),
               "the retained G56 offers remain separate from G59 appearances");
        window.show(
            list, client::SkinShopCategory::FullBody, static_cast<client::Class>(clazz), items);
        expect(window.groups().size() == 1, "full skins have one block");
        for (const auto& offer : window.groups().front().offers) {
            expect(items.find(offer.index)->full_body_skin(),
                   "partial costumes are excluded from full-body list");
        }
        window.show(
            list, client::SkinShopCategory::Parts, static_cast<client::Class>(clazz), items);
        expect(window.groups().size() == 6, "parts have Head Chest Hands Legs Feet Face blocks");
        for (const auto slot : {protocol::SkinSlot::Helmet,
                                protocol::SkinSlot::Upper,
                                protocol::SkinSlot::Gauntlet,
                                protocol::SkinSlot::Pants,
                                protocol::SkinSlot::Boots}) {
            int basic = 0;
            int azure = 0;
            for (const auto& group : window.groups()) {
                for (const auto& offer : group.offers) {
                    const auto* item = items.find(offer.index);
                    if (item->skin_slot() != slot) {
                        continue;
                    }
                    basic += item->action_class() == 1 && item->action_index() == 1;
                    azure += item->action_class() == 14 && item->action_index() == 1;
                    expect(item->class_limit() == clazz,
                           "set parts always match the browsing player's class");
                    expect(offer.buy == 2, "each class set part costs two tickets");
                }
            }
            expect(basic == 1 && azure == 1,
                   "both class sets offer exactly one matching part for every body slot");
        }
    }
    // An incoming partial skin replaces the full costume in the detached mirror only.
    client::Wear actual;
    actual.apply_item(*items.find(25006));
    const auto mirrored = client::SkinShopWindow::preview_wear(actual, *items.find(25037), items);
    expect(actual.skin_index(protocol::SkinSlot::Upper) == 25006 && actual.has_full_body_skin(),
           "trying on a skin never changes the real outfit");
    expect(!mirrored.has_full_body_skin() &&
               mirrored.skin_index(protocol::SkinSlot::Boots) == 25037,
           "partial-skin preview resolves full-body conflict");
    client::Wear two_handed;
    two_handed.set_weapon_index(101);
    const auto shield_preview =
        client::SkinShopWindow::preview_wear(two_handed, *items.find(25042), items);
    expect(
        two_handed.weapon_index() == 101 && shield_preview.weapon_index() == 100 &&
            shield_preview.has_shield(),
        "shield preview uses a one-hand display weapon without changing the real two-hand weapon");
    client::Bag bag;
    client::ItemStack tickets;
    tickets.iid = 1;
    tickets.index = 23030;
    tickets.count = 8;
    bag.normal.push_back(tickets);
    {
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        const bool initialized = renderer && renderer->init("Owned shop labels", 1100, 900);
        expect(initialized, "initialize the configured engine for visible shop card labels");
        if (initialized) {
            client::FreetypeFont font;
            expect(font.load(*renderer, client::FreetypeFont::find_system_sans(), 13),
                   "load TrueType for the normal shop-label rendering guard");
            protocol::ShopItemList owned_list;
            owned_list.npc_id = 77;
            owned_list.currency_index = 23030;
            owned_list.offers = {{65000, 2}};
            client::SkinShopWindow labels;
            labels.set_layout(&layout);
            labels.show(
                owned_list, client::SkinShopCategory::FullBody, client::Class::Knight, items);
            labels.center_on_screen(1100, 900);
            const auto* item = items.find(65000);
            expect(item && item->name() == label_name && labels.groups().size() == 1 &&
                       labels.groups().front().offers.size() == 1 &&
                       labels.groups().front().offers.front().index == 65000,
                   "the visible card retains its owned UTF-8 name and authoritative offer");
            auto display_name = label_name.substr(0, label_name.rfind(" ("));
            const auto expected_lines = client::layout_text_lines(
                display_name, 160, [&](std::string_view text) { return font.text_width(text); });
            expect(font.ready() && expected_lines.size() > 2,
                   "the long card name exercises wrapping and the caller's two-line limit");
            client::WidgetTools ui;
            ui.init(*renderer, "", std::string(OPENKAL_RUNTIME_ROOT));
            std::unordered_map<std::string, engine::TextureHandle> textures;
            renderer->begin_frame(0, 0, 0);
            labels.draw(*renderer,
                        ui,
                        font,
                        bag,
                        items,
                        actual,
                        "",
                        1,
                        1,
                        textures,
                        engine::Mat4::identity(),
                        engine::Mat4::identity());
            renderer->end_frame();
            expect(labels.open() && labels.npc_id() == 77 && labels.selected_index() == 0 &&
                       bag.count_item(23030) == 8,
                   "drawing visible wrapped card names leaves selection and ticket payload intact");
            labels.destroy(*renderer);
            font.destroy(*renderer);
            ui.destroy();
            for (const auto& entry : textures) {
                renderer->destroy_texture(entry.second);
            }
            renderer->shutdown();
        }
    }
    window.show(list, client::SkinShopCategory::FullBody, client::Class::Knight, items);
    window.center_on_screen(1100, 900);
    expect(window.handle_click(505, 325, bag, items) == client::SkinShopClickResult::Consumed &&
               window.selected_index() != 0,
           "clicking a card selects without purchasing");
    client::Bag empty;
    expect(window.handle_click(763, 730, empty, items) != client::SkinShopClickResult::Redeem,
           "empty live inventory disables redemption before drawing");
    expect(window.handle_click(763, 730, bag, items) == client::SkinShopClickResult::Redeem,
           "redemption requires an explicit button with enough tickets");
    const auto dir = test::temp_dir("skin_npc_standalone");
    std::filesystem::copy_file(repo + "/config/npcs/60002_skin_artisan.json",
                               dir + "/60002.json",
                               std::filesystem::copy_options::overwrite_existing);
    const auto npcs = platform::load_npc_definitions(dir);
    expect(npcs.size() == 1 && npcs.front().at("index").as_int() == 60002 &&
               npcs.front().at("name").as_string() == "Skin Artisan" &&
               npcs.front().at("html").as_int() == 990310 &&
               npcs.front().at("shape").as_int() == 73 && npcs.front().at("map").as_int() == 0 &&
               npcs.front().at("xy").array()[0].as_int() == 267261 &&
               npcs.front().at("xy").array()[1].as_int() == 243021 &&
               npcs.front().at("xy").array()[2].as_int() == 19630 &&
               npcs.front().at("appearance").at("boneId").as_int() == 953 &&
               !npcs.front().at("placement").is_object(),
           "standalone Skin Artisan retains placement and appearance without an external NPC "
           "template");
    std::filesystem::remove_all(dir);
    if (argc >= 4 && std::string(argv[1]) == "--preview") {
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        expect(renderer->init("OpenKal Skin Artisan", 1100, 900), "initialize preview engine");
        client::WidgetTools ui;
        ui.init(*renderer, argv[2], std::string(OPENKAL_RUNTIME_ROOT));
        client::FreetypeFont font;
        expect(font.load(*renderer, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13),
               "load preview TrueType font");
        std::unordered_map<std::string, engine::TextureHandle> textures;
        const auto preview_class =
            argc > 5 ? static_cast<client::Class>(std::clamp(std::atoi(argv[5]), 0, 3))
                     : client::Class::Knight;
        const auto category =
            argc > 4 && std::string(argv[4]) == "parts"     ? client::SkinShopCategory::Parts
            : argc > 4 && std::string(argv[4]) == "weapons" ? client::SkinShopCategory::Weapons
                                                            : client::SkinShopCategory::FullBody;
        if (argc > 6) {
            const int index = std::atoi(argv[6]);
            list.offers.erase(
                std::remove_if(list.offers.begin(),
                               list.offers.end(),
                               [index](const auto& offer) { return offer.index != index; }),
                list.offers.end());
        }
        window.show(list, category, preview_class, items);
        window.center_on_screen(1100, 900);
        (void) window.handle_click(505, 325, bag, items);
        for (int frame = 0; frame < 18; ++frame) {
            engine::FrameInput input;
            expect(renderer->poll(input), "poll preview");
            renderer->begin_frame(.07f, .08f, .09f);
            window.draw(*renderer,
                        ui,
                        font,
                        bag,
                        items,
                        actual,
                        argv[2],
                        1,
                        1,
                        textures,
                        engine::Mat4::identity(),
                        engine::Mat4::identity());
            renderer->end_frame();
        }
        expect(renderer->begin_offscreen(1100, 900, .07f, .08f, .09f), "begin UI capture");
        window.draw(*renderer,
                    ui,
                    font,
                    bag,
                    items,
                    actual,
                    argv[2],
                    1,
                    1,
                    textures,
                    engine::Mat4::identity(),
                    engine::Mat4::identity());
        std::vector<std::uint8_t> pixels;
        expect(renderer->end_offscreen_rgba(pixels), "capture shop");
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        image.width = 1100;
        image.height = 900;
        image.format = PNG_FORMAT_RGBA;
        expect(png_image_write_to_file(&image, argv[3], 0, pixels.data(), 0, nullptr) != 0,
               "write shop screenshot");
        window.destroy(*renderer);
        font.destroy(*renderer);
        ui.destroy();
        for (const auto& [name, t] : textures) {
            (void) name;
            renderer->destroy_texture(t);
        }
        renderer->shutdown();
    }
    std::filesystem::remove_all(item_dir);
    return test::g_failures ? 1 : 0;
}
