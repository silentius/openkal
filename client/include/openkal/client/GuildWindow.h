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
#include "openkal/client/TextWrap.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Guild.h"

#include <optional>
namespace openkal::client {
    /** Server-driven Dragon guild browser, management and founding consent. */
    class GuildWindow final {
      public:
        /** @param layout Universal guild layout. */
        void set_layout(const UiLayout* layout);

        /** Sets the shared rank selection/editor layout.
         * @param layout Universal rank dialog layout.
         */
        void set_rank_layout(const UiLayout* layout);

        /** Closes the rank dialog before the parent window.
         * @return Whether a rank dialog was closed.
         */
        bool close_rank_dialog();

        /** Restores rank controls after a request could not be sent. */
        void request_failed();

        /** Sets the preferred UI scale, reduced only when needed to fit the screen.
         * @param scale Global UI scale.
         */
        void set_scale(float scale);

        /** @param open Visibility. */
        void set_open(bool open);

        /** Opens creation with server requirements, or the member roster when already in a guild.
         */
        void show_creation();

        /** @return Current visibility. */
        bool open() const {
            return m_open;
        }
        /** @return Whether the guild editor or modal rank dialog owns the keyboard. */
        bool focused() const {
            return m_open && (m_focused || m_rank_open);
        }
        /** @param view Authenticated server view. */
        void update(const platform::Json& view);
        /** @param input Frame input. @param ui UI helpers. @return Optional server operation. */
        [[nodiscard]] std::optional<protocol::GuildRequest> handle(const engine::FrameInput& input,
                                                                   WidgetTools& ui);
        /** @param x Pointer X. @param y Pointer Y. @return Window hit. */
        bool contains(float x, float y) const;
        /** @param engine Renderer. @param ui UI helpers. @param font FreeType body face. */
        void draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font);
        /** @param engine Renderer owning font textures. */
        void destroy(engine::IEngine& engine);
        /** @return Resolved bindings, also used by owned UI/editor tests. */
        UiVarMap variables() const;
        /** @param action Widget action. @return Whether current server permissions allow it. */
        bool enabled(std::string_view action) const;

      private:
        class Host;
        protocol::GuildRequest query() const;
        void open_rank_dialog(bool edit);
        void load_rank_draft(int rank);
        std::string rank_unavailable(int rank) const;
        std::optional<protocol::GuildRequest> handle_rank_dialog(const engine::FrameInput& input,
                                                                 WidgetTools& ui);
        std::vector<TextLine> announcement_lines() const;
        void edit_announcement(const engine::FrameInput& input);
        void cancel_announcement();
        const UiLayout* m_layout = nullptr;
        UiDocument m_document;
        const UiLayout* m_rank_layout = nullptr;
        UiDocument m_rank_document;
        bool m_rank_open = false, m_rank_edit = false, m_rank_pending = false;
        protocol::GuildCommand m_rank_command = protocol::GuildCommand::Snapshot;
        std::uint32_t m_rank_target = 0, m_rank_guild = 0;
        std::uint64_t m_rank_request_id = 0;
        std::string m_rank_input, m_rank_status;
        bool m_announcement_editing = false, m_announcement_pending = false;
        std::uint32_t m_announcement_guild = 0;
        std::uint64_t m_announcement_request = 0;
        bool m_daily_pending = false;
        std::uint64_t m_daily_request = 0;
        std::size_t m_text_cursor = 0;
        int m_cursor_direction = 0;
        float m_cursor_repeat = 0;
        platform::Json m_view;
        FreetypeFont m_font, m_title;
        bool m_open = false, m_focused = false, m_refresh = false, m_dragging = false;
        /** Center on opening and resizing until the player drags the window. */
        bool m_centered = true;
        float m_x = 0, m_y = 0, m_scale = 1, m_ui_scale = 1, m_drag_x = 0, m_drag_y = 0,
              m_mouse_x = 0, m_mouse_y = 0;
        bool m_mouse_down = false;
        int m_tab = 0, m_rank = 2, m_mask = 0, m_selected = -1;
        /** Browsing category that owns the currently inspected guild details. */
        int m_detail_parent_tab = 0;
        std::uint32_t m_page = 0;
        std::uint64_t m_last_query_ms = 0;
        std::string m_input, m_search, m_status, m_confirm;
    };
}  // namespace openkal::client
