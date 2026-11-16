/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/QuestJournalWindow.h"

#include "openkal/client/Inventory.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/TextWrap.h"

#include <algorithm>
#include <string_view>

namespace openkal::client {
    namespace {
        constexpr float kListFraction = 0.4f;
        constexpr float kBodyPad = 12.0f;
        constexpr float kBodyLineStep = 18.0f;
        constexpr float kRowLineStep = 20.0f;
        constexpr float kRowPad = 8.0f;
    }  // namespace

    class QuestJournalWindow::ViewHost final : public UiViewHost {
      public:
        ViewHost(const QuestJournalWindow& window, const Bag& bag) : m_window(window), m_bag(bag) {}

        void draw_canvas(engine::IEngine& engine,
                         WidgetTools&,
                         const UiWidget& widget,
                         float x,
                         float y,
                         float width,
                         float height,
                         float scale,
                         const UiVarMap&,
                         const FreetypeFont& font) const override {
            if (widget.canvas_kind() == "quest_journal") {
                m_window.draw_content(engine, font, m_bag, x, y, width, height, scale);
            }
        }

        bool button_enabled(std::string_view action) const override {
            return action != "quest.snitch" || m_window.m_selected >= 0;
        }

        bool button_hovered(std::string_view id) const override {
            const float s = std::max(0.25f, m_window.m_scale);
            const UiHit hit = UiView::hit(m_window.m_document,
                                          nullptr,
                                          m_window.m_mouse_x,
                                          m_window.m_mouse_y,
                                          m_window.m_x,
                                          m_window.m_y,
                                          m_window.m_w * s,
                                          m_window.m_h * s,
                                          s,
                                          m_window.variables());
            return hit.widget_id() == id;
        }

        bool button_pressed(std::string_view id) const override {
            return m_window.m_mouse_down && button_hovered(id);
        }

      private:
        const QuestJournalWindow& m_window;
        const Bag& m_bag;
    };

    class QuestJournalWindow::SnitchHost final : public UiViewHost {
      public:
        explicit SnitchHost(const QuestJournalWindow& window) : m_window(window) {
            const float scale = std::max(.25f, window.m_scale);
            const auto hit = UiView::hit(window.m_snitch_document,
                                         nullptr,
                                         window.m_mouse_x,
                                         window.m_mouse_y,
                                         window.m_snitch_x,
                                         window.m_snitch_y,
                                         window.m_snitch_document.window_w() * scale,
                                         window.m_snitch_document.window_h() * scale,
                                         scale,
                                         window.snitch_variables());
            set_pointer_hit(std::string(hit.widget_id()), window.m_mouse_down);
        }
        void draw_canvas(engine::IEngine& engine,
                         WidgetTools& ui,
                         const UiWidget& widget,
                         float x,
                         float y,
                         float w,
                         float h,
                         float scale,
                         const UiVarMap&,
                         const FreetypeFont& font) const override {
            if (widget.bind() != "snitch.map" || m_window.m_locations.empty()) {
                return;
            }
            const auto& location = m_window.m_locations[m_window.m_location];
            m_window.m_snitch_map.draw(
                engine, ui, font, x, y, w, h, scale, {location.spawn}, 0, 1.25f);
        }

      private:
        const QuestJournalWindow& m_window;
    };

    void QuestJournalWindow::set_snitch_context(const UiLayout* layout,
                                                const std::string& root,
                                                const std::vector<NpcSpawn>& npcs) {
        m_snitch_layout = layout;
        UiView::reload(m_snitch_document, layout);
        m_npcs = &npcs;
        if (root != m_configuration_root) {
            m_configuration_root = root;
            m_snitch_loaded = false;
            m_snitch_quest = -1;
        }
    }

    bool QuestJournalWindow::close_snitch() {
        const bool was_open = m_snitch_open;
        m_snitch_open = false;
        m_snitch_dragging = false;
        return was_open;
    }

