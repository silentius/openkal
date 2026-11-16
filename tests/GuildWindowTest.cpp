/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GuildWindow.h"

#include "openkal/client/ClientInput.h"
#include "openkal/client/UiLayout.h"
#include "openkal/platform/ConfigArchive.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
using namespace openkal;
using test::expect;

namespace {
    float fitted_scale(const engine::FrameInput& input, float scale = 1) {
        return std::max(
            .25f, std::min({scale, (input.width - 16) / 980.0f, (input.height - 16) / 780.0f}));
    }
    float client_x(const engine::FrameInput& input, float scale = 1) {
        return (input.width - 900 * fitted_scale(input, scale)) * .5f;
    }
    float client_y(const engine::FrameInput& input, float scale = 1) {
        return (input.height - 640 * fitted_scale(input, scale)) * .5f +
               14 * fitted_scale(input, scale);
    }

    float tab_center(const client::UiLayout& layout, int id, bool member) {
        client::UiDocument doc;
        client::UiView::reload(doc, &layout);
        const auto* tabs = doc.find_widget(member ? "guild_member_tabs" : "guild_tabs");
        expect(tabs != nullptr, "load membership-specific navigation");
        for (const auto& tab : tabs->tabs()) {
            if (tab.id() == std::to_string(id)) {
                return tab.x() + tab.w() / 2;
            }
        }
        expect(false, "requested tab exists for the current membership");
        return 0;
    }

    void verify_membership_and_tables(const client::UiLayout& layout, const platform::Json& view) {
        client::GuildWindow window;
        client::WidgetTools ui;
        window.set_layout(&layout);
        window.update(view);
        window.show_creation();
        expect(window.variables().at("guild.page.2") == "1" &&
                   window.variables().at("guild.non_member") == "0" &&
                   !window.enabled("guild.search") && !window.enabled("guild.tab.1"),
               "members open their roster instead of the founding page");
        auto vars = window.variables();
        expect(vars.at("guild.table.members") == "1" &&
                   vars.at("guild.column.5") == "Last seen (UTC)" &&
                   vars.at("guild.row.0.cell.2") == "30" && vars.at("guild.row.0.cell.3") == "24" &&
                   vars.at("guild.row.1.cell.4") == "Offline" &&
                   vars.at("guild.row.1.cell.5") == "2026-10-25 03:46",
               "member cells expose separate levels, contributions, presence and UTC timestamps");
        engine::FrameInput input;
        input.width = 1280;
        input.height = 800;
        (void) window.handle(input, ui);
        input.mouse_left_clicked = true;
        input.mouse_x = client_x(input) + 790;
        input.mouse_y = client_y(input) + 210;
        (void) window.handle(input, ui);
        expect(window.variables().at("guild.row.0.selected") == "1",
               "clicking any table cell selects its entire row");
        auto long_view = view;
        long_view.object()["rows"].array()[0].object()["name"].string() = std::string(120, 'W');
        window.update(long_view);
        expect(window.variables().at("guild.row.0.cell.0").size() < 120 &&
                   window.variables().at("guild.row.0.cell.2") == "30",
               "long names fit their own cell without consuming adjacent columns");
        auto empty = view;
        empty.object()["rows"] = platform::Json::parse("[]");
        window.update(empty);
        expect(window.variables().at("guild.table.empty") == "1" &&
                   window.variables().at("guild.row.0.table_visible") == "0",
               "empty tables keep their heading and show an explicit empty state");
        auto guest = view;
        guest.object()["own"] = platform::Json::parse(R"({"guild":0})");
        window.update(guest);
        window.show_creation();
        expect(window.enabled("guild.search") && window.variables().at("guild.page.1") == "1",
               "leaving a guild restores Search / Create");
        auto joined = view;
        joined.object()["command"].set_number(0);
        window.update(joined);
        input.mouse_left_clicked = false;
        const auto request = window.handle(input, ui);
        expect(request && request->command == protocol::GuildCommand::Members &&
                   window.variables().at("guild.page.2") == "1",
               "joining from the creation page immediately switches to the roster");
    }

    void verify_tab_navigation(const client::UiLayout& layout, const platform::Json& view) {
        client::WidgetTools ui;
        constexpr protocol::GuildCommand queries[] = {protocol::GuildCommand::Ranking,
                                                      protocol::GuildCommand::Search,
                                                      protocol::GuildCommand::Members,
                                                      protocol::GuildCommand::Applications,
                                                      protocol::GuildCommand::MyApplications,
                                                      protocol::GuildCommand::Detail,
                                                      protocol::GuildCommand::Detail,
                                                      protocol::GuildCommand::Members,
                                                      protocol::GuildCommand::Members};
        for (const bool member : {false, true}) {
            auto navigation_view = view;
            if (!member) {
                navigation_view.object()["own"] = platform::Json::parse(R"({"guild":0})");
            }
            for (const float scale : {.75f, 1.0f, 1.5f}) {
                client::GuildWindow window;
                window.set_layout(&layout);
                window.update(navigation_view);
                window.set_scale(scale);
                window.set_open(true);
                engine::FrameInput input;
                input.width = 1920;
                input.height = 1200;
                (void) window.handle(input, ui);
                const float y = client_y(input, scale);
                for (int tab = 1; tab <= 9; ++tab) {
                    const int selected = tab % 9;
                    if (selected == 8 || (member && selected == 1)) {
                        continue;
                    }
                    input.mouse_left_clicked = true;
                    input.mouse_x =
                        client_x(input, scale) + tab_center(layout, selected, member) * scale;
                    input.mouse_y = y + 60 * scale;
                    const auto request = window.handle(input, ui);
                    expect(request && request->command == queries[selected],
                           "each scaled tab requests its matching page");
                    const auto vars = window.variables();
                    expect(vars.at("guild.tab") == std::to_string(selected),
                           "tab selection follows navigation");
                    for (int i = 0; i < 9; ++i) {
                        expect(vars.at("guild.tab." + std::to_string(i) + ".active") ==
                                   (selected == i ? "1" : "0"),
                               "exactly one tab has its active background and underline");
                    }
                }
                for (int parent = 0; parent < (member ? 1 : 2); ++parent) {
                    input.mouse_x =
                        client_x(input, scale) + tab_center(layout, parent, member) * scale;
                    input.mouse_y = y + 60 * scale;
                    (void) window.handle(input, ui);
                    window.update(navigation_view);
                    input.mouse_x = client_x(input, scale) + 40 * scale;
                    input.mouse_y = y + 210 * scale;
                    (void) window.handle(input, ui);
                    input.mouse_x = client_x(input, scale) + 75 * scale;
                    input.mouse_y = y + 535 * scale;
                    const auto details = window.handle(input, ui);
                    expect(details && details->command == protocol::GuildCommand::Detail &&
                               window.variables().at("guild.page.9") == "1" &&
                               window.variables().at("guild.tab") == std::to_string(parent),
                           "details retain the originating browsing category");
                    input.mouse_x =
                        client_x(input, scale) + tab_center(layout, parent, member) * scale;
                    input.mouse_y = y + 60 * scale;
                    const auto back = window.handle(input, ui);
                    expect(back && back->command == queries[parent] &&
                               window.variables().at("guild.page." + std::to_string(parent)) == "1",
                           "clicking the owning tab returns from details to its list");
                }
            }
        }
    }

