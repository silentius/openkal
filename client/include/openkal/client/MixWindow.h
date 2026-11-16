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
#include "openkal/server/MixCatalog.h"

#include <array>
#include <cstdint>
#include <string>

namespace openkal::client {

    /** Click result for the mixing window. */
    enum class MixClickResult {
        Ignored = 0,
        Consumed,
        Close,
        MakeMix,
    };

    /**
     * Mix Master pentagram: drag items into five slots and start a mix.
     *
     * Center is the weapon to mix. The four outer slots take weapons, armor, and
     * mixing stones. A valid JSON recipe for the NPC mode enables Start Mixing;
     * the click sends C2S_MIXING after the charge overlay finishes.
     */
    class MixWindow {
      public:
        /** Number of pentagram slots (center + four outer). */
        static constexpr int kSlotCount = 5;

        /** @return True while the window is shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /**
         * Shows or hides the window.
         * @param open True to show.
         * @param mode Mixing kind (0 elemental, 1 imperial).
         */
        void set_open(bool open, std::uint8_t mode = 0);

        /** Opens the read-only NPC material list and clears the mixing selection.
         * @param mode Mixing kind (0 upgrade, 1 create weapon).
         */
        void show_materials(std::uint8_t mode);

        /** @return True when the material list is displayed instead of the mixing slots. */
        [[nodiscard]] bool showing_materials() const {
            return m_show_materials;
        }

        /** @return Zero-based material-list page. */
        [[nodiscard]] std::size_t material_page() const {
            return m_material_page;
        }

        /** Flips the open state. */
        void toggle();

        /**
         * Sets the mixing mode (0 elemental, 1 imperial).
         * @param mode Mixing kind.
         */
        void set_mode(std::uint8_t mode);

        /** @return Current mixing mode (0 elemental, 1 imperial). */
        [[nodiscard]] std::uint8_t mode() const {
            return m_mode;
        }

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
         * Moves the window.
         * @param x Client box left in pixels.
         * @param y Client box top in pixels.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
        }

        /** @return Display scale (1 = native JSON units). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the display scale. Call sync_from_layout() afterwards to resize the
         * client box.
         * @param scale Display scale.
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /**
         * Binds Mix Master recipes loaded from config/mixing.
         * @param catalog Catalog; must outlive the window. Null disables Start Mixing.
         */
        void set_mix_catalog(const server::MixCatalog* catalog) {
            m_mix_catalog = catalog;
        }

        /** @return Bound mix catalog, or nullptr. */
        [[nodiscard]] const server::MixCatalog* mix_catalog() const {
            return m_mix_catalog;
        }

        /**
         * Binds the mix_master_window layout document.
         * @param layout Layout; must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** Binds the Dragon material-list layout.
         * @param layout Layout; must outlive the window.
         */
        void set_material_layout(const UiLayout* layout) {
            m_material_layout = layout;
        }

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** @return True while the window itself is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** Applies window.w / window.h from the layout, keeping the current position. */
        void sync_from_layout();

        /**
         * Centers on screen.
         * @param screen_w Width.
         * @param screen_h Height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * Hit-tests the window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Applies one frame of pointer input for title-bar dragging and button hover.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Accepts a dragged inventory item into the slot under the pointer.
         * @param stack Inventory stack under the cursor.
         * @param catalog Item catalog (for item kind and stone checks).
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the item was accepted.
         */
        [[nodiscard]] bool
        try_drop_item(const ItemStack& stack, const ItemDb& catalog, float mx, float my);

        /**
         * Handles a left click (close, slot clear, button press, title drag).
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param catalog Item catalog (for button enable rules).
         * @return Click result.
         */
        [[nodiscard]] MixClickResult handle_click(float mx, float my, const ItemDb& catalog);

        /**
         * Handles left-button release (Start Mixing fires on release over a valid
         * button).
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param catalog Item catalog (for button enable rules).
         * @return Click result.
         */
        [[nodiscard]] MixClickResult handle_release(float mx, float my, const ItemDb& catalog);

