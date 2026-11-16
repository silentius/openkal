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

#include <chrono>
#include <optional>

namespace openkal::client {

    /** Owns optional compatibility behavior profiling and its profile-test deadline. */
    class ClientCpuProfiler {
      public:
        /** Starts profiling when this executable was built with gperftools support. */
        ClientCpuProfiler();

        /** Stops an active compatibility behavior profiler. */
        ~ClientCpuProfiler();

        ClientCpuProfiler(const ClientCpuProfiler&) = delete;
        ClientCpuProfiler& operator=(const ClientCpuProfiler&) = delete;

        /** Reports whether OPENKAL_PROFILE_SECONDS has elapsed.
         * @return True only after the configured deadline.
         */
        [[nodiscard]] bool deadline_reached() const;

      private:
        bool m_active = false;
        std::optional<std::chrono::steady_clock::time_point> m_deadline;
    };

}  // namespace openkal::client