    void verify_announcement(const client::UiLayout& layout, platform::Json view) {
        client::WidgetTools ui;
        view.object()["guild"].object()["announcement"].string() =
            "First paragraph.\n\nMeeting at the village.";
        client::GuildWindow window;
        window.set_layout(&layout);
        window.update(view);
        window.set_open(true);
        engine::FrameInput input;
        input.width = 1280;
        input.height = 800;
        (void) window.handle(input, ui);
        auto click = [&](float x, float y) {
            input.mouse_left_clicked = true;
            input.mouse_x = client_x(input) + x;
            input.mouse_y = client_y(input) + y;
            return window.handle(input, ui);
        };
        (void) click(tab_center(layout, 6, true), 60);
        expect(window.variables().at("guild.pagination") == "0" &&
                   !window.enabled("guild.previous") && !window.enabled("guild.next"),
               "announcements do not expose horizontal page navigation");
        (void) click(80, 210);
        expect(!window.focused() && window.variables().at("guild.announcement_text").find("\n\n") !=
                                        std::string::npos,
               "announcements open as readable paragraphs without focusing an editor");
        (void) click(80, 535);
        expect(window.focused() && window.variables().at("guild.announcement_editing") == "1",
               "authorized members explicitly enter the multiline editor");
        input.mouse_left_clicked = false;
        input.key_backspace_pressed = true;
        for (int i = 0; i < 80; ++i) {
            (void) window.handle(input, ui);
        }
        input.key_backspace_pressed = false;
        auto type = [&](const std::string& value) {
            input.text_char_count = static_cast<int>(value.size());
            std::copy(value.begin(), value.end(), input.text_chars);
            (void) window.handle(input, ui);
            input.text_char_count = 0;
        };
        type("First ü");
        input.key_enter_pressed = true;
        (void) window.handle(input, ui);
        input.key_enter_pressed = false;
        type("Second");
        input.key_left = true;
        (void) window.handle(input, ui);
        input.key_left = false;
        type("X");
        const std::string draft = "First ü\nSeconXd";
        expect(window.variables().at("guild.announcement_text") == "First ü\nSeconX|d",
               "Enter inserts a paragraph break and cursor movement edits text in place");
        auto refreshed = view;
        refreshed.object()["guild"].object()["announcement"].string() = "New server announcement.";
        window.update(refreshed);
        expect(window.variables().at("guild.announcement_text").find("SeconX") != std::string::npos,
               "background refresh preserves the announcement draft");
        auto request = click(70, 535);
        expect(request && request->command == protocol::GuildCommand::Announcement &&
                   request->text == draft,
               "Save announcement submits every paragraph without flattening newlines");
        window.request_failed();
        expect(window.variables().at("guild.announcement_editing") == "1" &&
                   window.enabled("guild.set_announcement"),
               "a failed send keeps the draft and permits retry");
        request = click(70, 535);
        refreshed.object()["command"].set_number(0);
        refreshed.object()["success"].set_boolean(true);
        auto& id = refreshed.object()["_request_id"];
        id.set_type(platform::Json::Type::String);
        id.string() = std::to_string(request->request_id);
        window.update(refreshed);
        expect(window.variables().at("guild.announcement_editing") == "0" &&
                   window.variables().at("guild.announcement_text") == draft,
               "server acknowledgement publishes the draft and returns to reading mode");
        (void) click(70, 535);
        input.mouse_left_clicked = false;
        type(" unsaved");
        (void) click(240, 535);
        expect(window.variables().at("guild.announcement_text") == draft,
               "Cancel discards a draft without changing the published announcement");
        auto readonly = view;
        readonly.object()["own"].object()["rights"].set_number(0);
        window.update(readonly);
        expect(window.variables().at("guild.announcement_can_edit") == "0" &&
                   !window.enabled("guild.set_announcement"),
               "ordinary members can read announcements without editing controls");
        auto long_announcement = readonly;
        std::string paragraphs;
        for (int i = 0; i < 40; ++i) {
            paragraphs += "Announcement paragraph\n";
        }
        long_announcement.object()["guild"].object()["announcement"].string() = paragraphs;
        window.update(long_announcement);
        input.mouse_left_clicked = false;
        input.mouse_x = client_x(input) + 450;
        input.mouse_y = client_y(input) + 300;
        input.mouse_scroll = -1;
        (void) window.handle(input, ui);
        expect(window.variables().at("guild.announcement_scroll") == "16",
               "long announcements remain scrollable without paging buttons");
        input.mouse_scroll = 1;
        (void) window.handle(input, ui);
        expect(window.variables().at("guild.announcement_scroll") == "0",
               "announcement scrolling can return to the first paragraph");
        input.mouse_scroll = 0;
        (void) click(tab_center(layout, 2, true), 60);
        expect(!window.enabled("guild.leave") && !window.enabled("guild.daily"),
               "the member roster keeps membership actions in their own area");
        (void) click(tab_center(layout, 7, true), 60);
        expect(window.enabled("guild.leave") && window.enabled("guild.daily") &&
                   window.variables().at("guild.daily_label") == "Daily reward",
               "the Membership area contains the daily reward and leave actions");
        const auto reward = click(70, 503);
        expect(reward && reward->command == protocol::GuildCommand::DailyReward,
               "the relocated daily reward button sends the reward request");
        expect(!window.enabled("guild.daily"), "a pending reward cannot be requested twice");
        window.update(readonly);
        expect(!window.enabled("guild.daily"), "unrelated refreshes do not unlock a pending claim");
        auto claimed = readonly;
        claimed.object()["command"].set_number(0);
        claimed.object()["_request_id"] =
            platform::Json::parse("\"" + std::to_string(reward->request_id) + "\"");
        claimed.object()["own"].object()["daily_reward_available"].set_boolean(false);
        window.update(claimed);
        const auto blocked = click(70, 503);
        expect(!window.enabled("guild.daily") &&
                   (!blocked || blocked->command != protocol::GuildCommand::DailyReward),
               "a claimed or unavailable reward stays disabled and sends no claim");
        window.update(readonly);
        expect(window.enabled("guild.daily"), "new server availability re-enables the reward");
        (void) click(70, 503);
        window.request_failed();
        expect(window.enabled("guild.daily"), "a failed send releases the pending reward lock");
    }

