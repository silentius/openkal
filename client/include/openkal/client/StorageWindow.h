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
#include "openkal/protocol/Storage.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace openkal::client {

    /** Result of a pointer action in the account-storage window. */
    enum class StorageClickResult { Ignored = 0, Consumed, Close, Deposit, Withdraw };

    /** Two-pane account storage with six persistent pages of 36 items. */
    class StorageWindow final {
      public:
        /** @return True while the storage window is visible. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Closes storage and clears selection state. */
        void close();

        /** Opens or refreshes a server-authoritative snapshot.
         * @param info Account-storage state.
         */
        void show(const protocol::StorageInfo& info);

        /** Binds the JSON layout. @param layout Layout; must outlive the window. */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** Applies window geometry from the bound layout. */
        void sync_from_layout();

        /** Sets UI scale. @param scale Scale factor. */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** Centers storage on screen. @param screen_w Width. @param screen_h Height. */
        void center_on_screen(float screen_w, float screen_h);

        /** @return True when the pointer is over the storage frame. */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** @return True while the title is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** @return True while the transfer quantity layer owns keyboard input. */
        [[nodiscard]] bool quantity_open() const {
            return m_quantity_kind != QuantityKind::None;
        }

        /** Updates dragging and tooltip selection.
         * @param input Current input.
         * @param bag Current player inventory.
         */
        void update_pointer(const engine::FrameInput& input, const Bag& bag);

        /** Edits or confirms the modal transfer quantity using keyboard input.
         * @param input Current input.
         * @return Consumed while the modal is open, or the ready transfer.
         */
        [[nodiscard]] StorageClickResult handle_keyboard(const engine::FrameInput& input);

        /** Handles one pointer click.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param ui Texture-aware widget helper.
         * @param bag Current player inventory.
         * @param ctrl_pressed True while compatibility behavior is held to toggle an additional
         * item mark.
         * @return Consumed action or ready transfer.
         */
        [[nodiscard]] StorageClickResult
        handle_click(float mx, float my, WidgetTools& ui, const Bag& bag, bool ctrl_pressed);

        /** @return Runtime storage-keeper id. */
        [[nodiscard]] std::uint32_t npc_id() const {
            return m_npc_id;
        }

        /** Takes the next queued transfer.
         * @param iid Receives the source item instance id.
         * @param count Receives the selected transfer count.
         * @return True when another transfer was queued.
         */
        [[nodiscard]] bool take_pending_transfer(std::uint32_t& iid, std::uint32_t& count);

        /** Loads the layout's FreeType sizes. @param engine Texture owner. */
        void sync_fonts(engine::IEngine& engine);

        /** Releases owned FreeType atlas textures. @param engine Texture owner. */
        void destroy_fonts(engine::IEngine& engine);

        /** Draws the account-storage window.
         * @param engine Engine.
         * @param ui Widget helper.
         * @param font Fallback FreeType face.
         * @param bag Current player inventory.
         * @param catalog Item definitions.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const Bag& bag,
                  const ItemDb& catalog) const;

        /** Draws the hovered item tooltip.
         * @param ui Widget helper.
         * @param engine Engine.
         * @param font FreeType face.
         * @param catalog Item definitions.
         * @param screen_w Screen width.
         * @param screen_h Screen height.
         * @param player_clazz Player class.
         * @param player_level Player level.
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
        enum class Side { None = 0, Inventory, Storage };
        enum class QuantityKind { None = 0, Deposit, Withdraw };

        void reload_document() const;
        [[nodiscard]] UiVarMap vars(const Bag& bag) const;
        [[nodiscard]] const ItemStack* inventory_item(const Bag& bag, int cell) const;
        [[nodiscard]] const ItemStack* selected_item(const Bag& bag) const;
        [[nodiscard]] bool queue_marked_deposits(const Bag& bag);
        [[nodiscard]] bool queue_marked_withdrawals();
        void begin_quantity(QuantityKind kind, std::uint32_t iid, std::uint32_t maximum);
        [[nodiscard]] StorageClickResult confirm_quantity();
        void set_hover(const ItemStack* item, float mx, float my);

        bool m_open = false;
        float m_x = 120.0f;
        float m_y = 80.0f;
        float m_w = 552.0f;
        float m_h = 433.0f;
        float m_scale = 1.0f;
        const UiLayout* m_layout = nullptr;
        mutable UiDocument m_v1;
        std::uint32_t m_npc_id = 0;
        std::array<ItemStack, protocol::kStorageCapacity> m_storage{};
        Side m_selected_side = Side::None;
        std::uint32_t m_selected_iid = 0;
        int m_inventory_page = 0;
        int m_storage_page = 0;
        std::vector<std::uint32_t> m_marked_inventory_iids;
        std::vector<std::uint32_t> m_marked_storage_iids;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> m_pending_transfers;
        QuantityKind m_quantity_kind = QuantityKind::None;
        std::uint32_t m_quantity = 1;
        std::uint32_t m_quantity_max = 1;
        std::uint32_t m_quantity_iid = 0;
        bool m_quantity_typing_started = false;
        FreetypeFont m_body_font;
        FreetypeFont m_title_font;
        int m_hover_item_index = 0;
        int m_hover_item_count = 0;
        int m_hover_prefix = 0;
        int m_hover_cur_endurance = -1;
        int m_hover_max_endurance = -1;
        ItemTipExtras m_hover_extras;
        float m_hover_mx = 0.0f;
        float m_hover_my = 0.0f;
        bool m_dragging = false;
        float m_drag_dx = 0.0f;
        float m_drag_dy = 0.0f;
    };

}  // namespace openkal::client
