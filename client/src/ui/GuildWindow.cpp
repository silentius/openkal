/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GuildWindow.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace openkal::client {
    namespace {
        using C = protocol::GuildCommand;
        using J = platform::Json;
        constexpr float kWidth = 900, kHeight = 640, kRankWidth = 780, kRankHeight = 540;
        constexpr std::size_t kAnnouncementLines = 16;
        std::size_t previous_codepoint(std::string_view text, std::size_t cursor) {
            if (cursor) {
                --cursor;
            }
            while (cursor && (static_cast<unsigned char>(text[cursor]) & 0xC0) == 0x80) {
                --cursor;
            }
            return cursor;
        }
        std::size_t next_codepoint(std::string_view text, std::size_t cursor) {
            if (cursor < text.size()) {
                ++cursor;
            }
            while (cursor < text.size() &&
                   (static_cast<unsigned char>(text[cursor]) & 0xC0) == 0x80) {
                ++cursor;
            }
            return cursor;
        }
        void append_label(std::string& label, std::string_view input) {
            for (std::size_t cursor = 0; cursor < input.size();) {
                const auto next = next_codepoint(input, cursor);
                auto candidate = label + std::string(input.substr(cursor, next - cursor));
                if (protocol::guild_label_valid(candidate)) {
                    label = std::move(candidate);
                }
                cursor = next;
            }
        }
        int number(const J& j, std::string_view k) {
            return j.at(k).as_int();
        }
        std::string text(const J& j, std::string_view k) {
            return j.at(k).as_string();
        }
        std::vector<std::string> body_lines(std::string_view input) {
            std::vector<std::string> lines;
            while (!input.empty()) {
                auto end = std::min<std::size_t>(110, input.size());
                while (end > 0 && !protocol::guild_text_valid(input.substr(0, end), 110)) {
                    --end;
                }
                if (!end) {
                    break;
                }
                if (end < input.size()) {
                    const auto space = input.substr(0, end).find_last_of(' ');
                    if (space != input.npos && space > 30) {
                        end = space;
                    }
                }
                lines.emplace_back(input.substr(0, end));
                input.remove_prefix(end);
                while (!input.empty() && input.front() == ' ') {
                    input.remove_prefix(1);
                }
            }
            return lines;
        }
        std::string shortened(std::string value, std::size_t max) {
            if (value.size() <= max) {
                return value;
            }
            value.resize(max);
            while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80) {
                value.pop_back();
            }
            if (!protocol::guild_text_valid(value, max)) {
                while (!value.empty() && !protocol::guild_text_valid(value, max)) {
                    value.pop_back();
                }
            }
            return value + "...";
        }
        float line_width(const FreetypeFont& font, std::string_view value, int pixel_size) {
            return font.ready() ? font.text_width(value,
                                                  static_cast<float>(pixel_size) /
                                                      static_cast<float>(font.pixel_size()))
                                : static_cast<float>(value.size() * pixel_size) * 0.65f;
        }
        std::string
        fit_line(const FreetypeFont& font, std::string value, float width, int pixel_size = 13) {
            if (line_width(font, value, pixel_size) <= width) {
                return value;
            }
            std::size_t end = value.size();
            do {
                --end;
                while (end && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80) {
                    --end;
                }
            } while (end && line_width(font, value.substr(0, end) + "...", pixel_size) > width);
            return value.substr(0, end) + "...";
        }
    }  // namespace
    class GuildWindow::Host final : public UiViewHost {
      public:
        explicit Host(const GuildWindow& window) : m_window(window) {}
        bool button_enabled(std::string_view action) const override {
            return m_window.enabled(action);
        }

      private:
        const GuildWindow& m_window;
    };
    void GuildWindow::set_layout(const UiLayout* layout) {
        m_layout = layout;
        UiView::reload(m_document, m_layout);
    }
    void GuildWindow::set_rank_layout(const UiLayout* layout) {
        m_rank_layout = layout;
        UiView::reload(m_rank_document, m_rank_layout);
    }
    bool GuildWindow::close_rank_dialog() {
        if (!m_rank_open) {
            return false;
        }
        m_rank_open = m_rank_pending = m_focused = false;
        m_rank_input.clear();
        return true;
    }
    void GuildWindow::request_failed() {
        if (m_daily_pending) {
            m_daily_pending = false;
            m_refresh = true;
        }
        if (m_announcement_pending) {
            m_announcement_pending = false;
            m_status = "Could not send the announcement. Your draft has been kept.";
        }
        if (m_rank_open && m_rank_pending) {
            m_rank_pending = false;
            m_rank_status = "Could not send the guild request. Please try again.";
        }
    }
    void GuildWindow::load_rank_draft(int rank) {
        m_rank = std::clamp(rank, 1, 5);
        const auto& guild = m_view.at("guild");
        m_rank_input = guild.at("titles").array().size() == 6
                           ? guild.at("titles").array()[m_rank].as_string()
                           : "Rank " + std::to_string(m_rank);
        m_mask = guild.at("permissions").array().size() == 6
                     ? guild.at("permissions").array()[m_rank].as_int()
                     : 0;
        m_focused = false;
        m_rank_status.clear();
    }
    void GuildWindow::open_rank_dialog(bool edit) {
        UiView::reload(m_rank_document, m_rank_layout);
        if (m_rank_document.widgets().empty()) {
            return;
        }
        m_rank_open = true;
        m_rank_edit = edit;
        m_rank_pending = false;
        m_rank_guild = number(m_view.at("own"), "guild");
        m_rank_target = !edit ? number(m_view.at("rows").array()[m_selected], "id") : 0;
        const auto rank = !edit ? number(m_view.at("rows").array()[m_selected], "rank") : 2;
        load_rank_draft(rank ? rank : 5);
    }
    std::string GuildWindow::rank_unavailable(int rank) const {
        if (number(m_view.at("guild"), "id") != static_cast<int>(m_rank_guild)) {
            return "Refresh guild data before changing ranks.";
        }
        if (m_rank_edit) {
            return {};
        }
        const auto& rows = m_view.at("rows").array();
        const auto target = std::find_if(rows.begin(), rows.end(), [&](const J& row) {
            return number(row, "id") == static_cast<int>(m_rank_target);
        });
        if (target == rows.end()) {
            return "The selected member is no longer on this page. Close and select them again.";
        }
        const int current = number(*target, "rank");
        const int own = number(m_view.at("own"), "rank");
        if (rank == current) {
            return "Current rank";
        }
        if (rank == 1 || current == 1 || !(number(m_view.at("own"), "rights") & 8) ||
            (own != 1 && (rank <= own || current <= own || rank == 2 || current == 2))) {
            return "Protected rank";
        }
        if (current == 2 && rank != 5) {
            return "Demote the sub-leader to OfficialMember first.";
        }
        const auto& options = m_view.at("guild").at("rank_options").array();
        const auto option = std::find_if(options.begin(), options.end(), [&](const J& row) {
            return number(row, "rank") == rank;
        });
        if (option == options.end()) {
            return "Refresh guild data to load rank availability.";
        }
        if (!option->at("unlimited").boolean() &&
            number(*option, "count") >= number(*option, "limit")) {
            return "No free rank slots";
        }
        return {};
    }
    void GuildWindow::set_scale(float scale) {
        m_ui_scale = std::max(0.25f, scale);
    }
    void GuildWindow::set_open(bool value) {
        if (value && !m_open) {
            m_centered = true;
        }
        m_open = value;
        if (!value) {
            close_rank_dialog();
        }
        if (!value) {
            cancel_announcement();
        }
        m_focused = false;
        m_confirm.clear();
        if (value) {
            m_refresh = true;
        }
    }
    void GuildWindow::show_creation() {
        m_tab = number(m_view.at("own"), "guild") ? 2 : 1;
        m_page = 0;
        m_selected = -1;
        m_search.clear();
        m_input.clear();
        m_status.clear();
        set_open(true);
    }
    void GuildWindow::update(const J& view) {
        if (!view.is_object()) {
            return;
        }
        if (m_daily_pending && text(view, "_request_id") == std::to_string(m_daily_request)) {
            m_daily_pending = false;
        }
        if (view.get("status")) {
            m_status = text(view, "status");
            if (number(view, "command") == 0) {
                m_refresh = true;
            }
        }
        // Unsolicited membership snapshots update identity/rules without replacing a paged query.
        if (number(view, "command") == 0 && m_view.is_object()) {
            for (const auto* key : {"revision",
                                    "own",
                                    "rules",
                                    "formation",
                                    "formation_name",
                                    "formation_accepted"}) {
                if (view.get(key)) {
                    m_view.object()[key] = view.at(key);
                } else {
                    m_view.object().erase(key);
                }
            }
            if (number(view.at("guild"), "id") == number(m_view.at("guild"), "id")) {
                for (const auto* key : {"titles", "permissions", "rank_options", "announcement"}) {
                    if (view.at("guild").get(key)) {
                        m_view.object()["guild"].object()[key] = view.at("guild").at(key);
                    }
                }
            }
        } else {
            const auto& old_rows = m_view.at("rows").array();
            const int selected =
                m_selected >= 0 && static_cast<std::size_t>(m_selected) < old_rows.size()
                    ? number(old_rows[m_selected], "id")
                    : 0;
            m_view = view;
            m_selected = -1;
            for (std::size_t i = 0; i < m_view.at("rows").array().size(); ++i) {
                if (number(m_view.at("rows").array()[i], "id") == selected) {
                    m_selected = static_cast<int>(i);
                }
            }
        }
        if (m_announcement_editing) {
            if (number(m_view.at("own"), "guild") != static_cast<int>(m_announcement_guild) ||
                !(number(m_view.at("own"), "rights") & 4)) {
                cancel_announcement();
            } else if (m_announcement_pending && view.get("success") &&
                       text(view, "_request_id") == std::to_string(m_announcement_request)) {
                m_announcement_pending = false;
                if (view.at("success").boolean()) {
                    auto& published = m_view.object()["guild"].object()["announcement"];
                    published.set_type(J::Type::String);
                    published.string() = m_input;
                    cancel_announcement();
                }
            }
        }
        if (m_rank_open) {
            if (number(m_view.at("own"), "guild") != static_cast<int>(m_rank_guild) ||
                number(m_view.at("guild"), "id") != static_cast<int>(m_rank_guild)) {
                close_rank_dialog();
            } else if (m_rank_pending && view.get("success") &&
                       text(view, "_request_id") == std::to_string(m_rank_request_id)) {
                m_rank_pending = false;
                m_rank_status = text(view, "status");
                if (view.at("success").boolean()) {
                    if (!m_rank_edit) {
                        close_rank_dialog();
                    } else if (m_rank_command == C::Title &&
                               m_view.at("guild").at("titles").array().size() == 6) {
                        m_rank_input = m_view.at("guild").at("titles").array()[m_rank].as_string();
                    } else if (m_rank_command == C::Permissions &&
                               m_view.at("guild").at("permissions").array().size() == 6) {
                        m_mask = m_view.at("guild").at("permissions").array()[m_rank].as_int();
                    }
                }
                m_refresh = true;
            }
            if (m_focused && !enabled("guild_ranks.edit_title")) {
                m_focused = false;
            }
        }
        if (number(m_view.at("own"), "guild")) {
            if (m_tab == 1) {
                m_tab = 2;
                m_page = 0;
                m_selected = -1;
                m_input.clear();
                m_focused = false;
                m_refresh = true;
            }
            if (m_detail_parent_tab == 1) {
                m_detail_parent_tab = 0;
            }
        }
        if (!m_focused && m_input.empty() && m_tab == 5) {
            m_input = text(m_view.at("guild"), "invitation");
        }
    }
    std::vector<TextLine> GuildWindow::announcement_lines() const {
        const auto value =
            m_announcement_editing ? m_input : text(m_view.at("guild"), "announcement");
        if (m_font.ready()) {
            return layout_text_lines(
                m_font, value, 876, 13.0f / static_cast<float>(m_font.pixel_size()));
        }
        return layout_text_lines(
            value, 876, [&](std::string_view line) { return line_width(m_font, line, 13); });
    }
    void GuildWindow::cancel_announcement() {
        if (!m_announcement_editing) {
            return;
        }
        m_announcement_editing = m_announcement_pending = m_focused = false;
        m_input.clear();
        m_page = 0;
        m_cursor_direction = 0;
    }
    void GuildWindow::edit_announcement(const engine::FrameInput& in) {
        if (!m_focused || m_announcement_pending) {
            return;
        }
        m_text_cursor = std::min(m_text_cursor, m_input.size());
        bool moved = false;
        if (in.key_backspace_pressed && m_text_cursor) {
            const auto previous = previous_codepoint(m_input, m_text_cursor);
            m_input.erase(previous, m_text_cursor - previous);
            m_text_cursor = previous;
            moved = true;
        }
        if (in.key_delete_pressed && m_text_cursor < m_input.size()) {
            m_input.erase(m_text_cursor, next_codepoint(m_input, m_text_cursor) - m_text_cursor);
            moved = true;
        }
        if (in.key_enter_pressed && m_input.size() < 4096) {
            m_input.insert(m_text_cursor++, 1, '\n');
            moved = true;
        }
        for (int i = 0; i < in.text_char_count && m_input.size() < 4096; ++i) {
            if (static_cast<unsigned char>(in.text_chars[i]) >= 32) {
                m_input.insert(m_text_cursor++, 1, in.text_chars[i]);
                moved = true;
            }
        }
        const int direction = in.key_left    ? -1
                              : in.key_right ? 1
                              : in.key_up    ? -2
                              : in.key_down  ? 2
                                             : 0;
        m_cursor_repeat -= in.dt;
        if (direction && (direction != m_cursor_direction || m_cursor_repeat <= 0)) {
            m_cursor_repeat = direction != m_cursor_direction ? .3f : .045f;
            if (direction == -1) {
                m_text_cursor = previous_codepoint(m_input, m_text_cursor);
            } else if (direction == 1) {
                m_text_cursor = next_codepoint(m_input, m_text_cursor);
            } else {
                const auto lines = announcement_lines();
                std::size_t line = 0;
                while (line + 1 < lines.size() && lines[line + 1].start <= m_text_cursor) {
                    ++line;
                }
                const auto target =
                    direction == -2 ? (line ? line - 1 : 0) : std::min(line + 1, lines.size() - 1);
                const float column =
                    line_width(m_font,
                               std::string_view(m_input).substr(lines[line].start,
                                                                m_text_cursor - lines[line].start),
                               13);
                auto cursor = lines[target].start;
                while (cursor < lines[target].start + lines[target].length &&
                       line_width(m_font,
                                  std::string_view(m_input).substr(lines[target].start,
                                                                   cursor - lines[target].start),
                                  13) < column) {
                    cursor = next_codepoint(m_input, cursor);
                }
                m_text_cursor = cursor;
            }
            moved = true;
        }
        m_cursor_direction = direction;
        if (moved) {
            const auto lines = announcement_lines();
            std::size_t line = 0;
            while (line + 1 < lines.size() && lines[line + 1].start <= m_text_cursor) {
                ++line;
            }
            m_page = static_cast<std::uint32_t>(line / kAnnouncementLines);
        }
    }
    protocol::GuildRequest GuildWindow::query() const {
        protocol::GuildRequest r;
        constexpr C queries[] = {C::Ranking,
                                 C::Search,
                                 C::Members,
                                 C::Applications,
                                 C::MyApplications,
                                 C::Detail,
                                 C::Detail,
                                 C::Members,
                                 C::Members,
                                 C::Detail};
        r.command = queries[std::clamp(m_tab, 0, 9)];
        r.page = m_page;
        if (m_tab == 6) {
            r.value = 1;
        }
        r.guild = number(m_view.at("own"), "guild");
        if (m_tab == 1) {
            r.text = m_search;
        }
        if (m_tab == 9) {
            r.guild = number(m_view.at("guild"), "id");
        }
        return r;
    }
    bool GuildWindow::contains(float x, float y) const {
        if (m_open && m_rank_open) {
            return true;
        }
        return m_open && x >= m_x - 40 * m_scale && y >= m_y - 84 * m_scale &&
               x < m_x + (kWidth + 40) * m_scale && y < m_y + (kHeight + 56) * m_scale;
    }
    bool GuildWindow::enabled(std::string_view action) const {
        const bool member = number(m_view.at("own"), "guild") != 0;
        const int rank = number(m_view.at("own"), "rank"),
                  rights = number(m_view.at("own"), "rights");
        const bool selected = m_selected >= 0 && static_cast<std::size_t>(m_selected) <
                                                     m_view.at("rows").array().size();
        if (action == "guild.change_rank") {
            if (!member || !(rights & 8) || !selected || m_tab != 2 ||
                number(m_view.at("guild"), "id") != number(m_view.at("own"), "guild")) {
                return false;
            }
            const int target_rank = number(m_view.at("rows").array()[m_selected], "rank");
            return target_rank != 1 && (rank == 1 || (target_rank > rank && target_rank != 2));
        }
        if (action == "guild.edit_ranks") {
            return member && m_tab == 2 && (rights & 16 || rank == 1) &&
                   number(m_view.at("guild"), "id") == number(m_view.at("own"), "guild") &&
                   m_view.at("guild").at("titles").array().size() == 6 &&
                   m_view.at("guild").at("permissions").array().size() == 6;
        }
        if (action.rfind("guild_ranks.", 0) == 0) {
            if (action == "guild_ranks.cancel") {
                return true;
            }
            if (!m_rank_open || m_rank_pending || !member) {
                return false;
            }
            if (action == "guild_ranks.confirm") {
                return !m_rank_edit && rank_unavailable(m_rank).empty();
            }
            if (action == "guild_ranks.edit_title") {
                return m_rank_edit && (rights & 16);
            }
            if (action == "guild_ranks.save_title") {
                return m_rank_edit && (rights & 16) && !m_rank_input.empty() &&
                       protocol::guild_label_valid(m_rank_input) &&
                       m_view.at("guild").at("titles").array().size() == 6 &&
                       m_rank_input != m_view.at("guild").at("titles").array()[m_rank].as_string();
            }
            if (action == "guild_ranks.save_permissions" ||
                action.rfind("guild_ranks.option.", 0) == 0) {
                return m_rank_edit && rank == 1 && m_rank >= 2;
            }
            return true;
        }
        if (action == "guild.create") {
            return !member && protocol::guild_label_valid(m_input);
        }
        if (action == "guild.search" || action == "guild.tab.1") {
            return !member;
        }
        if (action == "guild.apply") {
            return !member && number(m_view.at("guild"), "id") != 0;
        }
        if (action == "guild.details") {
            return selected;
        }
        if (action == "guild.withdraw") {
            return !member && selected;
        }
        if (action == "guild.accept" || action == "guild.temporary" || action == "guild.reject") {
            return member && (rights & 1) && selected;
        }
        if (action == "guild.expel") {
            return member && (rights & 2) && selected;
        }
        if (action == "guild.set_rank") {
            return member && (rights & 8) && selected && m_rank >= 2;
        }
        if (action == "guild.set_invitation") {
            return member && (rights & 1);
        }
        if (action == "guild.set_announcement") {
            return member && (rights & 4) && m_tab == 6 && m_announcement_editing &&
                   !m_announcement_pending && protocol::guild_text_valid(m_input, 1024, true);
        }
        if (action == "guild.edit_announcement") {
            return member && (rights & 4) && m_tab == 6 && !m_announcement_editing;
        }
        if (action == "guild.cancel_announcement") {
            return m_announcement_editing && !m_announcement_pending;
        }
        if (action == "guild.set_title") {
            return member && (rights & 16);
        }
        if (action == "guild.set_permissions") {
            return rank == 1 && m_rank >= 2;
        }
        if (action == "guild.transfer") {
            return rank == 1 && selected;
        }
        if (action == "guild.daily") {
            return member && m_tab == 7 && !m_daily_pending &&
                   m_view.at("own").at("daily_reward_available").boolean();
        }
        if (action == "guild.leave") {
            return member && m_tab == 7;
        }
        if (m_tab == 6 && (action == "guild.previous" || action == "guild.next")) {
            return false;
        }
        if (action == "guild.next" && (m_tab == 5 || m_tab == 6 || m_tab == 9)) {
            return (m_page + 1) * 10 <
                   body_lines(text(m_view.at("guild"), m_tab == 6 ? "announcement" : "invitation"))
                       .size();
        }
        if (action == "guild.previous") {
            return m_page > 0;
        }
        if (action == "guild.next") {
            return (m_page + 1) * 10 < static_cast<std::uint32_t>(number(m_view, "total"));
        }
        return true;
    }
    UiVarMap GuildWindow::variables() const {
        UiVarMap v;
        const auto& g = m_view.at("guild");
        const auto& own = m_view.at("own");
        const auto& rules = m_view.at("rules");
        const char* tabs[] = {"Ranking",
                              "Search / Create",
                              "Members",
                              "Applications",
                              "My requests",
                              "Invitation",
                              "Announcement",
                              "Membership",
                              "Rank options",
                              "Guild details"};
        v["guild.title"] = "Guild";
        const bool member = number(own, "guild") != 0;
        v["guild.member"] = member ? "1" : "0";
        v["guild.non_member"] = member ? "0" : "1";
        const int active_tab = m_tab == 9 ? m_detail_parent_tab : m_tab;
        v["guild.tab"] = std::to_string(active_tab);
        v["guild.management_input"] = m_tab == 5 ? "1" : "0";
        v["guild.membership"] = m_tab == 7 && member ? "1" : "0";
        v["guild.membership_hint"] =
            "Claim your daily reward here. Leave guild requires a second click to confirm.";
        v["guild.pagination"] = m_tab == 6 ? "0" : "1";
        v["guild.membership_leader"] = m_tab == 7 && number(own, "rank") == 1 ? "1" : "0";
        v["guild.announcement_editing"] = m_tab == 6 && m_announcement_editing ? "1" : "0";
        v["guild.announcement_can_edit"] = enabled("guild.edit_announcement") ? "1" : "0";
        v["guild.announcement_hint"] = m_announcement_editing
                                           ? "Editing announcement. Enter adds a new line; Save "
                                             "publishes it to all guild members."
                                           : "Announcement — visible to all guild members.";
        v["guild.announcement_text"] = m_announcement_editing ? m_input : text(g, "announcement");
        if (m_announcement_editing && m_focused) {
            v["guild.announcement_text"].insert(std::min(m_text_cursor, m_input.size()), "|");
        }
        if (v["guild.announcement_text"].empty()) {
            v["guild.announcement_text"] = "No announcement has been posted.";
        }
        v["guild.announcement_scroll"] = std::to_string(m_page * kAnnouncementLines);
        v["guild.browsing"] = m_tab == 0 || m_tab == 1 ? "1" : "0";
        for (int i = 0; i < 10; ++i) {
            v["guild.tab." + std::to_string(i)] = tabs[i];
            v["guild.page." + std::to_string(i)] = m_tab == i ? "1" : "0";
            v["guild.tab." + std::to_string(i) + ".active"] = active_tab == i ? "1" : "0";
            v["guild.tab." + std::to_string(i) + ".member_active"] =
                member && active_tab == i ? "1" : "0";
            v["guild.tab." + std::to_string(i) + ".non_member_active"] =
                !member && active_tab == i ? "1" : "0";
        }
        v["guild.summary"] =
            text(g, "name").empty()
                ? "Browse a guild, read its invitation, then apply."
                : text(g, "name") + "  |  Grade " + std::to_string(number(g, "grade")) + "  |  " +
                      std::to_string(number(g, "count")) + " members, " +
                      std::to_string(number(g, "online")) + " online  |  " +
                      std::to_string(number(g, "points")) + " points" +
                      (g.at("penalized").boolean() ? "  [maintenance penalty]" : "");
        v["guild.own"] = "Your guild: " + (text(own, "name").empty()
                                               ? std::string("None")
                                               : text(own, "name") + " / " + text(own, "title"));
        v["guild.input"] = fit_line(m_font, m_input + (m_focused ? "|" : ""), 634);
        if (m_input.empty() && !m_focused) {
            v["guild.input"] = m_tab == 1 ? "Enter a guild name" : "Click here to enter text";
        }
        const auto status_lines = wrap_text(
            m_status, 872, [&](std::string_view value) { return line_width(m_font, value, 13); });
        v["guild.status"] = status_lines.empty() ? "" : status_lines.front();
        v["guild.status_more"] = status_lines.size() < 2 ? "" : status_lines[1];
        if (status_lines.size() > 2) {
            v["guild.status_more"] = fit_line(m_font, status_lines[1] + "...", 872);
        }
        v["guild.requirements"] =
            "Create: level " + std::to_string(number(rules.at("creation"), "minimum_level")) +
            ", " + std::to_string(number(rules.at("creation"), "minimum_members")) + " founders, " +
            std::to_string(number(rules.at("creation"), "fee_zamogeon")) + " Zamogeon" +
            (rules.at("creation").at("require_full_party").boolean() ? ", full party required"
             : rules.at("creation").at("require_party").boolean()    ? ", party required"
                                                                     : "");
        v["guild.fees"] =
            "Apply: " + std::to_string(number(rules.at("membership"), "application_fee_zamogeon")) +
            " / Admit: " +
            std::to_string(number(rules.at("membership"), "admission_fee_zamogeon")) +
            " Zamogeon; rejoin lock " +
            std::to_string(number(rules.at("membership"), "rejoin_lock_seconds")) + " s";
        v["guild.formation"] =
            number(m_view, "formation")
                ? "Founding request: " + text(m_view, "formation_name") +
                      (m_view.at("formation_accepted").boolean() ? " (accepted)"
                                                                 : " — accept or decline")
                : "";
        v["guild.has_formation"] = number(m_view, "formation") ? "1" : "0";
        v["guild.pages"] = std::to_string(m_page + 1) + " / " +
                           std::to_string(std::max(1, (number(m_view, "total") + 9) / 10));
        v["guild.body"] =
            shortened(m_tab == 6 ? text(g, "announcement") : text(g, "invitation"), 120);
        const bool body_page = m_tab == 5 || m_tab == 9;
        const auto paragraphs = body_lines(text(g, m_tab == 6 ? "announcement" : "invitation"));
        if (body_page) {
            v["guild.body"] = m_tab == 6 ? "Announcement" : "Invitation";
            v["guild.pages"] =
                std::to_string(m_page + 1) + " / " +
                std::to_string(std::max<std::size_t>(1, (paragraphs.size() + 9) / 10));
        }
        if (m_tab == 6) {
            v["guild.pages"] =
                std::to_string(m_page + 1) + " / " +
                std::to_string(std::max<std::size_t>(
                    1,
                    (announcement_lines().size() + kAnnouncementLines - 1) / kAnnouncementLines));
        }
        const auto& rows = m_view.at("rows").array();
        const bool member_table = m_tab == 2 || m_tab == 7;
        const bool table_page =
            m_tab == 0 || m_tab == 1 || member_table || m_tab == 3 || m_tab == 4;
        v["guild.body_visible"] = body_page ? "1" : "0";
        v["guild.table.visible"] = table_page ? "1" : "0";
        v["guild.table.members"] = member_table ? "1" : "0";
        v["guild.table.simple"] = table_page && !member_table ? "1" : "0";
        v["guild.table.empty"] = table_page && rows.empty() ? "1" : "0";
        v["guild.table.empty_label"] = "No entries on this page.";
        const char* member_columns[] = {
            "Name", "Rank", "Level", "Daily points", "Status", "Last seen (UTC)"};
        const char* browse_columns[] = {"Guild", "Grade", "Points"};
        const char* application_columns[] = {"Applicant", "Level", "Expires (UTC)"};
        const char* request_columns[] = {"Guild", "", "Expires (UTC)"};
        for (int i = 0; i < (member_table ? 6 : 3); ++i) {
            v["guild.column." + std::to_string(i)] = member_table ? member_columns[i]
                                                     : m_tab == 3 ? application_columns[i]
                                                     : m_tab == 4 ? request_columns[i]
                                                                  : browse_columns[i];
        }
        for (int i = 0; i < 10; ++i) {
            const auto key = "guild.row." + std::to_string(i);
            const bool has_row = i < static_cast<int>(rows.size());
            v[key + ".table_visible"] = table_page && has_row ? "1" : "0";
            v[key + ".members_visible"] = member_table && has_row ? "1" : "0";
            v[key + ".simple_visible"] = table_page && !member_table && has_row ? "1" : "0";
            v[key + ".selected"] = table_page && has_row && m_selected == i ? "1" : "0";
            v[key + ".body_visible"] = "0";
            if (body_page) {
                const auto index =
                    static_cast<std::size_t>(m_page) * 10 + static_cast<std::size_t>(i);
                v[key + ".visible"] = index < paragraphs.size() ? "1" : "0";
                v[key + ".body_visible"] = v[key + ".visible"];
                if (index < paragraphs.size()) {
                    v[key + ".text"] = paragraphs[index];
                }
                v[key + ".color"] = "#FFFFFF";
                continue;
            }
            v[key + ".visible"] = i < static_cast<int>(rows.size()) && m_tab != 8 ? "1" : "0";
            if (i >= static_cast<int>(rows.size())) {
                continue;
            }
            const auto& row = rows[i];
            std::vector<std::string> cells;
            auto timestamp = [&](std::string_view field) {
                const std::time_t time = static_cast<std::time_t>(row.at(field).number());
                char stamp[32]{};
                if (time > 0) {
                    if (const auto* tm = std::gmtime(&time)) {
                        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", tm);
                    }
                }
                return stamp[0] ? std::string(stamp) : std::string("—");
            };
            if (member_table) {
                const int rank = number(row, "rank");
                cells = {text(row, "name"),
                         rank && g.at("titles").array().size() > static_cast<std::size_t>(rank)
                             ? g.at("titles").array()[rank].as_string()
                         : rank ? "Rank " + std::to_string(rank)
                                : "Temporary",
                         std::to_string(number(row, "level")),
                         std::to_string(number(row, "daily")),
                         row.at("online").boolean() ? "Online" : "Offline",
                         timestamp("last_connection")};
            } else if (m_tab == 3) {
                cells = {
                    text(row, "name"), std::to_string(number(row, "level")), timestamp("expires")};
            } else if (m_tab == 4) {
                cells = {text(row, "name"), "", timestamp("expires")};
            } else {
                cells = {text(row, "name"),
                         std::to_string(number(row, "grade")),
                         std::to_string(number(row, "points"))};
            }
            // Match the native column insets in guild.json; long values stay inside their cell.
            constexpr float member_widths[] = {216, 158, 63, 88, 83, 208};
            constexpr float simple_widths[] = {360, 240, 238};
            for (std::size_t column = 0; column < cells.size(); ++column) {
                v[key + ".cell." + std::to_string(column)] =
                    fit_line(m_font,
                             cells[column],
                             member_table ? member_widths[column] : simple_widths[column],
                             12);
            }
            std::ostringstream line;
            line << (m_selected == i ? "> " : "  ") << text(row, "name");
            if (m_tab == 0 || m_tab == 1) {
                line << "    Grade " << number(row, "grade") << "    " << number(row, "points")
                     << " points";
            } else if (m_tab == 2 || m_tab == 7 || m_tab == 8) {
                const int rank = number(row, "rank");
                line << "    "
                     << (rank && g.at("titles").array().size() > static_cast<std::size_t>(rank)
                             ? g.at("titles").array()[rank].as_string()
                         : rank ? "Rank " + std::to_string(rank)
                                : "Temporary")
                     << "    Lv " << number(row, "level") << "    Today " << number(row, "daily")
                     << "    " << (row.at("online").boolean() ? "Online" : "Offline")
                     << "    Last ";
                const std::time_t time =
                    static_cast<std::time_t>(row.at("last_connection").number());
                char stamp[32]{};
                if (const auto* tm = std::gmtime(&time)) {
                    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M UTC", tm);
                }
                line << stamp;
            } else {
                line << "    Lv " << number(row, "level");
            }
            v[key + ".text"] = shortened(line.str(), 126);
            v[key + ".color"] =
                (m_tab == 2 || m_tab == 7 || m_tab == 8) && !row.at("online").boolean() ? "#999999"
                                                                                        : "#FFFFFF";
        }
        const std::pair<const char*, const char*> labels[] = {
            {"refresh", "Refresh"},
            {"search", "Search"},
            {"create", "Create"},
            {"details", "Details"},
            {"apply", "Apply"},
            {"withdraw", "Withdraw"},
            {"accept", "Accept"},
            {"temporary", "Temporary"},
            {"reject", "Reject"},
            {"leave", "Leave guild"},
            {"expel", "Expel"},
            {"change_rank", "Change rank"},
            {"edit_ranks", "Edit ranks"},
            {"rank", "Next rank"},
            {"set_invitation", "Save invitation"},
            {"set_announcement", "Save announcement"},
            {"edit_announcement", "Edit announcement"},
            {"cancel_announcement", "Cancel"},
            {"set_title", "Save title"},
            {"set_permissions", "Save options"},
            {"transfer", "Transfer"},
            {"daily", "Daily reward"},
            {"previous", "<"},
            {"next", ">"},
            {"formation_accept", "Accept founding"},
            {"formation_decline", "Decline"}};
        for (const auto& [key, label] : labels) {
            v["guild." + std::string(key) + "_label"] = label;
        }
        const char* options[] = {
            "Accept / invitation", "Expel", "Announcement", "Change rank", "Rename title"};
        for (int i = 0; i < 5; ++i) {
            v["guild.option." + std::to_string(i)] =
                std::string(m_mask & (1 << i) ? "[x] " : "[ ] ") + options[i];
        }
        for (const auto* key :
             {"guild.own", "guild.summary", "guild.body", "guild.requirements", "guild.fees"}) {
            v[key] = fit_line(m_font, v[key], 884);
        }
        v["guild.formation"] = fit_line(m_font, v["guild.formation"], 614);
        for (int i = 0; i < 10; ++i) {
            const auto key = "guild.row." + std::to_string(i) + ".text";
            if (v.count(key)) {
                v[key] = fit_line(m_font, v[key], 884, 12);
            }
        }
        v["guild_ranks.open"] = m_rank_open ? "1" : "0";
        v["guild_ranks.edit"] = m_rank_edit ? "1" : "0";
        v["guild_ranks.assign"] = m_rank_edit ? "0" : "1";
        v["guild_ranks.title"] = m_rank_edit ? "Edit guild ranks" : "Change rank";
        v["guild_ranks.context"] =
            "Rank names and permissions apply to everyone holding that rank.";
        if (!m_rank_edit) {
            const auto target = std::find_if(rows.begin(), rows.end(), [&](const J& row) {
                return number(row, "id") == static_cast<int>(m_rank_target);
            });
            v["guild_ranks.context"] = target != rows.end()
                                           ? "Member: " + text(*target, "name")
                                           : "The selected member is no longer available.";
        }
        v["guild_ranks.context"] = fit_line(m_font, v["guild_ranks.context"], 668);
        v["guild_ranks.hint"] = m_rank_edit
                                    ? "Edit rank names (1-14 characters, no spaces) or permissions."
                                    : "Choose the new rank, review its permissions, then confirm.";
        v["guild_ranks.selected"] = "Selected: " + fit_line(m_font, m_rank_input, 600);
        v["guild_ranks.availability"] = m_rank_edit ? "" : rank_unavailable(m_rank);
        v["guild_ranks.input"] = fit_line(m_font, m_rank_input + (m_focused ? "|" : ""), 526);
        v["guild_ranks.name_label"] = "Rank title";
        v["guild_ranks.permissions_label"] = "Permissions for this rank";
        v["guild_ranks.save_title"] = "Save title";
        v["guild_ranks.save_permissions"] = "Save permissions";
        v["guild_ranks.confirm"] = "Confirm rank";
        v["guild_ranks.cancel"] = "Close";
        v["guild_ranks.rank_heading"] = "Rank";
        v["guild_ranks.availability_heading"] = "Availability";
        const char* permission_headings[] = {"Recruit", "Expel", "Announce", "Ranks", "Titles"};
        for (int bit = 0; bit < 5; ++bit) {
            v["guild_ranks.heading." + std::to_string(bit)] = permission_headings[bit];
            v["guild_ranks.option." + std::to_string(bit)] =
                std::string(m_mask & (1 << bit) ? "[x] " : "[ ] ") + options[bit];
            v["guild_ranks.option." + std::to_string(bit) + ".label"] = options[bit];
            v["guild_ranks.option." + std::to_string(bit) + ".checked"] =
                (m_mask & (1 << bit)) ? "1" : "0";
            v["guild_ranks.option." + std::to_string(bit) + ".color"] =
                !m_rank_edit || enabled("guild_ranks.option." + std::to_string(bit)) ? "#EAE3D4"
                                                                                     : "#999999";
        }
        for (int rank = 1; rank <= 5; ++rank) {
            const auto key = "guild_ranks.row." + std::to_string(rank);
            const auto& titles = g.at("titles").array();
            const auto& permissions = g.at("permissions").array();
            v[key + ".title"] = fit_line(m_font,
                                         titles.size() == 6 ? titles[rank].as_string()
                                                            : "Rank " + std::to_string(rank),
                                         204,
                                         12);
            v[key + ".selected"] = m_rank == rank ? "1" : "0";
            const auto unavailable = rank_unavailable(rank);
            v[key + ".availability"] = fit_line(m_font,
                                                m_rank_edit           ? ""
                                                : unavailable.empty() ? "Available"
                                                                      : unavailable,
                                                178,
                                                12);
            v[key + ".color"] = !m_rank_edit && !unavailable.empty() ? "#999999" : "#EAE3D4";
            for (int bit = 0; bit < 5; ++bit) {
                v[key + ".permission." + std::to_string(bit)] =
                    permissions.size() == 6 && (permissions[rank].as_int() & (1 << bit)) ? "Yes"
                                                                                         : "—";
            }
        }
        const auto rank_status =
            wrap_text(m_rank_pending ? "Saving..." : m_rank_status,
                      756,
                      [&](std::string_view value) { return line_width(m_font, value, 12); });
        v["guild_ranks.status"] = rank_status.empty() ? "" : rank_status[0];
        v["guild_ranks.status_more"] =
            rank_status.size() < 2 ? "" : fit_line(m_font, rank_status[1], 756, 12);
        return v;
    }
    std::optional<protocol::GuildRequest>
    GuildWindow::handle_rank_dialog(const engine::FrameInput& in, WidgetTools& ui) {
        UiView::reload(m_rank_document, m_rank_layout);
        if (m_focused && enabled("guild_ranks.edit_title")) {
            if (in.key_backspace_pressed && !m_rank_input.empty()) {
                auto end = m_rank_input.size() - 1;
                while (end && (static_cast<unsigned char>(m_rank_input[end]) & 0xC0) == 0x80) {
                    --end;
                }
                m_rank_input.resize(end);
            }
            append_label(m_rank_input, std::string_view(in.text_chars, in.text_char_count));
        }
        if (in.mouse_left_clicked) {
            Host host(*this);
            const auto hit = UiView::hit(m_rank_document,
                                         &ui,
                                         in.mouse_x,
                                         in.mouse_y,
                                         m_x + (kWidth - kRankWidth) * m_scale / 2,
                                         m_y + (kHeight - kRankHeight) * m_scale / 2,
                                         kRankWidth * m_scale,
                                         kRankHeight * m_scale,
                                         m_scale,
                                         variables(),
                                         &host,
                                         true);
            const auto& action = hit.action();
            m_focused = hit.kind() == UiHitKind::Input && enabled("guild_ranks.edit_title");
            if (action == "window.close" || action == "guild_ranks.cancel") {
                close_rank_dialog();
                return {};
            }
            if (enabled(action)) {
                if (action.rfind("guild_ranks.select.", 0) == 0) {
                    load_rank_draft(std::atoi(action.c_str() + 19));
                    return {};
                }
                if (action.rfind("guild_ranks.option.", 0) == 0) {
                    m_mask ^= 1 << std::clamp(std::atoi(action.c_str() + 19), 0, 4);
                    return {};
                }
                if (action == "guild_ranks.confirm" || action == "guild_ranks.save_title" ||
                    action == "guild_ranks.save_permissions") {
                    protocol::GuildRequest request;
                    request.command = action == "guild_ranks.confirm"      ? C::Rank
                                      : action == "guild_ranks.save_title" ? C::Title
                                                                           : C::Permissions;
                    request.guild = m_rank_guild;
                    request.target = request.command == C::Rank ? m_rank_target : m_rank;
                    request.value = request.command == C::Permissions ? m_mask : m_rank;
                    request.text = m_rank_input;
                    request.revision = static_cast<std::uint64_t>(m_view.at("revision").number());
                    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count();
                    m_rank_request_id =
                        std::max(m_rank_request_id + 1, static_cast<std::uint64_t>(stamp));
                    request.request_id = m_rank_request_id;
                    m_rank_pending = true;
                    m_rank_command = request.command;
                    m_focused = false;
                    return request;
                }
            }
        }
        const auto now =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        if (!m_rank_pending && !m_focused && (m_refresh || now - m_last_query_ms >= 3000)) {
            m_refresh = false;
            m_last_query_ms = now;
            return query();
        }
        return {};
    }
    std::optional<protocol::GuildRequest> GuildWindow::handle(const engine::FrameInput& in,
                                                              WidgetTools& ui) {
        if (!m_open) {
            return {};
        }
        UiView::reload(m_document, m_layout);
        m_scale = std::max(
            0.25f,
            std::min(
                {m_ui_scale, (in.width - 16) / (kWidth + 80), (in.height - 16) / (kHeight + 140)}));
        if (m_centered && !m_dragging) {
            m_x = (in.width - (kWidth + 80) * m_scale) * .5f + 40 * m_scale;
            m_y = (in.height - (kHeight + 140) * m_scale) * .5f + 84 * m_scale;
        } else if (!m_dragging) {
            m_x = std::clamp(
                m_x, 40 * m_scale, std::max(40 * m_scale, in.width - (kWidth + 40) * m_scale));
            m_y = std::clamp(
                m_y, 84 * m_scale, std::max(84 * m_scale, in.height - (kHeight + 56) * m_scale));
        }
        m_mouse_x = in.mouse_x;
        m_mouse_y = in.mouse_y;
        m_mouse_down = in.mouse_left;
        if (m_rank_open) {
            return handle_rank_dialog(in, ui);
        }
        if (m_dragging) {
            if (in.mouse_left) {
                m_x = in.mouse_x - m_drag_x;
                m_y = in.mouse_y - m_drag_y;
            } else {
                m_dragging = false;
            }
            return {};
        }
        if (m_tab == 6 && m_announcement_editing) {
            edit_announcement(in);
        } else if (m_focused) {
            if (in.key_backspace_pressed && !m_input.empty()) {
                auto end = m_input.size() - 1;
                while (end > 0 && (static_cast<unsigned char>(m_input[end]) & 0xC0) == 0x80) {
                    --end;
                }
                m_input.resize(end);
            }
            if (m_tab == 1) {
                append_label(m_input, std::string_view(in.text_chars, in.text_char_count));
            } else {
                for (int i = 0; i < in.text_char_count && m_input.size() < 4096; ++i) {
                    if (static_cast<unsigned char>(in.text_chars[i]) >= 32) {
                        m_input += in.text_chars[i];
                    }
                }
            }
        }
        const auto now_ms =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        if (now_ms - m_last_query_ms >= 3000 && !m_focused) {
            m_refresh = true;
        }
        protocol::GuildRequest r = query();
        if (m_tab == 6) {
            m_page = std::min(
                m_page,
                static_cast<std::uint32_t>((announcement_lines().size() - 1) / kAnnouncementLines));
        }
        const auto* announcement_field = m_document.find_widget("announcement_text");
        const float announcement_x = announcement_field ? announcement_field->x() : 0;
        if (m_tab == 6 && in.mouse_scroll && in.mouse_x >= m_x + announcement_x * m_scale &&
            in.mouse_x < m_x + (announcement_x + 876) * m_scale &&
            in.mouse_y >= m_y + 172 * m_scale && in.mouse_y < m_y + 516 * m_scale) {
            if (in.mouse_scroll > 0 && m_page) {
                --m_page;
            } else if (in.mouse_scroll < 0 &&
                       (m_page + 1) * kAnnouncementLines < announcement_lines().size()) {
                ++m_page;
            }
        }
        if (in.mouse_left_clicked) {
            Host host(*this);
            const auto hit = UiView::hit(m_document,
                                         &ui,
                                         in.mouse_x,
                                         in.mouse_y,
                                         m_x,
                                         m_y,
                                         kWidth * m_scale,
                                         kHeight * m_scale,
                                         m_scale,
                                         variables(),
                                         &host,
                                         true);
            const auto a = hit.kind() == UiHitKind::Tab && hit.bind() == "guild.tab"
                               ? "guild.tab." + hit.tab_id()
                               : hit.action();
            if (hit.kind() == UiHitKind::TitleDrag) {
                m_centered = false;
                m_dragging = true;
                m_drag_x = in.mouse_x - m_x;
                m_drag_y = in.mouse_y - m_y;
                return {};
            }
            if (a == "window.close") {
                set_open(false);
                return {};
            }
            if (hit.kind() == UiHitKind::Input) {
                if (hit.bind() == "guild.announcement_text") {
                    m_focused = m_announcement_editing && !m_announcement_pending;
                    if (m_focused) {
                        const auto lines = announcement_lines();
                        const auto line = std::min(
                            lines.size() - 1,
                            m_page * kAnnouncementLines +
                                static_cast<std::size_t>(
                                    std::max(0.0f, (in.mouse_y - m_y) / m_scale - 184) / 20));
                        m_text_cursor = lines[line].start;
                        const float column =
                            std::max(0.0f, (in.mouse_x - m_x) / m_scale - announcement_x);
                        while (m_text_cursor < lines[line].start + lines[line].length &&
                               line_width(m_font,
                                          std::string_view(m_input).substr(
                                              lines[line].start, m_text_cursor - lines[line].start),
                                          13) < column) {
                            m_text_cursor = next_codepoint(m_input, m_text_cursor);
                        }
                    }
                    return {};
                }
                m_focused = true;
                return {};
            }
            m_focused = false;
            if (a == "guild.edit_announcement" && enabled(a)) {
                m_announcement_editing = m_focused = true;
                m_announcement_guild = number(m_view.at("own"), "guild");
                m_input = text(m_view.at("guild"), "announcement");
                m_text_cursor = m_input.size();
                m_page = static_cast<std::uint32_t>((announcement_lines().size() - 1) /
                                                    kAnnouncementLines);
                return {};
            } else if (a == "guild.cancel_announcement" && enabled(a)) {
                cancel_announcement();
                return {};
            } else if ((a == "guild.change_rank" || a == "guild.edit_ranks") && enabled(a)) {
                open_rank_dialog(a == "guild.edit_ranks");
                return {};
            } else if (a.rfind("guild.tab.", 0) == 0) {
                const int tab = std::clamp(std::atoi(a.c_str() + 10), 0, 9);
                if (tab == 1 && !enabled("guild.tab.1")) {
                    return {};
                }
                if (tab == 8) {
                    return {};
                }
                if (tab == m_tab) {
                    return {};
                }
                cancel_announcement();
                m_tab = tab;
                m_page = 0;
                m_selected = -1;
                m_confirm.clear();
                m_refresh = true;
                if (m_tab == 5) {
                    m_input = text(m_view.at("guild"), "invitation");
                } else {
                    m_input.clear();
                }
            } else if (a.rfind("guild.select.", 0) == 0) {
                m_selected = std::atoi(a.c_str() + 13);
                m_confirm.clear();
                return {};
            } else if (enabled(a)) {
                if (a == "guild.previous") {
                    --m_page;
                    m_refresh = true;
                } else if (a == "guild.next") {
                    ++m_page;
                    m_refresh = true;
                } else if (a == "guild.refresh") {
                    m_refresh = true;
                } else if (a == "guild.search") {
                    m_search = m_input;
                    m_tab = 1;
                    m_page = 0;
                    m_refresh = true;
                } else {
                    const auto& rows = m_view.at("rows").array();
                    const J* selected =
                        m_selected >= 0 && static_cast<std::size_t>(m_selected) < rows.size()
                            ? &rows[m_selected]
                            : nullptr;
                    r.guild = number(m_view.at("own"), "guild");
                    r.target = selected ? number(*selected, "id") : 0;
                    r.text = m_input;
                    r.value = m_rank;
                    if (a == "guild.create") {
                        r.command = C::Create;
                        r.expected_fee = static_cast<std::uint64_t>(
                            m_view.at("rules").at("creation").at("fee_zamogeon").number());
                    } else if (a == "guild.details") {
                        if (!selected) {
                            return {};
                        }
                        r.command = C::Detail;
                        r.guild = number(*selected, "id");
                        m_detail_parent_tab = m_tab;
                        m_tab = 9;
                    } else if (a == "guild.apply") {
                        r.command = C::Apply;
                        r.guild = number(m_view.at("guild"), "id");
                        r.expected_fee =
                            static_cast<std::uint64_t>(m_view.at("rules")
                                                           .at("membership")
                                                           .at("application_fee_zamogeon")
                                                           .number());
                    } else if (a == "guild.withdraw") {
                        r.command = C::Withdraw;
                        r.guild = selected ? number(*selected, "guild") : 0;
                    } else if (a == "guild.accept" || a == "guild.temporary") {
                        r.command = C::Accept;
                        r.value = a == "guild.temporary" ? 1 : 0;
                        r.expected_fee =
                            static_cast<std::uint64_t>(m_view.at("rules")
                                                           .at("membership")
                                                           .at("admission_fee_zamogeon")
                                                           .number());
                    } else if (a == "guild.reject") {
                        r.command = C::Reject;
                    } else if (a == "guild.leave") {
                        r.command = C::Leave;
                    } else if (a == "guild.expel") {
                        r.command = C::Expel;
                    } else if (a == "guild.set_rank") {
                        r.command = C::Rank;
                    } else if (a == "guild.transfer") {
                        r.command = C::Transfer;
                    } else if (a == "guild.set_invitation") {
                        r.command = C::Invitation;
                    } else if (a == "guild.set_announcement") {
                        r.command = C::Announcement;
                        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();
                        m_announcement_request =
                            std::max(m_announcement_request + 1, static_cast<std::uint64_t>(stamp));
                        r.request_id = m_announcement_request;
                        m_announcement_pending = true;
                    } else if (a == "guild.set_title") {
                        r.command = C::Title;
                        r.target = m_rank;
                    } else if (a == "guild.set_permissions") {
                        r.command = C::Permissions;
                        r.target = m_rank;
                        r.value = m_mask;
                    } else if (a == "guild.daily") {
                        r.command = C::DailyReward;
                        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();
                        m_daily_request =
                            std::max(m_daily_request + 1, static_cast<std::uint64_t>(stamp));
                        r.request_id = m_daily_request;
                        m_daily_pending = true;
                    } else if (a == "guild.formation_accept" || a == "guild.formation_decline") {
                        r.command = C::FormationReply;
                        r.target = number(m_view, "formation");
                        r.value = a == "guild.formation_accept" ? 1 : 0;
                    } else {
                        return {};
                    }
                    if (a == "guild.leave" || a == "guild.expel" || a == "guild.transfer") {
                        if (m_confirm != a) {
                            m_confirm = a;
                            m_status = "Click the same action again to confirm.";
                            return {};
                        }
                        m_confirm.clear();
                    }
                    r.revision = static_cast<std::uint64_t>(m_view.at("revision").number());
                    return r;
                }
            }
        }
        if (m_refresh) {
            m_refresh = false;
            m_last_query_ms = now_ms;
            return query();
        }
        return {};
    }
    void GuildWindow::draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& fallback) {
        if (!m_open) {
            return;
        }
        UiView::reload(m_document, m_layout);
        if (!m_font.ready()) {
            (void) m_font.load(engine, FreetypeFont::find_system_sans(), 13);
        }
        if (!m_title.ready()) {
            (void) m_title.load(engine, FreetypeFont::find_system_sans_bold(), 18);
        }
        Host host(*this);
        const auto vars = variables();
        const auto hit = UiView::hit(m_document,
                                     &ui,
                                     m_mouse_x,
                                     m_mouse_y,
                                     m_x,
                                     m_y,
                                     kWidth * m_scale,
                                     kHeight * m_scale,
                                     m_scale,
                                     vars,
                                     &host,
                                     true);
        host.set_pointer_hit(hit.widget_id(), m_mouse_down);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     kWidth * m_scale,
                     kHeight * m_scale,
                     m_scale,
                     vars,
                     m_font.ready() ? m_font : fallback,
                     m_title,
                     &host);
        if (m_rank_open) {
            const float x = m_x + (kWidth - kRankWidth) * m_scale / 2;
            const float y = m_y + (kHeight - kRankHeight) * m_scale / 2;
            const auto rank_hit = UiView::hit(m_rank_document,
                                              &ui,
                                              m_mouse_x,
                                              m_mouse_y,
                                              x,
                                              y,
                                              kRankWidth * m_scale,
                                              kRankHeight * m_scale,
                                              m_scale,
                                              vars,
                                              &host,
                                              true);
            host.set_pointer_hit(rank_hit.widget_id(), m_mouse_down);
            UiView::draw(engine,
                         ui,
                         m_rank_document,
                         x,
                         y,
                         kRankWidth * m_scale,
                         kRankHeight * m_scale,
                         m_scale,
                         vars,
                         m_font,
                         m_title,
                         &host);
        }
    }
    void GuildWindow::destroy(engine::IEngine& engine) {
        m_font.destroy(engine);
        m_title.destroy(engine);
    }
}  // namespace openkal::client
