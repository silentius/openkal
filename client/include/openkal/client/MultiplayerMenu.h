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
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/server/LobbyClient.h"

#include <memory>
namespace openkal::client {
    /** Chosen startup path; a remote lobby is present only after successful login. */
    struct MultiplayerChoice {
        bool quit = true;
        /** True when the account UI requests the full startup screen. */
        bool startup = false;
        bool multiplayer = false;
        std::string server_key;
        std::unique_ptr<server::LobbyClient> lobby;
    };
    /** Presents independent single-player and authenticated multiplayer startup paths. */
    class MultiplayerMenu {
      public:
        /** Runs mode selection and online account UI, using optional local auto-login credentials.
         * @param engine Input/render engine.
         * @param ui UI assets.
         * @param font TrueType text.
         * @param settings UI scaling.
         * @param configuration_root UI configuration root.
         * @param message Initial connection status.
         * @param start_online True to open multiplayer directly and try auto_login.json.
         * @return Chosen mode or quit.
         */
        [[nodiscard]] static MultiplayerChoice run(engine::IEngine& engine,
                                                   WidgetTools& ui,
                                                   const FreetypeFont& font,
                                                   const UiSettings& settings,
                                                   const std::string& configuration_root,
                                                   const std::string& message = {},
                                                   bool start_online = false);
    };
}  // namespace openkal::client
