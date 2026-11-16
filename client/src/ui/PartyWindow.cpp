/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PartyWindow.h"

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace openkal::client {
    namespace {
        class PartyViewHost final : public UiViewHost {
          public:
            explicit PartyViewHost(const UiVarMap& values, bool invite_enabled = false)
                : m_values(values), m_invite_enabled(invite_enabled) {}
            bool button_enabled(std::string_view action) const override {
                return action != "party.invite" || m_invite_enabled;
            }
            float progress_ratio(std::string_view bind) const override {
                const auto found = m_values.find(std::string(bind));
                return found == m_values.end() ? 0 : std::strtof(found->second.c_str(), nullptr);
            }

          private:
            const UiVarMap& m_values;
            bool m_invite_enabled;
        };

        bool inside(float x, float y, float left, float top, float width, float height) {
            return x >= left && y >= top && x < left + width && y < top + height;
        }
        struct DialogGeometry {
            float x, y, w, h, scale;
        };

        DialogGeometry
        dialog_geometry(const UiDocument& doc, float width, float height, float preferred_scale) {
            const float outer_w = doc.window_w() + doc.pad_left() + doc.pad_right();
            const float outer_h = doc.window_h() + doc.pad_top() + doc.pad_bottom();
            const float scale = std::max(
                0.25f,
                std::min({preferred_scale, (width - 16) / outer_w, (height - 16) / outer_h}));
            return {(width - outer_w * scale) / 2 + doc.pad_left() * scale,
                    (height - outer_h * scale) / 2 + doc.pad_top() * scale,
                    doc.window_w() * scale,
                    doc.window_h() * scale,
                    scale};
        }

        void fit_label(std::string& text, const FreetypeFont& font, float scale, float width) {
            if (!font.ready() || font.text_width(text, scale) <= width) {
                return;
            }
            while (!text.empty() && font.text_width(text + "...", scale) > width) {
                auto end = text.size() - 1;
                while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
                    --end;
                }
                text.resize(end);
            }
            text += "...";
        }

        constexpr float kActionRowsY = 118;
    }  // namespace

    void PartyWindow::set_layouts(UiLayoutStore* layouts) {
        m_layouts = layouts;
    }

    void PartyWindow::set_logical_position(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) {
            return;
        }
        m_logical_x = x;
        m_logical_y = y;
        m_positioned = true;
    }

    void PartyWindow::clamp_position() {
        m_x = std::clamp(m_x, 0.0f, std::max(0.0f, m_screen_w - m_width * m_scale));
        m_y = std::clamp(m_y, 0.0f, std::max(0.0f, m_screen_h - m_height * m_scale));
    }

    void PartyWindow::update(const protocol::PartyState& state,
                             float scale,
                             float width,
                             float height,
                             std::uint32_t local_player) {
        if (state.id != m_state.id || state.id == 0) {
            close_actions();
        }
        m_state = state;
        m_local_player = local_player;
        if (!open()) {
            m_dragging = false;
        }
        if (!m_layouts) {
            return;
        }
        UiView::reload(m_document, &m_layouts->get("party"));
        UiView::reload(m_row, &m_layouts->get("party_member"));
        UiView::reload(m_invitation_document, &m_layouts->get("party_invitation"));
        UiView::reload(m_actions_document, &m_layouts->get("party_actions"));
        UiView::reload(m_action_row, &m_layouts->get("party_action_member"));
        const auto removable =
            is_leader()
                ? std::count_if(state.members.begin(),
                                state.members.end(),
                                [&](const auto& member) { return member.id != m_local_player; })
                : 0;
        m_actions_document.set_window_h(m_actions_document.window_h() +
                                        static_cast<float>(removable) * m_action_row.window_h());
        m_width = m_document.window_w();
        m_height =
            m_document.window_h() + static_cast<float>(state.members.size()) * m_row.window_h() + 3;
        for (const auto* id : {"panel_fill", "frame"}) {
            if (auto* widget = m_document.find_widget(id)) {
                widget->set_h(m_height);
            }
        }
        m_screen_w = width;
        m_screen_h = height;
        m_scale =
            std::max(0.25f, std::min({scale, (width - 16) / m_width, (height - 16) / m_height}));
        m_preferred_scale = std::max(0.25f, scale);
        if (!m_positioned) {
            m_logical_x = (width - (m_width + 24) * m_scale) / m_preferred_scale;
            m_logical_y = 100 * m_scale / m_preferred_scale;
            m_positioned = true;
        }
        m_x = m_logical_x * m_preferred_scale;
        m_y = m_logical_y * m_preferred_scale;
        clamp_position();
    }

    bool PartyWindow::is_leader() const {
        return open() && m_local_player != 0 && m_state.leader == m_local_player;
    }

    bool PartyWindow::can_invite() const {
        return is_leader() && m_invite_target != 0 && m_invite_target != m_local_player &&
               m_state.members.size() < protocol::kPartyCapacity &&
               std::none_of(m_state.members.begin(),
                            m_state.members.end(),
                            [&](const auto& member) { return member.id == m_invite_target; });
    }

    void PartyWindow::toggle_actions(std::uint32_t target, std::string_view name) {
        if (!open() || m_state.invitation) {
            return;
        }
        m_actions_open = !m_actions_open;
        m_invite_target = target;
        m_invite_name = name;
        m_dragging = false;
    }

    void PartyWindow::close_actions() {
        m_actions_open = false;
        m_invite_target = 0;
        m_invite_name.clear();
    }

    UiVarMap PartyWindow::vars(const protocol::PartyMember* member) const {
        UiVarMap v;
        ui_fill_theme_vars(m_document, v);
        v["party.title"] = "Party";
        v["party.invitation_title"] = "Party invitation";
        v["party.inviter"] = m_state.inviter_name;
        v["party.invitation"] = "would like you to join their party.";
        v["party.is_leader"] = is_leader() ? "true" : "false";
        v["party.role"] = is_leader() ? "Party leader" : "Party member";
        v["party.count"] = std::to_string(m_state.members.size()) + " / 12 members";
        v["party.leave"] = "Leave party";
        v["party.remove"] = "Remove";
        v["party.invite_label"] = can_invite() ? "Invite: " + m_invite_name
                                               : (m_state.members.size() >= protocol::kPartyCapacity
                                                      ? "Party is full"
                                                      : "Select a player to invite");
        v["party.accept"] = "Accept";
        v["party.decline"] = "Decline";
        if (member) {
            v["party.member.name"] = (member->id == m_state.leader ? "* " : "") + member->name;
            v["party.member.hp_ratio"] = std::to_string(std::clamp(
                static_cast<float>(member->hp) / std::max(1, member->max_hp), 0.0f, 1.0f));
        }
        return v;
    }

    bool PartyWindow::contains(float x, float y, WidgetTools&) const {
        if (m_state.invitation || m_actions_open) {
            return true;
        }
        return open() &&
               (m_dragging || inside(x, y, m_x, m_y, m_width * m_scale, m_height * m_scale));
    }

    std::optional<protocol::PartyRequest> PartyWindow::handle(const engine::FrameInput& input,
                                                              WidgetTools& ui) {
        if (!input.mouse_left) {
            m_dragging = false;
        }
        if (m_dragging) {
            m_x = input.mouse_x - m_drag_x;
            m_y = input.mouse_y - m_drag_y;
            clamp_position();
            m_logical_x = m_x / m_preferred_scale;
            m_logical_y = m_y / m_preferred_scale;
            return std::nullopt;
        }
        if (!input.mouse_left_clicked) {
            return std::nullopt;
        }
        const auto hit_dialog = [&](const UiDocument& doc) {
            const auto rect = dialog_geometry(doc, m_screen_w, m_screen_h, m_preferred_scale);
            const auto values = vars();
            PartyViewHost host(values, can_invite());
            return UiView::hit(doc,
                               &ui,
                               input.mouse_x,
                               input.mouse_y,
                               rect.x,
                               rect.y,
                               rect.w,
                               rect.h,
                               rect.scale,
                               vars(),
                               &host,
                               true);
        };
        protocol::PartyRequest request;
        if (m_state.invitation) {
            const auto hit = hit_dialog(m_invitation_document);
            if (hit.action() != "party.accept" && hit.action() != "party.decline") {
                return std::nullopt;
            }
            request.command = protocol::PartyCommand::Reply;
            request.invitation = m_state.invitation;
            request.value = hit.action() == "party.accept" ? 1 : 0;
            return request;
        }
        if (m_actions_open) {
            const auto hit = hit_dialog(m_actions_document);
            if (hit.action() == "window.close" || hit.kind() == UiHitKind::Close) {
                close_actions();
            } else if (hit.action() == "party.leave" && open()) {
                request.command = protocol::PartyCommand::Leave;
                close_actions();
                return request;
            } else if (hit.action() == "party.invite" && can_invite()) {
                request.command = protocol::PartyCommand::Invite;
                request.target = m_invite_target;
                close_actions();
                return request;
            } else if (is_leader()) {
                const auto rect =
                    dialog_geometry(m_actions_document, m_screen_w, m_screen_h, m_preferred_scale);
                float row_y = rect.y + kActionRowsY * rect.scale;
                for (const auto& member : m_state.members) {
                    if (member.id == m_local_player) {
                        continue;
                    }
                    const auto values = vars(&member);
                    const auto row_hit = UiView::hit(m_action_row,
                                                     &ui,
                                                     input.mouse_x,
                                                     input.mouse_y,
                                                     rect.x,
                                                     row_y,
                                                     rect.w,
                                                     m_action_row.window_h() * rect.scale,
                                                     rect.scale,
                                                     values,
                                                     nullptr,
                                                     false);
                    if (row_hit.action() == "party.kick") {
                        request.command = protocol::PartyCommand::Kick;
                        request.target = member.id;
                        return request;
                    }
                    row_y += m_action_row.window_h() * rect.scale;
                }
            }
            return std::nullopt;
        }
        if (open() &&
            inside(input.mouse_x, input.mouse_y, m_x, m_y, m_width * m_scale, m_height * m_scale)) {
            m_dragging = true;
            m_drag_x = input.mouse_x - m_x;
            m_drag_y = input.mouse_y - m_y;
        }
        return std::nullopt;
    }

    void PartyWindow::draw(engine::IEngine& engine,
                           WidgetTools& ui,
                           const FreetypeFont& font,
                           const engine::FrameInput& input) const {
        const auto draw_document = [&](const UiDocument& doc,
                                       float x,
                                       float y,
                                       float w,
                                       float h,
                                       float scale,
                                       const UiVarMap& values) {
            PartyViewHost host(values, can_invite());
            const auto hit = UiView::hit(
                doc, &ui, input.mouse_x, input.mouse_y, x, y, w, h, scale, values, &host);
            host.set_pointer_hit(hit.widget_id(), input.mouse_left);
            UiView::draw(engine,
                         ui,
                         doc,
                         x,
                         y,
                         w,
                         h,
                         scale,
                         values,
                         font,
                         font,
                         &host,
                         doc.chrome_kind() == UiChromeKind::NineSlice);
        };
        if (open()) {
            draw_document(
                m_document, m_x, m_y, m_width * m_scale, m_height * m_scale, m_scale, vars());
            for (std::size_t i = 0; i < m_state.members.size(); ++i) {
                auto values = vars(&m_state.members[i]);
                auto& name = values["party.member.name"];
                const auto* label = m_row.find_widget("name");
                const float text_scale =
                    label && font.pixel_size() > 0
                        ? static_cast<float>(label->pixel_size()) / font.pixel_size()
                        : 1;
                fit_label(name, font, text_scale, m_width - 16);
                draw_document(
                    m_row,
                    m_x,
                    m_y + (m_document.window_h() + static_cast<float>(i) * m_row.window_h()) *
                              m_scale,
                    m_width * m_scale,
                    m_row.window_h() * m_scale,
                    m_scale,
                    values);
            }
        }
        if (m_actions_open || m_state.invitation) {
            const auto& doc = m_state.invitation ? m_invitation_document : m_actions_document;
            const auto rect = dialog_geometry(doc, m_screen_w, m_screen_h, m_preferred_scale);
            auto values = vars();
            fit_label(values["party.inviter"],
                      font,
                      16.0f / std::max(1, font.pixel_size()),
                      doc.window_w());
            fit_label(values["party.invite_label"],
                      font,
                      13.0f / std::max(1, font.pixel_size()),
                      doc.window_w() - 24);
            draw_document(doc, rect.x, rect.y, rect.w, rect.h, rect.scale, values);
            if (m_actions_open && !m_state.invitation && is_leader()) {
                float row_y = rect.y + kActionRowsY * rect.scale;
                for (const auto& member : m_state.members) {
                    if (member.id == m_local_player) {
                        continue;
                    }
                    auto row_values = vars(&member);
                    fit_label(row_values["party.member.name"],
                              font,
                              12.0f / std::max(1, font.pixel_size()),
                              200);
                    draw_document(m_action_row,
                                  rect.x,
                                  row_y,
                                  rect.w,
                                  m_action_row.window_h() * rect.scale,
                                  rect.scale,
                                  row_values);
                    row_y += m_action_row.window_h() * rect.scale;
                }
            }
        }
    }
}  // namespace openkal::client
