/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Nameplate.h"
#include "openkal/client/NpcCatalog.h"
#include "openkal/client/TradingPostWindow.h"
#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>

using namespace openkal;
int main(int argc, char** argv) {
    using test::expect;
    using namespace protocol;
    client::UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    client::TradingPostWindow window(layouts);
    client::WidgetTools ui;
    client::Bag bag;
    client::ItemDb items;
    client::NpcCatalog npcs;
    expect(npcs.load_json_dir(std::string(OPENKAL_REPO_ROOT) + "/config/npcs"),
           "load owned NPC definitions");
    const auto agent = std::find_if(npcs.spawns().begin(),
                                    npcs.spawns().end(),
                                    [](const auto& npc) { return npc.index() == 60003; });
    expect(
        agent != npcs.spawns().end() && agent->server_x() == 232525 &&
            agent->server_y() == 291711 && agent->server_z() == 45410 &&
            agent->overhead_icon() == "dragon_menu/trading_post_dialog.bmp" &&
            agent->name_offset_y(5) == 0 && client::NpcSpawn{}.name_offset_y(5) == 5 &&
            agent->name_offset_pixels() == 10 && client::NpcSpawn{}.name_offset_pixels() == 0 &&
            std::abs(client::NpcCatalog::yaw_from_dir(
                         32, agent->server_x(), agent->server_y(), agent->dir_x(), agent->dir_y()) *
                         180.f / 3.14159265f -
                     176.6f) < .05f,
        "trading-post agent preserves server coordinates, converted facing and configured overhead "
        "icon");
    expect(items.load(std::string(OPENKAL_REPO_ROOT) + "/config",
                      "",
                      std::string(OPENKAL_REPO_ROOT) + "/config/items"),
           "load owned market item catalog");
    client::ItemStack stack;
    stack.iid = 100;
    stack.index = 23000;
    stack.count = 50;
    bag.normal.push_back(stack);
    stack.iid = 101;
    stack.account_bound = true;
    bag.normal.push_back(stack);
    engine::FrameInput idle;
    idle.width = 1280;
    idle.height = 900;
    idle.dt = .03f;
    TradingPostState state;
    std::uint64_t serial = 1;
    window.set_open(true);
    window.update(state, serial, idle, 1, bag, items);
    auto request = window.handle(idle, ui, items);
    expect(request && request->command == TradingPostCommand::Browse,
           "opening market requests authoritative page");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    for (int i = 0; i < 20; ++i) {
        TradingPostEntry e;
        e.id = static_cast<std::uint64_t>(i + 1);
        e.item.index = i % 2 ? 23004 : 23000;
        e.item.count = 10;
        e.unit_price = i < 5 ? 100000 + i * 100000 : i == 5 ? 1500000 : 3000000;
        state.entries.push_back(e);
    }
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "1/1",
           "single-page offers show both current and total pages");
    expect(window.variables()["trading_post.range0"] == "100,000 - 3,000,000 Geons / item",
           "first ten sorted offers share their actual price range");
    expect(window.variables()["trading_post.range1"] == "3,000,000 Geons / item" &&
               window.variables()["trading_post.range2"].empty() &&
               window.variables()["trading_post.range4"].empty(),
           "equal-price offers continue across full rows and final partial row");
    const auto point = [&](const char* id, int slot = 0, bool quantity = false) {
        client::UiDocument doc;
        expect(doc.load(layouts.get(quantity ? "trading_post_quantity" : "trading_post").root()),
               "parse universal market layout");
        const auto* w = doc.find_widget(id);
        expect(w != nullptr, "market widget exists");
        auto in = idle;
        const float x =
            (1280 - doc.window_w() - doc.pad_left() - doc.pad_right()) / 2 + doc.pad_left();
        const float y =
            (900 - doc.window_h() - doc.pad_top() - doc.pad_bottom()) / 2 + doc.pad_top();
        float px, py, width, height;
        client::UiView::widget_rect(
            *w, nullptr, x, y, doc.window_w(), doc.window_h(), 1, px, py, width, height);
        in.mouse_x = px + (w->type() == client::UiWidgetType::IconGrid ? slot * w->pitch_x() + 16
                                                                       : width / 2);
        in.mouse_y = py + height / 2;
        return in;
    };
    const auto tap = [&](const char* id, int slot = 0, bool quantity = false) {
        auto in = point(id, slot, quantity);
        in.mouse_left = in.mouse_left_clicked = true;
        return window.handle(in, ui, items);
    };
    (void) tap("refresh");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    state.total_pages = 207;
    state.more = true;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "1/207",
           "first page displays the authoritative total");
    (void) tap("next");
    request = window.handle(idle, ui, items);
    expect(request && request->filter.page == 1, "next requests the second page");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    state.page = 1;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "2/207", "second page retains the total");
    (void) tap("refresh");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    state.page = 0;
    state.total_pages = 1;
    state.more = false;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "1/1",
           "server-clamped page replaces the client page after results shrink");
    expect(!tap("row0") && !tap("act"), "selecting offer opens shop-style quantity controls");
    auto typing = idle;
    typing.text_chars[0] = '4';
    typing.text_char_count = 1;
    (void) window.handle(typing, ui, items);
    auto enter = idle;
    enter.key_enter_pressed = true;
    request = window.handle(enter, ui, items);
    expect(request && request->command == TradingPostCommand::Buy && request->quantity == 4 &&
               request->unit_price == 100000,
           "quantity confirmation sends desired portion and displayed unit price");
    expect(!window.handle(enter, ui, items), "pending command blocks duplicate purchase");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    state.purchased = 2;
    state.entries.erase(state.entries.begin());
    state.status = "Only 2 of 4 items remained; that quantity was purchased.";
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.selected"].empty(),
           "sold listing is removed and its selection cleared");
    (void) tap("tab_sell");
    request = window.handle(idle, ui, items);
    expect(request && request->view == TradingPostView::Sell, "sell tab requests refresh");
    if (!request) {
        return 1;
    }
    state = {};
    state.view = TradingPostView::Sell;
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "1/1",
           "sell page count uses eligible inventory rather than stack quantity");
    auto sale_bag = bag;
    for (int i = 0; i < 60; ++i) {
        auto extra = stack;
        extra.iid = 200 + i;
        extra.account_bound = false;
        sale_bag.normal.push_back(extra);
    }
    window.update(state, serial, idle, 1, sale_bag, items);
    expect(window.variables()["trading_post.page"] == "1/2",
           "sixty-one eligible inventory stacks occupy two sale pages");
    (void) tap("next");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    state.page = 0;
    window.update(state, ++serial, idle, 1, sale_bag, items);
    expect(window.variables()["trading_post.page"] == "2/2",
           "sale page survives the inventory-independent server acknowledgement");
    window.update(state, serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.page"] == "1/1",
           "shrinking sale inventory clamps to its remaining page");
    (void) tap("row0");
    expect(window.variables()["trading_post.selected"] == "50 available",
           "sell view displays unbound inventory quantity");
    (void) tap("act");
    expect(window.variables()["trading_post.price"] == "Enter price",
           "listing requires an explicit price instead of silently defaulting to one Geon");
    (void) tap("price");
    auto price_input = idle;
    const std::string unit_price = "123456";
    price_input.text_char_count = static_cast<int>(unit_price.size());
    std::copy(unit_price.begin(), unit_price.end(), price_input.text_chars);
    (void) window.handle(price_input, ui, items);
    expect(window.variables()["trading_post.price"] == "123456|",
           "unit price accepts numeric input");
    expect(window.variables()["trading_post.price_color"] == "#00FF00",
           "entered unit price uses the matching Geon color");
    (void) tap("act");
    expect(window.variables()["trading_post.quantity_unit_price"] == "Unit price: 123,456 Geons",
           "quantity confirmation displays the entered unit price");
    std::vector<client::UiTextColorRun> geon_colors;
    (void) client::ui_interpolate(
        "${trading_post.quantity_total:geons}", window.variables(), &geon_colors);
    expect(geon_colors.size() == 1 && geon_colors[0].color == "#00FF00",
           "quantity dialog colors its actual requested total");
    (void) tap("quantity_increase", 0, true);
    (void) tap("quantity_add10", 0, true);
    expect(window.variables()["trading_post.quantity"] == "12|",
           "quantity presets add to an edited amount");
    (void) tap("quantity_max", 0, true);
    (void) client::ui_interpolate(
        "${trading_post.quantity_total:geons}", window.variables(), &geon_colors);
    expect(geon_colors.size() == 1 && geon_colors[0].color == "#FFFF00",
           "larger stack total changes tier independently of its green unit price");
    request = tap("quantity_confirm", 0, true);
    expect(request && request->command == TradingPostCommand::Sell && request->iid == 100 &&
               request->quantity == 50 && request->unit_price == 123456,
           "shop Max control sends selected stack with the entered unit price");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0", 1);
    expect(window.variables()["trading_post.selected"] == "50 available",
           "account-bound inventory is absent from sale choices");
    (void) tap("tab_buy");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state = {};
    state.view = TradingPostView::Buy;
    state.acknowledged = request->operation;
    for (int i = 0; i < 20; ++i) {
        TradingPostEntry e;
        e.id = static_cast<std::uint64_t>(i + 100);
        e.item.index = i % 2 ? 23004 : 23000;
        e.item.count = 25;
        e.unit_price = i < 5 ? 100000 + i * 100000 : i == 5 ? 1500000 : 3000000;
        state.entries.push_back(e);
    }
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("filter_type");
    auto choose = point("filter_type");
    choose.mouse_y += 14.5f + 4 * 26 + 13;
    choose.mouse_left = choose.mouse_left_clicked = true;
    (void) window.handle(choose, ui, items);
    request = window.handle(idle, ui, items);
    expect(request && request->filter.type == TradingPostType::Weapon,
           "type dropdown sends selected equipment category");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    client::ItemDb armor_items;
    expect(armor_items.load(std::string(OPENKAL_REPO_ROOT) + "/config",
                            "",
                            std::string(OPENKAL_REPO_ROOT) + "/tests/fixtures/trading_post/items"),
           "load synthetic armor catalog for grade filters");
    const auto select_armor_option = [&](const char* field, int row) {
        auto input = point(field);
        input.mouse_left = input.mouse_left_clicked = true;
        (void) window.handle(input, ui, armor_items);
        if (row >= 8) {
            input.mouse_left = input.mouse_left_clicked = false;
            input.mouse_scroll = -(row - 7);
            (void) window.handle(input, ui, armor_items);
            input.mouse_scroll = 0;
            input.mouse_left = input.mouse_left_clicked = true;
            row = 7;
        }
        input.mouse_y += 14.5f + row * 26 + 13;
        (void) window.handle(input, ui, armor_items);
        return window.handle(idle, ui, armor_items);
    };
    for (const auto type : {TradingPostType::Chest, TradingPostType::Shorts}) {
        request = select_armor_option("filter_type", static_cast<int>(type) + 1);
        expect(request && request->filter.type == type, "select catalog armor category");
        if (!request) {
            return 1;
        }
        state.acknowledged = request->operation;
        window.update(state, ++serial, idle, 1, bag, armor_items);
        request = select_armor_option("filter_grade", 1);
        expect(request && request->filter.type == type &&
                   request->filter.grade == (type == TradingPostType::Chest ? 50 : 46),
               "armor grade dropdown sends the selected category's catalog grade");
        if (!request) {
            return 1;
        }
        state.acknowledged = request->operation;
        window.update(state, ++serial, idle, 1, bag, armor_items);
    }
    (void) tap("filter_type");
    choose = point("filter_type");
    choose.mouse_y += 14.5f + 13;
    choose.mouse_left = choose.mouse_left_clicked = true;
    (void) window.handle(choose, ui, items);
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0");
    (void) tap("act");
    auto clear_quantity = idle;
    clear_quantity.key_backspace_pressed = true;
    (void) window.handle(clear_quantity, ui, items);
    auto escape_quantity = idle;
    escape_quantity.key_escape_pressed = true;
    (void) window.handle(escape_quantity, ui, items);
    (void) tap("tab_own");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Own;
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0");
    request = tap("act");
    expect(request && request->command == TradingPostCommand::Cancel && request->quantity == 1,
           "withdrawing an offer is independent of an abandoned empty quantity draft");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("tab_buy");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Buy;
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("tab_collection");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Collection;
    state.acknowledged = request->operation;
    const auto offers = state.entries;
    state.entries.resize(3);
    state.entries[0].delivery = TradingPostDelivery::Purchase;
    state.entries[1].delivery = TradingPostDelivery::Proceeds;
    state.entries[2].delivery = TradingPostDelivery::Returned;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0", 1);
    expect(window.variables()["trading_post.selected"] == "Sale proceeds: 5,000,000 Geons" &&
               window.variables()["trading_post.selected_color"] == "#FFFF00" &&
               window.variables()["trading_post.range0"] ==
                   "Items and sale proceeds awaiting NPC collection",
           "collection presents seller's Geons explicitly instead of an item price range");
    expect(!tap("act"), "collection receipts cannot be purchased or withdrawn again");
    (void) tap("row0");
    expect(window.variables()["trading_post.selected"] == "Purchased items",
           "buyer receipt remains a purchased item");
    (void) tap("row0", 2);
    expect(window.variables()["trading_post.selected"] == "Withdrawn items",
           "withdrawn items remain distinct from sale proceeds");
    (void) tap("tab_buy");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Buy;
    state.acknowledged = request->operation;
    state.entries = offers;
    window.update(state, ++serial, idle, 1, bag, items);
    window.show_collection(77);
    window.update(state, serial, idle, 1, bag, items);
    request = window.handle(idle, ui, items);
    expect(request && request->view == TradingPostView::Collection && request->npc_id == 77,
           "NPC collection opens the character's deliveries with its runtime NPC context");
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Collection;
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(window.variables()["trading_post.selected"].empty() &&
               window.variables()["trading_post.act"] == "Collect All",
           "Collection's primary action offers Collect All before any item is selected");
    expect(!tap("collect_selected"), "individual pickup requires a selected receipt");
    request = tap("act");
    expect(request && request->command == TradingPostCommand::CollectAll && request->npc_id == 77 &&
               !request->listing && request->filter.page == 0,
           "Collect All works without selecting an item and starts at the first page");
    if (!request) {
        return 1;
    }
    expect(!tap("act"), "pending Collect All cannot be sent twice");
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    const auto deliveries = state.entries;
    state.entries.clear();
    (void) tap("refresh");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    expect(!tap("act"), "empty Collection disables Collect All");
    state.entries = deliveries;
    (void) tap("refresh");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0");
    request = tap("collect_selected");
    expect(
        request && request->command == TradingPostCommand::Collect &&
            request->listing == state.entries.front().id && request->npc_id == 77 &&
            window.variables()["trading_post.act"] == "Collect All",
        "NPC pickup sends the selected receipt and server-validated NPC without buying it again");
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    window.set_open(false);
    window.set_open(true);
    window.update(state, serial, idle, 1, bag, items);
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    (void) tap("row0");
    expect(!tap("act") && window.variables()["trading_post.act"] == "Visit Trading Post Agent",
           "reopening through the menu clears NPC pickup permission");
    expect(!tap("collect_selected"), "menu-only Collection also disables individual pickup");
    (void) tap("tab_buy");
    request = window.handle(idle, ui, items);
    if (!request) {
        return 1;
    }
    state.view = TradingPostView::Buy;
    state.acknowledged = request->operation;
    window.update(state, ++serial, idle, 1, bag, items);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        (void) tap("refresh");
        request = window.handle(idle, ui, items);
        if (!request) {
            return 1;
        }
        state.acknowledged = request->operation;
        state.total_pages = 207;
        state.more = true;
        window.update(state, ++serial, idle, 1, bag, items);
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        expect(renderer->init("Trading post verification", 1280, 900),
               "initialize market renderer");
        ui.init(*renderer, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        client::FreetypeFont font;
        expect(font.load(*renderer, font.find_system_sans(), 14), "load market TrueType font");
        for (int view = 0; view < 11; ++view) {
            if (view == 10) {
                window.show_collection(77);
                window.update(state, serial, idle, 1, bag, items);
                request = window.handle(idle, ui, items);
                if (!request) {
                    return 1;
                }
                state.view = TradingPostView::Collection;
                state.acknowledged = request->operation;
                state.entries.resize(3);
                state.entries[0].delivery = TradingPostDelivery::Purchase;
                state.entries[1].delivery = TradingPostDelivery::Proceeds;
                state.entries[1].unit_price = 200000;
                state.entries[2].delivery = TradingPostDelivery::Returned;
                window.update(state, ++serial, idle, 1, bag, items);
            }
            if (view == 8) {
                auto escape = idle;
                escape.key_escape_pressed = true;
                (void) window.handle(escape, ui, items);
                (void) tap("tab_buy");
                request = window.handle(idle, ui, items);
                if (!request) {
                    return 1;
                }
                state.view = TradingPostView::Buy;
                state.acknowledged = request->operation;
                state.entries.resize(60, state.entries.front());
                const std::int64_t prices[] = {
                    99999, 100000, 1000000, 10000000, 100000000, 1000000000};
                for (std::size_t i = 0; i < state.entries.size(); ++i) {
                    state.entries[i].id = 500 + i;
                    state.entries[i].unit_price = prices[i / 10];
                }
                window.update(state, ++serial, idle, 1, bag, items);
            }
            if (view == 1) {
                (void) tap("row0");
                (void) tap("act");
            }
            if (view == 2) {
                auto esc = idle;
                esc.key_escape_pressed = true;
                (void) window.handle(esc, ui, items);
                (void) tap("filter_type");
            }
            if (view == 3) {
                auto esc = idle;
                esc.key_escape_pressed = true;
                (void) window.handle(esc, ui, items);
                idle.width = 800;
                idle.height = 600;
                window.update(state, serial, idle, 1.5f, bag, items);
            }
            if (view == 4) {
                idle.width = 1280;
                idle.height = 900;
                window.update(state, serial, idle, 1, bag, items);
            }
            if (view == 5 || view == 6) {
                (void) tap(view == 5 ? "tab_collection" : "tab_sell");
                request = window.handle(idle, ui, items);
                if (request) {
                    state.acknowledged = request->operation;
                    state.view = request->view;
                    if (view == 5) {
                        state.entries.resize(3);
                        state.entries[0].delivery = TradingPostDelivery::Purchase;
                        state.entries[1].delivery = TradingPostDelivery::Proceeds;
                        state.entries[2].delivery = TradingPostDelivery::Returned;
                    }
                    window.update(state, ++serial, idle, 1, bag, items);
                }
                (void) tap("row0", view == 5 ? 1 : 0);
                if (view == 6) {
                    auto price = idle;
                    const std::string value = "250000";
                    price.text_char_count = static_cast<int>(value.size());
                    std::copy(value.begin(), value.end(), price.text_chars);
                    (void) window.handle(price, ui, items);
                }
            }
            if (view == 7) {
                (void) tap("act");
                auto amount = idle;
                amount.text_chars[0] = '3';
                amount.text_char_count = 1;
                (void) window.handle(amount, ui, items);
            }
            expect(renderer->begin_offscreen(idle.width, idle.height, .20f, .25f, .22f),
                   "begin market capture");
            auto in = view == 4 || view == 5 ? point("row0", view == 5 ? 1 : 0) : idle;
            if (view == 9) {
                font.draw(*renderer, 290, 180, "NPC overhead symbol / native", 1, 1, 1);
                font.draw(*renderer, 740, 180, "NPC overhead symbol / 150%", 1, 1, 1);
                const auto color = client::Nameplate::npc_color();
                for (const auto scale : {1.f, 1.5f}) {
                    const float x = scale == 1 ? 400 : 850;
                    const auto height = font.line_height() * client::Nameplate::kTextScale;
                    client::Nameplate::draw(
                        *renderer, font, ui, x, 450, "Trading Post Agent", color);
                    client::Nameplate::draw_npc_icon(
                        ui, x, 450, "dragon_menu/trading_post_dialog.bmp", height, scale);
                }
            } else {
                window.draw(*renderer, ui, font, items, in);
            }
            std::vector<std::uint8_t> rgba;
            expect(renderer->end_offscreen_rgba(rgba), "read market capture");
            std::ofstream out(std::string(argv[2]) + "-" + std::to_string(view) + ".ppm",
                              std::ios::binary);
            out << "P6\n" << idle.width << " " << idle.height << "\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
        }
        font.destroy(*renderer);
        ui.destroy();
        renderer->shutdown();
    }
    return test::g_failures ? 1 : 0;
}
