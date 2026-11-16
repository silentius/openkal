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
#include <cstdint>

namespace openkal::protocol {

    /** Per-instance random equipment modifier stored after INSERTITEM charm fields. */
    enum class RandomItemStatType : std::uint8_t {
        None = 0,
        WeaponMinPhysicalDamagePercent,
        WeaponMinMagicalDamagePercent,
        WeaponMaxPhysicalDamagePercent,
        WeaponMaxMagicalDamagePercent,
        WeaponPhysicalDamagePercent,
        WeaponMagicalDamagePercent,
        ArmorStrength,
        ArmorHealth,
        ArmorIntelligence,
        ArmorWisdom,
        ArmorDexterity,
        ArmorDefense,
        ArmorDefensePercent,
        ArmorDefensePercentStrength,
        ArmorDefensePercentHealth,
        ArmorDefensePercentIntelligence,
        ArmorDefensePercentWisdom,
        ArmorDefensePercentDexterity,
        ArmorDefensePercentDefense,
    };

    /** S2C_INSERTITEM info bit 0: replace the existing iid count. */
    inline constexpr std::uint32_t kInsertReplace = 1u;

    /** S2C_INSERTITEM info bit 1: endurance bytes are packed into info. */
    inline constexpr std::uint32_t kInsertHasEndurance = 2u;

    /**
     * S2C_INSERTITEM info bit 2: instance is worn (ITEM_PUTON).
     * Bit 0 is kInsertReplace in this protocol, so PUTON uses bit 2.
     */
    inline constexpr std::uint32_t kInsertPutOn = 4u;

    /** S2C_INSERTITEM info bit 3: this instance is permanently account-bound. */
    inline constexpr std::uint32_t kInsertAccountBound = 8u;

    /** S2C_INSERTITEM info bit 4: protects one enchantment attempt. */
    inline constexpr std::uint32_t kInsertEnchantProtected = 16u;

    /** S2C_INSERTITEM info bit 5: this instance is permanently character-bound. */
    inline constexpr std::uint32_t kInsertCharacterBound = 32u;

    /**
     * Packs instance flags and endurance into the INSERTITEM info dword.
     * @param replace True to replace the stack count instead of adding.
     * @param cur_end Current endurance, or 0 when unused.
     * @param max_end Max endurance, or 0 when unused.
     * @param put_on True when the instance is worn (ITEM_PUTON).
     * @param account_bound True when the instance cannot leave its account.
     * @param enchant_protected True while one protected attempt remains.
     * @param character_bound True when the instance cannot leave its character.
     * @return Packed info field.
     */
    [[nodiscard]] inline std::uint32_t pack_insert_info(bool replace,
                                                        int cur_end,
                                                        int max_end,
                                                        bool put_on = false,
                                                        bool account_bound = false,
                                                        bool enchant_protected = false,
                                                        bool character_bound = false) {
        std::uint32_t info = replace ? kInsertReplace : 0u;
        if (put_on) {
            info |= kInsertPutOn;
        }
        if (account_bound) {
            info |= kInsertAccountBound;
        }
        if (enchant_protected) {
            info |= kInsertEnchantProtected;
        }
        if (character_bound) {
            info |= kInsertCharacterBound;
        }
        if (cur_end > 0 || max_end > 0) {
            info |= kInsertHasEndurance;
            info |= static_cast<std::uint32_t>(std::clamp(cur_end, 0, 255)) << 8;
            info |= static_cast<std::uint32_t>(std::clamp(max_end, 0, 255)) << 16;
        }
        return info;
    }

    /**
     * True when pack_insert_info set the worn flag.
     * @param info INSERTITEM info dword.
     * @return True when the instance is worn.
     */
    [[nodiscard]] inline bool insert_put_on(std::uint32_t info) {
        return (info & kInsertPutOn) != 0;
    }

    /**
     * True when pack_insert_info set the account-bound flag.
     * @param info INSERTITEM info dword.
     * @return True when the instance cannot leave its account.
     */
    [[nodiscard]] inline bool insert_account_bound(std::uint32_t info) {
        return (info & kInsertAccountBound) != 0;
    }

    /** Reports whether an INSERTITEM instance is character-bound.
     * @param info INSERTITEM info dword.
     * @return True when the instance cannot leave its character.
     */
    [[nodiscard]] inline bool insert_character_bound(std::uint32_t info) {
        return (info & kInsertCharacterBound) != 0;
    }

