/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientTlsConfig.h"

#include "openkal/platform/Path.h"

#include <filesystem>
#include <utility>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT ""
#endif

#ifndef OPENKAL_DEVELOPMENT_TLS_CA
#define OPENKAL_DEVELOPMENT_TLS_CA ""
#endif

namespace openkal::client {
    namespace {
        bool physical_file(const std::string& path) {
            std::error_code error;
            return !path.empty() && std::filesystem::is_regular_file(path, error);
        }
    }  // namespace

    server::TlsConfig resolve_client_tls_config(const std::string& host,
                                                const std::string& executable_dir,
                                                const std::string& resource_root,
                                                const std::string& development_certificate,
                                                server::TlsConfig configured) {
        if (!configured.ca_file.empty()) {
            return configured;
        }
        for (const auto& directory : {executable_dir, resource_root}) {
            if (directory.empty()) {
                continue;
            }
            const auto bundled = platform::join_path(directory, "tls/ca.pem");
            if (physical_file(bundled)) {
                configured.ca_file = bundled;
                return configured;
            }
        }
        if (host == "127.0.0.1" &&
            (configured.server_name.empty() || configured.server_name == "localhost" ||
             configured.server_name == "127.0.0.1") &&
            physical_file(development_certificate)) {
            configured.ca_file = development_certificate;
        }
        return configured;
    }

    server::TlsConfig client_tls_config(const std::string& host, const std::string& server_name) {
        auto configured = server::tls_config_from_environment();
        if (configured.server_name.empty()) {
            configured.server_name = server_name;
        }
        return resolve_client_tls_config(host,
                                         platform::executable_directory(),
                                         OPENKAL_REPO_ROOT,
                                         OPENKAL_DEVELOPMENT_TLS_CA,
                                         std::move(configured));
    }
}  // namespace openkal::client
