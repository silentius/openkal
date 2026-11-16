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

#include "openkal/client/CombatFloatText.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/WidgetTools.h"

namespace openkal::engine {
    class IEngine;
}

namespace openkal::client {

    /** Owns the shared UI helper and FreeType faces used by the client session. */
    class ClientUiResources {
      public:
        /** Initializes UI textures and optional system-font atlases.
         * @param engine Rendering backend.
         * @param data_root Client data root.
         */
        void initialize(engine::IEngine& engine, const std::string& data_root);

        /** Releases UI and font GPU resources.
         * @param engine Rendering backend.
         */
        void shutdown(engine::IEngine& engine);

        /** @return Shared UI texture helper. */
        [[nodiscard]] WidgetTools& ui();

        /** @return Font for nameplates. */
        [[nodiscard]] FreetypeFont& name_font();

        /** @return Font for item tips and loading status. */
        [[nodiscard]] FreetypeFont& tip_font();

        /** @return Font for full-screen banners. */
        [[nodiscard]] FreetypeFont& banner_font();

        /** @return Font for combat float text. */
        [[nodiscard]] FreetypeFont& combat_font();

        /** @return Combat-float collection. */
        [[nodiscard]] CombatFloatText& combat_floats();

      private:
        WidgetTools m_ui;
        FreetypeFont m_name_font;
        FreetypeFont m_tip_font;
        FreetypeFont m_banner_font;
        FreetypeFont m_combat_font;
        CombatFloatText m_combat_floats;
    };

}  // namespace openkal::client