    void verify_loaded_announcement(const client::UiLayout& layout, platform::Json view) {
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        const bool initialized =
            renderer && renderer->init("Guild announcement layout", 1920, 1200);
        expect(initialized, "initialize the announcement renderer");
        if (!initialized) {
            return;
        }
        client::WidgetTools ui;
        ui.init(*renderer, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        client::FreetypeFont reference;
        const bool loaded = reference.load(*renderer, reference.find_system_sans(), 13);
        expect(loaded, "load the same thirteen-pixel face used by guild announcements");
        if (!loaded) {
            ui.destroy();
            renderer->shutdown();
            return;
        }
        std::string announcement = std::string(160, 'i') + u8" ü… words with spaces.";
        announcement.append(31, '\n');
        expect(protocol::guild_text_valid(announcement + "X", 1024, true),
               "the multiline fixture remains within the public announcement save contract");
        view.object()["guild"].object()["announcement"].string() = announcement;
        const auto legacy_lines = client::layout_text_lines(
            announcement, 876, [&](std::string_view line) { return reference.text_width(line); });
        const auto fallback_lines =
            client::layout_text_lines(announcement, 876, [](std::string_view line) {
                return static_cast<float>(line.size() * 13) * .65f;
            });
        const auto pages = (legacy_lines.size() + 15) / 16;
        expect(pages > 1 && pages != (fallback_lines.size() + 15) / 16,
               "the owned announcement distinguishes loaded glyph metrics from byte estimates");
        for (float scale : {.75f, 1.0f, 1.5f}) {
            client::GuildWindow window;
            window.set_layout(&layout);
            window.update(view);
            window.set_scale(scale);
            window.set_open(true);
            engine::FrameInput input;
            input.width = 1920;
            input.height = 1200;
            (void) window.handle(input, ui);
            auto click = [&](float x, float y) {
                input.mouse_left_clicked = true;
                input.mouse_x = client_x(input, scale) + x * scale;
                input.mouse_y = client_y(input, scale) + y * scale;
                return window.handle(input, ui);
            };
            (void) click(tab_center(layout, 6, true), 60);
            input.mouse_left_clicked = false;
            renderer->begin_frame(.08f, .09f, .08f);
            renderer->begin_ui_pass();
            window.draw(*renderer, ui, reference);
            renderer->end_frame();
            expect(window.variables().at("guild.pages") == "1 / " + std::to_string(pages),
                   "drawing loads the guild face and retains legacy measured announcement pages");
            input.mouse_x = client_x(input, scale) + 450 * scale;
            input.mouse_y = client_y(input, scale) + 300 * scale;
            input.mouse_scroll = -1;
            for (std::size_t i = 0; i < pages + 2; ++i) {
                (void) window.handle(input, ui);
            }
            expect(window.variables().at("guild.announcement_scroll") ==
                           std::to_string((pages - 1) * 16) &&
                       window.variables().at("guild.pages") ==
                           std::to_string(pages) + " / " + std::to_string(pages),
                   "loaded announcement scrolling clamps to the measured final page at every zoom");
            input.mouse_scroll = 0;
            (void) click(80, 535);
            expect(window.focused() && window.variables().at("guild.announcement_scroll") ==
                                           std::to_string((pages - 1) * 16),
                   "the loaded multiline editor opens at its measured final cursor page");
            input.mouse_left_clicked = false;
            input.text_chars[0] = 'X';
            input.text_char_count = 1;
            (void) window.handle(input, ui);
            input.text_char_count = 0;
            expect(window.variables().at("guild.announcement_text") == announcement + "X|",
                   "editing a loaded announcement preserves UTF-8, spaces and blank paragraphs");
            expect(window.enabled("guild.set_announcement"),
                   "the valid loaded draft enables the announcement save action");
            const auto request = click(70, 535);
            expect(request && request->command == protocol::GuildCommand::Announcement &&
                       request->text == announcement + "X",
                   "the loaded editor submits the complete unmodified announcement source");
            window.destroy(*renderer);
        }
        reference.destroy(*renderer);
        ui.destroy();
        renderer->shutdown();
    }

    void verify_rank_dialog(const client::UiLayout& layout,
                            const client::UiLayout& rank_layout,
                            const platform::Json& view) {
        client::WidgetTools ui;
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            client::GuildWindow window;
            window.set_layout(&layout);
            window.set_rank_layout(&rank_layout);
            window.update(view);
            window.set_scale(scale);
            window.set_open(true);
            engine::FrameInput input;
            input.width = 1920;
            input.height = 1200;
            (void) window.handle(input, ui);
            const float y = client_y(input, scale);
            auto click = [&](float x, float yy, bool dialog = false) {
                input.mouse_left_clicked = true;
                input.mouse_x = client_x(input, scale) + (x + (dialog ? 60 : 0)) * scale;
                input.mouse_y = y + (yy + (dialog ? 50 : 0)) * scale;
                return window.handle(input, ui);
            };
            auto reply = [&](const protocol::GuildRequest& request,
                             bool success,
                             platform::Json state) {
                state.object()["command"].set_number(0);
                state.object()["success"].set_boolean(success);
                auto& id = state.object()["_request_id"];
                id.set_type(platform::Json::Type::String);
                id.string() = std::to_string(request.request_id);
                auto& status = state.object()["status"];
                status.set_type(platform::Json::Type::String);
                status.string() = success ? "Guild updated." : "Configured officer quota reached.";
                window.update(state);
            };
            (void) click(tab_center(layout, 2, true), 60);
            (void) click(50, 234);
            auto request = click(223, 535);
            expect((!request || request->command != protocol::GuildCommand::Rank) &&
                       window.variables().at("guild_ranks.open") == "1" &&
                       window.variables().at("guild_ranks.context").find("GuildMember") !=
                           std::string::npos,
                   "Change rank opens a modal for the selected member without submitting");
            expect(!window.enabled("guild_ranks.confirm"),
                   "the current rank cannot be resubmitted");
            (void) click(50, 116, true);
            expect(!window.enabled("guild_ranks.confirm"),
                   "leadership requires the protected transfer flow");
            (void) click(50, 212, true);
            expect(window.enabled("guild_ranks.confirm"),
                   "an available rank can be explicitly confirmed");
            auto full = view;
            full.object()["guild"].object()["rank_options"].array()[3].object()["limit"].set_number(
                0);
            window.update(full);
            expect(!window.enabled("guild_ranks.confirm"), "live server quotas disable full ranks");
            auto reordered = view;
            std::reverse(reordered.object()["rows"].array().begin(),
                         reordered.object()["rows"].array().end());
            window.update(reordered);
            request = click(666, 524, true);
            expect(request && request->command == protocol::GuildCommand::Rank &&
                       request->target == 2 && request->value == 4 && request->request_id != 0,
                   "confirmation keeps the selected member ID across roster reordering");
            expect(!window.enabled("guild_ranks.confirm"),
                   "pending rank changes cannot be submitted twice");
            auto unrelated = *request;
            unrelated.request_id += 1;
            reply(unrelated, true, reordered);
            expect(window.variables().at("guild_ranks.open") == "1" &&
                       !window.enabled("guild_ranks.confirm"),
                   "unrelated replies do not complete a pending rank change");
            reply(*request, false, reordered);
            expect(window.variables().at("guild_ranks.open") == "1" &&
                       window.variables().at("guild_ranks.status").find("quota") !=
                           std::string::npos,
                   "server rejection keeps the dialog and explains the failure");
            request = click(666, 524, true);
            window.request_failed();
            expect(window.enabled("guild_ranks.confirm"), "send failures restore rank controls");
            request = click(666, 524, true);
            reply(*request, true, reordered);
            expect(window.variables().at("guild_ranks.open") == "0" && window.open(),
                   "acknowledged rank changes close only the child dialog");
            window.update(view);
            (void) click(373, 535);
            expect(window.variables().at("guild_ranks.edit") == "1",
                   "Edit ranks reuses the rank dialog");
            (void) click(50, 244, true);
            (void) click(100, 340, true);
            input.mouse_left_clicked = false;
            input.key_backspace_pressed = true;
            for (int i = 0; i < 20; ++i) {
                (void) window.handle(input, ui);
            }
            input.key_backspace_pressed = false;
            const std::string title = "CaptainGPOKIMx";
            const std::string typed_title = "Captain GPOKIMxyz";
            input.text_char_count = static_cast<int>(typed_title.size());
            std::copy(typed_title.begin(), typed_title.end(), input.text_chars);
            input.key_p = input.key_p_pressed = input.key_g = input.key_g_pressed = true;
            client::mute_gameplay_input(input);
            (void) window.handle(input, ui);
            expect(window.focused() && window.variables().at("guild_ranks.input") == title + "|" &&
                       !input.key_p_pressed && !input.key_g_pressed,
                   "rank titles capture shortcut letters but stop at fourteen without spaces");
            input.text_char_count = 0;
            request = click(666, 340, true);
            expect(request && request->command == protocol::GuildCommand::Title &&
                       request->target == 5 && request->text == title,
                   "saving a title uses the selected rank, not a member index");
            auto renamed = view;
            renamed.object()["guild"].object()["titles"].array()[5].string() = title;
            reply(*request, true, renamed);
            expect(window.variables().at("guild_ranks.open") == "1" &&
                       window.variables().at("guild_ranks.row.5.title") == title,
                   "the rank list reflects the server-confirmed title");
            (void) click(150, 380, true);
            request = click(666, 524, true);
            expect(request && request->command == protocol::GuildCommand::Permissions &&
                       request->target == 5 && request->value == 1,
                   "permission editing submits the selected rank's checkbox mask");
            reply(*request, true, renamed);
            (void) click(50, 116, true);
            expect(!window.enabled("guild_ranks.option.0"), "leader permissions stay protected");
            auto lost = renamed;
            lost.object()["own"].object()["rank"].set_number(5);
            lost.object()["own"].object()["rights"].set_number(0);
            window.update(lost);
            expect(!window.enabled("guild_ranks.edit_title") &&
                       !window.enabled("guild_ranks.save_permissions"),
                   "permission loss disables editing while the dialog is open");
            expect(window.close_rank_dialog() && window.open() && !window.focused(),
                   "Escape closes the modal and releases keyboard capture before the parent");
        }
    }

