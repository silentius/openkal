/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/DropRoller.h"

#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/Combat.h"

#include <cmath>
#include <cstdlib>

namespace openkal::server {
    namespace {

        /** Chance percent → hundredths-of-a-percent units (0.01% = 1; 100% = 10000). */
        [[nodiscard]] int chance_to_units(float chance_pct) {
            if (chance_pct <= 0.0f) {
                return 0;
            }
            const int units =
                static_cast<int>(std::lround(static_cast<double>(chance_pct) * 100.0));
            if (units < 0) {
                return 0;
            }
            if (units > 10000) {
                return 10000;
            }
            return units;
        }

        /**
         * Single weighted pick on a 10000-unit (0.01%) scale.
         * Equal chances get equal odds (coin-flip among ties via the shared RNG).
         * Remainder above sum(chances) is no-drop.
         */
        template <typename EntryT, typename ChanceFn>
        [[nodiscard]] int pick_weighted_index(const std::vector<EntryT>& entries,
                                              ChanceFn chance_of) {
            if (entries.empty()) {
                return -1;
            }

            const int roll = combat::irand(0, 9999);
            int acc = 0;
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const int units = chance_to_units(chance_of(entries[static_cast<std::size_t>(i)]));
                if (units <= 0) {
                    continue;
                }
                acc += units;
                if (roll < acc) {
                    return i;
                }
            }
            return -1;
        }

    }  // namespace

    DropOutcome apply_wealth_stone_bonus(DropOutcome drop,
                                         DropSource source,
                                         bool solo_owner,
                                         bool stone_active) {
        if (source != DropSource::MonsterKill || !solo_owner || !stone_active ||
            drop.kind != DropOutcome::Kind::Money || drop.count <= 0) {
            return drop;
        }
        const std::int64_t count = drop.count;
        drop.count = combat::clamp_nonneg_i32(count + count / 5);
        return drop;
    }

    DropRoller::DropRoller(const ItemGroupDb& db) : m_db(db) {}

    std::vector<std::pair<int, int>> DropRoller::effective_monster_kill_item_groups(
        const std::vector<std::pair<int, int>>& item_groups, int race) {
        std::vector<std::pair<int, int>> out;
        out.reserve(item_groups.size() + 1);
        for (const auto& slot : item_groups) {
            if (slot.first == kGlobalNormalMonsterItemGroupIndex) {
                continue;
            }
            out.push_back(slot);
        }
        if (protocol::MonsterRaceInfo::is_normal_combat(race)) {
            out.emplace_back(kGlobalNormalMonsterItemGroupIndex, 1);
        }
        return out;
    }

    std::pair<int, int>
    DropRoller::scatter_drop_point(int corpse_x, int corpse_y, int max_offset, int min_offset) {
        if (max_offset < 0) {
            max_offset = 0;
        }
        if (min_offset < 0) {
            min_offset = 0;
        }
        if (min_offset > max_offset) {
            min_offset = max_offset;
        }
        if (max_offset == 0) {
            return {corpse_x, corpse_y};
        }
        // Legacy GetDropPt: random in ±max, require |ox|>=min and |oy|>=min.
        for (int attempt = 0; attempt < 32; ++attempt) {
            const int ox = combat::irand(-max_offset, max_offset);
            const int oy = combat::irand(-max_offset, max_offset);
            if (std::abs(ox) >= min_offset && std::abs(oy) >= min_offset) {
                return {corpse_x + ox, corpse_y + oy};
            }
        }
        return {corpse_x + min_offset, corpse_y + min_offset};
    }

    std::optional<DropOutcome> DropRoller::roll_group(int group_index) const {
        const GroupDef* group = m_db.find_group(group_index);
        if (!group || group->entries.empty()) {
            return std::nullopt;
        }

        const int idx =
            pick_weighted_index(group->entries, [](const GroupEntry& e) { return e.chance; });
        if (idx < 0) {
            return std::nullopt;
        }

        const GroupEntry& e = group->entries[static_cast<std::size_t>(idx)];
        DropOutcome out;
        if (e.kind == GroupEntry::Kind::Money) {
            out.kind = DropOutcome::Kind::Money;
            out.item_index = kMoneyItemIndex;
            out.prefix = 0;
            out.count = e.amount;
        } else {
            out.kind = DropOutcome::Kind::Item;
            out.item_index = e.item_index;
            out.prefix = e.prefix;
            out.count = e.count > 0 ? e.count : 1;
        }
        return out;
    }

    std::optional<DropOutcome> DropRoller::roll_itemgroup(int itemgroup_index) const {
        const ItemGroupDef* ig = m_db.find_itemgroup(itemgroup_index);
        if (!ig || ig->entries.empty()) {
            return std::nullopt;
        }

        const int idx =
            pick_weighted_index(ig->entries, [](const ItemGroupEntry& e) { return e.chance; });
        if (idx < 0) {
            return std::nullopt;
        }

        const int group_index = ig->entries[static_cast<std::size_t>(idx)].group_index;
        return roll_group(group_index);
    }

    std::vector<DropOutcome> DropRoller::roll_monster_drops(
        const std::vector<std::pair<int, int>>& item_groups, int corpse_x, int corpse_y) const {
        std::vector<DropOutcome> out;
        for (const auto& [ig_index, attempts] : item_groups) {
            if (ig_index <= 0 || attempts <= 0) {
                continue;
            }
            for (int n = 0; n < attempts; ++n) {
                auto rolled = roll_itemgroup(ig_index);
                if (!rolled) {
                    continue;
                }
                const auto pos = scatter_drop_point(
                    corpse_x, corpse_y, kMonsterDropScatterMax, kMonsterDropScatterMin);
                rolled->x = pos.first;
                rolled->y = pos.second;
                out.push_back(*rolled);
            }
        }
        return out;
    }

}  // namespace openkal::server
