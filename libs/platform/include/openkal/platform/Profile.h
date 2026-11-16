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

#include <cstdint>
#include <optional>
#include <string>

namespace openkal::platform {

    /** Stores shared client and server runtime configuration. */
    class Profile final {
      public:
        /** Gets the profile name.
         * @return Profile name.
         */
        [[nodiscard]] const std::string& name() const;

        /** Sets the profile name.
         * @param value Profile name.
         */
        void set_name(std::string value);

        /** Gets the client data root.
         * @return Client data root.
         */
        [[nodiscard]] const std::string& data_root() const;

        /** Sets the client data root.
         * @param value Client data root.
         */
        void set_data_root(std::string value);

        /** Gets the auxiliary map root.
         * @return Auxiliary map root.
         */
        [[nodiscard]] const std::string& map_root() const;

        /** Sets the auxiliary map root.
         * @param value Auxiliary map root.
         */
        void set_map_root(std::string value);

        /** Gets the maps subdirectory.
         * @return Maps subdirectory.
         */
        [[nodiscard]] const std::string& maps_subdir() const;

        /** Sets the maps subdirectory.
         * @param value Maps subdirectory.
         */
        void set_maps_subdir(std::string value);

        /** Gets the authoritative server map root.
         * @return Server map root.
         */
        [[nodiscard]] const std::string& server_map_root() const;

        /** Sets the authoritative server map root.
         * @param value Server map root.
         */
        void set_server_map_root(std::string value);

        /** Gets the preferred loose configuration root.
         * @return Configuration root, or an empty string to use config.pak.
         */
        [[nodiscard]] const std::string& configuration_root() const;

        /** Sets the preferred loose configuration root.
         * @param value Configuration root, or an empty string to use config.pak.
         */
        void set_configuration_root(std::string value);

        /** Gets the coordinate origin patch.
         * @return Coordinate origin patch.
         */
        [[nodiscard]] int origin_patch() const;

        /** Sets the coordinate origin patch.
         * @param value Coordinate origin patch.
         */
        void set_origin_patch(int value);

        /** Gets the default spawn name.
         * @return Default spawn name.
         */
        [[nodiscard]] const std::string& spawn_name() const;

        /** Sets the default spawn name.
         * @param value Default spawn name.
         */
        void set_spawn_name(std::string value);

        /** Gets the server spawn X coordinate.
         * @return Server spawn X coordinate.
         */
        [[nodiscard]] int spawn_server_x() const;

        /** Sets the server spawn X coordinate.
         * @param value Server spawn X coordinate.
         */
        void set_spawn_server_x(int value);

        /** Gets the server spawn Y coordinate.
         * @return Server spawn Y coordinate.
         */
        [[nodiscard]] int spawn_server_y() const;

        /** Sets the server spawn Y coordinate.
         * @param value Server spawn Y coordinate.
         */
        void set_spawn_server_y(int value);

        /** Gets the server spawn Z coordinate.
         * @return Server spawn Z coordinate.
         */
        [[nodiscard]] int spawn_server_z() const;

        /** Sets the server spawn Z coordinate.
         * @param value Server spawn Z coordinate.
         */
        void set_spawn_server_z(int value);

        /** Gets whether network access is enabled.
         * @return True when network access is enabled.
         */
        [[nodiscard]] bool network_enabled() const;

        /** Sets whether network access is enabled.
         * @param value Network enabled state.
         */
        void set_network_enabled(bool value);

        /** Gets the network host.
         * @return Network host.
         */
        [[nodiscard]] const std::string& network_host() const;

        /** Sets the network host.
         * @param value Network host.
         */
        void set_network_host(std::string value);

        /** Gets the network port.
         * @return Network port.
         */
        [[nodiscard]] std::uint16_t network_port() const;

        /** Sets the network port.
         * @param value Network port.
         */
        void set_network_port(std::uint16_t value);

