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

#include "openkal/server/HarvestPlants.h"
#include "openkal/server/net/Transport.h"

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::server {

    /** Selects how the game client reaches the server. */
    enum class PlayMode {
        Offline = 0,
        Single = 1,
        Multi = 2,
    };

    /** Selects the server persistence implementation. */
    enum class StoreBackend {
        Mysql = 1,
        Sqlite = 2,
        Postgres = 3,
    };

    /**
     * Parses a profile store_backend value.
     * @param name Backend name (sqlite, postgres, mysql).
     * @return Matching backend; unknown names select Sqlite.
     */
    [[nodiscard]] inline StoreBackend store_backend_from_string(std::string_view name) {
        if (name == "mysql") {
            return StoreBackend::Mysql;
        }
        if (name == "postgres") {
            return StoreBackend::Postgres;
        }
        return StoreBackend::Sqlite;
    }

    /** Selects the server socket transport. */
    enum class TransportKind {
        Unix = 0,
        Tcp = 1,
    };

    /** Runtime configuration shared by embedded and standalone server hosts. */
    struct ServerConfig {
        /** Authoritative gathering population settings. */
        HarvestConfig plants;
        /** Authoritative area-map root. */
        std::string maps_dir;
        /** Runtime dungeon geometry root; missing geometry denies indoor entry. */
        std::string indoor_geometry_root;
        /** Persistence directory (SQLite file plus WAL sidecars). */
        std::string save_dir;
        /** Server-only PostgreSQL connection string; never sent to game clients. */
        std::string database_dsn;
        /** Allows authenticated TCP multiplayer account and character sessions. */
        bool multiplayer = false;
        /** Physical owner guild policy; empty disables guild gameplay. */
        std::string guild_rules_path;
        /** Physical multiplayer Castle War policy; empty disables the event. */
        std::string castle_war_path;
        /** Inclusive Assassin-safe rectangles as [map, min x, min y, max x, max y]. */
        std::vector<std::array<int, 5>> assassin_safe_areas;
        /** Server-owned role command policy; empty denies privileged multiplayer commands. */
        std::string user_roles_path;
        /** Optional absolute owner-only local management socket. */
        std::string management_socket;
        /** Server-only management authentication token; never sent to game clients. */
        std::string management_token;
        /** Allows account creation through the TLS lobby protocol. */
        bool allow_client_registration = false;
        /** Enables privileged test control only when an explicit secret of 16+ bytes is set. */
        std::string test_control_token;
        /** JSON monster configuration directory. */
        std::string monsters_dir;
        /** Optional startup-only monster stat modifiers (config/server/monster_strength.json). */
        std::string monster_strength_path;
        /** map definition JSON configuration path (`config/mapinfo.json`). */
        std::string map_catalog_config;
        /** JSON item configuration directory. */
        std::string items_dir;
        /** Pet catalog JSON directory (config/pets). */
        std::string pets_dir;
        /** Common riding catalog JSON directory (config/riding). */
        std::string riding_dir;
        /** Server-authoritative Riding Cache reward JSON (config/riding/cache.json). */
        std::string riding_cache_path;
        /** Server-authoritative Stablemaster progression JSON. */
        std::string riding_progression_path;
        /** Prefix JSON directory (config/prefixes). */
        std::string prefixes_dir;
        /** Inner loot group JSON directory (config/groups). */
        std::string groups_dir;
        /** Outer itemgroup JSON directory (config/itemgroups). */
        std::string itemgroups_dir;
        /** Persistence implementation. */
        StoreBackend store = StoreBackend::Sqlite;
        /** Socket implementation. */
        TransportKind transport = TransportKind::Unix;
        /** Unix-domain socket path. */
        std::string unix_path;
        /** TCP bind host. */
        std::string tcp_host = "0.0.0.0";
        /** TCP bind port. */
        std::uint16_t tcp_port = 30001;
        /** Explicit TCP TLS settings; absent preserves environment-based transport defaults. */
        std::optional<TlsConfig> tls;
        /** Initial player X coordinate. */
        int spawn_x = 257285;
        /** Initial player Y coordinate. */
        int spawn_y = 258744;
        /** Initial player Z coordinate. */
        int spawn_z = 16100;
        /** Persisted character identifier to load on handshake (0 = config spawn). */
        std::int32_t character_id = 0;
        /** Initial player class index. */
        int player_clazz = 0;
        /** Initial player level. */
        int player_level = 1;
        /** Initial player name. */
        std::string player_name = "Hero";
        /** Number of area cells loaded around spawn. */
        int map_radius_cells = 1;
        /** World and AI update frequency. */
        float tick_hz = 20.0f;
        /** JSON skill configuration directory. */
        std::string skills_dir;
        /** Quest step JSON directory (config/quests). */
        std::string quests_dir;
        /** Quest journal JSON directory containing per-stage level requirements (config/tasks). */
        std::string tasks_dir;
        /** NPC JSON directory (config/npcs). */
        std::string npcs_dir;
        /** Per-NPC shop JSON directory (config/server/shops). */
        std::string shops_dir;
        /** D1–D6, Emok Island, and Valley of Devah portal JSON (config/portals.json). */
        std::string portals_path;
        /** Indoor map definition cells (`config/mapinfo.json`). */
        std::string mapinfo_path;
        /** Per-class starter equipment JSON directory (config/starters). */
        std::string starters_dir;
        /** Mix Master recipe JSON directory (config/mixing). */
        std::string mixing_dir;
        /** Beads of Fire JSON (config/bof.json). */
        std::string bof_path;
        /** Fishing reward and timing JSON (config/fishing.json). */
        std::string fishing_path;
        /** Level-up system mail reward JSON (config/level_mail_rewards.json). */
        std::string level_mail_rewards_path;
        /** Global server event definitions (config/server_events.json). */
        std::string server_events_path;
        /** Spawn loading radius in pixels, or zero for all spawns. */
        int spawn_load_radius_px = 8192;
        /** Resident monster budget per separated player neighborhood (global in singleplayer). */
        int max_monsters = 256;
    };

}  // namespace openkal::server
