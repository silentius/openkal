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
#include "openkal/client/ItemDb.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/protocol/Riding.h"

namespace openkal::client {

    /** Short non-blocking presentation of a server-confirmed Riding Cache opening. */
    class RidingCacheOverlay final {
      public:
        /** Begins the opened-cache presentation.
         * @param result Authoritative contents sent by the server.
         */
        void show(const protocol::RidingCacheResult& result);

        /** Advances the presentation timer.
         * @param dt Elapsed seconds.
         */
        void tick(float dt);

        /** Draws the opened cache, rewards, and decorative UI sparkles.
         * @param ui Shared texture and window helpers.
         * @param engine UI renderer.
         * @param font FreeType font for all overlay text.
         * @param catalog Item catalog used to resolve names and icons.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void draw(WidgetTools& ui,
                  engine::IEngine& engine,
                  const FreetypeFont& font,
                  const ItemDb& catalog,
                  float screen_w,
                  float screen_h) const;

        /** @return True while the confirmation presentation is visible. */
        [[nodiscard]] bool visible() const {
            return m_visible;
        }

      private:
        bool m_visible = false;
        float m_elapsed = 0.0f;
        protocol::RidingCacheResult m_result;
    };

}  // namespace openkal::client
