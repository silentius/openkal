/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientCpuProfiler.h"

#include "openkal/platform/Log.h"

#if defined(OPENKAL_USE_GPERFTOOLS)
#include <gperftools/profiler.h>
#endif

#include <cstdlib>
#include <string>

namespace openkal::client {

    ClientCpuProfiler::ClientCpuProfiler() {
#if defined(OPENKAL_USE_GPERFTOOLS)
        const char* path = std::getenv("CPUPROFILE");
        const char* profile_path = path != nullptr && path[0] != '\0' ? path : "openkal_cpu.prof";
        ProfilerStart(profile_path);
        m_active = true;
        log::info(std::string("gperftools CPU profile → ") + profile_path);

        if (const char* seconds = std::getenv("OPENKAL_PROFILE_SECONDS")) {
            const int value = std::atoi(seconds);
            if (value > 0) {
                m_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(value);
                log::info("OPENKAL_PROFILE_SECONDS=" + std::to_string(value) + " (auto-quit)");
            }
        }
#endif
    }

    ClientCpuProfiler::~ClientCpuProfiler() {
#if defined(OPENKAL_USE_GPERFTOOLS)
        if (m_active) {
            ProfilerStop();
        }
#endif
    }

    bool ClientCpuProfiler::deadline_reached() const {
        return m_deadline && std::chrono::steady_clock::now() >= *m_deadline;
    }

}  // namespace openkal::client