    void verify_input_alignment(engine::IEngine& renderer,
                                client::WidgetTools& ui,
                                const client::FreetypeFont& font,
                                const std::string& prefix) {
        client::UiDocument doc;
        expect(
            doc.load(platform::Json::parse(
                R"({"schema":"openkal.ui/v1","kind":"hud","window":{"w":840,"h":120},"chrome":{"kind":"pieces","pieces":[]},"widgets":[{"type":"input","x":10,"y":40,"w":240,"h":36,"text":"AgM","pixel_size":13,"color":"#FFFFFF"},{"type":"input","x":280,"y":40,"w":240,"h":36,"align":"center","text":"AgM","pixel_size":13,"color":"#FFFFFF"},{"type":"input","x":550,"y":40,"w":240,"h":36,"align":"right","text":"AgM","pixel_size":13,"color":"#FFFFFF"}]})")),
            "input alignment fixture loads");
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            const int width = static_cast<int>(840 * scale), height = static_cast<int>(120 * scale);
            expect(renderer.begin_offscreen(width, height, .08f, .09f, .08f),
                   "begin input alignment capture");
            client::UiView::draw(
                renderer, ui, doc, 0, 0, width, height, scale, {}, font, font, nullptr, false);
            std::vector<std::uint8_t> rgba;
            const bool read = renderer.end_offscreen_rgba(rgba);
            expect(read && rgba.size() == static_cast<std::size_t>(width * height * 4),
                   "read input alignment pixels");
            if (!read || rgba.size() != static_cast<std::size_t>(width * height * 4)) {
                return;
            }
            for (int field = 0; field < 3; ++field) {
                int top = height, bottom = -1;
                for (int y = static_cast<int>(40 * scale); y < static_cast<int>(76 * scale); ++y) {
                    for (int x = static_cast<int>((10 + field * 270) * scale);
                         x < static_cast<int>((250 + field * 270) * scale);
                         ++x) {
                        const auto at = static_cast<std::size_t>((y * width + x) * 4);
                        if (rgba[at] > 140 && rgba[at + 1] > 140 && rgba[at + 2] > 140) {
                            top = std::min(top, y);
                            bottom = std::max(bottom, y);
                        }
                    }
                }
                expect(bottom >= top &&
                           std::fabs((top + bottom + 1) * .5f - 58 * scale) <= 3 * scale + 1,
                       "left, center and right input glyphs are vertically centered");
            }
            std::ofstream out(prefix + "-inputs-" + std::to_string(static_cast<int>(100 * scale)) +
                                  ".ppm",
                              std::ios::binary);
            out << "P6\n" << width << ' ' << height << "\n255\n";
            for (std::size_t i = 0; i < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "save input alignment capture");
        }
    }

    void verify_scaling(const client::UiLayout& layout, const platform::Json& view) {
        client::WidgetTools ui;
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            client::GuildWindow window;
            window.set_layout(&layout);
            window.update(view);
            window.set_scale(scale);
            window.set_open(true);
            engine::FrameInput input;
            input.width = 1920;
            input.height = 1200;
            (void) window.handle(input, ui);
            const float x = client_x(input, scale), y = client_y(input, scale);
            const float left = (input.width - 980 * scale) * .5f;
            const float top = (input.height - 780 * scale) * .5f;
            expect(window.contains(left + 1, input.height / 2) &&
                       !window.contains(left - 1, input.height / 2) &&
                       window.contains(input.width / 2, top + 1) &&
                       !window.contains(input.width / 2, top - 1),
                   "the complete guild frame opens centered at every UI scale");
            expect(window.contains(x + 940 * scale - 1, y) &&
                       !window.contains(x + 940 * scale + 1, y),
                   "guild window bounds follow the global UI scale");
            input.mouse_left_clicked = true;
            input.mouse_x = x + tab_center(layout, 2, true) * scale;
            input.mouse_y = y + 60 * scale;
            const auto request = window.handle(input, ui);
            expect(request && request->command == protocol::GuildCommand::Members,
                   "scaled member tab keeps its hit region aligned");
            auto guest_view = view;
            guest_view.object()["own"] = platform::Json::parse(R"({"guild":0})");
            window.update(guest_view);
            window.show_creation();
            input.mouse_left_clicked = false;
            (void) window.handle(input, ui);
            input.mouse_left_clicked = true;
            input.mouse_x = x + 450 * scale;
            input.mouse_y = y + 500 * scale;
            (void) window.handle(input, ui);
            expect(window.focused(), "centered creation field captures focus at every scale");
            input.mouse_left_clicked = false;
            input.text_char_count = 7;
            const std::string name = "OpenKal";
            std::copy(name.begin(), name.end(), input.text_chars);
            (void) window.handle(input, ui);
            expect(window.variables().at("guild.input") == "OpenKal|",
                   "centered creation field shows the typed name and caret");
            const std::string extra = " TeamABCDEFGH";
            input.text_char_count = static_cast<int>(extra.size());
            std::copy(extra.begin(), extra.end(), input.text_chars);
            (void) window.handle(input, ui);
            expect(window.variables().at("guild.input") == "OpenKalTeamABC|",
                   "guild name entry rejects spaces and stops at fourteen characters");
            expect(window.enabled("guild.create"), "a valid bounded name enables guild creation");
        }
        client::GuildWindow window;
        window.set_layout(&layout);
        window.update(view);
        window.set_scale(1.5f);
        window.set_open(true);
        engine::FrameInput input;
        input.width = 640;
        input.height = 480;
        (void) window.handle(input, ui);
        expect(window.contains(600, 200) && !window.contains(639, 200),
               "large preferred scale is reduced to fit a small screen");
        input.width = 1920;
        input.height = 1200;
        (void) window.handle(input, ui);
        expect(window.contains(client_x(input, 1.5f) + 940 * 1.5f - 1, 200) &&
                   !window.contains(client_x(input, 1.5f) + 940 * 1.5f + 1, 200),
               "resizing restores the preferred UI scale");
    }
    void verify_centering_and_dragging(const client::UiLayout& layout) {
        client::GuildWindow window;
        client::WidgetTools ui;
        window.set_layout(&layout);
        window.set_open(true);
        engine::FrameInput input;
        input.width = 1920;
        input.height = 1200;
        (void) window.handle(input, ui);
        const float x = client_x(input), y = client_y(input);
        input.mouse_x = x + 200;
        input.mouse_y = y - 40;
        input.mouse_left_clicked = true;
        input.mouse_left = true;
        (void) window.handle(input, ui);
        input.mouse_left_clicked = false;
        input.mouse_x -= 80;
        input.mouse_y -= 20;
        (void) window.handle(input, ui);
        input.mouse_left = false;
        (void) window.handle(input, ui);
        (void) window.handle(input, ui);
        expect(window.contains(x - 120 + 1, y) && !window.contains(x - 120 - 1, y),
               "a manually dragged guild window keeps its position while open");
        window.set_open(false);
        window.set_open(true);
        (void) window.handle(input, ui);
        expect(window.contains(x - 40 + 1, y) && !window.contains(x - 40 - 1, y),
               "reopening the guild window restores its centered position");
        input.width = 1280;
        input.height = 800;
        (void) window.handle(input, ui);
        expect(window.contains(client_x(input) - 39, 400) &&
                   !window.contains(client_x(input) - 41, 400),
               "an untouched guild window stays centered after resizing");
    }
}  // namespace

