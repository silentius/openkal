/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ChatWidget.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Chatting.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace openkal::client {
    namespace {

        /** Builds the tab strip shared by rendering and pointer handling. */
        UiDocument channel_tabs(const UiDocument& source, float width, float height) {
            UiDocument result;
            if (const auto* configured = source.find_widget("channels")) {
                result.widgets().push_back(*configured);
                // Keep JSON tab proportions while the HUD expands the chat panel.
                const float base_width = source.window_w() > 16.0f ? source.window_w() : width;
                const float stretch =
                    std::max(0.0f, width - 16.0f) / std::max(1.0f, base_width - 16.0f);
                for (auto& tab : result.widgets().front().tabs()) {
                    tab.set_x(8.0f + (tab.x() - 8.0f) * stretch);
                    tab.set_w(tab.w() * stretch);
                }
            } else {
                UiWidget strip;
                strip.set_type(UiWidgetType::Tabs);
                strip.set_id("channels");
                strip.set_bind("chat.tab");
                strip.set_y(height - 26.0f);
                strip.set_h(20.0f);
                strip.set_align("center");
                strip.set_color_on("#FFF0CB");
                strip.set_color_off("#BDAE92");
                const float tab_width = (width - 16.0f) / ChatFilterModel::kTabCount;
                for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
                    const auto tab = static_cast<ChatTab>(t);
                    UiTabItem item;
                    item.set_id(ChatFilterModel::tab_id(tab));
                    item.set_label(ChatFilterModel::tab_label(tab));
                    item.set_x(8.0f + t * tab_width);
                    item.set_w(tab_width - 2.0f);
                    strip.tabs().push_back(std::move(item));
                }
                result.widgets().push_back(std::move(strip));
            }
            return result;
        }

        UiVarMap tab_variables(ChatTab selected) {
            UiVarMap vars;
            vars["chat.tab"] = ChatFilterModel::tab_id(selected);
            for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
                const auto tab = static_cast<ChatTab>(t);
                vars[std::string("chat.tab_") + ChatFilterModel::tab_id(tab)] =
                    ChatFilterModel::tab_label(tab);
            }
            return vars;
        }

        /** One wrapped line ready for FreeType drawing. */
        struct VisualLine {
            /** Text to draw. */
            std::string text;

            /** Red channel. */
            float r = 1.0f;

            /** Green channel. */
            float g = 1.0f;

            /** Blue channel. */
            float b = 1.0f;

            /** Receipt time, kept identical across all wrapped rows of an event. */
            double received_at = 0.0;

            /** Personal-message partner shared by all wrapped rows. */
            std::string reply_target;
        };

        /**
         * Wraps all accepted entries of one independent chat feed.
         * @param log Source history.
         * @param font Chat font.
         * @param max_width Maximum line width.
         * @param font_scale FreeType scale.
         * @param accepts Reports whether an entry is visible in this feed.
         * @return Visual lines in chronological order.
         */
        template <typename Accepts>
        [[nodiscard]] std::vector<VisualLine> build_visual_lines(const ChatLog& log,
                                                                 const FreetypeFont& font,
                                                                 float max_width,
                                                                 float font_scale,
                                                                 Accepts accepts) {
            std::vector<VisualLine> visual;
            for (const ChatLine& line : log.lines()) {
                if (!accepts(line)) {
                    continue;
                }
                if (!font.ready()) {
                    visual.push_back({line.text(),
                                      line.r(),
                                      line.g(),
                                      line.b(),
                                      line.received_at(),
                                      line.reply_target()});
                    continue;
                }
                const auto parts = wrap_text(font, line.text(), max_width, font_scale);
                if (parts.empty()) {
                    visual.push_back({std::string(),
                                      line.r(),
                                      line.g(),
                                      line.b(),
                                      line.received_at(),
                                      line.reply_target()});
                    continue;
                }
                for (const std::string& part : parts) {
                    visual.push_back({part,
                                      line.r(),
                                      line.g(),
                                      line.b(),
                                      line.received_at(),
                                      line.reply_target()});
                }
            }
            return visual;
        }

        /**
         * Draws one independent feed bottom-aligned inside its text region.
         * @param engine Engine used by FreeType.
         * @param font Chat font.
         * @param lines Wrapped source lines.
         * @param x Text left.
         * @param top Text region top.
         * @param bottom Text region bottom.
         * @param line_height Height of one visual line.
         * @param font_scale FreeType scale.
         * @param scroll Scroll offset in visual lines from newest.
         * @param visual_count Receives total visual lines.
         * @param row_drawn Optional callback for the actual visible row and its Y coordinate.
         */
        void draw_feed(engine::IEngine& engine,
                       const FreetypeFont& font,
                       const std::vector<VisualLine>& lines,
                       float x,
                       float top,
                       float bottom,
                       float line_height,
                       float font_scale,
                       int& scroll,
                       int& visual_count,
                       const std::function<void(const VisualLine&, float)>& row_drawn = {}) {
            visual_count = static_cast<int>(lines.size());
            scroll = std::min(scroll, std::max(0, visual_count - 1));
            const int visible = std::max(1, static_cast<int>((bottom - top) / line_height));
            int end = visual_count - scroll;
            const int start = std::max(0, end - visible);
            end = std::max(start, end);
            if (!font.ready()) {
                return;
            }
            float line_y = bottom - line_height;
            for (int i = end - 1; i >= start; --i) {
                const VisualLine& line = lines[static_cast<std::size_t>(i)];
                font.draw(engine, x, line_y, line.text, line.r, line.g, line.b, font_scale);
                if (row_drawn) {
                    row_drawn(line, line_y);
                }
                line_y -= line_height;
            }
        }

    }  // namespace

    void ChatWidget::set_battle_feed(bool enabled) {
        m_battle_feed = enabled;
        m_options_open = false;
        blur();
    }

    void ChatWidget::destroy(engine::IEngine& engine) {
        m_font.destroy(engine);
        if (m_feed_backdrop) {
            engine.destroy_texture(m_feed_backdrop);
            m_feed_backdrop = {};
        }
    }

    bool ChatWidget::contains_pointer(float mx,
                                      float my,
                                      float panel_x,
                                      float panel_y,
                                      float panel_w,
                                      float panel_h,
                                      float scale) const {
        if (m_battle_feed && !m_focused && !m_history_open) {
            return false;
        }
        return panel_contains_pointer(mx, my, panel_x, panel_y, panel_w, panel_h) ||
               options_contains_pointer(mx, my, panel_x, panel_y, panel_w, panel_h, scale);
    }

    bool ChatWidget::options_contains_pointer(float mx,
                                              float my,
                                              float panel_x,
                                              float panel_y,
                                              float panel_w,
                                              float panel_h,
                                              float scale) const {
        if (m_battle_feed || !m_options_open) {
            return false;
        }
        const OptionsRect r = options_rect(panel_x, panel_y, panel_w, panel_h, scale);
        return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
    }

    void ChatWidget::ensure_font(engine::IEngine& engine) {
        if (m_font.ready()) {
            return;
        }
        const std::string path = FreetypeFont::find_system_sans();
        if (!path.empty()) {
            (void) m_font.load(engine, path, 14);
        }
    }

    void ChatWidget::begin_whisper(std::string_view partner) {
        if (m_battle_feed || partner.empty() ||
            partner.find_first_of(" \t\r\n") != std::string_view::npos ||
            partner.size() + 2 > protocol::kChatMaxChars) {
            return;
        }
        m_input = "@" + std::string(partner) + " ";
        m_focused = true;
        m_options_open = false;
        m_filters.set_active_tab(ChatTab::Normal);
        m_filters.set_channel_enabled(ChatTab::Normal, protocol::ChatChannel::Whisper, true);
        m_chat_scroll = 0;
        m_reply_hits.clear();
    }

    void ChatWidget::push_chat(std::string_view speaker,
                               std::string_view wire_text,
                               std::string_view local_player) {
        if (m_battle_feed) {
            // Command replies and announcements arrive on the server chat channel too.
            if (speaker.empty() || speaker == "Server") {
                push_info(protocol::Chatting::display_body(wire_text), std::nullopt);
            }
            return;
        }
        if (speaker.empty() || speaker == "Server") {
            push_info(protocol::Chatting::display_body(wire_text), std::nullopt);
            return;
        }
        const protocol::ChatChannel channel = protocol::Chatting::channel_of(wire_text);
        float r = 1, g = 1, b = 1;
        ChatFilterModel::channel_color(channel, r, g, b);
        std::ostringstream oss;
        const std::string_view body = protocol::Chatting::display_body(wire_text);
        std::string reply_target;
        if (channel == protocol::ChatChannel::Whisper) {
            std::string recipient, message;
            if (protocol::Chatting::parse_whisper(wire_text, recipient, message)) {
                reply_target = speaker == local_player ? recipient : std::string(speaker);
            }
        }
        if (channel == protocol::ChatChannel::Whisper && speaker == local_player) {
            oss << "To " << reply_target << ": " << body;
        } else if (!speaker.empty()) {
            oss << speaker << ": " << body;
        } else {
            oss << body;
        }
        m_chat_log.push(
            ChatLine(channel, std::nullopt, oss.str(), r, g, b, 0.0, std::move(reply_target)));
        m_chat_scroll = 0;
    }

    void ChatWidget::push_info(std::string_view text,
                               std::optional<protocol::BattleMsgKind> kind,
                               std::optional<std::array<float, 3>> color) {
        if (text.empty()) {
            return;
        }
        float r = 1, g = 1, b = 1;
        ChatFilterModel::battle_color(kind, r, g, b);
        if (m_battle_feed) {
            r = 0.95f;
            g = 0.91f;
            b = 0.80f;
            if (kind == protocol::BattleMsgKind::Exp ||
                kind == protocol::BattleMsgKind::TransformExp) {
                r = 1.0f;
                g = 0.90f;
                b = 0.35f;
            } else if (kind == protocol::BattleMsgKind::Hp) {
                r = 0.62f;
                g = 0.75f;
                b = 0.65f;
            } else if (kind == protocol::BattleMsgKind::ItemGet) {
                r = 0.40f;
                g = 0.95f;
                b = 0.48f;
            } else if (kind == protocol::BattleMsgKind::MoneyGet) {
                r = 1.0f;
                g = 1.0f;
                b = 1.0f;
            } else if (kind == protocol::BattleMsgKind::Damage) {
                r = 0.85f;
                g = 0.65f;
                b = 0.59f;
            } else if (kind == protocol::BattleMsgKind::Mp) {
                r = 0.64f;
                g = 0.74f;
                b = 0.83f;
            }
        }
        if (color.has_value()) {
            r = (*color)[0];
            g = (*color)[1];
            b = (*color)[2];
        }
        if (m_battle_feed && m_message_scroll > 0 &&
            (kind ? m_filters.battle_enabled(*kind) : m_filters.system_enabled())) {
            const int rows =
                m_font.ready()
                    ? std::max(1,
                               static_cast<int>(
                                   wrap_text(m_font, text, m_message_width, m_message_font_scale)
                                       .size()))
                    : 1;
            m_message_scroll += rows;
        } else if (!m_battle_feed) {
            m_message_scroll = 0;
        }
        m_message_log.push(
            ChatLine(protocol::ChatChannel::Common, kind, std::string(text), r, g, b, m_elapsed));
    }

    bool ChatWidget::submit(const std::function<void(std::string_view)>& send_chat) {
        // Trim trailing whitespace.
        while (!m_input.empty() &&
               (m_input.back() == ' ' || m_input.back() == '\t' || m_input.back() == '\r')) {
            m_input.pop_back();
        }
        if (m_input.empty()) {
            return false;
        }
        if (m_battle_feed &&
            (m_input.size() == 1 || !protocol::Chatting::is_slash_command(m_input))) {
            return false;
        }
        const ClientChatCommandResult local = m_commands.try_dispatch(m_input);
        if (local.handled()) {
            if (!local.info_line().empty()) {
                push_info(local.info_line(), std::nullopt);
            }
            m_input.clear();
            return true;
        }
        std::string outgoing = m_input;
        if (!m_battle_feed && m_input.front() != '/' && m_input.front() != '!' &&
            protocol::Chatting::channel_of(m_input) == protocol::ChatChannel::Common) {
            char prefix = 0;
            switch (m_filters.active_tab()) {
            case ChatTab::Party:
                prefix = '#';
                break;
            case ChatTab::Guild:
                prefix = '$';
                break;
            case ChatTab::Alliance:
                push_info("Alliance chat is not available yet.", std::nullopt);
                return false;
            case ChatTab::Squad:
                push_info("Squad chat is not available yet.", std::nullopt);
                return false;
            case ChatTab::Shout:
                prefix = '^';
                break;
            default:
                break;
            }
            if (prefix) {
                outgoing.insert(outgoing.begin(), prefix);
            }
        }
        if (outgoing.size() > protocol::kChatMaxChars) {
            push_info("Message is too long for this channel.", std::nullopt);
            return false;
        }
        if (send_chat) {
            send_chat(outgoing);
        }
        m_input.clear();
        return true;
    }

    bool ChatWidget::handle_options_click(
        float mx, float my, float x, float y, float w, float h, float scale) {
        if (!m_options_open) {
            return false;
        }
        const float s = std::max(0.25f, scale);
        const OptionsRect r = options_rect(x, y, w, h, scale);
        if (mx < r.x || my < r.y || mx > r.x + r.w || my > r.y + r.h) {
            return false;
        }
        const float row_h = 16.0f * s;
        float cy = r.y + 22.0f * s;
        // Channel rows
        for (std::size_t c = 0; c < ChatFilterModel::kChannelCount; ++c) {
            if (my >= cy && my < cy + row_h) {
                const auto ch = static_cast<protocol::ChatChannel>(c);
                m_filters.set_channel_enabled(
                    m_filters.active_tab(),
                    ch,
                    !m_filters.channel_enabled(m_filters.active_tab(), ch));
                m_chat_scroll = 0;
                return true;
            }
            cy += row_h;
        }
        cy += 4.0f * s;
        // Battle header row is not clickable.
        cy += row_h;
        for (std::size_t b = 0; b < ChatFilterModel::kBattleCount; ++b) {
            if (my >= cy && my < cy + row_h) {
                const auto kind = static_cast<protocol::BattleMsgKind>(b);
                m_filters.set_battle_enabled(kind, !m_filters.battle_enabled(kind));
                return true;
            }
            cy += row_h;
        }
        return true;
    }

    ChatInputResult
    ChatWidget::handle_input(const engine::FrameInput& in,
                             float panel_x,
                             float panel_y,
                             float panel_w,
                             float panel_h,
                             float scale,
                             const UiLayout* layout,
                             const std::function<void(std::string_view)>& send_chat) {
        if (std::isfinite(in.dt) && in.dt > 0.0f) {
            m_elapsed += in.dt;
        }
        const float s = std::max(0.25f, scale);
        UiDocument doc;
        UiView::reload(doc, layout);
        const UiWidget* lines = doc.find_widget("lines");
        const float pad = (lines ? lines->x() : 8.0f) * s;

        const bool over_panel =
            panel_contains_pointer(in.mouse_x, in.mouse_y, panel_x, panel_y, panel_w, panel_h);
        const bool over_ui =
            over_panel ||
            options_contains_pointer(in.mouse_x, in.mouse_y, panel_x, panel_y, panel_w, panel_h, s);

        if (m_battle_feed) {
            if (in.key_escape_pressed && (m_focused || m_history_open)) {
                blur();
                return ChatInputResult::Handled;
            }
            if (std::fabs(in.mouse_scroll) > 0.0f && over_panel && !m_message_log.lines().empty()) {
                m_history_open = true;
                const int total = std::max(m_message_visual_count,
                                           static_cast<int>(m_message_log.lines().size()));
                m_message_scroll = std::clamp(m_message_scroll + static_cast<int>(in.mouse_scroll),
                                              0,
                                              std::max(0, total - 1));
                return ChatInputResult::Handled;
            }
            if (in.mouse_left_clicked) {
                if (!over_panel) {
                    blur();
                } else if (m_focused || m_history_open) {
                    return ChatInputResult::Handled;
                }
            }
            if (!m_focused) {
                if (in.key_enter_pressed) {
                    m_focused = true;
                    m_history_open = true;
                    m_message_scroll = 0;
                    m_input.clear();
                    return ChatInputResult::Handled;
                }
                return ChatInputResult::Ignored;
            }
        }

        if (!m_battle_feed && in.mouse_left_clicked) {
            if (handle_options_click(
                    in.mouse_x, in.mouse_y, panel_x, panel_y, panel_w, panel_h, s)) {
                return ChatInputResult::Handled;
            }
            if (over_panel) {
                const auto tabs = channel_tabs(doc, panel_w / s, panel_h / s);
                const auto hit = UiView::hit(tabs,
                                             nullptr,
                                             in.mouse_x,
                                             in.mouse_y,
                                             panel_x,
                                             panel_y,
                                             panel_w,
                                             panel_h,
                                             s,
                                             tab_variables(m_filters.active_tab()),
                                             nullptr,
                                             false);
                if (hit.kind() == UiHitKind::Tab && hit.bind() == "chat.tab") {
                    for (std::size_t t = 0; t < ChatFilterModel::kTabCount; ++t) {
                        const auto tab = static_cast<ChatTab>(t);
                        if (hit.tab_id() == ChatFilterModel::tab_id(tab)) {
                            m_filters.set_active_tab(tab);
                            m_chat_scroll = 0;
                            return ChatInputResult::Handled;
                        }
                    }
                }
                // Gear hitbox top-right of chat panel.
                const float gear = 16.0f * s;
                if (in.mouse_x >= panel_x + panel_w - pad - gear &&
                    in.mouse_x <= panel_x + panel_w - pad && in.mouse_y >= panel_y + pad &&
                    in.mouse_y <= panel_y + pad + gear) {
                    m_options_open = !m_options_open;
                    return ChatInputResult::Handled;
                }
                if (m_reply_tab == m_filters.active_tab() && m_reply_scroll == m_chat_scroll &&
                    m_filters.channel_enabled(m_filters.active_tab(),
                                              protocol::ChatChannel::Whisper)) {
                    for (const auto& row : m_reply_hits) {
                        if (WidgetTools::point_in_rect(
                                in.mouse_x, in.mouse_y, row.x, row.y, row.w, row.h)) {
                            const auto partner = row.partner;
                            begin_whisper(partner);
                            return ChatInputResult::Handled;
                        }
                    }
                }
                m_focused = true;
                return ChatInputResult::Handled;
            }
            if (m_focused) {
                m_focused = false;
            }
        }

        // Wheel over chat UI scrolls history (or is swallowed by options) and must
        // not zoom the camera — same as compatibility behavior.
        if (!m_battle_feed && std::fabs(in.mouse_scroll) > 0.0f && over_ui) {
            if (over_panel) {
                const float message_bottom = panel_y + pad + 4.0f * 14.0f * s;
                if (in.mouse_y <= message_bottom) {
                    m_message_scroll =
                        std::max(0, m_message_scroll + static_cast<int>(in.mouse_scroll));
                    const int max_scroll =
                        std::max(0,
                                 std::max(m_message_visual_count,
                                          static_cast<int>(m_message_log.lines().size())) -
                                     1);
                    m_message_scroll = std::min(m_message_scroll, max_scroll);
                } else {
                    m_chat_scroll = std::max(0, m_chat_scroll + static_cast<int>(in.mouse_scroll));
                    const int max_scroll = std::max(
                        0,
                        std::max(m_chat_visual_count, static_cast<int>(m_chat_log.lines().size())) -
                            1);
                    m_chat_scroll = std::min(m_chat_scroll, max_scroll);
                }
            }
            return ChatInputResult::Handled;
        }

        if (!m_focused) {
            if (in.key_enter_pressed) {
                m_focused = true;
                return ChatInputResult::Handled;
            }
            return ChatInputResult::Ignored;
        }

        if (in.key_escape_pressed) {
            m_focused = false;
            m_options_open = false;
            return ChatInputResult::Handled;
        }

        if (in.key_enter_pressed) {
            const bool ok = submit(send_chat);
            if (m_battle_feed && ok) {
                blur();
            }
            return ok ? ChatInputResult::Submitted : ChatInputResult::Handled;
        }

        if (in.key_backspace_pressed && !m_input.empty()) {
            m_input.pop_back();
            return ChatInputResult::Handled;
        }

        for (int i = 0; i < in.text_char_count; ++i) {
            const char c = in.text_chars[i];
            if (c < 32 || c == 127) {
                continue;
            }
            if (m_input.size() >= protocol::kChatMaxChars) {
                break;
            }
            m_input.push_back(c);
        }
        if (in.text_char_count > 0) {
            return ChatInputResult::Handled;
        }
        // Consume movement keys while focused so WASD does not walk.
        return ChatInputResult::Handled;
    }

    void ChatWidget::draw_options(
        WidgetTools& ui, engine::IEngine& engine, float x, float y, float w, float h, float scale) {
        if (!m_options_open) {
            return;
        }
        ensure_font(engine);
        const float s = std::max(0.25f, scale);
        const OptionsRect r = options_rect(x, y, w, h, scale);
        ui.renderer()->draw_ui_rect(r.x, r.y, r.w, r.h, 0.05f, 0.05f, 0.08f, 0.92f);
        const float row_h = 16.0f * s;
        if (m_font.ready()) {
            const std::string heading =
                std::string(ChatFilterModel::tab_label(m_filters.active_tab())) + " filters";
            m_font.draw(
                engine, r.x + 8.0f * s, r.y + 4.0f * s, heading, 1.0f, 0.85f, 0.4f, 0.85f * s);
        }
        float cy = r.y + 22.0f * s;
        for (std::size_t c = 0; c < ChatFilterModel::kChannelCount; ++c) {
            const auto ch = static_cast<protocol::ChatChannel>(c);
            const bool on = m_filters.channel_enabled(m_filters.active_tab(), ch);
            std::ostringstream label;
            label << (on ? "[x] " : "[ ] ") << ChatFilterModel::channel_label(ch);
            if (m_font.ready()) {
                m_font.draw(engine, r.x + 8.0f * s, cy, label.str(), 0.9f, 0.9f, 0.85f, 0.85f * s);
            }
            cy += row_h;
        }
        cy += 4.0f * s;
        if (m_font.ready()) {
            m_font.draw(engine,
                        r.x + 8.0f * s,
                        cy,
                        "Battle messages (global)",
                        1.0f,
                        0.85f,
                        0.4f,
                        0.85f * s);
        }
        cy += row_h;
        for (std::size_t b = 0; b < ChatFilterModel::kBattleCount; ++b) {
            const auto kind = static_cast<protocol::BattleMsgKind>(b);
            const bool on = m_filters.battle_enabled(kind);
            std::ostringstream label;
            label << (on ? "[x] " : "[ ] ") << ChatFilterModel::battle_label(kind);
            if (m_font.ready()) {
                m_font.draw(engine, r.x + 8.0f * s, cy, label.str(), 0.9f, 0.9f, 0.85f, 0.85f * s);
            }
            cy += row_h;
        }
    }

    void ChatWidget::draw_battle_feed(WidgetTools& ui,
                                      engine::IEngine& engine,
                                      float x,
                                      float y,
                                      float w,
                                      float h,
                                      float scale,
                                      const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const float s = std::max(0.25f, scale);
        const UiWidget* messages = doc.find_widget("messages");
        const UiWidget* input = doc.find_widget("input");
        const float pad = 8.0f * s;
        const float line_h =
            (messages && messages->line_height() > 0.0f ? messages->line_height() : 20.0f) * s;
        const float font_scale = s * (layout && doc.font_pixel_size() > 0
                                          ? static_cast<float>(doc.font_pixel_size()) / 14.0f
                                          : 1.0f);
        const float input_h = (input && input->h() > 0 ? input->h() : 22.0f) * s;
        const float bottom = y + h - pad - (m_focused ? input_h + pad : 0.0f);
        const float top = y + pad + (m_history_open ? line_h : 0.0f);
        const float text_x = x + (messages ? messages->x() : 8.0f) * s;
        const float text_w = std::max(8.0f, w - (text_x - x) - pad);
        m_message_width = text_w;
        m_message_font_scale = font_scale;
        const auto opacity = [this](double received_at) {
            // Eight seconds fully readable, followed by a four-second fade.
            return m_history_open ? 1.0f
                                  : static_cast<float>(std::clamp(
                                        (12.0 - (m_elapsed - received_at)) / 4.0, 0.0, 1.0));
        };
        const auto lines = build_visual_lines(
            m_message_log, m_font, text_w, font_scale, [this, &opacity](const ChatLine& line) {
                return (line.battle_kind() ? m_filters.battle_enabled(*line.battle_kind())
                                           : m_filters.system_enabled()) &&
                       opacity(line.received_at()) > 0.0f;
            });
        m_message_visual_count = static_cast<int>(lines.size());
        const float list_h = (messages && messages->h() > 0.0f ? messages->h() : 100.0f) * s;
        const int visible = std::max(1, static_cast<int>(std::min(bottom - top, list_h) / line_h));
        m_message_scroll =
            std::clamp(m_message_scroll, 0, std::max(0, m_message_visual_count - visible));
        const int end = m_message_visual_count - m_message_scroll;
        const int start = std::max(0, end - visible);
        float alpha = m_focused || m_history_open ? 1.0f : 0.0f;
        for (int i = start; i < end; ++i) {
            alpha = std::max(alpha, opacity(lines[static_cast<std::size_t>(i)].received_at));
        }
        if (alpha <= 0.0f) {
            return;
        }

        const UiWidget* fill = doc.find_widget("panel_fill");
        float fr = 0.05f, fg = 0.075f, fb = 0.06f;
        float fill_alpha = 0.48f;
        if (fill) {
            (void) ui_parse_hex_color(fill->color(), fr, fg, fb);
            fill_alpha = fill->alpha();
        }
        const float shade_top = m_history_open ? y : bottom - (end - start) * line_h - pad;
        const float shade_bottom = m_focused ? y + h : bottom + pad;
        const auto smooth = [](float value) {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };
        if (!m_feed_backdrop) {
            constexpr int columns = 64, rows = 32;
            std::array<std::uint8_t, columns * rows * 4> pixels{};
            for (int row = 0; row < rows; ++row) {
                const float v = static_cast<float>(row) / (rows - 1);
                const float vertical = smooth(v / 0.2f) * smooth((1.0f - v) / 0.2f);
                for (int column = 0; column < columns; ++column) {
                    const float u = static_cast<float>(column) / (columns - 1);
                    const float horizontal = smooth(u / 0.06f) * smooth((1.0f - u) / 0.5f);
                    const auto offset = static_cast<std::size_t>((row * columns + column) * 4);
                    pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = 255;
                    pixels[offset + 3] = static_cast<std::uint8_t>(255.0f * horizontal * vertical);
                }
            }
            m_feed_backdrop =
                engine.create_texture_rgba8(pixels.data(), columns, rows, false, false);
        }
        if (m_feed_backdrop) {
            engine.draw_ui_quad(m_feed_backdrop,
                                x + (fill ? fill->x() : -20.0f) * s,
                                shade_top,
                                w - (fill ? fill->x() : -20.0f) * s,
                                shade_bottom - shade_top,
                                fr,
                                fg,
                                fb,
                                fill_alpha * alpha);
        }
        if (!m_font.ready()) {
            return;
        }
        float text_y = bottom - static_cast<float>(end - start) * line_h;
        for (int i = start; i < end; ++i) {
            const auto& line = lines[static_cast<std::size_t>(i)];
            const float depth =
                m_history_open ? 1.0f : 0.65f + 0.35f * (i - start + 1) / std::max(1, end - start);
            const float a = opacity(line.received_at) * depth;
            m_font.draw(engine, text_x + s, text_y + s, line.text, 0, 0, 0, font_scale, a * 0.8f);
            m_font.draw(engine, text_x, text_y, line.text, line.r, line.g, line.b, font_scale, a);
            text_y += line_h;
        }
        if (m_history_open) {
            const UiWidget* hint = doc.find_widget("history_hint");
            const UiVarMap vars{
                {"chat.history_hint", "Wheel: history | Enter: command | Esc: close"}};
            const std::string text =
                ui_interpolate(hint ? hint->text() : "${chat.history_hint}", vars);
            m_font.draw(engine, text_x, y + pad, text, 0.72f, 0.69f, 0.60f, font_scale * 0.85f);
        }
        if (m_focused) {
            const float iy = y + h - pad - input_h;
            ui.renderer()->draw_ui_rect(text_x, iy, text_w, input_h, fr, fg, fb, 0.65f);
            std::string shown = m_input;
            if (static_cast<int>(m_elapsed * 2.0) % 2 == 0) {
                shown.push_back('_');
            }
            // Keep the command's end in view without covering the feed with extra editor rows.
            while (!shown.empty() && m_font.text_width(shown, font_scale) > text_w - 8.0f * s) {
                shown.erase(shown.begin());
            }
            m_font.draw(
                engine, text_x + 4.0f * s, iy + 2.0f * s, shown, 0.95f, 0.91f, 0.80f, font_scale);
        }
    }

    void ChatWidget::draw(WidgetTools& ui,
                          engine::IEngine& engine,
                          float x,
                          float y,
                          float w,
                          float h,
                          float scale,
                          const UiLayout* layout) {
        ensure_font(engine);
        m_reply_hits.clear();
        if (m_battle_feed) {
            draw_battle_feed(ui, engine, x, y, w, h, scale, layout);
            return;
        }
        const float s = std::max(0.25f, scale);
        UiDocument doc;
        UiView::reload(doc, layout);
        const UiWidget* fill = doc.find_widget("panel_fill");
        float fr = 0.0f, fg = 0.0f, fb = 0.0f, fa = 0.45f;
        if (fill) {
            (void) ui_parse_hex_color(
                fill->color().empty() ? "#000000" : fill->color(), fr, fg, fb);
            fa = fill->alpha();
        }
        ui.renderer()->draw_ui_rect(x, y, w, h, fr, fg, fb, fa);
        const UiWidget* border = doc.find_widget("frame");
        const float t = ((border && border->thickness() > 0.0f) ? border->thickness() : 3.0f) * s;
        const std::string top = (border && !border->border_top().empty()) ? border->border_top()
                                                                          : "chatting-box-top.bmp";
        const std::string bottom = (border && !border->border_bottom().empty())
                                       ? border->border_bottom()
                                       : "chatting-box-bottom.bmp";
        const std::string left = (border && !border->border_left().empty())
                                     ? border->border_left()
                                     : "chatting-box-left.bmp";
        const std::string right = (border && !border->border_right().empty())
                                      ? border->border_right()
                                      : "chatting-box-right.bmp";
        ui.draw_tex(top, x, y, w, t);
        ui.draw_tex(bottom, x, y + h - t, w, t);
        ui.draw_tex(left, x, y, t, h);
        ui.draw_tex(right, x + w - t, y, t, h);

        const UiWidget* input_w = doc.find_widget("input");
        const UiWidget* mode_w = doc.find_widget("mode");
        const float pad = 8.0f * s;
        const float input_line_h = (input_w && input_w->h() > 0.0f ? input_w->h() : 18.0f) * s;
        const float input_x0 = (input_w ? input_w->x() : 72.0f) * s;
        const float line_h = 14.0f * s;
        const float font_scale = 0.9f * s;
        const float ix = x + input_x0;
        const float iw = std::max(0.0f, w - input_x0 - pad);
        std::string shown = m_input;
        if (m_focused && (static_cast<int>(engine.frame_stats().fps) % 2 == 0 || shown.empty())) {
            shown.push_back('_');
        }
        std::vector<std::string> input_lines;
        if (m_font.ready()) {
            input_lines = wrap_text(m_font, shown, std::max(8.0f, iw - 4.0f * s), font_scale);
        }
        if (input_lines.empty()) {
            input_lines.emplace_back(shown);
        }
        const int input_rows = std::clamp(static_cast<int>(input_lines.size()), 1, 3);
        const float input_h = input_line_h * static_cast<float>(input_rows);
        const auto tabs = channel_tabs(doc, w / s, h / s);
        const float tab_top = tabs.widgets().front().y() * s;
        const float iy = y + tab_top - 4.0f * s - input_h;

        auto channel = ChatFilterModel::tab_channel(m_filters.active_tab());
        std::string mode = ChatFilterModel::tab_label(m_filters.active_tab());
        if (!m_input.empty()) {
            const auto explicit_channel = protocol::Chatting::channel_of(m_input);
            if (explicit_channel != protocol::ChatChannel::Common || m_input.front() == '!') {
                channel = explicit_channel;
                mode = channel == protocol::ChatChannel::Whisper
                           ? "PM"
                           : ChatFilterModel::channel_label(channel);
            }
        }
        float mode_r = 1, mode_g = 1, mode_b = 1;
        ChatFilterModel::channel_color(channel, mode_r, mode_g, mode_b);
        if (m_font.ready()) {
            const float mode_x = x + (mode_w ? mode_w->x() : 8.0f) * s;
            m_font.draw(engine, mode_x, iy + 2.0f * s, mode, mode_r, mode_g, mode_b, font_scale);
            m_font.draw(
                engine, x + w - pad - 14.0f * s, y + pad, "O", 0.85f, 0.75f, 0.4f, font_scale);
        }

        const float log_max_w = std::max(8.0f, w - pad * 2.0f);
        const auto message_lines = build_visual_lines(
            m_message_log, m_font, log_max_w, font_scale, [this](const ChatLine& line) {
                return line.battle_kind() ? m_filters.battle_enabled(*line.battle_kind())
                                          : m_filters.system_enabled();
            });
        const auto chat_lines = build_visual_lines(
            m_chat_log, m_font, log_max_w, font_scale, [this](const ChatLine& line) {
                return m_filters.channel_enabled(m_filters.active_tab(), line.channel());
            });

        // The compatible client has an independent message list above the tabbed chat list.
        // Keep four compact message rows and reserve the remaining height for player chat.
        const float message_top = y + pad;
        const float message_bottom = message_top + line_h * 4.0f;
        const float divider_y = message_bottom + 3.0f * s;
        ui.renderer()->draw_ui_rect(
            x + pad, divider_y, log_max_w, std::max(1.0f, s), 0.36f, 0.32f, 0.20f, 0.55f);
        const float text_bottom = iy - pad;
        const float chat_top = divider_y + 5.0f * s;
        draw_feed(engine,
                  m_font,
                  message_lines,
                  x + pad,
                  message_top,
                  message_bottom,
                  line_h,
                  font_scale,
                  m_message_scroll,
                  m_message_visual_count);
        draw_feed(engine,
                  m_font,
                  chat_lines,
                  x + pad,
                  chat_top,
                  text_bottom,
                  line_h,
                  font_scale,
                  m_chat_scroll,
                  m_chat_visual_count,
                  [this, x, pad, line_h, font_scale](const VisualLine& line, float row_y) {
                      if (!line.reply_target.empty()) {
                          m_reply_hits.push_back({x + pad,
                                                  row_y,
                                                  m_font.text_width(line.text, font_scale),
                                                  line_h,
                                                  line.reply_target});
                      }
                  });
        m_reply_tab = m_filters.active_tab();
        m_reply_scroll = m_chat_scroll;

        ui.renderer()->draw_ui_rect(
            ix, iy, iw, input_h, 0.0f, 0.0f, 0.0f, m_focused ? 0.55f : 0.35f);
        if (m_font.ready()) {
            float text_y = iy + 2.0f * s;
            const int first = std::max(0, static_cast<int>(input_lines.size()) - input_rows);
            for (int i = 0; i < input_rows; ++i) {
                const std::size_t idx = static_cast<std::size_t>(first + i);
                if (idx >= input_lines.size()) {
                    break;
                }
                m_font.draw(engine,
                            ix + 2.0f * s,
                            text_y,
                            input_lines[idx],
                            0.95f,
                            0.95f,
                            0.9f,
                            font_scale);
                text_y += input_line_h;
            }
        }

        // Draw backing even when optional tab artwork is unavailable.
        for (const auto& strip : tabs.widgets()) {
            for (const auto& tab : strip.tabs()) {
                const bool active = tab.id() == ChatFilterModel::tab_id(m_filters.active_tab());
                ui.renderer()->draw_ui_rect(x + tab.x() * s,
                                            y + strip.y() * s,
                                            tab.w() * s,
                                            strip.h() * s,
                                            active ? 0.25f : 0.10f,
                                            active ? 0.25f : 0.10f,
                                            active ? 0.35f : 0.15f,
                                            0.95f);
            }
        }
        UiView::draw_widgets(
            engine, ui, tabs, x, y, w, h, s, tab_variables(m_filters.active_tab()), m_font, m_font);
        draw_options(ui, engine, x, y, w, h, s);
    }

}  // namespace openkal::client
