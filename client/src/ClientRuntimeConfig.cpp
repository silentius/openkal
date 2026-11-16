/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientRuntimeConfig.h"

#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <cstdlib>
#include <filesystem>
#include <utility>

namespace openkal::client {
    namespace {

        std::string save_directory(const std::string& executable_dir) {
            if (const char* environment = std::getenv("OPENKAL_SAVE_DIR")) {
                if (environment[0] != '\0') {
                    return environment;
                }
            }
            return platform::join_path(executable_dir, "save");
        }

        std::string area_map_directory(const platform::Profile& profile,
                                       const std::string& resource_root) {
            if (profile.play_mode() == "single") {
                const std::string packaged =
                    platform::join_path(resource_root, "RuntimeData/areas");
                if (platform::is_directory(packaged)) {
                    return packaged;
                }
            }
            return profile.server_map_dir();
        }

    }  // namespace

    std::optional<ClientRuntimeConfig> ClientRuntimeConfig::load(int argc, char** argv) {
        const std::string repo_root = OPENKAL_REPO_ROOT;
        std::string profile_path = platform::Profile::default_path(repo_root);
        if (argc > 1) {
            profile_path = argv[1];
        }
        std::string runtime_error;
        auto runtime_profile =
            platform::RuntimeProfile::load(repo_root, profile_path, &runtime_error);
        if (!runtime_profile) {
            log::error(runtime_error);
            return std::nullopt;
        }
        std::string document_error;
        auto profile_document = platform::ProfileDocument::load(profile_path, &document_error);
        if (!profile_document) {
            log::error("failed to open editable profile: " + document_error);
            return std::nullopt;
        }
        if (!runtime_profile->requested_configuration_root.empty() &&
            runtime_profile->configuration.archive_path) {
            log::warn("configuration_root is unavailable, using config.pak: " +
                      runtime_profile->requested_configuration_root);
        }
        log::info("configuration source: " +
                  runtime_profile->configuration.archive_path.value_or(
                      runtime_profile->configuration.configuration_root));
        auto loaded_settings = settings::load_client_settings(*profile_document);
        const std::string executable_dir = platform::executable_directory();
        if (platform::ConfigArchive::file_exists(profile_path)) {
            const std::string settings_path =
                platform::join_path(save_directory(executable_dir), "client_settings.profile");
            if (platform::File::exists(settings_path)) {
                profile_document = platform::ProfileDocument::load(settings_path, &document_error);
                if (!profile_document) {
                    log::error("failed to open client settings: " + document_error);
                    return std::nullopt;
                }
                loaded_settings = settings::load_client_settings(*profile_document);
            } else {
                profile_document = platform::ProfileDocument::create(settings_path);
                settings::store_client_settings(*profile_document, loaded_settings.settings);
            }
        }
        for (const auto& warning : loaded_settings.warnings) {
            log::warn("profile setting: " + warning);
        }

        auto& profile = runtime_profile->profile;
        if (profile.play_mode() == "multi") {
            log::info("play_mode=multi ? connect to packaged multiplayer server");
        } else if (profile.play_mode() == "offline") {
            log::info("play_mode=offline ? no MainSvr");
        } else {
            log::info("play_mode=single ? embed MainSvr (unix + sqlite)");
        }
        log::info("openkal ? profile=" + profile.name());
        log::info("exe_dir: " + executable_dir);
        return ClientRuntimeConfig(std::move(profile),
                                   std::move(*profile_document),
                                   loaded_settings.settings,
                                   repo_root,
                                   executable_dir);
    }

    ClientRuntimeConfig::ClientRuntimeConfig(platform::Profile profile,
                                             platform::ProfileDocument profile_document,
                                             settings::ClientSettings client_settings,
                                             std::string repo_root,
                                             std::string executable_dir)
        : m_profile(std::move(profile)), m_profile_document(std::move(profile_document)),
          m_client_settings(std::move(client_settings)), m_profile_path(m_profile_document.path()),
          m_repo_root(std::move(repo_root)), m_executable_dir(std::move(executable_dir)),
          m_save_dir(save_directory(m_executable_dir)), m_maps_dir(m_profile.maps_dir()),
          m_server_map_dir(area_map_directory(m_profile, m_repo_root)),
          m_configuration_root(m_profile.configuration_root()),
          m_texture_dir(platform::join_path(m_maps_dir, "Tex")) {
        log::info("save: " + m_save_dir);
        log::info("data: " + m_profile.data_root());
        log::info("map: " + m_profile.map_root());
        log::info("maps (visual): " + m_maps_dir);
        log::info("area maps: " + m_server_map_dir);
        log::info("configuration_root: " + m_configuration_root);
    }

    const platform::Profile& ClientRuntimeConfig::profile() const {
        return m_profile;
    }

    platform::ProfileDocument& ClientRuntimeConfig::profile_document() {
        return m_profile_document;
    }

    const std::string& ClientRuntimeConfig::profile_path() const {
        return m_profile_path;
    }

    const settings::ClientSettings& ClientRuntimeConfig::client_settings() const {
        return m_client_settings;
    }

    void ClientRuntimeConfig::set_client_settings(settings::ClientSettings value) {
        m_client_settings = std::move(value);
    }

    bool ClientRuntimeConfig::save_client_settings(settings::ClientSettings value,
                                                   std::string& error) {
        error.clear();
        (void) settings::validate_client_settings(value);
        auto pending = m_profile_document;
        settings::store_client_settings(pending, value);
        const auto parent = std::filesystem::path(m_profile_path).parent_path().string();
        if (!parent.empty() && !platform::ensure_directory(parent)) {
            error = "failed to create settings directory: " + parent;
            return false;
        }
        if (pending.save(&error) != platform::ProfileSaveStatus::Saved) {
            return false;
        }
        m_profile_document = std::move(pending);
        m_client_settings = std::move(value);
        return true;
    }

    const std::string& ClientRuntimeConfig::repo_root() const {
        return m_repo_root;
    }

    const std::string& ClientRuntimeConfig::executable_dir() const {
        return m_executable_dir;
    }

    const std::string& ClientRuntimeConfig::save_dir() const {
        return m_save_dir;
    }

    const std::string& ClientRuntimeConfig::maps_dir() const {
        return m_maps_dir;
    }

    const std::string& ClientRuntimeConfig::server_map_dir() const {
        return m_server_map_dir;
    }

    const std::string& ClientRuntimeConfig::configuration_root() const {
        return m_configuration_root;
    }

    const std::string& ClientRuntimeConfig::texture_dir() const {
        return m_texture_dir;
    }

}  // namespace openkal::client