int main(int argc, char** argv) {
    const bool packaged = argc >= 2 && (std::string(argv[1]) == "--packaged" ||
                                        std::string(argv[1]) == "--capture-packaged");
    const std::string package_root = "/tmp/openkal-guild-package-test";
    if (packaged) {
        std::string error;
        expect(platform::ConfigArchive::mount(package_root, OPENKAL_TEST_CONFIG_ARCHIVE, &error),
               "mount the current build's packaged UI");
    }
    client::UiLayoutStore layouts;
    layouts.set_directory((packaged ? package_root : std::string(OPENKAL_REPO_ROOT)) +
                          "/config/ui");
    const auto& layout = layouts.get("guild");
    expect(layout.loaded(), "load universal guild layout");
    client::UiDocument doc;
    client::UiView::reload(doc, &layout);
    client::UiDocument roundtrip;
    expect(roundtrip.load(doc.to_json()) && roundtrip.widgets().size() == doc.widgets().size(),
           "guild layout editor roundtrip");
    expect(doc.find_widget("dialog_icon") != nullptr,
           "guild menu emblem uses the foreground dialog icon slot");
    const auto* status = doc.find_widget("status_more");
    expect(status && status->y() + status->pixel_size() < doc.window_h(),
           "both operation result lines stay above the client bottom");
    const auto* create_input = doc.find_widget("create_input");
    expect(create_input && create_input->align() == "center" &&
               create_input->x() + create_input->w() / 2 ==
                   doc.find_widget("table_background")->x() + doc.window_w() / 2,
           "creation text is centered in a centered name field");
    const auto* announcement_input = roundtrip.find_widget("announcement_text");
    const auto* previous = doc.find_widget("previous");
    const auto* next = doc.find_widget("next");
    const auto* pages = doc.find_widget("pages");
    expect(previous && next && pages && pages->align() == "center" &&
               pages->x() == (previous->x() + previous->w() + next->x()) / 2,
           "the page indicator is centered in the gap between its arrows");
    expect(doc.find_widget("leave")->y() == doc.find_widget("refresh")->y(),
           "Leave guild aligns vertically with Refresh");
    expect(doc.find_widget("leave")->x() + doc.find_widget("leave")->w() <=
               doc.find_widget("table_background")->x() + doc.find_widget("table_background")->w() -
                   10,
           "Leave guild leaves enough room for its complete right border");
    expect(announcement_input && announcement_input->multiline() &&
               announcement_input->line_height() == 20 && announcement_input->h() == 320,
           "editor roundtrip preserves the multiline announcement area");
    expect(announcement_input && announcement_input->x() + announcement_input->w() / 2 ==
                                     doc.find_widget("table_background")->x() + doc.window_w() / 2,
           "the multiline announcement area is horizontally centered inside the guild window");
    client::UiDocument rank_doc, rank_roundtrip;
    client::UiView::reload(rank_doc, &layouts.get("guild_ranks"));
    expect(rank_roundtrip.load(rank_doc.to_json()) &&
               rank_roundtrip.widgets().size() == rank_doc.widgets().size(),
           "shared rank dialog survives editor roundtrip");
    client::GuildWindow window;
    window.set_layout(&layout);
    window.set_rank_layout(&layouts.get("guild_ranks"));
    window.set_open(true);
    auto view = platform::Json::parse(
        R"({"revision":8,"command":4,"own":{"guild":1,"name":"OpenKal","title":"Leader","rank":1,"rights":31,"daily_reward_available":true},"guild":{"id":1,"name":"OpenKal","grade":4,"count":2,"online":1,"points":120,"invitation":"Welcome to OpenKal. Read the invitation and apply to join.","announcement":"Guild meeting at the village."},"rules":{"creation":{"minimum_level":1,"minimum_members":2,"fee_zamogeon":100,"require_party":true},"membership":{"application_fee_zamogeon":0,"admission_fee_zamogeon":0,"rejoin_lock_seconds":60}},"rows":[{"id":1,"name":"GuildLeader","rank":1,"level":30,"daily":24,"online":true,"last_connection":1793000000},{"id":2,"name":"GuildMember","rank":5,"level":21,"daily":11,"online":false,"last_connection":1792900000}],"page":0,"total":2})");
    verify_scaling(layout, view);
    verify_centering_and_dragging(layout);
    verify_tab_navigation(layout, view);
    verify_membership_and_tables(layout, view);
    verify_announcement(layout, view);
    verify_loaded_announcement(layout, view);
    view.object()["guild"].object()["titles"] = platform::Json::parse(
        R"(["", "Leader", "Sub-Leader", "HeadChief", "Chief", "OfficialMember"])");
    view.object()["guild"].object()["permissions"] = platform::Json::parse("[0, 31, 0, 0, 0, 0]");
    view.object()["guild"].object()["rank_options"] = platform::Json::parse(
        R"([{"rank":1,"count":1,"limit":1,"unlimited":false},{"rank":2,"count":0,"limit":1,"unlimited":false},{"rank":3,"count":0,"limit":1,"unlimited":false},{"rank":4,"count":0,"limit":1,"unlimited":false},{"rank":5,"count":1,"limit":0,"unlimited":true}])");
    verify_rank_dialog(layout, layouts.get("guild_ranks"), view);
    window.update(view);
    client::WidgetTools ui;
    engine::FrameInput input;
    input.width = 1280;
    input.height = 800;
    auto request = window.handle(input, ui);
    expect(request && request->command == protocol::GuildCommand::Ranking,
           "ranking is the initial tab");
    input.mouse_left_clicked = true;
    input.mouse_x = client_x(input) + tab_center(layout, 2, true);
    input.mouse_y = client_y(input) + 60;
    request = window.handle(input, ui);
    expect(request && request->command == protocol::GuildCommand::Members,
           "member tab requests authenticated server data");
    input.mouse_left_clicked = false;
    window.update(view);
    expect(window.variables().at("guild.row.1.color") == "#999999", "offline members are gray");
    expect(window.variables().at("guild.row.0.text").find("Today 24") != std::string::npos,
           "member row contains daily contribution");
    expect(!window.enabled("guild.create") && !window.enabled("guild.leave") &&
               !window.enabled("guild.daily"),
           "membership controls follow server state");
    input.mouse_left_clicked = true;
    input.mouse_x = client_x(input) + tab_center(layout, 5, true);
    input.mouse_y = client_y(input) + 60;
    (void) window.handle(input, ui);
    input.mouse_left_clicked = true;
    input.mouse_x = client_x(input) + 40;
    input.mouse_y = client_y(input) + 500;
    request = window.handle(input, ui);
    expect(window.focused(), "universal text input captures focus");
    input.mouse_left_clicked = false;
    input.key_backspace_pressed = true;
    for (int i = 0; i < 100; ++i) {
        (void) window.handle(input, ui);
    }
    input.key_backspace_pressed = false;
    input.text_char_count = 3;
    input.text_chars[0] = 'T';
    input.text_chars[1] = 'e';
    input.text_chars[2] = 's';
    (void) window.handle(input, ui);
    expect(window.variables().at("guild.input") == "Tes|",
           "guild editor accepts text without chat forwarding");
    input.text_char_count = 0;
    input.mouse_left_clicked = true;
    input.mouse_x = client_x(input) + tab_center(layout, 5, true);
    input.mouse_y = client_y(input) + 60;
    request = window.handle(input, ui);
    expect(!request && window.variables().at("guild.input") == "Tes",
           "clicking the current tab keeps the text draft");
    auto no_rights = view;
    no_rights.object()["own"].object()["rights"].set_number(0);
    no_rights.object()["own"].object()["rank"].set_number(5);
    window.update(no_rights);
    expect(!window.enabled("guild.set_invitation") && !window.enabled("guild.set_permissions"),
           "server permission loss disables management");
    window.update(view);
    input.text_char_count = 0;
    input.mouse_left_clicked = true;
    input.mouse_x = client_x(input) + tab_center(layout, 2, true);
    input.mouse_y = client_y(input) + 60;
    (void) window.handle(input, ui);
    input.mouse_x = client_x(input) + tab_center(layout, 5, true);
    input.mouse_y = client_y(input) + 60;
    request = window.handle(input, ui);
    expect(request && request->command == protocol::GuildCommand::Detail,
           "invitation tab fetches the full public text");
    auto long_view = view;
    std::string body;
    for (int i = 0; i < 16; ++i) {
        body += "GuildApplicantsNeedExplicitConsentBeforeTheyMayBeEnrolledHere ";
    }
    long_view.object()["guild"].object()["invitation"].string() = body;
    long_view.object()["rules"].object()["names"] =
        platform::Json::parse(R"({"maximum_text":1024})");
    window.update(long_view);
    expect(window.enabled("guild.next"), "long invitations expose another readable page");
    input.mouse_x = client_x(input) + 235;
    input.mouse_y = client_y(input) + 570;
    (void) window.handle(input, ui);
    expect(window.variables().at("guild.pages").rfind("2 /", 0) == 0,
           "reader can reach the second invitation page");
    input.mouse_x = client_x(input) + tab_center(layout, 5, true);
    input.mouse_y = client_y(input) + 60;
    request = window.handle(input, ui);
    expect(!request && window.variables().at("guild.pages").rfind("2 /", 0) == 0,
           "clicking the current tab preserves the reader page");
    input.mouse_x = client_x(input) + tab_center(layout, 2, true);
    input.mouse_y = client_y(input) + 60;
    (void) window.handle(input, ui);
    window.update(view);
    input.text_char_count = 0;
    input.mouse_left_clicked = false;
    if (argc == 3 &&
        (std::string(argv[1]) == "--capture" || std::string(argv[1]) == "--capture-packaged")) {
        std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
        if (!renderer->init("OpenKal guild verification", 1280, 800)) {
            expect(false, "initialize GL guild capture");
            return 1;
        }
        ui.init(*renderer, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        client::FreetypeFont font;
        expect(font.load(*renderer, font.find_system_sans(), 13),
               "load owned FreeType capture font");
        verify_input_alignment(*renderer, ui, font, argv[2]);
        auto unavailable_view = view;
        unavailable_view.object()["own"].object()["daily_reward_available"].set_boolean(false);
        window.update(unavailable_view);
        input.mouse_left_clicked = true;
        input.mouse_x = client_x(input) + 790;
        input.mouse_y = client_y(input) + 210;
        (void) window.handle(input, ui);
        input.mouse_left_clicked = false;
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            input.width = 1920;
            input.height = 1200;
            window.set_scale(scale);
            (void) window.handle(input, ui);
            for (const int tab : {7, 2}) {
                input.mouse_left_clicked = true;
                input.mouse_x = client_x(input, scale) + tab_center(layout, tab, true) * scale;
                input.mouse_y = client_y(input, scale) + 60 * scale;
                (void) window.handle(input, ui);
                input.mouse_left_clicked = false;
                expect(renderer->begin_offscreen(1920, 1200, 0.08f, 0.09f, 0.08f),
                       "begin scaled guild capture");
                window.draw(*renderer, ui, font);
                std::vector<std::uint8_t> rgba;
                expect(renderer->end_offscreen_rgba(rgba) && rgba.size() == 1920 * 1200 * 4,
                       "read scaled guild capture pixels");
                const auto base = std::string(argv[2]) + (tab == 7 ? "-membership" : "");
                const auto path =
                    scale == 1
                        ? base
                        : base + "-" + std::to_string(static_cast<int>(scale * 100)) + ".ppm";
                std::ofstream out(path, std::ios::binary);
                out << "P6\n1920 1200\n255\n";
                for (std::size_t i = 0; i < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save scaled guild capture");
            }
        }
        auto capture_rank_mode = [&](bool edit) {
            window.set_scale(1.5f);
            input.mouse_left_clicked = false;
            (void) window.handle(input, ui);
            input.mouse_left_clicked = true;
            input.mouse_x = client_x(input, 1.5f) + 50 * 1.5f;
            input.mouse_y = client_y(input, 1.5f) + 234 * 1.5f;
            (void) window.handle(input, ui);
            input.mouse_x = client_x(input, 1.5f) + (edit ? 373 : 223) * 1.5f;
            input.mouse_y = client_y(input, 1.5f) + 535 * 1.5f;
            (void) window.handle(input, ui);
            expect(window.variables().at("guild_ranks.open") == "1",
                   "open rank mode for GL capture");
            input.mouse_x = client_x(input, 1.5f) + 110 * 1.5f;
            input.mouse_y = client_y(input, 1.5f) + 198 * 1.5f;
            (void) window.handle(input, ui);
            input.mouse_left_clicked = false;
            for (const float scale : {.75f, 1.0f, 1.5f}) {
                window.set_scale(scale);
                (void) window.handle(input, ui);
                expect(renderer->begin_offscreen(1920, 1200, .08f, .09f, .08f),
                       "begin rank dialog capture");
                window.draw(*renderer, ui, font);
                std::vector<std::uint8_t> rgba;
                expect(renderer->end_offscreen_rgba(rgba), "read rank dialog capture");
                const auto mode = edit ? "edit" : "assign";
                std::ofstream out(std::string(argv[2]) + "-ranks-" + mode + "-" +
                                      std::to_string(static_cast<int>(scale * 100)) + ".ppm",
                                  std::ios::binary);
                out << "P6\n1920 1200\n255\n";
                for (std::size_t i = 0; i < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save rank dialog capture");
            }
            window.close_rank_dialog();
        };
        capture_rank_mode(false);
        capture_rank_mode(true);
        auto announcement_view = view;
        announcement_view.object()["guild"].object()["announcement"].string() =
            "Guild meeting\n\nWe will meet at the village at 20:00.\n\n"
            "Bring your supplies and check your equipment before we leave. "
            "New members are welcome to join us; ask an officer if you need help preparing.\n\n"
            "Please keep the guild informed if you cannot attend.";
        window.update(announcement_view);
        input.mouse_left_clicked = true;
        input.mouse_x = client_x(input, 1.5f) + tab_center(layout, 6, true) * 1.5f;
        input.mouse_y = client_y(input, 1.5f) + 60 * 1.5f;
        (void) window.handle(input, ui);
        for (const bool edit : {false, true}) {
            if (edit) {
                input.mouse_left_clicked = true;
                input.mouse_x = client_x(input, 1.5f) + 90 * 1.5f;
                input.mouse_y = client_y(input, 1.5f) + 535 * 1.5f;
                (void) window.handle(input, ui);
            }
            input.mouse_left_clicked = false;
            for (const float scale : {.75f, 1.0f, 1.5f}) {
                window.set_scale(scale);
                (void) window.handle(input, ui);
                expect(renderer->begin_offscreen(1920, 1200, .08f, .09f, .08f),
                       "begin announcement capture");
                window.draw(*renderer, ui, font);
                std::vector<std::uint8_t> rgba;
                expect(renderer->end_offscreen_rgba(rgba), "read announcement capture");
                const auto mode = edit ? "edit" : "read";
                std::ofstream out(std::string(argv[2]) + "-announcement-" + mode + "-" +
                                      std::to_string(static_cast<int>(scale * 100)) + ".ppm",
                                  std::ios::binary);
                out << "P6\n1920 1200\n255\n";
                for (std::size_t i = 0; i < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save announcement capture");
            }
        }
        input.mouse_left_clicked = true;
        input.mouse_x = client_x(input, 1.5f) + 240 * 1.5f;
        input.mouse_y = client_y(input, 1.5f) + 535 * 1.5f;
        (void) window.handle(input, ui);
        input.mouse_left_clicked = false;
        auto creation_view = view;
        creation_view.object()["own"] = platform::Json::parse(R"({"guild":0})");
        creation_view.object()["guild"] = platform::Json::parse("{}");
        creation_view.object()["rows"] = platform::Json::parse("[]");
        creation_view.object()["total"].set_number(0);
        creation_view.object()["command"].set_number(2);
        creation_view.object()["rules"].object()["creation"].object()["fee_zamogeon"].set_number(
            100000);
        creation_view.object()["status"].set_type(platform::Json::Type::String);
        creation_view.object()["status"].string() =
            "Guild creation requires 100000 Zamogeon; you have 99. "
            "Every founding party member must be eligible and explicitly accept the request "
            "before the guild can be created. No money has been deducted.";
        window.update(creation_view);
        window.show_creation();
        window.update(creation_view);
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            (void) window.handle(input, ui);
            expect(renderer->begin_offscreen(1920, 1200, 0.08f, 0.09f, 0.08f),
                   "begin centered creation capture");
            window.draw(*renderer, ui, font);
            const auto vars = window.variables();
            expect(!vars.at("guild.status_more").empty() &&
                       font.text_width(vars.at("guild.status")) <= 872 &&
                       font.text_width(vars.at("guild.status_more")) <= 872,
                   "long creation feedback wraps to measured footer lines");
            std::vector<std::uint8_t> rgba;
            expect(renderer->end_offscreen_rgba(rgba), "read centered creation capture");
            std::ofstream out(std::string(argv[2]) + "-create-" +
                                  std::to_string(static_cast<int>(scale * 100)) + ".ppm",
                              std::ios::binary);
            out << "P6\n1920 1200\n255\n";
            for (std::size_t i = 0; i < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "save centered creation capture");
        }
        window.destroy(*renderer);
        font.destroy(*renderer);
        ui.destroy();
        renderer->shutdown();
    }
    window.set_open(false);
    expect(!window.focused() && !window.contains(100, 100),
           "closing releases input and hit region");
    view.object()["own"] = platform::Json::parse(R"({"guild":0})");
    window.update(view);
    window.show_creation();
    request = window.handle(input, ui);
    expect(window.open() && request && request->command == protocol::GuildCommand::Search &&
               !window.enabled("guild.create") &&
               window.variables().at("guild.requirements").find("100 Zamogeon") !=
                   std::string::npos,
           "NPC creation entry opens the creation page with the configured fee");
    view.object()["status"].set_type(platform::Json::Type::String);
    view.object()["status"].string() = "Guild creation requires 100 Zamogeon; you have 99.";
    view.object()["command"].set_number(0);
    window.update(view);
    view.object().erase("status");
    window.update(view);
    expect(window.variables().at("guild.status").find("you have 99") != std::string::npos,
           "failure stays visible after an unsolicited membership snapshot");
    view.object()["status"].set_type(platform::Json::Type::String);
    view.object()["status"].string() = std::string(150, 'W') + " No money has been deducted.";
    window.update(view);
    expect(!window.variables().at("guild.status_more").empty(),
           "an oversized message uses the reserved second result line");
    return test::g_failures ? 1 : 0;
}
