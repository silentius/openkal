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

#include "openkal/client/ChatWidget.h"
#include "openkal/client/DragonHud.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/MenuBarWidget.h"
#include "openkal/client/MinimapWidget.h"
#include "openkal/client/StatusWidget.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace openkal::client {
    const MovablePanel& GameHud::panel(HudPanelId id) const {
        return m_panels[static_cast<std::size_t>(id)];
    }

    MovablePanel& GameHud::panel(HudPanelId id) {
        return m_panels[static_cast<std::size_t>(id)];
    }

    void GameHud::set_panel_pos(HudPanelId id, float x, float y) {
        if (id != HudPanelId::Dragon && !(id == HudPanelId::Chat && m_chat.battle_feed())) {
            panel(id).set_pos(x, y);
            if (id == HudPanelId::Chat) {
                update_chat_width();
            }
        }
    }

    void GameHud::set_menu_open(HudMenuAction action, bool open) {
        const auto index = static_cast<std::size_t>(action);
        if (action != HudMenuAction::None && index < m_menu_data.open.size()) {
            m_menu_data.open[index] = open;
        }
    }

    void GameHud::set_unread_messages(std::size_t count) {
        m_menu_data.unread_messages = count;
    }

    void GameHud::set_trading_post_collection_pending(bool pending) {
        m_menu_data.trading_post_collection_pending = pending;
    }

    void GameHud::update_menu_pointer(const engine::FrameInput& input, bool blocked) {
        m_menu_data.mouse_x = input.mouse_x;
        m_menu_data.mouse_y = input.mouse_y;
        m_menu_data.pointer_enabled = !blocked && !is_dragging();
        m_menu_data.pressed = input.mouse_left;
    }

    void GameHud::set_clock_visible(bool visible) {
        m_clock_visible = visible;
        panel(HudPanelId::Clock).set_visible(visible);
        if (!visible) {
            m_clock_options_open = false;
            if (m_drag_panel == static_cast<int>(HudPanelId::Clock)) {
                m_drag_panel = -1;
            }
        }
    }

    void GameHud::set_scale(float s) {
        m_scale = std::max(0.25f, s);
    }

    namespace {
        [[nodiscard]] std::pair<float, float>
        hud_native_size(const UiLayoutStore* store, const char* name, float fw, float fh) {
            const auto& doc = UiView::document(store ? &store->get(name) : nullptr);
            const float w = doc.window_w() > 0.0f
                                ? doc.window_w()
                                : (doc.fallback_w() > 0.0f ? doc.fallback_w() : fw);
            const float h = doc.window_h() > 0.0f
                                ? doc.window_h()
                                : (doc.fallback_h() > 0.0f ? doc.fallback_h() : fh);
            return {w, h};
        }
        std::string clock_caption(const FreetypeFont& font, std::string text) {
            if (!font.ready()) {
                return text;
            }
            const float scale = 11.0f / std::max(1, font.pixel_size());
            if (font.text_width(text, scale) <= 150) {
                return text;
            }
            while (!text.empty() && font.text_width(text + "...", scale) > 150) {
                auto last = text.size() - 1;
                while (last > 0 && (static_cast<unsigned char>(text[last]) & 0xC0) == 0x80) {
                    --last;
                }
                text.erase(last);
            }
            return text + "...";
        }
    }  // namespace

    void GameHud::reset_defaults(float screen_w, float screen_h) {
        const float s = m_scale;
        const auto st_sz = hud_native_size(m_layouts, "status", 220.0f, 72.0f);
        panel(HudPanelId::Status) =
            MovablePanel{0, 0, st_sz.first * s, st_sz.second * s, true, true};

        auto& target = panel(HudPanelId::Target);
        target.set_size(0.0f, 0.0f);
        target.set_pos(0.0f, 0.0f);
        target.set_movable(false);
        target.set_visible(false);

        // Standalone menu strip — top-right (not tied to the bottom EXP bar).
        auto& menu = panel(HudPanelId::MenuBar);
        const auto menu_sz = hud_native_size(m_layouts, "menu_bar", 472.0f, 42.0f);
        menu.set_size(menu_sz.first * s, menu_sz.second * s);
        menu.set_pos(screen_w - menu.w() - 8 * s, 4 * s);
        menu.set_movable(true);
        menu.set_visible(true);

        auto& minimap = panel(HudPanelId::Minimap);
        const auto mm_sz = hud_native_size(m_layouts, "minimap", 213.0f, 207.0f);
        minimap.set_size(mm_sz.first * s, mm_sz.second * s);
        minimap.set_pos(screen_w - minimap.w() - 8 * s, menu.y() + menu.h() + 4 * s);
        minimap.set_movable(true);
        minimap.set_visible(true);

        auto& dragon = panel(HudPanelId::Dragon);
        const auto dragon_sz = hud_native_size(m_layouts, "dragon_hud", 792.0f, 180.0f);
        const float dragon_scale = std::min(s, screen_w / std::max(1.0f, dragon_sz.first));
        dragon.set_size(dragon_sz.first * dragon_scale, dragon_sz.second * dragon_scale);
        dragon.set_pos((screen_w - dragon.w()) * 0.5f, screen_h - dragon.h());
        dragon.set_movable(false);
        dragon.set_visible(true);

        auto& chat = panel(HudPanelId::Chat);
        const auto chat_sz = hud_native_size(m_layouts,
                                             m_chat.layout_name(),
                                             m_chat.battle_feed() ? 420.0f : 450.0f,
                                             m_chat.battle_feed() ? 132.0f : 226.0f);
        chat.set_size(m_chat.battle_feed()
                          ? std::min(chat_sz.first * s, std::max(1.0f, screen_w - 48.0f * s))
                          : chat_sz.first * s,
                      chat_sz.second * s);
        if (m_chat.battle_feed()) {
            const float margin = 24.0f * s;
            const bool fits_beside = dragon.x() >= chat.w() + 2.0f * margin;
            chat.set_pos(margin, (fits_beside ? screen_h : dragon.y()) - chat.h() - margin);
        } else {
            chat.set_pos(8 * s, dragon.y() - chat.h() - 8 * s);
        }
        update_chat_width();
        chat.set_movable(!m_chat.battle_feed());
        chat.set_visible(true);

        const auto& clock_doc = UiView::document(m_layouts ? &m_layouts->get("clock") : nullptr);
        const auto clock_size = hud_native_size(m_layouts, "clock", 188, 108);
        auto& clock = panel(HudPanelId::Clock);
        clock.set_size((clock_size.first + clock_doc.pad_left() + clock_doc.pad_right()) * s,
                       (clock_size.second + clock_doc.pad_top() + clock_doc.pad_bottom()) * s);
        clock.set_pos(screen_w - clock.w() - 12 * s, screen_h - clock.h() - 36 * s);
        clock.set_movable(true);
        clock.set_visible(m_clock_visible);
        m_clock_options_open = false;
        clamp_to_screen(screen_w, screen_h);
        m_defaults_ready = true;
        m_last_w = screen_w;
        m_last_h = screen_h;
    }

    void GameHud::update_chat_width() {
        if (m_chat.battle_feed()) {
            return;
        }
        auto& chat = panel(HudPanelId::Chat);
        const auto size = hud_native_size(m_layouts, "chat", 450.0f, 226.0f);
        const float base_width = size.first * m_scale;
        const float gap = panel(HudPanelId::Dragon).x() - chat.x() - base_width;
        chat.set_size(base_width + std::max(0.0f, gap) * 0.5f, chat.h());
    }

    bool GameHud::ensure_layout(float screen_w, float screen_h, float new_scale) {
        const bool scale_changed = std::fabs(new_scale - m_scale) > 0.001f;
        const bool size_changed = !m_defaults_ready || std::fabs(screen_w - m_last_w) > 0.5f ||
                                  std::fabs(screen_h - m_last_h) > 0.5f;
        if (!scale_changed && !size_changed) {
            return false;
        }
        // Preserve logical positions across scale changes when already laid out.
        const float inv_old = (m_scale > 0.0001f) ? (1.0f / m_scale) : 1.0f;
        float logical_x[static_cast<int>(HudPanelId::Count)];
        float logical_y[static_cast<int>(HudPanelId::Count)];
        const bool keep = m_defaults_ready && !size_changed;
        if (keep) {
            for (std::size_t i = 0; i < m_panels.size(); ++i) {
                logical_x[i] = m_panels[i].x() * inv_old;
                logical_y[i] = m_panels[i].y() * inv_old;
            }
        }
        set_scale(new_scale);
        reset_defaults(screen_w, screen_h);
        if (keep) {
            for (std::size_t i = 0; i < m_panels.size(); ++i) {
                if (static_cast<HudPanelId>(i) != HudPanelId::Dragon &&
                    !(static_cast<HudPanelId>(i) == HudPanelId::Chat && m_chat.battle_feed())) {
                    m_panels[i].set_pos(logical_x[i] * m_scale, logical_y[i] * m_scale);
                }
            }
            update_chat_width();
            clamp_to_screen(screen_w, screen_h);
        }
        return true;
    }

    void GameHud::clamp_to_screen(float screen_w, float screen_h) {
        for (auto& p : m_panels) {
            if (p.w() <= 0 || p.h() <= 0) {
                continue;
            }
            p.set_pos(std::clamp(p.x(), 0.0f, std::max(0.0f, screen_w - p.w())),
                      std::clamp(p.y(), 0.0f, std::max(0.0f, screen_h - p.h())));
        }
    }

    int GameHud::hit_panel(float mx, float my) const {
        static constexpr HudPanelId kOrder[] = {
            HudPanelId::Clock,
            HudPanelId::MenuBar,
            HudPanelId::Dragon,
            HudPanelId::Chat,
            HudPanelId::Status,
            HudPanelId::Minimap,
        };
        for (HudPanelId id : kOrder) {
            const auto& p = panel(id);
            if (!p.visible()) {
                continue;
            }
            if (WidgetTools::point_in_rect(mx, my, p.x(), p.y(), p.w(), p.h())) {
                if (id == HudPanelId::Chat &&
                    !m_chat.contains_pointer(mx, my, p.x(), p.y(), p.w(), p.h(), m_scale)) {
                    continue;
                }
                if (id == HudPanelId::Status &&
                    !StatusWidget::contains(mx,
                                            my,
                                            p.x(),
                                            p.y(),
                                            m_scale,
                                            m_layouts ? &m_layouts->get("status") : nullptr)) {
                    continue;
                }
                if (id == HudPanelId::Minimap &&
                    !MinimapWidget::contains(mx,
                                             my,
                                             p.x(),
                                             p.y(),
                                             p.w(),
                                             p.h(),
                                             m_layouts ? &m_layouts->get("minimap") : nullptr)) {
                    continue;
                }
                return static_cast<int>(id);
            }
        }
        return -1;
    }

    bool GameHud::contains(float mx, float my) const {
        return clock_options_contains(mx, my) || hit_panel(mx, my) >= 0;
    }

    HudPointerResult
    GameHud::handle_pointer(const engine::FrameInput& in, float screen_w, float screen_h) {
        if (!m_defaults_ready) {
            reset_defaults(screen_w, screen_h);
        } else if (std::fabs(screen_w - m_last_w) > 0.5f || std::fabs(screen_h - m_last_h) > 0.5f) {
            m_last_w = screen_w;
            m_last_h = screen_h;
            clamp_to_screen(screen_w, screen_h);
        }

        m_last_menu_action = HudMenuAction::None;

        if (m_drag_panel >= 0) {
            if (in.mouse_left) {
                m_panels[static_cast<std::size_t>(m_drag_panel)].set_pos(in.mouse_x - m_drag_dx,
                                                                         in.mouse_y - m_drag_dy);
                clamp_to_screen(screen_w, screen_h);
                return HudPointerResult::Handled;
            }
            m_drag_panel = -1;
        }

        if (!in.mouse_left_clicked) {
            return HudPointerResult::Ignored;
        }

        const auto& clock = panel(HudPanelId::Clock);
        const auto event_count = m_server_clock ? m_server_clock->events().size() : 0;
        m_clock_page = std::min(m_clock_page, event_count / 6);
        const UiVarMap vars = clock_vars();
        const bool dismissing_clock_options = m_clock_options_open;
        if (m_clock_options_open && clock.visible()) {
            if (clock_options_contains(in.mouse_x, in.mouse_y)) {
                const auto rect = clock_options_rect();
                const auto doc = clock_options_document();
                const auto hit = UiView::hit(doc,
                                             nullptr,
                                             in.mouse_x,
                                             in.mouse_y,
                                             rect.x(),
                                             rect.y(),
                                             rect.w(),
                                             rect.h(),
                                             m_scale,
                                             vars,
                                             nullptr,
                                             false);
                const auto& action = hit.action();
                const auto count = m_server_clock ? m_server_clock->events().size() : 0;
                if (action == "clock.previous" && m_clock_page > 0) {
                    --m_clock_page;
                } else if (action == "clock.next" && (m_clock_page + 1) * 6 < count + 1) {
                    ++m_clock_page;
                } else if (action.rfind("clock.choose.", 0) == 0 && action.size() == 14 &&
                           action.back() >= '0' && action.back() <= '5') {
                    const auto index =
                        m_clock_page * 6 + static_cast<std::size_t>(action.back() - '0');
                    if (index <= count) {
                        m_clock_event_id = index == 0 ? "" : m_server_clock->events()[index - 1].id;
                        m_clock_options_open = false;
                    }
                }
                return HudPointerResult::Handled;
            }
            m_clock_options_open = false;
            if (!WidgetTools::point_in_rect(
                    in.mouse_x, in.mouse_y, clock.x(), clock.y(), clock.w(), clock.h())) {
                return HudPointerResult::Handled;
            }
        }
        if (clock.visible() &&
            WidgetTools::point_in_rect(
                in.mouse_x, in.mouse_y, clock.x(), clock.y(), clock.w(), clock.h())) {
            const auto& doc = UiView::document(m_layouts ? &m_layouts->get("clock") : nullptr);
            const auto hit = UiView::hit(doc,
                                         nullptr,
                                         in.mouse_x,
                                         in.mouse_y,
                                         clock.x() + doc.pad_left() * m_scale,
                                         clock.y() + doc.pad_top() * m_scale,
                                         clock.w() - (doc.pad_left() + doc.pad_right()) * m_scale,
                                         clock.h() - (doc.pad_top() + doc.pad_bottom()) * m_scale,
                                         m_scale,
                                         vars);
            if (hit.action() == "window.close" || hit.kind() == UiHitKind::Close) {
                set_clock_visible(false);
                return HudPointerResult::ClockClosed;
            }
            if (hit.action() == "clock.events") {
                m_clock_options_open = !dismissing_clock_options;
                m_clock_page = 0;
                return HudPointerResult::Handled;
            }
            if (in.mouse_y >= clock.y() + doc.title_drag_h() * m_scale) {
                return HudPointerResult::Handled;
            }
        }
        const auto& menu = panel(HudPanelId::MenuBar);
        if (menu.visible() && WidgetTools::point_in_rect(
                                  in.mouse_x, in.mouse_y, menu.x(), menu.y(), menu.w(), menu.h())) {
            const UiLayout* layout = m_layouts ? &m_layouts->get("menu_bar") : nullptr;
            const auto& doc = UiView::document(layout);
            const UiVarMap vars;
            const UiHit menu_hit = UiView::hit(doc,
                                               nullptr,
                                               in.mouse_x,
                                               in.mouse_y,
                                               menu.x(),
                                               menu.y(),
                                               menu.w(),
                                               menu.h(),
                                               m_scale,
                                               vars,
                                               nullptr,
                                               false);
            if (menu_hit.kind() == UiHitKind::Button) {
                m_last_menu_action = MenuBarWidget::action_for(menu_hit.action());
                return m_last_menu_action == HudMenuAction::None ? HudPointerResult::Handled
                                                                 : HudPointerResult::MenuAction;
            }
        }

        const int hit = hit_panel(in.mouse_x, in.mouse_y);
        if (hit < 0) {
            return HudPointerResult::Ignored;
        }
        auto& p = m_panels[static_cast<std::size_t>(hit)];
        if (hit == static_cast<int>(HudPanelId::Minimap)) {
            const UiLayout* lay = m_layouts ? &m_layouts->get("minimap") : nullptr;
            float bx = 0.0f;
            float by = 0.0f;
            float bw = 0.0f;
            float bh = 0.0f;
            if (MinimapWidget::zoom_plus_rect(p.x(), p.y(), p.w(), p.h(), lay, bx, by, bw, bh) &&
                WidgetTools::point_in_rect(in.mouse_x, in.mouse_y, bx, by, bw, bh)) {
                m_minimap_zoom = MinimapWidget::step_zoom_in(m_minimap_zoom);
                return HudPointerResult::Handled;
            }
            if (MinimapWidget::zoom_minus_rect(p.x(), p.y(), p.w(), p.h(), lay, bx, by, bw, bh) &&
                WidgetTools::point_in_rect(in.mouse_x, in.mouse_y, bx, by, bw, bh)) {
                m_minimap_zoom = MinimapWidget::step_zoom_out(m_minimap_zoom);
                return HudPointerResult::Handled;
            }
        }
        if (!p.movable()) {
            return HudPointerResult::Handled;
        }
        m_drag_panel = hit;
        m_drag_dx = in.mouse_x - p.x();
        m_drag_dy = in.mouse_y - p.y();
        return HudPointerResult::Handled;
    }

    void GameHud::draw(WidgetTools& ui,
                       engine::IEngine& engine,
                       const StatusWidgetData& status,
                       const MinimapWidgetData& minimap,
                       const DragonHudData& exp_data) const {
        const float s = m_scale;
        auto lay = [this](const char* name) -> const UiLayout* {
            return m_layouts ? &m_layouts->get(name) : nullptr;
        };

        const FreetypeFont* font = m_hud_font.ready() ? &m_hud_font : nullptr;

        const auto& st = panel(HudPanelId::Status);
        if (st.visible()) {
            StatusWidget::draw(ui,
                               engine,
                               font,
                               st.x(),
                               st.y(),
                               status,
                               m_menu_data.mouse_x,
                               m_menu_data.mouse_y,
                               m_menu_data.pointer_enabled,
                               s,
                               lay("status"));
        }

        const auto& mm = panel(HudPanelId::Minimap);
        if (mm.visible()) {
            MinimapWidgetData mm_data = minimap;
            mm_data.set_zoom(m_minimap_zoom);
            MinimapWidget::draw(ui,
                                m_hud_font,
                                m_hud_bold_font,
                                mm.x(),
                                mm.y(),
                                mm.w(),
                                mm.h(),
                                mm_data,
                                lay("minimap"));
        }

        const auto& dragon = panel(HudPanelId::Dragon);
        if (dragon.visible()) {
            const auto size = hud_native_size(m_layouts, "dragon_hud", 792.0f, 180.0f);
            DragonHud::draw(ui,
                            engine,
                            m_hud_font,
                            m_hud_bold_font,
                            dragon.x(),
                            dragon.y(),
                            dragon.w() / std::max(1.0f, size.first),
                            status,
                            exp_data,
                            lay("dragon_hud"));
        }

        const auto& chat = panel(HudPanelId::Chat);
        if (chat.visible()) {
            m_chat.draw(
                ui, engine, chat.x(), chat.y(), chat.w(), chat.h(), s, lay(m_chat.layout_name()));
        }

        const auto& menu = panel(HudPanelId::MenuBar);
        if (menu.visible()) {
            MenuBarWidget::draw(ui,
                                engine,
                                m_hud_font,
                                menu.x(),
                                menu.y(),
                                menu.w(),
                                menu.h(),
                                s,
                                m_menu_data,
                                m_last_w,
                                m_last_h,
                                lay("menu_bar"));
        }
        draw_clock(ui, engine);
    }

    UiVarMap GameHud::clock_vars() const {
        UiVarMap vars;
        const auto& doc = UiView::document(m_layouts ? &m_layouts->get("clock") : nullptr);
        ui_fill_theme_vars(doc, vars);
        vars["clock.title"] = "Clock";
        vars["clock.server_label"] = "Server";
        vars["clock.online_label"] = "Online";
        vars["clock.server_time"] = m_server_clock ? m_server_clock->time_text() : "--:--:--";
        vars["clock.online_time"] = m_server_clock ? m_server_clock->online_text() : "--:--:--";
        vars["clock.countdown"] =
            m_server_clock ? m_server_clock->countdown_text(m_clock_event_id) : "00:00:00:00";
        vars["clock.selected_event"] = "No event  v";
        if (m_server_clock) {
            for (const auto& event : m_server_clock->events()) {
                if (event.id == m_clock_event_id) {
                    vars["clock.selected_event"] = clock_caption(m_hud_font, event.name) + "  v";
                }
            }
        }
        const auto count = m_server_clock ? m_server_clock->events().size() : 0;
        const auto pages = (count + 6) / 6;
        const auto page = std::min(m_clock_page, pages - 1);
        vars["clock.paging"] = pages > 1 ? "true" : "false";
        vars["clock.previous"] = "<";
        vars["clock.next"] = ">";
        vars["clock.page"] = std::to_string(page + 1) + "/" + std::to_string(pages);
        for (std::size_t row = 0; row < 6; ++row) {
            const auto index = page * 6 + row;
            const auto key = "clock.option." + std::to_string(row);
            vars[key + ".visible"] = index <= count ? "true" : "false";
            vars[key] =
                index == 0
                    ? "No event"
                    : (index <= count
                           ? clock_caption(m_hud_font, m_server_clock->events()[index - 1].name)
                           : "");
        }
        return vars;
    }

    UiDocument GameHud::clock_options_document() const {
        auto doc = UiView::document(m_layouts ? &m_layouts->get("clock_event_options") : nullptr);
        const auto count = m_server_clock ? m_server_clock->events().size() : 0;
        if (count < 6) {
            if (const auto* row = doc.find_widget("option_" + std::to_string(count))) {
                const float height = row->y() + row->h() + 5;
                doc.set_window_h(height);
                for (auto& widget : doc.widgets()) {
                    if (widget.id() == "background" || widget.id() == "frame") {
                        widget.set_h(height);
                    }
                }
            }
        }
        return doc;
    }

    UiChromeRect GameHud::clock_options_rect() const {
        const auto& panel = this->panel(HudPanelId::Clock);
        const auto doc = clock_options_document();
        const float w = doc.window_w() * m_scale;
        const float h = doc.window_h() * m_scale;
        return UiChromeRect(
            std::clamp(panel.x() + 5 * m_scale, 0.0f, std::max(0.0f, m_last_w - w)),
            std::clamp(panel.y() - h - 2 * m_scale, 0.0f, std::max(0.0f, m_last_h - h)),
            w,
            h);
    }

    bool GameHud::clock_options_contains(float x, float y) const {
        if (!m_clock_options_open || !m_clock_visible) {
            return false;
        }
        const auto rect = clock_options_rect();
        return WidgetTools::point_in_rect(x, y, rect.x(), rect.y(), rect.w(), rect.h());
    }

    void GameHud::draw_clock(WidgetTools& ui, engine::IEngine& engine) const {
        const auto& panel = this->panel(HudPanelId::Clock);
        if (!panel.visible()) {
            return;
        }
        const auto& doc = UiView::document(m_layouts ? &m_layouts->get("clock") : nullptr);
        const auto vars = clock_vars();
        const float x = panel.x() + doc.pad_left() * m_scale;
        const float y = panel.y() + doc.pad_top() * m_scale;
        const float w = panel.w() - (doc.pad_left() + doc.pad_right()) * m_scale;
        const float h = panel.h() - (doc.pad_top() + doc.pad_bottom()) * m_scale;
        UiViewHost host;
        const auto hit = UiView::hit(
            doc, &ui, m_menu_data.mouse_x, m_menu_data.mouse_y, x, y, w, h, m_scale, vars);
        host.set_pointer_hit(m_menu_data.pointer_enabled ? hit.widget_id() : "",
                             m_menu_data.pressed);
        UiView::draw(
            engine, ui, doc, x, y, w, h, m_scale, vars, m_hud_font, m_hud_bold_font, &host);
        if (m_clock_options_open) {
            const auto options = clock_options_document();
            const auto rect = clock_options_rect();
            const auto option_hit = UiView::hit(options,
                                                &ui,
                                                m_menu_data.mouse_x,
                                                m_menu_data.mouse_y,
                                                rect.x(),
                                                rect.y(),
                                                rect.w(),
                                                rect.h(),
                                                m_scale,
                                                vars,
                                                nullptr,
                                                false);
            host.set_pointer_hit(m_menu_data.pointer_enabled ? option_hit.widget_id() : "",
                                 m_menu_data.pressed);
            UiView::draw(engine,
                         ui,
                         options,
                         rect.x(),
                         rect.y(),
                         rect.w(),
                         rect.h(),
                         m_scale,
                         vars,
                         m_hud_font,
                         m_hud_bold_font,
                         &host,
                         false);
        }
    }

    void GameHud::sync_fonts(engine::IEngine& engine) {
        int px = 12;
        if (m_layouts) {
            const auto& doc = UiView::document(&m_layouts->get("status"));
            if (doc.font_pixel_size() > 0) {
                px = doc.font_pixel_size();
            }
        }
        px = std::clamp(px, 8, 48);
        if (m_hud_font.ready() && m_hud_font_pixel_size == px) {
            return;
        }
        const std::string path = FreetypeFont::find_system_sans();
        m_hud_font.destroy(engine);
        m_hud_bold_font.destroy(engine);
        m_hud_font_pixel_size = 0;
        if (!path.empty() && m_hud_font.load(engine, path, px)) {
            m_hud_font_pixel_size = px;
            (void) m_hud_bold_font.load(engine, FreetypeFont::find_system_sans_bold(), px);
        }
    }

    void GameHud::destroy_fonts(engine::IEngine& engine) {
        m_chat.destroy(engine);
        m_hud_font.destroy(engine);
        m_hud_bold_font.destroy(engine);
        m_hud_font_pixel_size = 0;
    }

}  // namespace openkal::client
