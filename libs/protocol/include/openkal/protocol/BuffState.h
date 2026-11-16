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
#include <string_view>
#include <vector>

namespace openkal::protocol {

    /** Message identifiers for BState synchronization. */
    enum class BStateMsg : std::uint8_t {
        AddBState = 74,
        SubBState = 75,
    };

    /**
     * Item specialty buff IDs.
     * Values match item definition specialty.buff[0].
     */
    enum class BuffType : int {
        /** Timed percent bonus to physical attack (specialty.buff[0], amount in buff[2]). */
        AttackFood = 5,
        /** Timed percent bonus to magical attack (specialty.buff[0], amount in buff[2]). */
        MagicAttackFood = 6,
        /** Haste / Speed-up Medicine (compatibility behavior / specialty.buff[0]). */
        Haste = 12,
        /** Timed inventory register unlock (specialty.buff[0]). */
        InventoryUnlock = 100,
        /** Sit HP food (compatibility behavior / specialty.buff[0]). */
        Hp = 18,
        /** Sit MP food (compatibility behavior / specialty.buff[0]). */
        Mp = 19,
        PayUpAttack = 51,
        PayCutDamage = 52,
        PayUpXBlow3 = 53,
        PayUpXBlow5 = 54,
        PayUpHit = 56,
        PayUpDodge = 57,
        PayPerfectDef = 58,
        PayDrainHp = 59,
        /** Stone of EXP (specialty.buff[0]). */
        StoneExp = 101,
        /** Stone of Wealth (specialty.buff[0]). */
        StoneWealth = 102,
        /** Stone of EXP for Ancient Animal (specialty.buff[0]). */
        StoneAnimalExp = 103,
        /** Stone of Fishery (specialty.buff[0]). */
        StoneFishery = 104,
    };

    /** Haste/run movement-state bit. */
    inline constexpr std::uint64_t kCmsBoost = 0x00000001ull;

    /** Ice mastery movement-slow state bit. */
    inline constexpr std::uint64_t kCmsMoveSpeed = 0x00000002ull;

    /** Cold Binding / Frost movement-stop state bit. */
    inline constexpr std::uint64_t kCmsMoveStop = 0x00000200ull;

    /** Seated HP food recovery state bit. */
    inline constexpr std::uint64_t kCmsHp = 0x00100000ull;

    /** Seated MP food recovery state bit. */
    inline constexpr std::uint64_t kCmsMp = 0x00200000ull;

    /** compatibility behavior SetBuff index for compatibility behavior (macro-e skillbuff 28). */
    inline constexpr int kSkillBuffKeyRestHp = 28;

    /** compatibility behavior SetBuff index for compatibility behavior (macro-e skillbuff 29). */
    inline constexpr int kSkillBuffKeyRestMp = 29;

    /** Sys-message id for sit HP recover (SetBuff 28, 671). */
    inline constexpr int kSysRestHp = 671;

    /** Sys-message id for sit MP recover (SetBuff 29, 672). */
    inline constexpr int kSysRestMp = 672;

    /** HyperText icon for sit HP/MP food (macro-e skillbuff 28/29 image buff034). */
    inline constexpr std::string_view kRestFoodIcon = "buff034.bmp";

    /** Stone of EXP (+30% monster EXP). */
    inline constexpr std::uint64_t kCbsStoneExp = 0x00000002ull;
    /** Global community EXP event. This unused legacy bit only drives the HUD. */
    inline constexpr std::uint64_t kCbsExpEvent = 0x00000004ull;
    /** OpenKal state for Stone of Wealth's monster-money bonus. */
    inline constexpr std::uint64_t kCbsStoneWealth = 0x00000008ull;
    /** OpenKal state for Stone of Fishery's shorter fishing cycle. */
    inline constexpr std::uint64_t kCbsStoneFishery = 0x00000010ull;

    /** Character BState bitmasks for pay scrolls. */
    /** Timed physical-attack food (specialty.buff[0] = 5). */
    inline constexpr std::uint64_t kCbsFoodAttack = 0x04000000ull;
    /** Timed magical-attack food (specialty.buff[0] = 6). */
    inline constexpr std::uint64_t kCbsFoodMagic = 0x08000000ull;

