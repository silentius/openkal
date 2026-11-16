/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace openkal::log {
    namespace {

        Level g_level = Level::info;
        std::mutex g_mu;
        bool g_timestamps = false;

        const char* name(Level level) {
            switch (level) {
            case Level::debug:
                return "debug";
            case Level::info:
                return "info";
            case Level::warn:
                return "warn";
            case Level::error:
                return "error";
            }
            return "?";
        }

    }  // namespace

    void Log::set_level(Level level) {
        g_level = level;
    }

    void Log::set_timestamps(bool enabled) {
        std::lock_guard lock(g_mu);
        g_timestamps = enabled;
    }

    void Log::write(Level level, std::string_view message) {
        if (static_cast<int>(level) < static_cast<int>(g_level)) {
            return;
        }
        std::lock_guard lock(g_mu);
        if (g_timestamps) {
            const auto now = std::chrono::system_clock::now();
            const auto seconds = std::chrono::system_clock::to_time_t(now);
            const auto millis =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                    .count() %
                1000;
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &seconds);
#else
            localtime_r(&seconds, &local);
#endif
            char date[32]{}, offset[8]{};
            std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local);
            std::strftime(offset, sizeof(offset), "%z", &local);
            std::fprintf(stderr, "[%s.%03d %s] ", date, static_cast<int>(millis), offset);
        }
        std::fprintf(stderr,
                     "[openkal:%s] %.*s\n",
                     name(level),
                     static_cast<int>(message.size()),
                     message.data());
    }

    void Log::info(std::string_view message) {
        write(Level::info, message);
    }

    void Log::warn(std::string_view message) {
        write(Level::warn, message);
    }

    void Log::error(std::string_view message) {
        write(Level::error, message);
    }

    void Log::debug(std::string_view message) {
        write(Level::debug, message);
    }

}  // namespace openkal::log
