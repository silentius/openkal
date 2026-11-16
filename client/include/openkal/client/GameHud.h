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

#include "openkal/client/ChatWidget.h"
#include "openkal/client/DragonHud.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/MenuBarWidget.h"
#include "openkal/client/MinimapWidget.h"
#include "openkal/client/ServerClock.h"
#include "openkal/client/StatusWidget.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <array>

namespace openkal::client {

    /** Named on-map HUD panels, mirroring the KalOnline compatibility behavior keys. */
    enum class HudPanelId : int {
        Status = 0,
        Target,
        Minimap,
        Dragon,
        Chat,
        MenuBar,
        Clock,
        Count,
    };

    /** Screen rect of one draggable HUD panel, in pixels. */
    class MovablePanel {
      public:
        MovablePanel() = default;

        /**
         * Builds a panel from its rect and flags.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param movable True when the player may drag the panel.
         * @param visible True when the panel is drawn and hit-tested.
         */
        MovablePanel(float x, float y, float w, float h, bool movable, bool visible)
            : m_x(x), m_y(y), m_w(w), m_h(h), m_movable(movable), m_visible(visible) {}

        /** @return Panel left in pixels. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Panel top in pixels. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Panel width in pixels. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Panel height in pixels. */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /** @return True when the player may drag the panel. */
        [[nodiscard]] bool movable() const {
            return m_movable;
        }

        /** @return True when the panel is drawn and hit-tested. */
        [[nodiscard]] bool visible() const {
            return m_visible;
        }

        /**
         * Moves the panel.
         * @param x Panel left.
         * @param y Panel top.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
        }

        /**
         * Resizes the panel.
         * @param w Panel width.
         * @param h Panel height.
         */
        void set_size(float w, float h) {
            m_w = w;
            m_h = h;
        }

        /**
         * Enables or disables dragging.
         * @param movable True when the player may drag the panel.
         */
        void set_movable(bool movable) {
            m_movable = movable;
        }

        /**
         * Shows or hides the panel.
         * @param visible True when the panel is drawn and hit-tested.
         */
        void set_visible(bool visible) {
            m_visible = visible;
        }

      private:
        /** Panel left in pixels. */
        float m_x = 0;

        /** Panel top in pixels. */
        float m_y = 0;

        /** Panel width in pixels. */
        float m_w = 0;

        /** Panel height in pixels. */
        float m_h = 0;

        /** True when the player may drag the panel. */
        bool m_movable = true;

        /** True when the panel is drawn and hit-tested. */
        bool m_visible = true;
    };

    /** Outcome of feeding a frame's pointer state to the HUD. */
    enum class HudPointerResult : int {
        Ignored = 0,
        Handled = 1,
        MenuAction = 2,
        ClockClosed = 3,
    };

    /**
     * In-world renewal HUD — a C++ layout mirroring KControl.
     *
     * Panels are draggable and clamped to the screen; every size and offset scales
     * with the UI scale resolved by UiSettings. The HUD positions the panels,
     * tracks pointer feedback and forwards snapshots to their drawing widgets.
     */
    class GameHud {
      public:
        /**
         * Binds the layout store used to look up per-widget JSON documents.
         * @param store Layout store; must outlive the HUD. Pass nullptr for baked
         * defaults.
         */
        void set_layouts(const UiLayoutStore* store) {
            m_layouts = store;
        }

        /** Binds the clock for the active connection.
         * @param clock Clock whose lifetime covers HUD use, or nullptr to clear it.
         */
        void set_server_clock(const ServerClock* clock) {
            m_server_clock = clock;
        }

        /** Shows or hides the clock and dismisses its event menu when hidden.
         * @param visible Profile clock preference.
         */
        void set_clock_visible(bool visible);

        /** @return Persisted clock preference, independent of layout rebuilding. */
        bool clock_visible() const {
            return m_clock_visible;
        }

        /** @return Layout store, or nullptr when the HUD uses baked defaults. */
        [[nodiscard]] const UiLayoutStore* layouts() const {
            return m_layouts;
        }

        /** @return Current UI scale. */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the UI scale, clamped to a sane minimum.
         * @param s Requested scale.
         */
        void set_scale(float s);

        /** @return Menu action produced by the last handle_pointer() call. */
        [[nodiscard]] HudMenuAction last_menu_action() const {
            return m_last_menu_action;
        }

        /** Updates the menu marker from an actual window's visibility.
         * @param action Window's menu action.
         * @param open Whether the window is open.
         */
        void set_menu_open(HudMenuAction action, bool open);

        /** Updates the unread count displayed on the Messages button.
         * @param count Number of unread inbox entries.
         */
        void set_unread_messages(std::size_t count);

        /** Updates the Trading Post collection indicator.
         * @param pending Whether items or sale proceeds await NPC collection. */
        void set_trading_post_collection_pending(bool pending);

        /** Updates hover and press feedback independently of click dispatch.
         * @param input Current pointer input.
         * @param blocked Whether a foreground window or interaction owns the pointer.
         */
        void update_menu_pointer(const engine::FrameInput& input, bool blocked);

        /**
         * Reads one panel's rect.
         * @param id Panel to read.
         * @return Panel rect and flags.
         */
        [[nodiscard]] const MovablePanel& panel(HudPanelId id) const;

