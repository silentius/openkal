/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientServerConfig.h"
#include "openkal/client/ClientTlsConfig.h"
#include "openkal/platform/ConfigArchive.h"
#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <unistd.h>

int main() {
    using openkal::client::load_client_server_config;
    using openkal::client::resolve_client_tls_config;
    using openkal::test::expect;
    const auto root = std::filesystem::temp_directory_path() /
                      ("openkal_connection_config_" + std::to_string(getpid()));
    const auto binary = (root / "bin").string(), resource = (root / "resources").string();
    const auto development = (root / "development.pem").string();
    std::filesystem::create_directories(root / "bin/tls");
    std::filesystem::create_directories(root / "resources/tls");
    std::filesystem::create_directories(root / "config/server");
    const auto write = [](const std::string& path, const std::string& text) {
        std::ofstream file(path);
        file << text;
        file.close();
        expect(file.good(), "write owned connection fixture");
    };
    write(development, "synthetic trust selection fixture");
    const auto resolve = [&](const std::string& host, openkal::server::TlsConfig tls = {}) {
        return resolve_client_tls_config(host, binary, resource, development, std::move(tls));
    };
    expect(resolve("127.0.0.1").ca_file == development,
           "local development works without TLS environment variables");
    expect(resolve("192.0.2.1").ca_file.empty(), "remote servers use system trust by default");
    openkal::server::TlsConfig explicit_trust;
    explicit_trust.ca_file = "missing-explicit.pem";
    explicit_trust.server_name = "server.example.test";
    expect(resolve("127.0.0.1", explicit_trust).ca_file == explicit_trust.ca_file,
           "explicit trust is never silently replaced, even when missing");
    explicit_trust.ca_file.clear();
    expect(resolve("127.0.0.1", explicit_trust).ca_file.empty(),
           "local development trust requires a matching local identity");
    write(resource + "/tls/ca.pem", "synthetic resource trust fixture");
    expect(resolve("192.0.2.1").ca_file == resource + "/tls/ca.pem",
           "installed resource trust works for remote servers");
    write(binary + "/tls/ca.pem", "synthetic executable trust fixture");
    expect(resolve("127.0.0.1").ca_file == binary + "/tls/ca.pem",
           "executable bundle takes precedence over resource and development trust");
    expect(resolve("192.0.2.1", explicit_trust).server_name == explicit_trust.server_name,
           "expected DNS identity is preserved for bundled trust");

    const auto configuration = (root / "config").string();
    const auto path = configuration + "/server/client.json";
    expect(!load_client_server_config(configuration), "missing endpoint cannot select a server");
    for (const std::string text : {"{",
                                   "{}",
                                   R"({"host":"localhost","port":30001})",
                                   R"({"host":"127.0.0.1","port":0})",
                                   R"({"host":"127.0.0.1","port":65536})",
                                   R"({"host":"127.0.0.1","port":30001.5})",
                                   R"({"host":"127.0.0.1","port":"30001"})",
                                   R"({"host":"127.0.0.1\u0000extra","port":30001})",
                                   R"({"host":"127.0.0.1","port":30001,"tls_server_name":42})"}) {
        write(path, text);
        expect(!load_client_server_config(configuration), "invalid endpoint is rejected");
    }
    write(path, R"({"host":"192.0.2.1","port":12345,"tls_server_name":"server.example.test"})");
    auto endpoint = load_client_server_config(configuration);
    expect(endpoint && endpoint->host == "192.0.2.1" && endpoint->port == 12345 &&
               endpoint->tls_server_name == "server.example.test",
           "read address, port and certificate identity from the configuration source");
    const auto virtual_root = (root / "package").string();
    expect(openkal::platform::ConfigArchive::mount(virtual_root, OPENKAL_TEST_CONFIG_ARCHIVE),
           "mount the real build package without loose configuration");
    endpoint = load_client_server_config(virtual_root + "/config");
    expect(endpoint && endpoint->host == "127.0.0.1" && endpoint->port == 30001 &&
               endpoint->tls_server_name == "localhost",
           "client endpoint is present and readable inside config.pak");
    openkal::platform::ConfigArchive::unmount();
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
