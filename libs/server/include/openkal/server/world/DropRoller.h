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

#include "openkal/server/world/ItemGroupDb.h"

#include <optional>
#include <utility>
#include <vector>

namespace openkal::server {

    /** Legacy GetDropPt half-extent in server pixels (monster / chest). */
    inline constexpr int kMonsterDropScatterMax = 16;

    /** Legacy GetDropPt minimum |offset| on each axis (monster / chest). */
    inline constexpr int kMonsterDropScatterMin = 8;

    /** Player inventory drop half-extent (shorter toss than monster loot). */
    inline constexpr int kPlayerDropScatterMax = 8;

    /** Player inventory drop minimum |offset| on each axis. */
    inline constexpr int kPlayerDropScatterMin = 4;

    /** One rolled drop ready for ground-item spawning (Phase 2). */
    struct DropOutcome {
        /** Discriminates item vs money. */
        enum class Kind { Item = 0, Money = 1 };

        /** Drop kind. */
        Kind kind = Kind::Item;

        /** item definition index (money uses kMoneyItemIndex). */
        int item_index = 0;

        /** Prefix id for items. */
        int prefix = 0;

        /** Stack count or money amount. */
        int count = 1;

        /** World pixel X (scattered from corpse). */
        int x = 0;

        /** World pixel Y (scattered from corpse). */
        int y = 0;
    };

    /** Source of a rolled ground drop. */
    enum class DropSource { MonsterKill, Chest };

    /** Applies the Stone of Wealth bonus to eligible monster money.
     * @param drop Rolled item or money drop.
     * @param source Event that produced the drop.
     * @param solo_owner True for an individually owned monster kill.
     * @param stone_active True while the owner's Stone of Wealth is active.
     * @return Drop with its money amount increased when eligible.
     */
    [[nodiscard]] DropOutcome apply_wealth_stone_bonus(DropOutcome drop,
                                                       DropSource source,
                                                       bool solo_owner,
                                                       bool stone_active);

    /** Rolls percentage drop tables with additive weighted picks (0.01% precision).
     */
    class DropRoller {
      public:
        /** Constructs a roller bound to loaded drop tables.
         * @param db Item group database.
         */
        explicit DropRoller(const ItemGroupDb& db);

        /** Rolls one attempt against an outer itemgroup.
         * @param itemgroup_index Outer table index.
         * @return Drop outcome when something is selected.
         */
        [[nodiscard]] std::optional<DropOutcome> roll_itemgroup(int itemgroup_index) const;

        /** Rolls all configured itemgroup attempts for a monster kill.
         * @param item_groups Pairs of (itemgroup_index, attempt_count).
         * @param corpse_x Monster death X in server pixels.
         * @param corpse_y Monster death Y in server pixels.
         * @return Zero or more scattered drop outcomes.
         */
        [[nodiscard]] std::vector<DropOutcome> roll_monster_drops(
            const std::vector<std::pair<int, int>>& item_groups, int corpse_x, int corpse_y) const;

        /** Builds kill drop slots, appending the reserved global itemgroup once for
         *  normal combat races. Strips the reserved index from per-monster slots so it
         *  cannot double-roll when someone listed it on the monster.
         * @param item_groups Pairs of (itemgroup_index, attempt_count) from the monster.
         * @param race monster definition / JSON stats.race.
         * @return Slots to pass to roll_monster_drops.
         */
        [[nodiscard]] static std::vector<std::pair<int, int>>
        effective_monster_kill_item_groups(const std::vector<std::pair<int, int>>& item_groups,
                                           int race);

        /** Picks a scatter point near a corpse (legacy GetDropPt style).
         * @param corpse_x Corpse X in server pixels.
         * @param corpse_y Corpse Y in server pixels.
         * @param max_offset Inclusive half-extent (legacy 16).
         * @param min_offset Minimum |offset| on each axis (legacy 8).
         * @return Pixel offset position.
         */
        [[nodiscard]] static std::pair<int, int>
        scatter_drop_point(int corpse_x,
                           int corpse_y,
                           int max_offset = kMonsterDropScatterMax,
                           int min_offset = kMonsterDropScatterMin);

      private:
        /** Rolls one inner group table.
         * @param group_index Inner group index.
         * @return Drop outcome when something is selected.
         */
        [[nodiscard]] std::optional<DropOutcome> roll_group(int group_index) const;

        /** Bound drop tables. */
        const ItemGroupDb& m_db;
    };

}  // namespace openkal::server