    inline constexpr std::uint64_t kCbsPayUpAttack = 0x00000200ull;
    inline constexpr std::uint64_t kCbsPayCutDamage = 0x00000400ull;
    inline constexpr std::uint64_t kCbsPayUpXBlow3 = 0x00000800ull;
    inline constexpr std::uint64_t kCbsPayUpXBlow5 = 0x00001000ull;
    inline constexpr std::uint64_t kCbsPayUpHit = 0x00004000ull;
    inline constexpr std::uint64_t kCbsPayUpDodge = 0x00008000ull;
    inline constexpr std::uint64_t kCbsPayPerfectDef = 0x00010000ull;
    inline constexpr std::uint64_t kCbsPayDrainHp = 0x00020000ull;
    /** Archer Sharpness (BUFF_FLAGMSPEED). Unused compatibility behavior gap. */
    inline constexpr std::uint64_t kCbsSkillArcherSharpness = 0x00040000ull;

    /** Yungsang Rage (animal skill 106). Separate from Call of On Target Point. */
    inline constexpr std::uint64_t kCbsSkillAnimalRage = 0x00080000ull;

    /** Dance of Killing (Thief skill 13). */
    inline constexpr std::uint64_t kCbsSkillThiefDanceOfKilling = 0x00100000ull;

    /** Timed inventory register unlocks for registers 3 through 6. */
    inline constexpr std::uint64_t kCbsInventoryRegister3 = 0x00400000ull;
    inline constexpr std::uint64_t kCbsInventoryRegister4 = 0x00800000ull;
    inline constexpr std::uint64_t kCbsInventoryRegister5 = 0x01000000ull;
    inline constexpr std::uint64_t kCbsInventoryRegister6 = 0x02000000ull;
    inline constexpr std::uint64_t kCbsInventoryRegisterMask =
        kCbsInventoryRegister3 | kCbsInventoryRegister4 | kCbsInventoryRegister5 |
        kCbsInventoryRegister6;

    /**
     * Maps an additional inventory register index (0 = register 3) to its compatibility behavior
     * bit.
     * @param index Additional inventory register index.
     * @return compatibility behavior bit, or 0 for an invalid index.
     */
    [[nodiscard]] constexpr std::uint64_t inventory_register_bstate(int index) {
        switch (index) {
        case 0:
            return kCbsInventoryRegister3;
        case 1:
            return kCbsInventoryRegister4;
        case 2:
            return kCbsInventoryRegister5;
        case 3:
            return kCbsInventoryRegister6;
        default:
            return 0;
        }
    }

    /**
     * Maps an inventory-register compatibility behavior bit to its additional-register index.
     * @param bstate Single inventory-register compatibility behavior bit.
     * @return Additional register index, or -1 when the bit is unrelated.
     */
    [[nodiscard]] constexpr int inventory_register_index(std::uint64_t bstate) {
        switch (bstate) {
        case kCbsInventoryRegister3:
            return 0;
        case kCbsInventoryRegister4:
            return 1;
        case kCbsInventoryRegister5:
            return 2;
        case kCbsInventoryRegister6:
            return 3;
        default:
            return -1;
        }
    }

