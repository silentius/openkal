/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MailWindow.h"
#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;
    constexpr float kWidth = 1000, kHeight = 760;

    std::vector<openkal::protocol::MailEntry> inbox_entries() {
        std::vector<openkal::protocol::MailEntry> entries;
        for (std::uint32_t id = 1; id <= 20; ++id) {
            openkal::protocol::MailEntry e;
            e.id = id;
            e.created_at = 1791720000 + id * 60;
            e.item_index = 237;
            e.item_count = 3;
            e.subject =
                id == 20 ? "Level Up Reward" : "A gift for your journey " + std::to_string(id);
            e.body = "Congratulations on reaching a new level!\n\nYour reward is enclosed. "
                     "Make room in your inventory before receiving it.\n\n"
                     "Continue your journey through the world of Kal and visit the village "
                     "to prepare for your next adventure. This letter remains available "
                     "until you choose to delete it after receiving the attachment.\n\n"
                     "Unread letters stay marked until you open them. You can use the "
                     "buttons or mouse wheel to read longer messages.\n\nGood luck!";
            entries.push_back(std::move(e));
        }
        return entries;
    }

    MailClickResult
    click(MailWindow& window, const UiDocument& doc, const char* id, float scale = 1) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "inbox control exists in editable document");
        if (!widget) {
            return MailClickResult::Ignored;
        }
        const float x = (kWidth - doc.window_w()) * 0.5f * scale;
        const float y = std::max(84.0f, (kHeight - doc.window_h()) * 0.4f) * scale;
        float wx = 0, wy = 0, ww = 0, wh = 0;
        UiView::widget_rect(*widget,
                            nullptr,
                            x,
                            y,
                            doc.window_w() * scale,
                            doc.window_h() * scale,
                            scale,
                            wx,
                            wy,
                            ww,
                            wh);
        return window.handle_click(wx + ww * 0.5f, wy + wh * 0.5f);
    }

    std::vector<std::uint8_t> write_capture(openkal::engine::IEngine& engine,
                                            const std::string& path,
                                            int width,
                                            int height) {
        std::vector<std::uint8_t> rgba;
        expect(engine.end_offscreen_rgba(rgba), "read Dragon inbox capture");
        if (rgba.size() != static_cast<std::size_t>(width * height * 4)) {
            return {};
        }
        std::ofstream out(path, std::ios::binary);
        out << "P6\n" << width << ' ' << height << "\n255\n";
        for (std::size_t i = 0; i < rgba.size(); i += 4) {
            out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
        }
        expect(out.good(), "save Dragon inbox preview");
        return rgba;
    }

    std::vector<std::uint8_t> widget_pixels(const std::vector<std::uint8_t>& rgba,
                                            const UiDocument& doc,
                                            const char* id = "attachment") {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "captured widget exists");
        if (!widget || rgba.size() != static_cast<std::size_t>(kWidth * kHeight * 4)) {
            return {};
        }
        float x = 0, y = 0, w = 0, h = 0;
        UiView::widget_rect(*widget,
                            nullptr,
                            (kWidth - doc.window_w()) * 0.5f,
                            std::max(84.0f, (kHeight - doc.window_h()) * 0.4f),
                            doc.window_w(),
                            doc.window_h(),
                            1,
                            x,
                            y,
                            w,
                            h);
        std::vector<std::uint8_t> pixels;
        for (int row = static_cast<int>(y); row < static_cast<int>(y + h); ++row) {
            for (int col = static_cast<int>(x); col < static_cast<int>(x + w); ++col) {
                const auto offset = static_cast<std::size_t>((row * kWidth + col) * 4);
                pixels.insert(pixels.end(), rgba.begin() + offset, rgba.begin() + offset + 4);
            }
        }
        return pixels;
    }

    void capture(const std::string& path, const UiLayout& layout, const UiDocument& doc) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Dragon inbox verification", kWidth, kHeight)) {
            expect(false, "initialize inbox OpenGL capture");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans(), 13),
               "load inbox fallback face");
        MailWindow window;
        window.set_layout(&layout);
        window.center_on_screen(kWidth, kHeight);
        window.set_entries(inbox_entries());
        window.set_attachment_info("Medicine", "medicineSmall.bmp");
        for (const auto& widget : doc.widgets()) {
            const auto& file = widget.file().empty() ? widget.file_default() : widget.file();
            if (!file.empty()) {
                expect(ui.tex_info(file).width() > 0, "Dragon inbox artwork loads");
            }
        }
        window.set_open(true);
        const auto indicator_capture = [&](std::uint16_t item_index,
                                           std::uint32_t item_count,
                                           bool claimed,
                                           bool read,
                                           const char* suffix) {
            auto entries = inbox_entries();
            entries.back().item_index = item_index;
            entries.back().item_count = item_count;
            entries.back().claimed = claimed;
            entries.back().read = read;
            window.set_entries(std::move(entries));
            expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
                   "begin paperclip capture");
            window.draw(*engine, ui, font);
            return widget_pixels(
                write_capture(*engine, path + suffix, kWidth, kHeight), doc, "row_attachment_0");
        };
        const auto absent = indicator_capture(0, 0, false, false, ".no-attachment.ppm");
        const auto attached = indicator_capture(237, 3, false, false, ".attached.ppm");
        expect(!attached.empty() && attached != absent,
               "attachment adds a visible paperclip to the inbox row");
        expect(indicator_capture(237, 3, false, true, ".read.ppm") == attached,
               "reading mail preserves its paperclip");
        expect(indicator_capture(237, 3, true, true, ".claimed.ppm") == attached,
               "receiving the attachment preserves its paperclip");
        expect(indicator_capture(0, 3, false, false, ".no-item.ppm") == absent,
               "missing item ID hides the paperclip");
        expect(indicator_capture(237, 0, false, false, ".no-quantity.ppm") == absent,
               "zero quantity hides the paperclip");
        window.set_entries(inbox_entries());
        window.set_open(false);
        expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
               "begin inbox capture");
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 20, "hidden inbox does not consume unread messages");
        window.set_open(true);
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 20, "opening list leaves selection unread");
        expect(click(window, doc, "read") == MailClickResult::Read, "explicit read action");
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 19 && window.take_read_dirty(),
               "displayed message becomes read");
        const auto available = widget_pixels(write_capture(*engine, path, kWidth, kHeight), doc);
        expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
               "begin confirmation capture");
        (void) click(window, doc, "claim");
        window.draw(*engine, ui, font);
        write_capture(*engine, path + ".confirm.ppm", kWidth, kHeight);
        expect(click(window, doc, "confirm") == MailClickResult::Claim,
               "confirm captured attachment claim");
        window.request_sent(MailClickResult::Claim, true);
        window.claim_result({window.selected_id(), false});
        expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
               "begin failed claim capture");
        window.draw(*engine, ui, font);
        expect(widget_pixels(write_capture(*engine, path + ".failed.ppm", kWidth, kHeight), doc) ==
                   available,
               "failed claim keeps the attachment in color");
        (void) click(window, doc, "claim");
        expect(click(window, doc, "confirm") == MailClickResult::Claim,
               "retry captured attachment claim");
        window.request_sent(MailClickResult::Claim, true);
        window.claim_result({window.selected_id(), true});
        auto claimed_entries = inbox_entries();
        claimed_entries.back().claimed = true;
        claimed_entries.back().read = true;
        window.set_entries(std::move(claimed_entries));
        expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
               "begin received attachment capture");
        window.draw(*engine, ui, font);
        const auto received =
            widget_pixels(write_capture(*engine, path + ".received.ppm", kWidth, kHeight), doc);
        std::size_t grayscale_changes = 0;
        if (available.size() == received.size()) {
            for (std::size_t i = 0; i + 3 < received.size(); i += 4) {
                if (available[i] != received[i] || available[i + 1] != received[i + 1] ||
                    available[i + 2] != received[i + 2]) {
                    if (received[i] == received[i + 1] && received[i] == received[i + 2]) {
                        ++grayscale_changes;
                    }
                }
            }
        }
        expect(grayscale_changes > 50, "received attachment is visibly rendered in grayscale");
        (void) click(window, doc, "row_1");
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 19, "single row selection does not mark it read");
        expect(click(window, doc, "row_1") == MailClickResult::Read,
               "double-click opens selected mail");
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 18 && window.take_read_dirty(),
               "second displayed message becomes read");
        expect(engine->begin_offscreen(kWidth, kHeight, 0.08f, 0.09f, 0.09f),
               "begin available attachment capture");
        window.draw(*engine, ui, font);
        expect(widget_pixels(write_capture(*engine, path + ".available.ppm", kWidth, kHeight),
                             doc) == available,
               "another mail using the same icon retains its color");
        window.set_entries(inbox_entries());
        window.draw(*engine, ui, font);
        expect(window.unread_count() == 18 && !window.take_read_dirty(),
               "refresh keeps read state");
        window.set_scale(1.5f);
        window.center_on_screen(kWidth * 1.5f, kHeight * 1.5f);
        expect(engine->begin_offscreen(kWidth * 1.5f, kHeight * 1.5f, 0.08f, 0.09f, 0.09f),
               "begin scaled inbox capture");
        window.draw(*engine, ui, font);
        write_capture(*engine, path + ".150.ppm", kWidth * 1.5f, kHeight * 1.5f);
        window.destroy_fonts(*engine);
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    const auto& layout = layouts.get("mail");
    expect(layout.loaded(), "load standard Dragon inbox layout");
    UiDocument doc;
    UiView::reload(doc, &layout);
    expect(doc.chrome_preset() == "dragon_dialog", "shared Dragon chrome");
    UiVarMap preview;
    ui_fill_dummy_preview(preview);
    for (int row = 0; row < 8; ++row) {
        const auto* indicator = doc.find_widget("row_attachment_" + std::to_string(row));
        expect(indicator && indicator->file() == "dragon_dialog/mail_attachment.bmp",
               "each inbox row uses the shared paperclip image");
        expect(indicator && ui_eval_condition(indicator->visible_when(), preview) == (row < 2),
               "editor preview distinguishes attached, plain and unused rows");
    }
    {
        MailWindow closed_inbox;
        auto delivered = inbox_entries();
        const auto newest = delivered.back();
        delivered.pop_back();
        closed_inbox.set_entries(delivered);
        expect(!closed_inbox.open() && closed_inbox.unread_count() == 19,
               "initial inbox updates the HUD count while the dialog is closed");
        delivered.push_back(newest);
        closed_inbox.set_entries(std::move(delivered));
        expect(!closed_inbox.open() && closed_inbox.unread_count() == 20,
               "incoming mail updates the HUD count without opening the dialog");
    }
    for (float scale : {0.75f, 1.0f, 1.5f}) {
        MailWindow window;
        window.set_layout(&layout);
        window.set_scale(scale);
        window.center_on_screen(kWidth * scale, kHeight * scale);
        window.set_entries(inbox_entries());
        window.set_open(true);
        expect(window.unread_count() == 20 && window.selected_id() == 20,
               "newest-first unread inbox");
        expect(click(window, doc, "next", scale) == MailClickResult::Handled, "next inbox page");
        expect(click(window, doc, "row_0", scale) == MailClickResult::Handled &&
                   window.selected_id() == 12,
               "page two selects the ninth message");
        expect(click(window, doc, "read", scale) == MailClickResult::Read,
               "read current selection");
        const auto id = window.selected_id();
        (void) click(window, doc, "sort_subject", scale);
        expect(window.selected_id() == id, "sorting preserves the selected durable ID");
        expect(click(window, doc, "delete", scale) == MailClickResult::Handled,
               "unclaimed mail cannot be deleted");
        (void) click(window, doc, "claim", scale);
        const auto selected = window.selected_id();
        (void) click(window, doc, "row_0", scale);
        expect(window.selected_id() == selected, "confirmation blocks underlying selection");
        (void) click(window, doc, "cancel", scale);
        (void) click(window, doc, "claim", scale);
        expect(click(window, doc, "confirm", scale) == MailClickResult::Claim,
               "claim requires confirmation");
        window.request_sent(MailClickResult::Claim, true);
        expect(click(window, doc, "read", scale) == MailClickResult::Handled,
               "pending request blocks repeat mutations");
        window.claim_result({selected, true});
        auto entries = inbox_entries();
        for (auto& e : entries) {
            if (e.id == selected) {
                e.claimed = true;
                e.read = true;
            }
        }
        window.set_entries(entries);
        expect(window.unread_count() == 19, "server read/claim state updates unread count");
        (void) click(window, doc, "delete", scale);
        expect(click(window, doc, "confirm", scale) == MailClickResult::Delete,
               "claimed mail can be deleted after confirmation");
        window.request_sent(MailClickResult::Delete, true);
        entries.erase(std::remove_if(entries.begin(),
                                     entries.end(),
                                     [selected](const auto& e) { return e.id == selected; }),
                      entries.end());
        window.set_entries(entries);
        expect(window.selected_id() != selected, "deleted selection is cleared on server snapshot");
        auto message = inbox_entries().front();
        message.id = 9000;
        message.item_index = 0;
        message.item_count = 0;
        message.claimed = false;
        message.read = false;
        window.set_entries({message});
        (void) click(window, doc, "row_0", scale);
        (void) click(window, doc, "read", scale);
        expect(click(window, doc, "claim", scale) == MailClickResult::Handled,
               "text-only mail has no claim action");
        (void) click(window, doc, "delete", scale);
        expect(click(window, doc, "confirm", scale) == MailClickResult::Delete,
               "text-only mail can be deleted without claiming an item");
        window.set_entries(entries);

        expect(click(window, doc, "refresh", scale) == MailClickResult::Refresh,
               "refresh requests an authoritative snapshot");
        window.request_sent(MailClickResult::Refresh, true);
        window.set_entries(entries);
        expect(click(window, doc, "refresh", scale) == MailClickResult::Handled,
               "refresh observes three-second cooldown");
        expect(click(window, doc, "close", scale) == MailClickResult::Close && !window.open(),
               "Dragon footer closes inbox");
    }
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(argv[2], layout, doc);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
