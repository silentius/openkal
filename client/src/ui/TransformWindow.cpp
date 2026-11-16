/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TransformWindow.h"

#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace openkal::client {
    namespace {

        [[nodiscard]] int animal_skill_for_slot(int race, int slot) {
            if (slot < 0 || slot > 2) {
                return 0;
            }
            if (race == 0) {
                return 104 + slot;
            }
            if (race == 4) {
                return 107 + slot;
            }
            if (race == 6) {
                return 101 + slot;
            }
            if (race == 8) {
                return 110 + slot;
            }
            return 0;
        }

        [[nodiscard]] int need_stage_for_animal(int skill) {
            if (!protocol::is_animal_skill(skill)) {
                return 1;
            }
            return ((skill - 101) % 3) + 1;
        }

        constexpr const char* kStageIconStems[] = {"sinsu001", "sinsu002", "sinsu003"};

        bool stage_button_rect(const UiDocument& doc,
                               float s,
                               int i,
                               float win_x,
                               float win_y,
                               float& ix,
                               float& iy,
                               float& iw,
                               float& ih) {
            const UiWidget* grid = doc.find_widget("stages");
            const float x0 = (grid ? grid->x() : 16.0f) * s;
            const float y0 = (grid ? grid->y() : 118.0f) * s;
            const float icon = (grid && grid->cell_w() > 0.0f ? grid->cell_w() : 32.0f) * s;
            const float pitch = (grid && grid->pitch_x() > 0.0f ? grid->pitch_x() : 40.0f) * s;
            if (i >= 0 && i < 3) {
                ix = win_x + x0 + static_cast<float>(i) * pitch;
                iy = win_y + y0;
                iw = icon;
                ih = icon;
                return true;
            }
            return false;
        }

        void draw_stage_frame(engine::IEngine* engine,
                              float x,
                              float y,
                              float w,
                              float h,
                              bool selected,
                              float a,
                              float s) {
            if (!engine) {
                return;
            }
            const float t = selected ? 2.0f * s : std::max(1.0f, s);
            const float r = selected ? 0.95f : 0.42f;
            const float g = selected ? 0.82f : 0.26f;
            const float b = selected ? 0.35f : 0.20f;
            const float ba = selected ? 1.0f : a * 0.5f;
            engine->draw_ui_rect(x, y, w, t, r, g, b, ba);
            engine->draw_ui_rect(x, y + h - t, w, t, r, g, b, ba);
            engine->draw_ui_rect(x, y, t, h, r, g, b, ba);
            engine->draw_ui_rect(x + w - t, y, t, h, r, g, b, ba);
        }

        bool animal_slot_rect(const UiDocument& doc,
                              float s,
                              int i,
                              float win_x,
                              float win_y,
                              float& ix,
                              float& iy,
                              float& iw,
                              float& ih) {
            const UiWidget* grid = doc.find_widget("animals");
            const float x0 = (grid ? grid->x() : 16.0f) * s;
            const float y0 = (grid ? grid->y() : 196.0f) * s;
            const float icon = (grid && grid->cell_w() > 0.0f ? grid->cell_w() : 32.0f) * s;
            const float pitch = (grid && grid->pitch_x() > 0.0f ? grid->pitch_x() : 44.0f) * s;
            ix = win_x + x0 + static_cast<float>(i) * pitch;
            iy = win_y + y0;
            iw = icon;
            ih = icon;
            return true;
        }

        bool egg_icon_rect(const UiDocument& doc,
                           float s,
                           float win_x,
                           float win_y,
                           float& ix,
                           float& iy,
                           float& iw,
                           float& ih) {
            const UiWidget* egg = doc.find_widget("egg");
            ix = win_x + (egg ? egg->x() : 16.0f) * s;
            iy = win_y + (egg ? egg->y() : 16.0f) * s;
            iw = (egg && egg->w() > 0.0f ? egg->w() : 40.0f) * s;
            ih = (egg && egg->h() > 0.0f ? egg->h() : 40.0f) * s;
            return true;
        }

        [[nodiscard]] std::string
        transform_egg_display_name(const ItemDef& def, int egg_level, const ShinsuDb* shinsu) {
            const int stage = protocol::max_transform_stage(egg_level);
            if (stage >= 1 && shinsu) {
                const std::string grown = shinsu->stage_name(def.transform_race(), stage);
                if (!grown.empty()) {
                    return grown;
                }
            }
            return def.name();
        }

        void draw_skill_hover_tip(engine::IEngine& engine,
                                  WidgetTools& ui,
                                  const FreetypeFont& font,
                                  const SkillDef& def,
                                  float cursor_x,
                                  float cursor_y,
                                  float screen_w,
                                  float screen_h,
                                  float scale) {
            const float s = std::max(0.25f, scale);
            const float fs = 1.0f;
            const float line_h = font.line_height() * fs + 2.0f;
            const float border = WidgetTools::overlay_border_thickness(s);
            const float pad = 6.0f + border;
            const float max_text_w = 280.0f;
            const std::string title = def.display_name();
            const std::string body = def.tooltip_body();

            std::vector<std::string> body_lines;
            {
                std::string cur;
                auto flush = [&]() {
                    body_lines.push_back(cur);
                    cur.clear();
                };
                for (std::size_t i = 0; i <= body.size(); ++i) {
                    const char ch = (i < body.size()) ? body[i] : '\n';
                    if (ch == '\n') {
                        flush();
                        continue;
                    }
                    cur.push_back(ch);
                    if (font.text_width(cur, fs) > max_text_w) {
                        cur.pop_back();
                        if (cur.empty()) {
                            cur.push_back(ch);
                            flush();
                        } else {
                            flush();
                            cur.push_back(ch);
                        }
                    }
                }
                if (!cur.empty()) {
                    flush();
                }
            }

            float content_w = font.text_width(title, fs);
            for (const auto& ln : body_lines) {
                content_w = std::max(content_w, font.text_width(ln, fs));
            }
            const float box_w = content_w + pad * 2.0f;
            const float box_h =
                pad * 2.0f + line_h * static_cast<float>(1 + static_cast<int>(body_lines.size()));
            float tip_x = 0.0f;
            float tip_y = 0.0f;
            ItemTooltip::place_near_cursor(
                cursor_x, cursor_y, box_w, box_h, screen_w, screen_h, tip_x, tip_y);
            ui.draw_overlay_panel(tip_x, tip_y, box_w, box_h, s);
            float ty = tip_y + pad;
            const float tx = tip_x + pad;
            font.draw(engine, tx, ty, title, 0.35f, 0.55f, 1.0f, fs);
            ty += line_h;
            for (const auto& ln : body_lines) {
                font.draw(engine, tx, ty, ln, 0.92f, 0.92f, 0.88f, fs);
                ty += line_h;
            }
        }

    }  // namespace

    class TransformWindow::ViewHost final : public UiViewHost {
      public:
        explicit ViewHost(const TransformWindow& window) : m_window(window) {}

        bool button_hovered(std::string_view id) const override {
            const float s = std::max(0.25f, m_window.m_scale);
            const UiHit hit = UiView::hit(m_window.m_v1,
                                          nullptr,
                                          m_window.m_hover_mx,
                                          m_window.m_hover_my,
                                          m_window.m_x,
                                          m_window.m_y,
                                          m_window.m_w,
                                          m_window.m_h,
                                          s,
                                          {});
            return hit.kind() == UiHitKind::Button && hit.widget_id() == id;
        }

        bool button_pressed(std::string_view id) const override {
            return m_window.m_mouse_down && button_hovered(id);
        }

      private:
        const TransformWindow& m_window;
    };

    void TransformWindow::set_egg(
        bool equipped, int item_index, int race, int level, int exp, int need, int form_stage) {
        m_equipped = equipped;
        m_item_index = item_index;
        m_race = race;
        m_level = std::max(1, level);
        m_exp = std::max(0, exp);
        m_need = std::max(0, need);
        m_form_stage = form_stage;
    }

    void TransformWindow::sync_from_layout() {
        UiView::reload(m_v1, m_layout);
        const float s = std::max(0.25f, m_scale);
        m_w = m_v1.window_w() * s;
        m_h = m_v1.window_h() * s;
    }

    void TransformWindow::sync_fonts(engine::IEngine& engine) {
        const int body_size = std::clamp(m_v1.font_pixel_size(), 8, 48);
        const UiWidget* title = m_v1.find_widget("title");
        const int title_size = title && title->pixel_size() > 0 ? title->pixel_size() : body_size;
        if (!m_body_font.ready() || m_body_font_pixel_size != body_size) {
            const std::string path = FreetypeFont::find_system_sans();
            m_body_font.destroy(engine);
            m_body_font_pixel_size = 0;
            if (!path.empty() && m_body_font.load(engine, path, body_size)) {
                m_body_font_pixel_size = body_size;
            }
        }
        if (!m_title_font.ready() || m_title_font.pixel_size() != title_size) {
            m_title_font.destroy(engine);
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), title_size);
        }
    }

    void TransformWindow::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_title_font.destroy(engine);
        m_body_font_pixel_size = 0;
    }

    void TransformWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        m_x = std::max(0.0f, (screen_w - m_w) * 0.5f);
        m_y = std::max(0.0f, (screen_h - m_h) * 0.42f);
    }

    bool TransformWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(
            mx, my, m_x, m_y, m_w, m_h, s, UiView::frame_style(m_v1));
    }

    void TransformWindow::update_pointer(const engine::FrameInput& in) {
        m_hover_kind = HoverKind::None;
        m_hover_index = -1;
        m_hover_mx = in.mouse_x;
        m_hover_my = in.mouse_y;
        m_mouse_down = in.mouse_left;
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            if (in.mouse_left) {
                m_x = in.mouse_x - m_drag_dx;
                m_y = in.mouse_y - m_drag_dy;
            } else {
                m_dragging = false;
            }
        }
        if (m_dragging || m_drag_skill_index >= 0 || !m_equipped) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const float mx = in.mouse_x;
        const float my = in.mouse_y;
        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            animal_slot_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih);
            if (WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih)) {
                m_hover_kind = HoverKind::Animal;
                m_hover_index = i;
                return;
            }
        }
        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            stage_button_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih);
            if (WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih)) {
                m_hover_kind = HoverKind::Stage;
                m_hover_index = i;
                return;
            }
        }
        {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            egg_icon_rect(m_v1, s, m_x, m_y, ix, iy, iw, ih);
            if (WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih)) {
                m_hover_kind = HoverKind::Egg;
                m_hover_index = 0;
            }
        }
    }

    TransformClickResult TransformWindow::handle_click(float mx, float my) {
        m_last_skill_index = -1;
        if (!m_open) {
            return TransformClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const WindowFrameStyle chrome = UiView::frame_style(m_v1);
        const UiHit hit = UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w, m_h, s, {});
        if (hit.kind() == UiHitKind::Close ||
            (hit.kind() == UiHitKind::Button && hit.action() == "window.close")) {
            m_open = false;
            m_dragging = false;
            return TransformClickResult::Consumed;
        }
        if (!WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w, m_h, s, chrome)) {
            return TransformClickResult::Ignored;
        }
        {
            float tx = 0, ty = 0, tw = 0, th = 0;
            WidgetTools::window_title_drag_rect(m_x, m_y, m_w, m_h, s, chrome, tx, ty, tw, th);
            if (WidgetTools::point_in_rect(mx, my, tx, ty, tw, th)) {
                m_dragging = true;
                m_drag_dx = mx - m_x;
                m_drag_dy = my - m_y;
                return TransformClickResult::Consumed;
            }
        }
        if (!m_equipped) {
            return TransformClickResult::Consumed;
        }
        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            stage_button_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih);
            if (!WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih)) {
                continue;
            }
            const int stage = i + 1;
            const int skill = protocol::transform_into_skill(stage);
            if (skill <= 0) {
                return TransformClickResult::Consumed;
            }
            m_last_skill_index = skill;
            m_drag_skill_index = skill;
            return TransformClickResult::DragBegin;
        }
        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            animal_slot_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih);
            if (!WidgetTools::point_in_rect(mx, my, ix, iy, iw, ih)) {
                continue;
            }
            const int skill = animal_skill_for_slot(m_race, i);
            if (skill == 0 || m_form_stage < need_stage_for_animal(skill)) {
                return TransformClickResult::Consumed;
            }
            m_drag_skill_index = skill;
            m_last_skill_index = skill;
            return TransformClickResult::DragBegin;
        }
        return TransformClickResult::Consumed;
    }

    void TransformWindow::draw(engine::IEngine& engine, WidgetTools& ui) const {
        if (!m_open) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        UiVarMap vars;
        vars["ui.ancient_animal"] = "Ancient animal";
        vars["ui.equip_egg_hint"] = "Equip a Shinsu egg in the sinsu slot.";
        vars["ui.transform"] = "Transform";
        vars["ui.animal_skills"] = "Animal skills";
        vars["stats.str_label"] = "STR";
        vars["stats.hth_label"] = "HTH";
        vars["stats.dex_label"] = "DEX";
        vars["stats.intl_label"] = "INT";
        vars["stats.wis_label"] = "WIS";
        vars["transform.equipped"] = m_equipped ? "true" : "false";
        vars["transform.empty"] = m_equipped ? "false" : "true";
        std::string name = "Ancient animal";
        if (m_form_stage > 0 && m_shinsu) {
            name = m_shinsu->stage_name(m_race, m_form_stage);
        } else {
            const int stage = protocol::max_transform_stage(m_level);
            if (stage >= 1 && m_shinsu) {
                name = m_shinsu->stage_name(m_race, stage);
            } else if (m_items && m_item_index > 0) {
                const ItemDef* egg = m_items->find(m_item_index);
                if (egg && !egg->name().empty()) {
                    name = egg->name();
                }
            }
        }
        if (name.empty()) {
            name = "Ancient animal";
        }
        vars["transform.name"] = name;
        vars["transform.level"] = std::to_string(m_level);
        char lv_line[64];
        std::snprintf(lv_line,
                      sizeof(lv_line),
                      "Lv. %d   %.2f%% EXP",
                      m_level,
                      protocol::transform_exp_percent(m_exp, m_need));
        vars["transform.level_line"] = lv_line;
        const auto bonus = protocol::worn_transform_bonus(m_race, m_level);
        vars["transform.str"] = std::to_string(bonus.str);
        vars["transform.hth"] = std::to_string(bonus.hth);
        vars["transform.dex"] = std::to_string(bonus.dex);
        vars["transform.intl"] = std::to_string(bonus.intl);
        vars["transform.wis"] = std::to_string(bonus.wis);
        ui_fill_theme_vars(m_v1, vars);

        const ViewHost host(*this);
        UiView::draw(
            engine, ui, m_v1, m_x, m_y, m_w, m_h, s, vars, m_body_font, m_title_font, &host, true);

        if (!m_equipped) {
            return;
        }

        if (m_items && m_item_index > 0) {
            const ItemDef* egg = m_items->find(m_item_index);
            if (egg) {
                std::string stem = egg->image();
                const int ex = protocol::transform_egg_image_ex_index(m_level);
                if (ex >= 0) {
                    const std::string alt = egg->image_ex(ex);
                    if (!alt.empty()) {
                        stem = alt;
                    }
                }
                const std::string icon = ui.resolve_bmp(stem);
                if (!icon.empty()) {
                    float ix = 0, iy = 0, iw = 0, ih = 0;
                    egg_icon_rect(m_v1, s, m_x, m_y, ix, iy, iw, ih);
                    ui.draw_tex(icon, ix, iy, iw, ih);
                }
            }
        }

        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            if (!stage_button_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih)) {
                continue;
            }
            const bool locked = !protocol::transform_stage_unlocked(m_level, i + 1);
            const bool selected = m_form_stage == i + 1;
            const float a = locked ? 0.35f : 1.0f;
            std::string stem = kStageIconStems[i];
            if (m_catalog) {
                const SkillDef* def = m_catalog->find_any(protocol::transform_into_skill(i + 1));
                if (def && !def->icon_stem().empty()) {
                    stem = def->icon_stem();
                }
            }
            const std::string icon = ui.resolve_bmp(stem);
            if (!icon.empty()) {
                ui.draw_tex(icon, ix, iy, iw, ih, a);
            } else if (ui.renderer()) {
                ui.renderer()->draw_ui_rect(ix, iy, iw, ih, 0.18f, 0.14f, 0.10f, a);
            }
            draw_stage_frame(ui.renderer(), ix, iy, iw, ih, selected, a, s);
        }

        for (int i = 0; i < 3; ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            animal_slot_rect(m_v1, s, i, m_x, m_y, ix, iy, iw, ih);
            const int skill = animal_skill_for_slot(m_race, i);
            const bool locked = m_form_stage < need_stage_for_animal(skill);
            const SkillDef* def = m_catalog ? m_catalog->find_any(skill) : nullptr;
            if (def) {
                const std::string icon = ui.resolve_bmp(def->icon_stem());
                if (!icon.empty()) {
                    ui.draw_tex(icon, ix, iy, iw, ih, locked ? 0.35f : 1.0f);
                    continue;
                }
            }
            if (ui.renderer()) {
                ui.renderer()->draw_ui_rect(
                    ix, iy, iw, ih, 0.2f, 0.16f, 0.12f, locked ? 0.35f : 1.0f);
            }
        }
    }

    void TransformWindow::draw_tooltip(engine::IEngine& engine,
                                       WidgetTools& ui,
                                       const FreetypeFont& font,
                                       float screen_w,
                                       float screen_h,
                                       int player_clazz,
                                       int player_level) const {
        if (!m_open || m_drag_skill_index >= 0 || !font.ready()) {
            return;
        }
        if (m_hover_kind == HoverKind::Egg && m_items && m_item_index > 0) {
            const ItemDef* def = m_items->find(m_item_index);
            if (!def) {
                return;
            }
            ItemTipExtras extras;
            extras.egg_level = m_level;
            extras.egg_exp = m_exp;
            extras.egg_name = transform_egg_display_name(*def, m_level, m_shinsu);
            const std::string markup = ItemTooltip::make_tip(
                *def, *m_items, 1, player_clazz, player_level, 0, -1, -1, extras);
            const auto lines = ItemTooltip::parse_markup(markup);
            ItemTooltip::draw(ui, engine, font, lines, m_hover_mx, m_hover_my, screen_w, screen_h);
            return;
        }
        if (!m_catalog) {
            return;
        }
        const SkillDef* def = nullptr;
        if (m_hover_kind == HoverKind::Stage && m_hover_index >= 0 && m_hover_index < 3) {
            def = m_catalog->find_any(protocol::transform_into_skill(m_hover_index + 1));
        } else if (m_hover_kind == HoverKind::Animal && m_hover_index >= 0 && m_hover_index < 3) {
            def = m_catalog->find_any(animal_skill_for_slot(m_race, m_hover_index));
        }
        if (!def) {
            return;
        }
        draw_skill_hover_tip(
            engine, ui, font, *def, m_hover_mx, m_hover_my, screen_w, screen_h, m_scale);
    }

    void TransformWindow::draw_drag_ghost(WidgetTools& ui, float mx, float my) const {
        if (m_drag_skill_index < 0 || !m_catalog) {
            return;
        }
        const SkillDef* def = m_catalog->find_any(m_drag_skill_index);
        if (!def) {
            return;
        }
        const std::string icon = ui.resolve_bmp(def->icon_stem());
        if (icon.empty()) {
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const float sz = 32.0f * s;
        ui.draw_tex(icon, mx - sz * 0.5f, my - sz * 0.5f, sz, sz, 0.85f);
    }

}  // namespace openkal::client