        /**
         * Moves one panel, used when restoring saved per-character positions.
         * @param id Panel to move.
         * @param x Panel left in pixels.
         * @param y Panel top in pixels.
         */
        void set_panel_pos(HudPanelId id, float x, float y);

        /**
         * Rebuilds every panel rect from the current scale and screen size.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void reset_defaults(float screen_w, float screen_h);

        /**
         * Clamps every panel back inside the screen.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void clamp_to_screen(float screen_w, float screen_h);

        /**
         * Rebuilds the layout when the scale or screen size changed, preserving
         * logical positions.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param new_scale Scale resolved for this frame.
         * @return True when the layout was rebuilt, so the caller can re-apply saved
         * positions.
         */
        [[nodiscard]] bool ensure_layout(float screen_w, float screen_h, float new_scale);

        /**
         * Applies one frame of pointer input: menu clicks first, then minimap
         * zoom buttons, then panel dragging.
         * @param in Frame input.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @return Whether the HUD consumed the pointer, and whether a menu icon
         * fired.
         */
        HudPointerResult
        handle_pointer(const engine::FrameInput& in, float screen_w, float screen_h);

        /** @return True while a panel is being dragged. */
        [[nodiscard]] bool is_dragging() const {
            return m_drag_panel >= 0;
        }

        /**
         * Tests whether the pointer is over any visible panel.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when a visible panel is under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Draws every visible panel through its widget.
         * @param ui Shared UI helpers.
         * @param engine Engine used by widgets that draw untextured rects.
         * @param status Player status snapshot.
         * @param minimap Player world position for the minimap.
         * @param exp Experience snapshot.
         */
        void draw(WidgetTools& ui,
                  engine::IEngine& engine,
                  const StatusWidgetData& status,
                  const MinimapWidgetData& minimap,
                  const DragonHudData& exp) const;

        /**
         * Loads or reloads the HUD FreeType faces used by panels and menu tooltips.
         * @param engine Engine used to create the atlas texture.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases the HUD FreeType atlas.
         * @param engine Engine that owns the texture.
         */
        void destroy_fonts(engine::IEngine& engine);

        /** @return Mutable chat widget (history, input, filters). */
        [[nodiscard]] ChatWidget& chat() {
            return m_chat;
        }

        /** @return Chat widget. */
        [[nodiscard]] const ChatWidget& chat() const {
            return m_chat;
        }

      private:
        /**
         * Mutable access to one panel.
         * @param id Panel to fetch.
         * @return Panel rect and flags.
         */
        [[nodiscard]] MovablePanel& panel(HudPanelId id);

        /**
         * Finds the topmost visible panel under the pointer.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Panel id as an int, or -1 when nothing was hit.
         */
        [[nodiscard]] int hit_panel(float mx, float my) const;

        UiVarMap clock_vars() const;
        UiDocument clock_options_document() const;
        UiChromeRect clock_options_rect() const;
        bool clock_options_contains(float x, float y) const;
        void draw_clock(WidgetTools& ui, engine::IEngine& engine) const;
        /** Non-owning clock for the active network connection. */
        const ServerClock* m_server_clock = nullptr;
        /** Profile preference retained when rebuilding panel geometry. */
        bool m_clock_visible = true;
        bool m_clock_options_open = false;
        std::size_t m_clock_page = 0;
        /** Stable event ID retained across authoritative list refreshes. */
        std::string m_clock_event_id;

        /** Expands multiplayer chat to halve its positive gap to the skill HUD. */
        void update_chat_width();

        /** Panel rects indexed by HudPanelId. */
        std::array<MovablePanel, static_cast<std::size_t>(HudPanelId::Count)> m_panels{};

        /** Menu action produced by the last handle_pointer() call. */
        HudMenuAction m_last_menu_action = HudMenuAction::None;

        /** Menu pointer feedback and actual window visibility. */
        MenuBarWidgetData m_menu_data;

        /** Current UI scale. */
        float m_scale = 1.0f;

        /** Layout store used to look up per-widget JSON documents; not owned. */
        const UiLayoutStore* m_layouts = nullptr;

        /** Stateful chat panel (history, input, filters). */
        mutable ChatWidget m_chat;

        /** Shared HUD body face for panel text and menu tooltips. */
        mutable FreetypeFont m_hud_font;
        mutable FreetypeFont m_hud_bold_font;

        /** Last loaded HUD body pixel size. */
        int m_hud_font_pixel_size = 0;

        /** Panel currently being dragged, or -1. */
        int m_drag_panel = -1;

        /** Pointer offset inside the dragged panel, X. */
        float m_drag_dx = 0;

        /** Pointer offset inside the dragged panel, Y. */
        float m_drag_dy = 0;

        /** Discrete minimap zoom (1, 4, or 8). */
        int m_minimap_zoom = 1;

        /** True once reset_defaults() has run at least once. */
        bool m_defaults_ready = false;

        /** Framebuffer width the layout was last built for. */
        float m_last_w = 0;

        /** Framebuffer height the layout was last built for. */
        float m_last_h = 0;
    };

}  // namespace openkal::client
