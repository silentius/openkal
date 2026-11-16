/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/NpcTalkWindow.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiDocument.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace openkal::client {
    namespace {
        constexpr float kBodyPad = 16.0f;
        constexpr float kBodyLineStep = 18.0f;
        constexpr float kLinkHeight = 40.0f;
        constexpr float kLinkGap = 6.0f;
        constexpr float kNavigationHeight = 44.0f;
        constexpr float kResponsePadding = 8.0f;
        constexpr float kLinkLineStep = 18.0f;
        constexpr float kResponseInset = 20.0f;
        constexpr float kSelectWidth = 76.0f;
        constexpr float kSelectHeight = 28.0f;
        constexpr float kSelectGap = 12.0f;
        constexpr std::string_view kSelectLabel = "Select";
        constexpr float kDefaultWidth = 450.0f;
        constexpr float kDefaultHeight = 360.0f;

        [[nodiscard]] bool parse_dialog_link_id(std::string_view text, std::int32_t& out_id) {
            if (text.empty()) {
                return false;
            }
            int value = 0;
            const char* first = text.data();
            const char* last = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(first, last, value);
            if (ec != std::errc() || ptr != last || value <= 0) {
                return false;
            }
            out_id = value;
            return true;
        }

        [[nodiscard]] bool parse_revival_point(std::string_view type, std::uint8_t& out_point) {
            constexpr std::string_view kPrefix = "npc.save_revival:";
            if (type.substr(0, kPrefix.size()) != kPrefix) {
                return false;
            }
            unsigned int value = 0;
            const char* first = type.data() + kPrefix.size();
            const char* last = type.data() + type.size();
            const auto [ptr, ec] = std::from_chars(first, last, value);
            if (ec != std::errc() || ptr != last || value > 255) {
                return false;
            }
            out_point = static_cast<std::uint8_t>(value);
            return true;
        }
    }  // namespace

    WindowFrameStyle NpcTalkWindow::frame_style() const {
        UiView::reload(m_document, m_layout);
        return UiView::frame_style(m_document);
    }

    UiVarMap NpcTalkWindow::variables() const {
        UiVarMap vars{{"npc.name", m_caption}, {"npc.closable", m_closable ? "true" : "false"}};
        vars["npc.paged"] = m_pages.size() > 1 ? "true" : "false";
        vars["npc.page"] = std::to_string(m_page + 1) + " / " + std::to_string(m_pages.size());
        vars["npc.previous_label"] = "Previous";
        vars["npc.next_label"] = "Next";
        ui_fill_theme_vars(m_document, vars);
        return vars;
    }

    void NpcTalkWindow::center_on_screen(float screen_w, float screen_h) {
        const float s = std::max(0.25f, m_scale);
        const WindowFrameStyle style = frame_style();
        m_x = std::max(8.0f + style.pad_left() * s, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f + style.pad_top() * s, (screen_h - m_h * s) * 0.35f);
    }

    void NpcTalkWindow::show_dialog(const DialogDef& dialog,
                                    std::uint32_t npc_id,
                                    std::string_view player_name) {
        const UiVarMap vars{
            {"player.name", player_name.empty() ? "Player" : std::string(player_name)}};
        m_caption = dialog.caption.empty() ? "Talk" : ui_interpolate(dialog.caption, vars);
        UiView::reload(m_document, m_layout);
        m_w = m_document.window_w() > 0 ? m_document.window_w() : kDefaultWidth;
        m_h = m_document.window_h() > 0 ? m_document.window_h() : kDefaultHeight;
        if (dialog.width > 0.0f) {
            m_w = dialog.width;
        }
        if (dialog.height > 0.0f) {
            m_h = dialog.height;
        }
        m_lines = dialog.lines;
        m_links = dialog.links;
        for (auto& line : m_lines) {
            line = ui_interpolate(line, vars);
        }
        for (auto& link : m_links) {
            link.text = ui_interpolate(link.text, vars);
        }
        m_npc_id = npc_id;
        m_pages.clear();
        m_page = 0;
        m_closable = true;
        m_open = true;
    }

    bool NpcTalkWindow::close_if_npc_unavailable(bool available) {
        if (!m_open || m_npc_id == 0 || available) {
            return false;
        }
        m_open = false;
        m_dragging = false;
        return true;
    }

    void NpcTalkWindow::set_caption(std::string caption) {
        m_caption = caption.empty() ? "Talk" : std::move(caption);
    }

    void NpcTalkWindow::sync_fonts(engine::IEngine& engine) {
        if (m_title_font.ready()) {
            return;
        }
        const std::string path = FreetypeFont::find_system_sans_bold();
        if (path.empty()) {
            return;
        }
        (void) m_title_font.load(engine, path, 18);
    }

    void NpcTalkWindow::destroy_fonts(engine::IEngine& engine) {
        m_title_font.destroy(engine);
    }

    bool NpcTalkWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        const float width = m_w * s;
        const float height = m_h * s;
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, width, height, s, frame_style());
    }

    void NpcTalkWindow::update_pointer(const engine::FrameInput& in) {
        m_pointer_x = in.mouse_x;
        m_pointer_y = in.mouse_y;
        m_pointer_pressed = in.mouse_left;
        UiView::reload(m_document, m_layout);
        const float s = std::max(0.25f, m_scale);
        const UiHit hit = UiView::hit(m_document,
                                      nullptr,
                                      in.mouse_x,
                                      in.mouse_y,
                                      m_x,
                                      m_y,
                                      m_w * s,
                                      m_h * s,
                                      s,
                                      variables(),
                                      this);
        set_pointer_hit(m_open ? hit.widget_id() : "", in.mouse_left);
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (in.mouse_scroll != 0.0f && contains(in.mouse_x, in.mouse_y)) {
            if (m_pages.empty()) {
                layout_content(nullptr);
            }
            if (in.mouse_scroll < 0.0f && m_page + 1 < m_pages.size()) {
                ++m_page;
            } else if (in.mouse_scroll > 0.0f && m_page > 0) {
                --m_page;
            }
        }
        if (!in.mouse_left) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            m_x = in.mouse_x - m_drag_dx;
            m_y = in.mouse_y - m_drag_dy;
        }
    }

    NpcTalkClickResult NpcTalkWindow::handle_click(float mx, float my) {
        if (!m_open || !contains(mx, my)) {
            return NpcTalkClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const float width = m_w * s;
        const float height = m_h * s;
        if (m_pages.empty()) {
            layout_content(nullptr);
        }
        const UiHit hit =
            UiView::hit(m_document, nullptr, mx, my, m_x, m_y, width, height, s, variables(), this);
        if (m_closable && hit.action() == "window.close") {
            m_open = false;
            m_dragging = false;
            return NpcTalkClickResult::Close;
        }
        if (hit.action() == "npc.previous" || hit.action() == "npc.next") {
            if (button_enabled(hit.action())) {
                if (hit.action() == "npc.previous") {
                    --m_page;
                } else {
                    ++m_page;
                }
            }
            return NpcTalkClickResult::Consumed;
        }
        auto activate_link = [&](const DialogLink& link) -> NpcTalkClickResult {
            std::int32_t next_dialog_id = 0;
            std::uint8_t revival_point = 0;
            if (parse_dialog_link_id(link.type, next_dialog_id)) {
                m_pending_dialog_id = next_dialog_id;
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::OpenDialog;
            }
            if (parse_revival_point(link.type, revival_point)) {
                m_pending_revival_point = revival_point;
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::SaveRevival;
            }
            if (link.type == "npc.village_revival") {
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::VillageRevival;
            }
            if (link.type == "close") {
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::Close;
            }
            if (link.type == "quest") {
                m_pending_quest_index = link.quest_index;
                m_pending_quest_flag = link.quest_flag;
                m_open = false;
                return NpcTalkClickResult::QuestLink;
            }
            if (link.type == "npc_teleport") {
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::NpcTeleport;
            }
            if (link.type == "blacksmith") {
                m_open = false;
                return NpcTalkClickResult::Blacksmith;
            }
            if (link.type == "mixing_up") {
                m_pending_mixing_mode = 0;
                m_open = false;
                return NpcTalkClickResult::Mixing;
            }
            if (link.type == "mixing_new") {
                m_pending_mixing_mode = 1;
                m_open = false;
                return NpcTalkClickResult::Mixing;
            }
            if (link.type == "mixing_list_up" || link.type == "mixing_list_new") {
                m_pending_mixing_mode = link.type == "mixing_list_new" ? 1u : 0u;
                m_open = false;
                m_dragging = false;
                return NpcTalkClickResult::MixingList;
            }
            if (link.type == "skin_weapons" || link.type == "skin_full" ||
                link.type == "skin_parts") {
                m_open = false;
                m_dragging = false;
                return link.type == "skin_weapons" ? NpcTalkClickResult::SkinWeapons
                       : link.type == "skin_full"  ? NpcTalkClickResult::SkinFull
                                                   : NpcTalkClickResult::SkinParts;
            }
            if (link.type == "shop") {
                m_open = false;
                return NpcTalkClickResult::Shop;
            }
            if (link.type == "trading_post_collection") {
                m_open = false;
                return NpcTalkClickResult::TradingPostCollection;
            }
            if (link.type == "storage") {
                m_open = false;
                return NpcTalkClickResult::Storage;
            }
            if (link.type == "craft_medicine" || link.type == "craft_magic_potion" ||
                link.type == "craft_auto_medicine") {
                m_open = false;
                m_dragging = false;
                if (link.type == "craft_magic_potion") {
                    return NpcTalkClickResult::CraftMagicPotion;
                }
                if (link.type == "craft_auto_medicine") {
                    return NpcTalkClickResult::CraftAutoMedicine;
                }
                return NpcTalkClickResult::CraftMedicine;
            }
            if (link.type == "speed_medicine") {
                m_open = false;
                return NpcTalkClickResult::SpeedMedicine;
            }
            if (link.type == "stablemaster") {
                m_open = false;
                return NpcTalkClickResult::Stablemaster;
            }
            if (link.type == "guild" || link.type == "guild_create") {
                m_open = false;
                m_dragging = false;
                return link.type == "guild_create" ? NpcTalkClickResult::GuildCreate
                                                   : NpcTalkClickResult::Guild;
            }
            return NpcTalkClickResult::Consumed;
        };
        const float select_x = m_x + width - (kResponseInset + kSelectWidth) * s;
        for (const auto& row : m_pages[m_page]) {
            if (!row.response) {
                continue;
            }
            const float select_y = m_y + (row.y + (row.h - kSelectHeight) * 0.5f) * s;
            if (UiAssets::point_in_rect(
                    mx, my, select_x, select_y, kSelectWidth * s, kSelectHeight * s)) {
                return activate_link(m_links[row.link_index]);
            }
        }
        float tx = 0.0f;
        float ty = 0.0f;
        float tw = 0.0f;
        float th = 0.0f;
        WidgetTools::window_title_drag_rect(
            m_x, m_y, width, height, s, frame_style(), tx, ty, tw, th);
        if (UiAssets::point_in_rect(mx, my, tx, ty, tw, th)) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return NpcTalkClickResult::Consumed;
        }
        return NpcTalkClickResult::Consumed;
    }

    bool NpcTalkWindow::button_enabled(std::string_view action) const {
        if (action == "npc.previous") {
            return m_page > 0;
        }
        if (action == "npc.next") {
            return m_page + 1 < m_pages.size();
        }
        return true;
    }

    void NpcTalkWindow::layout_content(const FreetypeFont* font) const {
        const float text_w = std::max(8.0f, m_w - 2.0f * kResponseInset);
        const float response_w = std::max(8.0f, text_w - kSelectWidth - kSelectGap);
        const float text_scale = font && font->pixel_size() > 0
                                     ? static_cast<float>(m_document.font_pixel_size()) /
                                           static_cast<float>(font->pixel_size())
                                     : 1.0f;
        const auto wrap = [&](const std::string& text, float width) {
            if (font && font->ready()) {
                auto lines = wrap_text(*font, text, width, text_scale);
                if (!lines.empty()) {
                    return lines;
                }
            }
            return std::vector<std::string>{text};
        };
        std::vector<ContentRow> rows;
        for (const auto& line : m_lines) {
            for (const auto& part : wrap(line, text_w)) {
                rows.push_back({{part}, 0, false, 0.0f, kBodyLineStep});
            }
        }
        const float full_available = std::max(kLinkHeight, m_h - 2.0f * kBodyPad);
        float available = std::max(kLinkHeight, full_available - kNavigationHeight);
        const auto max_lines = static_cast<std::size_t>(
            std::max(1.0f, (available - 2.0f * kResponsePadding) / kLinkLineStep));
        for (std::size_t i = 0; i < m_links.size(); ++i) {
            auto lines = wrap(m_links[i].text, response_w);
            for (std::size_t first = 0; first < lines.size(); first += max_lines) {
                const auto last = std::min(first + max_lines, lines.size());
                std::vector<std::string> part(lines.begin() + first, lines.begin() + last);
                const float h = std::max(kLinkHeight,
                                         kLinkLineStep * static_cast<float>(part.size()) +
                                             2.0f * kResponsePadding);
                rows.push_back({std::move(part), i, true, 0.0f, h});
            }
        }
        float total_height = 0.0f;
        for (const auto& row : rows) {
            total_height += row.h + (row.response ? kLinkGap : 0.0f);
        }
        if (!rows.empty() && rows.back().response) {
            total_height -= kLinkGap;
        }
        if (total_height <= full_available) {
            available = full_available;
        }
        m_pages.clear();
        m_pages.emplace_back();
        float y = kBodyPad;
        for (auto& row : rows) {
            if (y + row.h > kBodyPad + available && !m_pages.back().empty()) {
                m_pages.emplace_back();
                y = kBodyPad;
            }
            row.y = y;
            y += row.h + (row.response ? kLinkGap : 0.0f);
            m_pages.back().push_back(std::move(row));
        }
        m_page = std::min(m_page, m_pages.size() - 1);
    }

    void
    NpcTalkWindow::draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font) const {
        if (!m_open) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const float width = m_w * s;
        const float height = m_h * s;
        UiView::reload(m_document, m_layout);
        const float text_scale = font.pixel_size() > 0
                                     ? s * static_cast<float>(m_document.font_pixel_size()) /
                                           static_cast<float>(font.pixel_size())
                                     : s;
        layout_content(&font);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     width,
                     height,
                     s,
                     variables(),
                     font,
                     m_title_font.ready() ? m_title_font : font,
                     this,
                     true);

        const float text_x = m_x + kResponseInset * s;
        const float text_w = std::max(8.0f, (m_w - 2.0f * kResponseInset) * s);
        const float select_x = m_x + width - (kResponseInset + kSelectWidth) * s;
        for (const auto& row : m_pages[m_page]) {
            const float row_y = m_y + row.y * s;
            const float row_h = row.h * s;
            if (!row.response) {
                if (font.ready()) {
                    font.draw(
                        engine, text_x, row_y, row.lines.front(), 0.9f, 0.9f, 0.85f, text_scale);
                }
                continue;
            }
            const float row_x = text_x - kResponsePadding * s;
            const float row_w = text_w + 2.0f * kResponsePadding * s;
            const bool row_hovered =
                UiAssets::point_in_rect(m_pointer_x, m_pointer_y, row_x, row_y, row_w, row_h);
            engine.draw_ui_rect(row_x,
                                row_y,
                                row_w,
                                row_h,
                                row_hovered ? 0.11f : 0.035f,
                                row_hovered ? 0.09f : 0.032f,
                                row_hovered ? 0.055f : 0.028f,
                                row_hovered ? 0.72f : 0.48f);
            const float border = std::max(1.0f, s);
            auto draw_edge = [&](float x, float y, float w, float h) {
                engine.draw_ui_rect(x, y, w, h, 0.30f, 0.26f, 0.20f, 1.0f);
            };
            draw_edge(row_x, row_y, row_w, border);
            draw_edge(row_x, row_y + row_h - border, row_w, border);
            draw_edge(row_x, row_y + border, border, row_h - 2.0f * border);
            draw_edge(row_x + row_w - border, row_y + border, border, row_h - 2.0f * border);
            const float select_y = row_y + (row_h - kSelectHeight * s) * 0.5f;
            const bool hovered = UiAssets::point_in_rect(
                m_pointer_x, m_pointer_y, select_x, select_y, kSelectWidth * s, kSelectHeight * s);
            const char* file = hovered ? (m_pointer_pressed ? "dragon_dialog/button_pressed.bmp"
                                                            : "dragon_dialog/button_hover.bmp")
                                       : "dragon_dialog/button_idle.bmp";
            ui.draw_tex(file, select_x, select_y, kSelectWidth * s, kSelectHeight * s);
            if (font.ready()) {
                const float line_height = font.line_height() * text_scale;
                const float label_x =
                    select_x +
                    (kSelectWidth * s - font.text_width(kSelectLabel, text_scale)) * 0.5f;
                const float label_y = select_y + (kSelectHeight * s - line_height) * 0.5f + s;
                font.draw(engine, label_x, label_y, kSelectLabel, 0.90f, 0.81f, 0.62f, text_scale);
                const float block_height =
                    static_cast<float>(row.lines.size() - 1) * kLinkLineStep * s + line_height;
                float link_ty = row_y + (row_h - block_height) * 0.5f + s;
                for (const auto& part : row.lines) {
                    font.draw(engine, text_x, link_ty, part, 0.90f, 0.81f, 0.62f, text_scale);
                    link_ty += kLinkLineStep * s;
                }
            }
        }
    }

}  // namespace openkal::client
