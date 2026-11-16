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

#include "openkal/protocol/ClockSync.h"

#include <string>
#include <vector>

namespace openkal::server {
    /** Optional weekly countdown notices, independent of gameplay event activation. */
    class ClockSchedule final {
      public:
        /** Loads optional clock_events from the shared server event configuration.
         * @param path Server event JSON file.
         * @return True for a valid list or an absent optional list.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Computes upcoming occurrences using the server's local calendar and DST rules.
         * @param unix_seconds Server wall time.
         * @param online_seconds Current authenticated account-session duration.
         * @return Snapshot ready for transmission.
         */
        [[nodiscard]] protocol::ClockSyncFields snapshot(std::uint64_t unix_seconds,
                                                         std::uint64_t online_seconds) const;

      private:
        struct Entry {
            std::string id;
            std::string name;
            std::vector<int> weekdays;
            int hour = 0;
            int minute = 0;
        };
        std::vector<Entry> m_entries;
    };
}  // namespace openkal::server
