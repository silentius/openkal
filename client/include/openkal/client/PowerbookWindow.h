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

#include "openkal/client/ItemTooltip.h"
#include "openkal/client/LocationMapView.h"
#include "openkal/client/PowerbookCatalog.h"
#include "openkal/client/UiView.h"

namespace openkal::client {
    class ItemDb;

    /** Fullscreen monster, loot and spawn atlas over the universal UI schema. */
    class PowerbookWindow final {
      public:
        /** @param layout Universal Powerbook layout. */
        void set_layout(const UiLayout* layout);

        /** Opens the book, lazily reading optional runtime catalogs once per session.
         * @param configuration_root Active client configuration root.
         * @param items Shared item names and icons; must outlive this window. */
        void open(const std::string& configuration_root, const ItemDb& items);

        /** Closes the book, retaining selection and scroll positions. */
        void close();

        /** Sets character context for requirement colors in catalog tooltips.
         * @param clazz Current character class.
         * @param level Current character level. */
        void set_player(int clazz, int level);

        /** @return Whether the book captures input and covers the viewport. */
        bool is_open() const;

        /** Handles book controls, including P/Escape and independent scrollbars.
         * @param input Unmuted pointer and keyboard snapshot.
         * @param ui Shared texture and hit-testing helpers. */
        void update(const engine::FrameInput& input, WidgetTools& ui);

        /** Draws the fullscreen document and selected spawn map.
         * @param engine UI renderer.
         * @param ui Shared texture helpers.
         * @param width Framebuffer width.
         * @param height Framebuffer height. */
        void draw(engine::IEngine& engine, WidgetTools& ui, float width, float height);

        /** @param engine Renderer owning this window's font textures. */
        void destroy(engine::IEngine& engine);

        /** @return Current text and visibility bindings for the universal layout. */
        UiVarMap variables() const;

        /** @return Inventory-style details for the hovered drop, or no lines outside a slot. */
        std::vector<TipLine> tooltip_lines() const;

      private:
        class Host;
        struct Rect {
            float x = 0, y = 0, w = 0, h = 0;
            bool contains(float px, float py) const;
        };
        static constexpr int kRows = 16;
        static constexpr int kDropColumns = 6;
        static constexpr int kDropRows = 10;
        static constexpr int kDropSlots = kDropColumns * kDropRows;
        int drop_row_count() const;
        Rect widget_rect(std::string_view id, float width, float height) const;
        const PowerbookMonster* selected() const;
        void select(int index);
        void
        draw_map(engine::IEngine& engine, WidgetTools& ui, const Rect& rect, float scale) const;
        UiDocument m_doc;
        PowerbookCatalog m_catalog;
        LocationMapView m_location_map;
        const ItemDb* m_items = nullptr;
        bool m_loaded = false;
        bool m_open = false;
        int m_selected = -1;
        int m_monster_scroll = 0;
        int m_drop_scroll = 0;
        int m_hover_drop = -1;
        int m_player_clazz = 0;
        int m_player_level = 1;
        float m_mouse_x = 0, m_mouse_y = 0;
        std::string m_hover_widget;
        bool m_pointer_pressed = false;
        int m_spawn = 0;
        int m_drag_scroll = 0;
        float m_drag_grab = 0;
        float m_width = 1600, m_height = 900;
        FreetypeFont m_font;
        FreetypeFont m_title_font;
        FreetypeFont m_tooltip_font;
    };
}  // namespace openkal::client
