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
#include "openkal/client/SoftCursor.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/client/WorldRenderCache.h"

#include <string>
#include <unordered_map>

namespace openkal::client {

    class ClientShutdownSignal;
    class ClientRuntimeConfig;
    class UiSettings;

    /** Login entrance, shown before character and world initialization. */
    class StartupScreen {
      public:
        /** Creates a screen whose GPU resources belong to the supplied engine.
         * @param engine Active render/input engine, which must outlive the screen.
         */
        explicit StartupScreen(engine::IEngine& engine);

        /** Releases scene meshes and textures while the engine is still alive. */
        ~StartupScreen();
        StartupScreen(const StartupScreen&) = delete;
        StartupScreen& operator=(const StartupScreen&) = delete;

        /** Shows the entrance and plays the door approach on either play mode or Enter.
         * Escape/close interrupts both the idle view and the transition.
         * @param map_root Client map root containing the login model.
         * @param runtime Active profile, asset roots, and editable client settings.
         * @param ui_settings Shared automatic or manual UI scale settings.
         * @param shutdown_signal Process termination flag observed by the frame loop.
         * @param wait_for_input False for automated profiling; still presents one frame.
         * @param multiplayer Receives the selected online mode when supplied.
         * @return True to continue startup; false on quit or missing background.
         */
        [[nodiscard]] bool run(const std::string& map_root,
                               ClientRuntimeConfig& runtime,
                               const UiSettings& ui_settings,
                               const ClientShutdownSignal& shutdown_signal,
                               bool wait_for_input = true,
                               bool* multiplayer = nullptr);

      private:
        engine::IEngine& m_engine;
        WidgetTools m_ui;
        FreetypeFont m_font;
        SoftCursor m_cursor;
        std::unordered_map<std::string, WorldRenderCache::GpuModel> m_models;
        std::unordered_map<std::string, engine::TextureHandle> m_textures;
    };

}  // namespace openkal::client
