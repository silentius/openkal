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

namespace openkal::platform {

    /** Provides stateless clock utility operations. */
    class Clock final {
      public:
        /** Disables construction of this static utility class. */
        Clock() = delete;

        /** Gets monotonic milliseconds since an arbitrary epoch.
         * @return Monotonic milliseconds.
         */
        [[nodiscard]] static std::uint64_t monotonic_ms();

        /** Gets wall-clock Unix time.
         * @return Seconds since the Unix epoch.
         */
        [[nodiscard]] static std::int64_t unix_time_seconds();
    };

    /** Compatibility wrapper for Clock::monotonic_ms.
     * @return Monotonic milliseconds.
     */
    [[nodiscard]] inline std::uint64_t monotonic_ms() {
        return Clock::monotonic_ms();
    }

    /** Compatibility wrapper for Clock::unix_time_seconds.
     * @return Seconds since the Unix epoch.
     */
    [[nodiscard]] inline std::int64_t unix_time_seconds() {
        return Clock::unix_time_seconds();
    }

}  // namespace openkal::platform
