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
#include "openkal/client/UiView.h"
#include "openkal/protocol/TradingPost.h"

#include <optional>

namespace openkal::client {
    class UiLayoutStore;
    /** Anonymous fixed-price market with server-owned offers and deferred NPC collection. */
    class TradingPostWindow final {
      public:
        /** @param layouts Shared universal UI document store. */
        explicit TradingPostWindow(UiLayoutStore& layouts);
        /** @return Whether the market owns modal input. */
        bool open() const {
            return m_open;
        }

        /** Opens/closes the window and schedules a fresh page. @param value Visibility. */
        void set_open(bool value);

        /** Opens the character's deliveries through a collection NPC.
         * @param npc Runtime NPC ID, checked again by the server on pickup. */
        void show_collection(std::uint32_t npc);

        /** Applies new server data and fits the window to the viewport.
         * @param state Latest page. @param serial Network revision.
         * @param input Frame dimensions and timing. @param scale User UI scale.
         * @param bag Current inventory. @param items Catalog for filters and icons. */
        void update(const protocol::TradingPostState& state,
                    std::uint64_t serial,
                    const engine::FrameInput& input,
                    float scale,
                    const Bag& bag,
                    const ItemDb& items);

        /** Processes modal input. @param input Frame input. @param ui Texture-aware helpers.
         * @param items Item catalog. @return Optional command ready for transport. */
        [[nodiscard]] std::optional<protocol::TradingPostRequest>
        handle(const engine::FrameInput& input, WidgetTools& ui, const ItemDb& items);

        /** Clears a failed send and exposes a retry message. */
        void send_failed();

        /** Draws the market and full instance tooltip.
         * @param engine Renderer. @param ui UI helpers. @param font FreeType fallback.
         * @param items Item catalog. @param input Pointer position. */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const ItemDb& items,
                  const engine::FrameInput& input) const;

        /** Supplies the same pet definition details used by inventory tooltips.
         * @param info Definitions keyed by item index. */
        void set_pet_tooltip_info(const std::unordered_map<int, PetTooltipInfo>& info) {
            m_pet_info = info;
        }

        /** @return Bound variables, also used by UI verification. */
        UiVarMap variables() const;

      private:
        class Host;
        using Option = std::pair<int, std::string>;
        std::pair<float, float> dropdown_origin() const;
        void build_inventory(const Bag& bag, const ItemDb& items);
        std::vector<Option> options(int filter, const ItemDb& items) const;
        const protocol::TradingPostEntry* selected() const;
        const protocol::TradingPostEntry* entry(std::string_view bind, int index) const;
        std::vector<protocol::TradingPostEntry> entries() const;
        UiHit hit(const UiDocument& doc,
                  const engine::FrameInput& input,
                  WidgetTools& ui,
                  float x,
                  float y) const;
        std::optional<protocol::TradingPostRequest> request(protocol::TradingPostCommand command);
        void tooltip(WidgetTools& ui,
                     engine::IEngine& engine,
                     const FreetypeFont& font,
                     const ItemDb& items,
                     const engine::FrameInput& input,
                     const protocol::TradingPostEntry& item) const;
        UiLayoutStore& m_layouts;
        UiDocument m_document, m_quantity_document, m_option_document;
        protocol::TradingPostState m_state;
        protocol::TradingPostFilter m_filter;
        protocol::TradingPostView m_view = protocol::TradingPostView::Buy;
        std::vector<protocol::TradingPostEntry> m_inventory;
        std::vector<Option> m_options;
        std::unordered_map<int, PetTooltipInfo> m_pet_info;
        std::uint64_t m_serial = 0, m_next_operation = 1, m_pending = 0, m_selected = 0;
        std::uint32_t m_collection_npc = 0;
        bool m_open = false, m_refresh = false, m_quantity_open = false, m_pristine = true,
             m_price_focus = false;
        bool m_center_pending = true;
        bool m_dragging = false;
        float m_drag_dx = 0, m_drag_dy = 0;
        int m_dropdown = -1, m_option_scroll = 0, m_quantity_max = 1;
        std::string m_quantity = "1", m_price, m_status;
        float m_x = 0, m_y = 0, m_qx = 0, m_qy = 0, m_scale = 1, m_width = 1280, m_height = 900,
              m_pending_seconds = 0;
    };
}  // namespace openkal::client
