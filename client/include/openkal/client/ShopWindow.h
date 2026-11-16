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

#include "openkal/client/Inventory.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/Shop.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openkal::client {

    /** Result of a pointer action in the NPC shop. */
    enum class ShopClickResult {
        Ignored = 0,
        Consumed,
        Close,
        Buy,
        Sell,
    };

    /** Two-pane NPC shop with a read-only player inventory and server offers. */
    class ShopWindow {
      public:
        /** @return True while the shop is visible. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Closes the shop and clears its transaction state. */
        void close();

        /**
         * Opens a server-authoritative offer list.
         * @param list NPC id, item indices, and prices.
         */
        void show(const protocol::ShopItemList& list);

        /** Binds the JSON layout. @param layout Layout; must outlive this window. */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** Applies window geometry from the bound layout. */
        void sync_from_layout();

        /** Sets UI scale. @param scale Scale factor. */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /**
         * Replaces pet definitions used by shop item tooltips.
         * @param info Definitions keyed by item definition index.
         */
        void set_pet_tooltip_info(std::unordered_map<int, PetTooltipInfo> info) {
            m_pet_tooltip_info = std::move(info);
        }

        /** Centers the shop. @param screen_w Screen width. @param screen_h Screen height. */
        void center_on_screen(float screen_w, float screen_h);

        /** @return True when the pointer is over the shop frame. */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** @return True while the title is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** @return True while the quantity layer owns keyboard input. */
        [[nodiscard]] bool quantity_open() const {
            return m_quantity_kind != QuantityKind::None;
        }

        /**
         * Updates title dragging and the item slot under the pointer.
         * @param input Current input.
         * @param bag Current player bag.
         * @param catalog Item definitions used to resolve visible shop slots.
         */
        void update_pointer(const engine::FrameInput& input, const Bag& bag, const ItemDb& catalog);

        /**
         * Edits or confirms the modal quantity using keyboard input.
         * @param input Current input.
         * @return Consumed while the modal is open, or the ready transaction.
         */
        [[nodiscard]] ShopClickResult handle_keyboard(const engine::FrameInput& input);

        /**
         * Handles a shop click.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param ui Texture-aware widget helpers.
         * @param bag Current player bag.
         * @param catalog Item definitions.
         * @param ctrl_pressed True while compatibility behavior is held to toggle an additional
         * sale mark.
         * @return Consumed action or ready transaction.
         */
        [[nodiscard]] ShopClickResult handle_click(float mx,
                                                   float my,
                                                   WidgetTools& ui,
                                                   const Bag& bag,
                                                   const ItemDb& catalog,
                                                   bool ctrl_pressed);

        /** @return Runtime NPC id for the ready transaction. */
        [[nodiscard]] std::uint32_t npc_id() const {
            return m_npc_id;
        }

        /** @return Selected shop item index for a buy. */
        [[nodiscard]] std::uint16_t pending_index() const {
            return m_pending_index;
        }

        /** @return Quantity selected in the modal. */
        [[nodiscard]] std::uint16_t pending_quantity() const {
            return m_pending_quantity;
        }

        /**
         * Takes the next completed sale transaction.
         * @param iid Receives the player inventory instance id.
         * @param quantity Receives the quantity to sell.
         * @return True if a transaction was available.
         */
        [[nodiscard]] bool take_pending_sale(std::uint32_t& iid, std::uint16_t& quantity);

        /** Loads the layout's FreeType sizes. @param engine Texture owner. */
        void sync_fonts(engine::IEngine& engine);

        /** Releases the shop fonts. @param engine Texture owner. */
        void destroy_fonts(engine::IEngine& engine);

        /** Draws the complete shop. @param engine Engine. @param ui Widgets. @param font FreeType
         * face. @param bag Player bag. @param catalog Items. */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag,
                  const ItemDb& catalog) const;

        /**
         * Draws the item tooltip for the slot currently under the pointer.
         * @param ui Widget drawing helper.
         * @param engine Engine used for tooltip quads.
         * @param font FreeType face for tooltip text.
         * @param catalog Item definitions.
         * @param screen_w Framebuffer width for tip clamping.
         * @param screen_h Framebuffer height for tip clamping.
         * @param player_clazz Current player class.
         * @param player_level Current player level.
         */
        void draw_tooltip(WidgetTools& ui,
                          engine::IEngine& engine,
                          const FreetypeFont& font,
                          const ItemDb& catalog,
                          float screen_w,
                          float screen_h,
                          int player_clazz,
                          int player_level) const;

      private:
        enum class Side { None = 0, Inventory, Offers };
        enum class QuantityKind { None = 0, Buy, Sell };

        /** Reloads the bound v1 JSON document. */
        void reload_document() const;
        /** Collects saleable normal-bag stacks in bag order.
         * @param bag Player bag. @param catalog Item definitions.
         * @return Compacted stacks shared by drawing and hit testing. */
        [[nodiscard]] static std::vector<const ItemStack*> inventory_slots(const Bag& bag,
                                                                           const ItemDb& catalog);
        /** @param count Filtered stack count. @return Clamped visible inventory page. */
        [[nodiscard]] int inventory_page(std::size_t count) const;
        /** @return True if a stack can be marked for sale. */
        [[nodiscard]] static bool sellable(const ItemStack& stack, const ItemDb& catalog);
        /** Queues every current compatibility behavior-marked stack for a full-stack sale. */
        [[nodiscard]] bool queue_marked_sales(const Bag& bag, const ItemDb& catalog);
        /** Queues the single sale selected in the quantity prompt. */
        void queue_pending_sale();
        /** @return The marked stack with the primary selection, or nullptr. */
        [[nodiscard]] const ItemStack* selected_inventory_stack(const Bag& bag) const;
        /** Builds dynamic text values for draw and hit testing. */
        [[nodiscard]] UiVarMap vars(const Bag& bag, const ItemDb& catalog) const;
        /** @return Selected item definition, or nullptr. */
        [[nodiscard]] const ItemDef* selected_def(const Bag& bag, const ItemDb& catalog) const;

        bool m_open = false;
        float m_x = 120.0f;
        float m_y = 80.0f;
        float m_w = 552.0f;
        float m_h = 433.0f;
        float m_scale = 1.0f;
        const UiLayout* m_layout = nullptr;
        mutable UiDocument m_v1;
        std::uint32_t m_npc_id = 0;
        std::vector<protocol::ShopOffer> m_offers;
        Side m_selected_side = Side::None;
        std::uint32_t m_selected_iid = 0;
        std::uint16_t m_selected_index = 0;
        int m_inventory_register = 0;
        int m_offers_page = 0;
        FreetypeFont m_body_font;
        FreetypeFont m_title_font;
        QuantityKind m_quantity_kind = QuantityKind::None;
        std::uint16_t m_quantity = 1;
        std::uint16_t m_quantity_max = 1;
        bool m_quantity_typing_started = false;
        /** The first preset replaces the untouched default quantity of one. */
        bool m_quantity_pristine = true;
        std::uint16_t m_pending_index = 0;
        std::uint32_t m_pending_iid = 0;
        std::uint16_t m_pending_quantity = 0;
        std::vector<std::uint32_t> m_marked_inventory_iids;
        std::vector<std::pair<std::uint32_t, std::uint16_t>> m_pending_sales;
        std::unordered_map<int, PetTooltipInfo> m_pet_tooltip_info;
        int m_hover_item_index = 0;
        int m_hover_item_count = 0;
        /** Instance-only data plus catalog pet data for the hovered item. */
        int m_hover_prefix = 0;
        int m_hover_cur_endurance = -1;
        int m_hover_max_endurance = -1;
        ItemTipExtras m_hover_extras;
        std::string m_hover_widget;
        bool m_pointer_pressed = false;
        float m_hover_mx = 0.0f;
        float m_hover_my = 0.0f;
        bool m_dragging = false;
        float m_drag_dx = 0.0f;
        float m_drag_dy = 0.0f;
    };

}  // namespace openkal::client
