/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PileSkillBar.h"

#include "openkal/protocol/ArcherSkill.h"

#include <algorithm>
#include <cstdio>

namespace openkal::client {
    namespace {

        /** Compatibility behavior info width. */
        constexpr float kBarW = 250.0f;

        /** Compatibility behavior info height. */
        constexpr float kBarH = 20.0f;

        /** Create() Y: viewHeight/2 + 60. */
        constexpr float kCenterYOffset = 60.0f;

        /** compatibility behavior MP fill RGB(0, 100, 255). */
        constexpr float kFillR = 0.0f;

        /** Green channel of the MP fill. */
        constexpr float kFillG = 100.0f / 255.0f;

        /** Blue channel of the MP fill. */
        constexpr float kFillB = 1.0f;

    }  // namespace

    void PileSkillBar::draw(WidgetTools& ui,
                            engine::IEngine& engine,
                            const FreetypeFont& font,
                            float screen_w,
                            float screen_h,
                            float elapsed_ms,
                            float max_ms,
                            std::string_view skill_name) {
        if (screen_w <= 1.0f || screen_h <= 1.0f || max_ms <= 0.0f) {
            return;
        }
        const float ratio = std::clamp(elapsed_ms / max_ms, 0.0f, 1.0f);
        const float bar_x = screen_w * 0.5f - kBarW * 0.5f;
        const float bar_y = screen_h * 0.5f + kCenterYOffset;
        ui.draw_bar(bar_x, bar_y, kBarW, kBarH, ratio, kFillR, kFillG, kFillB);
        ui.draw_progress_lcr("main-prograss-blue", bar_x, bar_y, kBarW, kBarH, ratio);

        if (!font.ready()) {
            return;
        }
        constexpr float kNameScale = 0.85f;
        if (!skill_name.empty()) {
            font.draw_centered(engine,
                               screen_w * 0.5f,
                               bar_y - font.line_height() * kNameScale - 4.0f,
                               skill_name,
                               1.0f,
                               1.0f,
                               1.0f,
                               kNameScale);
        }
        const int count = protocol::archer_focus_shot_bar_count(static_cast<int>(elapsed_ms));
        if (count <= 0) {
            return;
        }
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        if (count == 2) {
            g = 200.0f / 255.0f;
            b = 200.0f / 255.0f;
        } else if (count == 3) {
            g = 130.0f / 255.0f;
            b = 130.0f / 255.0f;
        } else if (count == 4) {
            g = 70.0f / 255.0f;
            b = 70.0f / 255.0f;
        } else if (count >= 5) {
            g = 0.0f;
            b = 0.0f;
        }
        char pip[8];
        std::snprintf(pip, sizeof(pip), "%d", count);
        font.draw(engine, bar_x + kBarW + 8.0f, bar_y + 1.0f, pip, r, g, b, kNameScale);
    }

}  // namespace openkal::client