        /**
         * Draws the mixing window.
         * @param engine Engine.
         * @param ui Widget helpers.
         * @param font Body font.
         * @param bag Player bag (for current item presence / icons).
         * @param catalog Item catalog.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag,
                  const ItemDb& catalog) const;

        /** @return Selected weapon iid (instance id), or 0 if empty. */
        [[nodiscard]] std::uint32_t weapon_iid() const {
            return m_weapon_iid;
        }

        /** @return Selected extra weapon iid, or 0 if unused. */
        [[nodiscard]] std::uint32_t extra_weapon_iid() const {
            return m_extra_weapon_iid;
        }

        /** @return Selected armor iid (instance id), or 0 if empty. */
        [[nodiscard]] std::uint32_t armor_iid() const {
            return m_armor_iid;
        }

        /** @return Selected first stone iid, or 0 if empty. */
        [[nodiscard]] std::uint32_t stone1_iid() const {
            return m_stone1_iid;
        }

        /** @return Selected second stone iid (making or G3), or 0 if empty. */
        [[nodiscard]] std::uint32_t stone2_iid() const {
            return m_stone2_iid;
        }

      private:
        /** Builds the current material page and its captions.
         * @return Interpolation variables for the material-list document.
         */
        [[nodiscard]] UiVarMap material_variables() const;

        /** Draws the read-only recipe table.
         * @param engine Rendering engine.
         * @param ui Shared widget helpers.
         * @param font FreeType body face.
         */
        void
        draw_materials(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font) const;

        /** @return Number of material-list pages, including an empty first page. */
        [[nodiscard]] std::size_t material_page_count() const;

        /** One pentagram slot holding a bag instance. */
        class Slot {
          public:
            /** @return Bag instance id, or 0 when empty. */
            [[nodiscard]] std::uint32_t iid() const {
                return m_iid;
            }

            /** @return item definition index, or 0 when empty. */
            [[nodiscard]] int index() const {
                return m_index;
            }

            /** @return mix_type low 8 bits captured at drop. */
            [[nodiscard]] std::uint8_t mix_type() const {
                return m_mix_type;
            }

            /** @return True when the slot holds an item. */
            [[nodiscard]] bool filled() const {
                return m_iid != 0;
            }

            /**
             * Places an item in the slot.
             * @param iid Bag instance id.
             * @param index item definition index.
             * @param mix_type mix_type low 8 bits.
             */
            void set(std::uint32_t iid, int index, std::uint8_t mix_type) {
                m_iid = iid;
                m_index = index;
                m_mix_type = mix_type;
            }

            /** Clears the slot. */
            void clear() {
                m_iid = 0;
                m_index = 0;
                m_mix_type = 0;
            }

          private:
            /** Bag instance id, or 0. */
            std::uint32_t m_iid = 0;

            /** item definition index, or 0. */
            int m_index = 0;

            /** mix_type low 8 bits captured at drop. */
            std::uint8_t m_mix_type = 0;
        };

        /** Builds the frame style used for close and title-drag hit tests. */
        [[nodiscard]] WindowFrameStyle frame_style() const;

        /** Draws chrome pieces from the layout, with the inner-frame dark fill. */
        void draw_chrome(WidgetTools& ui) const;