    /** Skill buff compatibility behavior bits (high range; parallel to pay compatibility behavior,
     * not pay scrolls). */
    inline constexpr std::uint64_t kCbsSkillStone = 0x000100000000ull;
    inline constexpr std::uint64_t kCbsSkillHaste = 0x000200000000ull;
    inline constexpr std::uint64_t kCbsSkillMeditation = 0x000400000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightDefense = 0x000800000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightHealing = 0x001000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightLightning = 0x002000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightIce = 0x004000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightFlame = 0x008000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightAccuracy = 0x010000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightBattle = 0x020000000000ull;
    inline constexpr std::uint64_t kCbsSkillKnightBerserk = 0x040000000000ull;
    /** Increasing Concentration (BUFF_HITUP / WildAccuracy). */
    inline constexpr std::uint64_t kCbsSkillKnightConcentration = 0x0001000000000000ull;
    /** Refining Weapon (BUFF_INCHANTWEAPON). */
    inline constexpr std::uint64_t kCbsSkillRefiningWeapon = 0x0002000000000000ull;
    /** Defense Improvement (BUFF_AURADEFENSE). */
    inline constexpr std::uint64_t kCbsSkillAuraDefense = 0x0004000000000000ull;
    /** Blessing of Strength. */
    inline constexpr std::uint64_t kCbsSkillBlessStrength = 0x0008000000000000ull;
    /** Blessing of Health. */
    inline constexpr std::uint64_t kCbsSkillBlessHealth = 0x0010000000000000ull;
    /** Blessing of Agility. */
    inline constexpr std::uint64_t kCbsSkillBlessAgility = 0x0020000000000000ull;
    /** Blessing of Intelligence. */
    inline constexpr std::uint64_t kCbsSkillBlessIntelligence = 0x0040000000000000ull;
    /** Increase of Critical Hit. */
    inline constexpr std::uint64_t kCbsSkillBlessCritical = 0x0080000000000000ull;
    /** Force of Defense. */
    inline constexpr std::uint64_t kCbsSkillKnightForceDefense = 0x0100000000000000ull;
    /** Protect. */
    inline constexpr std::uint64_t kCbsSkillKnightProtect = 0x0200000000000000ull;
    /** Elastic Shield. */
    inline constexpr std::uint64_t kCbsSkillKnightElasticShield = 0x0400000000000000ull;
    /** Perfect Defense. */
    inline constexpr std::uint64_t kCbsSkillKnightPerfectDefense = 0x0800000000000000ull;
    inline constexpr std::uint64_t kCbsSkillResistFire = 0x080000000000ull;
    inline constexpr std::uint64_t kCbsSkillResistIce = 0x100000000000ull;
    inline constexpr std::uint64_t kCbsSkillResistLightning = 0x200000000000ull;
    inline constexpr std::uint64_t kCbsSkillEscape = 0x400000000000ull;
    inline constexpr std::uint64_t kCbsSkillHiding = 0x800000000000ull;
    /** Archer Chance Capture. */
    inline constexpr std::uint64_t kCbsSkillArcherChanceCapture = 0x1000000000000000ull;
    /** Archer Muscle Solidation. */
    inline constexpr std::uint64_t kCbsSkillArcherMuscleSolidation = 0x2000000000000000ull;
    /** Archer Curse Resistance. */
    inline constexpr std::uint64_t kCbsSkillResistCurse = 0x4000000000000000ull;
    /** Archer Non-Attribution / Spirit Resistance. */
    inline constexpr std::uint64_t kCbsSkillResistSpirit = 0x8000000000000000ull;

    /**
     * SCREEN skillbuff keys (KBStateSystem::GetBuffIndex).
     * Macro image names are shopbuff_004…016, not shopbuff_060.
     */
    inline constexpr int kSkillBuffKeyPayUpAttack = 60;
    inline constexpr int kSkillBuffKeyPayCutDamage = 61;
    inline constexpr int kSkillBuffKeyPayUpXBlow3 = 62;
    inline constexpr int kSkillBuffKeyPayUpXBlow5 = 63;
    inline constexpr int kSkillBuffKeyPayUpHit = 64;
    inline constexpr int kSkillBuffKeyPayUpDodge = 65;
    inline constexpr int kSkillBuffKeyPayPerfectDef = 66;
    inline constexpr int kSkillBuffKeyPayDrainHp = 72;

    /** Sys-message ids for skillbuff tooltip names (macro-e name field). */
    inline constexpr int kSysPayUpAttack = 919;
    inline constexpr int kSysPayCutDamage = 920;
    inline constexpr int kSysPayUpXBlow3 = 921;
    inline constexpr int kSysPayUpXBlow5 = 922;
    inline constexpr int kSysPayUpHit = 923;
    inline constexpr int kSysPayUpDodge = 924;
    inline constexpr int kSysPayPerfectDef = 925;
    inline constexpr int kSysPayDrainHp = 931;

    /** Timed buff may be refreshed when remain is at most this many seconds. */
    inline constexpr int kPayBuffRefreshWindowSec = 60;

    /** Blink icon when timed remain is below this many milliseconds (ChkDrawSkillBuff). */
    inline constexpr int kPayBuffBlinkMs = 10000;

    /** Blink count buffs when charges are at or below this (ChkDrawSkillBuff TYPE_NUMBER). */
    inline constexpr int kPayBuffBlinkCharges = 10;

    /** Maximum stacked charges for Perfect Defense and Drain HP. */
    inline constexpr int kPayBuffChargeCap = 9999;

