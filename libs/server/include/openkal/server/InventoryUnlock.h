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
#include <vector>

namespace openkal::server {

    /** One persisted timed inventory-register unlock. */
    struct InventoryUnlockRecord {
        /** Character that owns the unlock. */
        std::int32_t character_id = 0;
        /** Additional register index (0 = register 3). */
        int register_index = 0;
        /** Seconds remaining. */
        std::int32_t remain = 0;
    };

    /** One active timed inventory-register unlock. */
    struct InventoryUnlockEntry {
        /** Unix timestamp at which the unlock expires. */
        std::int64_t expire_unix = 0;
    };

    /**
     * Keeps the four purchasable inventory registers ordered by remaining time.
     * The longest remaining unlock always occupies register 3 and the shortest
     * occupies the last active register.
     */
    class InventoryUnlockSet {
      public:
        /** Maximum number of additional inventory registers. */
        static constexpr int kMaxUnlocks = 4;

        /** @return Active unlocks in register order (longest remaining first). */
        [[nodiscard]] const std::vector<InventoryUnlockEntry>& entries() const {
            return m_entries;
        }

        /**
         * Activates one additional inventory register.
         * @param remain Seconds the register remains unlocked.
         * @param now_unix Current unix time.
         * @return True when a register was added.
         */
        [[nodiscard]] bool apply(int remain, std::int64_t now_unix);

        /**
         * Removes expired unlocks.
         * @param now_unix Current unix time.
         * @return True when the active register order changed.
         */
        [[nodiscard]] bool tick(std::int64_t now_unix);

        /**
         * Loads unlocks persisted for a character.
         * @param rows Stored unlock rows.
         * @param now_unix Current unix time.
         */
        void load(const std::vector<InventoryUnlockRecord>& rows, std::int64_t now_unix);

        /**
         * Serializes active unlocks for persistence.
         * @param character_id Owner character id.
         * @param now_unix Current unix time.
         * @return Rows in register order.
         */
        [[nodiscard]] std::vector<InventoryUnlockRecord> snapshot(std::int32_t character_id,
                                                                  std::int64_t now_unix) const;

        /**
         * Calculates remaining seconds for one unlock.
         * @param entry Active unlock.
         * @param now_unix Current unix time.
         * @return Remaining seconds, or zero when expired.
         */
        [[nodiscard]] static std::int32_t remain_of(const InventoryUnlockEntry& entry,
                                                    std::int64_t now_unix);

      private:
        /** Sorts active unlocks so the longest duration maps to register 3. */
        void sort_entries();

        /** Timed unlocks in register order. */
        std::vector<InventoryUnlockEntry> m_entries;
    };

}  // namespace openkal::server
