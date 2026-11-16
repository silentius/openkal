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

#include "openkal/platform/Profile.h"
#include "openkal/platform/ProfileDocument.h"
#include "openkal/settings/ClientSettings.h"

#include <optional>
#include <string>

namespace openkal::client {

    /** Owns the loaded profile and the client paths resolved from it. */
    class ClientRuntimeConfig {
      public:
        /** Loads a profile and resolves its asset roots for the current executable.
         * @param argc Number of command-line arguments.
         * @param argv Command-line arguments.
         * @return Runtime configuration, or no value when the profile cannot be loaded.
         */
        [[nodiscard]] static std::optional<ClientRuntimeConfig> load(int argc, char** argv);

        /** Returns the active profile.
         * @return Loaded client profile.
         */
        [[nodiscard]] const platform::Profile& profile() const;

        /** Returns the unresolved editable profile document.
         * @return Selected profile document.
         */
        [[nodiscard]] platform::ProfileDocument& profile_document();

        /** Returns the editable settings path, including the save-directory profile for packages.
         * @return Settings profile file path.
         */
        [[nodiscard]] const std::string& profile_path() const;

        /** Returns the validated client settings.
         * @return Client settings loaded from the profile.
         */
        [[nodiscard]] const settings::ClientSettings& client_settings() const;

        /** Replaces the active in-memory client settings.
         * @param value Validated client settings.
         */
        void set_client_settings(settings::ClientSettings value);

        /** Saves a validated draft to the active profile and updates runtime settings on success.
         * @param value Settings draft to validate and save.
         * @param error Failure detail, or empty on success.
         * @return True when the profile was saved.
         */
        [[nodiscard]] bool save_client_settings(settings::ClientSettings value, std::string& error);

        /** Returns the repository root compiled into the client.
         * @return Repository root.
         */
        [[nodiscard]] const std::string& repo_root() const;

        /** Returns the executable directory.
         * @return Executable directory.
         */
        [[nodiscard]] const std::string& executable_dir() const;

        /** Returns the persistent save directory.
         * @return Save directory.
         */
        [[nodiscard]] const std::string& save_dir() const;

        /** Returns the visual map directory.
         * @return Map directory.
         */
        [[nodiscard]] const std::string& maps_dir() const;

        /** Returns the server area-map directory.
         * @return Server map directory.
         */
        [[nodiscard]] const std::string& server_map_dir() const;

        /** Returns the selected configuration directory.
         * @return Loose or package-backed configuration root.
         */
        [[nodiscard]] const std::string& configuration_root() const;

        /** Returns the terrain texture directory.
         * @return Terrain texture directory.
         */
        [[nodiscard]] const std::string& texture_dir() const;

      private:
        ClientRuntimeConfig(platform::Profile profile,
                            platform::ProfileDocument profile_document,
                            settings::ClientSettings client_settings,
                            std::string repo_root,
                            std::string executable_dir);

        platform::Profile m_profile;
        platform::ProfileDocument m_profile_document;
        settings::ClientSettings m_client_settings;
        std::string m_profile_path;
        std::string m_repo_root;
        std::string m_executable_dir;
        std::string m_save_dir;
        std::string m_maps_dir;
        std::string m_server_map_dir;
        std::string m_configuration_root;
        std::string m_texture_dir;
    };

}  // namespace openkal::client
