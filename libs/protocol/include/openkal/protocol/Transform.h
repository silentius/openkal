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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Max transform / animal level (MAX_TRANSLV). */
    inline constexpr int kMaxTransformLevel = 100;

    /** Yellow monster color (DLT_YELLOW); kill EXP requires this or higher. */
    inline constexpr int kColorYellow = 3;

    /** C2S_TRANSFORM flag: start the transform cast. */
    inline constexpr std::uint8_t kTransMPre = 0;

    /** C2S_TRANSFORM flag: finish the transform after PRE. */
    inline constexpr std::uint8_t kTransMExecute = 1;

    /** C2S_TRANSFORM flag: leave animal form. */
    inline constexpr std::uint8_t kTransMFree = 2;

    /** Baby transform stage (TRANS_LV_BABY). */
    inline constexpr int kTransLvBaby = 1;

    /** Young transform stage (TRANS_LV_YOUNG). */
    inline constexpr int kTransLvYoung = 2;

    /** Adult transform stage (TRANS_LV_ADULT). */
    inline constexpr int kTransLvAdult = 3;

    /** compatibility behavior. */
    inline constexpr std::uint32_t kCgsOnTransform = 0x00000200u;

    /** compatibility behavior. */
    inline constexpr std::uint32_t kCgsOnStall = 0x00000010u;

    /** compatibility behavior. */
    inline constexpr std::uint32_t kCgsFish = 0x00000020u;

    /** compatibility behavior. */
    inline constexpr std::uint32_t kCgsCooking = 0x00000040u;

    /** METAMO_ORIGIN — human form. */
    inline constexpr int kMetamoOrigin = 15;

    /** Skill class offset so animal roles do not collide with PC 0–3. */
    inline constexpr int kSkillClassAnimalBase = 10;

    /** Transform-into skills (later IsTransformSkill). */
    inline constexpr int kSkillTransformBaby = 200;
    inline constexpr int kSkillTransformYoung = 201;
    inline constexpr int kSkillTransformAdult = 202;

    /** MSG_NOTENOUGHRAGE. */
    inline constexpr std::uint8_t kMsgNotEnoughRage = 65;

    /** MSG_NOTTRANSFORMAREA. */
    inline constexpr std::uint8_t kMsgNotTransformArea = 66;

    /** MSG_NOTTRANSFORMBYFLAG. */
    inline constexpr std::uint8_t kMsgNotTransformByFlag = 76;

    /**
     * Animal EXP to leave each egg level (g_nLevelUpExp).
     * Index = current animal level; [1] is cost of 1→2.
     */
    inline constexpr int kNeedTransformExp[101] = {
        0,       7680,    9120,    11520,   14880,   19200,   24480,   30720,   37920,   46080,
        55200,   65280,   76320,   88320,   101280,  115200,  130080,  145920,  162720,  180480,
        199200,  218880,  239520,  261120,  283680,  307200,  331680,  357120,  383520,  410880,
        439200,  468480,  498720,  529920,  562080,  595200,  629280,  664320,  700320,  737280,
        775200,  814080,  853920,  894720,  936480,  979200,  1022880, 1067520, 1113120, 1159680,
        1207200, 1255680, 1305120, 1355520, 1406880, 1459200, 1512480, 1566720, 1621920, 1678080,
        1735200, 1793280, 1852320, 1912320, 1973280, 2035200, 2098080, 2161920, 2226720, 2292480,
        2359200, 2426880, 2495520, 2565120, 2635680, 2707200, 2779680, 2853120, 2927520, 3002880,
        3079200, 3156480, 3234720, 3313920, 3394080, 3475200, 3557280, 3640320, 3724320, 3809280,
        3895200, 3982080, 4069920, 4158720, 4248480, 4339200, 4430880, 4523520, 4617120, 4711680,
        4807200};

    /**
     * Transform item table (g_nIT). Rows are item definition race.
     * Cols 0–4 STR/HTH/INT/WIS/DEX divisors; 5 MSPEED%; 6 ATK%; 7 MAGIC%;
     * 8–9 DEF min/max (young+); 10 resist-all (currently unused).
     */
    inline constexpr int kTransformIt[12][11] = {{2, 4, 0, 0, 6, 30, 4, 1, 36, 45, 60},
                                                 {3, 3, 0, 0, 4, 40, 4, 1, 45, 45, 60},
                                                 {4, 2, 0, 0, 6, 20, 3, 1, 45, 54, 80},
                                                 {4, 3, 0, 0, 3, 30, 3, 1, 54, 54, 80},
                                                 {4, 6, 0, 0, 2, 40, 4, 1, 18, 36, 100},
                                                 {3, 4, 0, 0, 3, 50, 4, 1, 27, 45, 100},
                                                 {0, 6, 2, 0, 4, 20, 1, 4, 27, 18, 80},
                                                 {0, 4, 3, 0, 3, 30, 1, 4, 36, 27, 80},
                                                 {0, 4, 2, 6, 0, 30, 1, 3, 27, 18, 100},
                                                 {0, 0, 3, 4, 3, 40, 1, 3, 36, 27, 120},
                                                 {4, 4, 0, 6, 4, 40, 4, 1, 45, 45, 80},
                                                 {0, 4, 4, 6, 4, 40, 1, 4, 36, 36, 100}};

    /** Rage required to enter each stage (g_nNeedRage). Index 3 is also the cap. */
    inline constexpr int kNeedRage[5] = {0, 34280, 137140, 308571, 613142};

    /** Rage drained every 5s while transformed (g_nDecRage). */
    inline constexpr int kDecRage[5] = {0, 357, 761, 857, 1000};

    /** Worn-egg primary bonuses from egg level and race. */
    struct TransformWornBonus {
        /** Strength points. */
        int str = 0;

        /** Health points. */
        int hth = 0;

        /** Intelligence points. */
        int intl = 0;

        /** Wisdom points. */
        int wis = 0;

        /** Dexterity points. */
        int dex = 0;
    };

    /** Combat bonuses applied while transformed. */
    struct TransformFormBonus {
        /** Move-speed percent from baby onward. */
        int mspeed_percent = 0;

        /** Physical attack percent from adult onward. */
        int attack_percent = 0;

        /** Magic attack percent from adult onward. */
        int magic_percent = 0;

        /** Defense min added from young onward. */
        int defense_min = 0;

        /** Defense max added from young onward. */
        int defense_max = 0;
    };

    /**
     * Animal EXP needed to leave `level`.
     * @param level Current egg level.
     * @return Table entry, or 0 when invalid.
     */
    [[nodiscard]] inline int need_transform_exp_for_level(int level) {
        if (level < 1 || level >= kMaxTransformLevel) {
            return 0;
        }
        return kNeedTransformExp[level];
    }

    /**
     * Spends overflow animal EXP into levels (no player-level gate).
     * @param level Current egg level (updated).
     * @param exp Current egg EXP (updated).
     * @return True when the egg level increased.
     */
    [[nodiscard]] inline bool consume_transform_exp_into_levels(int& level, int& exp) {
        bool leveled = false;
        if (level < 1) {
            level = 1;
        }
        if (exp < 0) {
            exp = 0;
        }
        while (level < kMaxTransformLevel) {
            const int need = need_transform_exp_for_level(level);
            if (need <= 0 || exp < need) {
                break;
            }
            exp -= need;
            ++level;
            leveled = true;
        }
        return leveled;
    }

    /**
     * Kill-path animal EXP add (compatibility behavior bTimer=FALSE).
     * @param player_level Owner character level.
     * @return 1, or (level-11)/10+1 when level > 20.
     */
    [[nodiscard]] inline int kill_transform_exp_add(int player_level) {
        if (player_level > 20) {
            return (player_level - 11) / 10 + 1;
        }
        return 1;
    }

    /**
     * Highest transform stage the egg unlocks.
     * @param egg_level Animal level.
     * @return 0–3 (baby at 11, young at 21, adult at 31).
     */
    [[nodiscard]] inline int max_transform_stage(int egg_level) {
        const int stage = (std::max(1, egg_level) - 1) / 10;
        return std::clamp(stage, 0, kTransLvAdult);
    }

    /**
     * Extra HyperText icon slot for a grown egg (related compatibility states).
     * Level 1–10 uses item definition Image; 11–20 / 21–30 / 31+ use imageex[0..2].
     * @param egg_level Animal level.
     * @return 0–2, or -1 to keep the base image.
     */
    [[nodiscard]] inline int transform_egg_image_ex_index(int egg_level) {
        const int level = std::max(1, egg_level);
        if (level >= 31) {
            return 2;
        }
        if (level >= 21) {
            return 1;
        }
        if (level >= 11) {
            return 0;
        }
        return -1;
    }

    /**
     * True when `stage` is unlocked for the egg.
     * @param egg_level Animal level.
     * @param stage 1 baby / 2 young / 3 adult.
     * @return True when stage <= (eggLevel-1)/10.
     */
    [[nodiscard]] inline bool transform_stage_unlocked(int egg_level, int stage) {
        return stage >= kTransLvBaby && stage <= kTransLvAdult &&
               stage <= max_transform_stage(egg_level);
    }

    /**
     * TFlag stored on equip (16 * race).
     * @param race item definition race (0/4/6/8).
     * @return High nibble race flag.
     */
    [[nodiscard]] inline int transform_equip_tflag(int race) {
        return 16 * std::max(0, race);
    }

    /**
     * Race nibble of a TFlag.
     * @param tflag Packed flag.
     * @return Race (TFlag >> 4).
     */
    [[nodiscard]] inline int transform_tflag_race(int tflag) {
        return (tflag >> 4) & 0x0f;
    }

    /**
     * Stage nibble of a TFlag.
     * @param tflag Packed flag.
     * @return Stage (TFlag & 15).
     */
    [[nodiscard]] inline int transform_tflag_stage(int tflag) {
        return tflag & 0x0f;
    }

    /**
     * Skill clazz used for animal JSON (gunchi/yungsang/tuguk/gale).
     * @param race item definition race.
     * @return 10 + race.
     */
    [[nodiscard]] inline int animal_skill_class(int race) {
        return kSkillClassAnimalBase + std::max(0, race);
    }

    /**
     * Transform-into skill index for a stage.
     * @param stage 1–3.
     * @return 200–202, or 0 when invalid.
     */
    [[nodiscard]] inline int transform_into_skill(int stage) {
        if (stage == kTransLvBaby) {
            return kSkillTransformBaby;
        }
        if (stage == kTransLvYoung) {
            return kSkillTransformYoung;
        }
        if (stage == kTransLvAdult) {
            return kSkillTransformAdult;
        }
        return 0;
    }

    /**
     * Stage implied by a transform-into skill.
     * @param skill 200–202.
     * @return 1–3, or 0.
     */
    [[nodiscard]] inline int transform_skill_stage(int skill) {
        if (skill == kSkillTransformBaby) {
            return kTransLvBaby;
        }
        if (skill == kSkillTransformYoung) {
            return kTransLvYoung;
        }
        if (skill == kSkillTransformAdult) {
            return kTransLvAdult;
        }
        return 0;
    }

    /**
     * True when the skill is a transform-into skill.
     * @param skill Skill index.
     * @return True for 200–202.
     */
    [[nodiscard]] inline bool is_transform_into_skill(int skill) {
        return transform_skill_stage(skill) != 0;
    }

    /**
     * True when the skill is an animal skill (101–112).
     * @param skill Skill index.
     * @return True for 101–112.
     */
    [[nodiscard]] inline bool is_animal_skill(int skill) {
        return skill >= 101 && skill <= 112;
    }

    /**
     * True when an animal skill is self-cast (no monster target).
     * @param skill Skill index 101–112.
     * @return True for Phoenix, Rage, Adamant, Hiding, and Shinrhoe.
     */
    [[nodiscard]] inline bool animal_skill_is_self(int skill) {
        return skill == 103 || skill == 106 || skill == 109 || skill == 110 || skill == 112;
    }

    /** Worn-egg stat bonus. Formula is `eggLevel / factor + 1` when factor > 0.
     *
     * TODO later: Review worn-egg stat behavior from level 40 and the EXP bonus
     * from level 68. These higher-level rules remain unverified.
     *
     * @param race item definition race.
     * @param egg_level Animal level.
     * @return Primary bonuses.
     */
    [[nodiscard]] inline TransformWornBonus worn_transform_bonus(int race, int egg_level) {
        TransformWornBonus bonus;
        if (race < 0 || race >= 12) {
            return bonus;
        }
        const int level = std::max(1, egg_level);
        auto apply = [&](int col, int& out) {
            const int factor = kTransformIt[race][col];
            if (factor > 0) {
                out = level / factor + 1;
            }
        };
        apply(0, bonus.str);
        apply(1, bonus.hth);
        apply(2, bonus.intl);
        apply(3, bonus.wis);
        apply(4, bonus.dex);
        return bonus;
    }

    /**
     * Combat bonuses while transformed.
     * @param race item definition race.
     * @param stage 1–3.
     * @return Speed / attack / defense percents and points.
     */
    [[nodiscard]] inline TransformFormBonus form_transform_bonus(int race, int stage) {
        TransformFormBonus bonus;
        if (race < 0 || race >= 12 || stage < kTransLvBaby) {
            return bonus;
        }
        bonus.mspeed_percent = kTransformIt[race][5];
        if (stage >= kTransLvYoung) {
            bonus.defense_min = kTransformIt[race][8];
            bonus.defense_max = kTransformIt[race][9];
        }
        if (stage >= kTransLvAdult) {
            bonus.attack_percent = kTransformIt[race][6];
            bonus.magic_percent = kTransformIt[race][7];
        }
        return bonus;
    }

    /**
     * Rage cap (adult need).
     * @return g_nNeedRage[3].
     */
    [[nodiscard]] inline int max_rage() {
        return kNeedRage[3];
    }

    /**
     * Rage required to enter a stage.
     * @param stage 1–3.
     * @return Need, or 0 when invalid.
     */
    [[nodiscard]] inline int need_rage_for_stage(int stage) {
        if (stage < 1 || stage > 4) {
            return 0;
        }
        return kNeedRage[stage];
    }

    /**
     * Rage drained every 5s at a stage.
     * @param stage 1–3.
     * @return Drain, or 0 when invalid.
     */
    [[nodiscard]] inline int dec_rage_for_stage(int stage) {
        if (stage < 1 || stage > 4) {
            return 0;
        }
        return kDecRage[stage];
    }

    /**
     * Client percent for the H-window EXP bar.
     * The compatibility full-size value is 7200 + (level² * 960) / 2; use the
     * server table for the ratio instead.
     * @param current Current egg EXP.
     * @param need EXP needed for the next level.
     * @return 0–100.
     */
    [[nodiscard]] inline double transform_exp_percent(int current, int need) {
        if (need <= 0) {
            return 0.0;
        }
        if (current <= 0) {
            return 0.0;
        }
        if (current >= need) {
            return 100.0;
        }
        return 100.0 * static_cast<double>(current) / static_cast<double>(need);
    }

    /** Encodes and decodes C2S_TRANSFORM / S2C transform actions. */
    class Transform final {
      public:
        Transform() = delete;

        /**
         * Frames C2S_TRANSFORM "bb".
         * @param flag TRANS_M_PRE / EXECUTE / FREE.
         * @param level Stage 1–3 (ignored for FREE).
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint8_t flag,
                                                                 std::uint8_t level);

        /**
         * Parses C2S_TRANSFORM "bb".
         * @param data Payload.
         * @param size Size.
         * @param flag Out flag.
         * @param level Out stage.
         * @return True on success.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint8_t& flag,
                                            std::uint8_t& level);

        /**
         * Frames C2S_TRANSFORMSKILL "b".
         * @param skill Animal skill index 101–112.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s_skill(std::uint8_t skill);

        /**
         * Parses C2S_TRANSFORMSKILL "b".
         * @param data Payload.
         * @param size Size.
         * @param skill Out skill index.
         * @return True on success.
         */
        [[nodiscard]] static bool
        parse_c2s_skill(const std::uint8_t* data, std::size_t size, std::uint8_t& skill);

        /**
         * Frames S2C_ACTION AT_PRETRANSFORM "dbb".
         * @param entity_id Caster id.
         * @param stage Stage 1–3.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_pre(std::uint32_t entity_id,
                                                                        std::uint8_t stage);

        /**
         * Frames S2C_ACTION AT_TRANSFORM "dbb".
         * @param entity_id Caster id.
         * @param tflag Packed TFlag (16*race | stage), or 0 to revert.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_form(std::uint32_t entity_id,
                                                                         std::uint8_t tflag);
    };

}  // namespace openkal::protocol
