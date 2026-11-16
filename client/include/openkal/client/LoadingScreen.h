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

#include <string>
#include <string_view>

namespace openkal::client {
    class FreetypeFont;
    class WidgetTools;
}  // namespace openkal::client

namespace openkal::engine {
    class IEngine;
}

namespace openkal::client {

    /** Keeps the render window responsive while an enter-world stage is loading. */
    class LoadingScreen {
      public:
        /** Creates a loading screen backed by an existing UI and status font.
         * @param engine Rendering backend.
         * @param ui UI texture helper.
         * @param status_font TrueType font for the stage status.
         * @param background_name Name of the loading-background texture.
         */
        LoadingScreen(engine::IEngine& engine,
                      WidgetTools& ui,
                      const FreetypeFont& status_font,
                      std::string background_name);

        /** Polls input and displays the next loading frame.
         * @param status Stage description shown when progress is unavailable.
         * @param progress Completed fraction in [0, 1], or -1 for a status-only screen.
         * @return False when the window was closed.
         */
        [[nodiscard]] bool present(std::string_view status, float progress = -1.0f);

      private:
        engine::IEngine& m_engine;
        WidgetTools& m_ui;
        const FreetypeFont& m_status_font;
        std::string m_background_name;
    };

}  // namespace openkal::client
