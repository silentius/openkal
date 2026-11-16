/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RidingCacheOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace openkal::client {
    namespace {
        constexpr float kDurationSec = 2.15f;
        constexpr float kGrowSec = 0.22f;
        constexpr float kFadeStartSec = 0.38f;
        constexpr float kStartScale = 0.38f;
        constexpr float kReferenceHeight = 600.0f;
        constexpr float kOverlayCenterY = 160.0f;
        constexpr float kCacheSize = 86.0f;
        constexpr float kCacheCenterY = 60.0f;
        constexpr float kRewardIconSize = 28.0f;
        constexpr float kBodyAlpha = 0.80f;

        [[nodiscard]] float ease_out_cubic(float t) {
            const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
            return 1.0f - u * u * u;
        }

        [[nodiscard]] float result_alpha(float elapsed) {
            if (elapsed >= kFadeStartSec) {
                const float fade_span = std::max(0.01f, kDurationSec - kFadeStartSec);
                return std::clamp(1.0f - (elapsed - kFadeStartSec) / fade_span, 0.0f, 1.0f);
            }
            return 1.0f;
        }
    }  // namespace

    void RidingCacheOverlay::show(const protocol::RidingCacheResult& result) {
        m_result = result;
        m_elapsed = 0.0f;
        m_visible = true;
    }

    void RidingCacheOverlay::tick(float dt) {
        if (!m_visible) {
            return;
        }
        m_elapsed += std::max(0.0f, dt);
        if (m_elapsed >= kDurationSec) {
            m_elapsed = kDurationSec;
            m_visible = false;
        }
    }

    void RidingCacheOverlay::draw(WidgetTools& ui,
                                  engine::IEngine& engine,
                                  const FreetypeFont& font,
                                  const ItemDb& catalog,
                                  float screen_w,
                                  float screen_h) const {
        if (!m_visible || !font.ready() || screen_w <= 0.0f || screen_h <= 0.0f) {
            return;
        }
        const float ui_scale = screen_h / kReferenceHeight;
        const float grow = ease_out_cubic(m_elapsed / kGrowSec);
        const float scale_mul = kStartScale + (1.0f - kStartScale) * grow;
        const float native = static_cast<float>(
            font.pixel_size() > 0 ? font.pixel_size() : FreetypeFont::kTipPixelSize);
        const float target_px = 2.4f * static_cast<float>(FreetypeFont::kTipPixelSize) * ui_scale;
        const float title_scale = std::max(0.05f, target_px / std::max(1.0f, native) * scale_mul);
        const float alpha = result_alpha(m_elapsed);
        const char* title = "Riding Cache Opened";
        const float border = WidgetTools::overlay_border_thickness(ui_scale);
        const float pad = 8.0f * ui_scale + border;
        const float title_w = font.text_width(title, title_scale);
        const float title_h = font.line_height() * title_scale;
        const float reward_size = kRewardIconSize * ui_scale;
        const float panel_w = std::max(title_w, reward_size * 2.0f + 46.0f * ui_scale) + pad * 2.0f;
        const float panel_h = title_h + reward_size + pad * 3.0f;
        const float panel_x = (screen_w - panel_w) * 0.5f;
        const float panel_y = kOverlayCenterY * ui_scale - panel_h * 0.5f;
        const float center_x = screen_w * 0.5f;

        ui.draw_overlay_panel(panel_x, panel_y, panel_w, panel_h, ui_scale, alpha);
        font.draw_centered(engine,
                           center_x,
                           panel_y + pad,
                           title,
                           0.20f,
                           0.92f,
                           0.28f,
                           title_scale,
                           alpha * kBodyAlpha);

        const float cache_size = kCacheSize * ui_scale;
        const float cache_x = center_x - cache_size * 0.5f;
        const float cache_y = kCacheCenterY * ui_scale - cache_size * 0.5f;
        ui.draw_tex("ridingCacheOpen.bmp", cache_x, cache_y, cache_size, cache_size, alpha);

        // Deliberately lightweight UI particles: they decorate the confirmation without
        // coupling a menu presentation to world SpriteFx/CombatFx semantics.
        constexpr std::array<float, 8> kAngles = {
            0.10f, 0.92f, 1.72f, 2.46f, 3.17f, 4.04f, 4.78f, 5.53f};
        const float pulse = 0.55f + 0.45f * std::sin(m_elapsed * 8.0f);
        for (std::size_t i = 0; i < kAngles.size(); ++i) {
            const float radius = (48.0f + 7.0f * std::sin(m_elapsed * 3.0f + i)) * ui_scale;
            const float x = center_x + std::cos(kAngles[i] + m_elapsed * 0.32f) * radius;
            const float y = cache_y + cache_size * 0.50f +
                            std::sin(kAngles[i] + m_elapsed * 0.32f) * radius * 0.72f;
            const float size = (2.0f + static_cast<float>(i % 3)) * ui_scale;
            engine.draw_ui_rect(
                x - size * 0.5f, y - size * 0.5f, size, size, 1.0f, 0.76f, 0.18f, pulse * alpha);
        }

        const float reward_y = panel_y + pad * 2.0f + title_h;
        const float reward_gap = 22.0f * ui_scale;
        const auto draw_reward = [&](std::uint16_t index, std::uint16_t count, float center) {
            ui.draw_item_tex(catalog.icon_filename(index),
                             center - reward_size * 0.5f,
                             reward_y,
                             reward_size,
                             reward_size,
                             alpha);
            if (count > 1) {
                font.draw_centered(engine,
                                   center + reward_size * 0.48f,
                                   reward_y + reward_size * 0.47f,
                                   "x" + std::to_string(count),
                                   1.0f,
                                   0.92f,
                                   0.60f,
                                   title_scale * 0.50f,
                                   alpha);
            }
        };
        draw_reward(m_result.seal_item_index, m_result.seal_count, center_x - reward_gap);
        draw_reward(m_result.reward_item_index, m_result.reward_count, center_x + reward_gap);
    }
}  // namespace openkal::client
