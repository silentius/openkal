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

    /** Monster definition / CreateMonster race byte.
     *
     * Combat race values occupy 0..11. Additional values support client UI
     * interaction; Tower shares value 11 with SiegeGunBall. Monster definitions
     * use race 10 plus the AI identifier for siege variants.
     */
    enum class MonsterRace : std::uint8_t {
        /** Normal maguni combat mob (AI roam/chase/attack). */
        Maguni = 0,

        /** Non-maguni combat mob (Real / BlackBug / TowerBoss / skill bosses). */
        NotMaguni = 1,

        /** Guild castle standard — static NotReal. */
        CastleStandard = 2,

        /** Castle gate — static NotReal. */
        CastleGate = 3,

        /** Treasure box — static; opened via GETITEMMONSTER, not attacked. */
        Item = 4,

        /** Ground magic AoE entity — static timed skill field. */
        Magic = 5,

        /** Inactive / spirit prop — static, still damageable. */
        Inactive = 6,

        /** Egg — static, invincible, hatches on timer. */
        Egg = 7,

        /** Cocoon — combat Real with a lifetime timer (may move). */
        Cocoon = 8,

        /** Flame plant — static, invincible, fire explosion on timer. */
        FireFlower = 9,

        /** Siege cannon / ball / ruins (ai selects subclass) — static Move(). */
        SiegeGun = 10,

        /** Server enum slot for siege gun ball (also client Tower). */
        SiegeGunBall = 11,

        /** Client-only UI race (same wire value as SiegeGunBall). */
        Tower = 11,

        /** Client-only prayer monster interaction. */
        Pray = 12,

        /** Client-only prayer door interaction. */
        PrayDoor = 13,

        /** Client-only claw interaction. */
        Claw = 14,
    };

    /** Race-behaviour helpers mirroring KalOnline compatibility behavior* specializations. */
    class MonsterRaceInfo {
      public:
        /** Maps a CreateMonster / monster definition race byte to MonsterRace.
         * @param race Raw race value (unknown values pass through as Maguni-safe int
         * cast).
         * @return Strongly typed race.
         */
        [[nodiscard]] static MonsterRace from_int(int race);

        /** Whether the race uses normal roam AI (compatibility behavior Move).
         * @param race Monster race.
         * @return True when idle roaming is allowed.
         */
        [[nodiscard]] static bool can_roam(MonsterRace race);

        /** Whether the race may chase or melee a player after aggro.
         * @param race Monster race.
         * @return True when pursue/attack AI is allowed.
         */
        [[nodiscard]] static bool can_pursue(MonsterRace race);

        /** Whether this is a living combat mob (roams and can attack players).
         * @param race Monster race.
         * @return True for Maguni / NotMaguni / Cocoon; false for chests, fire flowers,
         * and other static props.
         */
        [[nodiscard]] static bool is_normal_combat(MonsterRace race);

        /** Whether player attacks should apply HP damage (stub for later races).
         * @param race Monster race.
         * @return True when Damage() may reduce HP.
         */
        [[nodiscard]] static bool accepts_damage(MonsterRace race);

        /** Whether interaction is open-chest (C2S_GETITEMMONSTER) instead of attack.
         * @param race Monster race.
         * @return True for treasure boxes.
         */
        [[nodiscard]] static bool is_openable(MonsterRace race);

        /** Whether the client should draw an overhead name (ChkShowMonsterName).
         * @param race Monster race.
         * @return False for Magic and similar field entities.
         */
        [[nodiscard]] static bool shows_nameplate(MonsterRace race);

        /** Whether the entity participates in client collide-box checks.
         * @param race Monster race.
         * @return False for Magic ground fields.
         */
        [[nodiscard]] static bool blocks_collision(MonsterRace race);

        /** @param race Raw race byte.
         * @return True when idle roaming is allowed.
         */
        [[nodiscard]] static bool can_roam(int race);

        /** @param race Raw race byte.
         * @return True when pursue/attack AI is allowed.
         */
        [[nodiscard]] static bool can_pursue(int race);

        /** @param race Raw race byte.
         * @return True for living combat mobs (roam + attack).
         */
        [[nodiscard]] static bool is_normal_combat(int race);

        /** @param race Raw race byte.
         * @return True when Damage() may reduce HP.
         */
        [[nodiscard]] static bool accepts_damage(int race);

        /** @param race Raw race byte.
         * @return True for treasure boxes.
         */
        [[nodiscard]] static bool is_openable(int race);

        /** @param race Raw race byte.
         * @return False for Magic and similar field entities.
         */
        [[nodiscard]] static bool shows_nameplate(int race);

        /** @param race Raw race byte.
         * @return False for Magic ground fields.
         */
        [[nodiscard]] static bool blocks_collision(int race);
    };

}  // namespace openkal::protocol
