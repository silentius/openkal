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
#include <optional>
#include <string>

namespace openkal::client {
    /** Single multiplayer endpoint supplied with this client's configuration package. */
    struct ClientServerConfig {
        std::string host;
        std::uint16_t port = 0;
        std::string tls_server_name;
    };

    /** Reads server/client.json from the active configuration source.
     * @param configuration_root Loose or package-backed configuration directory.
     * @return Validated IPv4 endpoint, or no value when missing or malformed.
     */
    [[nodiscard]] std::optional<ClientServerConfig>
    load_client_server_config(const std::string& configuration_root);
}  // namespace openkal::client
