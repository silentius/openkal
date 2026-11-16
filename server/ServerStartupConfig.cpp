/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ServerStartupConfig.h"

#include "DevelopmentCertificate.h"
#include "openkal/platform/Json.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

namespace openkal::server {
    namespace {
        bool physical_file(const std::filesystem::path& path) {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        }

        std::string dsn_quote(const std::string& value) {
            std::string quoted = "'";
            for (const char c : value) {
                if (c == '\'' || c == '\\') {
                    quoted += '\\';
                }
                quoted += c;
            }
            return quoted + '\'';
        }

        std::optional<std::string> database_credentials(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return std::nullopt;
            }
            std::map<std::string, std::string> values;
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.empty() || line.front() == '#') {
                    continue;
                }
                const auto equals = line.find('=');
                if (equals == std::string::npos || line.find('\0') != std::string::npos) {
                    return std::nullopt;
                }
                const auto name = line.substr(0, equals);
                if (!values.emplace(name, line.substr(equals + 1)).second) {
                    return std::nullopt;
                }
            }
            if (file.bad() || values["POSTGRES_DB"].empty() || values["POSTGRES_USER"].empty() ||
                values["POSTGRES_PASSWORD"].empty()) {
                return std::nullopt;
            }
            return "host=127.0.0.1 port=5432 connect_timeout=5 dbname=" +
                   dsn_quote(values["POSTGRES_DB"]) +
                   " user=" + dsn_quote(values["POSTGRES_USER"]) +
                   " password=" + dsn_quote(values["POSTGRES_PASSWORD"]);
        }
    }  // namespace

    std::optional<ServerStartupConfig> load_server_startup(const ServerStartupOptions& options,
                                                           std::string& error) {
        error.clear();
        const bool development = !options.development_root.empty();
        std::filesystem::path path = options.configuration_path;
        if (path.empty()) {
            if (physical_file("server.json")) {
                path = std::filesystem::absolute("server.json");
            } else if (physical_file(std::filesystem::path(options.executable_dir) /
                                     "server.json")) {
                path = std::filesystem::path(options.executable_dir) / "server.json";
            } else if (development) {
                path = std::filesystem::path(options.development_root) /
                       "save/multiplayer/server.json";
            } else {
                error = "Create a private server.json beside MainSvr or in its working directory; "
                        "see server.example.json.";
                return std::nullopt;
            }
        }
        path = std::filesystem::absolute(path).lexically_normal();
        platform::Json config;
        if (physical_file(path)) {
            std::ifstream file(path, std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>()};
            if (!file || !platform::Json::parse(text, config) || !config.is_object() ||
                config.at("allow_client_registration").type() != platform::Json::Type::Bool) {
                error =
                    "Invalid server configuration: allow_client_registration must be a boolean.";
                return std::nullopt;
            }
        } else if (!development || !options.configuration_path.empty()) {
            error = "Could not read the requested server configuration file.";
            return std::nullopt;
        }
        for (const auto* field : {"guild_rules_file",
                                  "castle_war_file",
                                  "user_roles_file",
                                  "management_socket",
                                  "management_token_file",
                                  "database_dsn",
                                  "database_credentials_file",
                                  "tls_certificate_file",
                                  "tls_private_key_file"}) {
            const auto* value = config.get(field);
            if (value && (!value->is_string() || value->string().empty() ||
                          value->string().find('\0') != std::string::npos)) {
                error = std::string("Invalid server configuration field: ") + field;
                return std::nullopt;
            }
        }
        const auto local_path = [&](const std::string& value) {
            return (path.parent_path() / value).lexically_normal();
        };
        ServerStartupConfig result;
        if (const auto* plants = config.get("plants")) {
            result.plants = parse_harvest_config(*plants);
            if (!result.plants) {
                error = "Invalid server configuration: plants settings are invalid.";
                return std::nullopt;
            }
        }
        result.configuration_path = path.string();
        if (const auto* guilds = config.get("guild_rules_file")) {
            result.guild_rules_path = local_path(guilds->string()).string();
        } else if (development) {
            result.guild_rules_path =
                (std::filesystem::path(options.development_root) / "config/server/guilds.json")
                    .string();
        } else if (physical_file(local_path("guilds.json"))) {
            result.guild_rules_path = local_path("guilds.json").string();
        }
        if (const auto* castle_war = config.get("castle_war_file")) {
            result.castle_war_path = local_path(castle_war->string()).string();
        }
        if (const auto* roles = config.get("user_roles_file")) {
            result.user_roles_path = local_path(roles->string()).string();
        } else if (development) {
            result.user_roles_path =
                (std::filesystem::path(options.development_root) / "config/server/user_roles.json")
                    .string();
        } else if (physical_file(local_path("user_roles.json"))) {
            result.user_roles_path = local_path("user_roles.json").string();
        }
        const auto* management_socket = config.get("management_socket");
        const auto* management_token_file = config.get("management_token_file");
        if ((management_socket != nullptr) != (management_token_file != nullptr)) {
            error = "Management requires both management_socket and management_token_file.";
            return std::nullopt;
        }
        if (management_socket) {
            result.management_socket = local_path(management_socket->string()).string();
            std::ifstream token_file(local_path(management_token_file->string()), std::ios::binary);
            std::string token;
            char ch;
            while (token.size() <= 258 && token_file.get(ch)) {
                token.push_back(ch);
            }
            while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) {
                token.pop_back();
            }
            if (!token_file.eof() || token.size() < 32 || token.size() > 256 ||
                token.find('\0') != std::string::npos) {
                error =
                    "Cannot read a 32-256 byte management token from the selected private file.";
                return std::nullopt;
            }
            result.management_token = std::move(token);
        }
        result.allow_client_registration =
            config.is_null() ? development : config.at("allow_client_registration").boolean();
        result.database_dsn = options.database_dsn.empty() ? config.at("database_dsn").as_string()
                                                           : options.database_dsn;
        if (result.database_dsn.empty()) {
            const auto credentials = config.get("database_credentials_file");
            const auto credential_path =
                credentials ? local_path(credentials->string())
                : development
                    ? std::filesystem::path(options.development_root) / "save/postgres.env"
                    : local_path("postgres.env");
            const auto dsn = database_credentials(credential_path);
            if (!dsn) {
                error = "Cannot read database credentials: supply POSTGRES_DB, POSTGRES_USER and "
                        "POSTGRES_PASSWORD in postgres.env, database_dsn in server.json, or "
                        "OPENKAL_DATABASE_DSN.";
                return std::nullopt;
            }
            result.database_dsn = *dsn;
        }
        result.tls = options.tls;
        if (result.tls.certificate_file.empty()) {
            result.tls.certificate_file = config.at("tls_certificate_file").as_string();
        } else {
            result.tls.certificate_file =
                std::filesystem::absolute(result.tls.certificate_file).string();
        }
        if (result.tls.private_key_file.empty()) {
            result.tls.private_key_file = config.at("tls_private_key_file").as_string();
        } else {
            result.tls.private_key_file =
                std::filesystem::absolute(result.tls.private_key_file).string();
        }
        if (result.tls.certificate_file.empty() && result.tls.private_key_file.empty() &&
            development) {
            const auto directory =
                std::filesystem::path(options.development_root) / "save/multiplayer/tls";
            if (!ensure_development_certificate(directory.string(), error)) {
                return std::nullopt;
            }
            result.tls.certificate_file = (directory / "server.pem").string();
            result.tls.private_key_file = (directory / "server-key.pem").string();
        } else {
            if (result.tls.certificate_file.empty() && result.tls.private_key_file.empty()) {
                result.tls.certificate_file = "tls/server.pem";
                result.tls.private_key_file = "tls/server-key.pem";
            }
            result.tls.certificate_file = local_path(result.tls.certificate_file).string();
            result.tls.private_key_file = local_path(result.tls.private_key_file).string();
            if (!physical_file(result.tls.certificate_file) ||
                !physical_file(result.tls.private_key_file)) {
                error = "Supply both existing TLS certificate and private key files in server.json "
                        "or OPENKAL_TLS_CERT and OPENKAL_TLS_KEY.";
                return std::nullopt;
            }
        }
        return result;
    }
}  // namespace openkal::server
