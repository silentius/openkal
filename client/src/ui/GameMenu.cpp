/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GameMenu.h"

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"

#include <algorithm>

namespace openkal::client {

    void GameMenu::set_layout(const UiLayout* layout) {
        m_layout = layout;
        sync_from_layout();
    }

    void GameMenu::set_scale(float scale, float screen_w, float screen_h) {
        m_requested_scale = std::max(0.25f, scale);
        if (screen_w > 0 && screen_h > 0) {
            m_screen_w = screen_w;
            m_screen_h = screen_h;
        }
        sync_from_layout();
    }

    void GameMenu::show(float screen_w, float screen_h) {
        m_screen_w = screen_w;
        m_screen_h = screen_h;
        sync_from_layout();
        m_open = true;
        m_confirming_close = false;
    }

    void GameMenu::close() {
        m_open = false;
        m_confirming_close = false;
    }

    void GameMenu::handle_escape() {
        if (m_confirming_close) {
            m_confirming_close = false;
            return;
        }
        close();
    }

    GameMenuAction GameMenu::handle_click(float mouse_x, float mouse_y, WidgetTools* ui) {
        if (!m_open) {
            return GameMenuAction::Ignored;
        }
        reload_document();
        const UiHit hit = UiView::hit(m_document,
                                      ui,
                                      mouse_x,
                                      mouse_y,
                                      m_x,
                                      m_y,
                                      m_w,
                                      m_h,
                                      m_scale,
                                      make_vars(),
                                      nullptr,
                                      true);
        const std::string& action = hit.action();
        if (action == "game_menu.cancel") {
            close();
        } else if (action == "game_menu.options") {
            return GameMenuAction::Options;
        } else if (action == "game_menu.select_character") {
            return GameMenuAction::SelectCharacter;
        } else if (action == "game_menu.close") {
            m_confirming_close = true;
        } else if (action == "game_menu.close_confirm") {
            return GameMenuAction::QuitClient;
        } else if (action == "game_menu.close_cancel") {
            m_confirming_close = false;
        }
        return GameMenuAction::Consumed;
    }

    bool GameMenu::contains(float mouse_x, float mouse_y) const {
        if (!m_open) {
            return false;
        }
        return WidgetTools::window_outer_hit(
            mouse_x, mouse_y, m_x, m_y, m_w, m_h, m_scale, UiView::frame_style(m_document));
    }

    void GameMenu::draw(engine::IEngine& engine,
                        WidgetTools& ui,
                        const FreetypeFont& font,
                        const engine::FrameInput* input) const {
        if (!m_open) {
            return;
        }
        UiViewHost host;
        if (input) {
            const UiHit hit = UiView::hit(m_document,
                                          &ui,
                                          input->mouse_x,
                                          input->mouse_y,
                                          m_x,
                                          m_y,
                                          m_w,
                                          m_h,
                                          m_scale,
                                          make_vars());
            host.set_pointer_hit(hit.widget_id(), input->mouse_left);
        }
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_w,
                     m_h,
                     m_scale,
                     make_vars(),
                     font,
                     font,
                     &host,
                     true);
    }

    void GameMenu::reload_document() {
        UiView::reload(m_document, m_layout);
    }

    void GameMenu::sync_from_layout() {
        reload_document();
        const auto frame = UiView::frame_style(m_document);
        m_scale = m_requested_scale;
        if (m_screen_w > 0 && m_screen_h > 0) {
            const float outer_w = m_document.window_w() + frame.pad_left() + frame.pad_right();
            const float outer_h = m_document.window_h() + frame.pad_top() + frame.pad_bottom();
            m_scale = std::max(
                0.25f,
                std::min(
                    {m_scale, (m_screen_w - 24.0f) / outer_w, (m_screen_h - 24.0f) / outer_h}));
        }
        m_w = m_document.window_w() * m_scale;
        m_h = m_document.window_h() * m_scale;
        if (m_screen_w > 0 && m_screen_h > 0) {
            WidgetTools::center_rect(m_x, m_y, m_w, m_h, m_screen_w, m_screen_h);
            m_x += (frame.pad_left() - frame.pad_right()) * m_scale * 0.5f;
            m_y += (frame.pad_top() - frame.pad_bottom()) * m_scale * 0.5f;
        }
    }

    UiVarMap GameMenu::make_vars() const {
        UiVarMap vars;
        ui_fill_theme_vars(m_document, vars);
        vars["game_menu.title"] = "Menu";
        vars["game_menu.cancel"] = "Cancel";
        vars["game_menu.options"] = "Options";
        vars["game_menu.select_character"] = "Select Character";
        vars["game_menu.close"] = "Close";
        vars["game_menu.close_question"] = "Are you sure?";
        vars["game_menu.yes"] = "Yes";
        vars["game_menu.no"] = "No";
        vars["game_menu.main"] = m_confirming_close ? "false" : "true";
        vars["game_menu.confirming"] = m_confirming_close ? "true" : "false";
        return vars;
    }

}  // namespace openkal::client
