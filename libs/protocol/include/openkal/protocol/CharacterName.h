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
#include <string_view>

namespace openkal::protocol {
    /** Maximum number of ASCII letters and digits in a player name. */
    inline constexpr std::size_t kCharacterNameMaximum = 14;

    /** Checks a player-name character without locale-dependent character classes.
     * @param value Input character.
     * @return Whether the character is an ASCII letter or digit.
     */
    [[nodiscard]] constexpr bool character_name_character(char value) {
        return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
               (value >= '0' && value <= '9');
    }

    /** Validates the same player-name policy in the client and lobby service.
     * @param name Requested player name.
     * @return Whether the name contains 2 to 14 ASCII letters and digits.
     */
    [[nodiscard]] constexpr bool character_name_valid(std::string_view name) {
        if (name.size() < 2 || name.size() > kCharacterNameMaximum) {
            return false;
        }
        for (const char value : name) {
            if (!character_name_character(value)) {
                return false;
            }
        }
        return true;
    }
}  // namespace openkal::protocol
