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

#include "openkal/server/net/Transport.h"

#include <string>

namespace openkal::client {
    /** Resolves client trust without changing the working directory or environment.
     * @param host Server IPv4 address.
     * @param executable_dir Directory containing the client executable.
     * @param resource_root Installed or build-tree resource directory.
     * @param development_certificate Optional local development certificate, used only on loopback.
     * @param configured Explicit TLS settings, defaulting to environment overrides.
     * @return TLS settings with explicit, bundled, local development or system trust.
     */
    [[nodiscard]] server::TlsConfig
    resolve_client_tls_config(const std::string& host,
                              const std::string& executable_dir,
                              const std::string& resource_root,
                              const std::string& development_certificate,
                              server::TlsConfig configured = server::tls_config_from_environment());

    /** Resolves TLS settings for this client installation or development build.
     * @param host Server IPv4 address.
     * @param server_name Expected identity supplied with the client configuration package.
     * @return TLS settings for the selected server.
     */
    [[nodiscard]] server::TlsConfig client_tls_config(const std::string& host,
                                                      const std::string& server_name);
}  // namespace openkal::client
