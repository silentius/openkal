/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/EnchantOverlay.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    namespace {

        constexpr float kDurationSec = 2.15f;
        constexpr float kEnchantChargeSec = 2.0f;
        constexpr float kGrowSec = 0.22f;
        constexpr float kFadeStartSec = 0.38f;
        constexpr float kStartScale = 0.38f;
        constexpr float kRefHeight = 600.0f;
        constexpr float kRefY = 150.0f;
        constexpr float kBodyAlpha = 0.80f;
        constexpr float kPanelW = 190.0f;
        constexpr float kPanelH = 35.0f;
        constexpr float kBarPadX = 12.0f;
        constexpr float kBarH = 9.0f;
        constexpr float kBarFillR = 0.86f;
        constexpr float kBarFillG = 0.16f;
        constexpr float kBarFillB = 0.12f;

        [[nodiscard]] float ease_out_cubic(float t) {
            const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
            return 1.0f - u * u * u;
        }

    }  // namespace

    bool EnchantOverlay::start_charge(float duration_sec) {
        if (m_charging) {
            return false;
        }
        m_charge_sec = std::max(0.001f, duration_sec);
        m_charge_elapsed = 0.0f;
        m_charging = true;
        m_charge_ready = false;
        m_active = false;
        m_elapsed = 0.0f;
        return true;
    }

    void EnchantOverlay::begin_charge(std::uint32_t from_id, std::uint32_t to_id) {
        if (m_charging) {
            return;
        }
        m_from_id = from_id;
        m_to_id = to_id;
        m_upgrade_charge = false;
        m_upgrade_rate_up = false;
        m_mix_charge = false;
        m_mix_mode = 0;
        m_mix_weapon_iid = 0;
        m_mix_extra_weapon_iid = 0;
        m_mix_armor_iid = 0;
        m_mix_stone1_iid = 0;
        m_mix_stone2_iid = 0;
        (void) start_charge(kEnchantChargeSec);
    }

    void EnchantOverlay::begin_upgrade_charge(std::uint32_t iid, bool rate_up) {
        if (m_charging) {
            return;
        }
        m_from_id = 0;
        m_to_id = iid;
        m_upgrade_charge = true;
        m_upgrade_rate_up = rate_up;
        m_mix_charge = false;
        m_mix_mode = 0;
        m_mix_weapon_iid = 0;
        m_mix_extra_weapon_iid = 0;
        m_mix_armor_iid = 0;
        m_mix_stone1_iid = 0;
        m_mix_stone2_iid = 0;
        (void) start_charge(kEnchantChargeSec);
    }

    void EnchantOverlay::begin_mix_charge(float duration_sec,
                                          std::uint8_t mode,
                                          std::uint32_t weapon_iid,
                                          std::uint32_t extra_weapon_iid,
                                          std::uint32_t armor_iid,
                                          std::uint32_t stone1_iid,
                                          std::uint32_t stone2_iid) {
        if (m_charging) {
            return;
        }
        m_from_id = 0;
        m_to_id = 0;
        m_upgrade_charge = false;
        m_upgrade_rate_up = false;
        m_mix_charge = true;
        m_mix_mode = mode;
        m_mix_weapon_iid = weapon_iid;
        m_mix_extra_weapon_iid = extra_weapon_iid;
        m_mix_armor_iid = armor_iid;
        m_mix_stone1_iid = stone1_iid;
        m_mix_stone2_iid = stone2_iid;
        (void) start_charge(duration_sec);
    }

    void EnchantOverlay::show_success() {
        m_success = true;
        m_elapsed = 0.0f;
        m_active = true;
    }

    void EnchantOverlay::show_failed() {
        m_success = false;
        m_elapsed = 0.0f;
        m_active = true;
    }

    void EnchantOverlay::tick(float dt) {
        const float step = std::max(0.0f, dt);
        if (m_charging) {
            m_charge_elapsed += step;
            if (m_charge_elapsed >= m_charge_sec) {
                m_charging = false;
                m_charge_elapsed = m_charge_sec;
                m_charge_ready = true;
            }
            return;
        }
        if (!m_active) {
            return;
        }
        m_elapsed += step;
        if (m_elapsed >= kDurationSec) {
            m_active = false;
            m_elapsed = kDurationSec;
        }
    }

    bool EnchantOverlay::take_charge_complete() {
        if (!m_charge_ready) {
            return false;
        }
        m_charge_ready = false;
        return true;
    }

    void EnchantOverlay::draw_charge(WidgetTools& ui,
                                     engine::IEngine& engine,
                                     float screen_w,
                                     float screen_h) const {
        const float scale = screen_h / kRefHeight;
        const float panel_w = kPanelW * scale;
        const float panel_h = kPanelH * scale;
        const float panel_x = (screen_w - panel_w) * 0.5f;
        const float panel_y = kRefY * scale - panel_h * 0.5f;
        const float ratio = std::clamp(m_charge_elapsed / m_charge_sec, 0.0f, 1.0f);
        const float bar_x = panel_x + kBarPadX * scale;
        const float bar_w = panel_w - kBarPadX * 2.0f * scale;
        const float bar_h = kBarH * scale;
        const float bar_y = panel_y + (panel_h - bar_h) * 0.5f;

        engine.draw_ui_rect(0.0f, 0.0f, screen_w, screen_h, 0.0f, 0.0f, 0.0f, 0.40f);
        ui.draw_overlay_panel(panel_x, panel_y, panel_w, panel_h, scale);
        ui.draw_bar(bar_x, bar_y, bar_w, bar_h, ratio, kBarFillR, kBarFillG, kBarFillB);
    }

    void EnchantOverlay::draw(WidgetTools& ui,
                              engine::IEngine& engine,
                              const FreetypeFont& font,
                              float screen_w,
                              float screen_h) const {
        if (screen_w <= 0.0f || screen_h <= 0.0f) {
            return;
        }
        if (m_charging) {
            draw_charge(ui, engine, screen_w, screen_h);
            return;
        }
        if (!m_active || !font.ready()) {
            return;
        }
        const float grow = ease_out_cubic(m_elapsed / kGrowSec);
        const float scale_mul = kStartScale + (1.0f - kStartScale) * grow;
        const float native = static_cast<float>(
            font.pixel_size() > 0 ? font.pixel_size() : FreetypeFont::kTipPixelSize);
        const float target_px =
            2.4f * static_cast<float>(FreetypeFont::kTipPixelSize) * (screen_h / kRefHeight);
        const float base_scale = target_px / std::max(1.0f, native);
        const float scale = std::max(0.05f, base_scale * scale_mul);
        float alpha = 1.0f;
        if (m_elapsed >= kFadeStartSec) {
            const float fade_span = std::max(0.01f, kDurationSec - kFadeStartSec);
            alpha = 1.0f - (m_elapsed - kFadeStartSec) / fade_span;
            alpha = std::clamp(alpha, 0.0f, 1.0f);
        }
        const char* text = m_success ? "Enchanting Success" : "Enchanting Failed";
        const float ui_scale = screen_h / kRefHeight;
        const float border = WidgetTools::overlay_border_thickness(ui_scale);
        const float pad = 8.0f * ui_scale + border;
        const float text_w = font.text_width(text, scale);
        const float text_h = font.line_height() * scale;
        const float box_w = text_w + pad * 2.0f;
        const float box_h = text_h + pad * 2.0f;
        const float y_center = kRefY * ui_scale;
        const float box_x = (screen_w - box_w) * 0.5f;
        const float box_y = y_center - box_h * 0.5f;
        ui.draw_overlay_panel(box_x, box_y, box_w, box_h, ui_scale, alpha);
        const float r = m_success ? 0.20f : 0.95f;
        const float g = m_success ? 0.92f : 0.18f;
        const float b = m_success ? 0.28f : 0.18f;
        font.draw_centered(
            engine, screen_w * 0.5f, box_y + pad, text, r, g, b, scale, alpha * kBodyAlpha);
    }

}  // namespace openkal::client