    /**
     * Maps a pay BuffType to its compatibility behavior bit.
     * @param type Buff type from item specialty.
     * @return compatibility behavior bitmask, or 0 when unknown.
     */
    [[nodiscard]] constexpr std::uint64_t buff_type_to_bstate(BuffType type) {
        switch (type) {
        case BuffType::AttackFood:
            return kCbsFoodAttack;
        case BuffType::MagicAttackFood:
            return kCbsFoodMagic;
        case BuffType::PayUpAttack:
            return kCbsPayUpAttack;
        case BuffType::PayCutDamage:
            return kCbsPayCutDamage;
        case BuffType::PayUpXBlow3:
            return kCbsPayUpXBlow3;
        case BuffType::PayUpXBlow5:
            return kCbsPayUpXBlow5;
        case BuffType::PayUpHit:
            return kCbsPayUpHit;
        case BuffType::PayUpDodge:
            return kCbsPayUpDodge;
        case BuffType::PayPerfectDef:
            return kCbsPayPerfectDef;
        case BuffType::PayDrainHp:
            return kCbsPayDrainHp;
        case BuffType::StoneExp:
            return kCbsStoneExp;
        case BuffType::StoneWealth:
            return kCbsStoneWealth;
        case BuffType::StoneFishery:
            return kCbsStoneFishery;
        case BuffType::Haste:
        case BuffType::InventoryUnlock:
        case BuffType::Hp:
        case BuffType::Mp:
        case BuffType::StoneAnimalExp:
            return 0;
        }
        return 0;
    }

    /**
     * True when remain is a charge count (Perfect Def / HP Absorb).
     * @param type Buff type.
     * @return True for count-based pay buffs.
     */
    [[nodiscard]] constexpr bool buff_type_is_count(BuffType type) {
        return type == BuffType::PayPerfectDef || type == BuffType::PayDrainHp;
    }

    /**
     * Maps a compatibility behavior bit to the SCREEN skillbuff key (GetBuffIndex).
     * @param bstate Single compatibility behavior bit (or combined; first match wins).
     * @return Skillbuff key, or 0 when unknown.
     */
    [[nodiscard]] constexpr int bstate_to_skillbuff_key(std::uint64_t bstate) {
        if (bstate & kCbsPayUpAttack) {
            return kSkillBuffKeyPayUpAttack;
        }
        if (bstate & kCbsPayCutDamage) {
            return kSkillBuffKeyPayCutDamage;
        }
        if (bstate & kCbsPayUpXBlow3) {
            return kSkillBuffKeyPayUpXBlow3;
        }
        if (bstate & kCbsPayUpXBlow5) {
            return kSkillBuffKeyPayUpXBlow5;
        }
        if (bstate & kCbsPayUpHit) {
            return kSkillBuffKeyPayUpHit;
        }
        if (bstate & kCbsPayUpDodge) {
            return kSkillBuffKeyPayUpDodge;
        }
        if (bstate & kCbsPayPerfectDef) {
            return kSkillBuffKeyPayPerfectDef;
        }
        if (bstate & kCbsPayDrainHp) {
            return kSkillBuffKeyPayDrainHp;
        }
        return 0;
    }

    /**
     * HyperText icon stem for a SCREEN skillbuff key (macro-e image field).
     * @param skillbuff_key GetBuffIndex SCREEN key (60–66, 72).
     * @return Filename without path, e.g. shopbuff_004.bmp, or empty.
     */
    [[nodiscard]] constexpr std::string_view skillbuff_key_to_icon(int skillbuff_key) {
        switch (skillbuff_key) {
        case kSkillBuffKeyPayUpAttack:
            return "shopbuff_004.bmp";
        case kSkillBuffKeyPayCutDamage:
            return "shopbuff_005.bmp";
        case kSkillBuffKeyPayUpXBlow3:
            return "shopbuff_006.bmp";
        case kSkillBuffKeyPayUpXBlow5:
            return "shopbuff_007.bmp";
        case kSkillBuffKeyPayUpHit:
            return "shopbuff_008.bmp";
        case kSkillBuffKeyPayUpDodge:
            return "shopbuff_009.bmp";
        case kSkillBuffKeyPayPerfectDef:
            return "shopbuff_010.bmp";
        case kSkillBuffKeyPayDrainHp:
            return "shopbuff_016.bmp";
        default:
            return {};
        }
    }

