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
#include "openkal/protocol/ClockSync.h"
#include "openkal/server/ClockSchedule.h"
#include "test_util.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

namespace {
    using openkal::test::expect;
    using namespace openkal;

    void protocol_and_clock() {
        protocol::ClockSyncFields snapshot{
            1700000000, 3600, 90061, {{"castle_war", "Castle War", 1700183845, 1700788645}}};
        const auto frame = protocol::ClockSync::frame(snapshot);
        expect(!frame.empty(), "clock packet encodes");
        protocol::ClockSyncFields parsed;
        expect(protocol::ClockSync::parse(frame.data() + 3, frame.size() - 3, parsed) &&
                   parsed.events.size() == 1 && parsed.events[0].id == "castle_war" &&
                   parsed.online_seconds == 90061 && parsed.utc_offset_seconds == 3600,
               "clock packet roundtrip");
        for (std::size_t size = 0; size < frame.size() - 3; ++size) {
            protocol::ClockSyncFields untouched{1234, 0, 42, {}};
            expect(!protocol::ClockSync::parse(frame.data() + 3, size, untouched) &&
                       untouched.unix_seconds == 1234 && untouched.online_seconds == 42,
                   "truncated packet preserves snapshot");
        }
        auto trailing = frame;
        trailing.push_back(0);
        expect(!protocol::ClockSync::parse(trailing.data() + 3, trailing.size() - 3, parsed),
               "reject trailing packet data");
        auto invalid = snapshot;
        invalid.events.push_back(invalid.events.front());
        expect(protocol::ClockSync::frame(invalid).empty(), "reject duplicate event IDs");
        invalid = snapshot;
        invalid.utc_offset_seconds = 86401;
        expect(protocol::ClockSync::frame(invalid).empty(), "reject invalid UTC offset");
        invalid = snapshot;
        invalid.events[0].following_at = invalid.events[0].starts_at;
        expect(protocol::ClockSync::frame(invalid).empty(), "reject unordered event occurrences");
        invalid = snapshot;
        invalid.events.resize(protocol::ClockSync::kMaxEvents + 1);
        expect(protocol::ClockSync::frame(invalid).empty(), "bound event catalog size");

        client::ServerClock clock;
        const auto received = client::ServerClock::TimePoint(std::chrono::seconds(100));
        expect(!clock.ready() && clock.time_text(received) == "--:--:--" &&
                   clock.countdown_text("castle_war", received) == "00:00:00:00",
               "unsynchronized clock uses neutral placeholders");
        clock.synchronize(snapshot, received);
        expect(clock.time_text(received) == "23:13:20", "server UTC offset drives wall time");
        expect(clock.online_text(received) == "25:01:01", "online time retains full session hours");
        expect(clock.time_text(received + std::chrono::seconds(3601)) == "00:13:21",
               "monotonic elapsed time advances across midnight");
        expect(clock.countdown_text("", received) == "00:00:00:00" &&
                   clock.countdown_text("missing", received) == "00:00:00:00",
               "no selected or removed event yields zero countdown");
        expect(clock.countdown_text("castle_war", received) == "02:03:04:05",
               "countdown formats days hours minutes seconds");
        const auto starts = received + std::chrono::seconds(183845);
        expect(clock.countdown_text("castle_war", starts) == "00:00:00:00" &&
                   clock.countdown_text("castle_war", starts + std::chrono::seconds(1)) ==
                       "06:23:59:59",
               "event boundary advances to the following server occurrence");
        snapshot.unix_seconds += 3600;
        snapshot.online_seconds += 3600;
        clock.synchronize(snapshot, received + std::chrono::seconds(3600));
        expect(clock.online_text(received + std::chrono::seconds(3601)) == "26:01:02",
               "hourly resynchronization preserves account-session duration");
        clock.reset();
        expect(!clock.ready() && clock.events().empty() && clock.unix_seconds(received) == 0,
               "disconnect clears clock and events");
    }

