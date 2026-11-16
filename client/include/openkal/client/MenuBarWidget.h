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
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"

#include <array>
#include <string_view>

namespace openkal::client {

    /** Menu strip actions, shared by drawing and pointer dispatch. */
    enum class HudMenuAction : int {
        None = 0,
        Sinsu,
        Riding,
        Character,
        Inventory,
        Skill,
        Quest,
        Party,
        Community,
        Map,
        System,
        Messages,
        TradingPost,
        Count,
    };

    /** Live menu presentation snapshot. Open flags reflect actual window visibility. */
    struct MenuBarWidgetData {
        std::array<bool, static_cast<std::size_t>(HudMenuAction::Count)> open{};
        /** Number of unread inbox entries. */
        std::size_t unread_messages = 0;
        /** Whether this character has market items or proceeds awaiting NPC collection. */
        bool trading_post_collection_pending = false;
        float mouse_x = -1;
        float mouse_y = -1;
        bool pointer_enabled = false;
        bool pressed = false;
    };

    /** Compact Dragon menu assembled from standard JSON images and buttons. */
    class MenuBarWidget {
      public:
        /** Resolves a JSON action name.
         * @param action Action from a button hit.
         * @return Menu action, or None for an unknown name.
         */
        [[nodiscard]] static HudMenuAction action_for(std::string_view action);

        /** Draws the menu strip and its screen-clamped FreeType tooltip.
         * @param ui Shared UI helpers.
         * @param engine UI renderer.
         * @param font Tooltip face.
         * @param x Strip left.
         * @param y Strip top.
         * @param w Strip width.
         * @param h Strip height.
         * @param scale UI scale.
         * @param data Pointer and open-window snapshot.
         * @param screen_w Framebuffer width for tooltip placement.
         * @param screen_h Framebuffer height for tooltip placement.
         * @param layout Menu bar layout document, or nullptr.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         float x,
                         float y,
                         float w,
                         float h,
                         float scale,
                         const MenuBarWidgetData& data,
                         float screen_w,
                         float screen_h,
                         const UiLayout* layout = nullptr);
    };

}  // namespace openkal::client
