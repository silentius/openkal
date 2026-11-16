/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ConsumableCraftingWindow.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    void ConsumableCraftingWindow::set_open(bool open) {
        m_open = open;
        m_dragging = false;
    }

    void ConsumableCraftingWindow::set_family(server::ConsumableFamily family) {
        m_family = family;
        m_pending_recipe = 0;
    }

    void ConsumableCraftingWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        m_x = std::max(8.0f, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f, (screen_h - m_h * s) * 0.35f);
    }

    WindowFrameStyle ConsumableCraftingWindow::frame_style() const {
        return m_layout ? UiView::frame_style(m_v1) : WindowFrameStyle::info_menu(true);
    }

    void ConsumableCraftingWindow::sync_from_layout() {
        UiView::reload(m_v1, m_layout);
        if (m_v1.window_w() > 0.0f) {
            m_w = m_v1.window_w();
        }
        if (m_v1.window_h() > 0.0f) {
            m_h = m_v1.window_h();
        }
    }

    bool ConsumableCraftingWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        UiView::reload(m_v1, m_layout);
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w * s, m_h * s, s, frame_style());
    }

    void ConsumableCraftingWindow::update_pointer(const engine::FrameInput& input) {
        UiView::reload(m_v1, m_layout);
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

    bool ConsumableCraftingWindow::can_make(const Bag& bag, std::size_t row) const {
        const auto& recipes = server::consumable_recipes(m_family);
        if (row >= recipes.size()) {
            return false;
        }
        for (const auto& material : recipes[row].ingredients) {
            if (bag.count_item(material.index) < material.count) {
                return false;
            }
        }
        return true;
    }

    UiVarMap ConsumableCraftingWindow::variables(const Bag& bag) const {
        const auto& recipes = server::consumable_recipes(m_family);
        UiVarMap result{
            {"crafting.title", std::string(server::consumable_family_name(m_family)) + " Crafting"},
            {"crafting.empty", recipes.empty() ? "true" : "false"},
            {"crafting.empty_label", "Recipes for this family will be added later."},
            {"crafting.make_label", "Make"},
            {"crafting.close_label", "Close"},
            {"crafting.hint",
             "Each craft makes one item. Ingredients are consumed only on success."}};
        for (std::size_t row = 0; row < m_make_enabled.size(); ++row) {
            const std::string prefix = "crafting.row." + std::to_string(row) + ".";
            result[prefix + "visible"] = row < recipes.size() ? "true" : "false";
            if (row >= recipes.size()) {
                continue;
            }
            const auto& recipe = recipes[row];
            result[prefix + "title"] =
                std::to_string(recipe.output_count) + "x " + std::string(recipe.name);
            std::string materials, owned;
            for (const auto& material : recipe.ingredients) {
                if (!materials.empty()) {
                    materials += " + ";
                    owned += "   |   ";
                }
                materials += std::to_string(material.count) + "x " + std::string(material.name);
                owned += std::string(material.name) + ": " +
                         std::to_string(bag.count_item(material.index));
            }
            result[prefix + "materials"] = materials;
            result[prefix + "owned"] = "Owned: " + owned;
            result[prefix + "color"] = can_make(bag, row) ? "#B5D991" : "#BDAE92";
        }
        ui_fill_theme_vars(m_v1, result);
        return result;
    }

    bool ConsumableCraftingWindow::button_enabled(std::string_view action) const {
        for (std::size_t row = 0; row < m_make_enabled.size(); ++row) {
            if (action == "crafting.make." + std::to_string(row)) {
                return m_make_enabled[row];
            }
        }
        return true;
    }

    ConsumableCraftingClickResult
    ConsumableCraftingWindow::handle_click(float mx, float my, const Bag& bag) {
        if (!m_open || !contains(mx, my)) {
            return ConsumableCraftingClickResult::Ignored;
        }
        for (std::size_t row = 0; row < m_make_enabled.size(); ++row) {
            m_make_enabled[row] = can_make(bag, row);
        }
        const float s = std::max(0.25f, m_scale);
        const auto hit =
            UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w * s, m_h * s, s, variables(bag), this);
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            set_open(false);
            return ConsumableCraftingClickResult::Close;
        }
        for (std::size_t row = 0; row < m_make_enabled.size(); ++row) {
            if (hit.kind() == UiHitKind::Button &&
                hit.action() == "crafting.make." + std::to_string(row) && can_make(bag, row)) {
                m_pending_recipe = server::consumable_recipes(m_family)[row].flag;
                return ConsumableCraftingClickResult::Make;
            }
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
        }
        return ConsumableCraftingClickResult::Consumed;
    }

    std::uint8_t ConsumableCraftingWindow::take_recipe() {
        const auto result = m_pending_recipe;
        m_pending_recipe = 0;
        return result;
    }

    void ConsumableCraftingWindow::draw(engine::IEngine& engine,
                                        WidgetTools& ui,
                                        const FreetypeFont& font,
                                        const Bag& bag) const {
        if (!m_open) {
            return;
        }
        UiView::reload(m_v1, m_layout);
        for (std::size_t row = 0; row < m_make_enabled.size(); ++row) {
            m_make_enabled[row] = can_make(bag, row);
        }
        const float s = std::max(0.25f, m_scale);
        UiView::draw(engine,
                     ui,
                     m_v1,
                     m_x,
                     m_y,
                     m_w * s,
                     m_h * s,
                     s,
                     variables(bag),
                     font,
                     font,
                     this,
                     true);
    }
}  // namespace openkal::client
