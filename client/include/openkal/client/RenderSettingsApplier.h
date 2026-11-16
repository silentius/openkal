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

#include "openkal/settings/ClientSettings.h"

namespace openkal::engine {
    class IEngine;
}

namespace openkal::client {

    /** Rendering features available to the current client and engine pair. */
    struct RenderSettingsCapabilities {
        bool grass = true;
        bool bloom = false;
        bool shadows = true;
    };

    /** Applies renderer-owned settings and reports live feature support. */
    class RenderSettingsApplier final {
      public:
        /** Creates an applier for an initialized engine.
         * @param engine Active engine.
         */
        explicit RenderSettingsApplier(engine::IEngine& engine);

        /** @return Rendering features supported by the active backend. */
        [[nodiscard]] RenderSettingsCapabilities capabilities() const;

        /** Applies renderer-owned values immediately.
         * @param settings Validated rendering settings.
         */
        void apply(const settings::RenderSettings& settings);

      private:
        engine::IEngine& m_engine;
    };

}  // namespace openkal::client
