/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ChatWidget.h"
#include "openkal/client/PlayerOptionsWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/protocol/FrameParser.h"
#include "test_util.h"

#include <fstream>
#include <memory>
using namespace openkal;
using test::expect;
namespace {
    std::vector<std::uint8_t> payload(const std::vector<std::uint8_t>& frame) {
        std::size_t used = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> bytes;
        expect(protocol::FrameParser::pop(frame.data(), frame.size(), used, type, bytes) ==
                   protocol::FrameParseStatus::Complete,
               "context frame decodes");
        return bytes;
    }
    void protocol_test() {
        protocol::PlayerOptionsRequest r;
        r.request_id = 10;
        r.target = 7;
        r.guild = 44;
        r.expected_fee = 17;
        r.action = protocol::PlayerAction::ApplyGuild;
        auto bytes = payload(protocol::frame_player_options_request(r));
        protocol::PlayerOptionsRequest decoded;
        expect(protocol::parse_player_options_request(bytes.data(), bytes.size(), decoded) &&
                   decoded.target == 7 && decoded.guild == 44 && decoded.expected_fee == 17 &&
                   decoded.request_id == 10,
               "context request preserves target, guild, price, and correlation");
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            expect(!protocol::parse_player_options_request(bytes.data(), i, decoded),
                   "reject truncated context request");
        }
        auto bad = bytes;
        bad.push_back(0);
        expect(!protocol::parse_player_options_request(bad.data(), bad.size(), decoded),
               "reject trailing context request bytes");
        bad = bytes;
        bad[1] = 255;
        expect(!protocol::parse_player_options_request(bad.data(), bad.size(), decoded),
               "reject unknown context action");
        r.target = 0;
        expect(protocol::frame_player_options_request(r).empty(), "reject missing context target");
        protocol::PlayerOptionsState s;
        s.target = 7;
        s.request_id = 10;
        s.name = "GuildMember";
        s.guild = 44;
        s.guild_name = "OpenKal";
        s.application_fee = 17;
        s.actions = {protocol::PlayerAction::ApplyGuild};
        bytes = payload(protocol::frame_player_options_state(s));
        protocol::PlayerOptionsState state;
        expect(protocol::parse_player_options_state(bytes.data(), bytes.size(), state) &&
                   state.actions == s.actions && state.name == s.name,
               "context response preserves only offered actions");
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            expect(!protocol::parse_player_options_state(bytes.data(), i, state),
                   "reject truncated context response");
        }
        bad = bytes;
        bad.push_back(0);
        expect(!protocol::parse_player_options_state(bad.data(), bad.size(), state),
               "reject trailing context response");
        s.actions.push_back(protocol::PlayerAction::ApplyGuild);
        expect(protocol::frame_player_options_state(s).empty(), "reject duplicate actions");
    }
    void window_test(const std::string& capture) {
        client::UiLayoutStore layouts;
        layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
        expect(layouts.get("player_options").loaded() && layouts.get("player_option_row").loaded(),
               "context layouts load");
        client::UiDocument doc;
        client::UiDocument row;
        client::UiView::reload(row, &layouts.get("player_option_row"));
        expect(row.kind() == client::UiKind::Hud &&
                   row.chrome_kind() == client::UiChromeKind::Pieces,
               "repeated action rows have no independent window frame");
        client::UiView::reload(doc, &layouts.get("player_options"));
        client::UiDocument roundtrip;
        expect(roundtrip.load(doc.to_json()) && roundtrip.widgets().size() == doc.widgets().size(),
               "context layout editor roundtrip");
        client::PlayerOptionsWindow window;
        window.set_layouts(&layouts);
        client::WidgetTools ui;
        engine::FrameInput in;
        in.width = 1280;
        in.height = 800;
        window.show(7);
        auto r = window.handle(in, ui, 1);
        expect(r && r->action == protocol::PlayerAction::Query && r->target == 7,
               "opening queries selected player");
        window.request_sent(11);
        protocol::PlayerOptionsState state;
        state.target = 7;
        state.request_id = 10;
        state.name = "GuildMember";
        state.guild = 44;
        state.guild_name = "OpenKal";
        state.application_fee = 17;
        state.actions = {protocol::PlayerAction::InviteParty, protocol::PlayerAction::ApplyGuild};
        window.update(state);
        expect(window.variables().at("player_options.target").empty(),
               "stale request cannot replace context");
        state.request_id = 11;
        window.update(state);
        expect(window.variables().at("player_options.target") == "GuildMember",
               "matching server response opens offered actions");
        in.mouse_left_clicked = true;
        in.mouse_x = 640;
        in.mouse_y = 365;
        r = window.handle(in, ui, 1);
        expect(r && r->action == protocol::PlayerAction::InviteParty && r->target == 7,
               "first row sends party invitation for selected player");
        window.request_sent(12);
        state.request_id = 12;
        state.actions = {protocol::PlayerAction::ApplyGuild};
        window.update(state);
        r = window.handle(in, ui, 1);
        expect(r && r->action == protocol::PlayerAction::ApplyGuild && r->guild == 44 &&
                   r->expected_fee == 17,
               "remaining guild action reflows and preserves the quoted price");
        window.request_sent(13);
        state.request_id = 13;
        state.actions.clear();
        window.update(state);
        r = window.handle(in, ui, 1);
        expect(!r && window.variables().at("player_options.empty") == "1",
               "unavailable actions disappear and cannot be clicked");
        const auto* pm_button = doc.find_widget("send_pm");
        expect(pm_button && pm_button->action() == "player_options.send_pm",
               "O menu includes the universal Send PM button");
        const float pm_height = std::max(doc.window_h(), 156.0f);
        float px = 0, py = 0, pw = 0, ph = 0;
        if (pm_button) {
            client::UiView::widget_rect(*pm_button,
                                        nullptr,
                                        (in.width - 420.0f) / 2,
                                        (in.height - pm_height) / 2,
                                        420,
                                        pm_height,
                                        1,
                                        px,
                                        py,
                                        pw,
                                        ph);
        }
        in.mouse_x = px + pw / 2;
        in.mouse_y = py + ph / 2;
        r = window.handle(in, ui, 1);
        expect(!r && !window.open(), "Send PM closes the context without a server mutation");
        const auto partner = window.take_whisper_target();
        expect(partner == "GuildMember" && window.take_whisper_target().empty(),
               "Send PM exposes the selected authenticated name exactly once");
        client::ChatWidget compose;
        compose.begin_whisper(partner);
        engine::FrameInput text;
        text.text_chars[0] = 'H';
        text.text_char_count = 1;
        (void) compose.handle_input(text, 0, 0, 450, 226, 1, nullptr, {});
        text = {};
        text.key_enter_pressed = true;
        std::string sent;
        (void) compose.handle_input(
            text, 0, 0, 450, 226, 1, nullptr, [&](std::string_view value) { sent = value; });
        expect(sent == "@GuildMember H", "O menu PM action fills and focuses the chat composer");
        window.show(8);
        expect(window.take_whisper_target().empty(),
               "opening another player clears local PM actions");
        window.update(state);
        expect(window.variables().at("player_options.target").empty(),
               "old target response is ignored after selection change");
        in.mouse_left_clicked = false;
        (void) window.handle(in, ui, 1);
        in.key_o_pressed = true;
        (void) window.handle(in, ui, 1);
        expect(!window.open(), "O closes the context without activating an action");
        in.key_o_pressed = false;
        std::unique_ptr<engine::IEngine> renderer;
        client::FreetypeFont font;
        if (!capture.empty()) {
            renderer.reset(engine::create_engine());
            expect(renderer->init("Player options verification", 1280, 800),
                   "initialize context capture");
            ui.init(*renderer, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
            expect(font.load(*renderer, font.find_system_sans(), 13), "context uses FreeType");
        }
        for (float scale : {.75f, 1.f, 1.5f}) {
            window.show(7);
            in.width = 1920;
            in.height = 1200;
            (void) window.handle(in, ui, scale);
            window.request_sent(20);
            state.request_id = 20;
            state.actions = {protocol::PlayerAction::InviteParty,
                             protocol::PlayerAction::ApplyGuild};
            window.update(state);
            (void) window.handle(in, ui, scale);
            if (renderer) {
                expect(renderer->begin_offscreen(1920, 1200, .08f, .09f, .08f),
                       "begin context capture");
                window.draw(*renderer, ui, font);
                std::vector<std::uint8_t> rgba;
                expect(renderer->end_offscreen_rgba(rgba), "read context capture");
                std::ofstream out(capture + "-" + std::to_string(static_cast<int>(scale * 100)) +
                                      ".ppm",
                                  std::ios::binary);
                out << "P6\n1920 1200\n255\n";
                for (std::size_t i = 0; i < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save context capture");
            }
            in.mouse_left_clicked = true;
            in.mouse_x = 960;
            in.mouse_y = (1200 - 228 * scale) / 2 + 106 * scale;
            r = window.handle(in, ui, scale);
            expect(r && r->action == protocol::PlayerAction::ApplyGuild,
                   "guild action hit region follows UI scale");
            in.mouse_left_clicked = false;
        }
        if (renderer) {
            window.destroy(*renderer);
            font.destroy(*renderer);
            ui.destroy();
            renderer->shutdown();
        }
    }
}  // namespace
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "protocol") {
        protocol_test();
    } else {
        window_test(argc == 3 && std::string(argv[1]) == "--capture" ? argv[2] : "");
    }
    return test::g_failures ? 1 : 0;
}
