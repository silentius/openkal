/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/RuntimeProfile.h"

#include "openkal/platform/Path.h"

#include <cctype>
#include <filesystem>
#include <utility>

namespace openkal::platform {
    namespace {

        std::optional<RuntimeProfile>
        load_runtime_profile(const std::string& resource_root,
                             const std::string& profile_override,
                             const std::string& asset_base_dir,
                             const std::string& configuration_root_override,
                             std::string* error) {
            const std::string profile_path =
                profile_override.empty() ? Profile::default_path(resource_root) : profile_override;

            auto profile = Profile::load(profile_path);
            if (!profile) {
                std::string archive_error;
                if (!ConfigArchive::select(resource_root, {}, &archive_error)) {
                    if (error) {
                        *error = "failed to load profile and config.pak: " + archive_error;
                    }
                    return std::nullopt;
                }
                profile = Profile::load(profile_path);
            }
            if (!profile) {
                if (error) {
                    *error = "failed to load profile: " + profile_path;
                }
                return std::nullopt;
            }

            const std::string requested_configuration_root = configuration_root_override.empty()
                                                                 ? profile->configuration_root()
                                                                 : configuration_root_override;
            std::string configuration_error;
            auto configuration = ConfigArchive::select(
                resource_root, requested_configuration_root, &configuration_error);
            if (!configuration) {
                if (error) {
                    *error = "failed to load configuration: " + configuration_error;
                }
                return std::nullopt;
            }

            profile->set_configuration_root(configuration->configuration_root);
            Profile::resolve_asset_roots(*profile, asset_base_dir);
            if (error) {
                error->clear();
            }
            return RuntimeProfile{std::move(*profile),
                                  profile_path,
                                  requested_configuration_root,
                                  std::move(*configuration)};
        }

    }  // namespace

    bool is_profile_path(std::string_view path) {
        constexpr std::string_view extension = ".profile";
        if (path.size() < extension.size()) {
            return false;
        }
        const std::string_view suffix = path.substr(path.size() - extension.size());
        for (std::size_t index = 0; index < extension.size(); ++index) {
            const auto value = static_cast<unsigned char>(suffix[index]);
            if (static_cast<char>(std::tolower(value)) != extension[index]) {
                return false;
            }
        }
        return true;
    }

    std::optional<RuntimeProfile> RuntimeProfile::load(const std::string& resource_root,
                                                       const std::string& profile_override,
                                                       std::string* error) {
        return load_runtime_profile(
            resource_root, profile_override, executable_directory(), {}, error);
    }

    std::optional<RuntimeProfile>
    RuntimeProfile::load_for_tool(const std::string& resource_root,
                                  const std::string& profile_override,
                                  std::string* error,
                                  const std::string& configuration_root_override) {
        std::filesystem::path asset_base(executable_directory());
        if (asset_base.filename() == "tools") {
            asset_base = asset_base.parent_path();
        }
        return load_runtime_profile(resource_root,
                                    profile_override,
                                    asset_base.lexically_normal().string(),
                                    configuration_root_override,
                                    error);
    }

}  // namespace openkal::platform
