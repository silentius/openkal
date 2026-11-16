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

#include "openkal/client/UiLayout.h"
#include "openkal/client/UiSettings.h"

#include <string>

namespace openkal::client {

    /** Owns persistent UI settings and the reloadable UI-layout catalog. */
    class ClientUiConfiguration {
      public:
        /** Loads settings, writes first-run defaults, and preloads layout definitions.
         * @param save_dir Persistent save directory.
         * @param configuration_root Root containing default UI layouts.
         */
        void initialize(const std::string& save_dir, const std::string& configuration_root);

        /** Returns the persistent settings file path.
         * @return UI settings path.
         */
        [[nodiscard]] const std::string& settings_path() const;

        /** Returns mutable persistent UI settings.
         * @return UI settings.
         */
        [[nodiscard]] UiSettings& settings();

        /** Returns the reloadable layout catalog.
         * @return UI layout store.
         */
        [[nodiscard]] UiLayoutStore& layouts();

      private:
        std::string m_settings_path;
        UiSettings m_settings;
        UiLayoutStore m_layouts;
    };

}  // namespace openkal::client
