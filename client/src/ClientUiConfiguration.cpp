/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientUiConfiguration.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

namespace openkal::client {

    void ClientUiConfiguration::initialize(const std::string& save_dir,
                                           const std::string& configuration_root) {
        m_settings_path = platform::join_path(save_dir, "ui_settings.json");
        if (!m_settings.load(m_settings_path)) {
            if (!platform::ensure_directory(save_dir)) {
                log::warn("could not create save directory: " + save_dir);
            } else if (!m_settings.save(m_settings_path)) {
                log::warn("could not write default UI settings: " + m_settings_path);
            }
        }
        m_layouts.set_directory(platform::join_path(configuration_root, "ui"));
        m_layouts.preload_defaults();
    }

    const std::string& ClientUiConfiguration::settings_path() const {
        return m_settings_path;
    }

    UiSettings& ClientUiConfiguration::settings() {
        return m_settings;
    }

    UiLayoutStore& ClientUiConfiguration::layouts() {
        return m_layouts;
    }

}  // namespace openkal::client
