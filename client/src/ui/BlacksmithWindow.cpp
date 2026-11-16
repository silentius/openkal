/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BlacksmithWindow.h"

#include "openkal/client/UiView.h"
#include "openkal/server/PrefixEnchant.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    namespace {

        [[nodiscard]] const ItemStack* find_iid(const Bag& bag, std::uint32_t iid) {
            if (iid == 0) {
                return nullptr;
            }
            for (const auto& slot : bag.normal) {
                if (slot.iid == iid && slot.count > 0) {
                    return &slot;
                }
            }
            return nullptr;
        }

    }  // namespace

    void BlacksmithWindow::reload_document() const {
        UiView::reload(m_v1, m_layout);
    }

    void BlacksmithWindow::sync_from_layout() {
        reload_document();
        if (m_v1.window_w() > 0.0f) {
            m_w = m_v1.window_w();
        }
        if (m_v1.window_h() > 0.0f) {
            m_h = m_v1.window_h();
        }
    }

    WindowFrameStyle BlacksmithWindow::frame_style() const {
        return UiView::frame_style(m_v1);
    }

    void BlacksmithWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        const auto style = frame_style();
        m_x = std::max(8.0f + style.pad_left() * s, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f + style.pad_top() * s, (screen_h - m_h * s) * 0.35f);
    }

    bool BlacksmithWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        reload_document();
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w * s, m_h * s, s, frame_style());
    }

    bool BlacksmithWindow::try_drop_weapon(const ItemStack& stack,
                                           const ItemDb& catalog,
                                           float mx,
                                           float my) {
        if (!m_open || stack.iid == 0 || stack.count <= 0 || stack.equipped) {
            return false;
        }
        const ItemDef* def = catalog.find(stack.index);
        if (!def || !def->is_weapon() || def->grade() < 40) {
            return false;
        }
        reload_document();
        const float s = std::max(0.25f, m_scale);
        const UiHit hit =
            UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w * s, m_h * s, s, {}, this);
        if (hit.bind() != "blacksmith.weapon") {
            return false;
        }
        m_weapon_iid = stack.iid;
        return true;
    }

    const ItemStack* BlacksmithWindow::slotted_weapon(const Bag& bag) const {
        return find_iid(bag, m_weapon_iid);
    }

    void BlacksmithWindow::sync_fonts(engine::IEngine& engine) {
        if (!m_open || m_title_font.ready()) {
            return;
        }
        const auto path = FreetypeFont::find_system_sans_bold();
        if (!path.empty()) {
            (void) m_title_font.load(engine, path, 18);
        }
    }

    void BlacksmithWindow::destroy_fonts(engine::IEngine& engine) {
        m_title_font.destroy(engine);
    }

    void BlacksmithWindow::update_pointer(const engine::FrameInput& input) {
        reload_document();
        const float s = std::max(0.25f, m_scale);
        const UiHit hit = UiView::hit(
            m_v1, nullptr, input.mouse_x, input.mouse_y, m_x, m_y, m_w * s, m_h * s, s, {}, this);
        set_pointer_hit(m_open ? hit.widget_id() : "", input.mouse_left);
        if (!m_open || !input.mouse_left) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
        }
    }

    UiVarMap BlacksmithWindow::variables(const Bag& bag, const ItemDb& catalog) const {
        const auto* weapon = slotted_weapon(bag);
        const auto* def = weapon ? catalog.find(weapon->index) : nullptr;
        const bool valid = def && def->is_weapon() && def->grade() >= 40 && !weapon->equipped;
        const int level = valid ? weapon->upgrade_level : 0;
        const int cap = valid ? server::explosive_blow_cap(
                                    def->class_small() == "wand", weapon->x_attack, weapon->x_magic)
                              : 0;
        const int chance = valid ? server::explosive_blow_chance(level, weapon->upgrade_rate) : 0;
        const int stones = valid ? server::explosive_blow_stone_cost(level) : 0;
        const int fee = valid ? server::explosive_blow_gold_cost(level) : 0;
        const int owned = bag.count_item(server::kRevisionStoneIndex);
        m_can_revise = valid && chance < 100 && owned >= stones;
        m_can_eb = valid && level < cap && bag.money >= fee;
        m_weapon_icon = {};
        if (valid) {
            m_weapon_icon.set_file(catalog.icon_filename(weapon->index));
            m_weapon_icon.set_empty(m_weapon_icon.file().empty());
            m_weapon_icon.set_enchant_protected(weapon->enchant_protected);
        }
        UiVarMap vars{
            {"blacksmith.title", "Explosive Blow"},
            {"blacksmith.weapon_name", valid ? def->name() : "No weapon selected"},
            {"blacksmith.weapon_hint",
             valid ? "Grade " + std::to_string(def->grade()) + ". Click the icon to remove."
                   : "Unequipped weapon, grade 40 or higher."},
            {"blacksmith.placement_hint", "Drag a weapon from your inventory into the slot."},
            {"blacksmith.level_label", "Level / maximum"},
            {"blacksmith.level", valid ? std::to_string(level) : "-"},
            {"blacksmith.cap", valid ? std::to_string(cap) : "-"},
            {"blacksmith.chance_label", "Success chance"},
            {"blacksmith.chance", valid ? std::to_string(chance) + "%" : "-"},
            {"blacksmith.stones_label", "Stones (owned / needed)"},
            {"blacksmith.stones_owned", std::to_string(owned)},
            {"blacksmith.stones_required", valid ? std::to_string(stones) : "-"},
            {"blacksmith.fee_label", "Geon (owned / fee)"},
            {"blacksmith.geon_owned", std::to_string(bag.money)},
            {"blacksmith.fee", valid ? std::to_string(fee) : "-"},
            {"blacksmith.status",
             !valid            ? "Select a weapon to view its upgrade details."
             : level >= cap    ? "This weapon has reached its Explosive Blow cap."
             : bag.money < fee ? "Not enough Geon for the next upgrade."
             : chance == 100   ? "Success chance is already at 100%."
                               : "Use Stones of Revision to improve the chance."},
            {"blacksmith.warning", "Failed upgrades can destroy the weapon."},
            {"blacksmith.revise_label", "Revise chance"},
            {"blacksmith.upgrade_label", "Apply Explosive Blow"},
            {"blacksmith.can_revise", m_can_revise ? "true" : "false"},
            {"blacksmith.can_eb", m_can_eb ? "true" : "false"}};
        ui_fill_theme_vars(m_v1, vars);
        return vars;
    }

    bool BlacksmithWindow::button_enabled(std::string_view action) const {
        if (action == "blacksmith.revise") {
            return m_can_revise;
        }
        if (action == "blacksmith.explosive_blow") {
            return m_can_eb;
        }
        return true;
    }

    bool BlacksmithWindow::resolve_slot(std::string_view bind, int, UiSlotIcon& out) const {
        if (bind != "blacksmith.weapon") {
            return false;
        }
        out = m_weapon_icon;
        return true;
    }

    BlacksmithClickResult
    BlacksmithWindow::handle_click(float mx, float my, const Bag& bag, const ItemDb& catalog) {
        if (!m_open || !contains(mx, my)) {
            return BlacksmithClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const UiHit hit = UiView::hit(
            m_v1, nullptr, mx, my, m_x, m_y, m_w * s, m_h * s, s, variables(bag, catalog), this);
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            set_open(false);
            return BlacksmithClickResult::Close;
        }
        if (hit.bind() == "blacksmith.weapon") {
            m_weapon_iid = 0;
        } else if (hit.action() == "blacksmith.revise" && m_can_revise) {
            return BlacksmithClickResult::Revise;
        } else if (hit.action() == "blacksmith.explosive_blow" && m_can_eb) {
            return BlacksmithClickResult::ExplosiveBlow;
        } else if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
        }
        return BlacksmithClickResult::Consumed;
    }

    void BlacksmithWindow::draw(engine::IEngine& engine,
                                WidgetTools& ui,
                                const FreetypeFont& font,
                                const Bag& bag,
                                const ItemDb& catalog) const {
        if (!m_open) {
            return;
        }
        reload_document();
        const float s = std::max(0.25f, m_scale);
        auto vars = variables(bag, catalog);
        if (const auto* name = m_v1.find_widget("weapon_name"); name && font.ready()) {
            const float text_scale =
                name->pixel_size() > 0 ? static_cast<float>(name->pixel_size()) / font.pixel_size()
                                       : 1.0f;
            auto& text = vars["blacksmith.weapon_name"];
            if (name->w() > 0 && font.text_width(text, text_scale) > name->w()) {
                while (!text.empty() && font.text_width(text + "...", text_scale) > name->w()) {
                    auto start = text.size() - 1;
                    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
                        --start;
                    }
                    text.erase(start);
                }
                text += "...";
            }
        }
        UiView::draw(engine,
                     ui,
                     m_v1,
                     m_x,
                     m_y,
                     m_w * s,
                     m_h * s,
                     s,
                     vars,
                     font,
                     m_title_font.ready() ? m_title_font : font,
                     this,
                     true);
    }

}  // namespace openkal::client
