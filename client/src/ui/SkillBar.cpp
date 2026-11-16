/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillBar.h"

#include "openkal/client/Inventory.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/UseItem.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace openkal::client {
    namespace {

        [[nodiscard]] bool item_is_refresh(const ItemDb* items, int item_index) {
            if (!items || item_index <= 0) {
                return false;
            }
            const ItemDef* def = items->find(item_index);
            return def && (def->refresh_hp() > 0 || def->refresh_mp() > 0);
        }

    }  // namespace

    void SkillBar::set_layout(const UiLayout* layout) {
        m_layout = layout;
        UiView::reload(m_document, layout);
    }

    void SkillBar::set_panel(float x, float y, float w, float h, float s) {
        m_panel_x = x;
        m_panel_y = y;
        m_panel_w = w;
        m_panel_h = h;
        m_scale = std::max(0.25f, s);
    }

    bool SkillBar::contains(float mx, float my) const {
        const float x = (mx - m_panel_x) / m_scale;
        const float y = (my - m_panel_y) / m_scale;
        return WidgetTools::point_in_rect(x, y, 124, 34, 544, 146) ||
               WidgetTools::point_in_rect(x, y, 0, 28, 136, 152) ||
               WidgetTools::point_in_rect(x, y, 656, 28, 136, 152);
    }

    bool SkillBar::active_rect(float& ix, float& iy, float& iw, float& ih) const {
        const UiDocument& doc = m_document;
        const float nbw = (m_document.window_w() > 0 ? m_document.window_w() : 792.0f);
        const float sx = m_panel_w / std::max(1.0f, nbw);
        const UiWidget* active = doc.find_widget("active");
        const float isz = (active && active->w() > 0.0f ? active->w() : 32.0f);
        iw = isz * sx;
        ih = (active && active->h() > 0.0f ? active->h() : isz) * sx;
        ix = m_panel_x + (active ? active->x() : 624.0f) * sx;
        iy = m_panel_y + (active ? active->y() : 52.0f) * sx;
        return true;
    }

    bool SkillBar::hotbar_rect(int slot, float& ix, float& iy, float& iw, float& ih) const {
        if (slot < 0 || slot >= static_cast<int>(PlayerSkills::kHotbarSlots)) {
            return false;
        }
        const UiDocument& doc = m_document;
        const float sx =
            m_panel_w /
            std::max(1.0f, (m_document.window_w() > 0 ? m_document.window_w() : 792.0f));
        const UiWidget* grid = doc.find_widget(slot < 10 ? "hotbar_base" : "hotbar_shift");
        const int index = slot % 10;
        const int cols = grid ? std::max(1, grid->cols()) : 10;
        iw = (grid ? grid->cell_w() : 32.0f) * sx;
        ih = (grid ? grid->cell_h() : 32.0f) * sx;
        ix = m_panel_x +
             ((grid ? grid->x() : 136.0f) + (index % cols) * (grid ? grid->pitch_x() : 42.0f)) * sx;
        iy = m_panel_y + ((grid ? grid->y() : (slot < 10 ? 106.0f : 52.0f)) +
                          (index / cols) * (grid ? grid->pitch_y() : 54.0f)) *
                             sx;
        return true;
    }

    bool SkillBar::auto_hp_rect(float& ix, float& iy, float& iw, float& ih) const {
        const auto* widget = m_document.find_widget("auto_hp");
        const float sx =
            m_panel_w / std::max(1.0f, m_document.window_w() > 0 ? m_document.window_w() : 792.0f);
        ix = m_panel_x + (widget ? widget->x() : 624.0f) * sx;
        iy = m_panel_y + (widget ? widget->y() : 106.0f) * sx;
        iw = (widget ? widget->w() : 32.0f) * sx;
        ih = (widget ? widget->h() : 32.0f) * sx;
        return true;
    }

    bool SkillBar::hit_auto_hp(float mx, float my) const {
        float x = 0, y = 0, w = 0, h = 0;
        if (!auto_hp_rect(x, y, w, h)) {
            return false;
        }
        const float pad = 4.0f * m_scale;
        return WidgetTools::point_in_rect(mx, my, x - pad, y - pad, w + 2 * pad, h + 2 * pad);
    }

    bool SkillBar::auto_hp_available() const {
        const ItemDef* item = m_items ? m_items->find(m_auto_hp_item_index) : nullptr;
        std::uint32_t iid = 0;
        int count = 0;
        return item && item->auto_hp() && item->refresh_hp() > 0 && m_bag &&
               m_bag->try_find_stack(m_auto_hp_item_index, iid, count);
    }

    void SkillBar::restore_auto_hp(int item_index, int percent) {
        const ItemDef* item = m_items ? m_items->find(item_index) : nullptr;
        m_auto_hp_item_index =
            item && item->auto_hp() && item->refresh_hp() > 0 ? item->index() : 0;
        m_auto_hp_percent =
            percent == 25 || percent == 50 || percent == 75 || percent == 100 ? percent : 75;
        m_auto_hp_click_age = -1.0f;
        m_auto_hp_dirty = false;
    }

    bool SkillBar::take_auto_hp_dirty() {
        const bool dirty = m_auto_hp_dirty;
        m_auto_hp_dirty = false;
        return dirty;
    }

    bool SkillBar::auto_hp_needed(std::int32_t hp, std::int32_t max_hp) const {
        return protocol::UseItem::auto_hp_eligible(hp, max_hp, m_auto_hp_percent) &&
               auto_hp_available() &&
               (m_item_refresh_cd <= 0.0f || (m_item_refresh_cd_index != m_auto_hp_item_index &&
                                              !item_is_refresh(m_items, m_item_refresh_cd_index)));
    }

    int SkillBar::hit_hotbar(float mx, float my) const {
        const float pad = 4.0f * m_scale;
        int best = -1;
        float best_d2 = 0.0f;
        for (int i = 0; i < static_cast<int>(PlayerSkills::kHotbarSlots); ++i) {
            float ix = 0, iy = 0, iw = 0, ih = 0;
            if (!hotbar_rect(i, ix, iy, iw, ih)) {
                continue;
            }
            if (!WidgetTools::point_in_rect(
                    mx, my, ix - pad, iy - pad, iw + pad * 2.0f, ih + pad * 2.0f)) {
                continue;
            }
            const float dx = mx - (ix + iw * 0.5f);
            const float dy = my - (iy + ih * 0.5f);
            const float d2 = dx * dx + dy * dy;
            if (best < 0 || d2 < best_d2) {
                best = i;
                best_d2 = d2;
            }
        }
        return best;
    }

    bool SkillBar::hit_active(float mx, float my) const {
        float ix = 0, iy = 0, iw = 0, ih = 0;
        if (!active_rect(ix, iy, iw, ih)) {
            return false;
        }
        const float pad = 4.0f * m_scale;
        return WidgetTools::point_in_rect(
            mx, my, ix - pad, iy - pad, iw + pad * 2.0f, ih + pad * 2.0f);
    }

    bool SkillBar::try_drop(float mx, float my, int skill_index) {
        if (!m_player || skill_index < 0) {
            return false;
        }
        const auto* skill = m_catalog ? m_catalog->find(m_player_clazz, skill_index) : nullptr;
        const bool social = skill && social_action(*skill) != SocialAction::None;
        if (skill && social_action(*skill) == SocialAction::Removed) {
            return false;
        }
        const ItemDef* item = m_items && protocol::shortcut_is_item(skill_index)
                                  ? m_items->find(protocol::shortcut_item_index(skill_index))
                                  : nullptr;
        if (hit_auto_hp(mx, my)) {
            if (!item || !item->auto_hp() || item->refresh_hp() <= 0) {
                return false;
            }
            m_auto_hp_dirty = m_auto_hp_dirty || m_auto_hp_item_index != item->index();
            m_auto_hp_item_index = item->index();
            m_auto_hp_click_age = -1.0f;
            return true;
        }
        if (item && item->auto_hp()) {
            return false;
        }
        const int slot = hit_hotbar(mx, my);
        if (slot >= 0) {
            m_player->set_hotbar(slot, skill_index);
            if (protocol::shortcut_is_skill(skill_index) &&
                !Catalog::skill_bar_casts_on_click(m_player_clazz, skill_index)) {
                m_player->set_active(skill_index);
            }
            return true;
        }
        if (hit_active(mx, my) && protocol::shortcut_is_skill(skill_index)) {
            if ((social && !social_action_uses_active_slot(social_action(*skill))) ||
                Catalog::skill_bar_casts_on_click(m_player_clazz, skill_index)) {
                return false;
            }
            m_player->set_active(skill_index);
            return true;
        }
        return false;
    }

    bool SkillBar::begin_press(float mx, float my) {
        if (!m_player || !m_catalog) {
            return false;
        }
        m_press_slot = -1;
        m_drag_slot = -1;
        m_drag_skill = -1;
        if (hit_auto_hp(mx, my)) {
            m_press_slot = kAutoHpSlot;
            m_press_mx = mx;
            m_press_my = my;
            m_drag_skill =
                m_auto_hp_item_index > 0 ? protocol::shortcut_pack_item(m_auto_hp_item_index) : -1;
            return true;
        }
        const int slot = hit_hotbar(mx, my);
        if (slot >= 0) {
            m_press_slot = slot;
            m_press_mx = mx;
            m_press_my = my;
            m_drag_skill = m_player->hotbar_slot(slot);
            return true;
        }
        if (hit_active(mx, my)) {
            m_press_slot = -2;
            m_press_mx = mx;
            m_press_my = my;
            m_drag_skill = -1;
            return true;
        }
        return false;
    }

    void SkillBar::update_pointer(const engine::FrameInput& in) {
        if (m_auto_hp_click_age >= 0.0f) {
            m_auto_hp_click_age += std::max(0.0f, in.dt);
            if (m_auto_hp_click_age > 0.35f) {
                m_auto_hp_click_age = -1.0f;
            }
        }
        m_hover_skill = -1;
        m_hover_mx = in.mouse_x;
        m_hover_my = in.mouse_y;
        if (m_player && m_drag_slot < 0 && (m_catalog || m_items)) {
            if (hit_auto_hp(in.mouse_x, in.mouse_y) && m_auto_hp_item_index > 0) {
                m_hover_skill = protocol::shortcut_pack_item(m_auto_hp_item_index);
            } else if (hit_active(in.mouse_x, in.mouse_y)) {
                m_hover_skill = m_player->active_index();
            } else {
                const int slot = hit_hotbar(in.mouse_x, in.mouse_y);
                if (slot >= 0) {
                    m_hover_skill = m_player->hotbar_slot(slot);
                }
            }
        }
        if (m_press_slot < 0 || m_drag_slot >= 0 || !in.mouse_left) {
            return;
        }
        if (m_press_slot > kAutoHpSlot || m_drag_skill < 0) {
            return;
        }
        const float dx = in.mouse_x - m_press_mx;
        const float dy = in.mouse_y - m_press_my;
        constexpr float kDragThreshold = 16.0f;
        if (dx * dx + dy * dy >= kDragThreshold * kDragThreshold) {
            m_drag_slot = m_press_slot;
            m_auto_hp_click_age = -1.0f;
            m_hover_skill = -1;
        }
    }

    BarResult SkillBar::activate_binding(int binding) {
        m_last_cast_index = -1;
        m_last_cast_kind = CastKind::None;
        m_last_item_index = -1;
        if (!m_player || !m_catalog) {
            return BarResult::Ignored;
        }
        if (protocol::shortcut_is_item(binding)) {
            m_last_item_index = protocol::shortcut_item_index(binding);
            const ItemDef* item = m_items ? m_items->find(m_last_item_index) : nullptr;
            if (item && item->auto_hp()) {
                return BarResult::Consumed;
            }
            return BarResult::UseItem;
        }
        if (protocol::shortcut_is_skill(binding)) {
            const auto* definition = m_catalog->find(m_player_clazz, binding);
            if (definition && social_action(*definition) != SocialAction::None) {
                m_last_cast_index = binding;
                if (social_action_uses_active_slot(social_action(*definition))) {
                    m_player->set_active(binding);
                    return BarResult::SetActive;
                }
                return BarResult::Cast;
            }
            m_last_cast_index = binding;
            m_last_cast_kind = Catalog::cast_kind_for(m_player_clazz, binding);
            if (Catalog::skill_bar_casts_on_click(m_player_clazz, binding)) {
                return BarResult::Cast;
            }
            if (Catalog::skill_bar_sets_active(m_last_cast_kind)) {
                m_player->set_active(binding);
                return BarResult::SetActive;
            }
            return BarResult::Cast;
        }
        return BarResult::Consumed;
    }

    BarResult SkillBar::handle_release(float mx, float my, bool over_inventory) {
        const int press = m_press_slot;
        const int drag = m_drag_slot;
        const int drag_skill = m_drag_skill;
        m_press_slot = -1;
        m_drag_slot = -1;
        m_drag_skill = -1;
        m_last_drop_iid = 0;
        m_last_drop_index = 0;
        m_last_drop_count = 0;
        if (press == kAutoHpSlot) {
            if (drag == kAutoHpSlot) {
                if (over_inventory || !contains(mx, my)) {
                    m_auto_hp_dirty = m_auto_hp_dirty || m_auto_hp_item_index != 0;
                    m_auto_hp_item_index = 0;
                }
            } else if (hit_auto_hp(mx, my) && m_auto_hp_item_index > 0) {
                if (m_auto_hp_click_age >= 0.0f) {
                    m_auto_hp_dirty = true;
                    m_auto_hp_percent = m_auto_hp_percent == 25    ? 100
                                        : m_auto_hp_percent == 100 ? 75
                                                                   : m_auto_hp_percent - 25;
                    m_auto_hp_click_age = -1.0f;
                } else {
                    m_auto_hp_click_age = 0.0f;
                }
            } else {
                m_auto_hp_click_age = -1.0f;
            }
            return BarResult::Consumed;
        }
        m_auto_hp_click_age = -1.0f;
        if (drag >= 0 && drag_skill >= 0 && m_player) {
            const int dest = hit_hotbar(mx, my);
            if (dest >= 0 && dest != drag) {
                const int dest_skill = m_player->hotbar_slot(dest);
                m_player->set_hotbar(dest, drag_skill);
                m_player->set_hotbar(drag, dest_skill);
                return BarResult::Consumed;
            }
            if (dest == drag) {
                return BarResult::Consumed;
            }
            if (hit_active(mx, my) && protocol::shortcut_is_skill(drag_skill)) {
                if (!Catalog::skill_bar_casts_on_click(m_player_clazz, drag_skill)) {
                    m_player->set_active(drag_skill);
                }
                return BarResult::Consumed;
            }
            if (contains(mx, my)) {
                return BarResult::Consumed;
            }
            if (protocol::shortcut_is_item(drag_skill)) {
                if (over_inventory) {
                    m_player->set_hotbar(drag, -1);
                    return BarResult::Consumed;
                }
                const int item_index = protocol::shortcut_item_index(drag_skill);
                if (m_items) {
                    const ItemDef* def = m_items->find(item_index);
                    if (def && def->is_quest()) {
                        m_player->set_hotbar(drag, -1);
                        return BarResult::Consumed;
                    }
                }
                m_last_drop_index = item_index;
                if (m_bag &&
                    m_bag->try_find_stack(item_index, m_last_drop_iid, m_last_drop_count)) {
                    m_player->set_hotbar(drag, -1);
                    return BarResult::DropItem;
                }
                m_player->set_hotbar(drag, -1);
                return BarResult::Consumed;
            }
            m_player->set_hotbar(drag, -1);
            return BarResult::Consumed;
        }
        if (press >= 0 && m_player) {
            return activate_binding(m_player->hotbar_slot(press));
        }
        if (press == -2) {
            return handle_click(mx, my);
        }
        return BarResult::Ignored;
    }

    BarResult SkillBar::handle_click(float mx, float my) {
        if (!m_player || !m_catalog) {
            return BarResult::Ignored;
        }
        const int slot = hit_hotbar(mx, my);
        if (slot >= 0) {
            return activate_binding(m_player->hotbar_slot(slot));
        }
        if (hit_active(mx, my) && m_player->active_index() >= 0) {
            return activate_binding(m_player->active_index());
        }
        return BarResult::Ignored;
    }

    void SkillBar::set_item_refresh_cooldown(float fraction, int item_index) {
        m_item_refresh_cd = std::clamp(fraction, 0.0f, 1.0f);
        if (m_item_refresh_cd <= 0.0f) {
            m_item_refresh_cd_index = -1;
        } else if (item_index > 0) {
            m_item_refresh_cd_index = item_index;
        }
    }

    bool SkillBar::item_shows_refresh_cd(int item_index) const {
        if (m_item_refresh_cd <= 0.05f || item_index <= 0 || m_item_refresh_cd_index <= 0) {
            return false;
        }
        if (item_index == m_item_refresh_cd_index) {
            return true;
        }
        return item_is_refresh(m_items, item_index) &&
               item_is_refresh(m_items, m_item_refresh_cd_index);
    }

    void SkillBar::set_mortal_points(int points) {
        m_mortal_points = std::clamp(points, 0, 5);
    }

    void SkillBar::draw(WidgetTools& ui, const FreetypeFont* count_font) const {
        if (!m_player) {
            return;
        }
        auto draw_binding = [&](int binding, float ix, float iy, float iw, float ih) {
            if (binding < 0) {
                return;
            }
            if (protocol::shortcut_is_item(binding)) {
                if (!m_items) {
                    return;
                }
                const int item_index = protocol::shortcut_item_index(binding);
                const int egg_level = m_bag ? m_bag->transform_level_for_index(item_index) : 1;
                const std::string icon = m_items->icon_filename(item_index, egg_level);
                if (icon.empty() || !ui.item_tex(icon)) {
                    return;
                }
                const int have = m_bag ? m_bag->count_item(item_index) : 0;
                const float alpha = item_index == m_auto_hp_item_index && have <= 0 ? 0.35f : 1.0f;
                ui.draw_item_tex(icon, ix, iy, iw, ih, alpha);
                ui.draw_stack_count(count_font, ix, iy, iw, ih, m_scale, have);
                if (item_shows_refresh_cd(item_index)) {
                    ui.draw_cooldown_sweep(ix, iy, std::min(iw, ih), m_item_refresh_cd);
                }
                return;
            }
            if (!m_catalog) {
                return;
            }
            const client::SkillDef* def = m_catalog->find(m_player_clazz, binding);
            if (!def) {
                def = m_catalog->find_any(binding);
            }
            if (!def) {
                return;
            }
            const std::string icon = ui.resolve_bmp(def->icon_stem());
            if (icon.empty() || !ui.tex(icon)) {
                return;
            }
            float a = 1.0f;
            const float cd = m_skill_window ? m_skill_window->cooldown_fraction(binding) : 0.0f;
            if (cd > 0.05f) {
                a = 0.7f;
            }
            if (m_player_clazz == 0 && binding == 17 && m_mortal_points == 0) {
                a *= 0.4f;
            }
            ui.draw_tex(icon, ix, iy, iw, ih, a);
            if (m_player_clazz == 0 && binding == 17) {
                ui.draw_stack_count(count_font, ix, iy, iw, ih, m_scale, m_mortal_points);
            }
            if (cd > 0.05f) {
                ui.draw_cooldown_sweep(ix, iy, std::min(iw, ih), cd);
            }
        };

        float ix = 0, iy = 0, iw = 0, ih = 0;
        if (active_rect(ix, iy, iw, ih)) {
            draw_binding(m_player->active_index(), ix, iy, iw, ih);
        }
        if (m_auto_hp_item_index > 0 && m_drag_slot != kAutoHpSlot &&
            auto_hp_rect(ix, iy, iw, ih)) {
            draw_binding(protocol::shortcut_pack_item(m_auto_hp_item_index), ix, iy, iw, ih);
        }

        for (int i = 0; i < static_cast<int>(PlayerSkills::kHotbarSlots); ++i) {
            if (m_drag_slot == i) {
                continue;
            }
            if (!hotbar_rect(i, ix, iy, iw, ih)) {
                continue;
            }
            draw_binding(m_player->hotbar_slot(i), ix, iy, iw, ih);
        }
    }

    void SkillBar::draw_drag_ghost(WidgetTools& ui,
                                   float mx,
                                   float my,
                                   const FreetypeFont* count_font) const {
        if (m_drag_slot < 0 || m_drag_skill < 0) {
            return;
        }
        std::string icon;
        int item_index = -1;
        if (protocol::shortcut_is_item(m_drag_skill)) {
            if (!m_items) {
                return;
            }
            item_index = protocol::shortcut_item_index(m_drag_skill);
            const int egg_level = m_bag ? m_bag->transform_level_for_index(item_index) : 1;
            icon = m_items->icon_filename(item_index, egg_level);
        } else if (m_catalog) {
            const client::SkillDef* def = m_catalog->find(m_player_clazz, m_drag_skill);
            if (!def) {
                def = m_catalog->find_any(m_drag_skill);
            }
            if (!def) {
                return;
            }
            icon = ui.resolve_bmp(def->icon_stem());
        }
        if (icon.empty()) {
            return;
        }
        const bool item_icon = item_index > 0;
        if (item_icon ? !ui.item_tex(icon) : !ui.tex(icon)) {
            return;
        }
        float ix = 0, iy = 0, iw = 0, ih = 0;
        if (!hotbar_rect(m_drag_slot, ix, iy, iw, ih)) {
            iw = 32.0f;
            ih = 32.0f;
        }
        const float gx = mx - iw * 0.5f;
        const float gy = my - ih * 0.5f;
        if (item_icon) {
            ui.draw_item_tex(icon, gx, gy, iw, ih, 0.85f);
        } else {
            ui.draw_tex(icon, gx, gy, iw, ih, 0.85f);
        }
        if (item_index > 0) {
            const int have = m_bag ? m_bag->count_item(item_index) : 0;
            ui.draw_stack_count(count_font, gx, gy, iw, ih, m_scale, have);
        }
    }

    void SkillBar::draw_tooltip(WidgetTools& ui,
                                const FreetypeFont& font,
                                float screen_w,
                                float screen_h) const {
        if (m_hover_skill < 0 || m_drag_slot >= 0 || !font.ready()) {
            return;
        }
        engine::IEngine* engine = ui.renderer();
        if (!engine) {
            return;
        }
        std::string title;
        std::string body;
        if (protocol::shortcut_is_item(m_hover_skill)) {
            if (!m_items) {
                return;
            }
            const ItemDef* item = m_items->find(protocol::shortcut_item_index(m_hover_skill));
            if (!item) {
                return;
            }
            title = item->name();
            if (item->auto_hp()) {
                body = item->description() + "\nAuto HP: below " +
                       std::to_string(m_auto_hp_percent) +
                       "%\nDouble-click to cycle: 75 / 50 / 25 / 100%."
                       "\nDrag to the inventory to remove.";
            }
        } else {
            if (!m_catalog) {
                return;
            }
            const SkillDef* def = m_catalog->find(m_player_clazz, m_hover_skill);
            if (!def) {
                def = m_catalog->find_any(m_hover_skill);
            }
            if (!def) {
                return;
            }
            title = def->display_name();
            body = def->tooltip_body();
            const int learned = m_player ? m_player->level_of(m_hover_skill) : 0;
            const std::string ability = def->format_ability(std::max(1, learned), m_ability_vars);
            if (!ability.empty()) {
                if (!body.empty()) {
                    body.push_back('\n');
                }
                body += ability;
            }
            if (def->max_level() > 0) {
                if (!body.empty()) {
                    body.push_back('\n');
                }
                body += "Lv " + std::to_string(learned) + " / " + std::to_string(def->max_level());
            }
        }

        const float s = std::max(0.5f, m_scale);
        // Match ItemTooltip: draw at tip_font native size (no UI-scale multiply).
        const float fs = 1.0f;
        const float line_h = font.line_height() * fs + 2.0f;
        const float border = WidgetTools::overlay_border_thickness(s);
        const float pad = 6.0f + border;
        const float max_text_w = 280.0f;

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
            while (!body_lines.empty() && body_lines.back().empty()) {
                body_lines.pop_back();
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
            m_hover_mx, m_hover_my, box_w, box_h, screen_w, screen_h, tip_x, tip_y);

        ui.draw_overlay_panel(tip_x, tip_y, box_w, box_h, s);

        float ty = tip_y + pad;
        const float tx = tip_x + pad;
        font.draw(*engine, tx, ty, title, 0.35f, 0.55f, 1.0f, fs);
        ty += line_h;
        for (const auto& ln : body_lines) {
            font.draw(*engine, tx, ty, ln, 0.92f, 0.92f, 0.88f, fs);
            ty += line_h;
        }
    }

}  // namespace openkal::client
