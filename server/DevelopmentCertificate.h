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

namespace openkal::server {
    /** Creates missing localhost development credentials through OpenSSL, preserving existing
     * pairs.
     * @param directory Physical directory containing server.pem and server-key.pem.
     * @param error Failure description, without private key material.
     * @return True when a complete credential pair exists or was created.
     */
    [[nodiscard]] bool ensure_development_certificate(const std::string& directory,
                                                      std::string& error);
}  // namespace openkal::server
