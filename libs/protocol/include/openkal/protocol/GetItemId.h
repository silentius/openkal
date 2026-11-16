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

    /** Usage line for /getitemid. */
    inline constexpr const char* kGetItemIdUsage =
        "Usage: /getitemid <index> [count] | /getitemid <index> <prefix> <count>";

    /** Parsed /getitemid arguments. */
    struct GetItemIdArgs {
        /** item definition index. */
        int index = 0;

        /** Prefix index, or 0. */
        int prefix = 0;

        /** Stack count (non-stackable items consume this many slots). */
        int count = 1;
    };

    /** Parses /getitemid argument combinations. */
    class GetItemId final {
      public:
        GetItemId() = delete;

        /**
         * Parses /getitemid arguments.
         *
         * Supported forms:
         * - `<index>` → count 1, prefix 0
         * - `<index> <count>` → prefix 0
         * - `<index> <prefix> <count>`
         *
         * @param args Text after the command name.
         * @param out Parsed values on success.
         * @return True when the argument list is valid.
         */
        [[nodiscard]] static bool parse_args(std::string_view args, GetItemIdArgs& out);
    };

}  // namespace openkal::protocol
