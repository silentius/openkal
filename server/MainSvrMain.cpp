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
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/Profile.h"
#include "openkal/server/ServerHost.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>

#ifndef OPENKAL_DEVELOPMENT_ROOT
#define OPENKAL_DEVELOPMENT_ROOT ""
#endif

namespace {
    std::atomic<bool> g_stop{false};
    void on_signal(int) {
        g_stop = true;
    }
}  // namespace

int main(int argc, char** argv) {
    using namespace openkal;
    log::Log::set_timestamps(true);
    const std::string repo_root = OPENKAL_REPO_ROOT;
    std::string profile_path = platform::Profile::default_path(repo_root);
    std::string multiplayer_config;
    bool unix_mode = false, explicit_profile = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--unix") {
            unix_mode = true;
        } else if (argument == "--multiplayer-config") {
            if (++i >= argc) {
                log::error("Missing multiplayer configuration path");
                return 1;
            }
            multiplayer_config = argv[i];
        } else if (!argument.empty() && argument.front() != '-' && !explicit_profile) {
            profile_path = argument;
            explicit_profile = true;
        } else {
            log::error(
                "Usage: openkal_mainsvr [profile] [--multiplayer-config server.json] [--unix]");
            return 1;
        }
    }
    if (unix_mode && !multiplayer_config.empty()) {
        log::error("--unix and --multiplayer-config cannot be combined");
        return 1;
    }
    auto profile_opt = platform::Profile::load(profile_path);
    if (!profile_opt) {
        std::string archive_error;
        if (!platform::ConfigArchive::select(repo_root, {}, &archive_error)) {
            log::error("failed to load profile and config.pak: " + archive_error);
            return 1;
        }
        profile_opt = platform::Profile::load(profile_path);
    }
    if (!profile_opt) {
        log::error("failed to load profile: " + profile_path);
        return 1;
    }
    platform::Profile profile = *profile_opt;
    const std::string configured_root = profile.configuration_root();
    std::string source_error;
    auto configuration_source =
        platform::ConfigArchive::select(repo_root, configured_root, &source_error);
    if (!configuration_source) {
        log::error("failed to load configuration: " + source_error);
        return 1;
    }
    if (!configured_root.empty() && configuration_source->archive_path) {
        log::warn("configuration_root is unavailable, using config.pak: " + configured_root);
    }
    profile.set_configuration_root(configuration_source->configuration_root);
    const std::string& configuration_root = profile.configuration_root();
    log::info("configuration source: " +
              configuration_source->archive_path.value_or(configuration_root));
    const std::string exe_dir = platform::executable_directory();
    platform::Profile::resolve_asset_roots(profile, exe_dir);

    server::ServerConfig cfg;
    cfg.maps_dir = profile.server_map_dir();
    cfg.indoor_geometry_root = profile.map_root();
    cfg.save_dir = platform::join_path(exe_dir, "save");
    cfg.monsters_dir = platform::join_path(configuration_root, "monsters");
    cfg.monster_strength_path =
        platform::join_path(configuration_root, "server/monster_strength.json");
    if (!platform::file_exists(cfg.monster_strength_path)) {
        cfg.monster_strength_path.clear();
    }
    cfg.items_dir = platform::join_path(configuration_root, "items");
    cfg.pets_dir = platform::join_path(configuration_root, "pets");
    cfg.riding_dir = platform::join_path(configuration_root, "riding");
    cfg.riding_cache_path = platform::join_path(cfg.riding_dir, "cache.json");
    cfg.riding_progression_path = platform::join_path(cfg.riding_dir, "progression.json");
    cfg.prefixes_dir = platform::join_path(configuration_root, "prefixes");
    cfg.groups_dir = platform::join_path(configuration_root, "groups");
    cfg.itemgroups_dir = platform::join_path(configuration_root, "itemgroups");
    cfg.skills_dir = platform::join_path(configuration_root, "skills");
    cfg.quests_dir = platform::join_path(configuration_root, "quests");
    cfg.tasks_dir = platform::join_path(configuration_root, "tasks");
    cfg.npcs_dir = platform::join_path(configuration_root, "npcs");
    cfg.shops_dir = platform::join_path(configuration_root, "server/shops");
    cfg.portals_path = platform::join_path(configuration_root, "portals.json");
    {
        const std::string info = platform::join_path(configuration_root, "mapinfo.json");
        if (platform::file_exists(info)) {
            cfg.mapinfo_path = info;
        }
    }
    cfg.starters_dir = platform::join_path(configuration_root, "starters");
    cfg.mixing_dir = platform::join_path(configuration_root, "mixing");
    cfg.bof_path = platform::join_path(configuration_root, "bof.json");
    cfg.fishing_path = platform::join_path(configuration_root, "fishing.json");
    cfg.level_mail_rewards_path =
        platform::join_path(configuration_root, "level_mail_rewards.json");
    cfg.server_events_path = platform::join_path(configuration_root, "server_events.json");
    {
        const std::string info = platform::join_path(configuration_root, "mapinfo.json");
        if (platform::file_exists(info)) {
            cfg.map_catalog_config = info;
        }
    }
    if (!platform::is_directory(cfg.monsters_dir)) {
        cfg.monsters_dir.clear();
    }
    if (!platform::is_directory(cfg.items_dir)) {
        cfg.items_dir.clear();
    }
    if (!platform::is_directory(cfg.pets_dir)) {
        cfg.pets_dir.clear();
    }
    if (!platform::is_directory(cfg.riding_dir)) {
        cfg.riding_dir.clear();
    }
    if (!platform::file_exists(cfg.riding_cache_path)) {
        cfg.riding_cache_path.clear();
    }
    if (!platform::file_exists(cfg.riding_progression_path)) {
        cfg.riding_progression_path.clear();
    }
    if (!platform::is_directory(cfg.groups_dir)) {
        cfg.groups_dir.clear();
    }
    if (!platform::is_directory(cfg.itemgroups_dir)) {
        cfg.itemgroups_dir.clear();
    }
    if (!platform::is_directory(cfg.skills_dir)) {
        cfg.skills_dir.clear();
    }
    if (!platform::is_directory(cfg.quests_dir)) {
        cfg.quests_dir.clear();
    }
    if (!platform::is_directory(cfg.tasks_dir)) {
        cfg.tasks_dir.clear();
    }
    if (!platform::is_directory(cfg.npcs_dir)) {
        cfg.npcs_dir.clear();
    }
    if (!platform::is_directory(cfg.shops_dir)) {
        cfg.shops_dir.clear();
    }
    if (!platform::file_exists(cfg.fishing_path)) {
        cfg.fishing_path.clear();
    }
    if (!platform::file_exists(cfg.server_events_path)) {
        cfg.server_events_path.clear();
    }
    if (!platform::file_exists(cfg.portals_path)) {
        cfg.portals_path.clear();
    }
    if (!platform::is_directory(cfg.starters_dir)) {
        cfg.starters_dir.clear();
    }
    cfg.store = server::store_backend_from_string(profile.store_backend());
    cfg.transport = server::TransportKind::Tcp;
    cfg.tcp_host = profile.network_host().empty() ? "0.0.0.0" : profile.network_host();
    cfg.tcp_port = profile.network_port();
    cfg.spawn_x = profile.spawn_server_x();
    cfg.spawn_y = profile.spawn_server_y();
    cfg.spawn_z = profile.spawn_server_z();
    cfg.map_radius_cells = 2;
    cfg.spawn_load_radius_px = 16384;
    cfg.max_monsters = 256;
    const auto plants =
        server::load_harvest_config(platform::join_path(configuration_root, "server/plants.json"));
    if (plants) {
        cfg.plants = *plants;
    }

    if (unix_mode) {
        cfg.transport = server::TransportKind::Unix;
        cfg.unix_path = platform::join_path(cfg.save_dir, "openkal.sock");
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!unix_mode) {
#ifndef OPENKAL_WITH_POSTGRES
        log::error("Build openkal_mainsvr with OPENKAL_WITH_POSTGRES=ON for multiplayer.");
        return 1;
#else
        server::ServerStartupOptions options;
        options.executable_dir = exe_dir;
        options.configuration_path = multiplayer_config;
        if (cfg.tcp_host == "127.0.0.1") {
            options.development_root = OPENKAL_DEVELOPMENT_ROOT;
        }
        if (const char* dsn = std::getenv("OPENKAL_DATABASE_DSN")) {
            options.database_dsn = dsn;
        }
        options.tls = server::tls_config_from_environment();
        std::string error;
        const auto startup = server::load_server_startup(options, error);
        if (!startup) {
            log::error(error);
            return 1;
        }
        cfg.multiplayer = true;
        cfg.store = server::StoreBackend::Postgres;
        cfg.database_dsn = startup->database_dsn;
        cfg.guild_rules_path = startup->guild_rules_path;
        cfg.castle_war_path = startup->castle_war_path;
        cfg.user_roles_path = startup->user_roles_path;
        cfg.management_socket = startup->management_socket;
        cfg.management_token = startup->management_token;
        cfg.allow_client_registration = startup->allow_client_registration;
        if (const char* token = std::getenv("OPENKAL_TEST_CONTROL_TOKEN")) {
            cfg.test_control_token = token;
        }
        if (startup->plants) {
            cfg.plants = *startup->plants;
        }
        cfg.tls = startup->tls;
        log::info("server settings: " + startup->configuration_path);
#endif
    }

    if (!platform::is_directory(cfg.maps_dir)) {
        const std::string packaged_maps = platform::join_path(repo_root, "RuntimeData/areas");
        if (platform::is_directory(packaged_maps)) {
            cfg.maps_dir = packaged_maps;
        }
    }
    server::ServerHost host;
    if (!host.start(cfg)) {
        log::error("openkal_mainsvr failed to start");
        return 1;
    }
    log::info(std::string("openkal_mainsvr running (") +
              (cfg.transport == server::TransportKind::Unix ? "unix" : "tcp") +
              ") endpoint=" + host.listen_endpoint());

    while (!g_stop.load() && host.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    host.stop();
    return 0;
}
