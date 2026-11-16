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
#include "openkal/client/UiDocument.h"
#include "openkal/protocol/Party.h"

#include <optional>

namespace openkal::engine {
    class IEngine;
    struct FrameInput;
}  // namespace openkal::engine
namespace openkal::client {
    class UiLayoutStore;
    class WidgetTools;
    class FreetypeFont;
    /** Compact draggable names/HP roster with separate invitation controls. */
    class PartyWindow final {
      public:
        /** @param layouts Shared universal layout store. */
        void set_layouts(UiLayoutStore* layouts);

        /** Updates authoritative data and clamps the growing window to the viewport.
         * @param state Server snapshot. @param scale Preferred UI scale.
         * @param width Viewport width. @param height Viewport height.
         * @param local_player Local player ID for leader permissions. */
        void update(const protocol::PartyState& state,
                    float scale,
                    float width,
                    float height,
                    std::uint32_t local_player = 0);

        /** Toggles the separate membership actions dialog.
         * @param target Selected player ID, or zero. @param name Selected player name. */
        void toggle_actions(std::uint32_t target = 0, std::string_view name = {});

        /** Closes management without hiding the roster. */
        void close_actions();

        /** Restores an anchor before the next viewport update.
         * @param x Logical X, before HUD scaling. @param y Logical Y, before HUD scaling. */
        void set_logical_position(float x, float y);

        /** @return Preferred logical X, before viewport clamping. */
        float logical_x() const {
            return m_logical_x;
        }

        /** @return Preferred logical Y, before viewport clamping. */
        float logical_y() const {
            return m_logical_y;
        }

        /** @return Whether a default or saved anchor is available. */
        bool positioned() const {
            return m_positioned;
        }

        /** @return Whether the roster is being dragged. */
        bool dragging() const {
            return m_dragging;
        }

        /** @return Whether membership actions are visible. */
        bool actions_open() const {
            return m_actions_open;
        }

        /** @return Whether an active party roster is shown. */
        bool open() const {
            return m_state.id != 0 && !m_state.members.empty();
        }

        /** @param x Pointer X. @param y Pointer Y. @param ui Artwork helper.
         * @return Whether party controls capture the pointer. */
        bool contains(float x, float y, WidgetTools& ui) const;

        /** Handles dragging and one click.
         * @param input Current frame input. @param ui Artwork helper.
         * @return Optional command to send to the server. */
        [[nodiscard]] std::optional<protocol::PartyRequest> handle(const engine::FrameInput& input,
                                                                   WidgetTools& ui);

        /** @param engine Render engine. @param ui Artwork helper. @param font TrueType face.
         * @param input Pointer feedback. */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const engine::FrameInput& input) const;

      private:
        void clamp_position();
        UiVarMap vars(const protocol::PartyMember* member = nullptr) const;
        bool is_leader() const;
        bool can_invite() const;
        UiLayoutStore* m_layouts = nullptr;
        UiDocument m_document;
        UiDocument m_row;
        UiDocument m_invitation_document;
        UiDocument m_actions_document;
        UiDocument m_action_row;
        protocol::PartyState m_state;
        std::uint32_t m_local_player = 0;
        std::uint32_t m_invite_target = 0;
        std::string m_invite_name;
        bool m_actions_open = false;
        bool m_positioned = false;
        bool m_dragging = false;
        float m_x = 80;
        float m_y = 160;
        /** Preferred anchor in HUD units, preserved while viewport clamping moves the list. */
        float m_logical_x = 80;
        float m_logical_y = 160;
        float m_scale = 1;
        float m_preferred_scale = 1;
        float m_height = 18;
        float m_width = 150;
        float m_screen_w = 1280;
        float m_screen_h = 720;
        float m_drag_x = 0;
        float m_drag_y = 0;
    };
}  // namespace openkal::client