    /**
     * Sys-message id used as skillbuff tooltip title (macro-e name).
     * @param bstate compatibility behavior bit.
     * @return Interface label id, or 0.
     */
    [[nodiscard]] constexpr int bstate_to_sys_name(std::uint64_t bstate) {
        if (bstate & kCbsPayUpAttack) {
            return kSysPayUpAttack;
        }
        if (bstate & kCbsPayCutDamage) {
            return kSysPayCutDamage;
        }
        if (bstate & kCbsPayUpXBlow3) {
            return kSysPayUpXBlow3;
        }
        if (bstate & kCbsPayUpXBlow5) {
            return kSysPayUpXBlow5;
        }
        if (bstate & kCbsPayUpHit) {
            return kSysPayUpHit;
        }
        if (bstate & kCbsPayUpDodge) {
            return kSysPayUpDodge;
        }
        if (bstate & kCbsPayPerfectDef) {
            return kSysPayPerfectDef;
        }
        if (bstate & kCbsPayDrainHp) {
            return kSysPayDrainHp;
        }
        return 0;
    }

    /**
     * Maps a compatibility behavior bit to the left-HUD icon filename (via skillbuff key → macro
     * image).
     * @param bstate Single compatibility behavior bit (or combined; first match wins).
     * @return HyperText bmp name, or empty when unknown.
     */
    [[nodiscard]] constexpr std::string_view bstate_to_shopbuff_icon(std::uint64_t bstate) {
        return skillbuff_key_to_icon(bstate_to_skillbuff_key(bstate));
    }

    /**
     * Maps pay or skill compatibility behavior bits to a BuffSlotBar icon filename.
     * @param bstate compatibility behavior bitmask.
     * @return HyperText bmp name, or empty when unknown.
     */
    [[nodiscard]] constexpr std::string_view bstate_to_buff_icon(std::uint64_t bstate) {
        const std::string_view shop = bstate_to_shopbuff_icon(bstate);
        if (!shop.empty()) {
            return shop;
        }
        if (bstate & kCbsExpEvent) {
            return "expEvent.bmp";
        }
        if (bstate & kCbsSkillStone) {
            return "active015.bmp";
        }
        if (bstate & kCbsSkillHaste) {
            return "buff021.bmp";
        }
        if (bstate & kCbsSkillMeditation) {
            return "active017.bmp";
        }
        if (bstate & kCbsSkillKnightForceDefense) {
            return "active060.bmp";
        }
        if (bstate & kCbsSkillKnightDefense) {
            return "active061.bmp";
        }
        if (bstate & kCbsSkillKnightHealing) {
            return "active025.bmp";
        }
        if (bstate & kCbsSkillKnightLightning) {
            return "active096.bmp";
        }
        if (bstate & kCbsSkillKnightIce) {
            return "active097.bmp";
        }
        if (bstate & kCbsSkillKnightFlame) {
            return "active098.bmp";
        }
        if (bstate & kCbsSkillKnightAccuracy) {
            return "active033.bmp";
        }
        if (bstate & kCbsSkillKnightBattle) {
            return "active034.bmp";
        }
        if (bstate & kCbsSkillKnightBerserk) {
            return "active093.bmp";
        }
        if (bstate & kCbsSkillKnightProtect) {
            return "active040.bmp";
        }
        if (bstate & kCbsSkillKnightElasticShield) {
            return "active102.bmp";
        }
        if (bstate & kCbsSkillKnightPerfectDefense) {
            return "active125.bmp";
        }
        if (bstate & kCbsSkillKnightConcentration) {
            return "active033.bmp";
        }
        if (bstate & kCbsSkillRefiningWeapon) {
            return "active065.bmp";
        }
        if (bstate & kCbsSkillAuraDefense) {
            return "active066.bmp";
        }
        if (bstate & kCbsSkillBlessStrength) {
            return "active067.bmp";
        }
        if (bstate & kCbsSkillBlessHealth) {
            return "active068.bmp";
        }
        if (bstate & kCbsSkillBlessAgility) {
            return "active069.bmp";
        }
        if (bstate & kCbsSkillBlessIntelligence) {
            return "active070.bmp";
        }
        if (bstate & kCbsSkillBlessCritical) {
            return "active071.bmp";
        }
        if (bstate & kCbsSkillResistFire) {
            return "active040.bmp";
        }
        if (bstate & kCbsSkillResistIce) {
            return "active041.bmp";
        }
        if (bstate & kCbsSkillResistLightning) {
            return "active042.bmp";
        }
        if (bstate & kCbsSkillEscape) {
            return "active046.bmp";
        }
        if (bstate & kCbsSkillArcherSharpness) {
            return "active154.bmp";
        }
        if (bstate & kCbsSkillAnimalRage) {
            return "active046.bmp";
        }
        if (bstate & kCbsSkillThiefDanceOfKilling) {
            return "active140.bmp";
        }
        if (bstate & kCbsSkillHiding) {
            return "active070.bmp";
        }
        if (bstate & kCbsSkillArcherChanceCapture) {
            return "passive011.bmp";
        }
        if (bstate & kCbsSkillArcherMuscleSolidation) {
            return "active036.bmp";
        }
        if (bstate & kCbsSkillResistCurse) {
            return "active107.bmp";
        }
        if (bstate & kCbsSkillResistSpirit) {
            return "active108.bmp";
        }
        return {};
    }

