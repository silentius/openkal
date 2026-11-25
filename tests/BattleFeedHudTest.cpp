/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BattleMsgBuilder.h"
#include "openkal/client/GameHud.h"
#include "openkal/client/PartyWindow.h"
#include "openkal/client/SettingsWindow.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace openkal::client;
    using openkal::engine::FrameInput;
    using openkal::protocol::BattleMsgKind;
    using openkal::test::expect;

    SettingsWindowAction
    click_settings(SettingsWindow& options, UiLayoutStore& layouts, std::string_view target) {
        UiDocument doc;
        UiView::reload(doc, &layouts.get("settings"));
        const float scale =
            std::min({1.0f,
                      (960.0f - 24.0f) / (doc.window_w() + doc.pad_left() + doc.pad_right()),
                      (540.0f - 24.0f) / (doc.window_h() + doc.pad_top() + doc.pad_bottom())});
        const float x = (960.0f - doc.window_w() * scale) * 0.5f +
                        (doc.pad_left() - doc.pad_right()) * scale * 0.5f;
        const float y = (540.0f - doc.window_h() * scale) * 0.5f +
                        (doc.pad_top() - doc.pad_bottom()) * scale * 0.5f;
        for (const auto& widget : doc.widgets()) {
            if (widget.type() == UiWidgetType::Button && widget.action() == target) {
                return options.handle_click(x + (widget.x() + widget.w() * 0.5f) * scale,
                                            y + (widget.y() + widget.h() * 0.5f) * scale);
            }
            for (const auto& tab : widget.tabs()) {
                if (tab.id() == target) {
                    return options.handle_click(x + (tab.x() + tab.w() * 0.5f) * scale,
                                                y + (widget.y() + widget.h() * 0.5f) * scale);
                }
            }
        }
        expect(false, "settings test action exists in layout");
        return SettingsWindowAction::Ignored;
    }

    FrameInput party_button(UiLayoutStore& layouts,
                            std::string_view document,
                            std::string_view widget_id,
                            int members = 0,
                            float width = 1920,
                            float height = 1080) {
        UiDocument doc;
        UiView::reload(doc, &layouts.get(std::string(document)));
        if (document == "party_actions") {
            doc.set_window_h(doc.window_h() + members * 28);
        }
        const float outer_w = doc.window_w() + doc.pad_left() + doc.pad_right();
        const float outer_h = doc.window_h() + doc.pad_top() + doc.pad_bottom();
        const float scale = std::min({1.0f, (width - 16) / outer_w, (height - 16) / outer_h});
        const float x = (width - outer_w * scale) / 2 + doc.pad_left() * scale;
        const float y = (height - outer_h * scale) / 2 + doc.pad_top() * scale;
        UiDocument row;
        const UiWidget* widget = doc.find_widget(widget_id);
        float row_y = 0;
        if (widget_id == "kick") {
            UiView::reload(row, &layouts.get("party_action_member"));
            widget = row.find_widget(widget_id);
            row_y = 118 * scale;
        }
        expect(widget != nullptr, "party test button exists");
        float wx = 0, wy = 0, ww = 0, wh = 0;
        if (widget) {
            UiView::widget_rect(*widget,
                                nullptr,
                                x,
                                y + row_y,
                                doc.window_w() * scale,
                                doc.window_h() * scale,
                                scale,
                                wx,
                                wy,
                                ww,
                                wh);
        }
        FrameInput frame;
        frame.mouse_left_clicked = true;
        frame.mouse_left = true;
        frame.mouse_x = wx + ww / 2;
        frame.mouse_y = wy + wh / 2;
        return frame;
    }

    const MovablePanel& chat_panel(const GameHud& hud) {
        return hud.panel(HudPanelId::Chat);
    }

    ChatInputResult input(ChatWidget& chat,
                          const FrameInput& frame,
                          const std::function<void(std::string_view)>& send = {}) {
        return chat.handle_input(frame, 24, 24, 420, 132, 1, nullptr, send);
    }

    void type(ChatWidget& chat, std::string_view text) {
        FrameInput frame;
        expect(text.size() <= sizeof(frame.text_chars), "test input fits one frame");
        frame.text_char_count = static_cast<int>(text.size());
        std::copy(text.begin(), text.end(), frame.text_chars);
        (void) input(chat, frame);
    }

    void seed(ChatWidget& chat) {
        chat.push_info("You dealt 128 damage.", BattleMsgKind::Damage);
        chat.push_info("You received 34 damage.", BattleMsgKind::Damage);
        chat.push_info("You gave critical damage of 256 to the enemy.", BattleMsgKind::Damage);
        chat.push_info("You gained 240 experience.", BattleMsgKind::Exp);
        chat.push_info("You acquired 120 Geon.", BattleMsgKind::MoneyGet);
        chat.push_info(BattleMsgBuilder::party_item_acquired("Companion", "Herb", 2),
                       BattleMsgKind::ItemGet,
                       kPartyItemColor);
    }

    void write_capture(const std::string& path,
                       int width,
                       int height,
                       const std::vector<std::uint8_t>& rgba) {
        expect(rgba.size() == static_cast<std::size_t>(width * height * 4), "capture dimensions");
        std::ofstream out(path, std::ios::binary);
        out << "P6\n" << width << ' ' << height << "\n255\n";
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
        }
        expect(out.good(), "write battle feed capture");
    }

    void capture(const std::string& prefix, UiLayoutStore& layouts) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Battle feed verification", 960, 540)) {
            expect(false, "initialize battle feed GL capture");
            return;
        }
        WidgetTools ui;
        const char* data_root = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data_root ? data_root : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        struct Viewport {
            int width;
            int height;
            float scale;
            const char* name;
        };
        for (const auto view : std::array<Viewport, 3>{{{1920, 1080, 1, "full-hd"},
                                                        {1280, 720, 1, "narrow"},
                                                        {1920, 1080, 1.5f, "scaled"}}}) {
            GameHud hud;
            hud.set_layouts(&layouts);
            hud.chat().set_battle_feed(true);
            hud.set_scale(view.scale);
            hud.reset_defaults(static_cast<float>(view.width), static_cast<float>(view.height));
            seed(hud.chat());
            hud.sync_fonts(*engine);
            expect(engine->begin_offscreen(view.width, view.height, .20f, .25f, .22f),
                   "begin HUD capture");
            hud.draw(ui, *engine, StatusWidgetData{}, MinimapWidgetData{}, DragonHudData{});
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read HUD capture");
            write_capture(prefix + "-" + view.name + ".ppm", view.width, view.height, rgba);
            hud.destroy_fonts(*engine);
        }

        ChatWidget chat;
        chat.set_battle_feed(true);
        const auto draw = [&] {
            expect(engine->begin_offscreen(500, 200, .20f, .25f, .22f), "begin feed capture");
            chat.draw(ui, *engine, 24, 24, 420, 132, 1, &layouts.get("battle_feed"));
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read feed capture");
            return rgba;
        };
        const auto empty = draw();
        seed(chat);
        const auto fresh = draw();
        expect(fresh != empty, "new events and backdrop are visible");
        std::array<bool, ChatFilterModel::kBattleCount> flags{};
        chat.filters().set_battle_flags(flags);
        expect(draw() == empty, "disabled battle kinds leave no text or backdrop");
        chat.push_info("System test", std::nullopt);
        expect(draw() != empty, "system messages remain independently visible");
        chat.filters().set_system_enabled(false);
        expect(draw() == empty, "system toggle hides untyped messages");
        flags.fill(true);
        chat.filters().set_battle_flags(flags);
        expect(draw() == fresh, "reenabling filters restores retained battle rows");
        FrameInput frame;
        frame.dt = 10;
        (void) input(chat, frame);
        const auto fading = draw();
        expect(fading != fresh && fading != empty, "events fade gradually after eight seconds");
        frame.dt = 3;
        (void) input(chat, frame);
        expect(draw() == empty, "expired events leave no backdrop or invisible frame");
        frame = {};
        frame.mouse_x = 50;
        frame.mouse_y = 100;
        frame.mouse_scroll = 1;
        (void) input(chat, frame);
        const auto history = draw();
        expect(history != empty, "wheel restores faded history");
        write_capture(prefix + "-history.ppm", 500, 200, history);
        frame = {};
        frame.key_enter_pressed = true;
        (void) input(chat, frame);
        type(chat, "/help");
        const auto command = draw();
        expect(command != history, "command editor appears only when requested");
        write_capture(prefix + "-command.ppm", 500, 200, command);
        chat.destroy(*engine);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans(), 13), "load options font");
        SettingsWindow options;
        options.set_layout(&layouts.get("settings"));
        options.show(openkal::settings::ClientSettings{}, 960, 540);
        for (const auto* tab : {"display", "world", "render", "audio", "battle", "misc"}) {
            (void) click_settings(options, layouts, tab);
            expect(engine->begin_offscreen(960, 540, .20f, .25f, .22f), "begin options capture");
            options.draw(*engine, ui, font);
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read options capture");
            write_capture(prefix + "-options-" + tab + ".ppm", 960, 540, rgba);
        }
        PartyWindow party;
        party.set_layouts(&layouts);
        openkal::protocol::PartyState state;
        FrameInput pointer;
        for (int count : {0, 2, 12}) {
            state.id = count ? 1 : 0;
            state.leader = count ? 1 : 0;
            state.members.clear();
            for (int i = 1; i <= count; ++i) {
                state.members.push_back({static_cast<std::uint32_t>(i),
                                         "Companion " + std::to_string(i),
                                         42,
                                         i == 1 ? 1000 : (i == 2 ? 620 : 270 + i * 35),
                                         1000,
                                         0});
            }
            party.update(state, 1, 960, 540, 1);
            expect(engine->begin_offscreen(960, 540, .20f, .25f, .22f),
                   "begin growing party capture");
            party.draw(*engine, ui, font, pointer);
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read party capture");
            write_capture(prefix + "-party-" + std::to_string(count) + ".ppm", 960, 540, rgba);
        }
        party.toggle_actions(30, "New companion");
        expect(engine->begin_offscreen(960, 540, .20f, .25f, .22f), "begin leader actions capture");
        party.draw(*engine, ui, font, pointer);
        std::vector<std::uint8_t> management_rgba;
        expect(engine->end_offscreen_rgba(management_rgba), "read leader actions capture");
        write_capture(prefix + "-party-actions-leader.ppm", 960, 540, management_rgba);
        party.close_actions();
        state.members.resize(2);
        party.update(state, 1, 960, 540, 2);
        party.toggle_actions();
        expect(engine->begin_offscreen(960, 540, .20f, .25f, .22f), "begin member actions capture");
        party.draw(*engine, ui, font, pointer);
        expect(engine->end_offscreen_rgba(management_rgba), "read member actions capture");
        write_capture(prefix + "-party-actions-member.ppm", 960, 540, management_rgba);
        state = {};
        state.inviter = 30;
        state.inviter_name = "Inviting player";
        state.invitation = 5;
        party.update(state, 1, 960, 540, 1);
        expect(!party.open(), "invitation prompt does not open the passive roster");
        expect(engine->begin_offscreen(960, 540, .20f, .25f, .22f),
               "begin separate invitation capture");
        party.draw(*engine, ui, font, pointer);
        std::vector<std::uint8_t> invitation_rgba;
        expect(engine->end_offscreen_rgba(invitation_rgba), "read separate invitation capture");
        write_capture(prefix + "-party-invitation.ppm", 960, 540, invitation_rgba);
        for (const auto* name : {"party",
                                 "party_member",
                                 "party_invitation",
                                 "party_actions",
                                 "party_action_member"}) {
            UiDocument source, roundtrip;
            UiView::reload(source, &layouts.get(name));
            expect(roundtrip.load(source.to_json()) &&
                       roundtrip.widgets().size() == source.widgets().size(),
                   "party layouts survive editor roundtrip");
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    {
        ChatWidget chat;
        chat.set_battle_feed(true);
        chat.push_info("You acquired Herb.", BattleMsgKind::ItemGet);
        chat.push_info("You acquired 120 Geon.", BattleMsgKind::MoneyGet);
        const auto& lines = chat.message_log().lines();
        expect(lines.size() == 2, "battle feed item and geon lines");
        expect(lines[0].g() > lines[0].r() && lines[0].g() > lines[0].b(),
               "picked items are green");
        expect(lines[1].r() > 0.95f && lines[1].g() > 0.95f && lines[1].b() > 0.95f,
               "geon pickup stays white");
    }
    {
        ChatWidget chat;
        chat.set_battle_feed(true);
        chat.push_info(BattleMsgBuilder::party_item_acquired("Companion", "Herb", 2),
                       BattleMsgKind::ItemGet,
                       kPartyItemColor);
        const auto& line = chat.message_log().lines().back();
        expect(line.text() == "Companion acquired Herb x2.",
               "party item line identifies recipient and count");
        expect(line.g() > line.r() && line.g() > line.b() && line.g() < .75f,
               "other members' item awards use a darker green");
    }
    UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    expect(layouts.get("battle_feed").loaded(), "load universal battle feed layout");
    UiDocument document;
    UiView::reload(document, &layouts.get("battle_feed"));
    UiDocument roundtrip;
    expect(roundtrip.load(document.to_json()), "battle feed survives the UI editor roundtrip");
    const auto* editor_input = roundtrip.find_widget("input");
    const auto* editor_messages = roundtrip.find_widget("messages");
    expect(editor_input && editor_input->visible_when() == "chat.focused" && editor_messages &&
               editor_messages->bind() == "chat.messages" && editor_messages->line_height() == 20,
           "editor preserves command visibility and message geometry");
    {
        PartyWindow party;
        WidgetTools ui;
        openkal::protocol::PartyState state;
        party.set_layouts(&layouts);
        party.update(state, 1, 1920, 1080);
        expect(!party.open(), "roster stays hidden without party membership");
        state.id = 1;
        state.leader = 1;
        state.members = {{1, "Leader", 42, 100, 100, 0}, {2, "Companion", 42, 50, 100, 0}};
        party.update(state, 1, 1920, 1080);
        expect(party.open(), "active party roster is always visible");
        FrameInput frame;
        frame.mouse_left_clicked = true;
        frame.mouse_left = true;
        frame.mouse_x = 1020;
        frame.mouse_y = 120;
        expect(!party.contains(frame.mouse_x, frame.mouse_y, ui) && !party.handle(frame, ui),
               "passive player selection has no centered invite overlay");
        expect(party.open(), "outside clicks do not hide the active party");
        expect(party.contains(1750, 105, ui), "compact roster captures only its actual bounds");
        frame.mouse_x = 1780;
        frame.mouse_y = 105;
        (void) party.handle(frame, ui);
        frame.mouse_left_clicked = false;
        frame.mouse_x = 1580;
        frame.mouse_y = 305;
        (void) party.handle(frame, ui);
        frame.mouse_left = false;
        (void) party.handle(frame, ui);
        expect(!party.contains(1750, 105, ui) && party.contains(1550, 305, ui),
               "dragging moves the compact party hitbox");
        party.update(state, 1, 1920, 1080, 2);
        party.toggle_actions();
        expect(party.actions_open() && party.open(),
               "member can open actions without hiding roster");
        auto request = party.handle(party_button(layouts, "party_actions", "leave"), ui);
        expect(request && request->command == openkal::protocol::PartyCommand::Leave &&
                   !party.actions_open() && party.open(),
               "member can leave via party actions");
        party.toggle_actions();
        request = party.handle(party_button(layouts, "party_actions", "close"), ui);
        expect(!request && !party.actions_open() && party.open(),
               "closing actions preserves membership");
        party.update(state, 1, 1920, 1080, 1);
        party.toggle_actions(3, "Newcomer");
        request = party.handle(party_button(layouts, "party_actions", "invite", 1), ui);
        expect(request && request->command == openkal::protocol::PartyCommand::Invite &&
                   request->target == 3,
               "leader explicitly invites selected player from actions");
        party.toggle_actions(2, "Companion");
        request = party.handle(party_button(layouts, "party_actions", "invite", 1), ui);
        expect(!request && party.actions_open(), "existing member cannot be invited again");
        request = party.handle(party_button(layouts, "party_actions", "kick", 1), ui);
        expect(request && request->command == openkal::protocol::PartyCommand::Kick &&
                   request->target == 2,
               "leader removes the chosen member and cannot target self");
        state.leader = 2;
        party.update(state, 1, 1920, 1080, 1);
        request = party.handle(party_button(layouts, "party_actions", "kick", 1), ui);
        expect(!request, "losing leadership immediately removes kick controls");
        party.close_actions();
        party.toggle_actions(3, "Newcomer");
        request = party.handle(party_button(layouts, "party_actions", "invite"), ui);
        expect(!request, "ordinary member has no invitation control");
        state = {};
        state.inviter = 20;
        state.invitation = 12;
        state.inviter_name = "Companion";
        party.update(state, 1, 1920, 1080);
        expect(!party.open() && !party.actions_open(), "leaving closes both roster and management");
        frame.mouse_left_clicked = true;
        frame.mouse_left = true;
        frame = party_button(layouts, "party_invitation", "accept");
        expect(party.contains(frame.mouse_x, frame.mouse_y, ui),
               "separate invitation captures its own input");
        const auto accept = party.handle(frame, ui);
        expect(accept && accept->command == openkal::protocol::PartyCommand::Reply &&
                   accept->invitation == 12 && accept->value == 1 && !party.open(),
               "accept invitation without showing a party roster");
        state = {};
        party.update(state, 1, 1920, 1080);
        expect(!party.contains(frame.mouse_x, frame.mouse_y, ui),
               "expired invitation releases pointer input");
        for (const auto* name : {"party",
                                 "party_member",
                                 "party_invitation",
                                 "party_actions",
                                 "party_action_member"}) {
            UiDocument source, roundtrip;
            UiView::reload(source, &layouts.get(name));
            expect(roundtrip.load(source.to_json()) &&
                       roundtrip.widgets().size() == source.widgets().size(),
                   "party documents roundtrip through universal editor schema");
            if (std::string_view(name) == "party" || std::string_view(name) == "party_member") {
                expect(source.window_w() == 150, "roster stays narrow");
                for (const auto& widget : source.widgets()) {
                    expect(widget.type() != UiWidgetType::Button, "passive roster has no controls");
                    expect(widget.bind().find("mp") == std::string::npos,
                           "roster contains no mana binding");
                }
            }
        }
    }
    SettingsWindow options;
    options.set_layout(&layouts.get("settings"));
    ChatFilterModel initial_filters;
    options.show(openkal::settings::ClientSettings{}, 960, 540, initial_filters);
    const auto click = [&](std::string_view target) {
        return click_settings(options, layouts, target);
    };
    (void) click("render");
    (void) click("settings.players.next");
    expect(options.settings().render.max_rendered_players == 50,
           "Graphics changes the remote body budget independently");
    (void) click("settings.player_names.previous");
    (void) click("settings.player_names.previous");
    expect(options.settings().render.max_player_names == -1 &&
               options.settings().render.max_rendered_players == 50,
           "Graphics can hide every remote nameplate without hiding player models");
    (void) click("misc");
    (void) click("settings.misc.clock");
    expect(!options.settings().clock_enabled, "Misc checkbox disables clock in the draft");
    (void) click("settings.misc.show_own_name");
    expect(options.settings().show_own_name && options.settings().render.max_player_names == -1,
           "Misc enables the own nameplate with remote names disabled");
    (void) click("settings.misc.show_own_name");
    expect(!options.settings().show_own_name, "Misc can turn the own nameplate back off");
    (void) click("settings.misc.show_own_name");
    (void) click("settings.misc.ignore_party");
    expect(options.settings().ignore_party_requests, "Misc toggle edits party invitation draft");
    (void) click("world");
    (void) click("settings.preset.next");
    expect(!options.settings().clock_enabled, "quality preset preserves clock preference");
    expect(options.settings().show_own_name, "quality preset preserves own nameplate preference");
    expect(options.settings().ignore_party_requests,
           "graphics preset preserves party ignore option");
    (void) click("display");
    (void) click("settings.battle.0");
    expect(options.chat_filters().battle_enabled(BattleMsgKind::Damage),
           "hidden battle controls do not react on Display");
    (void) click("battle");
    (void) click("settings.battle.0");
    expect(!options.chat_filters().battle_enabled(BattleMsgKind::Damage) &&
               initial_filters.battle_enabled(BattleMsgKind::Damage),
           "battle toggle edits only the draft");
    (void) click("settings.battle.system");
    expect(!options.chat_filters().system_enabled(), "system messages toggle independently");
    (void) click("audio");
    (void) click("battle");
    expect(!options.chat_filters().battle_enabled(BattleMsgKind::Damage),
           "tab switching retains draft filters");
    expect(click("settings.close") == SettingsWindowAction::Close, "Cancel closes options");
    options.show(openkal::settings::ClientSettings{}, 960, 540, initial_filters);
    expect(!options.settings().show_own_name, "reopening discards cancelled own nameplate edits");
    expect(options.chat_filters().battle_enabled(BattleMsgKind::Damage) &&
               options.chat_filters().system_enabled(),
           "reopening discards cancelled filters");
    (void) click("battle");
    (void) click("settings.battle.0");
    (void) click("settings.battle.system");
    expect(click("settings.save") == SettingsWindowAction::Save &&
               !options.chat_filters().battle_enabled(BattleMsgKind::Damage),
           "Save exposes the edited battle draft");
    {
        WidgetTools ui;
        PartyWindow party;
        party.set_layouts(&layouts);
        party.set_logical_position(200, 140);
        openkal::protocol::PartyState state;
        state.id = state.leader = 1;
        state.members = {{1, "Leader", 42, 100, 100, 0}, {2, "Companion", 42, 50, 100, 0}};
        party.update(state, 2, 1920, 1080, 1);
        FrameInput drag;
        drag.mouse_left = drag.mouse_left_clicked = true;
        drag.mouse_x = 410;
        drag.mouse_y = 290;
        (void) party.handle(drag, ui);
        expect(party.dragging(), "party drag exposes its active state for save-on-release");
        drag.mouse_left_clicked = false;
        drag.mouse_x = 610;
        drag.mouse_y = 490;
        (void) party.handle(drag, ui);
        drag.mouse_left = false;
        (void) party.handle(drag, ui);
        expect(!party.dragging() && party.logical_x() == 300 && party.logical_y() == 240,
               "party drag records logical coordinates using the HUD scale");
        UiSettings saved;
        expect(!saved.party_position(42).has_pos(), "new character has no stored party anchor");
        saved.set_party_position(42, party.logical_x(), party.logical_y());
        saved.set_party_position(43, 20, 30);
        GameHud hud;
        hud.set_layouts(&layouts);
        hud.reset_defaults(1920, 1080);
        saved.capture_character(hud, 42, "Party position test", 2);
        const auto path =
            std::filesystem::temp_directory_path() /
            ("openkal-party-position-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        expect(saved.save(path.string()), "save party position with existing HUD settings");
        UiSettings loaded;
        expect(loaded.load(path.string()), "load party position from disk");
        const auto& anchor = loaded.party_position(42);
        expect(anchor.has_pos() && anchor.x() == 300 && anchor.y() == 240 &&
                   loaded.party_position(43).x() == 20 && loaded.party_position(43).y() == 30,
               "party anchors survive restart separately for each character");
        PartyWindow restored;
        restored.set_layouts(&layouts);
        restored.set_logical_position(anchor.x(), anchor.y());
        restored.update({}, 1.5f, 1920, 1080, 1);
        expect(!restored.open(), "restoring party position never opens an empty roster");
        restored.update(state, 1.5f, 1920, 1080, 1);
        expect(restored.contains(451, 361, ui) && !restored.contains(449, 359, ui),
               "saved logical anchor follows a changed UI scale");
        for (std::uint32_t id = 3; id <= 12; ++id) {
            state.members.push_back({id, "Member", 42, 100, 100, 0});
        }
        restored.update(state, 1.5f, 1920, 400, 1);
        expect(restored.logical_x() == 300 && restored.logical_y() == 240,
               "fitting a growing list to a short viewport preserves the preferred anchor");
        restored.update({}, 1.5f, 1920, 1080, 1);
        restored.update(state, 1.5f, 1920, 1080, 1);
        expect(restored.contains(451, 361, ui),
               "rejoining after viewport changes restores the same preferred anchor");
        {
            std::ofstream legacy(path);
            legacy
                << R"({"characters":{"42":{"name":"Legacy"},"43":{"panels":{"party":{"x":"bad","y":30}}}}})";
        }
        expect(loaded.load(path.string()) && !loaded.party_position(42).has_pos() &&
                   !loaded.party_position(43).has_pos(),
               "older or invalid party settings retain the default-position fallback");
        std::filesystem::remove(path);
    }
    GameHud hud;
    hud.set_layouts(&layouts);
    hud.chat().set_battle_feed(true);
    hud.reset_defaults(1920, 1080);
    const auto& panel = chat_panel(hud);
    expect(panel.x() == 24 && panel.y() + panel.h() == 1056, "Full HD feed docks bottom-left");
    expect(panel.x() + panel.w() + 24 <=
               static_cast<const GameHud&>(hud).panel(HudPanelId::Dragon).x(),
           "feed clears the left Dragon ornament");
    expect(!panel.movable(), "automatic feed cannot accidentally be dragged");

    GameHud legacy;
    legacy.set_layouts(&layouts);
    legacy.reset_defaults(1920, 1080);
    legacy.set_panel_pos(HudPanelId::Chat, 500, 400);
    UiSettings saved;
    saved.capture_character(legacy, 42, "Feed test", 1);
    saved.apply_character(hud, 42, 1);
    expect(panel.x() == 24 && panel.y() + panel.h() == 1056,
           "saved old chat position cannot displace feed");
    saved.capture_character(hud, 42, "Feed test", 1);
    saved.apply_character(legacy, 42, 1);
    expect(chat_panel(legacy).x() == 500 && chat_panel(legacy).y() == 400,
           "single-player settings preserve the multiplayer chat position");
    hud.chat().filters() = options.chat_filters();
    saved.capture_character(hud, 42, "Feed test", 1);
    const auto settings_path =
        std::filesystem::temp_directory_path() /
        ("openkal-feed-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    expect(saved.save(settings_path.string()), "write character filters");
    UiSettings reloaded;
    expect(reloaded.load(settings_path.string()), "reload character filters");
    reloaded.apply_character(legacy, 42, 1);
    expect(!legacy.chat().filters().battle_enabled(BattleMsgKind::Damage) &&
               !legacy.chat().filters().system_enabled() &&
               legacy.chat().filters().battle_enabled(BattleMsgKind::Exp),
           "battle and system selections survive disk roundtrip");
    std::filesystem::remove(settings_path);

    options.show(openkal::settings::ClientSettings{}, 960, 540, initial_filters, false);
    (void) click("battle");
    const auto initial_mode = options.settings().display.mode;
    (void) click("settings.display.next");
    expect(options.settings().display.mode != initial_mode,
           "disabled Battle tab keeps the current page active before character selection");
    (void) click("settings.battle.0");
    (void) click("settings.battle.system");
    expect(options.chat_filters().battle_enabled(BattleMsgKind::Damage) &&
               options.chat_filters().system_enabled(),
           "startup options cannot edit character-specific filters");
    (void) click("audio");
    const bool initial_bgm = options.settings().audio.bgm_enabled;
    (void) click("settings.bgm.toggle");
    expect(options.settings().audio.bgm_enabled != initial_bgm,
           "startup options edit global settings before character selection");
    expect(click("settings.close") == SettingsWindowAction::Close,
           "startup options cancel closes the draft");
    options.show(openkal::settings::ClientSettings{}, 960, 540, initial_filters, false);
    expect(options.settings().audio.bgm_enabled == initial_bgm,
           "startup options discard cancelled changes");
    (void) click("audio");
    (void) click("settings.bgm.toggle");
    expect(click("settings.save") == SettingsWindowAction::Save &&
               options.settings().audio.bgm_enabled != initial_bgm,
           "startup options expose the saved global draft");

    expect(hud.ensure_layout(1920, 1080, 1.5f), "scale change updates docking");
    expect(panel.y() + panel.h() + 36 <=
               static_cast<const GameHud&>(hud).panel(HudPanelId::Dragon).y(),
           "large HUD scale moves feed above skills");
    expect(hud.ensure_layout(1280, 720, 1), "narrow resize updates docking");
    expect(panel.y() + panel.h() + 24 <=
               static_cast<const GameHud&>(hud).panel(HudPanelId::Dragon).y(),
           "narrow viewport keeps feed above skills");
    expect(hud.ensure_layout(1920, 1080, 1), "return to Full HD updates docking");
    expect(panel.y() + panel.h() == 1056, "feed returns beside skills after resizing back");
    hud.reset_defaults(320, 480);
    expect(panel.x() >= 0 && panel.x() + panel.w() <= 320 && panel.y() >= 0,
           "small viewport constrains feed width");

    ChatWidget chat;
    chat.set_battle_feed(true);
    chat.push_chat("Server", "Command completed.");
    chat.push_chat("Other player", "Hello.");
    expect(chat.message_log().lines().size() == 1 &&
               chat.message_log().lines().front().text() == "Command completed.",
           "server replies survive while player chat stays hidden");
    FrameInput frame;
    frame.mouse_x = 50;
    frame.mouse_y = 100;
    frame.mouse_left_clicked = true;
    expect(input(chat, frame) == ChatInputResult::Ignored && !chat.focused(),
           "passive feed lets world clicks through");
    expect(!chat.contains_pointer(50, 100, 24, 24, 420, 132, 1),
           "passive feed does not block HUD hit testing");
    for (int i = 0; i < 10; ++i) {
        chat.push_info("Event " + std::to_string(i), BattleMsgKind::Exp);
    }
    frame.mouse_left_clicked = false;
    frame.mouse_scroll = 2;
    expect(input(chat, frame) == ChatInputResult::Handled && chat.history_open() && !chat.focused(),
           "wheel opens scrollback without grabbing movement keys");
    expect(chat.contains_pointer(50, 100, 24, 24, 420, 132, 1),
           "open history consumes wheel instead of camera zoom");
    const int scroll = chat.history_scroll();
    chat.push_info("Another event", BattleMsgKind::Exp);
    expect(chat.history_scroll() == scroll + 1,
           "new events preserve the current scrollback position");
    chat.filters().set_battle_enabled(BattleMsgKind::Hp, false);
    chat.push_info("Hidden HP event", BattleMsgKind::Hp);
    expect(chat.history_scroll() == scroll + 1, "filtered events do not move scrollback");
    frame = {};
    frame.key_escape_pressed = true;
    expect(input(chat, frame) == ChatInputResult::Handled && !chat.history_open(),
           "Escape closes history");

    std::string args;
    chat.commands().register_command("test", [&](const ClientChatCommandContext& ctx) {
        chat.blur();
        args = ctx.args();
        return ClientChatCommandResult(true, "Test succeeded.");
    });
    frame = {};
    frame.key_enter_pressed = true;
    (void) input(chat, frame);
    expect(chat.focused(), "Enter opens command-only input");
    type(chat, "/test value");
    expect(input(chat, frame) == ChatInputResult::Submitted && !chat.focused(),
           "local command submits and closes editor");
    expect(args == "value", "closing focus inside a command preserves its argument views");
    expect(chat.message_log().lines().back().text() == "Test succeeded.",
           "local command replies use the event feed");
    (void) input(chat, frame);
    type(chat, "/time");
    std::string sent;
    (void) input(chat, frame, [&](std::string_view text) { sent = text; });
    expect(sent == "/time" && !chat.focused(), "server slash commands remain available");
    (void) input(chat, frame);
    FrameInput backspace;
    backspace.key_backspace_pressed = true;
    (void) input(chat, backspace);
    type(chat, "hello");
    sent.clear();
    expect(input(chat, frame, [&](std::string_view text) { sent = text; }) !=
                   ChatInputResult::Submitted &&
               sent.empty(),
           "single-player editor never sends ordinary player chat");
    chat.blur();
    frame = {};
    frame.mouse_x = 500;
    frame.mouse_y = 180;
    frame.mouse_left_clicked = true;
    expect(input(chat, frame) == ChatInputResult::Ignored,
           "outside click remains available to the world");

    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(argv[2], layouts);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
