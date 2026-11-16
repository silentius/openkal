/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ServerClock.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace openkal::client {
    namespace {
        std::string duration_text(std::uint64_t seconds, bool days) {
            char text[64];
            const auto hours = seconds / 3600;
            if (days) {
                std::snprintf(text,
                              sizeof(text),
                              "%02llu:%02llu:%02llu:%02llu",
                              static_cast<unsigned long long>(hours / 24),
                              static_cast<unsigned long long>(hours % 24),
                              static_cast<unsigned long long>((seconds / 60) % 60),
                              static_cast<unsigned long long>(seconds % 60));
            } else {
                std::snprintf(text,
                              sizeof(text),
                              "%02llu:%02llu:%02llu",
                              static_cast<unsigned long long>(hours),
                              static_cast<unsigned long long>((seconds / 60) % 60),
                              static_cast<unsigned long long>(seconds % 60));
            }
            return text;
        }
    }  // namespace

    void ServerClock::synchronize(protocol::ClockSyncFields snapshot, TimePoint received) {
        m_snapshot = std::move(snapshot);
        m_received = received;
        m_ready = true;
    }

    void ServerClock::reset() {
        m_snapshot = {};
        m_received = {};
        m_ready = false;
    }

    bool ServerClock::ready() const {
        return m_ready;
    }

    std::uint64_t ServerClock::elapsed(TimePoint now) const {
        return static_cast<std::uint64_t>(std::max<std::int64_t>(
            0, std::chrono::duration_cast<std::chrono::seconds>(now - m_received).count()));
    }

    std::uint64_t ServerClock::unix_seconds(TimePoint now) const {
        return m_ready ? m_snapshot.unix_seconds + elapsed(now) : 0;
    }

    std::string ServerClock::time_text(TimePoint now) const {
        if (!m_ready) {
            return "--:--:--";
        }
        const auto seconds =
            static_cast<std::int64_t>(unix_seconds(now)) + m_snapshot.utc_offset_seconds;
        return duration_text(static_cast<std::uint64_t>((seconds % 86400 + 86400) % 86400), false);
    }

    std::string ServerClock::online_text(TimePoint now) const {
        return m_ready ? duration_text(m_snapshot.online_seconds + elapsed(now), false)
                       : "--:--:--";
    }

    std::string ServerClock::countdown_text(std::string_view id, TimePoint now) const {
        const auto current = unix_seconds(now);
        if (m_ready && !id.empty()) {
            for (const auto& event : m_snapshot.events) {
                if (event.id != id) {
                    continue;
                }
                const auto target =
                    current <= event.starts_at ? event.starts_at : event.following_at;
                if (target >= current) {
                    return duration_text(target - current, true);
                }
            }
        }
        return "00:00:00:00";
    }

    const std::vector<protocol::ClockEvent>& ServerClock::events() const {
        return m_snapshot.events;
    }
}  // namespace openkal::client
