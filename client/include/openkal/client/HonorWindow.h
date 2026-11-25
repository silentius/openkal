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

#include "openkal/client/UiView.h"
#include "openkal/protocol/Honor.h"

#include <optional>

namespace openkal::client {
    /** Read-only view of the local character's Honor standings and Reward Points. */
    class HonorWindow {
      public:
        /** Loads the layout used by the window.
         * @param layout Current Honor UI definition.
         */
        void set_layout(const UiLayout* layout);

        /** Changes window visibility.
         * @param open True to show the window.
         */
        void set_open(bool open);

        /** @return Whether the window is visible. */
        [[nodiscard]] bool open() const;

        /** Fits and centers the window in the viewport.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         * @param scale Preferred UI scale.
         */
        void arrange(float width, float height, float scale);

        /** Checks for a pointer hit.
         * @param ui Widget drawing and measurement helpers.
         * @param x Pointer X.
         * @param y Pointer Y.
         * @return Hit kind, including a click on the window chrome.
         */
        [[nodiscard]] UiHit hit(WidgetTools& ui, float x, float y) const;

        /** Draws the window at its current position.
         * @param engine Rendering engine.
         * @param ui Widget drawing helpers.
         * @param font Shared TrueType font.
         * @param status Personal snapshot, or no value while unavailable.
         * @param input Pointer state for button feedback.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const std::optional<protocol::HonorStatus>& status,
                  const engine::FrameInput& input) const;

      private:
        UiDocument m_document;
        bool m_open = false;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_scale = 1.0f;
    };
}  // namespace openkal::client