        /**
         * Fills the bottom-right close hitbox in screen pixels.
         * @param sx Out X.
         * @param sy Out Y.
         * @param sw Out W.
         * @param sh Out H.
         * @return True when the hitbox has positive size.
         */
        [[nodiscard]] bool exit_bottom_rect(float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Fills a pentagram slot rect in screen pixels.
         * @param slot Slot index 0..4 (center, north, east, south, west).
         * @param sx Out X.
         * @param sy Out Y.
         * @param sw Out W.
         * @param sh Out H.
         * @return True when the slot index is valid.
         */
        [[nodiscard]] bool slot_rect(int slot, float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Fills the Start Mixing button rect in screen pixels.
         * @param sx Out X.
         * @param sy Out Y.
         * @param sw Out W.
         * @param sh Out H.
         */
        void mix_button_rect(float& sx, float& sy, float& sw, float& sh) const;

        /**
         * Slot under the pointer, or -1.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Slot index 0..4, or -1.
         */
        [[nodiscard]] int slot_at(float mx, float my) const;

        /**
         * True when the pointer is over the mix button.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when over the button.
         */
        [[nodiscard]] bool mix_button_hit(float mx, float my) const;

        /**
         * True when the item may occupy the given slot.
         * Mystery and Demon Blood need a center weapon that is already mix level 2.
         * @param slot Slot index (0 = center).
         * @param def Item definition.
         * @param index item definition index.
         * @return True when the drop is allowed.
         */
        [[nodiscard]] bool slot_accepts(int slot, const ItemDef& def, int index) const;

        /**
         * Rebuilds classified C2S_MIXING fields from the five slots.
         * @param catalog Item catalog.
         */
        void refresh_classification(const ItemDb& catalog);

        /**
         * Validates whether the currently selected slots can form a valid
         * mixing request for the active mode.
         * @param catalog Item catalog.
         * @return True when the UI can send C2S_MIXING.
         */
        [[nodiscard]] bool can_make_mix(const ItemDb& catalog) const;

        /**
         * Clears every pentagram slot and the classified mix fields.
         */
        void clear_slots();

        /** Visible. */
        bool m_open = false;

        /** Mixing kind (0 elemental, 1 imperial). */
        std::uint8_t m_mode = 0;

        /** Geometry. */
        float m_x = 40.0f;

        /** Top. */
        float m_y = 110.0f;

        /** Width. */
        float m_w = 440.0f;

        /** Height. */
        float m_h = 500.0f;

        /** Display scale. */
        float m_scale = 1.0f;

        /** Mix Master recipes; not owned. */
        const server::MixCatalog* m_mix_catalog = nullptr;

        /** Layout document; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Read-only material-list layout; not owned. */
        const UiLayout* m_material_layout = nullptr;

        /** Whether the NPC material list is active. */
        bool m_show_materials = false;

        /** Current material-list page, with nine recipes per page. */
        std::size_t m_material_page = 0;

        /** Parsed openkal.ui/v1 document. */
        UiDocument m_v1;

        /** Pentagram slots: 0 center, then north, east, south, west. */
        std::array<Slot, kSlotCount> m_slots{};

        /** Classified weapon iid for C2S_MIXING, or 0. */
        std::uint32_t m_weapon_iid = 0;

        /** Classified extra weapon iid for C2S_MIXING, or 0. */
        std::uint32_t m_extra_weapon_iid = 0;

        /** Classified armor iid for C2S_MIXING, or 0. */
        std::uint32_t m_armor_iid = 0;

        /** Classified first stone iid for C2S_MIXING, or 0. */
        std::uint32_t m_stone1_iid = 0;

        /** Classified second stone iid for C2S_MIXING, or 0. */
        std::uint32_t m_stone2_iid = 0;

        /** Cached item indices for validation / mapping. */
        int m_weapon_index = 0;

        /** Armor item definition index. */
        int m_armor_index = 0;

        /** First stone item definition index. */
        int m_stone1_index = 0;

        /** Second stone item definition index. */
        int m_stone2_index = 0;

        /** Cached mix_type low 8 bits for the selected weapon. */
        std::uint8_t m_weapon_mix_type = 0;

        /** True while the window itself is being dragged. */
        bool m_dragging = false;

        /** Pointer offset inside the window while dragging, X. */
        float m_drag_dx = 0.0f;

        /** Pointer offset inside the window while dragging, Y. */
        float m_drag_dy = 0.0f;

        /** Document widget under the pointer for shared Dragon button feedback. */
        std::string m_hover_widget;

        /** True while the mix button is held after a valid press. */
        bool m_button_pressed = false;
    };

}  // namespace openkal::client
