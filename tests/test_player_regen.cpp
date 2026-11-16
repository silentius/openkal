/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Transform.h"
#include "openkal/server/InventoryUnlock.h"
#include "openkal/server/PayBuff.h"
#include "openkal/server/PlayerRegen.h"
#include "openkal/server/RestFoodBuff.h"
#include "test_util.h"

int main() {
    using openkal::test::expect;
    using namespace openkal::server::player_regen;
    namespace protocol = openkal::protocol;

    expect(standing_hp_amount(100) == 1, "stand hp maxHP/100");
    expect(standing_hp_amount(3999) == 39, "stand hp below 40 cap");
    expect(standing_hp_amount(4000) == 40, "stand hp 40 cap at 4000");
    expect(standing_hp_amount(9000) == 40, "stand hp 40 cap");
    expect(standing_hp_amount(0) == 0, "stand hp zero max");

    expect(standing_mp_amount(200) == 2, "stand mp maxMP/100");
    expect(standing_mp_amount(1999) == 19, "stand mp below 20 cap");
    expect(standing_mp_amount(2000) == 20, "stand mp 20 cap at 2000");
    expect(standing_mp_amount(5000) == 20, "stand mp 20 cap");
    expect(standing_mp_amount(0) == 0, "stand mp zero max");

    expect(sit_random_hi(0) == 0, "sit rng hi at restPoint 0");
    expect(sit_random_hi(3) == 6, "sit rng hi is 2*restPoint");
    expect(sit_hp_amount(0, 0) == 25, "sit hp restPoint 0");
    expect(sit_mp_amount(0, 0) == 20, "sit mp restPoint 0");
    expect(sit_hp_amount(1, 0) == 32, "sit hp restPoint 1 roll 0");
    expect(sit_mp_amount(1, 0) == 27, "sit mp restPoint 1 roll 0");
    expect(sit_hp_amount(1, 2) == 34, "sit hp restPoint 1 roll 2");
    expect(sit_mp_amount(1, 2) == 29, "sit mp restPoint 1 roll 2");

    expect(sit_first_due_ms(1000) == 6000, "sit first pulse +5000");
    expect(sit_next_due_ms(6000) == 10000, "sit later pulses +4000");

    {
        std::int32_t hp = 90;
        expect(add_clamped(hp, 100, 25) && hp == 100, "add_clamped caps at max");
        expect(!add_clamped(hp, 100, 10), "add_clamped no change at max");
        std::int32_t mp = 10;
        expect(add_clamped(mp, 50, 20) && mp == 30, "add_clamped adds");
    }

    {
        TransformExpState state;
        state.index = 337;
        state.level = 2;
        state.exp = 0;
        state.put_on_ticks = 0;
        bool pending = false;
        for (int i = 0; i < 179; ++i) {
            const TransformExpTickResult r = tick_transform_exp(state, 50);
            pending = pending || r.pulsed || r.applied || state.exp != 0;
        }
        expect(!pending && state.exp == 0, "transform 179 ticks no exp");
        const TransformExpTickResult pulse = tick_transform_exp(state, 50);
        expect(pulse.pulsed && pulse.applied && pulse.added == 10, "transform tick 180 +10");
        expect(state.exp == 10 && state.level == 2 && !pulse.leveled,
               "transform exp kept without level-up");
        expect(state.put_on_ticks == 0, "transform put_on wraps by 180");
    }

    {
        TransformExpState state;
        state.index = 337;
        state.level = 1;
        state.exp = protocol::need_transform_exp_for_level(1) - 10;
        state.put_on_ticks = 179;
        const TransformExpTickResult blocked = tick_transform_exp(state, 1);
        expect(blocked.pulsed && !blocked.applied &&
                   state.exp == protocol::need_transform_exp_for_level(1) - 10,
               "transform exp reverted at player level gate");
    }

    {
        TransformExpState state;
        state.index = 337;
        state.level = 1;
        state.exp = protocol::need_transform_exp_for_level(1) - 10;
        const TransformExpTickResult up = add_transform_exp(state, 50, 10);
        expect(up.applied && up.leveled && state.level == 2 && state.exp == 0,
               "transform table level-up 1 to 2");
    }

    {
        expect(protocol::need_transform_exp_for_level(1) == 7680, "g_nLevelUpExp[1]");
        expect(protocol::kill_transform_exp_add(20) == 1, "kill exp at 20");
        expect(protocol::kill_transform_exp_add(21) == 2, "kill exp at 21");
        expect(protocol::max_transform_stage(11) == 1, "baby at egg 11");
        expect(protocol::max_transform_stage(21) == 2, "young at egg 21");
        expect(protocol::max_transform_stage(31) == 3, "adult at egg 31");
        const auto tiger = protocol::worn_transform_bonus(0, 16);
        expect(tiger.str == 9 && tiger.hth == 5 && tiger.dex == 3, "tiger worn bonus lv16");
        const auto form = protocol::form_transform_bonus(0, 3);
        expect(form.mspeed_percent == 30 && form.attack_percent == 4, "tiger adult form");
        expect(protocol::form_transform_bonus(0, 1).mspeed_percent == 30,
               "tiger baby already has move-speed percent");
    }

    {
        TransformExpState empty;
        empty.put_on_ticks = 50;
        const TransformExpTickResult none = tick_transform_exp(empty, 50);
        expect(!none.pulsed && empty.put_on_ticks == 0, "no transform clears put_on");
    }

    {
        openkal::server::RestFoodSet food;
        expect(!food.active() && food.mstate() == 0, "rest food starts empty");
        expect(food.apply_hp(9, 30, 1000), "apply hp food");
        expect(food.hp_active() && food.mstate() == openkal::protocol::kCmsHp, "cms hp bit");
        const auto none = food.tick(1500);
        expect(none.hp_add == 0 && !none.hp_expired, "no pulse before 1s");
        int pulses = 0;
        bool expired = false;
        for (std::uint64_t t = 2000; t <= 31000; t += 1000) {
            const auto pulse = food.tick(t);
            if (pulse.hp_add == 9) {
                ++pulses;
            }
            expired = expired || pulse.hp_expired;
        }
        expect(pulses == 30, "hp food 30 pulses");
        expect(expired && !food.hp_active(), "hp food expires at 30s");
    }

    {
        openkal::server::RestFoodSet food;
        expect(food.apply_mp(39, 30, 0), "apply mp food");
        expect(food.mp_active() && food.mstate() == openkal::protocol::kCmsMp, "cms mp bit");
        expect(food.apply_hp(9, 30, 0), "hp and mp food together");
        expect(food.mstate() == (openkal::protocol::kCmsHp | openkal::protocol::kCmsMp),
               "both cms bits");
        food.cancel_all();
        expect(!food.active() && food.mstate() == 0, "stand cancel clears rest food");
    }

    {
        openkal::server::RestFoodSet food;
        expect(food.apply_hp(9, 30, 0), "apply before refresh");
        expect(food.apply_hp(12, 30, 5000), "refresh replaces amount");
        const auto pulse = food.tick(6000);
        expect(pulse.hp_add == 12, "refresh uses new amount");
    }

    {
        openkal::server::PayBuffSet pay;
        std::int32_t remain = 0;
        std::uint64_t bstate = 0;
        expect(!pay.apply(18, 30, 1000, remain, bstate), "pay buff rejects hp food");
        expect(!pay.apply(19, 30, 1000, remain, bstate), "pay buff rejects mp food");
        expect(pay.apply(51, 3600, 1000, remain, bstate) &&
                   bstate == openkal::protocol::kCbsPayUpAttack,
               "pay attack scroll still applies");
        expect(openkal::server::is_pay_buff_type(51) && !openkal::server::is_pay_buff_type(18),
               "pay type helper");
        expect(!pay.apply(5, 3600, 1000, remain, bstate, 271, 0), "attack food needs a percent");
        expect(pay.apply(5, 3600, 1000, remain, bstate, 271, 10) &&
                   bstate == openkal::protocol::kCbsFoodAttack && remain == 3600,
               "physical attack food applies");
        expect(pay.percent_of(5) == 10, "physical attack food stores its percent");
        expect(pay.apply(5, 3600, 2000, remain, bstate, 271, 10) &&
                   openkal::server::PayBuffSet::remain_of(pay.entries().back(), 2000) == 3600,
               "eating attack food again refreshes the hour");
        expect(pay.apply(6, 3600, 2000, remain, bstate, 272, 10) &&
                   bstate == openkal::protocol::kCbsFoodMagic && pay.percent_of(6) == 10,
               "magical attack food applies beside the physical one");
        const auto rows = pay.snapshot(4, 2000);
        openkal::server::PayBuffSet restored;
        restored.load(rows, 2000);
        expect(restored.percent_of(5) == 10 && restored.percent_of(6) == 10,
               "attack food percent survives persistence");
    }

    {
        openkal::server::InventoryUnlockSet unlocks;
        constexpr std::int64_t now = 1000;
        expect(unlocks.apply(86400, now), "unlock first inventory register");
        expect(unlocks.apply(604800, now), "unlock second inventory register");
        expect(unlocks.apply(2592000, now), "unlock third inventory register");
        expect(unlocks.entries().size() == 3, "three inventory registers active");
        expect(openkal::server::InventoryUnlockSet::remain_of(unlocks.entries()[0], now) == 2592000,
               "longest inventory unlock maps to register 3");
        expect(openkal::server::InventoryUnlockSet::remain_of(unlocks.entries()[2], now) == 86400,
               "shortest inventory unlock maps to last active register");
        const auto rows = unlocks.snapshot(7, now);
        expect(rows.size() == 3 && rows[0].register_index == 0 && rows[2].register_index == 2,
               "inventory unlock persistence preserves register order");
        openkal::server::InventoryUnlockSet restored;
        restored.load(rows, now);
        expect(restored.entries().size() == 3 && openkal::server::InventoryUnlockSet::remain_of(
                                                     restored.entries()[0], now) == 2592000,
               "inventory unlock persistence restores sorted durations");
        expect(restored.tick(now + 86400) && restored.entries().size() == 2,
               "expired inventory unlock is removed");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
