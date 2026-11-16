/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AutoLogin.h"

#include "openkal/client/AccountTextInput.h"
#include "openkal/platform/Json.h"
#include "openkal/server/AccountPassword.h"

#include <fstream>
#include <iterator>

namespace openkal::client {
    std::optional<AutoLoginCredentials> load_auto_login(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::nullopt;
        }
        const std::string text{std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>()};
        platform::Json root;
        if (file.bad() || !platform::Json::parse(text, root)) {
            return std::nullopt;
        }
        const auto& entries = root.at("login");
        if (!entries.is_array() || entries.array().empty()) {
            return std::nullopt;
        }
        const auto& entry = entries.array().front();
        const auto& id = entry.at("id");
        const auto& password = entry.at("password");
        AccountTextInput name;
        if (!id.is_string() || id.string().empty() || !name.insert(id.string(), 32) ||
            !password.is_string() || !server::valid_account_password(password.string())) {
            return std::nullopt;
        }
        return AutoLoginCredentials{id.string(), password.string()};
    }
}  // namespace openkal::client