    void QuestJournalWindow::sync_snitch(const Bag& bag) {
        if (!m_snitch_open) {
            return;
        }
        UiView::reload(m_snitch_document, m_snitch_layout);
        if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) {
            close_snitch();
            return;
        }
        if (!m_snitch_loaded) {
            (void) m_snitch_catalog.load(m_configuration_root);
            m_snitch_map.load(m_configuration_root);
            m_snitch_loaded = true;
        }
        const auto& row = m_rows[m_selected];
        const int have = bag.count_item(row.item_index);
        if (m_snitch_quest == row.quest && m_snitch_flag == row.flag && m_snitch_have == have) {
            return;
        }
        m_snitch_quest = row.quest;
        m_snitch_flag = row.flag;
        m_snitch_have = have;
        m_location = 0;
        static const std::vector<NpcSpawn> empty_npcs;
        m_locations = quest_locations(
            row.quest, row.stage, have, m_snitch_catalog.monsters(), m_npcs ? *m_npcs : empty_npcs);
    }

    UiVarMap QuestJournalWindow::snitch_variables() const {
        UiVarMap vars{{"snitch.title", "Snitch"},
                      {"snitch.previous", "<"},
                      {"snitch.next", ">"},
                      {"snitch.multiple", m_locations.size() > 1 ? "1" : "0"},
                      {"snitch.target", "No known location for this objective."},
                      {"snitch.position", ""},
                      {"snitch.page", ""},
                      {"snitch.legend", "Yellow: NPC / monster spawn"}};
        if (!m_locations.empty()) {
            const auto& location = m_locations[m_location];
            const auto& spawn = location.spawn;
            vars["snitch.target"] = (location.npc ? "NPC: " : "Monster: ") + location.name;
            vars["snitch.position"] = "Map " + std::to_string(spawn.map) + " / X " +
                                      std::to_string(spawn.x0 + (spawn.x1 - spawn.x0) / 2) +
                                      " / Y " +
                                      std::to_string(spawn.y0 + (spawn.y1 - spawn.y0) / 2);
            vars["snitch.page"] =
                std::to_string(m_location + 1) + " / " + std::to_string(m_locations.size());
        }
        vars["snitch.target_more"] = "";
        if (m_body_font.ready()) {
            const auto lines = wrap_text(m_body_font,
                                         vars["snitch.target"],
                                         std::max(1.0f, m_snitch_document.window_w() - 108),
                                         1.0f);
            if (!lines.empty()) {
                vars["snitch.target"] = lines[0];
            }
            if (lines.size() > 1) {
                vars["snitch.target_more"] = lines[1];
            }
        }
        ui_fill_theme_vars(m_snitch_document, vars);
        return vars;
    }

    bool QuestJournalWindow::snitch_contains(float mx, float my) const {
        const float s = std::max(.25f, m_scale);
        return m_open && m_snitch_open &&
               WidgetTools::window_outer_hit(mx,
                                             my,
                                             m_snitch_x,
                                             m_snitch_y,
                                             m_snitch_document.window_w() * s,
                                             m_snitch_document.window_h() * s,
                                             s,
                                             UiView::frame_style(m_snitch_document));
    }

    void QuestJournalWindow::draw_snitch(engine::IEngine& engine,
                                         WidgetTools& ui,
                                         const FreetypeFont& body,
                                         const FreetypeFont& title) const {
        if (!m_snitch_open) {
            return;
        }
        const float s = std::max(.25f, m_scale);
        const SnitchHost host(*this);
        UiView::draw(engine,
                     ui,
                     m_snitch_document,
                     m_snitch_x,
                     m_snitch_y,
                     m_snitch_document.window_w() * s,
                     m_snitch_document.window_h() * s,
                     s,
                     snitch_variables(),
                     body,
                     title,
                     &host);
    }

    void QuestJournalWindow::set_layout(const UiLayout* layout) {
        m_layout = layout;
        sync_from_layout();
    }

    void QuestJournalWindow::sync_from_layout() const {
        UiView::reload(m_document, m_layout);
        if (m_document.window_w() > 0 && m_document.window_h() > 0) {
            m_w = m_document.window_w();
            m_h = m_document.window_h();
        }
    }

    UiVarMap QuestJournalWindow::variables() const {
        UiVarMap vars{{"quest.title", "Quest Journal"},
                      {"quest.active_label", "Active quests"},
                      {"quest.details_label", "Quest details"},
                      {"quest.count", std::to_string(m_rows.size())},
                      {"quest.snitch_label", "Snitch"}};
        ui_fill_theme_vars(m_document, vars);
        return vars;
    }

    WindowFrameStyle QuestJournalWindow::frame_style() const {
        return UiView::frame_style(m_document);
    }

    void QuestJournalWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        m_screen_w = static_cast<int>(screen_w);
        m_screen_h = static_cast<int>(screen_h);
        const float s = std::max(0.25f, m_scale);
        m_x = std::max(8.0f + m_document.pad_left() * s, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f + m_document.pad_top() * s, (screen_h - m_h * s) * 0.3f);
    }

    void QuestJournalWindow::sync_quests(const std::vector<protocol::QuestFlagEntry>& entries,
                                         const TaskCatalog& tasks,
                                         const ItemDb& items) {
        const std::uint16_t keep_quest =
            (m_selected >= 0 && m_selected < static_cast<int>(m_rows.size()))
                ? m_rows[static_cast<std::size_t>(m_selected)].quest
                : 0;
        m_snitch_quest = -1;
        m_rows.clear();
        m_row_y.clear();
        m_row_h.clear();
        for (const auto& e : entries) {
            if (e.clear != 0) {
                continue;
            }
            Row row;
            row.quest = e.quest;
            row.flag = e.flag;
            if (const TaskDef* task = tasks.find(e.quest)) {
                row.title = task->name;
                row.desc = task->desc;
                for (const auto& f : task->flags) {
                    if (f.key != e.flag) {
                        continue;
                    }
                    row.stage = f;
                    row.flag_desc = f.desc;
                    row.npc_desc = f.npc_desc;
                    row.level_required = f.level;
                    row.item_index = f.item_index;
                    row.item_need = f.item_count;
                    if (f.item_index > 0) {
                        if (const ItemDef* item = items.find(f.item_index)) {
                            row.item_name = item->name();
                        } else {
                            row.item_name = "Item #" + std::to_string(f.item_index);
                        }
                    }
                    break;
                }
            } else {
                row.title = "Quest #" + std::to_string(e.quest);
                row.desc = "Quest details are not available yet.";
            }
            m_rows.push_back(std::move(row));
        }
        m_selected = -1;
        if (keep_quest != 0) {
            for (std::size_t i = 0; i < m_rows.size(); ++i) {
                if (m_rows[i].quest == keep_quest) {
                    m_selected = static_cast<int>(i);
                    break;
                }
            }
        }
        if (m_selected < 0 && !m_rows.empty()) {
            m_selected = 0;
        }
        if (m_rows.empty()) {
            close_snitch();
        }
    }

    void QuestJournalWindow::sync_fonts(engine::IEngine& engine) {
        sync_from_layout();
        if (!m_title_font.ready()) {
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), 18);
        }
        const int size = std::max(1, m_document.font_pixel_size());
        if (!m_body_font.ready() || m_body_font.pixel_size() != size) {
            m_body_font.destroy(engine);
            (void) m_body_font.load(engine, FreetypeFont::find_system_sans(), size);
        }
    }

    void QuestJournalWindow::destroy_fonts(engine::IEngine& engine) {
        m_title_font.destroy(engine);
        m_body_font.destroy(engine);
    }

    bool QuestJournalWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        if (snitch_contains(mx, my)) {
            return true;
        }
        const float s = std::max(0.25f, m_scale);
        sync_from_layout();
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w * s, m_h * s, s, frame_style());
    }

    void QuestJournalWindow::update_pointer(const engine::FrameInput& in) {
        if (in.width > 0 && in.height > 0) {
            m_screen_w = in.width;
            m_screen_h = in.height;
        }
        if (!in.mouse_left) {
            m_snitch_dragging = false;
        }
        if (m_snitch_dragging) {
            m_snitch_x = in.mouse_x - m_snitch_drag_dx;
            m_snitch_y = in.mouse_y - m_snitch_drag_dy;
        }
        m_mouse_x = in.mouse_x;
        m_mouse_y = in.mouse_y;
        m_mouse_down = in.mouse_left;
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (!in.mouse_left) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            m_x = in.mouse_x - m_drag_dx;
            m_y = in.mouse_y - m_drag_dy;
        }
    }

    bool QuestJournalWindow::handle_click(float mx, float my) {
        if (!m_open || !contains(mx, my)) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        const float width = m_w * s;
        const float height = m_h * s;
        if (snitch_contains(mx, my)) {
            const auto hit = UiView::hit(m_snitch_document,
                                         nullptr,
                                         mx,
                                         my,
                                         m_snitch_x,
                                         m_snitch_y,
                                         m_snitch_document.window_w() * s,
                                         m_snitch_document.window_h() * s,
                                         s,
                                         snitch_variables());
            if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
                close_snitch();
            } else if (hit.kind() == UiHitKind::TitleDrag) {
                m_snitch_dragging = true;
                m_snitch_drag_dx = mx - m_snitch_x;
                m_snitch_drag_dy = my - m_snitch_y;
            } else if (m_locations.size() > 1) {
                const int count = static_cast<int>(m_locations.size());
                if (hit.action() == "snitch.next") {
                    m_location = (m_location + 1) % count;
                }
                if (hit.action() == "snitch.previous") {
                    m_location = (m_location + count - 1) % count;
                }
            }
            return true;
        }
        const UiHit hit =
            UiView::hit(m_document, nullptr, mx, my, m_x, m_y, width, height, s, variables());
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            set_open(false);
            return true;
        }
        if (hit.action() == "quest.snitch" && m_selected >= 0 && m_snitch_layout) {
            m_snitch_open = true;
            UiView::reload(m_snitch_document, m_snitch_layout);
            const float right =
                m_screen_w - (m_snitch_document.window_w() + m_snitch_document.pad_right() + 8) * s;
            m_snitch_x = std::max((m_snitch_document.pad_left() + 8) * s,
                                  std::min(m_x + m_w * s + 48 * s, right));
            m_snitch_y = std::max((m_snitch_document.pad_top() + 8) * s,
                                  std::min(m_y,
                                           m_screen_h - (m_snitch_document.window_h() +
                                                         m_snitch_document.pad_bottom() + 8) *
                                                            s));
            return true;
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return true;
        }
        if (hit.kind() == UiHitKind::Canvas && hit.widget_id() == "journal") {
            const UiWidget* canvas = m_document.find_widget("journal");
            float x = 0, y = 0, w = 0, h = 0;
            UiView::widget_rect(*canvas, nullptr, m_x, m_y, width, height, s, x, y, w, h);
            if (mx >= x + kRowPad * s && mx <= x + w * kListFraction - kRowPad * s) {
                for (std::size_t i = 0; i < m_row_y.size(); ++i) {
                    const float top = y + m_row_y[i] * s;
                    const float row_h = m_row_h[i] * s;
                    if (my >= top && my < top + row_h) {
                        m_selected = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
        return true;
    }

    void QuestJournalWindow::draw(engine::IEngine& engine,
                                  WidgetTools& ui,
                                  const FreetypeFont& font,
                                  const Bag& bag) {
        if (!m_open) {
            return;
        }
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        const FreetypeFont& body = m_body_font.ready() ? m_body_font : font;
        const FreetypeFont& title = m_title_font.ready() ? m_title_font : body;
        sync_snitch(bag);
        const ViewHost host(*this, bag);
        UiView::draw(
            engine, ui, m_document, m_x, m_y, m_w * s, m_h * s, s, variables(), body, title, &host);
        draw_snitch(engine, ui, body, title);
    }

    void QuestJournalWindow::draw_content(engine::IEngine& engine,
                                          const FreetypeFont& font,
                                          const Bag& bag,
                                          float x,
                                          float y,
                                          float width,
                                          float height,
                                          float s) const {
        const float text_scale = s;
        const float split_x = width * kListFraction;
        engine.draw_ui_rect(x + split_x, y, s, height, 0.36f, 0.29f, 0.19f, 0.8f);
        const float list_top = y + kBodyPad * s;
        const float list_left = x + kRowPad * s;
        const float list_w = split_x - kRowPad * 2.0f * s;
        const float list_bottom = y + height - kBodyPad * s;
        m_row_y.clear();
        m_row_h.clear();
        float list_y = list_top;
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            std::vector<std::string> title_lines;
            if (font.ready()) {
                title_lines = wrap_text(font, row.title, list_w - 4.0f * s, text_scale);
            }
            if (title_lines.empty()) {
                title_lines.emplace_back(row.title);
            }
            const float row_h = std::max(kRowLineStep * s,
                                         kRowLineStep * s * static_cast<float>(title_lines.size()));
            if (list_y + row_h > list_bottom) {
                break;
            }
            m_row_y.push_back((list_y - y) / s);
            m_row_h.push_back(row_h / s);
            const int have = bag.count_item(row.item_index);
            const bool complete = item_requirement_met(have, row.item_need);
            if (static_cast<int>(i) == m_selected) {
                engine.draw_ui_rect(list_left, list_y, list_w, row_h, 0.32f, 0.25f, 0.15f, 0.9f);
            }
            if (font.ready()) {
                const float r = complete ? 0.71f : 0.92f;
                const float g = complete ? 0.85f : 0.89f;
                const float b = complete ? 0.57f : 0.83f;
                float ty = list_y + 1.0f * s;
                for (const auto& line : title_lines) {
                    font.draw(engine, list_left + 4.0f * s, ty, line, r, g, b, text_scale);
                    ty += kRowLineStep * s;
                }
            }
            list_y += row_h;
        }

        const float right_left = x + split_x + kRowPad * s;
        const float right_w = width - split_x - kRowPad * s * 2.0f;
        const float right_bottom = y + height - (kBodyPad + 40.0f) * s;
        if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) {
            if (m_rows.empty() && font.ready()) {
                font.draw(engine,
                          right_left,
                          y + kBodyPad * s,
                          "No active quests.",
                          0.75f,
                          0.75f,
                          0.7f,
                          text_scale);
            }
            return;
        }

        const Row& row = m_rows[static_cast<std::size_t>(m_selected)];
        const int have = bag.count_item(row.item_index);
        const bool show_item = row.item_need > 0 && !row.item_name.empty();
        std::vector<std::string> item_lines;
        if (show_item && font.ready()) {
            item_lines = wrap_text(font,
                                   format_item_progress(row.item_name, have, row.item_need),
                                   right_w,
                                   text_scale);
        }
        const float item_reserve =
            show_item
                ? (kBodyLineStep * static_cast<float>(std::max<std::size_t>(1, item_lines.size())) +
                   8.0f) *
                      s
                : 0.0f;
        const float text_bottom = right_bottom - item_reserve;
        float ty = y + kBodyPad * s;
        auto draw_wrapped = [&](std::string_view text, float r, float g, float b) {
            if (!font.ready() || text.empty()) {
                return;
            }
            for (const auto& line : wrap_text(font, text, right_w, text_scale)) {
                if (ty + kBodyLineStep * s > text_bottom) {
                    break;
                }
                font.draw(engine, right_left, ty, line, r, g, b, text_scale);
                ty += kBodyLineStep * s;
            }
        };

        draw_wrapped(row.title, 0.90f, 0.81f, 0.62f);
        ty += 4.0f * s;
        if (row.level_required > 0) {
            draw_wrapped(
                "Required level: " + std::to_string(row.level_required), 0.85f, 0.9f, 0.7f);
            ty += 6.0f * s;
        }
        if (!row.npc_desc.empty()) {
            draw_wrapped("Turn in: " + row.npc_desc, 0.85f, 0.9f, 0.7f);
            ty += 6.0f * s;
        }
        if (!row.desc.empty()) {
            draw_wrapped(row.desc, 0.85f, 0.85f, 0.8f);
            ty += 8.0f * s;
        }
        if (!row.flag_desc.empty()) {
            draw_wrapped(row.flag_desc, 0.9f, 0.9f, 0.85f);
            ty += 8.0f * s;
        } else {
            draw_wrapped("Stage " + std::to_string(row.flag), 0.9f, 0.9f, 0.85f);
            ty += 8.0f * s;
        }

        if (show_item && font.ready()) {
            const bool complete = item_requirement_met(have, row.item_need);
            const float r = complete ? 0.71f : 0.92f;
            const float g = complete ? 0.85f : 0.89f;
            const float b = complete ? 0.57f : 0.63f;
            float iy = right_bottom - kBodyLineStep * s * static_cast<float>(item_lines.size());
            for (const auto& part : item_lines) {
                if (iy + kBodyLineStep * s > right_bottom + 1.0f) {
                    break;
                }
                font.draw(engine, right_left, iy, part, r, g, b, text_scale);
                iy += kBodyLineStep * s;
            }
        }
    }

}  // namespace openkal::client
