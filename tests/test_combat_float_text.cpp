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
#include "test_util.h"

#include <cmath>
#include <cstring>
#include <string_view>

namespace openkal::client {

    float FreetypeFont::text_width(std::string_view, float) const {
        return 0.0f;
    }

    void FreetypeFont::draw_centered(
        engine::IEngine&, float, float, std::string_view, float, float, float, float, float) const {
    }

    void FreetypeFont::draw(
        engine::IEngine&, float, float, std::string_view, float, float, float, float, float) const {
    }

    bool WorldPick::project_world_to_screen(const FollowCamera&,
                                            const LocalPlayer&,
                                            float,
                                            float,
                                            float,
                                            float,
                                            float,
                                            float&,
                                            float&) {
        return false;
    }

}  // namespace openkal::client

namespace {

    bool near_rgb(const openkal::client::RgbColor& c, float r, float g, float b) {
        return std::fabs(c.r() - r) < 1e-4f && std::fabs(c.g() - g) < 1e-4f &&
               std::fabs(c.b() - b) < 1e-4f;
    }

    bool near_f(float a, float b) {
        return std::fabs(a - b) < 1e-4f;
    }

}  // namespace

int main() {
    using openkal::client::CombatFloatKind;
    using openkal::client::CombatFloatText;
    using openkal::test::expect;
    using openkal::test::g_failures;

    expect(near_rgb(CombatFloatText::damage_dealt_color(), 1.0f, 0.0f, 0.0f),
           "player-on-monster damage is red");
    expect(near_rgb(CombatFloatText::damage_taken_color(), 1.0f, 1.0f, 1.0f),
           "monster-on-player damage is white");
    expect(near_rgb(CombatFloatText::emerald_color(), 0.31f, 0.78f, 0.47f),
           "miss block heal emerald");
    expect(near_rgb(CombatFloatText::color_for(CombatFloatKind::DamageDealt), 1.0f, 0.0f, 0.0f),
           "dealt kind uses red");
    expect(near_rgb(CombatFloatText::color_for(CombatFloatKind::DamageTaken), 1.0f, 1.0f, 1.0f),
           "taken kind uses white");
    expect(near_rgb(CombatFloatText::color_for(CombatFloatKind::Missed), 0.31f, 0.78f, 0.47f),
           "missed kind uses emerald");
    expect(near_rgb(CombatFloatText::color_for(CombatFloatKind::Blocked), 0.31f, 0.78f, 0.47f),
           "blocked kind uses emerald");
    expect(near_rgb(CombatFloatText::color_for(CombatFloatKind::Heal), 0.31f, 0.78f, 0.47f),
           "heal kind uses emerald");

    expect(std::strcmp(CombatFloatText::missed_text(), "Missed") == 0, "miss copy is Missed");
    expect(std::strcmp(CombatFloatText::blocked_text(), "Blocked") == 0, "block copy is Blocked");
    expect(near_f(CombatFloatText::kDrawScale, 1.56f), "combat draw scale is 1.56");
    expect(near_f(CombatFloatText::kHeadLift, 12.0f), "combat text starts above the head");

    expect(near_f(CombatFloatText::alpha_at(0.0f), 1.0f), "alpha is 1 at spawn");
    expect(near_f(CombatFloatText::rise_at(0.0f), 0.0f), "rise is 0 at spawn");

    const float mid = CombatFloatText::kDurationSec * 0.5f;
    expect(near_f(CombatFloatText::alpha_at(mid), 1.0f), "alpha stays 1 at mid life");
    expect(near_f(CombatFloatText::rise_at(mid), CombatFloatText::kRisePixels * 0.5f),
           "rise is half at mid life");

    expect(near_f(CombatFloatText::alpha_at(CombatFloatText::kDurationSec), 0.0f),
           "alpha is 0 at expiry");
    expect(near_f(CombatFloatText::rise_at(CombatFloatText::kDurationSec),
                  CombatFloatText::kRisePixels),
           "rise is full at expiry");

    const float fade_mid =
        CombatFloatText::kDurationSec * (CombatFloatText::kFadeStart + 1.0f) * 0.5f;
    expect(CombatFloatText::alpha_at(fade_mid) > 0.0f && CombatFloatText::alpha_at(fade_mid) < 1.0f,
           "alpha is between 0 and 1 during fade");

    CombatFloatText floats;
    floats.spawn(CombatFloatKind::DamageDealt, "", 1);
    expect(floats.size() == 0, "empty text is not spawned");
    floats.spawn(CombatFloatKind::Missed, CombatFloatText::missed_text(), 0);
    expect(floats.size() == 1, "missed line is live");
    floats.spawn(CombatFloatKind::Blocked, CombatFloatText::blocked_text(), 0);
    expect(floats.size() == 2, "second line is also visible");
    floats.spawn(CombatFloatKind::Heal, "12", 0);
    expect(floats.size() == 3, "third same-anchor line stays visible");
    expect(near_f(floats.last_extra_rise(), CombatFloatText::kStackGapPixels * 2.0f),
           "stacked spawn is offset instead of overlapping");
    floats.update(CombatFloatText::kDurationSec * 0.5f);
    expect(floats.size() == 3, "all three still live at mid duration");
    floats.update(CombatFloatText::kDurationSec);
    expect(floats.size() == 0, "lines expire after full lifetime");

    return g_failures ? 1 : 0;
}
