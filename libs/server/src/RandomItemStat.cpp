/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/RandomItemStat.h"

#include "openkal/server/Combat.h"
#include "openkal/server/world/GameWorld.h"

namespace openkal::server {
    namespace {

        [[nodiscard]] std::uint8_t roll_tier_value() {
            const int roll = combat::irand(1, 100);
            if (roll <= 76) {
                return 1;
            }
            if (roll <= 96) {
                return 2;
            }
            if (roll <= 104) {
                return 3;
            }
            if (roll <= 109) {
                return 4;
            }
            return 5;
        }

    }  // namespace

    RandomItemStatRoll roll_random_item_stat(const ItemCombatDef& item) {
        if ((!item.is_weapon && !item.is_defense) || combat::irand(1, 100) > 10) {
            return {};
        }

        const std::uint8_t value = roll_tier_value();
        RandomItemStatRoll result;
        result.value = value;
        if (item.is_weapon) {
            if (value == 5) {
                result.type = combat::irand(0, 1) == 0
                                  ? protocol::RandomItemStatType::WeaponPhysicalDamagePercent
                                  : protocol::RandomItemStatType::WeaponMagicalDamagePercent;
                return result;
            }
            switch (combat::irand(0, 5)) {
            case 0:
                result.type = protocol::RandomItemStatType::WeaponMinPhysicalDamagePercent;
                break;
            case 1:
                result.type = protocol::RandomItemStatType::WeaponMinMagicalDamagePercent;
                break;
            case 2:
                result.type = protocol::RandomItemStatType::WeaponMaxPhysicalDamagePercent;
                break;
            case 3:
                result.type = protocol::RandomItemStatType::WeaponMaxMagicalDamagePercent;
                break;
            case 4:
                result.type = protocol::RandomItemStatType::WeaponPhysicalDamagePercent;
                break;
            default:
                result.type = protocol::RandomItemStatType::WeaponMagicalDamagePercent;
                break;
            }
            return result;
        }

        if (value == 5) {
            switch (combat::irand(0, 5)) {
            case 0:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentStrength;
                break;
            case 1:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentHealth;
                break;
            case 2:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentIntelligence;
                break;
            case 3:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentWisdom;
                break;
            case 4:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentDexterity;
                break;
            default:
                result.type = protocol::RandomItemStatType::ArmorDefensePercentDefense;
                break;
            }
            return result;
        }

        switch (combat::irand(0, 6)) {
        case 0:
            result.type = protocol::RandomItemStatType::ArmorStrength;
            break;
        case 1:
            result.type = protocol::RandomItemStatType::ArmorHealth;
            break;
        case 2:
            result.type = protocol::RandomItemStatType::ArmorIntelligence;
            break;
        case 3:
            result.type = protocol::RandomItemStatType::ArmorWisdom;
            break;
        case 4:
            result.type = protocol::RandomItemStatType::ArmorDexterity;
            break;
        case 5:
            result.type = protocol::RandomItemStatType::ArmorDefense;
            break;
        default:
            result.type = protocol::RandomItemStatType::ArmorDefensePercent;
            break;
        }
        return result;
    }

}  // namespace openkal::server
