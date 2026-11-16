/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MonsterRender.h"
#include "test_util.h"

#include <string>
#include <vector>

int main() {
    using openkal::client::MonsterInstance;
    using openkal::test::expect;

    expect(MonsterInstance::is_effect_damage_option("(*EFFECT_DAMAGE)"),
           "bare EFFECT_DAMAGE is detected");
    expect(MonsterInstance::is_effect_damage_option("(*SOUND efc003) (*EFFECT_DAMAGE)"),
           "EFFECT_DAMAGE is found beside SOUND");
    expect(!MonsterInstance::is_effect_damage_option("(*PROJECTILE 2 1)"),
           "projectile keys are not EFFECT_DAMAGE");
    expect(!MonsterInstance::is_effect_damage_option("(*EFFECT_MODEL effect_attack01)"),
           "EFFECT_MODEL is not EFFECT_DAMAGE");

    {
        const std::vector<std::uint16_t> times{0, 180, 400};
        const std::vector<std::string> options{
            "(*SOUND efc003)", "(*EFFECT_DAMAGE)", "(*EFFECT_MODEL effect_attack01)"};
        expect(MonsterInstance::effect_damage_time_ms(times, options) == 180.0f,
               "earliest EFFECT_DAMAGE time is 180 ms");
    }
    {
        const std::vector<std::uint16_t> times{0, 250};
        const std::vector<std::string> options{"(*SOUND efc003)", "(*PROJECTILE 2 1)"};
        expect(MonsterInstance::effect_damage_time_ms(times, options) < 0.0f,
               "missing EFFECT_DAMAGE returns negative");
    }

    expect(!MonsterInstance::action_damage_should_fire(false, 180.0f, 180.0f, false),
           "unlatched hit does not fire");
    expect(!MonsterInstance::action_damage_should_fire(true, 100.0f, 180.0f, false),
           "latched hit waits for the key");
    expect(MonsterInstance::action_damage_should_fire(true, 180.0f, 180.0f, false),
           "latched hit fires at the key");
    expect(MonsterInstance::action_damage_should_fire(true, 400.0f, 180.0f, false),
           "latched hit fires after the key");
    expect(!MonsterInstance::action_damage_should_fire(true, 400.0f, -1.0f, false),
           "missing key waits for clip end");
    expect(MonsterInstance::action_damage_should_fire(true, 400.0f, -1.0f, true),
           "missing key fires at clip end or move interrupt");

    expect(MonsterInstance::blend_opacity_at(0.0f, 0.0f) == 1.0f,
           "SetBlendInTimer(0) stays fully opaque");
    expect(MonsterInstance::blend_opacity_at(3000.0f, 0.0f) == 0.0f, "fade starts invisible");
    expect(MonsterInstance::blend_opacity_at(3000.0f, 1500.0f) == 0.5f, "fade is half at 1.5s");
    expect(MonsterInstance::blend_opacity_at(3000.0f, 3000.0f) == 1.0f, "fade is opaque at 3s");
    expect(MonsterInstance::blend_opacity_at(3000.0f, 4000.0f) == 1.0f, "fade clamps after 3s");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
