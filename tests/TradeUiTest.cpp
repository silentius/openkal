/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ItemDb.h"
#include "openkal/client/TradeWindow.h"
#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <algorithm>
#include <fstream>
#include <memory>

using namespace openkal;
int main(int argc, char** argv) {
    using test::expect;
    client::UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    client::TradeWindow window(layouts);
    client::WidgetTools ui;
    protocol::TradeState state;
    state.phase = protocol::TradePhase::Open;
    state.token = 1;
    state.revision = 1;
    state.partner = "OwnedTrader";
    state.inventory_slots = 216;
    state.status = "Both players must accept the same offer.";
    state.available = {{42, 23000, 5, 0},
                       {43, 23004, 1, 1},
                       {44, 23029, 10, 54},
                       {45, 23027, 1, 3, protocol::kTradeCharacterBound}};
    state.other = {{99, 23004, 3, 5}};
    state.partner_money = 125;
    std::uint64_t serial = 1;
    window.update(state, serial, 1280, 900, 1);
    const auto point = [&](const char* id, int slot = 0, const char* name = "trade") {
        client::UiDocument doc;
        expect(doc.load(layouts.get(name).root()), "parse universal trade layout");
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "trade control exists");
        const float x =
            (1280 - doc.window_w() - doc.pad_left() - doc.pad_right()) / 2 + doc.pad_left();
        const float y =
            (900 - doc.window_h() - doc.pad_top() - doc.pad_bottom()) / 2 + doc.pad_top();
        float px, py, w, h;
        client::UiView::widget_rect(
            *widget, nullptr, x, y, doc.window_w(), doc.window_h(), 1, px, py, w, h);
        engine::FrameInput in;
        in.width = 1280;
        in.height = 900;
        in.dt = .03f;
        in.mouse_x = px + (widget->type() == client::UiWidgetType::IconGrid
                               ? (slot % 6) * widget->pitch_x() + 16
                               : w / 2);
        in.mouse_y = py + (widget->type() == client::UiWidgetType::IconGrid
                               ? (slot / 6) * widget->pitch_y() + 16
                               : h / 2);
        return in;
    };
    const auto tap = [&](const char* id, int slot = 0, const char* name = "trade") {
        auto in = point(id, slot, name);
        in.mouse_left = true;
        in.mouse_left_clicked = true;
        auto result = window.handle(in, ui);
        in.mouse_left = false;
        in.mouse_left_clicked = false;
        auto released = window.handle(in, ui);
        return result ? result : released;
    };
    const auto amount = [&](const char* text) {
        engine::FrameInput in;
        in.dt = .03f;
        in.text_char_count = static_cast<int>(std::char_traits<char>::length(text));
        std::copy(text, text + in.text_char_count, in.text_chars);
        (void) window.handle(in, ui);
        in.text_char_count = 0;
        in.key_enter_pressed = true;
        return window.handle(in, ui);
    };
    const auto acknowledge = [&](const protocol::TradeRequest& request) {
        state.acknowledged = request.operation;
        if (request.command == protocol::TradeCommand::Offer) {
            state.own.clear();
            for (const auto& offered : request.items) {
                const auto source =
                    std::find_if(state.available.begin(),
                                 state.available.end(),
                                 [&](const auto& v) { return v.iid == offered.iid; });
                auto item = *source;
                item.slot = offered.slot;
                item.count = offered.count;
                state.own.push_back(item);
            }
            state.money = request.money;
            ++state.revision;
            state.confirmed = state.partner_confirmed = false;
        } else if (request.command == protocol::TradeCommand::Confirm) {
            state.confirmed = true;
        }
        window.update(state, ++serial, 1280, 900, 1);
    };
    expect(!tap("inventory_grid", 0), "single inventory click changes no offer");
    expect(!tap("inventory_grid", 0), "double click opens stack quantity selector");
    auto request = amount("2");
    expect(request && request->command == protocol::TradeCommand::Offer &&
               request->items == std::vector<protocol::TradeAmount>{{42, 2, 0}},
           "double click publishes selected quantity automatically");
    if (!request) {
        return 1;
    }
    expect(!tap("confirm"), "pending item change cannot be accepted before server acknowledgement");
    // An unrelated partner snapshot must not acknowledge our pending edit.
    state.partner_confirmed = true;
    window.update(state, ++serial, 1280, 900, 1);
    expect(!tap("confirm"), "partner traffic cannot release our pending edit latch");
    acknowledge(*request);
    request = tap("confirm");
    expect(request && request->command == protocol::TradeCommand::Confirm && request->revision == 2,
           "Accept confirms exact server revision");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    expect(!tap("confirm"), "already accepted offer cannot accept again");
    ++state.revision;
    state.confirmed = state.partner_confirmed = false;
    state.partner_money = 200;
    window.update(state, ++serial, 1280, 900, 1);
    request = tap("confirm");
    expect(request && request->command == protocol::TradeCommand::Confirm,
           "peer offer change revokes acceptance and enables review and reacceptance");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    (void) tap("inventory_grid", 1);
    request = tap("inventory_grid", 1);
    expect(request && request->items.size() == 2 && request->items.back().iid == 43,
           "single item double click moves directly into own offer");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    auto from = point("inventory_grid", 0), to = point("own_grid", 11);
    from.mouse_left = from.mouse_left_clicked = true;
    (void) window.handle(from, ui);
    to.mouse_left = true;
    (void) window.handle(to, ui);
    to.mouse_left = false;
    expect(!window.handle(to, ui), "dropping a stack into cell twelve asks for its quantity");
    request = amount("1");
    expect(request && std::find(request->items.begin(),
                                request->items.end(),
                                protocol::TradeAmount{42, 3, 11}) != request->items.end(),
           "drag and drop adds a partial stack at the actual destination cell");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    from = point("own_grid", 11);
    to = point("inventory_grid", 2);
    from.mouse_left = from.mouse_left_clicked = true;
    (void) window.handle(from, ui);
    to.mouse_left = true;
    (void) window.handle(to, ui);
    to.mouse_left = false;
    request = window.handle(to, ui);
    expect(request && request->items.size() == 1 && request->items[0].iid == 43,
           "dragging own offer back to inventory withdraws it");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    auto right = point("own_grid", 1);
    right.mouse_right_clicked = true;
    request = window.handle(right, ui);
    expect(request && request->items.empty(), "right click withdraws an offered item");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    (void) tap("scroll_down");
    (void) tap("scroll_up");
    auto wheel = point("inventory_grid", 0);
    wheel.mouse_scroll = -2;
    expect(!window.handle(wheel, ui), "inventory wheel scroll is local and sends no command");
    // After scrolling two rows, source cell 54 is visible at the physical position of cell 42.
    (void) tap("inventory_grid", 42);
    (void) tap("inventory_grid", 42);
    request = amount("3");
    expect(request && request->items == std::vector<protocol::TradeAmount>{{44, 3, 0}},
           "scrolled grid preserves underlying inventory cell identity");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    (void) tap("scroll_up");
    (void) tap("scroll_up");
    expect(!tap("inventory_grid", 3) && !tap("inventory_grid", 3),
           "bound inventory item remains visible but cannot enter offer");
    (void) tap("money");
    engine::FrameInput typing;
    typing.dt = .03f;
    typing.text_chars[0] = '5';
    typing.text_char_count = 1;
    request = window.handle(typing, ui);
    expect(request && request->money == 5,
           "editing Geons publishes change without an update button");
    if (!request) {
        return 1;
    }
    typing.text_chars[0] = '0';
    expect(!window.handle(typing, ui), "fast Geon typing is buffered during acknowledgement");
    acknowledge(*request);
    engine::FrameInput idle;
    idle.dt = .03f;
    request = window.handle(idle, ui);
    expect(request && request->money == 50,
           "buffered Geon edit is published after prior acknowledgement");
    if (!request) {
        return 1;
    }
    acknowledge(*request);
    request = tap("reject");
    expect(request && request->command == protocol::TradeCommand::Cancel,
           "Reject cancels the entire trade");
    state = {};
    window.update(state, ++serial, 1280, 900, 1);
    expect(!tap("confirm"), "closed trade ignores input");
    state.phase = protocol::TradePhase::Invitation;
    state.token = 2;
    state.revision = 1;
    state.inviter = true;
    state.partner = "OwnedTrader";
    window.update(state, ++serial, 1280, 900, 1);
    expect(!tap("accept", 0, "trade_invitation"), "inviter cannot accept own invitation");
    state.inviter = false;
    window.update(state, ++serial, 1280, 900, 1);
    request = tap("accept", 0, "trade_invitation");
    expect(request && request->command == protocol::TradeCommand::Accept,
           "recipient can accept invitation");
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        expect(renderer->init("Trade grid verification", 1280, 900), "initialize renderer");
        ui.init(*renderer, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        client::FreetypeFont font;
        expect(font.load(*renderer, font.find_system_sans(), 14), "load TrueType font");
        client::ItemDb items;
        expect(items.load(std::string(OPENKAL_REPO_ROOT) + "/config",
                          "",
                          std::string(OPENKAL_REPO_ROOT) + "/config/items"),
               "load owned item catalog");
        state = {};
        state.phase = protocol::TradePhase::Open;
        state.token = 3;
        state.revision = 1;
        state.inventory_slots = 216;
        state.partner = "Companion";
        state.status = "Both players must accept. Reject cancels the trade.";
        state.money = 12500;
        state.partner_money = 7300;
        for (unsigned i = 0; i < 72; ++i) {
            state.available.push_back(
                {1000 + i, i % 2 ? 23004 : 23000, 20, static_cast<std::uint16_t>(i)});
        }
        for (unsigned i = 0; i < 12; ++i) {
            state.own.push_back(
                {1000 + i, i % 2 ? 23004 : 23000, 3, static_cast<std::uint16_t>(i)});
            state.other.push_back(
                {2000 + i, i % 2 ? 23029 : 23027, 5, static_cast<std::uint16_t>(i)});
        }
        for (int view = 0; view < 4; ++view) {
            const int w = view == 2 ? 800 : 1280, h = view == 2 ? 600 : 900;
            state.phase = view == 1 ? protocol::TradePhase::Invitation : protocol::TradePhase::Open;
            window.update(state, ++serial, w, h, view == 2 ? 1.5f : 1.f);
            if (view == 3) {
                (void) tap("own_grid", 0);
                (void) tap("own_grid", 0);
            }
            expect(renderer->begin_offscreen(w, h, .20f, .25f, .22f),
                   "begin rendered grid capture");
            engine::FrameInput in;
            in.width = w;
            in.height = h;
            window.draw(*renderer, ui, font, items, in);
            std::vector<std::uint8_t> rgba;
            expect(renderer->end_offscreen_rgba(rgba), "read grid capture");
            std::ofstream out(std::string(argv[2]) + "-" + std::to_string(view) + ".ppm",
                              std::ios::binary);
            out << "P6\n" << w << " " << h << "\n255\n";
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
