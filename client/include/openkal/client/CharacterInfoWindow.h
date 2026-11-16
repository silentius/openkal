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
#include "openkal/client/NetClient.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace openkal::client {

    /** Outcome of a click offered to the character info window. */
    enum class CharacterInfoClickResult {
        Ignored = 0,
        Consumed,
        ApplyPending,
    };

    /**
     * Dragon Character Info dialog: identity, staged attribute allocation,
     * combat stats, resistances, and active item effects.
     */
    class CharacterInfoWindow {
      public:
        /** @return True while the window is shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /**
         * Shows or hides the window.
         * @param open True to show it.
         */
        void set_open(bool open) {
            m_open = open;
            if (!open) {
                m_dragging = false;
                m_hovered_button.clear();
                m_pointer_down = false;
            }
        }

        /** Flips the open state. */
        void toggle() {
            set_open(!m_open);
        }

        /** @return Client box left in pixels. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Client box top in pixels. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Client box width in pixels. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Client box height in pixels. */
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
         * Binds the layout document holding chrome and geometry.
         * @param layout Layout; must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** Applies window.w / window.h from the layout, keeping the current position.
         */
        void sync_from_layout();

        /**
         * Loads or reloads body + title FreeType faces from layout font_size /
         * title.*. Call after layout bind/reload and before draw when those keys may
         * have changed.
         * @param engine Engine used to create the atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases body and title atlas textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Resizes from the layout and centers the window on screen.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * Replaces the displayed combat / identity snapshot.
         * @param stats Latest NetClient character info stats.
         */
        void set_stats(const NetClient::CharInfoStats& stats);

        /**
         * Sets the primary-stat bonuses supplied by currently worn equipment.
         * @param bonuses STR, HTH, INT, WIS, and AGI bonuses in that order.
         */
        void set_equipment_primary_bonuses(std::array<int, 5> bonuses) {
            m_equipment_primary_bonuses = bonuses;
        }

        /** @return Bound player class used for GetNeedPoint preview. */
        [[nodiscard]] int player_clazz() const {
            return m_stats.clazz;
        }

        /**
         * Sets active item buffs for the Apply Item grid.
         * @param buffs Item-sourced active buffs, in display order.
         */
        void set_apply_buffs(std::vector<BuffSlotEntry> buffs) {
            m_apply_buffs = std::move(buffs);
        }

        /**
         * Gets the tooltip for an Apply Item grid icon under the pointer.
         * @param mx Pointer X in client pixels.
         * @param my Pointer Y in client pixels.
         * @param now_ms Current steady-clock milliseconds.
         * @param ui Optional helpers for native texture dimensions.
         * @return Tooltip text, or empty when no active item buff is under the pointer.
         */
        [[nodiscard]] std::string
        apply_buff_tooltip(float mx, float my, std::uint64_t now_ms, WidgetTools* ui) const;

        /**
         * Callback invoked for each primary batch when OK is pressed.
         * @param property PropertyType Str..Dex ordinal.
         * @param count Requested raise count.
         */
        using ApplyPropertyFn = std::function<bool(std::uint8_t property, std::uint8_t count)>;

        /**
         * Sets the network apply callback used by OK.
         * @param fn Send function; return true when the packet was queued.
         */
        void set_apply_property(ApplyPropertyFn fn) {
            m_apply_property = std::move(fn);
        }

        /** Clears staged pending adds. */
        void clear_pending();

        /**
         * Tests whether the pointer is over the framed window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when open and under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** @return True while the window itself is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /**
         * Applies one frame of pointer input for title-bar dragging.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Applies a left click: closes, stages +/- points, or applies them.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param ui Optional helpers for native BMP sizes when w/h are 0.
         * @return What the window did with the click.
         */
        [[nodiscard]] CharacterInfoClickResult
        handle_click(float mx, float my, WidgetTools* ui = nullptr);

        /**
         * Draws chrome and TTF labels/values.
         * @param engine Engine used for untextured rects and font quads.
         * @param ui Shared UI helpers.
         * @param font FreeType face for all Info text.
         */
        void draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font) const;

      private:
        /** Builds the frame style used for close/hit tests. */
        [[nodiscard]] WindowFrameStyle frame_style() const;

        /** Reloads the v1 document from the bound layout. */
        void reload_document();

        /**
         * Fills interpolation vars from current stats and staged PU.
         * @return Catalog for ${…} and conditions.
         */
        [[nodiscard]] UiVarMap make_vars() const;

        /** @return Number of staged allocation intents. */
        [[nodiscard]] int pending_count() const;

        /**
         * Returns the authoritative base value of a primary stat.
         * @param index Primary-stat index (STR through DEX).
         * @return Current base stat, or zero for an invalid index.
         */
        [[nodiscard]] std::int32_t primary_stat_value(int index) const;

        /**
         * Returns the total PU reserved by all staged allocations.
         * @return Total point cost of the staged allocations.
         */
        [[nodiscard]] int pending_point_cost() const;

        /**
         * Returns the cost of the next staged allocation for one stat.
         * @param index Primary-stat index (STR through DEX).
         * @return Required PU, or zero when the allocation is invalid.
         */
        [[nodiscard]] int next_point_cost(int index) const;

        /** True while the window is shown. */
        bool m_open = false;

        /** Client box left in pixels. */
        float m_x = 80.0f;

        /** Client box top in pixels. */
        float m_y = 60.0f;

        /** Client box width in pixels. */
        float m_w = 245.0f;

        /** Client box height in pixels. */
        float m_h = 417.0f;

        /** Display scale. */
        float m_scale = 1.0f;

        /** Layout document; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Parsed openkal.ui/v1 document. */
        UiDocument m_v1;

        /** Body face (layout font_size); empty when not loaded. */
        FreetypeFont m_body_font;

        /** Last loaded body pixel size. */
        int m_body_font_pixel_size = 0;

        /** Title face (size/bold from layout); empty when not loaded. */
        FreetypeFont m_title_font;

        /** Last loaded title pixel size (avoids redundant atlas rebuilds). */
        int m_title_font_pixel_size = 0;

        /** Last loaded title bold flag. */
        bool m_title_font_bold = false;

        /** Latest server/local stats. */
        NetClient::CharInfoStats m_stats{};

        /** Staged primary adds (STR..DEX). */
        std::array<int, 5> m_pending{{0, 0, 0, 0, 0}};

        /** Worn-equipment bonuses for the primary-stat display (STR..DEX). */
        std::array<int, 5> m_equipment_primary_bonuses{{0, 0, 0, 0, 0}};

        /** Button under the pointer, used by the shared renderer. */
        std::string m_hovered_button;
        bool m_pointer_down = false;

        /** Item-sourced active buffs for the Apply Item grid. */
        std::vector<BuffSlotEntry> m_apply_buffs;

        /** Network apply callback. */
        ApplyPropertyFn m_apply_property;

        /** True while the window itself is being dragged. */
        bool m_dragging = false;

        /** Pointer offset inside the window while dragging, X. */
        float m_drag_dx = 0.0f;

        /** Pointer offset inside the window while dragging, Y. */
        float m_drag_dy = 0.0f;
    };

}  // namespace openkal::client
