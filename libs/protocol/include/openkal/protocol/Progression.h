/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>

namespace openkal::protocol {

    /** Max character level supported by NeedExp (index 0 unused; 1..110). */
    inline constexpr int kMaxPlayerLevel = 110;

    /**
     * EXP required to advance from each level (g_nNeedExp).
     * Index = current level; NeedExp[1] is cost of 1→2.
     */
    inline constexpr std::int64_t kNeedExp[111] = {
        0,
        5,
        24,
        60,
        160,  // 4
        328,
        542,
        814,
        1157,
        1588,  // 9
        2250,
        3086,
        4135,
        5444,
        7068,  // 14
        9126,
        11660,
        14769,
        18572,
        23214,  // 19
        29155,
        36409,
        45245,
        55988,
        69026,  // 24
        85569,
        105656,
        130009,
        159494,
        195150,  // 29
        238225,
        290216,
        352918,
        428488,
        519513,  // 34
        629149,
        761081,
        919784,
        1110624,
        1340045,  // 39
        1615778,
        1947099,
        2345141,
        2823265,
        3397501,  // 44
        4087087,
        4915108,
        5909267,
        7102808,
        8535623,  // 49
        10255633,
        12320243,
        14798389,
        17772795,
        21342730,  // 54
        25627317,
        30769501,
        36940820,
        44347118,
        53235407,  // 59
        63902104,
        76702906,
        92064653,
        110499551,
        132622249,  // 64
        159170373,
        191028978,
        229260177,
        275138508,
        330193415,  // 69
        396260232,
        1188853398,
        1426699190,
        1712116603,
        2054620005,  // 74
        2465626643,
        2958837208,
        3550692535,
        4260921625,
        5113199283,  // 79
        6135935388,
        7363221560,
        8835967855,
        10603266353,
        12724027540,  // 84
        15268944005,
        18322846853,
        21987533410,
        26385160468,
        31662316180,  // 89
        37994906328,
        45594017848,
        54712955065,
        65655683173,
        78786960398,  // 94
        94544496618,
        113453543683,
        136144403815,
        163373439683,
        196048286480,  // 99
        60129542154,
        55834574861,
        51539607562,
        68719476754,
        34359738376,  // 104
        773094113290,
        34359738488,
        77309411338,
        34359738384,
        601295421610,  // 109
        42949672974    // 110
    };

    /** Level-diff EXP bonus when monster is higher (g_nReviseExpA[band][diff]). */
    inline constexpr int kReviseExpA[10][21] = {
        {0, 0, 0, 25, 25, 25, 25, 50, 50, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 25, 25, 25, 25, 50, 50, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   0,   25,  25,  25,  25,  50,  50,  50,  50, 50,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   0,   25,  25,  25,  25,  50,  50,  50,  50, 50,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  25,  50,  50,  50,  50,  50, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  25,  50,  50,  50,  50,  50, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  50,  50,  50,  50,  50,  100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  50,  50,  50,  50,  50,  100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  50,  50,  50,  50,  50,  100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   25,  25,  25,  50,  50,  50,  50,  50,  100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100}};

    /** Level-diff EXP penalty when player is higher (g_nReviseExpB[band][diff]). */
    inline constexpr int kReviseExpB[10][21] = {
        {0,   25,  25,  25,  50,  50,  50,  100, 100, 100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   0,   25,  25,  25,  50,  50,  50,  100, 100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0,   0,   0,   25,  25,  25,  50,  50,  50,  100, 100,
         100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100},
        {0, 0, 0, 0, 0, 0, 0, 0, 25, 25, 25, 50, 50, 50, 100, 100, 100, 100, 100, 100, 100}};

    /** S2C_EFFECT effect types (EFFECT_TYPE). */
    enum class EffectType : std::uint8_t {
        None = 0,
        Hp = 1,
        Mp = 2,
        LevelUp = 3,
        Charm1 = 4,
        Charm2 = 5,
        Charm3 = 6,
        Charm4 = 7,
        Charm5 = 8,
        Charm6 = 9,
        Charm7 = 10,
        Charm8 = 11,
        /** E_DAMAGED; packet carries a trailing damage value. */
        Damaged = 23,
    };

    /** Status points granted per level-up (P_PUPOINT). */
    inline constexpr int kPuPerLevel = 5;

    /** Skill points granted per level-up (P_SUPOINT). */
    inline constexpr int kSuPerLevel = 1;

    /**
     * Returns EXP needed to leave the given level.
     * @param level Current player level (clamped to table).
     * @return NeedExp entry, or 0 when invalid.
     */
    [[nodiscard]] std::int64_t need_exp_for_level(int level);

    /**
     * Applies KalOnline solo level-diff revise to a base EXP award.
     * @param base_exp Monster base EXP.
     * @param player_level Killer level.
     * @param monster_level Monster level.
     * @return Revised EXP (may be zero).
     */
    [[nodiscard]] std::int64_t
    revise_kill_exp(std::int64_t base_exp, int player_level, int monster_level);

    /**
     * Computes village revival EXP cut (CutdownExp with nExtraCut=0, no Bless/PK).
     * @param level Player level at death/revival.
     * @return Absolute EXP to subtract (0 if level ≤ 5).
     */
    [[nodiscard]] std::int64_t cutdown_exp_amount(int level);

}  // namespace openkal::protocol
