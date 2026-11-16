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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::protocol {
    /** Next two server-calculated occurrences of a weekly event. */
    struct ClockEvent {
        /** Stable server-owned selection key. */
        std::string id;
        std::string name;
        /** Next occurrence, including the current second. */
        std::uint64_t starts_at = 0;
        /** Following occurrence, calculated with the server calendar. */
        std::uint64_t following_at = 0;
    };

    /** Authoritative server time and current account-session duration. */
    struct ClockSyncFields {
        std::uint64_t unix_seconds = 0;
        /** Server local wall time minus UTC at the snapshot instant. */
        std::int32_t utc_offset_seconds = 0;
        std::uint64_t online_seconds = 0;
        std::vector<ClockEvent> events;
    };

    /** Bounded OpenKal clock extension; all timestamps use Unix seconds. */
    class ClockSync final {
      public:
        ClockSync() = delete;
        static constexpr std::size_t kMaxEvents = 16;
        static constexpr std::uint64_t kIntervalMs = 3600000;

        /** Frames a validated snapshot.
         * @param fields Server snapshot.
         * @return Framed bytes, or an empty vector for invalid values.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame(const ClockSyncFields& fields);

        /** Parses a snapshot, preserving the output on failure.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param fields Validated output.
         * @return True for a complete valid payload.
         */
        [[nodiscard]] static bool
        parse(const std::uint8_t* data, std::size_t size, ClockSyncFields& fields);
    };
}  // namespace openkal::protocol