    /**
     * Maps a class skill index to the compatibility behavior bit used for that skill buff.
     * Keep in sync with SkillBuffSet::buff_id_for_skill / bstate_for.
     * @param clazz Class index (0 knight / 1 magician / 2 archer / 3 thief).
     * @param skill_index Skill index within the class.
     * @return compatibility behavior bit, or 0 when the skill has no client buff icon.
     */
    [[nodiscard]] constexpr std::uint64_t skill_to_bstate(int clazz, int skill_index) {
        if (skill_index == 106) {
            return kCbsSkillAnimalRage;
        }
        if (skill_index == 110) {
            return kCbsSkillHiding;
        }
        if (clazz == 1) {
            if ((skill_index >= 17 && skill_index <= 21) || skill_index == 63) {
                return kCbsSkillStone;
            }
            switch (skill_index) {
            case 15:
                return kCbsSkillMeditation;
            case 26:
                return kCbsSkillHaste;
            case 38:
                return kCbsSkillRefiningWeapon;
            case 39:
                return kCbsSkillAuraDefense;
            case 49:
                return kCbsSkillBlessStrength;
            case 50:
                return kCbsSkillBlessHealth;
            case 51:
                return kCbsSkillBlessAgility;
            case 52:
                return kCbsSkillBlessIntelligence;
            case 53:
                return kCbsSkillBlessCritical;
            default:
                break;
            }
        } else if (clazz == 0) {
            switch (skill_index) {
            case 10:
                return kCbsSkillKnightConcentration;
            case 14:
                return kCbsSkillKnightProtect;
            case 18:
                return kCbsSkillKnightForceDefense;
            case 19:
                return kCbsSkillKnightDefense;
            case 23:
                return kCbsSkillKnightHealing;
            case 24:
                return kCbsSkillKnightBerserk;
            case 27:
                return kCbsSkillKnightLightning;
            case 28:
                return kCbsSkillKnightIce;
            case 29:
                return kCbsSkillKnightFlame;
            case 31:
                return kCbsSkillKnightAccuracy;
            case 32:
                return kCbsSkillKnightBattle;
            case 34:
                return kCbsSkillKnightElasticShield;
            case 40:
                return kCbsSkillKnightPerfectDefense;
            default:
                break;
            }
        } else if (clazz == 2) {
            switch (skill_index) {
            case 7:
                return kCbsSkillArcherChanceCapture;
            case 10:
                return kCbsSkillArcherMuscleSolidation;
            case 24:
                return kCbsSkillResistLightning;
            case 25:
                return kCbsSkillResistIce;
            case 26:
                return kCbsSkillResistFire;
            case 28:
                return kCbsSkillResistCurse;
            case 29:
                return kCbsSkillResistSpirit;
            case 31:
                return kCbsSkillEscape;
            case 46:
                return kCbsSkillArcherSharpness;
            default:
                break;
            }
        } else if (clazz == 3) {
            if (skill_index == 6) {
                return kCbsSkillHiding;
            }
            if (skill_index == 13) {
                return kCbsSkillThiefDanceOfKilling;
            }
        }
        return 0;
    }

    /**
     * FX shader name for S2C_EFFECT Charm1–Charm8 (E_CHARM_*).
     * @param effect EffectType ordinal 4–11.
     * @return FX name without path, or empty when not a charm effect.
     */
    [[nodiscard]] std::string_view charm_effect_fx_name(std::uint8_t effect);

