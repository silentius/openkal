/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CharacterInfoWindow.h"

#include "openkal/client/UiView.h"
#include "openkal/server/Combat.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    namespace {
        void put_bool(UiVarMap& vars, const char* key, bool v) {
            vars[key] = v ? "true" : "false";
        }

        void put_i(UiVarMap& vars, const char* key, int v) {
            vars[key] = std::to_string(v);
        }

        [[nodiscard]] std::string format_primary_stat(int base, int pending, int equipment_bonus) {
            return std::to_string(base + pending) + " +" + std::to_string(equipment_bonus);
        }

        class CharInfoHost : public UiViewHost {
          public:
            CharInfoHost(const std::vector<BuffSlotEntry>* buffs,
                         bool apply_on,
                         std::string_view hovered = {},
                         bool pressed = false)
                : m_buffs(buffs), m_apply_on(apply_on), m_hovered(hovered), m_pressed(pressed) {}

            [[nodiscard]] bool button_hovered(std::string_view id) const override {
                return id == m_hovered;
            }

            [[nodiscard]] bool button_pressed(std::string_view id) const override {
                return m_pressed && id == m_hovered;
            }

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
                if (bind != "apply_item.buffs" || !m_buffs) {
                    return false;
                }
                if (index < 0 || index >= static_cast<int>(m_buffs->size())) {
                    out.set_empty(true);
                    return true;
                }
                const BuffSlotEntry& buff = (*m_buffs)[static_cast<std::size_t>(index)];
                if (buff.icon_file.empty()) {
                    out.set_empty(true);
                    return true;
                }
                out.set_file(buff.icon_file);
                out.set_empty(false);
                return true;
            }

            void set_primary_enabled(int index, bool spend, bool unspend) {
                m_can_spend[static_cast<std::size_t>(index)] = spend;
                m_can_unspend[static_cast<std::size_t>(index)] = unspend;
            }

            [[nodiscard]] bool widget_enabled(const UiWidget& widget) const override {
                const int index = ui_primary_index(widget.bind());
                if (widget.action() == "stats.spend") {
                    return index >= 0 && m_can_spend[static_cast<std::size_t>(index)];
                }
                if (widget.action() == "stats.unspend") {
                    return index >= 0 && m_can_unspend[static_cast<std::size_t>(index)];
                }
                return button_enabled(widget.action());
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                if (action == "stats.apply") {
                    return m_apply_on;
                }
                return true;
            }

          private:
            const std::vector<BuffSlotEntry>* m_buffs = nullptr;
            bool m_apply_on = true;
            std::string_view m_hovered;
            bool m_pressed = false;
            std::array<bool, 5> m_can_spend{};
            std::array<bool, 5> m_can_unspend{};
        };

    }  // namespace

    void CharacterInfoWindow::reload_document() {
        UiView::reload(m_v1, m_layout);
    }

    void CharacterInfoWindow::sync_from_layout() {
        reload_document();
        const float s = std::max(0.25f, m_scale);
        m_w = m_v1.window_w() * s;
        m_h = m_v1.window_h() * s;
    }

    void CharacterInfoWindow::sync_fonts(engine::IEngine& engine) {
        reload_document();
        int body_px = m_v1.font_pixel_size();
        int title_px = 12;
        bool bold = true;
        if (const UiWidget* title = m_v1.find_widget("title")) {
            if (title->pixel_size() > 0) {
                title_px = title->pixel_size();
            }
            bold = title->bold();
        }
        body_px = std::clamp(body_px, 8, 48);
        title_px = std::clamp(title_px, 8, 64);

        if (!m_body_font.ready() || m_body_font_pixel_size != body_px) {
            const std::string path = FreetypeFont::find_system_sans();
            m_body_font.destroy(engine);
            m_body_font_pixel_size = 0;
            if (!path.empty() && m_body_font.load(engine, path, body_px)) {
                m_body_font_pixel_size = body_px;
            }
        }
        if (!m_title_font.ready() || m_title_font_pixel_size != title_px ||
            m_title_font_bold != bold) {
            const std::string path =
                bold ? FreetypeFont::find_system_sans_bold() : FreetypeFont::find_system_sans();
            m_title_font.destroy(engine);
            m_title_font_pixel_size = 0;
            m_title_font_bold = bold;
            if (!path.empty() && m_title_font.load(engine, path, title_px)) {
                m_title_font_pixel_size = title_px;
            }
        }
    }

    void CharacterInfoWindow::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_body_font_pixel_size = 0;
        m_title_font.destroy(engine);
        m_title_font_pixel_size = 0;
        m_title_font_bold = false;
    }

    void CharacterInfoWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        WidgetTools::center_rect(m_x,
                                 m_y,
                                 m_w + (m_v1.pad_left() + m_v1.pad_right()) * s,
                                 m_h + (m_v1.pad_top() + m_v1.pad_bottom()) * s,
                                 screen_w,
                                 screen_h);
        m_x += m_v1.pad_left() * s;
        m_y += m_v1.pad_top() * s;
    }

    void CharacterInfoWindow::set_stats(const NetClient::CharInfoStats& stats) {
        m_stats = stats;
    }

    void CharacterInfoWindow::clear_pending() {
        m_pending.fill(0);
    }

    std::string CharacterInfoWindow::apply_buff_tooltip(float mx,
                                                        float my,
                                                        std::uint64_t now_ms,
                                                        WidgetTools* ui) const {
        if (!m_open) {
            return {};
        }
        const float s = std::max(0.25f, m_scale);
        const UiVarMap vars = make_vars();
        CharInfoHost host(&m_apply_buffs, pending_count() > 0);
        const UiHit hit = UiView::hit(m_v1, ui, mx, my, m_x, m_y, m_w, m_h, s, vars, &host, true);
        if (hit.kind() != UiHitKind::IconGridCell || hit.bind() != "apply_item.buffs" ||
            hit.index() < 0 || hit.index() >= static_cast<int>(m_apply_buffs.size())) {
            return {};
        }
        return BuffSlotBar::tooltip_text(m_apply_buffs[static_cast<std::size_t>(hit.index())],
                                         now_ms);
    }

    WindowFrameStyle CharacterInfoWindow::frame_style() const {
        return UiView::frame_style(m_v1);
    }

    UiVarMap CharacterInfoWindow::make_vars() const {
        UiVarMap vars;
        ui_fill_theme_vars(m_v1, vars);
        vars["ui.title"] = "Character Info";
        vars["ui.ok"] = "Apply";
        vars["ui.close"] = "Close";
        vars["ui.honor"] = "Honor";
        vars["ui.ancient_animal"] = "Ancient animal";
        vars["ui.point"] = "Point";
        vars["player.name"] = m_stats.name.empty() ? "-" : m_stats.name;
        vars["player.name_label"] = "Name";
        vars["player.job"] = m_stats.job.empty() ? "-" : m_stats.job;
        vars["player.job_label"] = "Job";
        vars["player.guild"] = m_stats.guild.empty() ? "-" : m_stats.guild;
        vars["player.guild_label"] = "Guild";
        vars["player.guild_pos"] = m_stats.guild_pos.empty() ? "-" : m_stats.guild_pos;
        vars["player.guild_pos_label"] = "Pos";
        vars["player.gov_pos"] = m_stats.gov_pos.empty() ? "-" : m_stats.gov_pos;
        vars["player.gov_pos_label"] = "G.P";
        vars["player.awaken"] = m_stats.awaken.empty() ? "Awaken -" : m_stats.awaken;
        put_i(vars, "player.level", m_stats.level);
        vars["player.level_label"] = "Level";
        put_i(vars, "player.ctb", m_stats.ctb);
        vars["player.ctb_label"] = "CTB";

        vars["stats.str"] =
            format_primary_stat(m_stats.str, m_pending[0], m_equipment_primary_bonuses[0]);
        vars["stats.str_label"] = "STR";
        put_bool(vars, "stats.str_pending", m_pending[0] != 0);
        vars["stats.hth"] =
            format_primary_stat(m_stats.hth, m_pending[1], m_equipment_primary_bonuses[1]);
        vars["stats.hth_label"] = "HTH";
        put_bool(vars, "stats.hth_pending", m_pending[1] != 0);
        vars["stats.intl"] =
            format_primary_stat(m_stats.intl, m_pending[2], m_equipment_primary_bonuses[2]);
        vars["stats.intl_label"] = "INT";
        put_bool(vars, "stats.intl_pending", m_pending[2] != 0);
        vars["stats.wis"] =
            format_primary_stat(m_stats.wis, m_pending[3], m_equipment_primary_bonuses[3]);
        vars["stats.wis_label"] = "WIS";
        put_bool(vars, "stats.wis_pending", m_pending[3] != 0);
        vars["stats.dex"] =
            format_primary_stat(m_stats.dex, m_pending[4], m_equipment_primary_bonuses[4]);
        vars["stats.dex_label"] = "AGI";
        put_bool(vars, "stats.dex_pending", m_pending[4] != 0);
        put_i(vars, "stats.pu_remaining", std::max(0, m_stats.pu_point - pending_point_cost()));
        vars["stats.pu_point_label"] = "Point";

        vars["vital.hp_label"] = "HP";
        vars["vital.mp_label"] = "MP";
        vars["vital.hp_text"] = std::to_string(m_stats.hp) + "/" + std::to_string(m_stats.max_hp);
        vars["vital.mp_text"] = std::to_string(m_stats.mp) + "/" + std::to_string(m_stats.max_mp);
        put_bool(vars, "vital.hp_pending", false);
        put_bool(vars, "vital.mp_pending", false);

        vars["combat.attack"] =
            std::to_string(m_stats.min_attack) + "~" + std::to_string(m_stats.max_attack);
        vars["combat.attack_label"] = "ATK";
        put_bool(vars, "combat.attack_pending", false);
        vars["combat.mattack"] =
            std::to_string(m_stats.min_magic) + "~" + std::to_string(m_stats.max_magic);
        vars["combat.mattack_label"] = "Mg.ATK";
        put_bool(vars, "combat.mattack_pending", false);
        put_i(vars, "combat.hit", m_stats.hit);
        vars["combat.hit_label"] = "OTP";
        put_bool(vars, "combat.hit_pending", false);
        put_i(vars, "combat.evasion", m_stats.dodge);
        vars["combat.evasion_label"] = "Eva";
        put_bool(vars, "combat.evasion_pending", false);
        put_i(vars, "combat.defense", m_stats.defense);
        vars["combat.defense_label"] = "DEF";
        put_i(vars, "combat.absorb", m_stats.absorb);
        vars["combat.absorb_label"] = "Absorb.(%)";

        put_i(vars, "resist.fire", m_stats.res_fire);
        vars["resist.fire_label"] = "Fire";
        put_i(vars, "resist.ice", m_stats.res_ice);
        vars["resist.ice_label"] = "Ice";
        put_i(vars, "resist.lightning", m_stats.res_lightning);
        vars["resist.lightning_label"] = "Lightning";
        put_i(vars, "resist.curse", m_stats.res_curse);
        vars["resist.curse_label"] = "Curse";
        put_i(vars, "resist.palsy", m_stats.res_palsy);
        vars["resist.palsy_label"] = "Non Elemental";

        vars["character.attributes"] = "Attributes";
        vars["character.combat"] = "Combat";
        vars["character.resistances"] = "Resistances";
        vars["character.active_effects"] = "Active Effects";
        return vars;
    }

    int CharacterInfoWindow::pending_count() const {
        int total = 0;
        for (int pending : m_pending) {
            total += pending;
        }
        return total;
    }

    std::int32_t CharacterInfoWindow::primary_stat_value(int index) const {
        switch (index) {
        case 0:
            return m_stats.str;
        case 1:
            return m_stats.hth;
        case 2:
            return m_stats.intl;
        case 3:
            return m_stats.wis;
        case 4:
            return m_stats.dex;
        default:
            return 0;
        }
    }

    int CharacterInfoWindow::pending_point_cost() const {
        int total = 0;
        for (int i = 0; i < static_cast<int>(m_pending.size()); ++i) {
            total += server::combat::pending_need_points(
                m_stats.clazz, i, primary_stat_value(i), m_pending[static_cast<std::size_t>(i)]);
        }
        return total;
    }

    int CharacterInfoWindow::next_point_cost(int index) const {
        if (index < 0 || index >= static_cast<int>(m_pending.size())) {
            return 0;
        }
        return server::combat::get_need_point(m_stats.clazz,
                                              index,
                                              primary_stat_value(index) +
                                                  m_pending[static_cast<std::size_t>(index)]);
    }

    bool CharacterInfoWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w, m_h, s, frame_style());
    }

    void CharacterInfoWindow::update_pointer(const engine::FrameInput& in) {
        m_hovered_button.clear();
        m_pointer_down = in.mouse_left;
        if (m_open && !m_dragging) {
            const auto hit = UiView::hit(
                m_v1, nullptr, in.mouse_x, in.mouse_y, m_x, m_y, m_w, m_h, m_scale, make_vars());
            if (hit.kind() == UiHitKind::Button) {
                m_hovered_button = hit.widget_id();
            }
        }
        if (!m_open) {
            m_dragging = false;
            return;
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

    CharacterInfoClickResult
    CharacterInfoWindow::handle_click(float mx, float my, WidgetTools* ui) {
        if (!m_open) {
            return CharacterInfoClickResult::Ignored;
        }
        reload_document();
        const float s = std::max(0.25f, m_scale);
        const UiVarMap vars = make_vars();
        CharInfoHost host(&m_apply_buffs, pending_count() > 0);
        const UiHit hit = UiView::hit(m_v1, ui, mx, my, m_x, m_y, m_w, m_h, s, vars, &host, true);
        if (hit.kind() == UiHitKind::None) {
            return CharacterInfoClickResult::Ignored;
        }
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            set_open(false);
            return CharacterInfoClickResult::Consumed;
        }
        if (hit.action() == "window.noop") {
            return CharacterInfoClickResult::Consumed;
        }
        if (hit.action() == "stats.spend") {
            const int i = ui_primary_index(hit.bind());
            const int cost = next_point_cost(i);
            if (i >= 0 && cost > 0 && m_pending[static_cast<std::size_t>(i)] < 255 &&
                m_stats.pu_point - pending_point_cost() >= cost) {
                m_pending[static_cast<std::size_t>(i)] += 1;
            }
            return CharacterInfoClickResult::Consumed;
        }
        if (hit.action() == "stats.unspend") {
            const int i = ui_primary_index(hit.bind());
            if (i >= 0 && m_pending[static_cast<std::size_t>(i)] > 0) {
                m_pending[static_cast<std::size_t>(i)] -= 1;
            }
            return CharacterInfoClickResult::Consumed;
        }
        if (hit.action() == "stats.apply") {
            bool any = false;
            for (int i = 0; i < 5; ++i) {
                const int pending = m_pending[static_cast<std::size_t>(i)];
                if (pending <= 0 || !m_apply_property ||
                    !m_apply_property(static_cast<std::uint8_t>(i),
                                      static_cast<std::uint8_t>(pending))) {
                    continue;
                }
                m_pending[static_cast<std::size_t>(i)] = 0;
                any = true;
            }
            return any ? CharacterInfoClickResult::ApplyPending
                       : CharacterInfoClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return CharacterInfoClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::Consume) {
            return CharacterInfoClickResult::Consumed;
        }
        return CharacterInfoClickResult::Consumed;
    }

    void CharacterInfoWindow::draw(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   const FreetypeFont& font) const {
        if (!m_open) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const UiVarMap vars = make_vars();
        const FreetypeFont& body = m_body_font.ready() ? m_body_font : font;
        const FreetypeFont& title = m_title_font.ready() ? m_title_font : body;
        CharInfoHost host(&m_apply_buffs, pending_count() > 0, m_hovered_button, m_pointer_down);
        const int remaining = m_stats.pu_point - pending_point_cost();
        for (int i = 0; i < static_cast<int>(m_pending.size()); ++i) {
            const int cost = next_point_cost(i);
            const int pending = m_pending[static_cast<std::size_t>(i)];
            host.set_primary_enabled(
                i, cost > 0 && remaining >= cost && pending < 255, pending > 0);
        }
        UiView::draw(engine, ui, m_v1, m_x, m_y, m_w, m_h, s, vars, body, title, &host, true);
    }

}  // namespace openkal::client
