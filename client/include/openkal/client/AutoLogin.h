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

#include <optional>
#include <string>

namespace openkal::client {
    /** Credentials from the first account in the optional local auto-login file. */
    struct AutoLoginCredentials {
        std::string id;
        std::string password;
    };

    /** Reads credentials using the same input limits as the multiplayer login form.
     * @param path Physical file path, relative to the process working directory by default.
     * @return First login entry, or no value for missing, malformed or invalid input.
     */
    [[nodiscard]] std::optional<AutoLoginCredentials>
    load_auto_login(const std::string& path = "auto_login.json");
}  // namespace openkal::client
