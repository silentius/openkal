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
#include <string>
#include <string_view>
namespace openkal::server {
    /** Validates and lowercases ASCII account usernames. @param value Input. @return Canonical
     * username or empty. */
    [[nodiscard]] std::string normalize_account_name(std::string_view value);

    /** Validates and lowercases development-account email addresses. @param value Input. @return
     * Canonical address or empty. */
    [[nodiscard]] std::string normalize_account_email(std::string_view value);

    /** Validates password byte bounds. @param value Password. @return True for 12..128 bytes
     * without controls. */
    [[nodiscard]] bool valid_account_password(std::string_view value);

    /** Derives a versioned salted scrypt hash. @param password Password. @return Encoded hash or
     * empty on failure. */
    [[nodiscard]] std::string hash_account_password(std::string_view password);

    /** Verifies a password in constant time after derivation. @param password Candidate. @param
     * encoded Stored hash. @return True on match. */
    [[nodiscard]] bool verify_account_password(std::string_view password, std::string_view encoded);
}  // namespace openkal::server
