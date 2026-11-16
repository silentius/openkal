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

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cmath>

namespace openkal::client {
    std::optional<ClientServerConfig>
    load_client_server_config(const std::string& configuration_root) {
        const auto bytes =
            platform::File::read(platform::join_path(configuration_root, "server/client.json"));
        platform::Json root;
        if (!bytes || !platform::Json::parse(std::string(bytes->begin(), bytes->end()), root)) {
            return std::nullopt;
        }
        const auto& host = root.at("host");
        const auto& port = root.at("port");
        const auto& name = root.at("tls_server_name");
        in_addr address{};
        if (!host.is_string() || host.string().find('\0') != std::string::npos ||
            inet_pton(AF_INET, host.string().c_str(), &address) != 1 || !port.is_number() ||
            !std::isfinite(port.number()) || port.number() < 1 || port.number() > 65535 ||
            std::floor(port.number()) != port.number() ||
            (!name.is_null() &&
             (!name.is_string() || name.string().size() > 253 ||
              !std::all_of(name.string().begin(), name.string().end(), [](unsigned char c) {
                  return c > 32 && c < 127;
              })))) {
            return std::nullopt;
        }
        return ClientServerConfig{
            host.string(), static_cast<std::uint16_t>(port.number()), name.as_string()};
    }
}  // namespace openkal::client
