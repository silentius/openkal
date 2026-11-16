/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/InventoryUnlock.h"

#include <algorithm>
#include <limits>

namespace openkal::server {

    std::int32_t InventoryUnlockSet::remain_of(const InventoryUnlockEntry& entry,
                                               std::int64_t now_unix) {
        if (entry.expire_unix <= now_unix) {
            return 0;
        }
        const std::int64_t remain = entry.expire_unix - now_unix;
        return remain > std::numeric_limits<std::int32_t>::max()
                   ? std::numeric_limits<std::int32_t>::max()
                   : static_cast<std::int32_t>(remain);
    }

    void InventoryUnlockSet::sort_entries() {
        std::sort(m_entries.begin(),
                  m_entries.end(),
                  [](const InventoryUnlockEntry& a, const InventoryUnlockEntry& b) {
                      return a.expire_unix > b.expire_unix;
                  });
    }

    bool InventoryUnlockSet::apply(int remain, std::int64_t now_unix) {
        (void) tick(now_unix);
        if (remain <= 0 || static_cast<int>(m_entries.size()) >= kMaxUnlocks) {
            return false;
        }
        m_entries.push_back({now_unix + static_cast<std::int64_t>(remain)});
        sort_entries();
        return true;
    }

    bool InventoryUnlockSet::tick(std::int64_t now_unix) {
        const std::size_t previous_count = m_entries.size();
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [now_unix](const InventoryUnlockEntry& entry) {
                                           return entry.expire_unix <= now_unix;
                                       }),
                        m_entries.end());
        if (m_entries.size() == previous_count) {
            return false;
        }
        sort_entries();
        return true;
    }

    void InventoryUnlockSet::load(const std::vector<InventoryUnlockRecord>& rows,
                                  std::int64_t now_unix) {
        m_entries.clear();
        for (const auto& row : rows) {
            if (row.remain <= 0 || static_cast<int>(m_entries.size()) >= kMaxUnlocks) {
                continue;
            }
            m_entries.push_back({now_unix + static_cast<std::int64_t>(row.remain)});
        }
        (void) tick(now_unix);
        sort_entries();
    }

    std::vector<InventoryUnlockRecord> InventoryUnlockSet::snapshot(std::int32_t character_id,
                                                                    std::int64_t now_unix) const {
        std::vector<InventoryUnlockRecord> rows;
        rows.reserve(m_entries.size());
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            const std::int32_t remain = remain_of(m_entries[i], now_unix);
            if (remain <= 0) {
                continue;
            }
            rows.push_back({character_id, static_cast<int>(i), remain});
        }
        return rows;
    }

}  // namespace openkal::server