    void schedule() {
        const auto path = std::filesystem::temp_directory_path() /
                          ("openkal_clock_schedule_" + std::to_string(getpid()) + ".json");
        server::ClockSchedule schedule;
        std::ofstream(path)
            << R"({"clock_events":[{"id":"war","name":"Castle War","weekdays":[0],"hour":12,"minute":0}]})";
        expect(schedule.load_file(path.string()), "load optional weekly event notice");
        const char* old_tz = std::getenv("TZ");
        const std::string previous = old_tz ? old_tz : "";
        const bool had_tz = old_tz != nullptr;
        setenv("TZ", "Europe/Berlin", 1);
        tzset();
        // Sunday before the spring DST transition: local noon remains local noon.
        const auto snapshot = schedule.snapshot(1774170000, 123);
        expect(snapshot.online_seconds == 123 && snapshot.utc_offset_seconds == 3600 &&
                   snapshot.events.size() == 1,
               "snapshot carries server offset and authoritative online duration");
        if (!snapshot.events.empty()) {
            const auto& event = snapshot.events.front();
            expect(event.following_at - event.starts_at == 7 * 86400 - 3600,
                   "weekly event follows server DST instead of a fixed seven-day duration");
            protocol::ClockSyncFields parsed;
            const auto bytes = protocol::ClockSync::frame(snapshot);
            expect(!bytes.empty() &&
                       protocol::ClockSync::parse(bytes.data() + 3, bytes.size() - 3, parsed),
                   "server schedule yields a valid wire snapshot");
        }
        std::ofstream(path)
            << R"({"clock_events":[{"id":"bad","name":"Bad","weekdays":[7],"hour":12,"minute":0}]})";
        expect(!schedule.load_file(path.string()), "reject invalid weekday");
        std::ofstream(path) << R"({"clock_events":[]})";
        expect(schedule.load_file(path.string()) && schedule.snapshot(1700000000, 0).events.empty(),
               "empty prepared event list remains empty");
        std::ofstream(path) << R"({"events":[]})";
        expect(schedule.load_file(path.string()), "older server event config remains valid");
        std::filesystem::remove(path);
        if (had_tz) {
            setenv("TZ", previous.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
    }

    void hud_and_capture(int argc, char** argv) {
        client::UiLayoutStore layouts;
        layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
        expect(layouts.get("clock").loaded() && layouts.get("clock_event_options").loaded(),
               "clock and event menu load through the universal schema");
        const auto& document = client::UiView::document(&layouts.get("clock"));
        client::UiDocument roundtrip;
        expect(roundtrip.load(document.to_json()) &&
                   roundtrip.chrome_kind() == client::UiChromeKind::Pieces &&
                   roundtrip.find_widget("frame") &&
                   roundtrip.find_widget("frame")->thickness() == 3 &&
                   roundtrip.find_widget("frame")->border_top() == "chatting-box-top.bmp" &&
                   roundtrip.find_widget("close") &&
                   roundtrip.find_widget("close")->action() == "window.close",
               "compact clock preserves chat border and actions in editor roundtrip");
        client::GameHud hud;
        client::ServerClock clock;
        clock.synchronize(
            {1700000000, 3600, 5025, {{"war", "Castle War", 1700183845, 1700788645}}});
        hud.set_layouts(&layouts);
        hud.set_server_clock(&clock);
        hud.reset_defaults(1000, 700);
        const auto panel =
            static_cast<const client::GameHud&>(hud).panel(client::HudPanelId::Clock);
        expect(panel.visible() && panel.movable() && panel.w() == 198 && panel.h() == 118 &&
                   panel.x() > 700 && panel.y() > 450,
               "compact movable clock defaults to the lower right");
        engine::FrameInput input;
        input.mouse_x = panel.x() + 20;
        input.mouse_y = panel.y() + 10;
        input.mouse_left = input.mouse_left_clicked = true;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::Handled &&
                   hud.is_dragging(),
               "clock heading starts dragging");
        input.mouse_left_clicked = false;
        input.mouse_x = 500;
        input.mouse_y = 400;
        (void) hud.handle_pointer(input, 1000, 700);
        input.mouse_left = false;
        (void) hud.handle_pointer(input, 1000, 700);
        expect(!hud.is_dragging(), "clock drag ends on release");
        hud.set_panel_pos(client::HudPanelId::Clock, 700, 480);
        client::UiSettings saved;
        saved.capture_character(hud, 17, "Clock tester", hud.scale());
        client::GameHud restored;
        restored.set_layouts(&layouts);
        restored.reset_defaults(1000, 700);
        saved.apply_character(restored, 17, restored.scale());
        expect(static_cast<const client::GameHud&>(restored).panel(client::HudPanelId::Clock).x() ==
                   700,
               "clock uses existing per-character position persistence");
        input.mouse_left = input.mouse_left_clicked = true;
        input.mouse_x = 700 + 5 + 100;
        input.mouse_y = 480 + 5 + 74;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::Handled &&
                   !hud.is_dragging(),
               "event dropdown opens without dragging the panel");
        expect(hud.contains(720, 440), "open event dropdown captures its own pointer area");
        if (argc > 1) {
            std::unique_ptr<engine::IEngine> engine(engine::create_engine());
            const bool initialized = engine->init("Clock verification", 1000, 700);
            expect(initialized, "initialize clock capture");
            if (!initialized) {
                return;
            }
            client::WidgetTools ui;
            ui.init(*engine, argc > 2 ? argv[2] : "", OPENKAL_TEST_RUNTIME_ROOT);
            hud.sync_fonts(*engine);
            expect(engine->begin_offscreen(1000, 700, 0.10f, 0.14f, 0.18f), "begin clock capture");
            hud.draw(ui, *engine, {}, {}, {});
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read clock preview");
            std::ofstream out(argv[1], std::ios::binary);
            out << "P6\n1000 700\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            hud.destroy_fonts(*engine);
            ui.destroy();
            engine->shutdown();
        }
        // The X closes the widget in one click even while its menu is open.
        input.mouse_x = 700 + 5 + 175;
        input.mouse_y = 480 + 5 + 12;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::ClockClosed &&
                   !hud.clock_visible(),
               "frame X disables the clock");
        expect(hud.ensure_layout(1200, 800, 1.25f) && !hud.clock_visible() &&
                   !static_cast<const client::GameHud&>(hud)
                        .panel(client::HudPanelId::Clock)
                        .visible(),
               "resize and scale changes preserve the disabled preference");
        hud.set_clock_visible(true);
        expect(static_cast<const client::GameHud&>(hud).panel(client::HudPanelId::Clock).visible(),
               "settings can re-enable the clock");
        hud.set_scale(1);
        hud.reset_defaults(1000, 700);
        hud.set_panel_pos(client::HudPanelId::Clock, 700, 480);
        protocol::ClockSyncFields many{1700000000, 3600, 123, {}};
        for (int event = 0; event < 16; ++event) {
            many.events.push_back({"event_" + std::to_string(event),
                                   "Event " + std::to_string(event),
                                   1700003600,
                                   1700608400});
        }
        clock.synchronize(many);
        input.mouse_x = 805;
        input.mouse_y = 559;
        (void) hud.handle_pointer(input, 1000, 700);
        input.mouse_x = 870;
        input.mouse_y = 460;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::Handled &&
                   hud.contains(720, 320),
               "long event list supports paging without world clicks");
        input.mouse_x = 720;
        input.mouse_y = 320;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::Handled &&
                   !hud.contains(720, 320),
               "choosing a later-page event dismisses the selector");
        input.mouse_x = 805;
        input.mouse_y = 559;
        (void) hud.handle_pointer(input, 1000, 700);
        clock.synchronize({1700000000, 3600, 123, {}});
        expect(hud.contains(720, 460) && !hud.contains(720, 400),
               "empty authoritative event catalog shrinks selector to No event");
        input.mouse_x = 720;
        input.mouse_y = 460;
        expect(hud.handle_pointer(input, 1000, 700) == client::HudPointerResult::Handled &&
                   !hud.contains(720, 460),
               "No event remains selectable after catalog replacement");
    }
}  // namespace

int main(int argc, char** argv) {
    protocol_and_clock();
    schedule();
    hud_and_capture(argc, argv);
    return openkal::test::g_failures ? 1 : 0;
}
