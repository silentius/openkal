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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::platform {
    class Json;
}

namespace openkal::server {
    /** Current phase of the recurring Castle War schedule. */
    enum class CastleWarPhase { Idle, Declaration, Preparation, Admission, Battle };

    /** Map index and server position used for castle objectives and transfers. */
    struct CastleWarPosition {
        int map = 0;
        int x = 0;
        int y = 0;
        int z = 0;
    };

    /** Round phase boundaries in Unix seconds, with an exclusive end. */
    struct CastleWarRound {
        std::int64_t declaration = 0;
        std::int64_t preparation = 0;
        std::int64_t admission = 0;
        std::int64_t battle = 0;
        std::int64_t end = 0;
    };

    /** Definition of a capturable standard, independent of spawn catalogs. */
    struct CastleWarFlag {
        CastleWarPosition position;
        int monster_index = 0;
        int points = 0;
        int hit_points = 0;
    };

    /** Runtime settings loaded from the private Castle War JSON. */
    struct CastleWarRules {
        bool enabled = false;
        int minimum_level = 70;
        /** Number of partner guilds. The leading guild is additional. */
        int max_allied_guilds = 5;
        int alliance_min_days = 1;
        int alliance_max_days = 15;
        int alliance_invitation_seconds = 86400;

        /** Fixed UTC offset and local weekly battle start, with Sunday numbered zero. */
        int utc_offset_minutes = 540;
        int battle_weekday = 0;
        int battle_hour = 1;
        int battle_minute = 0;
        int battle_second = 0;
        int declaration_seconds = 86400;
        /** Includes the admission window immediately before battle. */
        int preparation_seconds = 86400;
        int admission_seconds = 600;
        int battle_seconds = 3600;

        int respawn_seconds = 30;
        int protection_seconds = 10;
        int capture_unlock_seconds = 600;
        int hold_seconds = 300;
        int flag_regen_seconds = 5;
        int flag_regen_points = 100;
        int match_score_flush_seconds = 5;

        int activity_minimum = 1;
        int honor_winner = 7000;
        int honor_ally = 3000;
        int honor_loser = 300;
        int honor_flag_max = 2000;
        int honor_kill_max = 150;
        int honor_activity_max = 4000;
        /** Merchant prices as percentages of the original price, in policy order. */
        std::array<int, 3> tax_percent{95, 100, 110};
        /** Catalog NPC indices subject to the current castle owner's price policy. */
        std::vector<int> merchant_npc_indices;
        int max_defenses = 10;

        int guard_npc_index = 0;
        int master_npc_index = 0;
        int camp_npc_index = 0;
        int manager_npc_index = 0;
        CastleWarPosition attack_camp;
        CastleWarPosition defense_camp;
        CastleWarPosition east_gate;
        CastleWarPosition west_gate;
        CastleWarPosition exit;
        /** Inclusive rectangle [min_x, min_y, max_x, max_y] on the attack camp map. */
        std::array<int, 4> bounds{};
        /** The first objective is the large standard. */
        std::array<CastleWarFlag, 4> flags{{{CastleWarPosition{}, 0, 50, 10000},
                                            {CastleWarPosition{}, 0, 30, 5000},
                                            {CastleWarPosition{}, 0, 30, 5000},
                                            {CastleWarPosition{}, 0, 30, 5000}}};

        /** Loads configuration directly from the server filesystem.
         * @param path Path to the private JSON file.
         * @param error Reason if the file cannot be loaded or is invalid.
         * @return True when validated settings replaced the current settings.
         */
        [[nodiscard]] bool load(const std::string& path, std::string& error);

        /** Validates a rules document and applies it atomically.
         * @param document Parsed Castle War JSON.
         * @param error Reason if the configuration is invalid.
         * @return True when validated settings replaced the current settings.
         */
        [[nodiscard]] bool parse(const platform::Json& document, std::string& error);

        /** Computes the current or upcoming round with the configured UTC offset.
         * @param now Current Unix time in seconds.
         * @return Current round during declaration through battle, or the next round.
         */
        [[nodiscard]] CastleWarRound round(std::int64_t now) const;

        /** Finds the schedule phase active at a given time.
         * @param now Current Unix time in seconds.
         * @return Phase, or Idle when disabled or between rounds.
         */
        [[nodiscard]] CastleWarPhase phase(std::int64_t now) const;

        /** Checks whether a server point is inside the configured arena.
         * @param map Map index.
         * @param x Server pixel X.
         * @param y Server pixel Y.
         * @return True for a point on the arena map within its inclusive bounds.
         */
        [[nodiscard]] bool contains(int map, int x, int y) const;
    };
}  // namespace openkal::server
