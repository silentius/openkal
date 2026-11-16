/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/FishingBar.h"

#include <algorithm>

namespace openkal::client {
    namespace {

        constexpr float kBarW = 250.0f;
        constexpr float kBarH = 20.0f;
        constexpr float kTopMargin = 44.0f;
        constexpr float kTitleGap = 4.0f;
        constexpr float kHintGap = 4.0f;

    }  // namespace

    void FishingBar::draw(WidgetTools& ui,
                          engine::IEngine& engine,
                          const FreetypeFont& font,
                          float screen_w,
                          float screen_h,
                          float ui_scale,
                          float elapsed_ms,
                          float duration_ms,
                          std::string_view title) {
        if (screen_w <= 1.0f || screen_h <= 1.0f || duration_ms <= 0.0f) {
            return;
        }
        const float scale = std::max(0.25f, ui_scale);
        const float ratio = std::clamp(elapsed_ms / duration_ms, 0.0f, 1.0f);
        const float bar_w = kBarW * scale;
        const float bar_h = kBarH * scale;
        const float bar_x = screen_w * 0.5f - bar_w * 0.5f;
        const float bar_y = kTopMargin * scale;
        ui.draw_bar(bar_x, bar_y, bar_w, bar_h, ratio, 0.0f, 100.0f / 255.0f, 1.0f);
        ui.draw_progress_lcr("main-prograss-blue", bar_x, bar_y, bar_w, bar_h, ratio);
        if (!font.ready()) {
            return;
        }
        constexpr float kTextScale = 0.85f;
        const float text_scale = kTextScale * scale;
        font.draw_centered(engine,
                           screen_w * 0.5f,
                           bar_y - font.line_height() * text_scale - kTitleGap * scale,
                           title,
                           1.0f,
                           1.0f,
                           1.0f,
                           text_scale);
        font.draw_centered(engine,
                           screen_w * 0.5f,
                           bar_y + bar_h + kHintGap * scale,
                           "Press Esc to cancel",
                           0.85f,
                           0.85f,
                           0.85f,
                           text_scale);
    }

}  // namespace openkal::client