    /**
     * FX shader name for S2C_EFFECT E_HP / E_MP (medicine and instant refresh).
     * @param effect EffectType ordinal 1–2.
     * @return FX name without path, or empty when not a refresh effect.
     */
    [[nodiscard]] std::string_view refresh_effect_fx_name(std::uint8_t effect);

    /**
     * FX shader name for oneshot S2C_EFFECT attachments (charm or refresh).
     * @param effect EffectType ordinal.
     * @return FX name without path, or empty when unknown.
     */
    [[nodiscard]] std::string_view oneshot_effect_fx_name(std::uint8_t effect);

    /**
     * True when specialty.buff[0] is sit HP/MP food (BUFF_HP / BUFF_MP).
     * @param buff_type item definition specialty.buff[0].
     * @return True for types 18 and 19.
     */
    [[nodiscard]] constexpr bool buff_type_is_rest_food(int buff_type) {
        return buff_type == static_cast<int>(BuffType::Hp) ||
               buff_type == static_cast<int>(BuffType::Mp);
    }

    /**
     * True when specialty.buff[0] is a timed attack-percent food.
     * @param buff_type specialty.buff[0].
     * @return True for physical (5) and magical (6) attack food.
     */
    [[nodiscard]] constexpr bool buff_type_is_attack_food(int buff_type) {
        return buff_type == static_cast<int>(BuffType::AttackFood) ||
               buff_type == static_cast<int>(BuffType::MagicAttackFood);
    }

    /**
     * Icon filename for sit HP/MP compatibility behavior bits.
     * @param mstate compatibility behavior bitmask.
     * @return HyperText bmp name, or empty.
     */
    [[nodiscard]] constexpr std::string_view mstate_to_rest_food_icon(std::uint64_t mstate) {
        if ((mstate & kCmsHp) != 0 || (mstate & kCmsMp) != 0) {
            return kRestFoodIcon;
        }
        return {};
    }

    /**
     * Sys-message id for sit HP/MP compatibility behavior bits.
     * @param mstate compatibility behavior bitmask.
     * @return Interface label id, or 0.
     */
    [[nodiscard]] constexpr int mstate_to_rest_food_sys(std::uint64_t mstate) {
        if (mstate & kCmsHp) {
            return kSysRestHp;
        }
        if (mstate & kCmsMp) {
            return kSysRestMp;
        }
        return 0;
    }

    /** Packet helpers for MSG_ADDBSTATE / MSG_SUBBSTATE via S2C_MESSAGEV. */
    class BuffState final {
      public:
        BuffState() = delete;

        /**
         * Builds S2C_MESSAGEV "bId" plus optional source ids for HUD icons.
         * @param bstate compatibility behavior bitmask.
         * @param remain Seconds remaining, or charge count for Perfect Def / Drain.
         * @param skill_index Source skill index (0 when unknown).
         * @param item_index Source item index (0 when unknown).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_add(std::uint64_t bstate,
                                                                 std::int32_t remain,
                                                                 std::int32_t skill_index = 0,
                                                                 std::int32_t item_index = 0);

        /**
         * Builds S2C_MESSAGEV "bI" (MSG_SUBBSTATE, bstate).
         * @param bstate compatibility behavior bitmask to clear.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_sub(std::uint64_t bstate);

        /**
         * Parses MSG_ADDBSTATE payload after the message byte has been read.
         * @param data Remaining payload (I + d [+ d skill index] [+ d item index]).
         * @param size Remaining size.
         * @param bstate Out compatibility behavior bitmask.
         * @param remain Out seconds or charges.
         * @param skill_index Out source skill index (0 when absent).
         * @param item_index Out source item index (0 when absent).
         * @return True on success.
         */
        [[nodiscard]] static bool parse_add(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint64_t& bstate,
                                            std::int32_t& remain,
                                            std::int32_t& skill_index,
                                            std::int32_t& item_index);

        /**
         * Parses MSG_SUBBSTATE payload after the message byte has been read.
         * @param data Remaining payload (I).
         * @param size Remaining size.
         * @param bstate Out compatibility behavior bitmask.
         * @return True on success.
         */
        [[nodiscard]] static bool
        parse_sub(const std::uint8_t* data, std::size_t size, std::uint64_t& bstate);
    };

}  // namespace openkal::protocol
