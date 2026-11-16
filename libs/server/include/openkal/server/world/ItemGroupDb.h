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

#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** Legacy item definition index used for ground money piles. */
    inline constexpr int kMoneyItemIndex = 31;

    /** Reserved outer itemgroup rolled once on every normal combat-monster death. */
    inline constexpr int kGlobalNormalMonsterItemGroupIndex = 1000000;

    /** One weighted entry inside an inner loot group. */
    struct GroupEntry {
        /** Discriminates item vs money payloads. */
        enum class Kind { Item = 0, Money = 1 };

        /** Entry kind. */
        Kind kind = Kind::Item;

        /** Drop chance in percent (0.01–100). */
        float chance = 0.0f;

        /** item definition index when kind is Item. */
        int item_index = 0;

        /** Prefix id when kind is Item. */
        int prefix = 0;

        /** Stack count when kind is Item. */
        int count = 1;

        /** Money amount when kind is Money. */
        int amount = 0;
    };

    /** Inner loot table referenced by itemgroups. */
    struct GroupDef {
        /** Group index. */
        int index = 0;

        /** Optional display name from conversion comments. */
        std::string name;

        /** Weighted entries (sum of chances ≤ 100). */
        std::vector<GroupEntry> entries;
    };

    /** One weighted reference from an outer itemgroup to an inner group. */
    struct ItemGroupEntry {
        /** Drop chance in percent (0.01–100). */
        float chance = 0.0f;

        /** Inner group index. */
        int group_index = 0;
    };

    /** Outer loot table referenced by monsters as itemGroups. */
    struct ItemGroupDef {
        /** Itemgroup index. */
        int index = 0;

        /** Optional display name from conversion comments. */
        std::string name;

        /** Weighted group references (sum of chances ≤ 100). */
        std::vector<ItemGroupEntry> entries;
    };

    /** Loads percentage-based group / itemgroup JSON drop tables. */
    class ItemGroupDb {
      public:
        /** Clears all loaded tables. */
        void clear();

        /** Loads inner group JSON files from a directory.
         * @param dir Directory of `{index}.json` group files.
         * @return True when at least one group loaded.
         */
        [[nodiscard]] bool load_groups_dir(const std::string& dir);

        /** Loads outer itemgroup JSON files from a directory.
         * @param dir Directory of `{index}.json` itemgroup files.
         * @return True when at least one itemgroup loaded.
         */
        [[nodiscard]] bool load_itemgroups_dir(const std::string& dir);

        /** Looks up an inner group by index.
         * @param index Group index.
         * @return Group definition, or nullptr.
         */
        [[nodiscard]] const GroupDef* find_group(int index) const;

        /** Looks up a mutable inner group by index.
         * @param index Group index.
         * @return Group definition, or nullptr.
         */
        [[nodiscard]] GroupDef* find_group_mut(int index);

        /** Looks up an outer itemgroup by index.
         * @param index Itemgroup index.
         * @return Itemgroup definition, or nullptr.
         */
        [[nodiscard]] const ItemGroupDef* find_itemgroup(int index) const;

        /** Looks up a mutable outer itemgroup by index.
         * @param index Itemgroup index.
         * @return Itemgroup definition, or nullptr.
         */
        [[nodiscard]] ItemGroupDef* find_itemgroup_mut(int index);

        /** Inserts or replaces an inner group.
         * @param def Group definition.
         */
        void upsert_group(GroupDef def);

        /** Removes an inner group when present.
         * @param index Group index.
         * @return True when a group was removed.
         */
        [[nodiscard]] bool remove_group(int index);

        /** Inserts or replaces an outer itemgroup.
         * @param def Itemgroup definition.
         */
        void upsert_itemgroup(ItemGroupDef def);

        /** Removes an outer itemgroup when present.
         * @param index Itemgroup index.
         * @return True when an itemgroup was removed.
         */
        [[nodiscard]] bool remove_itemgroup(int index);

        /** Writes one inner group JSON file.
         * @param dir Output directory.
         * @param def Group definition.
         * @return True on success.
         */
        [[nodiscard]] static bool save_group(const std::string& dir, const GroupDef& def);

        /** Writes one outer itemgroup JSON file.
         * @param dir Output directory.
         * @param def Itemgroup definition.
         * @return True on success.
         */
        [[nodiscard]] static bool save_itemgroup(const std::string& dir, const ItemGroupDef& def);

        /** Suggests the next unused inner group index (max + 1).
         * @return Next free group index, or 1 when empty.
         */
        [[nodiscard]] int next_free_group_index() const;

        /** Suggests the next unused outer itemgroup index (max + 1).
         *  Indices at or above the reserved global itemgroup are ignored.
         * @return Next free itemgroup index, or 1 when empty.
         */
        [[nodiscard]] int next_free_itemgroup_index() const;

        /** Reports loaded group count.
         * @return Number of groups.
         */
        [[nodiscard]] std::size_t group_count() const {
            return m_groups.size();
        }

        /** Reports loaded itemgroup count.
         * @return Number of itemgroups.
         */
        [[nodiscard]] std::size_t itemgroup_count() const {
            return m_itemgroups.size();
        }

        /** Returns all loaded inner groups keyed by index.
         * @return Group map.
         */
        [[nodiscard]] const std::unordered_map<int, GroupDef>& groups() const {
            return m_groups;
        }

        /** Returns all loaded outer itemgroups keyed by index.
         * @return Itemgroup map.
         */
        [[nodiscard]] const std::unordered_map<int, ItemGroupDef>& itemgroups() const {
            return m_itemgroups;
        }

      private:
        /** Inner groups keyed by index. */
        std::unordered_map<int, GroupDef> m_groups;

        /** Outer itemgroups keyed by index. */
        std::unordered_map<int, ItemGroupDef> m_itemgroups;
    };

}  // namespace openkal::server
