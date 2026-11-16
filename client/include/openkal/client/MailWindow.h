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
#include "openkal/client/UiView.h"
#include "openkal/protocol/Mail.h"

#include <chrono>
#include <cstdint>
#include <set>
#include <vector>

namespace openkal::client {
    /** Result of an inbox action for the session controller. */
    enum class MailClickResult : std::uint8_t {
        Ignored,
        Handled,
        Close,
        Read,
        Refresh,
        Claim,
        Delete
    };

    /** Dragon system inbox with explicit reading and confirmed attachment actions. */
    class MailWindow final {
      public:
        /** @return True while the inbox is visible. */
        bool open() const {
            return m_open;
        }

        /** Shows or hides the inbox. @param open True to show. */
        void set_open(bool open);

        /** Toggles inbox visibility. */
        void toggle();

        /** Sets the scale. @param scale Native-pixel scale factor. */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** Binds the shared UI document. @param layout Inbox layout. */
        void set_layout(const UiLayout* layout);

        /** Centers the inbox. @param screen_w Screen width. @param screen_h Screen height. */
        void center_on_screen(float screen_w, float screen_h);

        /** Replaces the authoritative snapshot. @param entries Received mail. */
        void set_entries(std::vector<protocol::MailEntry> entries);

        /** Restores local read state. @param ids Read identifiers for this character. */
        void set_read_ids(std::set<std::uint32_t> ids);

        /** @return Locally remembered read message identifiers. */
        const std::set<std::uint32_t>& read_ids() const {
            return m_read_ids;
        }

        /** @return Unread count, excluding claimed mail. */
        std::size_t unread_count() const;

        /** Consumes the persistence flag. @return True if local read state changed. */
        [[nodiscard]] bool take_read_dirty();

        /** Supplies display data resolved from the item catalog.
         * @param name Item display name.
         * @param icon Item icon file.
         */
        void set_attachment_info(std::string name, std::string icon);

        /** @return True when a fresh list request is allowed by the cooldown and pending state. */
        bool refresh_ready() const;

        /** Records whether a requested operation was sent.
         * @param action Requested operation.
         * @param sent True when the transport accepted the request.
         */
        void request_sent(MailClickResult action, bool sent);

        /** Applies an attachment response. @param result Server claim result. */
        void claim_result(const protocol::MailClaimResult& result);

        /** Updates drag, hover, scrolling and request timeout. @param input Frame input. */
        void update_pointer(const engine::FrameInput& input);

        /** Dispatches one click. @param mx Pointer X. @param my Pointer Y.
         * @return Action for the session controller.
         */
        [[nodiscard]] MailClickResult handle_click(float mx, float my);

        /** @return Selected durable ID, or zero. */
        std::uint32_t selected_id() const;

        /** @return Selected attachment item index, or zero. */
        std::uint16_t selected_item_index() const;

        /** Hit-tests the complete window. @param mx Pointer X. @param my Pointer Y.
         * @return True inside the open window.
         */
        bool contains(float mx, float my) const;

        /** Draws the inbox. @param engine Renderer. @param ui UI helpers.
         * @param font Fallback FreeType face.
         */
        void draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font);

        /** Releases owned faces. @param engine Renderer owning their textures. */
        void destroy_fonts(engine::IEngine& engine);

      private:
        class ViewHost;
        enum class Sort { Date, Subject, Status };
        void sync_layout() const;
        void sort_entries();
        bool is_unread(const protocol::MailEntry& entry) const;
        const protocol::MailEntry* selected() const;
        UiVarMap variables() const;
        bool action_enabled(std::string_view action) const;
        WindowFrameStyle frame_style() const;
        void prepare_body(const FreetypeFont& font);
        MailClickResult read_selected();
        bool m_open = false;
        const UiLayout* m_layout = nullptr;
        mutable UiDocument m_document;
        mutable float m_w = 760, m_h = 470;
        float m_x = 80, m_y = 100, m_scale = 1;
        bool m_dragging = false;
        float m_drag_dx = 0, m_drag_dy = 0;
        float m_mouse_x = -1, m_mouse_y = -1;
        bool m_mouse_down = false;
        int m_selected = -1;
        std::size_t m_page = 0, m_body_offset = 0;
        bool m_viewing = false;
        Sort m_sort = Sort::Date;
        bool m_sort_ascending = false;
        std::vector<protocol::MailEntry> m_entries;
        std::vector<std::string> m_body_lines;
        std::string m_attachment_name, m_attachment_icon, m_status;
        std::set<std::uint32_t> m_read_ids;
        bool m_read_dirty = false;
        MailClickResult m_confirmation = MailClickResult::Ignored;
        MailClickResult m_pending = MailClickResult::Ignored;
        std::uint32_t m_pending_id = 0, m_last_click_id = 0;
        std::chrono::steady_clock::time_point m_last_click{}, m_last_refresh{}, m_request_time{};
        FreetypeFont m_body_font, m_title_font;
    };
}  // namespace openkal::client
