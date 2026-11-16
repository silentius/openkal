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

    /** Compatibility behavior: centered charge bar for pile-type-B skills (Focus Shot). */
    class PileSkillBar {
      public:
        /**
         * Draws the charge bar, pip count, and skill name.
         * @param ui Widget helper for MP-style progress art.
         * @param engine Engine used to submit UI quads and TTF.
         * @param font TrueType font for the name and pip count.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param elapsed_ms Elapsed charge time in milliseconds.
         * @param max_ms Bar fill duration (`SetPileSkillTime`, 3300 ms non-KR).
         * @param skill_name Jobsystem skill name drawn above the bar.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         float screen_w,
                         float screen_h,
                         float elapsed_ms,
                         float max_ms,
                         std::string_view skill_name);
    };

}  // namespace openkal::client
