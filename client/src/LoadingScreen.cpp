/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/LoadingScreen.h"

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <algorithm>
#include <utility>

namespace openkal::client {

    LoadingScreen::LoadingScreen(engine::IEngine& engine,
                                 WidgetTools& ui,
                                 const FreetypeFont& status_font,
                                 std::string background_name)
        : m_engine(engine), m_ui(ui), m_status_font(status_font),
          m_background_name(std::move(background_name)) {}

    bool LoadingScreen::present(std::string_view status, float progress) {
        engine::FrameInput input;
        if (!m_engine.poll(input) || input.quit) {
            return false;
        }
        m_engine.begin_frame(0.05f, 0.06f, 0.08f);
        const float width = static_cast<float>(input.width);
        const float height = static_cast<float>(input.height);
        const float scale = std::min({1.0f, width / 640.0f, height / 480.0f});
        const float footer_h = 32.0f * scale;
        const float footer_y = height - footer_h;
        // Reserve the footer instead of covering copyright text baked into the background.
        m_ui.draw_tex(m_background_name, 0.0f, 0.0f, width, footer_y);
        m_ui.draw_tex("dragon_dialog/panel_background.bmp", 0, footer_y, width, footer_h);
        std::string percentage;
        if (progress >= 0.0f) {
            progress = std::clamp(progress, 0.0f, 1.0f);
            const float bar_h = 4.0f * scale;
            m_engine.draw_ui_rect(0, height - bar_h, width, bar_h, 0.15f, 0.13f, 0.09f, 1.0f);
            m_engine.draw_ui_rect(
                0, height - bar_h, width * progress, bar_h, 0.78f, 0.63f, 0.32f, 1.0f);
            percentage = std::to_string(static_cast<int>(progress * 100.0f)) + "%";
            status = percentage;
        }
        if (!status.empty() && m_status_font.ready()) {
            const float text_scale = 18.0f * scale / m_status_font.pixel_size();
            const float text_h = m_status_font.line_height() * text_scale;
            m_status_font.draw_centered(m_engine,
                                        width * 0.5f,
                                        footer_y + (footer_h - 4.0f * scale - text_h) * 0.5f,
                                        status,
                                        0.898f,
                                        0.812f,
                                        0.620f,
                                        text_scale);
        }
        m_engine.end_frame();
        return true;
    }

}  // namespace openkal::client
