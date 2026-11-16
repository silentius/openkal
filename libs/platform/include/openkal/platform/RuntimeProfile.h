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

#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/Profile.h"

#include <optional>
#include <string>
#include <string_view>

namespace openkal::platform {

    /** Checks whether a positional command-line argument names a profile file.
     * @param path Candidate path.
     * @return True when the path has the .profile extension.
     */
    [[nodiscard]] bool is_profile_path(std::string_view path);

    /** Holds a loaded profile with runtime paths and configuration source resolved. */
    struct RuntimeProfile {
        /** Parsed profile with absolute asset roots and the selected configuration root. */
        Profile profile;

        /** Profile file path used for loading. */
        std::string profile_path;

        /** Configuration root requested by the profile before fallback selection. */
        std::string requested_configuration_root;

        /** Loose configuration directory or mounted config.pak source. */
        ConfigurationSource configuration;

        /** Loads the same runtime profile and configuration source used by the client.
         * @param resource_root Directory containing config, config.pak, and profiles.
         * @param profile_override Explicit profile path, or empty to use Profile::default_path.
         * @param error Optional destination for a human-readable failure reason.
         * @return Resolved runtime profile, or no value on failure.
         */
        [[nodiscard]] static std::optional<RuntimeProfile>
        load(const std::string& resource_root,
             const std::string& profile_override = {},
             std::string* error = nullptr);

        /** Loads a runtime profile for an executable stored in a bin/tools directory.
         * Relative asset roots are resolved against the parent bin directory, matching the client.
         * @param resource_root Directory containing config, config.pak, and profiles.
         * @param profile_override Explicit profile path, or empty to use Profile::default_path.
         * @param error Optional destination for a human-readable failure reason.
         * @param configuration_root_override Loose configuration root to use instead of the
         * profile value, or empty to use the profile value.
         * @return Resolved runtime profile, or no value on failure.
         */
        [[nodiscard]] static std::optional<RuntimeProfile>
        load_for_tool(const std::string& resource_root,
                      const std::string& profile_override = {},
                      std::string* error = nullptr,
                      const std::string& configuration_root_override = {});
    };

}  // namespace openkal::platform
