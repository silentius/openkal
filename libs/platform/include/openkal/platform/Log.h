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

#include <string_view>

namespace openkal::log {

    /** Log message severity. */
    enum class Level { debug, info, warn, error };

    /** Provides stateless logging utility operations. */
    class Log final {
      public:
        /** Disables construction of this static utility class. */
        Log() = delete;

        /** Sets the minimum emitted log level.
         * @param level Minimum log level.
         */
        static void set_level(Level level);

        /** Enables local date/time prefixes with milliseconds and UTC offset.
         * @param enabled True to include timestamps in subsequent log messages.
         */
        static void set_timestamps(bool enabled);

        /** Writes a message at the specified level.
         * @param level Message level.
         * @param message Message text.
         */
        static void write(Level level, std::string_view message);

        /** Writes an informational message.
         * @param message Message text.
         */
        static void info(std::string_view message);

        /** Writes a warning message.
         * @param message Message text.
         */
        static void warn(std::string_view message);

        /** Writes an error message.
         * @param message Message text.
         */
        static void error(std::string_view message);

        /** Writes a debug message.
         * @param message Message text.
         */
        static void debug(std::string_view message);
    };

    /** Compatibility wrapper for Log::set_level.
     * @param level Minimum log level.
     */
    inline void set_level(Level level) {
        Log::set_level(level);
    }

    /** Compatibility wrapper for Log::write.
     * @param level Message level.
     * @param message Message text.
     */
    inline void write(Level level, std::string_view message) {
        Log::write(level, message);
    }

    /** Compatibility wrapper for Log::info.
     * @param message Message text.
     */
    inline void info(std::string_view message) {
        Log::info(message);
    }

    /** Compatibility wrapper for Log::warn.
     * @param message Message text.
     */
    inline void warn(std::string_view message) {
        Log::warn(message);
    }

    /** Compatibility wrapper for Log::error.
     * @param message Message text.
     */
    inline void error(std::string_view message) {
        Log::error(message);
    }

    /** Compatibility wrapper for Log::debug.
     * @param message Message text.
     */
    inline void debug(std::string_view message) {
        Log::debug(message);
    }

}  // namespace openkal::log
