/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Clock.h"

#include <chrono>
#include <ctime>

namespace openkal::platform {

    std::uint64_t Clock::monotonic_ms() {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    }

    std::int64_t Clock::unix_time_seconds() {
        return static_cast<std::int64_t>(std::time(nullptr));
    }

}  // namespace openkal::platform
