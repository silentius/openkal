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
#include "openkal/client/ItemDb.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <cstdint>

namespace openkal::client {

    /** Click result for the blacksmith window. */
    enum class BlacksmithClickResult {
        Ignored = 0,
        Consumed,
        Close,
        Revise,
        ExplosiveBlow,
    };

    /** Dragon Explosive Blow / Revise service opened from blacksmith NPCs. */
    class BlacksmithWindow : private UiViewHost {
      public:
        /** @return True while shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Shows or hides the window. @param open True to show. */
        void set_open(bool open) {
            m_open = open;
            m_dragging = false;
            if (!open) {
                m_weapon_iid = 0;
            }
        }

        /** @return UI scale factor (1 = native pixels). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the UI scale factor.
         * @param scale Scale factor (1 = native pixels).
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /**
         * Binds the blacksmith layout document.
         * @param layout Layout; must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** Applies window.w / window.h from the layout, keeping the current position. */
        void sync_from_layout();

        /**
         * Centers on screen.
         * @param screen_w Width.
         * @param screen_h Height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /** @return Left. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Top. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Width. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Height. */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /**
         * Hit-tests the window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Accepts a dragged weapon into the slot.
         * @param stack Bag stack.
         * @param catalog Item catalog.
         * @param mx Drop pointer X.
         * @param my Drop pointer Y.
         * @return True when the weapon was accepted by the framed slot.
         */
        [[nodiscard]] bool
        try_drop_weapon(const ItemStack& stack, const ItemDb& catalog, float mx, float my);

        /** @return Weapon instance id in the slot, or 0. */
        [[nodiscard]] std::uint32_t weapon_iid() const {
            return m_weapon_iid;
        }

        /**
         * Handles a left click.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param bag Live player bag.
         * @param catalog Item catalog.
         * @return Click result.
         */
        [[nodiscard]] BlacksmithClickResult
        handle_click(float mx, float my, const Bag& bag, const ItemDb& catalog);

        /** Applies pointer feedback and title-bar dragging.
         * @param input Frame pointer state.
         */
        void update_pointer(const engine::FrameInput& input);

        /** Loads the bold Dragon caption face.
         * @param engine Engine owning the font atlas.
         */
        void sync_fonts(engine::IEngine& engine);

        /** Releases the caption atlas.
         * @param engine Engine owning the font atlas.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Draws the blacksmith window.
         * @param engine Engine.
         * @param ui Widget helpers.
         * @param font Body font.
         * @param bag Player bag for the slotted weapon.
         * @param catalog Item catalog.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag,
                  const ItemDb& catalog) const;

      private:
        /** Reloads the v1 document from the bound layout. */
        void reload_document() const;

        /** @return Frame style from the v1 chrome. */
        [[nodiscard]] WindowFrameStyle frame_style() const;

        /** Builds current labels, item icon and action availability.
         * @param bag Live player bag.
         * @param catalog Item catalog.
         * @return View bindings derived without changing inventory state.
         */
        [[nodiscard]] UiVarMap variables(const Bag& bag, const ItemDb& catalog) const;

        /** @param action Button action. @return Whether the current bag permits it. */
        [[nodiscard]] bool button_enabled(std::string_view action) const override;

        /** Resolves the selected weapon icon.
         * @param bind Slot binding.
         * @param index Slot index; unused for the single weapon slot.
         * @param out Resolved icon.
         * @return True for the weapon binding.
         */
        [[nodiscard]] bool
        resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override;

        /**
         * Looks up the slotted weapon in the bag.
         * @param bag Player bag.
         * @return Stack pointer, or nullptr.
         */
        [[nodiscard]] const ItemStack* slotted_weapon(const Bag& bag) const;

        /** Visible. */
        bool m_open = false;

        /** Geometry. */
        float m_x = 40.0f;

        /** Geometry. */
        float m_y = 80.0f;

        /** Geometry. */
        float m_w = 460.0f;

        /** Geometry. */
        float m_h = 390.0f;

        /** UI scale factor (1 = native pixels). */
        float m_scale = 1.0f;

        /** Layout document; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Parsed openkal.ui/v1 document. */
        mutable UiDocument m_v1;

        /** Bold FreeType caption face. */
        FreetypeFont m_title_font;

        /** State derived from the latest live view bindings. */
        mutable UiSlotIcon m_weapon_icon;
        mutable bool m_can_revise = false;
        mutable bool m_can_eb = false;

        /** Pointer offset while dragging the title. */
        bool m_dragging = false;
        float m_drag_dx = 0.0f;
        float m_drag_dy = 0.0f;

        /** Weapon instance id, or 0. */
        std::uint32_t m_weapon_iid = 0;
    };

}  // namespace openkal::client