    /**
     * Reads current endurance packed by pack_insert_info.
     * @param info INSERTITEM info dword.
     * @return Current endurance, or 0 when the flag is clear.
     */
    [[nodiscard]] inline int insert_cur_end(std::uint32_t info) {
        if ((info & kInsertHasEndurance) == 0) {
            return 0;
        }
        return static_cast<int>((info >> 8) & 0xFFu);
    }

    /**
     * Reads max endurance packed by pack_insert_info.
     * @param info INSERTITEM info dword.
     * @return Max endurance, or 0 when the flag is clear.
     */
    [[nodiscard]] inline int insert_max_end(std::uint32_t info) {
        if ((info & kInsertHasEndurance) == 0) {
            return 0;
        }
        return static_cast<int>((info >> 16) & 0xFFu);
    }

    /** Optional charm / Explosive Blow bytes after S2C_INSERTITEM num. */
    inline constexpr std::size_t kInsertCharmBytes = 14;

    /** Per-instance talisman and blacksmith fields packed after INSERTITEM. */
    struct InsertCharmFields {
        /** Talisman of Attack plus. */
        std::uint8_t x_attack = 0;

        /** Talisman of Magic plus. */
        std::uint8_t x_magic = 0;

        /** Talisman of Accuracy (OTP). */
        std::uint8_t x_hit = 0;

        /** Talisman of Defense plus. */
        std::uint8_t x_defense = 0;

        /** Talisman of Evasion plus. */
        std::uint8_t x_dodge = 0;

        /** Explosive Blow level. */
        std::uint8_t upgrade_level = 0;

        /** Stone of Revision applications for the current EB level. */
        std::uint8_t upgrade_rate = 0;

        /** Mixing effect type encoded as int32 mix_type low 8 bits. */
        std::uint8_t mix_type = 0;

        /** 1 when Bead of Fire is applied to this armor instance. */
        std::uint8_t bof = 0;

        /** 1 when a Perforation Shot has prepared the item for Qigong. */
        std::uint8_t qigong_perforated = 0;

        /** Qigong random effect id in [1, 80], or zero. */
        std::uint8_t qigong_effect = 0;

        /** Dragon Fusing random effect id in [1, 240], or zero. */
        std::uint8_t dragon_effect = 0;

        /** RandomItemStatType encoded as a byte. */
        std::uint8_t random_stat_type = 0;

        /** Random equipment-stat magnitude (1 through 5), or zero. */
        std::uint8_t random_stat_value = 0;
    };

    /**
     * Writes kInsertCharmBytes into out.
     * @param out Destination of kInsertCharmBytes bytes.
     * @param fields Values to pack.
     */
    inline void pack_insert_charm(std::uint8_t out[kInsertCharmBytes],
                                  const InsertCharmFields& fields) {
        out[0] = fields.x_attack;
        out[1] = fields.x_magic;
        out[2] = fields.x_hit;
        out[3] = fields.x_defense;
        out[4] = fields.x_dodge;
        out[5] = fields.upgrade_level;
        out[6] = fields.upgrade_rate;
        out[7] = fields.mix_type;
        out[8] = fields.bof;
        out[9] = fields.qigong_perforated;
        out[10] = fields.qigong_effect;
        out[11] = fields.dragon_effect;
        out[12] = fields.random_stat_type;
        out[13] = fields.random_stat_value;
    }

    /**
     * Reads optional charm bytes (zeros when the payload is short).
     * @param data Remaining payload after num.
     * @param size Remaining length.
     * @return Decoded fields.
     */
    [[nodiscard]] inline InsertCharmFields unpack_insert_charm(const std::uint8_t* data,
                                                               std::size_t size) {
        InsertCharmFields fields;
        if (!data || size < 8) {
            return fields;
        }
        fields.x_attack = data[0];
        fields.x_magic = data[1];
        fields.x_hit = data[2];
        fields.x_defense = data[3];
        fields.x_dodge = data[4];
        fields.upgrade_level = data[5];
        fields.upgrade_rate = data[6];
        fields.mix_type = data[7];
        if (size >= 9) {
            fields.bof = data[8];
        }
        if (size >= 10) {
            fields.qigong_perforated = data[9];
        }
        if (size >= 11) {
            fields.qigong_effect = data[10];
        }
        if (size >= 12) {
            fields.dragon_effect = data[11];
        }
        if (size >= 13) {
            fields.random_stat_type = data[12];
        }
        if (size >= 14) {
            fields.random_stat_value = data[13];
        }
        return fields;
    }

}  // namespace openkal::protocol
