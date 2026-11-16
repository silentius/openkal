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
#include "openkal/protocol/PlayerOptions.h"

#include <optional>

namespace openkal::client {
    class UiLayoutStore;
    /** Server-driven, extensible actions for the current player selection. */
    class PlayerOptionsWindow final {
      public:
        /** @param layouts Universal UI layout store. */
        void set_layouts(UiLayoutStore* layouts);

        /** Opens a fresh context. @param target Selected runtime player ID. */
        void show(std::uint32_t target);

        /** Closes the dialog and ignores outstanding responses. */
        void close();

        /** @return Whether the dialog is visible. */
        bool open() const {
            return m_open;
        }

        /** @return Selected runtime player ID. */
        std::uint32_t target() const {
            return m_state.target;
        }

        /** @param id Assigned transport request ID, or zero on send failure. */
        void request_sent(std::uint64_t id);

        /** @param state Server response; stale selection/request responses are ignored. */
        void update(const protocol::PlayerOptionsState& state);

        /** @param input Current frame. @param ui UI helpers. @param scale Preferred UI scale.
         * @return Optional context query or action. */
        [[nodiscard]] std::optional<protocol::PlayerOptionsRequest>
        handle(const engine::FrameInput& input, WidgetTools& ui, float scale);

        /** @param engine Renderer. @param ui UI helpers. @param fallback Body face. */
        void draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& fallback);

        /** Takes the selected personal-message partner once.
         * @return Character name, or empty when no local PM action is pending.
         */
        [[nodiscard]] std::string take_whisper_target();

        /** @param engine Renderer owning font textures. */
        void destroy(engine::IEngine& engine);

        /** @return Resolved view bindings for the UI and owned tests. */
        UiVarMap variables() const;

      private:
        void reload();
        UiLayoutStore* m_layouts = nullptr;
        UiDocument m_document, m_row;
        protocol::PlayerOptionsState m_state;
        FreetypeFont m_font, m_title;
        std::uint64_t m_expected = 0, m_last_poll = 0;
        bool m_open = false, m_query_due = false, m_mutating = false;
        float m_x = 0, m_y = 0, m_scale = 1, m_w = 420, m_h = 210;
        float m_mouse_x = 0, m_mouse_y = 0;
        bool m_mouse_down = false;
        std::string m_status;
        /** Local compose request waiting for the chat host. */
        std::string m_whisper_target;
    };
}  // namespace openkal::client