        /** Gets the play mode.
         * @return Play mode.
         */
        [[nodiscard]] const std::string& play_mode() const;

        /** Sets the play mode.
         * @param value Play mode.
         */
        void set_play_mode(std::string value);

        /** Gets the persistence backend.
         * @return Persistence backend.
         */
        [[nodiscard]] const std::string& store_backend() const;

        /** Sets the persistence backend.
         * @param value Persistence backend.
         */
        void set_store_backend(std::string value);

        /** Gets the server Unix socket path.
         * @return Server Unix socket path.
         */
        [[nodiscard]] const std::string& server_unix_path() const;

        /** Sets the server Unix socket path.
         * @param value Server Unix socket path.
         */
        void set_server_unix_path(std::string value);

        /** Gets the visual map directory.
         * @return Joined data root and maps subdirectory.
         */
        [[nodiscard]] std::string maps_dir() const;

        /** Gets the authoritative server map directory.
         * @return Server map directory.
         */
        [[nodiscard]] std::string server_map_dir() const;

        /** Loads a key-value profile file.
         * @param path Profile file path.
         * @return Loaded profile, or no value when the file cannot be opened.
         */
        [[nodiscard]] static std::optional<Profile> load(const std::string& path);

        /** Resolves relative asset roots against a base directory.
         * @param profile Profile whose roots are updated.
         * @param base_dir Base directory for relative roots.
         */
        static void resolve_asset_roots(Profile& profile, const std::string& base_dir);

        /** Gets the configured default profile path.
         * @param repo_root Repository root used for the fallback path.
         * @return Environment override or repository default profile path.
         */
        [[nodiscard]] static std::string default_path(const std::string& repo_root);

      private:
        /** Profile name. */
        std::string m_name = "default";

        /** Client data root. */
        std::string m_data_root = "data";

        /** Auxiliary map root. */
        std::string m_map_root = "map";

        /** Visual maps subdirectory. */
        std::string m_maps_subdir = "MAPS";

        /** Authoritative server map root. */
        std::string m_server_map_root = "server_map";

        /** Preferred loose configuration root; empty selects config.pak. */
        std::string m_configuration_root;

        /** Coordinate origin patch. */
        int m_origin_patch = 32;

        /** Default spawn name. */
        std::string m_spawn_name = "Narootuh";

        /** Server spawn X coordinate. */
        int m_spawn_server_x = 257285;

        /** Server spawn Y coordinate. */
        int m_spawn_server_y = 258744;

        /** Server spawn Z coordinate. */
        int m_spawn_server_z = 16100;

        /** Network enabled state. */
        bool m_network_enabled = false;

        /** Network host. */
        std::string m_network_host;

        /** Network port. */
        std::uint16_t m_network_port = 30001;

        /** Runtime play mode. */
        std::string m_play_mode = "single";

        /** Persistence backend. */
        std::string m_store_backend = "sqlite";

        /** Server Unix socket path. */
        std::string m_server_unix_path;
    };

    /** Compatibility wrapper for Profile::load.
     * @param path Profile file path.
     * @return Loaded profile, or no value when the file cannot be opened.
     */
    [[nodiscard]] inline std::optional<Profile> load_profile(const std::string& path) {
        return Profile::load(path);
    }

    /** Compatibility wrapper for Profile::resolve_asset_roots.
     * @param profile Profile whose roots are updated.
     * @param base_dir Base directory for relative roots.
     */
    inline void resolve_asset_roots(Profile& profile, const std::string& base_dir) {
        Profile::resolve_asset_roots(profile, base_dir);
    }

    /** Compatibility wrapper for Profile::default_path.
     * @param repo_root Repository root used for the fallback path.
     * @return Environment override or repository default profile path.
     */
    [[nodiscard]] inline std::string default_profile_path(const std::string& repo_root) {
        return Profile::default_path(repo_root);
    }

}  // namespace openkal::platform
