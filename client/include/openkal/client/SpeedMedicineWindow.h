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
#include "openkal/client/Inventory.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/Cooking.h"

#include <string_view>

namespace openkal::client {

    /** Click result for the Speed-up Medicine crafting window. */
    enum class SpeedMedicineClickResult { Ignored = 0, Consumed, Close, Make };

    /** Quantity selector for the exposed Chemist of Speed-up Medicine recipes. */
    class SpeedMedicineWindow final : private UiViewHost {
      public:
        /** @return True while the window is visible. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Shows or hides the window and clears quantities when hidden.
         * @param open True to show the window.
         */
        void set_open(bool open);

        /** @return True while the title bar is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** Sets the UI scale.
         * @param scale UI scale factor.
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** Binds the openkal.ui/v1 layout.
         * @param layout Layout; must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** Applies the bound layout's window geometry. */
        void sync_from_layout();

        /** Centers the window on screen.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /** Tests the window bounds.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the pointer is over the window.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** Applies title-bar dragging.
         * @param input Current frame input.
         */
        void update_pointer(const engine::FrameInput& input);

        /** Handles quantity, make, close, and drag clicks.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param bag Current player inventory.
         * @param fast True to adjust quantities by ten.
         * @return Click result.
         */
        [[nodiscard]] SpeedMedicineClickResult
        handle_click(float mx, float my, const Bag& bag, bool fast);

        /** Takes the order captured by the last Make click.
         * @return Captured order, then clears it.
         */
        [[nodiscard]] protocol::CookingOrder take_order();

        /** Draws the recipe selector using TrueType text.
         * @param engine Rendering engine.
         * @param ui Widget helpers.
         * @param font TrueType body font.
         * @param bag Current player inventory.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag) const;

      private:
        [[nodiscard]] WindowFrameStyle frame_style() const;
        [[nodiscard]] bool can_make(const Bag& bag) const;
        [[nodiscard]] UiVarMap variables(const Bag& bag) const;
        [[nodiscard]] bool button_enabled(std::string_view action) const override;

        bool m_open = false;
        bool m_dragging = false;
        float m_x = 100.0f;
        float m_y = 100.0f;
        float m_w = 540.0f;
        float m_h = 248.0f;
        float m_scale = 1.0f;
        float m_drag_dx = 0.0f;
        float m_drag_dy = 0.0f;
        protocol::CookingOrder m_order;
        protocol::CookingOrder m_pending_order;
        const UiLayout* m_layout = nullptr;
        mutable UiDocument m_v1;
        mutable bool m_make_enabled = false;
    };

}  // namespace openkal::client
