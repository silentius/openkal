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

#include <chrono>
#include <string>
#include <string_view>

namespace openkal::client {
    /** Extrapolates server snapshots without depending on the PC wall clock. */
    class ServerClock final {
      public:
        using TimePoint = std::chrono::steady_clock::time_point;

        /** Replaces the snapshot without resetting the account duration locally.
         * @param snapshot Authoritative server data.
         * @param received Monotonic receive time.
         */
        void synchronize(protocol::ClockSyncFields snapshot,
                         TimePoint received = std::chrono::steady_clock::now());

        /** Clears synchronization on disconnect or a new account connection. */
        void reset();

        /** @return True after the first server snapshot. */
        bool ready() const;

        /** Reads the extrapolated timestamp.
         * @param now Monotonic current time.
         * @return Server Unix seconds, or zero before synchronization.
         */
        std::uint64_t unix_seconds(TimePoint now = std::chrono::steady_clock::now()) const;

        /** Formats server wall time using the server's UTC offset.
         * @param now Monotonic current time.
         * @return HH:MM:SS, or --:--:-- before synchronization.
         */
        std::string time_text(TimePoint now = std::chrono::steady_clock::now()) const;

        /** Formats the current account-session duration.
         * @param now Monotonic current time.
         * @return HH:MM:SS with unbounded hours, or --:--:-- before synchronization.
         */
        std::string online_text(TimePoint now = std::chrono::steady_clock::now()) const;

        /** Formats the next selected event countdown.
         * @param id Stable event ID; empty means no selection.
         * @param now Monotonic current time.
         * @return DD:HH:MM:SS, zero when unavailable or elapsed.
         */
        std::string countdown_text(std::string_view id,
                                   TimePoint now = std::chrono::steady_clock::now()) const;

        /** @return Latest server-owned event list. */
        const std::vector<protocol::ClockEvent>& events() const;

      private:
        std::uint64_t elapsed(TimePoint now) const;
        protocol::ClockSyncFields m_snapshot;
        TimePoint m_received{};
        bool m_ready = false;
    };
}  // namespace openkal::client
