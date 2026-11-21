/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "DevelopmentCertificate.h"
#include "ServerStartupConfig.h"
#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    void write(const std::filesystem::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary);
        file << text;
        file.close();
        openkal::test::expect(file.good(), "write owned synthetic startup fixture");
    }

    std::string read(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
}  // namespace

int main(int argc, char** argv) {
    std::string error;
    if (argc == 3 && std::string(argv[1]) == "--generate") {
        if (openkal::server::ensure_development_certificate(argv[2], error)) {
            return 0;
        }
        std::cerr << error << '\n';
        return 1;
    }
    using openkal::server::load_server_startup;
    using openkal::test::expect;
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
                      ("openkal_server_startup_" + std::to_string(getpid()));
    std::filesystem::create_directories(root / "development/save/multiplayer");
    std::filesystem::create_directories(root / "bin");
    std::filesystem::create_directories(root / "working");
    std::filesystem::current_path(root / "working");
    openkal::server::ServerStartupOptions options;
    options.executable_dir = (root / "bin").string();
    options.development_root = (root / "development").string();
    expect(!load_server_startup(options, error) && !error.empty(),
           "missing database credentials produce an actionable failure");
    expect(!std::filesystem::exists(root / "development/save/multiplayer/tls"),
           "failed database setup does not create TLS files");
    write(root / "development/save/postgres.env",
          "# owned synthetic credentials\r\nPOSTGRES_DB=fixture\r\nPOSTGRES_USER=tester\r\n"
          "POSTGRES_PASSWORD=space quote'back\\slash=tail\r\n");
    auto result = load_server_startup(options, error);
    expect(result && error.empty() && result->allow_client_registration,
           "development startup works without a policy file or environment variables");
    if (!result) {
        return 1;
    }
    expect(result->database_dsn == "host=127.0.0.1 port=5432 connect_timeout=5 dbname='fixture' "
                                   "user='tester' password='space quote\\'back\\\\slash=tail'",
           "database values are quoted literally without shell evaluation");
    expect(result->user_roles_path == (root / "development/config/server/user_roles.json").string(),
           "development startup uses the repository role policy");
    expect(result->castle_war_path.empty(), "Castle War requires an explicit policy path");
    const auto certificate = read(result->tls.certificate_file);
    const auto key = read(result->tls.private_key_file);
    expect(certificate.find("BEGIN CERTIFICATE") != std::string::npos &&
               key.find("BEGIN PRIVATE KEY") != std::string::npos,
           "OpenSSL generated a complete PEM credential pair");
    struct stat permissions {};
    expect(::stat(result->tls.private_key_file.c_str(), &permissions) == 0 &&
               (permissions.st_mode & 0777) == 0600,
           "new development private keys are owner-only");
    auto again = load_server_startup(options, error);
    expect(again && read(again->tls.certificate_file) == certificate &&
               read(again->tls.private_key_file) == key,
           "restarting preserves the development identity");
    write(root / "development/save/multiplayer/server.json",
          R"({"allow_client_registration":false})");
    result = load_server_startup(options, error);
    expect(result && !result->allow_client_registration,
           "existing server registration policy is respected");
    const auto development_tls = result->tls;
    const auto working_config = root / "working/server.json";
    write(working_config, R"({"allow_client_registration":false,"database_dsn":"local fixture"})");
    result = load_server_startup(options, error);
    expect(result && result->database_dsn == "local fixture" &&
               result->configuration_path == working_config.string(),
           "working-directory server.json overrides development defaults");
    options.database_dsn = "explicit fixture";
    options.tls = development_tls;
    result = load_server_startup(options, error);
    expect(result && result->database_dsn == "explicit fixture" &&
               result->tls.private_key_file == development_tls.private_key_file,
           "environment overrides retain precedence without being changed");
    write(
        working_config,
        R"({"allow_client_registration":false,"plants":{"types":{"aloe_vera":{"entity_index":65000,"item_index":23027,"items_per_pick":4,"enabled":true,"max_plants":25,"plants_per_area":3,"respawn_seconds":600}}}})");
    result = load_server_startup(options, error);
    expect(result && result->plants && result->plants->types.size() == 1 &&
               result->plants->types.front().items_per_pick == 4 &&
               result->plants->types.front().max_plants == 25 &&
               result->plants->types.front().plants_per_area == 3,
           "server.json controls the gathering population");
    write(
        working_config,
        R"({"allow_client_registration":false,"plants":{"types":{"aloe_vera":{"entity_index":65000,"item_index":23027,"enabled":true,"max_plants":-2}}}})");
    expect(!load_server_startup(options, error), "invalid gathering settings reject startup");
    write(working_config,
          R"({"allow_client_registration":false,"guild_rules_file":"owner/guilds.json"})");
    result = load_server_startup(options, error);
    expect(result && result->guild_rules_path ==
                         (working_config.parent_path() / "owner/guilds.json").string(),
           "owner guild path is relative to the selected server.json");
    write(working_config,
          R"({"allow_client_registration":false,"castle_war_file":"owner/castle_war.json"})");
    result = load_server_startup(options, error);
    expect(result && result->castle_war_path ==
                         (working_config.parent_path() / "owner/castle_war.json").string(),
           "Castle War policy is resolved relative to the selected server.json");
    write(working_config,
          R"({"allow_client_registration":false,"assassin_safe_areas":[[0, 100, 200, 300, 400]]})");
    result = load_server_startup(options, error);
    expect(result && result->assassin_safe_areas.size() == 1 &&
               result->assassin_safe_areas.front() == std::array<int, 5>{0, 100, 200, 300, 400},
           "read inclusive Assassin safe areas in server coordinates");
    write(working_config,
          R"({"allow_client_registration":false,"assassin_safe_areas":[[0, 300, 200, 100, 400]]})");
    expect(!load_server_startup(options, error), "reject inverted Assassin safe area bounds");
    write(working_config, R"({"allow_client_registration":false,"castle_war_file":false})");
    expect(!load_server_startup(options, error), "Castle War policy requires a file path");
    write(working_config,
          R"({"allow_client_registration":false,"user_roles_file":"owner/user_roles.json"})");
    result = load_server_startup(options, error);
    expect(result && result->user_roles_path ==
                         (working_config.parent_path() / "owner/user_roles.json").string(),
           "role policy is resolved relative to server.json");
    write(working_config, R"({"allow_client_registration":false,"user_roles_file":true})");
    expect(!load_server_startup(options, error), "role policy path must be a nonempty string");
    write(root / "working/management.token", std::string(32, 'm') + "\n");
    write(
        working_config,
        R"({"allow_client_registration":false,"management_socket":"management.sock","management_token_file":"management.token"})");
    result = load_server_startup(options, error);
    expect(result && result->management_socket == (root / "working/management.sock").string() &&
               result->management_token == std::string(32, 'm'),
           "management resolves local socket and private token file relative to server settings");
    write(root / "working/management.token", std::string(256, 'm') + "\r\n");
    result = load_server_startup(options, error);
    expect(result && result->management_token.size() == 256,
           "maximum-length management tokens allow a trailing line ending");
    write(root / "working/management.token", "too-short\n");
    expect(!load_server_startup(options, error) && error.find("too-short") == std::string::npos,
           "invalid management secret rejects startup without exposing it");
    write(working_config,
          R"({"allow_client_registration":false,"management_socket":"management.sock"})");
    expect(!load_server_startup(options, error),
           "management cannot be enabled without its token file");
    write(working_config, R"({"allow_client_registration":"invalid"})");
    expect(!load_server_startup(options, error) &&
               error.find("explicit fixture") == std::string::npos,
           "invalid policy fails without exposing database credentials");
    options.configuration_path = (root / "absent.json").string();
    expect(!load_server_startup(options, error), "explicit missing config does not fall back");

    options.configuration_path.clear();
    options.development_root.clear();
    options.database_dsn.clear();
    options.tls = {};
    std::filesystem::remove(working_config);
    expect(!load_server_startup(options, error),
           "deployment requires a private server configuration");
    std::filesystem::create_directories(root / "bin/tls");
    write(root / "bin/postgres.env",
          "POSTGRES_USER=tester\nPOSTGRES_DB=fixture\nPOSTGRES_PASSWORD=test\n");
    write(root / "bin/server.json", R"({"allow_client_registration":false})");
    expect(!load_server_startup(options, error) &&
               !std::filesystem::exists(root / "bin/tls/server-key.pem"),
           "deployment never generates or replaces TLS credentials");
    write(root / "bin/tls/server.pem", certificate);
    write(root / "bin/tls/server-key.pem", key);
    result = load_server_startup(options, error);
    expect(result && !result->allow_client_registration &&
               result->tls.certificate_file == (root / "bin/tls/server.pem").string(),
           "deployment discovers settings beside the binary and resolves relative paths there");
    std::filesystem::create_directories(root / "incomplete");
    write(root / "incomplete/server.pem", certificate);
    expect(
        !openkal::server::ensure_development_certificate((root / "incomplete").string(), error) &&
            read(root / "incomplete/server.pem") == certificate &&
            !std::filesystem::exists(root / "incomplete/server-key.pem"),
        "a partial credential pair is preserved instead of silently replaced");
    expect(std::filesystem::current_path() == root / "working",
           "startup preserves the working directory");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
