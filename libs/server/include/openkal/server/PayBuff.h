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

#include "openkal/protocol/BuffState.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace openkal::server {

    /** One active pay-scroll buff on a character. */
    struct PayBuffEntry {
        /** BUFF_TYPE ordinal. */
        int buff_type = 0;

        /** compatibility behavior bitmask for this buff. */
        std::uint64_t bstate = 0;

        /** True when remain is a charge count. */
        bool is_count = false;

        /** Remaining charges when is_count; unused when timed. */
        std::int32_t charges = 0;

        /**
         * Charge-pack size from the item that started this stack (100/300/500).
         * Same buff_type with a different pack cannot mix.
         */
        std::int32_t stack_pack = 0;

        /** Item index that started or last refreshed this buff. */
        std::int32_t source_item_index = 0;

        /** Unix expire time when timed; unused when is_count. */
        std::int64_t expire_unix = 0;
    };

    /** Persisted pay-buff row (S2D_BUFFREMAIN subset). */
    struct BuffRemainRecord {
        std::int32_t character_id = 0;
        int buff_type = 0;
        /** Seconds left, or charge count. */
        std::int32_t remain = 0;
        /** 1 when remain is charges. */
        int is_count = 0;

        /**
         * Charge-pack size for count buffs (100/300/500); 0 when timed or unknown.
         */
        std::int32_t stack_pack = 0;

        /** Item index used to restore the HUD icon and title after login. */
        std::int32_t source_item_index = 0;
    };

    /**
     * Runtime manager for pay-scroll BState buffs (related compatibility states).
     */
    class PayBuffSet {
      public:
        /** @return Combined compatibility behavior bitmask of active entries. */
        [[nodiscard]] std::uint64_t bstate() const;

        /** @return True when the given compatibility behavior bit is active. */
        [[nodiscard]] bool has(std::uint64_t bit) const;

        /** @return Active entries (read-only). */
        [[nodiscard]] const std::vector<PayBuffEntry>& entries() const {
            return m_entries;
        }

        /**
         * Applies or refreshes a pay buff from item specialty.
         * @param buff_type BUFF_TYPE ordinal.
         * @param remain Seconds or charges.
         * @param now_unix Current unix time.
         * @param out_remain Out remain sent to the client (seconds or charges).
         * @param out_bstate Out compatibility behavior bit for ADDBSTATE.
         * @param source_item_index Item index used for the HUD icon and title.
         * @param amount Percent for attack food (specialty.buff[2]); ignored for other types.
         * @return True when applied.
         */
        [[nodiscard]] bool apply(int buff_type,
                                 int remain,
                                 std::int64_t now_unix,
                                 std::int32_t& out_remain,
                                 std::uint64_t& out_bstate,
                                 std::int32_t source_item_index = 0,
                                 int amount = 0);

        /**
         * Stored percent for an active attack-food buff.
         * @param buff_type specialty.buff[0] (5 physical, 6 magical).
         * @return Percent, or 0 when that food is not active.
         */
        [[nodiscard]] int percent_of(int buff_type) const;

        /**
         * Expires timed buffs whose deadline has passed.
         * @param now_unix Current unix time.
         * @return compatibility behavior bits that were cleared (for SUBBSTATE), or 0.
         */
        [[nodiscard]] std::uint64_t tick(std::int64_t now_unix);

        /**
         * Decrements a count buff by one (Perfect Def / Drain HP proc).
         * @param buff_type BUFF_TYPE to consume.
         * @param out_remain Out remaining charges after consume (0 if removed).
         * @param out_removed True when the buff was cleared.
         * @return True when a charge was consumed.
         */
        [[nodiscard]] bool
        consume_charge(int buff_type, std::int32_t& out_remain, bool& out_removed);

        /**
         * Remaining seconds or charges for client sync.
         * @param entry Active entry.
         * @param now_unix Current unix time.
         * @return Remain value for MSG_ADDBSTATE.
         */
        [[nodiscard]] static std::int32_t remain_of(const PayBuffEntry& entry,
                                                    std::int64_t now_unix);

        /**
         * Rebuilds from persisted rows (login).
         * @param rows Stored remains.
         * @param now_unix Current unix time.
         */
        void load(const std::vector<BuffRemainRecord>& rows, std::int64_t now_unix);

        /**
         * Serializes active entries for SQLite.
         * @param character_id Owner character id.
         * @param now_unix Current unix time.
         * @return Rows to upsert.
         */
        [[nodiscard]] std::vector<BuffRemainRecord> snapshot(std::int32_t character_id,
                                                             std::int64_t now_unix) const;

        /** Clears all entries. */
        void clear();

      private:
        /** Active pay buffs. */
        std::vector<PayBuffEntry> m_entries;

        /**
         * Finds an entry by buff type.
         * @param buff_type BUFF_TYPE.
         * @return Index, or -1.
         */
        [[nodiscard]] int find_index(int buff_type) const;

        /**
         * Removes an entry by index.
         * @param index Entry index.
         */
        void erase_at(int index);
    };

    /**
     * True when buff_type is a supported pay scroll.
     * @param buff_type BUFF_TYPE ordinal.
     * @return True when the type has server-side timed-buff support.
     */
    [[nodiscard]] bool is_pay_buff_type(int buff_type);

    /**
     * Maps buff type to compatibility behavior bit (0 when unsupported).
     * @param buff_type BUFF_TYPE ordinal.
     * @return compatibility behavior bitmask.
     */
    [[nodiscard]] std::uint64_t pay_buff_bstate(int buff_type);

}  // namespace openkal::server
