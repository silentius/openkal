/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CombatFloatText.h"

#include "openkal/client/WorldPick.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace openkal::client {

    RgbColor CombatFloatText::color_for(CombatFloatKind kind) {
        switch (kind) {
        case CombatFloatKind::DamageDealt:
            return damage_dealt_color();
        case CombatFloatKind::DamageTaken:
            return damage_taken_color();
        case CombatFloatKind::Missed:
        case CombatFloatKind::Blocked:
        case CombatFloatKind::Heal:
            return emerald_color();
        }
        return emerald_color();
    }

    RgbColor CombatFloatText::damage_dealt_color() {
        return RgbColor{1.0f, 0.0f, 0.0f};
    }

    RgbColor CombatFloatText::damage_taken_color() {
        return RgbColor{1.0f, 1.0f, 1.0f};
    }

    RgbColor CombatFloatText::emerald_color() {
        return RgbColor{0.31f, 0.78f, 0.47f};
    }

    const char* CombatFloatText::missed_text() {
        return "Missed";
    }

    const char* CombatFloatText::blocked_text() {
        return "Blocked";
    }

    float CombatFloatText::alpha_at(float age_sec) {
        if (age_sec <= 0.0f) {
            return 1.0f;
        }
        if (age_sec >= kDurationSec) {
            return 0.0f;
        }
        const float fade_start = kDurationSec * kFadeStart;
        if (age_sec < fade_start) {
            return 1.0f;
        }
        const float span = kDurationSec - fade_start;
        if (span <= 1e-6f) {
            return 0.0f;
        }
        return std::clamp(1.0f - (age_sec - fade_start) / span, 0.0f, 1.0f);
    }

    float CombatFloatText::rise_at(float age_sec) {
        const float t = std::clamp(age_sec / kDurationSec, 0.0f, 1.0f);
        return t * kRisePixels;
    }

    void CombatFloatText::spawn(CombatFloatKind kind, std::string text, std::uint32_t anchor_id) {
        if (text.empty()) {
            return;
        }
        Entry entry;
        entry.set_kind(kind);
        entry.set_text(std::move(text));
        entry.set_color(color_for(kind));
        entry.set_anchor_id(anchor_id);
        entry.set_extra_rise(static_cast<float>(live_on_anchor(anchor_id)) * kStackGapPixels);
        m_entries.push_back(std::move(entry));
    }

    float CombatFloatText::last_extra_rise() const {
        if (m_entries.empty()) {
            return 0.0f;
        }
        return m_entries.back().extra_rise();
    }

    int CombatFloatText::live_on_anchor(std::uint32_t anchor_id) const {
        int n = 0;
        for (const Entry& entry : m_entries) {
            if (entry.anchor_id() == anchor_id) {
                ++n;
            }
        }
        return n;
    }

    void CombatFloatText::update(float dt) {
        const float step = std::max(0.0f, dt);
        for (Entry& entry : m_entries) {
            entry.set_age_sec(entry.age_sec() + step);
        }
        m_entries.erase(
            std::remove_if(m_entries.begin(),
                           m_entries.end(),
                           [](const Entry& entry) { return entry.age_sec() >= kDurationSec; }),
            m_entries.end());
    }

    void CombatFloatText::draw_outlined_centered(engine::IEngine& engine,
                                                 const FreetypeFont& font,
                                                 float cx,
                                                 float y,
                                                 std::string_view text,
                                                 const RgbColor& color,
                                                 float alpha) {
        const float g = kOutlineGray;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                font.draw_centered(engine,
                                   cx + static_cast<float>(dx),
                                   y + static_cast<float>(dy),
                                   text,
                                   g,
                                   g,
                                   g,
                                   kDrawScale,
                                   alpha);
            }
        }
        font.draw_centered(engine, cx, y, text, color.r(), color.g(), color.b(), kDrawScale, alpha);
    }

    void CombatFloatText::draw(
        engine::IEngine& engine,
        const FreetypeFont& font,
        const FollowCamera& cam,
        const LocalPlayer& player,
        float screen_w,
        float screen_h,
        float local_x,
        float local_y,
        float local_z,
        const std::function<bool(std::uint32_t, float&, float&, float&)>& monster_pos) {
        if (!font.ready() || m_entries.empty()) {
            return;
        }
        for (Entry& entry : m_entries) {
            if (!entry.has_world()) {
                float wx = 0.0f;
                float wy = 0.0f;
                float wz = 0.0f;
                bool have = false;
                if (entry.anchor_id() == 0) {
                    wx = local_x;
                    wy = local_y + kHeadLift;
                    wz = local_z;
                    have = true;
                } else if (monster_pos && monster_pos(entry.anchor_id(), wx, wy, wz)) {
                    wy += kHeadLift;
                    have = true;
                }
                if (!have) {
                    continue;
                }
                entry.set_wx(wx);
                entry.set_wy(wy);
                entry.set_wz(wz);
                entry.set_has_world(true);
            }
            float sx = 0.0f;
            float sy = 0.0f;
            if (!WorldPick::project_world_to_screen(
                    cam, player, entry.wx(), entry.wy(), entry.wz(), screen_w, screen_h, sx, sy)) {
                continue;
            }
            const float alpha = alpha_at(entry.age_sec());
            if (alpha <= 0.0f) {
                continue;
            }
            const float y = sy - rise_at(entry.age_sec()) - entry.extra_rise() -
                            font.line_height() * kDrawScale;
            draw_outlined_centered(
                engine, font, sx + entry.jitter_x(), y, entry.text(), entry.color(), alpha);
        }
    }

}  // namespace openkal::client
