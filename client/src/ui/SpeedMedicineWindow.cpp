/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SpeedMedicineWindow.h"

#include "openkal/server/SpeedMedicine.h"

#include <algorithm>
#include <array>
#include <string>

namespace openkal::client {
    namespace {
        constexpr std::uint16_t kMaxQuantity = 999;

        [[nodiscard]] std::array<std::uint16_t*, 2> quantity_fields(protocol::CookingOrder& order) {
            return {&order.blood_fish, &order.medicine_from_blood_fish};
        }

    }  // namespace

    void SpeedMedicineWindow::set_open(bool open) {
        m_open = open;
        m_dragging = false;
        if (!open) {
            m_order = {};
        }
    }

    void SpeedMedicineWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        m_x = std::max(8.0f, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f, (screen_h - m_h * s) * 0.35f);
    }

    WindowFrameStyle SpeedMedicineWindow::frame_style() const {
        return m_layout ? UiView::frame_style(m_v1) : WindowFrameStyle::info_menu(true);
    }

    void SpeedMedicineWindow::sync_from_layout() {
        UiView::reload(m_v1, m_layout);
        if (m_v1.window_w() > 0.0f) {
            m_w = m_v1.window_w();
        }
        if (m_v1.window_h() > 0.0f) {
            m_h = m_v1.window_h();
        }
    }

    bool SpeedMedicineWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        UiView::reload(m_v1, m_layout);
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w * s, m_h * s, s, frame_style());
    }

    void SpeedMedicineWindow::update_pointer(const engine::FrameInput& input) {
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

    bool SpeedMedicineWindow::can_make(const Bag& bag) const {
        if (!server::valid_speed_medicine_order(m_order)) {
            return false;
        }
        const server::SpeedMedicineCost cost = server::speed_medicine_cost(m_order);
        return bag.count_item(server::kDemonBloodItemIndex) >= cost.demon_blood &&
               bag.count_item(server::kSeaBreamItemIndex) >= cost.sea_bream &&
               bag.count_item(server::kBambooKegItemIndex) >= cost.bamboo_keg &&
               bag.count_item(server::kBloodFishItemIndex) >= cost.blood_fish &&
               bag.money >= cost.geon;
    }

    UiVarMap SpeedMedicineWindow::variables(const Bag& bag) const {
        const server::SpeedMedicineCost cost = server::speed_medicine_cost(m_order);
        UiVarMap result{
            {"speed_medicine.title", "Chemist of Speed-up Medicine"},
            {"speed_medicine.from_blood_fish_label",
             "Speed-up Medicine (use Sea Bream with Blood)"},
            {"speed_medicine.blood_fish_label", "Sea Bream with Blood (use raw materials)"},
            {"speed_medicine.from_blood_fish", std::to_string(m_order.medicine_from_blood_fish)},
            {"speed_medicine.blood_fish", std::to_string(m_order.blood_fish)},
            {"speed_medicine.requirements_label", "Requirements"},
            {"speed_medicine.owned_label", "Owned"},
            {"speed_medicine.required_label", "Required"},
            {"speed_medicine.requirements.demon_blood.label", "Blood"},
            {"speed_medicine.requirements.demon_blood.required", std::to_string(cost.demon_blood)},
            {"speed_medicine.requirements.demon_blood.owned",
             std::to_string(bag.count_item(server::kDemonBloodItemIndex))},
            {"speed_medicine.requirements.sea_bream.label", "Sea Bream"},
            {"speed_medicine.requirements.sea_bream.required", std::to_string(cost.sea_bream)},
            {"speed_medicine.requirements.sea_bream.owned",
             std::to_string(bag.count_item(server::kSeaBreamItemIndex))},
            {"speed_medicine.requirements.bamboo_keg.label", "Keg"},
            {"speed_medicine.requirements.bamboo_keg.required", std::to_string(cost.bamboo_keg)},
            {"speed_medicine.requirements.bamboo_keg.owned",
             std::to_string(bag.count_item(server::kBambooKegItemIndex))},
            {"speed_medicine.requirements.blood_fish.label", "Sea Bream with Blood"},
            {"speed_medicine.requirements.blood_fish.required", std::to_string(cost.blood_fish)},
            {"speed_medicine.requirements.blood_fish.owned",
             std::to_string(bag.count_item(server::kBloodFishItemIndex))},
            {"speed_medicine.requirements.geon.label", "Geon"},
            {"speed_medicine.requirements.geon.required", std::to_string(cost.geon)},
            {"speed_medicine.requirements.geon.owned", std::to_string(bag.money)},
            {"speed_medicine.materials",
             "Need: Blood " + std::to_string(cost.demon_blood) + "/" +
                 std::to_string(bag.count_item(server::kDemonBloodItemIndex)) + "   Sea Bream " +
                 std::to_string(cost.sea_bream) + "/" +
                 std::to_string(bag.count_item(server::kSeaBreamItemIndex)) + "   Keg " +
                 std::to_string(cost.bamboo_keg) + "/" +
                 std::to_string(bag.count_item(server::kBambooKegItemIndex))},
            {"speed_medicine.fee",
             "Sea Bream with Blood " + std::to_string(cost.blood_fish) + "/" +
                 std::to_string(bag.count_item(server::kBloodFishItemIndex)) + "   Geon " +
                 std::to_string(cost.geon) + "/" + std::to_string(bag.money)},
            {"speed_medicine.hint", "Production takes 10 seconds. Esc cancels it."},
            {"speed_medicine.make_label", "Make"}};
        ui_fill_theme_vars(m_v1, result);
        return result;
    }

    bool SpeedMedicineWindow::button_enabled(std::string_view action) const {
        return action != "speed_medicine.make" || m_make_enabled;
    }

    SpeedMedicineClickResult
    SpeedMedicineWindow::handle_click(float mx, float my, const Bag& bag, bool fast) {
        if (!m_open || !contains(mx, my)) {
            return SpeedMedicineClickResult::Ignored;
        }
        const float s = std::max(0.25f, m_scale);
        const UiHit hit =
            UiView::hit(m_v1, nullptr, mx, my, m_x, m_y, m_w * s, m_h * s, s, variables(bag), this);
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            set_open(false);
            return SpeedMedicineClickResult::Close;
        }
        auto fields = quantity_fields(m_order);
        const std::uint16_t step = fast ? 10 : 1;
        if (hit.kind() == UiHitKind::Button) {
            const std::string& action = hit.action();
            int recipe = -1;
            bool add = false;
            if (action == "speed_medicine.from_blood_fish.decrease") {
                recipe = 1;
            } else if (action == "speed_medicine.from_blood_fish.increase") {
                recipe = 1;
                add = true;
            } else if (action == "speed_medicine.blood_fish.decrease") {
                recipe = 0;
            } else if (action == "speed_medicine.blood_fish.increase") {
                recipe = 0;
                add = true;
            } else if (action == "speed_medicine.make" && can_make(bag)) {
                m_pending_order = m_order;
                set_open(false);
                return SpeedMedicineClickResult::Make;
            }
            if (recipe >= 0) {
                std::uint16_t& quantity = *fields[static_cast<std::size_t>(recipe)];
                quantity =
                    add ? static_cast<std::uint16_t>(std::min<int>(kMaxQuantity, quantity + step))
                        : (quantity > step ? static_cast<std::uint16_t>(quantity - step) : 0);
            }
            return SpeedMedicineClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
        }
        return SpeedMedicineClickResult::Consumed;
    }

    protocol::CookingOrder SpeedMedicineWindow::take_order() {
        const protocol::CookingOrder result = m_pending_order;
        m_pending_order = {};
        return result;
    }

    void SpeedMedicineWindow::draw(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   const FreetypeFont& font,
                                   const Bag& bag) const {
        if (!m_open) {
            return;
        }
        UiView::reload(m_v1, m_layout);
        const float s = std::max(0.25f, m_scale);
        const float width = m_w * s;
        const float height = m_h * s;
        m_make_enabled = can_make(bag);
        UiView::draw(
            engine, ui, m_v1, m_x, m_y, width, height, s, variables(bag), font, font, this, true);
        m_make_enabled = false;
    }

}  // namespace openkal::client
