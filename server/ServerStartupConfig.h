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

#include "openkal/server/HarvestPlants.h"
#include "openkal/server/net/Transport.h"

#include <optional>
#include <string>

namespace openkal::server {
    /** Inputs to standalone server setup, separate from packaged game configuration. */
    struct ServerStartupOptions {
        std::string executable_dir;
        std::string configuration_path;  ///< Explicit --multiplayer-config override.
        std::string development_root;    ///< Empty disables local development defaults.
        std::string database_dsn;        ///< Explicit environment override, never logged.
        TlsConfig tls;                   ///< Explicit TLS environment overrides.
    };

    /** Resolved private runtime settings for the dedicated multiplayer server. */
    struct ServerStartupConfig {
        std::string configuration_path;
        std::string database_dsn;
        TlsConfig tls;
        bool allow_client_registration = false;
        std::string guild_rules_path;
        /** Optional private Castle War rules, resolved relative to server.json. */
        std::string castle_war_path;
        /** Server-owned user role policy, resolved relative to server.json. */
        std::string user_roles_path;
        /** Optional local management endpoint and private authentication secret. */
        std::string management_socket;
        std::string management_token;
        std::optional<HarvestConfig> plants;
    };

    /** Loads physical server settings, local database credentials and TLS defaults.
     * @param options Paths and explicit environment settings.
     * @param error Failure description without credentials or file contents.
     * @return Prepared multiplayer settings, or no value on setup failure.
     */
    [[nodiscard]] std::optional<ServerStartupConfig>
    load_server_startup(const ServerStartupOptions& options, std::string& error);
}  // namespace openkal::server
