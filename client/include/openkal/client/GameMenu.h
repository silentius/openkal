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

#include "openkal/client/UiDocument.h"

namespace openkal::engine {
    class IEngine;
    struct FrameInput;
}  // namespace openkal::engine

namespace openkal::client {

    class FreetypeFont;
    class UiLayout;
    class WidgetTools;

    /** Action selected from the modal in-game menu. */
    enum class GameMenuAction {
        Ignored = 0,
        Consumed,
        Options,
        SelectCharacter,
        QuitClient,
    };

    /** Modal ESC menu and its client-close confirmation view. */
    class GameMenu {
      public:
        /** @return True while the menu is visible. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** @return True while the close confirmation is visible. */
        [[nodiscard]] bool confirming_close() const {
            return m_confirming_close;
        }

        /**
         * Binds the data-driven menu layout.
         * @param layout Layout that outlives this menu.
         */
        void set_layout(const UiLayout* layout);

        /**
         * Sets the display scale and refreshes the layout dimensions.
         * @param scale Display scale where 1 is one JSON layout pixel.
         * @param screen_w Available framebuffer width, or zero to keep the last viewport.
         * @param screen_h Available framebuffer height, or zero to keep the last viewport.
         */
        void set_scale(float scale, float screen_w = 0.0f, float screen_h = 0.0f);

        /**
         * Shows and centers the main menu.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void show(float screen_w, float screen_h);

        /** Hides the menu and clears its confirmation state. */
        void close();

        /** Cancels the confirmation first, or closes the main menu. */
        void handle_escape();

        /**
         * Applies a left click to the active modal view.
         * @param mouse_x Pointer X in framebuffer pixels.
         * @param mouse_y Pointer Y in framebuffer pixels.
         * @param ui UI helper used to resolve native button dimensions.
         * @return Selected menu action.
         */
        [[nodiscard]] GameMenuAction
        handle_click(float mouse_x, float mouse_y, WidgetTools* ui = nullptr);

        /**
         * Tests whether a point is over the framed menu.
         * @param mouse_x Pointer X.
         * @param mouse_y Pointer Y.
         * @return True when the menu is open and contains the point.
         */
        [[nodiscard]] bool contains(float mouse_x, float mouse_y) const;

        /**
         * Draws the modal menu using TrueType text.
         * @param engine Rendering engine.
         * @param ui UI artwork helper.
         * @param font TrueType font used for body and title text.
         * @param input Optional pointer state for button feedback.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const engine::FrameInput* input = nullptr) const;

      private:
        void reload_document();
        void sync_from_layout();
        [[nodiscard]] UiVarMap make_vars() const;

        bool m_open = false;
        bool m_confirming_close = false;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_w = 300.0f;
        float m_h = 252.0f;
        float m_scale = 1.0f;
        float m_requested_scale = 1.0f;
        float m_screen_w = 0.0f;
        float m_screen_h = 0.0f;
        const UiLayout* m_layout = nullptr;
        UiDocument m_document;
    };

}  // namespace openkal::client
