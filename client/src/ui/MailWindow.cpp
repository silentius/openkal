/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MailWindow.h"

#include "openkal/client/TextWrap.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace openkal::client {
    namespace {
        constexpr std::size_t kPageSize = 8;
        constexpr std::size_t kBodyRows = 9;

        std::string date_text(std::uint64_t timestamp) {
            const auto time = static_cast<std::time_t>(timestamp);
            std::tm local{};
            if (timestamp == 0 || !localtime_r(&time, &local)) {
                return "-";
            }
            std::ostringstream out;
            out << std::put_time(&local, "%Y-%m-%d %H:%M");
            return out.str();
        }

        std::string fit_text(const FreetypeFont& font, std::string text, float width) {
            if (!font.ready() || font.text_width(text) <= width) {
                return text;
            }
            const std::string suffix = "...";
            while (!text.empty() && font.text_width(text + suffix) > width) {
                auto start = text.size() - 1;
                while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
                    --start;
                }
                text.erase(start);
            }
            return text + suffix;
        }
    }  // namespace

    class MailWindow::ViewHost final : public UiViewHost {
      public:
        explicit ViewHost(const MailWindow& window) : m_window(window) {}
        bool button_enabled(std::string_view action) const override {
            return m_window.action_enabled(action);
        }
        bool resolve_slot(std::string_view bind, int, UiSlotIcon& out) const override {
            if (bind != "mail.attachment") {
                return false;
            }
            const auto* entry = m_window.selected();
            if (entry && m_window.m_viewing) {
                out.set_file(m_window.m_attachment_icon);
                out.set_count(static_cast<int>(entry->item_count));
                out.set_empty(entry->item_index == 0 || m_window.m_attachment_icon.empty());
                out.set_grayscale(entry->claimed);
            }
            return true;
        }

      private:
        const MailWindow& m_window;
    };

    void MailWindow::set_layout(const UiLayout* layout) {
        m_layout = layout;
        sync_layout();
    }
    void MailWindow::sync_layout() const {
        UiView::reload(m_document, m_layout);
        if (m_document.window_w() > 0 && m_document.window_h() > 0) {
            m_w = m_document.window_w();
            m_h = m_document.window_h();
        }
    }
    WindowFrameStyle MailWindow::frame_style() const {
        return UiView::frame_style(m_document);
    }
    void MailWindow::set_open(bool open) {
        m_open = open;
        m_confirmation = MailClickResult::Ignored;
        m_last_click_id = 0;
        if (!open) {
            m_dragging = false;
        }
    }
    void MailWindow::toggle() {
        set_open(!m_open);
    }
    void MailWindow::center_on_screen(float screen_w, float screen_h) {
        sync_layout();
        const float s = std::max(0.25f, m_scale);
        m_x = std::max(40 * s, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(84 * s, (screen_h - m_h * s) * 0.4f);
    }
    const protocol::MailEntry* MailWindow::selected() const {
        return m_selected >= 0 && m_selected < static_cast<int>(m_entries.size())
                   ? &m_entries[m_selected]
                   : nullptr;
    }
    std::uint32_t MailWindow::selected_id() const {
        const auto* e = selected();
        return e ? e->id : 0;
    }
    std::uint16_t MailWindow::selected_item_index() const {
        const auto* e = selected();
        return e ? e->item_index : 0;
    }
    bool MailWindow::is_unread(const protocol::MailEntry& entry) const {
        return !entry.read && !entry.claimed && m_read_ids.count(entry.id) == 0;
    }
    std::size_t MailWindow::unread_count() const {
        return static_cast<std::size_t>(std::count_if(
            m_entries.begin(), m_entries.end(), [this](const auto& e) { return is_unread(e); }));
    }
    void MailWindow::set_read_ids(std::set<std::uint32_t> ids) {
        m_read_ids = std::move(ids);
        m_read_dirty = false;
    }
    bool MailWindow::take_read_dirty() {
        const bool dirty = m_read_dirty;
        m_read_dirty = false;
        return dirty;
    }
    void MailWindow::sort_entries() {
        const auto id = selected_id();
        std::stable_sort(m_entries.begin(), m_entries.end(), [this](const auto& a, const auto& b) {
            if (m_sort == Sort::Subject && a.subject != b.subject) {
                return m_sort_ascending ? a.subject < b.subject : a.subject > b.subject;
            }
            if (m_sort == Sort::Status && is_unread(a) != is_unread(b)) {
                return m_sort_ascending ? is_unread(a) : is_unread(b);
            }
            if (a.created_at != b.created_at) {
                return m_sort_ascending ? a.created_at < b.created_at : a.created_at > b.created_at;
            }
            return m_sort_ascending ? a.id < b.id : a.id > b.id;
        });
        m_selected = -1;
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == id) {
                m_selected = static_cast<int>(i);
            }
        }
        if (m_selected < 0 && !m_entries.empty()) {
            m_selected = 0;
        }
        m_page = m_selected < 0 ? 0 : static_cast<std::size_t>(m_selected) / kPageSize;
    }
    void MailWindow::set_entries(std::vector<protocol::MailEntry> entries) {
        const auto id = selected_id();
        if (m_pending == MailClickResult::Delete) {
            const bool remains = std::any_of(entries.begin(), entries.end(), [this](const auto& e) {
                return e.id == m_pending_id;
            });
            m_status = remains ? "Message could not be deleted." : "Message deleted.";
        } else if (m_pending == MailClickResult::Refresh) {
            m_status = "Inbox updated.";
        }
        if (m_pending != MailClickResult::Claim) {
            m_pending = MailClickResult::Ignored;
        }
        m_entries = std::move(entries);
        m_selected = -1;
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == id) {
                m_selected = static_cast<int>(i);
            }
        }
        if (m_selected < 0) {
            m_viewing = false;
            m_body_offset = 0;
            m_confirmation = MailClickResult::Ignored;
        }
        sort_entries();
    }
    void MailWindow::set_attachment_info(std::string name, std::string icon) {
        m_attachment_name = std::move(name);
        m_attachment_icon = std::move(icon);
    }
    bool MailWindow::refresh_ready() const {
        return action_enabled("mail.refresh");
    }

    void MailWindow::request_sent(MailClickResult action, bool sent) {
        if (!sent) {
            m_pending = MailClickResult::Ignored;
            m_status = "Not connected. Please try again.";
            return;
        }
        m_pending = action;
        m_pending_id = selected_id();
        m_request_time = std::chrono::steady_clock::now();
        m_status = "Updating inbox...";
        if (action == MailClickResult::Refresh) {
            m_last_refresh = m_request_time;
        }
    }
    void MailWindow::claim_result(const protocol::MailClaimResult& result) {
        if (m_pending != MailClickResult::Claim || result.id != m_pending_id) {
            return;
        }
        m_pending = MailClickResult::Ignored;
        m_status = result.claimed ? "Attachment received."
                                  : "Could not receive attachment. Check your inventory space.";
    }
    bool MailWindow::action_enabled(std::string_view action) const {
        const auto* entry = selected();
        if (action == "window.close" || action == "mail.cancel") {
            return true;
        }
        if (m_pending != MailClickResult::Ignored) {
            return false;
        }
        if (action == "mail.read") {
            return entry != nullptr;
        }
        if (action == "mail.claim") {
            return entry && m_viewing && entry->item_index != 0 && !entry->claimed;
        }
        if (action == "mail.delete") {
            return entry && m_viewing && (entry->item_index == 0 || entry->claimed);
        }
        if (action == "mail.previous") {
            return m_page > 0;
        }
        if (action == "mail.next") {
            return (m_page + 1) * kPageSize < m_entries.size();
        }
        if (action == "mail.body_up") {
            return m_body_offset > 0;
        }
        if (action == "mail.body_down") {
            return m_body_offset + kBodyRows < m_body_lines.size();
        }
        if (action == "mail.refresh") {
            return std::chrono::steady_clock::now() - m_last_refresh >= std::chrono::seconds(3);
        }
        return true;
    }
    UiVarMap MailWindow::variables() const {
        UiVarMap vars{{"mail.title", "Messages"},
                      {"mail.inbox_label", "Inbox"},
                      {"mail.details_label", "Message"},
                      {"mail.summary",
                       std::to_string(m_entries.size()) + " messages / " +
                           std::to_string(unread_count()) + " unread"},
                      {"mail.date_label", "Date"},
                      {"mail.subject_label", "Subject"},
                      {"mail.status_label", "Status"},
                      {"mail.read_label", "Read"},
                      {"mail.refresh_label", "Refresh"},
                      {"mail.claim_label", "Receive attachment"},
                      {"mail.delete_label", "Delete"},
                      {"mail.previous_label", "<"},
                      {"mail.next_label", ">"},
                      {"mail.body_up_label", "Up"},
                      {"mail.body_down_label", "Down"},
                      {"mail.confirm_label", "Confirm"},
                      {"mail.cancel_label", "Cancel"},
                      {"mail.empty_label", "Your inbox is empty."},
                      {"mail.empty", m_entries.empty() ? "1" : "0"},
                      {"mail.viewing", m_viewing && selected() ? "1" : "0"},
                      {"mail.select_hint",
                       m_entries.empty() ? "No messages to read."
                                         : "Select a message, then Read or double-click."},
                      {"mail.not_viewing", !m_viewing ? "1" : "0"},
                      {"mail.status", m_status},
                      {"mail.page",
                       std::to_string(m_page + 1) + " / " +
                           std::to_string(std::max<std::size_t>(
                               1, (m_entries.size() + kPageSize - 1) / kPageSize))},
                      {"mail.confirming", m_confirmation != MailClickResult::Ignored ? "1" : "0"},
                      {"mail.confirmation",
                       m_confirmation == MailClickResult::Delete ? "Delete this message?"
                                                                 : "Receive this attachment?"}};
        ui_fill_theme_vars(m_document, vars);
        for (std::size_t row = 0; row < kPageSize; ++row) {
            const auto i = m_page * kPageSize + row;
            const auto key = "mail.row." + std::to_string(row) + ".";
            vars[key + "visible"] = i < m_entries.size() ? "1" : "0";
            vars[key + "selected"] = static_cast<int>(i) == m_selected ? "1" : "0";
            vars[key + "has_attachment"] = "0";
            if (i >= m_entries.size()) {
                continue;
            }
            const auto& e = m_entries[i];
            vars[key + "has_attachment"] = e.item_index != 0 && e.item_count != 0 ? "1" : "0";
            vars[key + "subject"] = fit_text(m_body_font, e.subject, 256);
            vars[key + "date"] = date_text(e.created_at);
            vars[key + "status"] = is_unread(e) ? "Unread" : (e.claimed ? "Claimed" : "Read");
            vars[key + "color"] = is_unread(e) ? "#E5CF9E" : "#BDAE92";
        }
        if (const auto* e = selected(); e && m_viewing) {
            vars["mail.subject"] = fit_text(m_body_font, e->subject, 394);
            vars["mail.sender"] = "From: System";
            vars["mail.date"] = "Sent: " + date_text(e->created_at);
            vars["mail.attachment_name"] =
                fit_text(m_body_font,
                         m_attachment_name.empty() ? "Item #" + std::to_string(e->item_index)
                                                   : m_attachment_name,
                         328);
            vars["mail.attachment_count"] = "Quantity: " + std::to_string(e->item_count);
            vars["mail.attachment_status"] =
                e->claimed ? "Attachment received" : "Attachment available";
            vars["mail.delete_hint"] = e->claimed ? "" : "Receive the attachment before deleting.";
            if (e->item_index == 0) {
                vars["mail.attachment_name"] = "No attachment";
                vars["mail.attachment_count"] = "";
                vars["mail.attachment_status"] = "";
                vars["mail.delete_hint"] = "";
            }
            vars["mail.body_page"] =
                m_body_lines.size() > kBodyRows
                    ? std::to_string(m_body_offset + 1) + "-" +
                          std::to_string(std::min(m_body_lines.size(), m_body_offset + kBodyRows)) +
                          " / " + std::to_string(m_body_lines.size())
                    : "";
        }
        for (std::size_t i = 0; i < kBodyRows; ++i) {
            vars["mail.body." + std::to_string(i)] =
                m_body_offset + i < m_body_lines.size() ? m_body_lines[m_body_offset + i] : "";
        }
        return vars;
    }
    bool MailWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        sync_layout();
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w * s, m_h * s, s, frame_style());
    }
    void MailWindow::update_pointer(const engine::FrameInput& input) {
        m_mouse_x = input.mouse_x;
        m_mouse_y = input.mouse_y;
        m_mouse_down = input.mouse_left;
        if (m_pending != MailClickResult::Ignored &&
            std::chrono::steady_clock::now() - m_request_time > std::chrono::seconds(10)) {
            m_pending = MailClickResult::Ignored;
            m_status = "Request timed out. Refresh the inbox to try again.";
        }
        if (!m_open || !input.mouse_left) {
            m_dragging = false;
        }
        if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
        }
        if (!m_open || m_confirmation != MailClickResult::Ignored ||
            m_pending != MailClickResult::Ignored || input.mouse_scroll == 0 ||
            !contains(input.mouse_x, input.mouse_y)) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        if (input.mouse_x < m_x + 326 * s) {
            if (input.mouse_scroll > 0 && m_page > 0) {
                --m_page;
            }
            if (input.mouse_scroll < 0 && (m_page + 1) * kPageSize < m_entries.size()) {
                ++m_page;
            }
        } else {
            if (input.mouse_scroll > 0 && m_body_offset > 0) {
                --m_body_offset;
            }
            if (input.mouse_scroll < 0 && m_body_offset + kBodyRows < m_body_lines.size()) {
                ++m_body_offset;
            }
        }
    }
    MailClickResult MailWindow::read_selected() {
        if (!selected()) {
            return MailClickResult::Handled;
        }
        m_viewing = true;
        m_body_offset = 0;
        m_status.clear();
        return MailClickResult::Read;
    }
    MailClickResult MailWindow::handle_click(float mx, float my) {
        if (!contains(mx, my)) {
            return MailClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const auto hit =
            UiView::hit(m_document, nullptr, mx, my, m_x, m_y, m_w * s, m_h * s, s, variables());
        const auto& action = hit.action();
        if (action == "window.close") {
            set_open(false);
            return MailClickResult::Close;
        }
        if (m_confirmation != MailClickResult::Ignored) {
            if (action == "mail.cancel") {
                m_confirmation = MailClickResult::Ignored;
            }
            if (action == "mail.confirm") {
                const auto result = m_confirmation;
                m_confirmation = MailClickResult::Ignored;
                return result;
            }
            return MailClickResult::Handled;
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return MailClickResult::Handled;
        }
        if (!action_enabled(action)) {
            return MailClickResult::Handled;
        }
        constexpr std::string_view prefix = "mail.select.";
        if (action.compare(0, prefix.size(), prefix) == 0) {
            const auto row = static_cast<std::size_t>(action.back() - '0');
            const auto index = m_page * kPageSize + row;
            if (row >= kPageSize || index >= m_entries.size()) {
                return MailClickResult::Handled;
            }
            const auto now = std::chrono::steady_clock::now();
            const auto id = m_entries[index].id;
            const bool double_click =
                id == m_last_click_id && now - m_last_click < std::chrono::milliseconds(400);
            if (static_cast<int>(index) != m_selected) {
                m_viewing = false;
                m_body_offset = 0;
            }
            m_selected = static_cast<int>(index);
            m_last_click_id = id;
            m_last_click = now;
            return double_click ? read_selected() : MailClickResult::Handled;
        }
        m_last_click_id = 0;
        if (action == "mail.read") {
            return read_selected();
        }
        if (action == "mail.refresh") {
            return MailClickResult::Refresh;
        }
        if (action == "mail.claim") {
            m_confirmation = MailClickResult::Claim;
        } else if (action == "mail.delete") {
            m_confirmation = MailClickResult::Delete;
        } else if (action == "mail.previous") {
            --m_page;
        } else if (action == "mail.next") {
            ++m_page;
        } else if (action == "mail.body_up") {
            --m_body_offset;
        } else if (action == "mail.body_down") {
            ++m_body_offset;
        } else if (action == "mail.sort.date" || action == "mail.sort.subject" ||
                   action == "mail.sort.status") {
            const auto sort = action == "mail.sort.date"
                                  ? Sort::Date
                                  : (action == "mail.sort.subject" ? Sort::Subject : Sort::Status);
            m_sort_ascending = sort == m_sort ? !m_sort_ascending : sort != Sort::Date;
            m_sort = sort;
            sort_entries();
        }
        return MailClickResult::Handled;
    }
    void MailWindow::prepare_body(const FreetypeFont& font) {
        const auto* e = selected();
        m_body_lines =
            e && m_viewing ? wrap_text(font, e->body, 394, 1) : std::vector<std::string>{};
        m_body_offset = std::min(
            m_body_offset, m_body_lines.size() > kBodyRows ? m_body_lines.size() - kBodyRows : 0);
    }
    void MailWindow::draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font) {
        if (!m_open) {
            return;
        }
        sync_layout();
        if (!m_body_font.ready()) {
            (void) m_body_font.load(engine, FreetypeFont::find_system_sans(), 13);
        }
        if (!m_title_font.ready()) {
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), 18);
        }
        const auto& body = m_body_font.ready() ? m_body_font : font;
        prepare_body(body);
        const float s = std::max(0.25f, m_scale);
        if (m_viewing && selected() && body.ready() && m_read_ids.insert(selected_id()).second) {
            m_read_dirty = true;
        }
        const auto vars = variables();
        ViewHost host(*this);
        const auto hit =
            UiView::hit(m_document, &ui, m_mouse_x, m_mouse_y, m_x, m_y, m_w * s, m_h * s, s, vars);
        host.set_pointer_hit(hit.widget_id(), m_mouse_down);
        UiView::draw(
            engine, ui, m_document, m_x, m_y, m_w * s, m_h * s, s, vars, body, m_title_font, &host);
    }
    void MailWindow::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_title_font.destroy(engine);
    }
}  // namespace openkal::client
