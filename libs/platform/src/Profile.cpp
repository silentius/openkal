/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Profile.h"

#include "openkal/platform/File.h"

#include <cstdlib>
#include <sstream>
#include <utility>

namespace openkal::platform {
    namespace {

        std::string trim(std::string value) {
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' ||
                                      value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            std::size_t index = 0;
            while (index < value.size() && (value[index] == ' ' || value[index] == '\t')) {
                ++index;
            }
            return value.substr(index);
        }

        bool parse_bool(const std::string& value) {
            return value == "1" || value == "true" || value == "TRUE" || value == "yes" ||
                   value == "on";
        }

    }  // namespace

    const std::string& Profile::name() const {
        return m_name;
    }

    void Profile::set_name(std::string value) {
        m_name = std::move(value);
    }

    const std::string& Profile::data_root() const {
        return m_data_root;
    }

    void Profile::set_data_root(std::string value) {
        m_data_root = std::move(value);
    }

    const std::string& Profile::map_root() const {
        return m_map_root;
    }

    void Profile::set_map_root(std::string value) {
        m_map_root = std::move(value);
    }

    const std::string& Profile::maps_subdir() const {
        return m_maps_subdir;
    }

    void Profile::set_maps_subdir(std::string value) {
        m_maps_subdir = std::move(value);
    }

    const std::string& Profile::server_map_root() const {
        return m_server_map_root;
    }

    void Profile::set_server_map_root(std::string value) {
        m_server_map_root = std::move(value);
    }

    const std::string& Profile::configuration_root() const {
        return m_configuration_root;
    }

    void Profile::set_configuration_root(std::string value) {
        m_configuration_root = std::move(value);
    }

    int Profile::origin_patch() const {
        return m_origin_patch;
    }

    void Profile::set_origin_patch(int value) {
        m_origin_patch = value;
    }

    const std::string& Profile::spawn_name() const {
        return m_spawn_name;
    }

    void Profile::set_spawn_name(std::string value) {
        m_spawn_name = std::move(value);
    }

    int Profile::spawn_server_x() const {
        return m_spawn_server_x;
    }

    void Profile::set_spawn_server_x(int value) {
        m_spawn_server_x = value;
    }

    int Profile::spawn_server_y() const {
        return m_spawn_server_y;
    }

    void Profile::set_spawn_server_y(int value) {
        m_spawn_server_y = value;
    }

    int Profile::spawn_server_z() const {
        return m_spawn_server_z;
    }

    void Profile::set_spawn_server_z(int value) {
        m_spawn_server_z = value;
    }

    bool Profile::network_enabled() const {
        return m_network_enabled;
    }

    void Profile::set_network_enabled(bool value) {
        m_network_enabled = value;
    }

    const std::string& Profile::network_host() const {
        return m_network_host;
    }

    void Profile::set_network_host(std::string value) {
        m_network_host = std::move(value);
    }

    std::uint16_t Profile::network_port() const {
        return m_network_port;
    }

    void Profile::set_network_port(std::uint16_t value) {
        m_network_port = value;
    }

    const std::string& Profile::play_mode() const {
        return m_play_mode;
    }

    void Profile::set_play_mode(std::string value) {
        m_play_mode = std::move(value);
    }

    const std::string& Profile::store_backend() const {
        return m_store_backend;
    }

    void Profile::set_store_backend(std::string value) {
        m_store_backend = std::move(value);
    }

    const std::string& Profile::server_unix_path() const {
        return m_server_unix_path;
    }

    void Profile::set_server_unix_path(std::string value) {
        m_server_unix_path = std::move(value);
    }

    std::string Profile::maps_dir() const {
        return join_path(m_data_root, m_maps_subdir);
    }

    std::string Profile::server_map_dir() const {
        return m_server_map_root;
    }

    void Profile::resolve_asset_roots(Profile& profile, const std::string& base_dir) {
        const auto resolve = [&](std::string& path) {
            if (!path.empty() && path.front() != '/') {
                path = join_path(base_dir, path);
            }
        };
        resolve(profile.m_data_root);
        resolve(profile.m_map_root);
        resolve(profile.m_server_map_root);
    }

    std::optional<Profile> Profile::load(const std::string& path) {
        const auto bytes = File::read(path);
        if (!bytes) {
            return std::nullopt;
        }
        std::istringstream input(std::string(bytes->begin(), bytes->end()));

        Profile profile;
        std::string line;
        while (std::getline(input, line)) {
            line = trim(line);
            if (line.empty() || line.front() == '#' || line.front() == ';') {
                continue;
            }
            const auto equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }
            const std::string key = trim(line.substr(0, equals));
            const std::string value = trim(line.substr(equals + 1));
            if (key == "name") {
                profile.set_name(value);
            } else if (key == "data_root") {
                profile.set_data_root(value);
            } else if (key == "map_root") {
                profile.set_map_root(value);
            } else if (key == "maps_subdir") {
                profile.set_maps_subdir(value);
            } else if (key == "server_map_root") {
                profile.set_server_map_root(value);
            } else if (key == "configuration_root") {
                profile.set_configuration_root(value);
            } else if (key == "origin_patch") {
                profile.set_origin_patch(std::atoi(value.c_str()));
            } else if (key == "spawn_name") {
                profile.set_spawn_name(value);
            } else if (key == "spawn_server_x") {
                profile.set_spawn_server_x(std::atoi(value.c_str()));
            } else if (key == "spawn_server_y") {
                profile.set_spawn_server_y(std::atoi(value.c_str()));
            } else if (key == "spawn_server_z") {
                profile.set_spawn_server_z(std::atoi(value.c_str()));
            } else if (key == "network_enabled") {
                profile.set_network_enabled(parse_bool(value));
            } else if (key == "network_host") {
                profile.set_network_host(value);
            } else if (key == "network_port") {
                profile.set_network_port(static_cast<std::uint16_t>(std::atoi(value.c_str())));
            } else if (key == "play_mode") {
                profile.set_play_mode(value);
            } else if (key == "store_backend") {
                profile.set_store_backend(value);
            } else if (key == "server_unix_path") {
                profile.set_server_unix_path(value);
            }
        }
        return profile;
    }

    std::string Profile::default_path(const std::string& repo_root) {
        if (const char* environment = std::getenv("OPENKAL_PROFILE")) {
            if (environment[0] != '\0') {
                return std::string(environment);
            }
        }
        return join_path(join_path(repo_root, "config/profiles"), "default.profile");
    }

}  // namespace openkal::platform
