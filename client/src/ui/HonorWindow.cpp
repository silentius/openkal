/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/HonorWindow.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    void HonorWindow::set_layout(const UiLayout* layout) {
        UiView::reload(m_document, layout);
    }

    void HonorWindow::set_open(bool open) {
        m_open = open;
    }

    bool HonorWindow::open() const {
        return m_open;
    }

    void HonorWindow::arrange(float width, float height, float scale) {
        const auto chrome = UiView::frame_style(m_document);
        const float outer_w =
            std::max(1.0f, m_document.window_w() + chrome.pad_left() + chrome.pad_right());
        const float outer_h =
            std::max(1.0f, m_document.window_h() + chrome.pad_top() + chrome.pad_bottom());
        m_scale = std::min({scale, width / outer_w, height / outer_h});
        m_x = (width - outer_w * m_scale) * 0.5f + chrome.pad_left() * m_scale;
        m_y = (height - outer_h * m_scale) * 0.5f + chrome.pad_top() * m_scale;
    }

    UiHit HonorWindow::hit(WidgetTools& ui, float x, float y) const {
        if (!m_open) {
            return {};
        }
        return UiView::hit(m_document,
                           &ui,
                           x,
                           y,
                           m_x,
                           m_y,
                           m_document.window_w() * m_scale,
                           m_document.window_h() * m_scale,
                           m_scale,
                           {});
    }

    void HonorWindow::draw(engine::IEngine& engine,
                           WidgetTools& ui,
                           const FreetypeFont& font,
                           const std::optional<protocol::HonorStatus>& status,
                           const engine::FrameInput& input) const {
        if (!m_open) {
            return;
        }
        UiVarMap vars;
        ui_fill_theme_vars(m_document, vars);
        vars["honor.rank"] = status ? std::string(protocol::honor_rank_name(status->rank)) : "—";
        vars["honor.points"] = status ? std::to_string(status->honor_points) : "—";
        vars["honor.weekly_points"] = status ? std::to_string(status->weekly_honor) : "—";
        vars["honor.reward_points"] = status ? std::to_string(status->reward_points) : "—";
        UiViewHost host;
        host.set_pointer_hit(hit(ui, input.mouse_x, input.mouse_y).widget_id(), input.mouse_left);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_document.window_w() * m_scale,
                     m_document.window_h() * m_scale,
                     m_scale,
                     vars,
                     font,
                     font,
                     &host);
    }
}  // namespace openkal::client
