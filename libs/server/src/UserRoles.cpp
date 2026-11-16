/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/UserRoles.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <utility>

namespace openkal::server {
    bool UserRoles::load_file(const std::string& path, std::string& error) {
        m_commands = {};
        error.clear();
        const auto fail = [&error](std::string_view reason) {
            error = "UserRoles: " + std::string(reason);
            return false;
        };
        const auto bytes = platform::read_file(path);
        platform::Json root;
        if (!bytes || !platform::Json::parse(std::string(bytes->begin(), bytes->end()), root) ||
            !root.is_object() || root.at("schema").as_string() != "openkal.user_roles/v1" ||
            !root.at("roles").is_object()) {
            return fail("expected openkal.user_roles/v1 and a roles object");
        }
        for (const auto& entry : root.object()) {
            if (entry.first != "schema" && entry.first != "roles") {
                return fail("unknown configuration field");
            }
        }
        auto commands = m_commands;
        for (const auto& entry : root.at("roles").object()) {
            if (entry.first != "1" && entry.first != "2" && entry.first != "3") {
                return fail("only roles 1, 2 and 3 may receive privileged commands");
            }
            const auto& definition = entry.second;
            if (!definition.is_object() || !definition.at("commands").is_array()) {
                return fail("each role requires a commands array");
            }
            for (const auto& field : definition.object()) {
                if (field.first != "commands") {
                    return fail("unknown role field");
                }
            }
            auto& grants = commands[static_cast<std::size_t>(entry.first[0] - '0')];
            for (const auto& command : definition.at("commands").array()) {
                if (!command.is_string() || command.string().empty()) {
                    return fail("command names must be nonempty strings");
                }
                for (const char ch : command.string()) {
                    if ((ch < 'a' || ch > 'z') && (ch < '0' || ch > '9') && ch != '_') {
                        return fail("command names must be lowercase tokens without a slash");
                    }
                }
                if (!grants.insert(command.string()).second) {
                    return fail("duplicate command grant");
                }
            }
        }
        m_commands = std::move(commands);
        return true;
    }

    bool UserRoles::allows(UserRole role, std::string_view command) const {
        const auto index = static_cast<std::int32_t>(role);
        return index > 0 && index < static_cast<std::int32_t>(m_commands.size()) &&
               m_commands[static_cast<std::size_t>(index)].count(std::string(command)) != 0;
    }
}  // namespace openkal::server
