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

namespace openkal::protocol {

    /** Usage line for /level. */
    inline constexpr const char* kLevelUsage = "Usage: /level [+=N|-=N|=N]";

    /** How /level should change the character level. */
    enum class LevelOp {
        /** Add `value` levels (empty argument is +1). */
        Add,
        /** Subtract `value` levels and rebuild stats/skills. */
        Subtract,
        /** Set the character to level `value` and rebuild stats/skills. */
        Set,
    };

    /** Parsed /level arguments. */
    struct LevelUpArgs {
        /** Add, subtract, or set. */
        LevelOp op = LevelOp::Add;

        /** Amount to add/subtract, or the target level for Set. */
        int value = 1;
    };

    /** Parses /level argument combinations. */
    class LevelUp final {
      public:
        LevelUp() = delete;

        /**
         * Parses /level arguments.
         *
         * Supported forms:
         * - (empty) → +=1
         * - `+=N` → add N levels
         * - `-=N` → subtract N levels
         * - `=N` → set level to N
         *
         * @param args Text after the command name.
         * @param out Parsed values on success.
         * @return True when the argument list is valid.
         */
        [[nodiscard]] static bool parse_args(std::string_view args, LevelUpArgs& out);
    };

}  // namespace openkal::protocol
