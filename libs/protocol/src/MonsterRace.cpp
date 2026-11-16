/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/MonsterRace.h"

namespace openkal::protocol {

    MonsterRace MonsterRaceInfo::from_int(int race) {
        if (race < 0) {
            return MonsterRace::Maguni;
        }
        return static_cast<MonsterRace>(static_cast<std::uint8_t>(race));
    }

    bool MonsterRaceInfo::can_roam(MonsterRace race) {
        // Stationary controller types do not execute ordinary movement.
        switch (race) {
        case MonsterRace::CastleStandard:
        case MonsterRace::CastleGate:
        case MonsterRace::Item:
        case MonsterRace::Magic:
        case MonsterRace::Inactive:
        case MonsterRace::Egg:
        case MonsterRace::FireFlower:
        case MonsterRace::SiegeGun:
        case MonsterRace::SiegeGunBall:
        case MonsterRace::Pray:
        case MonsterRace::PrayDoor:
        case MonsterRace::Claw:
            return false;
        case MonsterRace::Maguni:
        case MonsterRace::NotMaguni:
        case MonsterRace::Cocoon:
        default:
            return true;
        }
    }

    bool MonsterRaceInfo::can_pursue(MonsterRace race) {
        // Combat chase/attack is only for living Real/Maguni-style AI.
        switch (race) {
        case MonsterRace::Maguni:
        case MonsterRace::NotMaguni:
        case MonsterRace::Cocoon:
            return true;
        default:
            return false;
        }
    }

    bool MonsterRaceInfo::is_normal_combat(MonsterRace race) {
        return can_roam(race) && can_pursue(race);
    }

    bool MonsterRaceInfo::accepts_damage(MonsterRace race) {
        // Egg / FireFlower / Magic Damage() return FALSE; Item uses open path.
        switch (race) {
        case MonsterRace::Item:
        case MonsterRace::Magic:
        case MonsterRace::Egg:
        case MonsterRace::FireFlower:
        case MonsterRace::SiegeGunBall:
        case MonsterRace::Pray:
        case MonsterRace::PrayDoor:
            return false;
        case MonsterRace::Maguni:
        case MonsterRace::NotMaguni:
        case MonsterRace::CastleStandard:
        case MonsterRace::CastleGate:
        case MonsterRace::Inactive:
        case MonsterRace::Cocoon:
        case MonsterRace::SiegeGun:
        case MonsterRace::Claw:
        default:
            return true;
        }
    }

    bool MonsterRaceInfo::is_openable(MonsterRace race) {
        return race == MonsterRace::Item;
    }

    bool MonsterRaceInfo::shows_nameplate(MonsterRace race) {
        return race != MonsterRace::Magic;
    }

    bool MonsterRaceInfo::blocks_collision(MonsterRace race) {
        return race != MonsterRace::Magic;
    }

    bool MonsterRaceInfo::can_roam(int race) {
        return can_roam(from_int(race));
    }

    bool MonsterRaceInfo::can_pursue(int race) {
        return can_pursue(from_int(race));
    }

    bool MonsterRaceInfo::is_normal_combat(int race) {
        return is_normal_combat(from_int(race));
    }

    bool MonsterRaceInfo::accepts_damage(int race) {
        return accepts_damage(from_int(race));
    }

    bool MonsterRaceInfo::is_openable(int race) {
        return is_openable(from_int(race));
    }

    bool MonsterRaceInfo::shows_nameplate(int race) {
        return shows_nameplate(from_int(race));
    }

    bool MonsterRaceInfo::blocks_collision(int race) {
        return blocks_collision(from_int(race));
    }

}  // namespace openkal::protocol
