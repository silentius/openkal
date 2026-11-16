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

#include "openkal/client/UiView.h"
#include "openkal/protocol/TalentTree.h"

#include <cstdint>
#include <string>

namespace openkal::client {
    /** Fullscreen, server-confirmed talent draft over the universal UI document. */
    class TalentTreeWindow final {
      public:
        /** Actions returned to the owning game session. */
        enum class Action { None, Apply, Exit, Beacon };

        /** @param layout Universal talent-tree layout. */
        void set_layout(const UiLayout* layout);

        /** @param clazz Character class index, matching the identity HUD. */
        void set_class(int clazz);

        /** Starts a fresh draft from the last authoritative selection. */
        void open();

        /** Discards uncommitted choices. Pending network requests still complete. */
        void close();

        /** @return Whether the fullscreen modal is visible. */
        bool is_open() const {
            return m_open;
        }

        /** @return Last server-confirmed selection. */
        std::uint64_t committed() const {
            return m_state.mask;
        }

        /** @return Selection staged for Apply. */
        std::uint64_t draft() const {
            return m_draft;
        }

        /** @return Whether an authoritative response is pending. */
        bool awaiting() const {
            return m_pending;
        }

        /** @param state New server state; failed applies preserve an editable draft. */
        void receive(const protocol::TalentState& state);

        /** @param sent Whether the request reached the connection. */
        void request_sent(bool sent);

        /** @param dt Elapsed seconds, for displayed Beacon timers only. */
        void tick(float dt);

        /** @param node Node to toggle in the uncommitted draft.
         * @return Whether the draft changed. */
        [[nodiscard]] bool toggle_node(int node);

        /** @param input Pointer, dimensions and click state.
         * @param ui Texture and hit-testing helpers.
         * @return Requested action; never applies locally. */
        [[nodiscard]] Action update(const engine::FrameInput& input, WidgetTools& ui);

        /** @param engine UI renderer.
         * @param ui Texture helpers.
         * @param width Current viewport width.
         * @param height Current viewport height. */
        void draw(engine::IEngine& engine, WidgetTools& ui, float width, float height);

        /** @param engine Renderer owning font textures. */
        void destroy(engine::IEngine& engine);

        /** @return Runtime bindings, also useful for layout previews. */
        UiVarMap variables() const;

      private:
        class Host;
        bool button_enabled(std::string_view action) const;
        void geometry(float width, float height, float& x, float& y, float& scale) const;
        void draw_node_overlay(engine::IEngine& engine,
                               WidgetTools& ui,
                               float width,
                               float height,
                               float x,
                               float y,
                               float scale,
                               const UiVarMap& vars) const;
        UiDocument m_doc;
        protocol::TalentState m_state;
        std::uint64_t m_draft = 0;
        bool m_open = false;
        bool m_have_state = false;
        bool m_pending = false;
        int m_class = 0;
        int m_hover = -1;
        std::string m_hover_widget;
        bool m_pointer_pressed = false;
        std::string m_feedback;
        FreetypeFont m_font;
        FreetypeFont m_title_font;
        FreetypeFont m_overlay_font;
        FreetypeFont m_overlay_title_font;
        float m_timer_fraction_ms = 0.0f;
    };
}  // namespace openkal::client
