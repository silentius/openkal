/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PowerbookWindow.h"

#include "openkal/client/ItemDb.h"
#include "openkal/client/UiLayout.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    namespace {
        constexpr float kWidth = 1600, kHeight = 900;
        constexpr float kClientX = 40, kClientY = 84;
        constexpr float kClientWidth = 1520, kClientHeight = 760;

        int row_index(std::string_view value, std::string_view prefix) {
            if (value.substr(0, prefix.size()) != prefix) {
                return -1;
            }
            value.remove_prefix(prefix.size());
            if (value.empty() || value.size() > 2) {
                return -1;
            }
            int index = 0;
            for (const char c : value) {
                if (c < '0' || c > '9') {
                    return -1;
                }
                index = index * 10 + c - '0';
            }
            return index;
        }

        std::string fitted(std::string text, const FreetypeFont& font, float width) {
            if (!font.ready() || font.text_width(text) <= width) {
                return text;
            }
            while (!text.empty() && font.text_width(text + "...") > width) {
                auto end = text.size() - 1;
                while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0u) == 0x80u) {
                    --end;
                }
                text.erase(end);
            }
            return text + "...";
        }

        std::vector<TipLine> wrap_tip_lines(const std::vector<TipLine>& lines,
                                            const FreetypeFont& font,
                                            float max_width) {
            std::vector<TipLine> result;
            for (const auto& line : lines) {
                TipLine current;
                float width = 0;
                for (const auto& span : line.spans()) {
                    const auto& text = span.text();
                    std::size_t start = 0;
                    while (start < text.size()) {
                        auto end = text.find(' ', start);
                        end = end == std::string::npos ? text.size() : end + 1;
                        auto part = text.substr(start, end - start);
                        if (width > 0 && width + font.text_width(part) > max_width) {
                            result.push_back(std::move(current));
                            current = TipLine{};
                            width = 0;
                        }
                        // Split an oversized word at UTF-8 boundaries, preserving span colors.
                        while (font.text_width(part) > max_width) {
                            std::size_t cut = part.size();
                            do {
                                --cut;
                                while (cut > 0 &&
                                       (static_cast<unsigned char>(part[cut]) & 0xc0u) == 0x80u) {
                                    --cut;
                                }
                            } while (cut > 0 && font.text_width(std::string_view(part).substr(
                                                    0, cut)) > max_width);
                            if (cut == 0) {
                                break;
                            }
                            current.add(part.substr(0, cut), span.r(), span.g(), span.b());
                            result.push_back(std::move(current));
                            current = TipLine{};
                            width = 0;
                            part.erase(0, cut);
                        }
                        current.add(part, span.r(), span.g(), span.b());
                        width += font.text_width(part);
                        start = end;
                    }
                }
                result.push_back(std::move(current));
            }
            return result;
        }
    }  // namespace

    class PowerbookWindow::Host final : public UiViewHost {
      public:
        explicit Host(const PowerbookWindow& book) : m_book(book) {
            set_pointer_hit(book.m_hover_widget, book.m_pointer_pressed);
        }

        bool button_enabled(std::string_view action) const override {
            if (action == "powerbook.monsters.up") {
                return m_book.m_monster_scroll > 0;
            }
            if (action == "powerbook.monsters.down") {
                return m_book.m_monster_scroll + kRows <
                       static_cast<int>(m_book.m_catalog.monsters().size());
            }
            if (action == "powerbook.drops.up") {
                return m_book.m_drop_scroll > 0;
            }
            if (action == "powerbook.drops.down") {
                return m_book.m_drop_scroll + kDropRows < m_book.drop_row_count();
            }
            return true;
        }

        bool resolve_slot(std::string_view bind, int, UiSlotIcon& out) const override {
            const auto* monster = m_book.selected();
            const int row = row_index(bind, "powerbook.drop.");
            const int index = m_book.m_drop_scroll * kDropColumns + row;
            if (!monster || !m_book.m_items || row < 0 || row >= kDropSlots || index < 0 ||
                index >= static_cast<int>(monster->drops.size())) {
                return false;
            }
            out.set_file(m_book.m_items->icon_filename(monster->drops[index].item_index));
            out.set_empty(out.file().empty());
            return true;
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
                         const FreetypeFont&) const override {
            if (widget.bind() == "powerbook.map") {
                m_book.draw_map(engine, ui, {x, y, w, h}, scale);
            }
        }

      private:
        const PowerbookWindow& m_book;
    };

    bool PowerbookWindow::Rect::contains(float px, float py) const {
        return w > 0 && h > 0 && px >= x && py >= y && px < x + w && py < y + h;
    }

    PowerbookWindow::Rect
    PowerbookWindow::widget_rect(std::string_view id, float width, float height) const {
        Rect rect;
        if (const auto* widget = m_doc.find_widget(id)) {
            const float scale = std::max(.1f, std::min(width / kWidth, height / kHeight));
            UiView::widget_rect(*widget,
                                nullptr,
                                (width - kWidth * scale) * .5f + kClientX * scale,
                                (height - kHeight * scale) * .5f + kClientY * scale,
                                kClientWidth * scale,
                                kClientHeight * scale,
                                scale,
                                rect.x,
                                rect.y,
                                rect.w,
                                rect.h);
        }
        return rect;
    }

    void PowerbookWindow::set_layout(const UiLayout* layout) {
        UiView::reload(m_doc, layout);
        m_hover_drop = -1;
        m_hover_widget.clear();
    }

    void PowerbookWindow::open(const std::string& configuration_root, const ItemDb& items) {
        if (!m_loaded) {
            (void) m_catalog.load(configuration_root);
            m_location_map.load(configuration_root);
            m_loaded = true;
            select(m_catalog.monsters().empty() ? -1 : 0);
        }
        m_items = &items;
        m_open = true;
        m_hover_drop = -1;
        m_hover_widget.clear();
        m_pointer_pressed = false;
        m_drag_scroll = 0;
    }

    void PowerbookWindow::close() {
        m_open = false;
        m_hover_drop = -1;
        m_hover_widget.clear();
        m_pointer_pressed = false;
        m_drag_scroll = 0;
    }

    bool PowerbookWindow::is_open() const {
        return m_open;
    }

    void PowerbookWindow::set_player(int clazz, int level) {
        m_player_clazz = std::clamp(clazz, 0, 3);
        m_player_level = std::max(1, level);
    }

    int PowerbookWindow::drop_row_count() const {
        const auto* monster = selected();
        return monster ? (static_cast<int>(monster->drops.size()) + kDropColumns - 1) / kDropColumns
                       : 0;
    }

    const PowerbookMonster* PowerbookWindow::selected() const {
        return m_selected >= 0 && m_selected < static_cast<int>(m_catalog.monsters().size())
                   ? &m_catalog.monsters()[m_selected]
                   : nullptr;
    }

    void PowerbookWindow::select(int index) {
        m_selected = index;
        m_drop_scroll = 0;
        m_hover_drop = -1;
        m_spawn = 0;
    }

    void PowerbookWindow::update(const engine::FrameInput& input, WidgetTools& ui) {
        if (!m_open) {
            return;
        }
        m_width = static_cast<float>(input.width);
        m_height = static_cast<float>(input.height);
        m_mouse_x = input.mouse_x;
        m_mouse_y = input.mouse_y;
        if (input.key_escape_pressed || input.key_p_pressed) {
            close();
            return;
        }
        const auto* monster = selected();
        const int monster_max = std::max(0, static_cast<int>(m_catalog.monsters().size()) - kRows);
        const int drop_max = std::max(0, drop_row_count() - kDropRows);
        const auto scroll =
            [&](std::string_view id, int& offset, int maximum, int visible_rows, int drag_id) {
                const auto track = widget_rect(id, m_width, m_height);
                const float thumb =
                    track.h * visible_rows / static_cast<float>(maximum + visible_rows);
                const float travel = track.h - thumb;
                const float top = track.y + (maximum ? travel * offset / maximum : 0);
                if (input.mouse_left_clicked && track.contains(input.mouse_x, input.mouse_y) &&
                    maximum) {
                    m_drag_scroll = drag_id;
                    m_drag_grab = input.mouse_y >= top && input.mouse_y < top + thumb
                                      ? input.mouse_y - top
                                      : thumb * .5f;
                }
                if (m_drag_scroll == drag_id && travel > 0) {
                    offset =
                        std::clamp(static_cast<int>(std::lround(
                                       (input.mouse_y - track.y - m_drag_grab) * maximum / travel)),
                                   0,
                                   maximum);
                }
            };
        scroll("monster_scroll", m_monster_scroll, monster_max, kRows, 1);
        scroll("drop_scroll", m_drop_scroll, drop_max, kDropRows, 2);
        if (!input.mouse_left) {
            m_drag_scroll = 0;
        }
        if (input.mouse_scroll != 0) {
            const int steps = static_cast<int>(std::copysign(
                std::max(1.0f, std::abs(input.mouse_scroll) * 3), input.mouse_scroll));
            if (widget_rect("monster_panel", m_width, m_height)
                    .contains(input.mouse_x, input.mouse_y)) {
                m_monster_scroll = std::clamp(m_monster_scroll - steps, 0, monster_max);
            }
            if (widget_rect("drop_panel", m_width, m_height)
                    .contains(input.mouse_x, input.mouse_y)) {
                m_drop_scroll = std::clamp(m_drop_scroll - steps, 0, drop_max);
            }
        }
        const float s = std::max(.1f, std::min(m_width / kWidth, m_height / kHeight));
        const Host host(*this);
        const auto hit = UiView::hit(m_doc,
                                     &ui,
                                     input.mouse_x,
                                     input.mouse_y,
                                     (m_width - kWidth * s) * .5f + kClientX * s,
                                     (m_height - kHeight * s) * .5f + kClientY * s,
                                     kClientWidth * s,
                                     kClientHeight * s,
                                     s,
                                     variables(),
                                     &host,
                                     false);
        m_hover_widget = hit.widget_id();
        m_pointer_pressed = input.mouse_left;
        m_hover_drop =
            hit.kind() == UiHitKind::IconSlot ? row_index(hit.bind(), "powerbook.drop.") : -1;
        if (m_drag_scroll != 0) {
            m_hover_drop = -1;
        }
        if (!input.mouse_left_clicked || !host.button_enabled(hit.action())) {
            return;
        }
        const int row = row_index(hit.action(), "powerbook.select.");
        if (row >= 0 && row < kRows &&
            m_monster_scroll + row < static_cast<int>(m_catalog.monsters().size())) {
            select(m_monster_scroll + row);
        } else if (hit.action() == "window.close") {
            close();
        } else if (hit.action() == "powerbook.monsters.up") {
            m_monster_scroll = std::max(0, m_monster_scroll - 1);
        } else if (hit.action() == "powerbook.monsters.down") {
            m_monster_scroll = std::min(monster_max, m_monster_scroll + 1);
        } else if (hit.action() == "powerbook.drops.up") {
            m_drop_scroll = std::max(0, m_drop_scroll - 1);
        } else if (hit.action() == "powerbook.drops.down") {
            m_drop_scroll = std::min(drop_max, m_drop_scroll + 1);
        } else if (monster && !monster->spawns.empty()) {
            const int count = static_cast<int>(monster->spawns.size());
            if (hit.action() == "powerbook.spawn.next") {
                m_spawn = (m_spawn + 1) % count;
            } else if (hit.action() == "powerbook.spawn.previous") {
                m_spawn = (m_spawn + count - 1) % count;
            }
        }
    }

    UiVarMap PowerbookWindow::variables() const {
        UiVarMap vars;
        ui_fill_theme_vars(m_doc, vars);
        vars["powerbook.title"] = "Powerbook";
        vars["powerbook.blank"] = "";
        vars["powerbook.monsters_label"] = "Monsters / Level";
        vars["powerbook.drops_label"] = "Possible drops";
        vars["powerbook.spawns_label"] = "Spawn areas";
        vars["powerbook.previous"] = "< Previous";
        vars["powerbook.next"] = "Next >";
        vars["powerbook.up"] = "^";
        vars["powerbook.down"] = "v";
        vars["powerbook.legend"] = "Yellow: spawn area   /   Q: requires an eligible quest";
        vars["powerbook.controls"] =
            "Scroll over either list or drag its scrollbar. Select a "
            "monster; hover an item slot for its effects and requirements.";
        const auto* monster = selected();
        vars["powerbook.selection"] =
            monster
                ? fitted(monster->name + "  /  Lv. " + std::to_string(monster->level), m_font, 1300)
                : "No monster selected";
        vars["powerbook.empty"] =
            m_catalog.monsters().empty() ? "Monster catalog unavailable." : "";
        vars["powerbook.drop_status"] =
            !monster                   ? ""
            : !monster->drops_complete ? "Drop catalog incomplete."
            : monster->drops.empty()   ? "No configured drops."
                                       : std::to_string(monster->drops.size()) + " possible items";
        vars["powerbook.spawn_visible"] = monster && !monster->spawns.empty() ? "true" : "false";
        vars["powerbook.spawn_multiple"] = monster && monster->spawns.size() > 1 ? "true" : "false";
        vars["powerbook.spawn_status"] = "No configured spawn areas.";
        if (monster && !monster->spawns.empty()) {
            const auto& spawn = monster->spawns[m_spawn];
            vars["powerbook.spawn_status"] = "Area " + std::to_string(spawn.area) + "  /  Map " +
                                             std::to_string(spawn.map) + "  /  " +
                                             std::to_string(m_spawn + 1) + " of " +
                                             std::to_string(monster->spawns.size());
        }
        vars["powerbook.monster_range"] =
            m_catalog.monsters().empty()
                ? "0 monsters"
                : std::to_string(m_monster_scroll + 1) + " - " +
                      std::to_string(std::min(m_monster_scroll + kRows,
                                              static_cast<int>(m_catalog.monsters().size()))) +
                      " / " + std::to_string(m_catalog.monsters().size());
        for (int row = 0; row < kRows; ++row) {
            const auto prefix = "powerbook.monster." + std::to_string(row);
            const int index = m_monster_scroll + row;
            const bool visible = index < static_cast<int>(m_catalog.monsters().size());
            vars[prefix + ".visible"] = visible ? "true" : "false";
            vars[prefix + ".color"] = index == m_selected ? "#4C3B22"
                                      : m_hover_widget == "monster_" + std::to_string(row)
                                          ? "#29241B"
                                      : row % 2 ? "#151410"
                                                : "#0D0D0C";
            if (visible) {
                const auto& entry = m_catalog.monsters()[index];
                vars[prefix + ".name"] = fitted(entry.name, m_font, 204);
                vars[prefix + ".level"] = std::to_string(entry.level);
            }
        }
        for (int row = 0; row < kDropSlots; ++row) {
            const auto drop = "powerbook.drop." + std::to_string(row);
            const int item = m_drop_scroll * kDropColumns + row;
            const bool have = monster && item < static_cast<int>(monster->drops.size());
            vars[drop + ".visible"] = have ? "true" : "false";
            vars[drop + ".color"] = row == m_hover_drop ? "#D9BA72" : "#554A37";
            if (have) {
                const auto& entry = monster->drops[item];
                const auto* definition = m_items ? m_items->find(entry.item_index) : nullptr;
                const std::string name = definition ? definition->name()
                                         : entry.item_index == server::kMoneyItemIndex
                                             ? "Money"
                                             : "Item #" + std::to_string(entry.item_index);
                vars[drop + ".name"] = fitted(name, m_font, 270);
                vars[drop + ".quest"] = entry.quest_only ? "Q" : "";
            }
        }
        return vars;
    }

    std::vector<TipLine> PowerbookWindow::tooltip_lines() const {
        const auto* monster = selected();
        const int index = m_drop_scroll * kDropColumns + m_hover_drop;
        if (!m_open || !monster || !m_items || m_hover_drop < 0 || m_hover_drop >= kDropSlots ||
            index < 0 || index >= static_cast<int>(monster->drops.size())) {
            return {};
        }
        const auto& drop = monster->drops[index];
        std::vector<TipLine> lines;
        if (const auto* definition = m_items->find(drop.item_index)) {
            lines = ItemTooltip::parse_markup(
                ItemTooltip::make_tip(*definition, *m_items, 1, m_player_clazz, m_player_level));
        } else {
            TipLine name;
            name.add(drop.item_index == server::kMoneyItemIndex
                         ? "Money"
                         : "Item #" + std::to_string(drop.item_index),
                     .92f,
                     .87f,
                     .76f);
            lines.push_back(std::move(name));
        }
        if (drop.quest_only) {
            TipLine quest;
            quest.add("Requires an eligible quest.", 1.0f, .84f, .38f);
            lines.push_back(std::move(quest));
        }
        const float scale = std::max(.1f, std::min(m_width / kWidth, m_height / kHeight));
        return m_tooltip_font.ready()
                   ? wrap_tip_lines(lines,
                                    m_tooltip_font,
                                    std::max(80.0f, std::min(440.0f, m_width / scale - 32)))
                   : lines;
    }

    void
    PowerbookWindow::draw(engine::IEngine& engine, WidgetTools& ui, float width, float height) {
        if (!m_open) {
            return;
        }
        if (!m_font.ready()) {
            (void) m_font.load(engine, FreetypeFont::find_system_sans(), 16);
        }
        if (!m_title_font.ready()) {
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), 26);
        }
        if (!m_tooltip_font.ready()) {
            (void) m_tooltip_font.load(engine, FreetypeFont::find_system_sans(), 14);
        }
        const float s = std::max(.1f, std::min(width / kWidth, height / kHeight));
        const float x = (width - kWidth * s) * .5f + kClientX * s;
        const float y = (height - kHeight * s) * .5f + kClientY * s;
        engine.draw_ui_rect(0, 0, width, height, .035f, .032f, .028f, 1);
        const Host host(*this);
        UiView::draw(engine,
                     ui,
                     m_doc,
                     x,
                     y,
                     kClientWidth * s,
                     kClientHeight * s,
                     s,
                     variables(),
                     m_font,
                     m_title_font,
                     &host);
        const auto bar = [&](std::string_view id, int offset, int count, int visible_rows) {
            const auto track = widget_rect(id, width, height);
            if (count <= visible_rows || track.h <= 0) {
                return;
            }
            const float h = track.h * visible_rows / count;
            engine.draw_ui_rect(track.x,
                                track.y + (track.h - h) * offset / (count - visible_rows),
                                track.w,
                                h,
                                .72f,
                                .60f,
                                .36f,
                                1);
        };
        bar("monster_scroll",
            m_monster_scroll,
            static_cast<int>(m_catalog.monsters().size()),
            kRows);
        bar("drop_scroll", m_drop_scroll, drop_row_count(), kDropRows);
        ItemTooltip::draw(
            ui, engine, m_tooltip_font, tooltip_lines(), m_mouse_x, m_mouse_y, width, height, s);
    }

    void PowerbookWindow::draw_map(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   const Rect& rect,
                                   float scale) const {
        const auto* monster = selected();
        if (!monster || monster->spawns.empty()) {
            m_font.draw(engine,
                        rect.x + 20 * scale,
                        rect.y + 20 * scale,
                        "No configured spawn areas.",
                        .90f,
                        .84f,
                        .68f,
                        scale);
            return;
        }
        m_location_map.draw(
            engine, ui, m_font, rect.x, rect.y, rect.w, rect.h, scale, monster->spawns, m_spawn);
    }

    void PowerbookWindow::destroy(engine::IEngine& engine) {
        m_font.destroy(engine);
        m_title_font.destroy(engine);
        m_tooltip_font.destroy(engine);
    }
}  // namespace openkal::client
