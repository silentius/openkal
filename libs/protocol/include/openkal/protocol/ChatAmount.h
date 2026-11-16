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
#include <string_view>

namespace openkal::protocol {

    /** Usage line for /exp. */
    inline constexpr const char* kExpUsage = "Usage: /exp +=N|-=N|=N";

    /** Usage line for /eggexp. */
    inline constexpr const char* kEggExpUsage = "Usage: /eggexp +=N|-=N|=N";

    /** Usage line for /rage. */
    inline constexpr const char* kRageUsage = "Usage: /rage +=N|-=N|=N";

    /** How a numeric chat command should change a value. */
    enum class ChatAmountOp {
        /** Add `value` to the current amount. */
        Add,
        /** Subtract `value` from the current amount. */
        Subtract,
        /** Replace the current amount with `value`. */
        Set,
    };

    /** Parsed +=N / -=N / =N arguments. */
    struct ChatAmountArgs {
        /** Add, subtract, or set. */
        ChatAmountOp op = ChatAmountOp::Add;

        /** Amount to add/subtract, or the target for Set. */
        std::int64_t value = 0;
    };

    /** Parses shared +=N / -=N / =N chat-command arguments. */
    class ChatAmount final {
      public:
        ChatAmount() = delete;

        /**
         * Parses a single +=N, -=N, or =N token.
         *
         * Empty arguments are invalid. Add/subtract require N > 0; set allows N = 0.
         *
         * @param args Text after the command name.
         * @param out Parsed values on success.
         * @return True when the argument list is valid.
         */
        [[nodiscard]] static bool parse_args(std::string_view args, ChatAmountArgs& out);

        /**
         * Applies a parsed amount op to `current` with saturating int64 arithmetic.
         * @param current Value before the command.
         * @param parsed Parsed operator and amount.
         * @return Result before caller clamps to a domain (may be negative).
         */
        [[nodiscard]] static std::int64_t apply(std::int64_t current, const ChatAmountArgs& parsed);
    };

}  // namespace openkal::protocol
