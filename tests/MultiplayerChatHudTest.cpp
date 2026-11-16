/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GameHud.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {
    using namespace openkal::client;
    using openkal::engine::FrameInput;
    using openkal::protocol::BattleMsgKind;
    using openkal::protocol::ChatChannel;
    using openkal::test::expect;

    ChatInputResult input(ChatWidget& chat,
                          const UiLayout& layout,
                          const FrameInput& frame,
                          const std::function<void(std::string_view)>& send = {},
                          float scale = 1) {
        return chat.handle_input(frame, 24, 24, 450 * scale, 226 * scale, scale, &layout, send);
    }

    FrameInput click_tab(const UiWidget& strip, const UiTabItem& tab, float scale = 1) {
        FrameInput frame;
        frame.mouse_left_clicked = true;
        frame.mouse_x = 24 + (tab.x() + tab.w() / 2) * scale;
        frame.mouse_y = 24 + (strip.y() + strip.h() / 2) * scale;
        return frame;
    }

    void type(ChatWidget& chat, const UiLayout& layout, std::string_view text) {
        for (const char c : text) {
            FrameInput frame;
            frame.text_chars[0] = c;
            frame.text_char_count = 1;
            (void) input(chat, layout, frame);
        }
    }

    void settings_roundtrip(UiLayoutStore& layouts) {
        const auto path =
            std::filesystem::temp_directory_path() /
            ("openkal-chat-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        GameHud hud;
        hud.set_layouts(&layouts);
        hud.reset_defaults(1920, 1080);
        auto& filters = hud.chat().filters();
        filters.set_active_tab(ChatTab::Shout);
        filters.set_channel_enabled(ChatTab::Guild, ChatChannel::Whisper, true);
        filters.set_battle_enabled(BattleMsgKind::Damage, false);
        UiSettings saved;
        saved.capture_character(hud, 42, "Chat tester", 1);
        expect(saved.save(path.string()), "save per-character chat options");
        UiSettings loaded;
        expect(loaded.load(path.string()), "load per-character chat options");
        GameHud restored;
        loaded.apply_character(restored, 42, 1);
        const auto& result = restored.chat().filters();
        expect(result.active_tab() == ChatTab::Shout &&
                   result.channel_flags() == filters.channel_flags() &&
                   result.battle_flags() == filters.battle_flags(),
               "tab, independent channel options and global battle options survive disk roundtrip");

        // Old defaults upgrade to focused tabs; custom supported rows are preserved.
        {
            std::ofstream file(path);
            file << R"({"characters":{"42":{"chat":{"active_tab":5,"channels":[)";
            for (int t = 0; t < 6; ++t) {
                for (int c = 0; c < 8; ++c) {
                    if (t || c) {
                        file << ", ";
                    }
                    const int own = t == 3 ? 4 : t;
                    const bool enabled =
                        t == 0 || c == own || c == 3 || c == 7 || (t == 2 && c == 1);
                    file << (enabled ? 1 : 0);
                }
            }
            file << R"(],"battle_msgs":[0, 1, 1, 1, 1, 1, 1, 1, 1, 1]}}}})";
        }
        UiSettings migrated;
        expect(migrated.load(path.string()), "load obsolete channel matrix");
        GameHud upgraded;
        migrated.apply_character(upgraded, 42, 1);
        const auto& updated = upgraded.chat().filters();
        expect(updated.active_tab() == ChatTab::Normal, "removed tab never becomes Shout or Squad");
        expect(!updated.channel_enabled(ChatTab::Normal, ChatChannel::Guild) &&
                   updated.channel_enabled(ChatTab::Normal, ChatChannel::Whisper),
               "old all-channel default becomes focused Normal with personal messages");
        expect(updated.channel_enabled(ChatTab::Guild, ChatChannel::Party) &&
                   updated.channel_enabled(ChatTab::Guild, ChatChannel::Shout),
               "supported custom filter rows migrate with correct channel indices");
        expect(updated.channel_enabled(ChatTab::Shout, ChatChannel::Shout) &&
                   updated.channel_enabled(ChatTab::Squad, ChatChannel::Squad) &&
                   !updated.battle_enabled(BattleMsgKind::Damage),
               "new tabs keep defaults and global battle preferences survive migration");
        std::filesystem::remove(path);
    }

    void capture(const std::string& prefix, UiLayoutStore& layouts) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Multiplayer chat verification", 1100, 720)) {
            expect(false, "initialize chat GL capture");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        const auto& layout = layouts.get("chat");
        const std::array<std::pair<const char*, const char*>, 6> messages{
            {{"Traveler", "Normal conversation"},
             {"Companion", "#Party conversation"},
             {"Guildmate", "$Guild conversation"},
             {"Ally", "~Alliance conversation"},
             {"Friend", "@Hero Personal message"},
             {"Herald", "^Shout announcement"}}};
        const auto draw = [&](ChatWidget& chat, float scale) {
            expect(engine->begin_offscreen(1100, 720, .12f, .15f, .17f), "begin chat capture");
            chat.draw(ui, *engine, 24, 250, 450 * scale, 226 * scale, scale, &layout);
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read chat capture");
            return rgba;
        };
        const auto write = [&](const std::string& name,
                               const std::vector<std::uint8_t>& rgba,
                               int width = 1100,
                               int height = 720) {
            std::ofstream file(prefix + name + ".ppm", std::ios::binary);
            file << "P6\n" << width << ' ' << height << "\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                file.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(file.good(), "write multiplayer chat capture");
        };
        ChatWidget populated;
        for (const auto& [speaker, text] : messages) {
            populated.push_chat(speaker, text);
        }
        populated.push_info("Global battle message", BattleMsgKind::Damage);
        for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
            const auto tab = static_cast<ChatTab>(t);
            populated.filters().set_active_tab(tab);
            ChatWidget reference;
            reference.filters().set_active_tab(tab);
            reference.push_info("Global battle message", BattleMsgKind::Damage);
            for (const auto& [speaker, text] : messages) {
                if (reference.filters().accepts(openkal::protocol::Chatting::channel_of(text),
                                                std::nullopt)) {
                    reference.push_chat(speaker, text);
                }
            }
            for (const float scale : {1.0f, 1.5f}) {
                const auto actual = draw(populated, scale);
                expect(
                    actual == draw(reference, scale),
                    "rendered tab contains exactly its filtered history plus global battle feed");
                write(std::string("-") + ChatFilterModel::tab_id(tab) +
                          (scale > 1 ? "-scaled" : ""),
                      actual);
            }
            reference.destroy(*engine);
        }
        const auto check_label =
            [&](ChatWidget& chat, ChatChannel channel, const std::string& name) {
                const auto pixels = draw(chat, 1);
                float r = 0, g = 0, b = 0;
                ChatFilterModel::channel_color(channel, r, g, b);
                const std::array<float, 3> target{r * 255, g * 255, b * 255};
                const auto background_offset = static_cast<std::size_t>((430 * 1100 + 93) * 4);
                std::array<float, 3> background{};
                for (std::size_t c = 0; c < 3; ++c) {
                    background[c] = pixels[background_offset + c];
                }
                std::size_t dominant = 0;
                for (std::size_t c = 1; c < 3; ++c) {
                    if (std::abs(target[c] - background[c]) >
                        std::abs(target[dominant] - background[dominant])) {
                        dominant = c;
                    }
                }
                int matches = 0;
                for (int y = 430; y < 446; ++y) {
                    for (int x = 32; x < 94; ++x) {
                        const auto offset = static_cast<std::size_t>((y * 1100 + x) * 4);
                        // FreeType coverage blends the intended color with the panel background.
                        const float alpha = (pixels[offset + dominant] - background[dominant]) /
                                            (target[dominant] - background[dominant]);
                        bool matches_color = alpha > .25f && alpha <= 1.01f;
                        for (std::size_t c = 0; c < 3; ++c) {
                            matches_color &=
                                std::abs(pixels[offset + c] -
                                         (background[c] + alpha * (target[c] - background[c]))) < 3;
                        }
                        if (matches_color) {
                            ++matches;
                        }
                    }
                }
                expect(matches > 4, "compose label uses its actual sending channel color");
                write("-label-" + name, pixels);
            };
        for (const auto tab : {ChatTab::Guild, ChatTab::Alliance, ChatTab::Shout}) {
            ChatWidget colored;
            colored.filters().set_active_tab(tab);
            check_label(colored, ChatFilterModel::tab_channel(tab), ChatFilterModel::tab_id(tab));
            colored.destroy(*engine);
        }
        ChatWidget pm_label;
        pm_label.begin_whisper("Friend");
        check_label(pm_label, ChatChannel::Whisper, "pm");
        pm_label.destroy(*engine);

        for (const float scale : {1.0f, 1.5f}) {
            GameHud hud;
            hud.set_layouts(&layouts);
            hud.set_scale(scale);
            hud.reset_defaults(2560, 1440);
            hud.chat().filters().set_active_tab(ChatTab::Guild);
            hud.chat().push_chat("Guildmate", "$A wider guild chat.");
            hud.sync_fonts(*engine);
            expect(engine->begin_offscreen(2560, 1440, .12f, .15f, .17f),
                   "begin wider HUD capture");
            hud.draw(ui, *engine, StatusWidgetData{}, MinimapWidgetData{}, DragonHudData{});
            std::vector<std::uint8_t> pixels;
            expect(engine->end_offscreen_rgba(pixels), "read wider HUD capture");
            write(scale > 1 ? "-wide-hud-scaled" : "-wide-hud", pixels, 2560, 1440);
            const auto& panel = static_cast<const GameHud&>(hud).panel(HudPanelId::Chat);
            const auto& strip = *UiView::document(&layout).find_widget("channels");
            const float stretch = (panel.w() / scale - 16) / 434;
            for (std::size_t t = 0; t < strip.tabs().size(); ++t) {
                const auto& tab = strip.tabs()[t];
                FrameInput click;
                click.mouse_left_clicked = true;
                click.mouse_x = panel.x() + (8 + (tab.x() + tab.w() / 2 - 8) * stretch) * scale;
                click.mouse_y = panel.y() + (strip.y() + strip.h() / 2) * scale;
                (void) hud.chat().handle_input(
                    click, panel.x(), panel.y(), panel.w(), panel.h(), scale, &layout, {});
                expect(hud.chat().filters().active_tab() == static_cast<ChatTab>(t),
                       "expanded tabs retain matching render and click geometry");
            }
            hud.destroy_fonts(*engine);
        }
        FrameInput gear;
        gear.mouse_left_clicked = true;
        gear.mouse_x = 24 + 450 - 16;
        gear.mouse_y = 250 + 12;
        (void) populated.handle_input(gear, 24, 250, 450, 226, 1, &layout, {});
        write("-options", draw(populated, 1));
        for (const float scale : {1.0f, 1.5f}) {
            const auto handle = [&](ChatWidget& pm,
                                    const FrameInput& frame,
                                    const std::function<void(std::string_view)>& send = {}) {
                return pm.handle_input(
                    frame, 24, 250, 450 * scale, 226 * scale, scale, &layout, send);
            };
            const auto reply = [&](ChatWidget& pm, const std::string& expected) {
                FrameInput click;
                click.mouse_left_clicked = true;
                click.mouse_x = 24 + 12 * scale;
                click.mouse_y = 250 + 163 * scale;
                std::string sent;
                expect(handle(pm, click, [&](std::string_view value) { sent = value; }) ==
                               ChatInputResult::Handled &&
                           sent.empty() && pm.focused(),
                       "PM click opens a draft without sending");
                type(pm, layout, "Reply");
                FrameInput enter;
                enter.key_enter_pressed = true;
                (void) handle(pm, enter, [&](std::string_view value) { sent = value; });
                expect(sent == expected,
                       "visible PM row selects the correct partner at this scale");
            };
            ChatWidget incoming;
            incoming.push_chat("Friend", "@Hero Hello!", "Hero");
            (void) draw(incoming, scale);
            reply(incoming, "@Friend Reply");
            incoming.destroy(*engine);

            ChatWidget outgoing;
            outgoing.push_chat("Hero", "@Friend Hello!", "Hero");
            (void) draw(outgoing, scale);
            reply(outgoing, "@Friend Reply");
            outgoing.destroy(*engine);

            ChatWidget wrapped;
            std::string long_message = "@Hero ";
            for (int i = 0; i < 16; ++i) {
                long_message += "hello ";
            }
            wrapped.push_chat("LongSender", long_message, "Hero");
            (void) draw(wrapped, scale);
            reply(wrapped, "@LongSender Reply");
            wrapped.destroy(*engine);

            ChatWidget history;
            for (int i = 0; i < 20; ++i) {
                history.push_chat("Sender" + std::to_string(i), "@Hero Hello!", "Hero");
            }
            (void) draw(history, scale);
            FrameInput wheel;
            wheel.mouse_x = 24 + 20 * scale;
            wheel.mouse_y = 250 + 150 * scale;
            wheel.mouse_scroll = 10;
            (void) handle(history, wheel);
            (void) draw(history, scale);
            reply(history, "@Sender9 Reply");
            history.destroy(*engine);

            ChatWidget hidden;
            hidden.push_chat("Friend", "@Hero Hello!", "Hero");
            (void) draw(hidden, scale);
            hidden.filters().set_active_tab(ChatTab::Guild);
            reply(hidden, "$Reply");
            hidden.destroy(*engine);
        }
        populated.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    const auto& layout = layouts.get("chat");
    expect(layout.loaded(), "load multiplayer chat layout");
    UiDocument doc;
    UiView::reload(doc, &layout);
    UiDocument roundtrip;
    expect(roundtrip.load(doc.to_json()), "chat layout survives UI editor roundtrip");
    const auto* strip = roundtrip.find_widget("channels");
    expect(strip && strip->tabs().size() == 6 && strip->bind() == "chat.tab",
           "universal chat layout exposes the six channel tabs");
    if (!strip || strip->tabs().size() != 6) {
        return 1;
    }
    for (const auto viewport : std::array<std::array<float, 3>, 4>{
             {{1920, 1080, 1}, {2560, 1440, 1}, {3840, 2160, 1.5f}, {1280, 720, 1}}}) {
        GameHud hud;
        hud.set_layouts(&layouts);
        hud.set_scale(viewport[2]);
        hud.reset_defaults(viewport[0], viewport[1]);
        const auto check_gap = [&] {
            const auto& panel = static_cast<const GameHud&>(hud).panel(HudPanelId::Chat);
            const auto& skills = static_cast<const GameHud&>(hud).panel(HudPanelId::Dragon);
            const float base = doc.window_w() * hud.scale();
            const float old_gap = skills.x() - panel.x() - base;
            const float new_gap = skills.x() - panel.x() - panel.w();
            expect(std::abs(panel.w() - (base + std::max(0.0f, old_gap) / 2)) < .01f,
                   "chat width halves the positive skill-widget gap at this viewport and scale");
            if (old_gap > 0) {
                expect(new_gap > 0 && std::abs(new_gap - old_gap / 2) < .01f,
                       "expanded chat keeps half the breathing room");
            }
        };
        check_gap();
        hud.set_panel_pos(HudPanelId::Chat,
                          32 * hud.scale(),
                          static_cast<const GameHud&>(hud).panel(HudPanelId::Chat).y());
        check_gap();
        expect(hud.ensure_layout(viewport[0], viewport[1], viewport[2] * .9f),
               "changing UI scale recomputes chat geometry");
        check_gap();
    }
    ChatWidget chat;
    for (const float scale : {1.0f, 1.5f}) {
        for (std::size_t t = 0; t < strip->tabs().size(); ++t) {
            const auto tab = static_cast<ChatTab>(t);
            expect(input(chat, layout, click_tab(*strip, strip->tabs()[t], scale), {}, scale) ==
                       ChatInputResult::Handled,
                   "tab click is consumed at every tested UI scale");
            expect(chat.filters().active_tab() == tab && !chat.focused(),
                   "tab click selects channel without grabbing keyboard focus");
        }
    }
    const std::array<const char*, 6> outgoing{{"hello", "#hello", "$hello", "", "", "^hello"}};
    for (std::size_t t = 0; t < outgoing.size(); ++t) {
        ChatWidget composer;
        (void) input(composer, layout, click_tab(*strip, strip->tabs()[t]));
        FrameInput enter;
        enter.key_enter_pressed = true;
        (void) input(composer, layout, enter);
        type(composer, layout, "hello");
        std::string sent;
        const auto result =
            input(composer, layout, enter, [&](std::string_view text) { sent = text; });
        expect(sent == outgoing[t], "ordinary input uses the selected tab's destination");
        if (t == 3 || t == 4) {
            expect(result == ChatInputResult::Handled && composer.focused() &&
                       !composer.message_log().lines().empty(),
                   "unavailable channel retains draft and reports why it cannot send");
        }
    }
    for (const auto text :
         {"@Friend hello", "#hello", "$hello", "~hello", "^hello", "!hello", "/time", "/s hello"}) {
        ChatWidget composer;
        composer.filters().set_active_tab(ChatTab::Guild);
        FrameInput enter;
        enter.key_enter_pressed = true;
        (void) input(composer, layout, enter);
        type(composer, layout, text);
        std::string sent;
        (void) input(composer, layout, enter, [&](std::string_view value) { sent = value; });
        expect(sent == text, "explicit channel prefixes and commands override the active tab");
    }
    {
        ChatWidget composer;
        composer.filters().set_active_tab(ChatTab::Guild);
        FrameInput enter;
        enter.key_enter_pressed = true;
        (void) input(composer, layout, enter);
        type(composer, layout, std::string(openkal::protocol::kChatMaxChars, 'a'));
        std::string sent;
        expect(input(composer, layout, enter, [&](std::string_view text) { sent = text; }) ==
                       ChatInputResult::Handled &&
                   sent.empty(),
               "adding a destination marker never silently truncates a full message");
        FrameInput backspace;
        backspace.key_backspace_pressed = true;
        (void) input(composer, layout, backspace);
        expect(input(composer, layout, enter, [&](std::string_view text) { sent = text; }) ==
                       ChatInputResult::Submitted &&
                   sent.size() == openkal::protocol::kChatMaxChars && sent.front() == '$',
               "oversize rejection retains the draft and editing it allows submission");
    }
    {
        ChatWidget messages;
        messages.push_chat("Guildmate", "$Hello, #friend!", "Hero");
        messages.push_chat("Friend", "@Hero Hello!", "Hero");
        messages.push_chat("Hero", "@Friend Hi!", "Hero");
        const auto& lines = messages.chat_log().lines();
        expect(lines[0].text() == "Guildmate: Hello, #friend!" && lines[0].reply_target().empty(),
               "chat output hides routing markers and preserves actual message punctuation");
        expect(lines[1].text() == "Friend: Hello!" && lines[1].reply_target() == "Friend",
               "incoming personal message stores sender separately from clean display text");
        expect(lines[2].text() == "To Friend: Hi!" && lines[2].reply_target() == "Friend",
               "outgoing echo preserves the conversation partner rather than replying to self");
        messages.filters().set_active_tab(ChatTab::Guild);
        messages.filters().set_channel_enabled(ChatTab::Normal, ChatChannel::Whisper, false);
        messages.begin_whisper("Friend");
        expect(messages.focused() && messages.filters().active_tab() == ChatTab::Normal &&
                   messages.filters().channel_enabled(ChatTab::Normal, ChatChannel::Whisper),
               "starting a personal message opens input and makes its replies visible");
        type(messages, layout, "Thanks");
        FrameInput enter;
        enter.key_enter_pressed = true;
        std::string sent;
        (void) input(messages, layout, enter, [&](std::string_view value) { sent = value; });
        expect(sent == "@Friend Thanks",
               "personal-message draft includes the partner and separator");
    }
    // The options header no longer switches tabs; channel rows always edit the active tab.
    FrameInput gear;
    gear.mouse_left_clicked = true;
    gear.mouse_x = 24 + 450 - 16;
    gear.mouse_y = 24 + 12;
    (void) input(chat, layout, gear);
    expect(chat.options_open(), "gear opens tab options");
    const auto options = ChatWidget::options_rect(24, 24, 450, 226, 1);
    FrameInput toggle = gear;
    toggle.mouse_x = options.x + 20;
    toggle.mouse_y = options.y + 22 + 8;  // Common row on the selected Shout tab.
    (void) input(chat, layout, toggle);
    expect(chat.filters().channel_enabled(ChatTab::Shout, ChatChannel::Common) &&
               !chat.filters().channel_enabled(ChatTab::Guild, ChatChannel::Common),
           "options change only the active tab's channel filters");
    toggle.mouse_y = options.y + 22 + 16 * ChatFilterModel::kChannelCount + 4 + 16 + 8;
    (void) input(chat, layout, toggle);
    chat.filters().set_active_tab(ChatTab::Party);
    expect(!chat.filters().battle_enabled(BattleMsgKind::Damage),
           "battle toggle remains global after switching tabs");
    ChatWidget server_reply;
    server_reply.filters().set_active_tab(ChatTab::Guild);
    server_reply.push_chat("Server", "You are not in a guild.");
    expect(server_reply.message_log().lines().size() == 1,
           "server rejection remains visible even when Normal chat is filtered out");
    settings_roundtrip(layouts);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(argv[2], layouts);
    }
    return openkal::test::g_failures ? 1 : 0;
}
