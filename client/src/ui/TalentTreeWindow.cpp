/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TalentTreeWindow.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    namespace {
        const char* class_icons[] = {"main-character-icon(musa).bmp",
                                     "main-character-icon(soolsa).bmp",
                                     "main-character-icon(koongsu).bmp",
                                     "main-character-icon(dojuk).bmp"};
        const char* class_names[] = {"Knight", "Magician", "Archer", "Thief"};
        const char* icon_for_node(int i) {
            if (i < 5) {
                return "talents/physical.bmp";
            }
            if (i < 10) {
                return "talents/explosive.bmp";
            }
            if (i < 15) {
                return "talents/magical.bmp";
            }
            if (i < 20) {
                return "talents/explosive.bmp";
            }
            if (i < 23) {
                return "talents/defense.bmp";
            }
            if (i < 25) {
                return "talents/auto_hp.bmp";
            }
            if (i < 29) {
                return "talents/auto_mp.bmp";
            }
            if (i < 31) {
                return "talents/auto_pick.bmp";
            }
            if (i < 36) {
                return "talents/speed.bmp";
            }
            return "talents/beacon.bmp";
        }
        int node_from_bind(std::string_view bind) {
            constexpr std::string_view prefix = "talent.node.";
            if (bind.substr(0, prefix.size()) != prefix) {
                return -1;
            }
            int i = 0;
            const auto suffix = bind.substr(prefix.size());
            if (suffix.empty()) {
                return -1;
            }
            for (char c : suffix) {
                if (c < '0' || c > '9') {
                    return -1;
                }
                i = i * 10 + c - '0';
                if (i >= protocol::kTalentNodeCount) {
                    return -1;
                }
            }
            return i;
        }
    }  // namespace

    class TalentTreeWindow::Host final : public UiViewHost {
      public:
        explicit Host(const TalentTreeWindow& window) : m_window(window) {
            set_pointer_hit(window.m_hover_widget, window.m_pointer_pressed);
        }
        bool resolve_slot(std::string_view bind, int, UiSlotIcon& out) const override {
            if (bind == "talent.class") {
                out.set_file(class_icons[m_window.m_class]);
                out.set_empty(false);
                return true;
            }
            if (bind == "talent.future") {
                out.set_file("talents/locked.bmp");
                out.set_grayscale(true);
                out.set_empty(false);
                return true;
            }
            const int i = node_from_bind(bind);
            if (i < 0) {
                return false;
            }
            out.set_file(icon_for_node(i));
            out.set_empty(false);
            out.set_grayscale((m_window.m_state.mask & protocol::talent_bit(i)) == 0);
            return true;
        }
        bool button_enabled(std::string_view action) const override {
            return m_window.button_enabled(action);
        }

      private:
        const TalentTreeWindow& m_window;
    };

    void TalentTreeWindow::set_layout(const UiLayout* layout) {
        UiView::reload(m_doc, layout);
    }
    void TalentTreeWindow::set_class(int clazz) {
        m_class = std::clamp(clazz, 0, 3);
    }
    void TalentTreeWindow::open() {
        m_open = true;
        m_draft = m_state.mask;
        m_feedback.clear();
        m_hover = -1;
    }
    void TalentTreeWindow::close() {
        m_open = false;
        m_hover = -1;
        m_draft = m_state.mask;
    }
    void TalentTreeWindow::receive(const protocol::TalentState& state) {
        const bool was_pending = m_pending;
        const bool changed = !m_have_state || state.mask != m_state.mask;
        m_state = state;
        m_have_state = true;
        if (changed || (m_pending && state.status == protocol::TalentStatus::Ok)) {
            m_draft = state.mask;
        }
        m_pending = false;
        switch (state.status) {
        case protocol::TalentStatus::Ok:
            m_feedback = was_pending
                             ? (state.beacon_active_ms > 0 ? "Beacon active."
                                                           : "Selection saved. Bonuses are active.")
                             : "";
            break;
        case protocol::TalentStatus::Invalid:
            m_feedback = "Selection rejected: check points and prerequisites.";
            break;
        case protocol::TalentStatus::Stale:
            m_draft = state.mask;
            m_feedback = "Selection refreshed. Please choose again.";
            break;
        case protocol::TalentStatus::SaveFailed:
            m_feedback = "Save failed. Your points were not spent. Retry Apply.";
            break;
        case protocol::TalentStatus::Unavailable:
            m_feedback = "Beacon is unavailable or still cooling down.";
            break;
        }
        if (!protocol::valid_talents(m_draft, state.level)) {
            m_draft = state.mask;
        }
    }
    void TalentTreeWindow::request_sent(bool sent) {
        m_pending = sent;
        m_feedback = sent ? "Waiting for server confirmation..."
                          : "Request could not be sent. Reconnect and try again.";
    }
    void TalentTreeWindow::tick(float dt) {
        m_timer_fraction_ms += std::clamp(dt * 1000.0f, 0.0f, 120000.0f);
        const auto elapsed = static_cast<std::uint32_t>(m_timer_fraction_ms);
        m_timer_fraction_ms -= static_cast<float>(elapsed);
        m_state.beacon_cooldown_ms -= std::min(elapsed, m_state.beacon_cooldown_ms);
        m_state.beacon_active_ms -= std::min(elapsed, m_state.beacon_active_ms);
    }
    bool TalentTreeWindow::toggle_node(int node) {
        if (!m_have_state || m_pending || node < 0 || node >= protocol::kTalentNodeCount ||
            (m_state.mask & protocol::talent_bit(node))) {
            return false;
        }
        if (m_draft & protocol::talent_bit(node)) {
            // Removing a staged prerequisite also removes every staged descendant in its path.
            const auto path = protocol::talent_nodes()[node].path;
            for (int i = node;
                 i < protocol::kTalentNodeCount && protocol::talent_nodes()[i].path == path;
                 ++i) {
                m_draft &= ~protocol::talent_bit(i);
            }
            m_feedback.clear();
            return true;
        }
        const auto next = m_draft | protocol::talent_bit(node);
        if (!protocol::can_apply_talents(m_state.mask, next, m_state.level)) {
            m_feedback = "Requires the previous node and enough available points.";
            return false;
        }
        m_draft = next;
        m_feedback.clear();
        return true;
    }
    bool TalentTreeWindow::button_enabled(std::string_view action) const {
        if (action == "window.close") {
            return true;
        }
        if (!m_have_state || m_pending) {
            return false;
        }
        if (action == "talent.apply") {
            return m_draft != m_state.mask &&
                   protocol::can_apply_talents(m_state.mask, m_draft, m_state.level);
        }
        if (action == "talent.beacon") {
            return protocol::talent_bonuses(m_state.mask).beacon && m_state.beacon_cooldown_ms == 0;
        }
        return true;
    }
    void
    TalentTreeWindow::geometry(float width, float height, float& x, float& y, float& scale) const {
        scale = std::max(0.1f, std::min(width / 1600.0f, height / 900.0f));
        x = (width - 1600 * scale) * 0.5f;
        y = (height - 900 * scale) * 0.5f;
    }
    TalentTreeWindow::Action TalentTreeWindow::update(const engine::FrameInput& input,
                                                      WidgetTools& ui) {
        if (!m_open) {
            return Action::None;
        }
        if (input.key_escape_pressed) {
            close();
            return Action::Exit;
        }
        float x, y, s;
        geometry(static_cast<float>(input.width), static_cast<float>(input.height), x, y, s);
        const Host host(*this);
        const auto hit = UiView::hit(m_doc,
                                     &ui,
                                     input.mouse_x,
                                     input.mouse_y,
                                     x,
                                     y,
                                     1600 * s,
                                     900 * s,
                                     s,
                                     variables(),
                                     &host,
                                     false);
        m_hover = node_from_bind(hit.bind());
        m_hover_widget = hit.widget_id();
        m_pointer_pressed = input.mouse_left;
        if (!input.mouse_left_clicked) {
            return Action::None;
        }
        if (m_hover >= 0) {
            (void) toggle_node(m_hover);
            return Action::None;
        }
        if (!button_enabled(hit.action())) {
            return Action::None;
        }
        if (hit.action() == "window.close") {
            close();
            return Action::Exit;
        }
        if (hit.action() == "talent.apply") {
            return Action::Apply;
        }
        if (hit.action() == "talent.beacon") {
            return Action::Beacon;
        }
        return Action::None;
    }
    UiVarMap TalentTreeWindow::variables() const {
        UiVarMap vars;
        vars["talent.title"] = "Talent Tree";
        vars["talent.points"] =
            m_have_state
                ? "Available: " + std::to_string(m_state.level - protocol::talent_cost(m_draft)) +
                      "   |   Spent: " + std::to_string(protocol::talent_cost(m_state.mask)) +
                      "   |   Pending: " +
                      std::to_string(protocol::talent_cost(m_draft) -
                                     protocol::talent_cost(m_state.mask))
                : "Waiting for server...";
        vars["talent.physical_label"] = "Offensive / Physical";
        vars["talent.magical_label"] = "Offensive / Magical";
        vars["talent.defensive_label"] = "Defensive";
        vars["talent.qol_label"] = "Quality of Life";
        vars["talent.upgrades_label"] = "Upgrades";
        vars["talent.class_name"] = class_names[m_class];
        vars["talent.future_label"] = "Class Updates";
        vars["talent.future_hint"] = "TBD / Unavailable";
        vars["talent.apply"] = m_pending ? "Waiting..." : "Apply";
        vars["talent.exit"] = "Exit";
        vars["talent.beacon"] =
            m_state.beacon_active_ms > 0
                ? "Beacon active: " + std::to_string((m_state.beacon_active_ms + 999) / 1000) + "s"
                : (m_state.beacon_cooldown_ms > 0
                       ? "Beacon: " + std::to_string((m_state.beacon_cooldown_ms + 999) / 1000) +
                             "s"
                       : "Activate Beacon");
        vars["talent.status"] = m_feedback;
        vars["talent.overlay_visible"] = m_hover >= 0 ? "true" : "false";
        if (m_hover >= 0) {
            const auto& node = protocol::talent_nodes()[m_hover];
            const auto& bonus = node.bonus;
            const int available = m_state.level - protocol::talent_cost(m_draft);
            vars["talent.detail_title"] = node.name + " " + std::to_string(node.ordinal + 1);
            vars["talent.detail_cost"] = std::to_string(node.cost) + " talent points  /  " +
                                         std::to_string(available) + " available";
            std::string effects;
            const auto add = [&](const std::string& label, int value, const char* suffix = "") {
                if (value != 0) {
                    effects += label + " " + (value > 0 ? "+" : "") + std::to_string(value) +
                               suffix + "\n";
                }
            };
            add("Min Physical", bonus.physical);
            add("Max Physical", bonus.physical + bonus.max_physical);
            add("Min Magical", bonus.magical);
            add("Max Magical", bonus.magical);
            add("Defense", bonus.defense);
            add("Mana", bonus.mana);
            add("Mana", bonus.mana_percent, "%");
            add("Explosive Blow power", bonus.explosive_percent, "%");
            add("Auto HP", bonus.hp_per_pulse, " every 2 seconds");
            add("Auto MP", bonus.mp_per_pulse, " every 2 seconds");
            add("Auto Pick", bonus.pickup_count, " items every 5 seconds");
            add("Speed", bonus.speed_percent, "%");
            add("Evasion", bonus.evasion);
            if (bonus.beacon) {
                effects = "Attracts idle monsters within 20 tiles.\nDuration: 30 "
                          "seconds\nCooldown: 120 seconds\nDoes not steal another player's aggro.";
            }
            if (!effects.empty() && effects.back() == '\n') {
                effects.pop_back();
            }
            vars["talent.detail"] = effects;
            const bool committed = (m_state.mask & protocol::talent_bit(m_hover)) != 0;
            const bool staged = (m_draft & protocol::talent_bit(m_hover)) != 0;
            const bool previous =
                node.prerequisite < 0 || (m_draft & protocol::talent_bit(node.prerequisite)) != 0;
            vars["talent.detail_requirement"] =
                node.prerequisite < 0
                    ? "Starts a new path."
                    : "Requires: " + protocol::talent_nodes()[node.prerequisite].name + " " +
                          std::to_string(protocol::talent_nodes()[node.prerequisite].ordinal + 1);
            vars["talent.detail_extra"] =
                committed       ? "Active / committed"
                : staged        ? "Pending / Apply to activate"
                : !m_have_state ? "Waiting for server..."
                : !previous     ? "Unlock the previous node first."
                : available < node.cost
                    ? "Need " + std::to_string(node.cost - available) + " more points."
                    : "Click to select this node.";
        }
        for (int i = 0; i < protocol::kTalentNodeCount; ++i) {
            const auto prefix = "talent.node." + std::to_string(i);
            const auto bit = protocol::talent_bit(i);
            const bool committed = (m_state.mask & bit) != 0;
            const bool staged = (m_draft & bit) != 0;
            const bool available =
                !staged && protocol::can_apply_talents(m_state.mask, m_draft | bit, m_state.level);
            vars[prefix + ".color"] = committed   ? "#76AB8B"
                                      : staged    ? "#E3BC6D"
                                      : available ? "#A08B63"
                                                  : "#3E454F";
            vars[prefix + ".line"] = committed ? "#527760" : staged ? "#A68A50" : "#343B46";
            vars[prefix + ".cost"] = std::to_string(protocol::talent_nodes()[i].ordinal + 1) +
                                     " / " + std::to_string(protocol::talent_nodes()[i].cost) +
                                     " pt";
        }
        ui_fill_theme_vars(m_doc, vars);
        return vars;
    }
    void TalentTreeWindow::draw_node_overlay(engine::IEngine& engine,
                                             WidgetTools& ui,
                                             float width,
                                             float height,
                                             float x,
                                             float y,
                                             float scale,
                                             const UiVarMap& vars) const {
        if (m_hover < 0 || !m_overlay_font.ready() || !m_overlay_title_font.ready()) {
            return;
        }
        const auto* widget = m_doc.find_widget("node_" + std::to_string(m_hover));
        if (!widget) {
            return;
        }
        float nx, ny, nw, nh;
        UiView::widget_rect(*widget, &ui, x, y, 1600 * scale, 900 * scale, scale, nx, ny, nw, nh);
        const float padding = 16;
        const float margin = 12;
        const float panel_width = std::min(std::max(340.0f, 390.0f * scale), width - 2 * margin);
        const float text_width = panel_width - 2 * padding;
        const auto title =
            wrap_text(m_overlay_title_font, vars.at("talent.detail_title"), text_width, 1);
        const auto cost = wrap_text(m_overlay_font, vars.at("talent.detail_cost"), text_width, 1);
        const auto effects = wrap_text(m_overlay_font, vars.at("talent.detail"), text_width, 1);
        const auto requirement =
            wrap_text(m_overlay_font, vars.at("talent.detail_requirement"), text_width, 1);
        const auto status =
            wrap_text(m_overlay_font, vars.at("talent.detail_extra"), text_width, 1);
        const float line_height = std::ceil(m_overlay_font.line_height()) + 3;
        const float title_height = std::ceil(m_overlay_title_font.line_height()) + 4;
        const float panel_height =
            2 * padding + title_height * title.size() +
            line_height * (cost.size() + effects.size() + requirement.size() + status.size()) + 22;
        float left = nx + nw + margin;
        if (left + panel_width > width - margin) {
            left = nx - margin - panel_width;
        }
        left = std::round(std::clamp(left, margin, std::max(margin, width - margin - panel_width)));
        const auto* footer = m_doc.find_widget("footer_line");
        const float bottom = footer ? std::min(height, y + footer->y() * scale) : height;
        const float top = std::round(
            std::clamp(ny - 8, margin, std::max(margin, bottom - margin - panel_height)));
        // Use the existing tooltip chrome over an opaque card so branches cannot bleed through.
        engine.draw_ui_rect(left, top, panel_width, panel_height, .43f, .36f, .24f, 1);
        engine.draw_ui_rect(
            left + 1, top + 1, panel_width - 2, panel_height - 2, .07f, .085f, .11f, 1);
        ui.draw_overlay_panel(left, top, panel_width, panel_height, std::max(.75f, scale));
        float ty = top + padding;
        const float tx = left + padding;
        for (const auto& line : title) {
            m_overlay_title_font.draw(engine, tx, ty, line, .94f, .82f, .59f);
            ty += title_height;
        }
        const auto draw_lines =
            [&](const std::vector<std::string>& lines, float r, float g, float b) {
                for (const auto& line : lines) {
                    m_overlay_font.draw(engine, tx, ty, line, r, g, b);
                    ty += line_height;
                }
            };
        draw_lines(cost, .72f, .73f, .76f);
        ty += 8;
        engine.draw_ui_rect(tx, ty - 3, text_width, 1, .30f, .27f, .21f, 1);
        for (const auto& line : effects) {
            const bool penalty = line.find(" -") != std::string::npos;
            const bool bonus = line.find(" +") != std::string::npos;
            m_overlay_font.draw(engine,
                                tx,
                                ty,
                                line,
                                penalty ? .96f
                                : bonus ? .61f
                                        : .85f,
                                penalty ? .53f
                                : bonus ? .84f
                                        : .86f,
                                penalty ? .48f
                                : bonus ? .65f
                                        : .88f);
            ty += line_height;
        }
        ty += 8;
        draw_lines(requirement, .65f, .68f, .72f);
        ty += 6;
        draw_lines(status, .89f, .77f, .52f);
    }

    void
    TalentTreeWindow::draw(engine::IEngine& engine, WidgetTools& ui, float width, float height) {
        if (!m_open) {
            return;
        }
        float x, y, s;
        geometry(width, height, x, y, s);
        const auto* heading = m_doc.find_widget("title");
        const int heading_size = heading && heading->pixel_size() > 0 ? heading->pixel_size() : 26;
        const auto load_font = [&](FreetypeFont& font, int pixels, bool bold) {
            if (!font.ready() || font.pixel_size() != pixels) {
                (void) font.load(engine,
                                 bold ? FreetypeFont::find_system_sans_bold()
                                      : FreetypeFont::find_system_sans(),
                                 pixels);
            }
        };
        load_font(m_font, std::clamp(static_cast<int>(std::lround(14 * s)), 10, 28), false);
        load_font(m_title_font,
                  std::clamp(static_cast<int>(std::lround(heading_size * s)), 18, 64),
                  true);
        load_font(m_overlay_font, std::clamp(static_cast<int>(std::lround(16 * s)), 14, 24), false);
        load_font(
            m_overlay_title_font, std::clamp(static_cast<int>(std::lround(18 * s)), 16, 28), true);
        engine.draw_ui_rect(0, 0, width, height, 0.063f, 0.082f, 0.11f, 1);
        const Host host(*this);
        const auto vars = variables();
        UiView::draw_widgets(engine,
                             ui,
                             m_doc,
                             x,
                             y,
                             1600 * s,
                             900 * s,
                             s,
                             vars,
                             m_font,
                             m_title_font,
                             &host,
                             UiView::WidgetDrawPass::Content);
        if (heading && heading->visible() && ui_eval_condition(heading->visible_when(), vars)) {
            const auto text = ui_interpolate(heading->text(), vars);
            float hx = heading->x_from_right() >= 0 ? x + 1600 * s - heading->x_from_right() * s
                                                    : x + heading->x() * s;
            if (heading->align() == "center") {
                hx -= m_title_font.text_width(text) * .5f;
            } else if (heading->align() == "right") {
                hx -= m_title_font.text_width(text);
            }
            float r = .9f, g = .81f, b = .62f;
            (void) ui_parse_hex_color(ui_interpolate(heading->color(), vars), r, g, b);
            // Draw from an atlas rasterized at the target size, aligned to physical pixels.
            const float hy = heading->y_from_bottom() >= 0
                                 ? y + 900 * s - heading->y_from_bottom() * s
                                 : y + heading->y() * s;
            m_title_font.draw(engine, std::round(hx), std::round(hy), text, r, g, b);
        }
        draw_node_overlay(engine, ui, width, height, x, y, s, vars);
    }
    void TalentTreeWindow::destroy(engine::IEngine& engine) {
        m_font.destroy(engine);
        m_title_font.destroy(engine);
        m_overlay_font.destroy(engine);
        m_overlay_title_font.destroy(engine);
    }
}  // namespace openkal::client
