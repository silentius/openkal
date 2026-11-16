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
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <string_view>

namespace openkal::client {

    /** Centered progress display for a cancellable quest-job cycle. */
    class FishingBar final {
      public:
        /** Draws quest-job progress and the Escape cancellation hint.
         * @param ui Widget helper for progress artwork.
         * @param engine Engine used for TrueType text rendering.
         * @param font Font used for the title and hint.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param ui_scale Active HUD scale.
         * @param elapsed_ms Milliseconds since the server began this cycle.
         * @param duration_ms Configured cycle duration in milliseconds.
         * @param title Progress title.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         float screen_w,
                         float screen_h,
                         float ui_scale,
                         float elapsed_ms,
                         float duration_ms,
                         std::string_view title = "Fishing");
    };

}  // namespace openkal::client
