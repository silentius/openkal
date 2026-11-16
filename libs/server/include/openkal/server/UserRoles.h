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

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace openkal::server {
    /** Persistent character privilege level; permissions are granted explicitly per role. */
    enum class UserRole : std::int32_t {
        Player = 0,
        Moderator = 1,
        CoAdmin = 2,
        Admin = 3,
    };

    /** Server-owned allowlists for privileged multiplayer chat commands. */
    class UserRoles {
      public:
        /** Loads explicit command grants, clearing permissions on any failure.
         * @param path Server-owned JSON configuration file.
         * @param error Failure description, empty on success.
         * @return Whether the complete configuration is valid. */
        [[nodiscard]] bool load_file(const std::string& path, std::string& error);

        /** Checks an explicit grant; players and invalid roles always have no privileges.
         * @param role Persisted character role.
         * @param command Lowercase command name without the slash.
         * @return Whether the role may execute this privileged command. */
        [[nodiscard]] bool allows(UserRole role, std::string_view command) const;

      private:
        /** Independent allowlists; roles do not inherit commands from other roles. */
        std::array<std::unordered_set<std::string>, 4> m_commands;
    };
}  // namespace openkal::server
