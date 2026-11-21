/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PlayerOptionsWindow.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace openkal::client {
    namespace {
        std::string action_label(protocol::PlayerAction action,
                                 const protocol::PlayerOptionsState& state) {
            switch (action) {
            case protocol::PlayerAction::InviteParty:
                return "Invite to party";
            case protocol::PlayerAction::ApplyGuild:
                return "Request to join " + state.guild_name +
                       (state.application_fee
                            ? " (" + std::to_string(state.application_fee) + " Zamogeon)"
                            : "");
            case protocol::PlayerAction::ChallengeDuel:
                return "Challenge to Duel";
            case protocol::PlayerAction::AcceptDuel:
                return "Accept Duel";
            case protocol::PlayerAction::DeclineDuel:
                return "Decline Duel";
            case protocol::PlayerAction::Query:
                break;
            }
            return {};
        }
    }  // namespace
    void PlayerOptionsWindow::set_layouts(UiLayoutStore* layouts) {
        m_layouts = layouts;
        reload();
    }
    void PlayerOptionsWindow::reload() {
        if (!m_layouts) {
            return;
        }
        UiView::reload(m_document, &m_layouts->get("player_options"));
        UiView::reload(m_row, &m_layouts->get("player_option_row"));
    }
    void PlayerOptionsWindow::show(std::uint32_t target) {
        m_state = {};
        m_whisper_target.clear();
        m_state.target = target;
        m_open = target != 0;
        m_expected = 0;
        m_query_due = true;
        m_mutating = false;
        m_status = "Loading available actions...";
    }
    void PlayerOptionsWindow::close() {
        m_open = false;
        m_expected = 0;
    }
    void PlayerOptionsWindow::request_sent(std::uint64_t id) {
        m_expected = id;
        m_last_poll =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        if (!id) {
            m_mutating = false;
            m_status = "Request could not be sent.";
        }
    }
    void PlayerOptionsWindow::update(const protocol::PlayerOptionsState& state) {
        if (!m_open || state.target != m_state.target || !m_expected ||
            state.request_id != m_expected) {
            return;
        }
        m_state = state;
        m_expected = 0;
        m_mutating = false;
        if (!state.status.empty()) {
            m_status = state.status;
        } else if (m_status == "Loading available actions..." || m_status == "Sending request...") {
            m_status.clear();
        }
    }
    std::string PlayerOptionsWindow::take_whisper_target() {
        return std::exchange(m_whisper_target, {});
    }

    UiVarMap PlayerOptionsWindow::variables() const {
        UiVarMap vars;
        vars["player_options.title"] = "Player options";
        vars["player_options.target"] = m_state.name;
        vars["player_options.pm_available"] = !m_state.name.empty() ? "1" : "0";
        vars["player_options.pm_label"] = "Send PM";
        vars["player_options.empty"] = !m_expected && m_state.actions.empty() ? "1" : "0";
        vars["player_options.empty_label"] = "No actions are available.";
        auto lines = wrap_text(m_status, 396, [&](std::string_view text) {
            return m_font.ready() ? m_font.text_width(text) : static_cast<float>(text.size()) * 8;
        });
        vars["player_options.status"] = lines.empty() ? "" : lines[0];
        vars["player_options.status_more"] = lines.size() < 2 ? "" : lines[1];
        return vars;
    }
    std::optional<protocol::PlayerOptionsRequest>
    PlayerOptionsWindow::handle(const engine::FrameInput& input, WidgetTools& ui, float scale) {
        if (!m_open) {
            return {};
        }
        reload();
        m_mouse_x = input.mouse_x;
        m_mouse_y = input.mouse_y;
        m_mouse_down = input.mouse_left;
        m_w = m_document.window_w() > 0 ? m_document.window_w() : 420;
        m_h = std::max(m_document.window_h(),
                       156.0f + 36 * static_cast<float>(m_state.actions.size()));
        m_scale = std::max(
            .25f,
            std::min({scale, (input.width - 16) / (m_w + 80), (input.height - 16) / (m_h + 140)}));
        m_x = (input.width - m_w * m_scale) * .5f;
        m_y = (input.height - m_h * m_scale) * .5f;
        if (input.key_o_pressed || input.key_escape_pressed) {
            close();
            return {};
        }
        protocol::PlayerOptionsRequest request;
        request.target = m_state.target;
        request.guild = m_state.guild;
        request.expected_fee = m_state.application_fee;
        if (input.mouse_left_clicked) {
            const auto hit = UiView::hit(m_document,
                                         &ui,
                                         input.mouse_x,
                                         input.mouse_y,
                                         m_x,
                                         m_y,
                                         m_w * m_scale,
                                         m_h * m_scale,
                                         m_scale,
                                         variables(),
                                         nullptr,
                                         true);
            if (hit.action() == "window.close" || input.mouse_x < m_x - 40 * m_scale ||
                input.mouse_x > m_x + (m_w + 40) * m_scale || input.mouse_y < m_y - 84 * m_scale ||
                input.mouse_y > m_y + (m_h + 56) * m_scale) {
                close();
                return {};
            }
            if (hit.action() == "player_options.send_pm" && !m_state.name.empty()) {
                m_whisper_target = m_state.name;
                close();
                return {};
            }
            if (!m_mutating) {
                for (std::size_t i = 0; i < m_state.actions.size(); ++i) {
                    auto vars = variables();
                    vars["player_options.action_label"] = action_label(m_state.actions[i], m_state);
                    const auto row = UiView::hit(m_row,
                                                 &ui,
                                                 input.mouse_x,
                                                 input.mouse_y,
                                                 m_x,
                                                 m_y + (54 + 36 * static_cast<float>(i)) * m_scale,
                                                 m_w * m_scale,
                                                 32 * m_scale,
                                                 m_scale,
                                                 vars,
                                                 nullptr,
                                                 false);
                    if (row.action() == "player_options.choose") {
                        request.action = m_state.actions[i];
                        m_mutating = true;
                        m_status = "Sending request...";
                        return request;
                    }
                }
            }
        }
        const auto now =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        if (m_query_due || (!m_expected && !m_mutating && now - m_last_poll >= 1500) ||
            (m_expected && now - m_last_poll >= 5000)) {
            m_query_due = false;
            m_last_poll = now;
            return request;
        }
        return {};
    }
    void PlayerOptionsWindow::draw(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   const FreetypeFont& fallback) {
        if (!m_open) {
            return;
        }
        if (!m_font.ready()) {
            (void) m_font.load(engine, FreetypeFont::find_system_sans(), 13);
        }
        if (!m_title.ready()) {
            (void) m_title.load(engine, FreetypeFont::find_system_sans_bold(), 18);
        }
        const auto vars = variables();
        const auto& font = m_font.ready() ? m_font : fallback;
        UiViewHost chrome_host;
        const auto chrome_hit = UiView::hit(m_document,
                                            &ui,
                                            m_mouse_x,
                                            m_mouse_y,
                                            m_x,
                                            m_y,
                                            m_w * m_scale,
                                            m_h * m_scale,
                                            m_scale,
                                            vars,
                                            &chrome_host,
                                            true);
        chrome_host.set_pointer_hit(chrome_hit.widget_id(), m_mouse_down);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_w * m_scale,
                     m_h * m_scale,
                     m_scale,
                     vars,
                     font,
                     m_title,
                     &chrome_host);
        if (!m_mutating) {
            for (std::size_t i = 0; i < m_state.actions.size(); ++i) {
                auto row_vars = vars;
                row_vars["player_options.action_label"] = action_label(m_state.actions[i], m_state);
                UiViewHost host;
                const auto hit = UiView::hit(m_row,
                                             &ui,
                                             m_mouse_x,
                                             m_mouse_y,
                                             m_x,
                                             m_y + (54 + 36 * static_cast<float>(i)) * m_scale,
                                             m_w * m_scale,
                                             32 * m_scale,
                                             m_scale,
                                             row_vars,
                                             &host,
                                             false);
                host.set_pointer_hit(hit.widget_id(), m_mouse_down);
                UiView::draw(engine,
                             ui,
                             m_row,
                             m_x,
                             m_y + (54 + 36 * static_cast<float>(i)) * m_scale,
                             m_w * m_scale,
                             32 * m_scale,
                             m_scale,
                             row_vars,
                             font,
                             m_title,
                             &host,
                             false);
            }
        }
    }
    void PlayerOptionsWindow::destroy(engine::IEngine& engine) {
        m_font.destroy(engine);
        m_title.destroy(engine);
    }
}  // namespace openkal::client
