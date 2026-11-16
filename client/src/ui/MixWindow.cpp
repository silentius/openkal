/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/client/MixWindow.h"

#include "openkal/client/UiView.h"
#include "openkal/protocol/MixingItem.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace openkal::client {
    namespace {
        constexpr int kCenterSlot = 0;
        constexpr std::size_t kMaterialPageSize = 9;
        constexpr const char* kSlotIds[MixWindow::kSlotCount] = {
            "mix.center",
            "mix.north",
            "mix.east",
            "mix.south",
            "mix.west",
        };
        constexpr float kSlotFallbackX[MixWindow::kSlotCount] = {
            204.0f, 204.0f, 276.0f, 204.0f, 132.0f};
        constexpr float kSlotFallbackY[MixWindow::kSlotCount] = {
            214.0f, 142.0f, 214.0f, 286.0f, 214.0f};

        [[nodiscard]] server::MixItemOffer
        make_offer(const ItemDef* def, std::uint32_t iid, int index, std::uint8_t mix_type) {
            server::MixItemOffer offer;
            if (iid == 0) {
                return offer;
            }
            offer.set_iid(iid);
            offer.set_index(index);
            offer.set_mix_type(mix_type);
            if (def) {
                offer.set_grade(def->grade());
                offer.set_wearable(def->wearable());
                offer.set_weapon(def->is_weapon());
                offer.set_defense(def->is_defense());
                offer.set_class_small(def->class_small());
            }
            return offer;
        }

        [[nodiscard]] server::MixMatch
        match_slots(const std::array<std::uint32_t, MixWindow::kSlotCount>& iids,
                    const std::array<int, MixWindow::kSlotCount>& indices,
                    const std::array<std::uint8_t, MixWindow::kSlotCount>& mix_types,
                    std::uint8_t mode,
                    const ItemDb& catalog,
                    const server::MixCatalog* mix_catalog) {
            server::MixMatch out;
            if (!mix_catalog || !mix_catalog->ready() || iids[0] == 0) {
                return out;
            }
            const ItemDef* center_def = catalog.find(indices[0]);
            const server::MixItemOffer center =
                make_offer(center_def, iids[0], indices[0], mix_types[0]);
            std::vector<server::MixItemOffer> outer;
            outer.reserve(MixWindow::kSlotCount - 1);
            for (int i = 1; i < MixWindow::kSlotCount; ++i) {
                const auto si = static_cast<std::size_t>(i);
                if (iids[si] == 0) {
                    continue;
                }
                const ItemDef* def = catalog.find(indices[si]);
                outer.push_back(make_offer(def, iids[si], indices[si], mix_types[si]));
            }
            return mix_catalog->match(mode, center, outer, -1);
        }

        [[nodiscard]] const ItemStack* find_stack(const Bag& bag, std::uint32_t iid) {
            if (iid == 0) {
                return nullptr;
            }
            for (const auto& s : bag.normal) {
                if (s.iid == iid && s.count > 0) {
                    return &s;
                }
            }
            for (const auto& s : bag.quest) {
                if (s.iid == iid && s.count > 0) {
                    return &s;
                }
            }
            return nullptr;
        }
    }  // namespace

    void MixWindow::clear_slots() {
        for (auto& slot : m_slots) {
            slot.clear();
        }
        m_weapon_iid = 0;
        m_extra_weapon_iid = 0;
        m_armor_iid = 0;
        m_stone1_iid = 0;
        m_stone2_iid = 0;
        m_weapon_index = 0;
        m_armor_index = 0;
        m_stone1_index = 0;
        m_stone2_index = 0;
        m_weapon_mix_type = 0;
        m_hover_widget.clear();
        m_button_pressed = false;
        m_dragging = false;
    }

    void MixWindow::set_open(bool open, std::uint8_t mode) {
        const bool was_materials = m_show_materials;
        m_show_materials = false;
        m_material_page = 0;
        m_open = open;
        if (was_materials) {
            sync_from_layout();
        }
        if (open) {
            m_mode = mode == 1 ? 1u : 0u;
            return;
        }
        clear_slots();
    }

    void MixWindow::show_materials(std::uint8_t mode) {
        clear_slots();
        m_open = true;
        m_show_materials = true;
        m_mode = mode == 1 ? 1u : 0u;
        m_material_page = 0;
        sync_from_layout();
    }

    void MixWindow::toggle() {
        set_open(!m_open, m_mode);
    }

    void MixWindow::set_mode(std::uint8_t mode) {
        m_mode = mode == 1 ? 1u : 0u;
    }

    void MixWindow::sync_from_layout() {
        UiView::reload(m_v1, m_show_materials ? m_material_layout : m_layout);
        const float s = std::max(0.25f, m_scale);
        m_w = m_v1.window_w() * s;
        m_h = m_v1.window_h() * s;
    }

    void MixWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        WidgetTools::center_rect(m_x, m_y, m_w, m_h, screen_w, screen_h);
    }

    WindowFrameStyle MixWindow::frame_style() const {
        return UiView::frame_style(m_v1);
    }

    bool MixWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(mx, my, m_x, m_y, m_w, m_h, s, frame_style());
    }

    bool MixWindow::exit_bottom_rect(float& sx, float& sy, float& sw, float& sh) const {
        const float s = std::max(0.25f, m_scale);
        const UiWidget* w = m_v1.find_widget("exit_bottom");
        if (!w) {
            return false;
        }
        UiView::widget_rect(*w, nullptr, m_x, m_y, m_w, m_h, s, sx, sy, sw, sh);
        return sw > 0.0f && sh > 0.0f;
    }

    bool MixWindow::slot_rect(int slot, float& sx, float& sy, float& sw, float& sh) const {
        if (slot < 0 || slot >= kSlotCount) {
            return false;
        }
        const float s = std::max(0.25f, m_scale);
        const UiWidget* w = m_v1.find_widget(kSlotIds[slot]);
        if (w) {
            UiView::widget_rect(*w, nullptr, m_x, m_y, m_w, m_h, s, sx, sy, sw, sh);
            return true;
        }
        sx = m_x + kSlotFallbackX[slot] * s;
        sy = m_y + kSlotFallbackY[slot] * s;
        sw = 32.0f * s;
        sh = 32.0f * s;
        return sw > 0.0f && sh > 0.0f;
    }

    void MixWindow::mix_button_rect(float& sx, float& sy, float& sw, float& sh) const {
        const float s = std::max(0.25f, m_scale);
        const UiWidget* w = m_v1.find_widget("mix_start");
        if (w) {
            UiView::widget_rect(*w, nullptr, m_x, m_y, m_w, m_h, s, sx, sy, sw, sh);
            return;
        }
        sx = m_x + 132.0f * s;
        sy = m_y + 448.0f * s;
        sw = 176.0f * s;
        sh = 36.0f * s;
    }

    int MixWindow::slot_at(float mx, float my) const {
        if (m_show_materials) {
            return -1;
        }
        for (int i = 0; i < kSlotCount; ++i) {
            float sx = 0.0f;
            float sy = 0.0f;
            float sw = 0.0f;
            float sh = 0.0f;
            if (slot_rect(i, sx, sy, sw, sh) &&
                WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                return i;
            }
        }
        return -1;
    }

    bool MixWindow::mix_button_hit(float mx, float my) const {
        if (m_show_materials) {
            return false;
        }
        float bx = 0.0f;
        float by = 0.0f;
        float bw = 0.0f;
        float bh = 0.0f;
        mix_button_rect(bx, by, bw, bh);
        return WidgetTools::point_in_rect(mx, my, bx, by, bw, bh);
    }

    bool MixWindow::slot_accepts(int slot, const ItemDef& def, int index) const {
        if (slot == kCenterSlot) {
            return def.is_weapon() && def.wearable();
        }
        const bool is_stone = m_mix_catalog ? m_mix_catalog->is_mixing_stone(index)
                                            : protocol::MixingItem::is_mixing_stone(index);
        if (is_stone) {
            int required = 0;
            if (m_mix_catalog) {
                required = m_mix_catalog->stone_require_mix_level(index);
            } else if (index == 904 || index == 905 || index == 1166 || index == 1167) {
                required = 2;
            }
            if (required > 0) {
                const int cur_level = static_cast<int>(
                    (m_slots[static_cast<std::size_t>(kCenterSlot)].mix_type() >> 4) & 0x03);
                if (cur_level < required) {
                    return false;
                }
            }
            return true;
        }
        return def.is_weapon() || def.is_defense();
    }

    void MixWindow::refresh_classification(const ItemDb& catalog) {
        std::array<std::uint32_t, kSlotCount> iids{};
        std::array<int, kSlotCount> indices{};
        std::array<std::uint8_t, kSlotCount> mix_types{};
        for (int i = 0; i < kSlotCount; ++i) {
            const auto si = static_cast<std::size_t>(i);
            iids[si] = m_slots[si].iid();
            indices[si] = m_slots[si].index();
            mix_types[si] = m_slots[si].mix_type();
        }
        const server::MixMatch recipe =
            match_slots(iids, indices, mix_types, m_mode, catalog, m_mix_catalog);
        m_weapon_iid = recipe.weapon_iid();
        m_extra_weapon_iid = recipe.extra_weapon_iid();
        m_armor_iid = recipe.armor_iid();
        m_stone1_iid = recipe.stone1_iid();
        m_stone2_iid = recipe.stone2_iid();
        if (recipe.valid()) {
            m_weapon_index = indices[0];
            m_weapon_mix_type = mix_types[0];
        } else {
            m_weapon_index = 0;
            m_armor_index = 0;
            m_stone1_index = 0;
            m_stone2_index = 0;
            m_weapon_mix_type = 0;
        }
    }

    bool MixWindow::can_make_mix(const ItemDb& catalog) const {
        if (!m_open || m_show_materials) {
            return false;
        }
        std::array<std::uint32_t, kSlotCount> iids{};
        std::array<int, kSlotCount> indices{};
        std::array<std::uint8_t, kSlotCount> mix_types{};
        for (int i = 0; i < kSlotCount; ++i) {
            const auto si = static_cast<std::size_t>(i);
            iids[si] = m_slots[si].iid();
            indices[si] = m_slots[si].index();
            mix_types[si] = m_slots[si].mix_type();
        }
        return match_slots(iids, indices, mix_types, m_mode, catalog, m_mix_catalog).valid();
    }

    void MixWindow::update_pointer(const engine::FrameInput& in) {
        if (!m_open) {
            m_dragging = false;
            m_hover_widget.clear();
            m_button_pressed = false;
            return;
        }
        const UiHit hit = UiView::hit(
            m_v1, nullptr, in.mouse_x, in.mouse_y, m_x, m_y, m_w, m_h, m_scale, {}, nullptr, false);
        m_hover_widget = hit.widget_id();
        if (!in.mouse_left) {
            m_dragging = false;
            return;
        }
        if (m_dragging) {
            m_x = in.mouse_x - m_drag_dx;
            m_y = in.mouse_y - m_drag_dy;
        }
    }

    bool
    MixWindow::try_drop_item(const ItemStack& stack, const ItemDb& catalog, float mx, float my) {
        if (!m_open || stack.iid == 0 || stack.count <= 0 || stack.equipped) {
            return false;
        }
        const int slot = slot_at(mx, my);
        if (slot < 0) {
            return false;
        }
        const ItemDef* def = catalog.find(stack.index);
        if (!def || !slot_accepts(slot, *def, stack.index)) {
            return false;
        }
        for (int i = 0; i < kSlotCount; ++i) {
            if (i != slot && m_slots[static_cast<std::size_t>(i)].iid() == stack.iid) {
                m_slots[static_cast<std::size_t>(i)].clear();
            }
        }
        m_slots[static_cast<std::size_t>(slot)].set(
            stack.iid, stack.index, static_cast<std::uint8_t>(stack.mix_type));
        refresh_classification(catalog);
        return true;
    }

    MixClickResult MixWindow::handle_click(float mx, float my, const ItemDb& catalog) {
        if (!m_open || !contains(mx, my)) {
            return MixClickResult::Ignored;
        }
        const WindowFrameStyle style = frame_style();
        if (WidgetTools::window_close_hit(mx, my, m_x, m_y, m_w, m_h, m_scale, style)) {
            set_open(false, m_mode);
            return MixClickResult::Close;
        }
        float bx = 0.0f;
        float by = 0.0f;
        float bw = 0.0f;
        float bh = 0.0f;
        if (exit_bottom_rect(bx, by, bw, bh) &&
            WidgetTools::point_in_rect(mx, my, bx, by, bw, bh)) {
            set_open(false, m_mode);
            return MixClickResult::Close;
        }
        if (m_show_materials) {
            const UiHit hit = UiView::hit(m_v1,
                                          nullptr,
                                          mx,
                                          my,
                                          m_x,
                                          m_y,
                                          m_w,
                                          m_h,
                                          m_scale,
                                          material_variables(),
                                          nullptr,
                                          false);
            if (hit.action() == "mix.list.upgrade" || hit.action() == "mix.list.create") {
                m_mode = hit.action() == "mix.list.create" ? 1u : 0u;
                m_material_page = 0;
                return MixClickResult::Consumed;
            }
            if (hit.action() == "mix.list.previous" && m_material_page > 0) {
                --m_material_page;
                return MixClickResult::Consumed;
            }
            if (hit.action() == "mix.list.next" && m_material_page + 1 < material_page_count()) {
                ++m_material_page;
                return MixClickResult::Consumed;
            }
        }
        if (mix_button_hit(mx, my)) {
            m_button_pressed = can_make_mix(catalog);
            return MixClickResult::Consumed;
        }
        const int slot = slot_at(mx, my);
        if (slot >= 0 && m_slots[static_cast<std::size_t>(slot)].filled()) {
            m_slots[static_cast<std::size_t>(slot)].clear();
            refresh_classification(catalog);
            return MixClickResult::Consumed;
        }
        float tx = 0.0f;
        float ty = 0.0f;
        float tw = 0.0f;
        float th = 0.0f;
        WidgetTools::window_title_drag_rect(m_x, m_y, m_w, m_h, m_scale, style, tx, ty, tw, th);
        if (WidgetTools::point_in_rect(mx, my, tx, ty, tw, th)) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return MixClickResult::Consumed;
        }
        return MixClickResult::Consumed;
    }

    MixClickResult MixWindow::handle_release(float mx, float my, const ItemDb& catalog) {
        const bool pressed = m_button_pressed;
        m_button_pressed = false;
        if (!m_open || !pressed) {
            return MixClickResult::Ignored;
        }
        if (mix_button_hit(mx, my) && can_make_mix(catalog)) {
            refresh_classification(catalog);
            return MixClickResult::MakeMix;
        }
        return MixClickResult::Consumed;
    }

    void MixWindow::draw_chrome(WidgetTools& ui) const {
        const float s = std::max(0.25f, m_scale);
        UiView::draw_chrome(ui, m_v1, m_x, m_y, m_w, m_h, s, false);
    }

    std::size_t MixWindow::material_page_count() const {
        const auto count = m_mix_catalog ? m_mix_catalog->material_rows(m_mode).size() : 0;
        return std::max(std::size_t{1}, (count + kMaterialPageSize - 1) / kMaterialPageSize);
    }

    UiVarMap MixWindow::material_variables() const {
        UiVarMap vars;
        vars["mix_list.title"] = "Mixing Item List";
        vars["mix_list.upgrade_label"] = "Upgrade Weapon";
        vars["mix_list.create_label"] = "Create Weapon";
        vars["mix_list.description"] = m_mode == 1 ? "Materials for a new Imperial weapon"
                                                   : "Materials for upgrading a weapon's mix grade";
        vars["mix_list.weapon_label"] = "Main weapon";
        vars["mix_list.extra_label"] = "Extra weapon";
        vars["mix_list.armor_label"] = "Armor";
        vars["mix_list.stones_label"] = "Stones";
        vars["mix_list.gold_label"] = "Gold";
        vars["mix_list.previous_label"] = "Previous";
        vars["mix_list.next_label"] = "Next";
        vars["mix_list.upgrade_color"] = m_mode == 0 ? "#E5CF9E" : "#BDAE92";
        vars["mix_list.create_color"] = m_mode == 1 ? "#E5CF9E" : "#BDAE92";
        vars["mix_list.note"] =
            m_mode == 1 ? "Creation starts the new weapon at mix Grade 1."
                        : "G3: use a mix Grade 2 weapon and two Mystery or Demon's Blood stones.";
        vars["mix_list.stone_note"] =
            m_mode == 1
                ? "* Use two elemental stones of the same type. Mystery / Blood cannot be used."
                : "* Use two stones of the same type for G3; ordinary upgrades use one elemental "
                  "stone.";
        vars["mix_list.warning"] =
            "Extra weapon, armor and stones are consumed even if mixing fails.";
        vars["mix_list.empty_label"] = "No mixing recipes are available.";
        const auto rows = m_mix_catalog ? m_mix_catalog->material_rows(m_mode)
                                        : std::vector<server::MixMaterialRow>{};
        const std::size_t pages =
            std::max(std::size_t{1}, (rows.size() + kMaterialPageSize - 1) / kMaterialPageSize);
        const std::size_t page = std::min(m_material_page, pages - 1);
        vars["mix_list.page"] = std::to_string(page + 1) + " / " + std::to_string(pages);
        vars["mix_list.empty"] = rows.empty() ? "true" : "false";
        auto grade_text = [](int grade) {
            return grade == 0  ? std::string("None")
                   : grade < 0 ? std::string("Any grade")
                               : "Grade " + std::to_string(grade);
        };
        for (std::size_t i = 0; i < kMaterialPageSize; ++i) {
            const std::string prefix = "mix_list.row." + std::to_string(i) + ".";
            const std::size_t index = page * kMaterialPageSize + i;
            vars[prefix + "visible"] = index < rows.size() ? "true" : "false";
            if (index >= rows.size()) {
                continue;
            }
            const auto& row = rows[index];
            vars[prefix + "weapon"] = grade_text(row.center_grade);
            vars[prefix + "extra"] = grade_text(row.extra_grade);
            vars[prefix + "armor"] = grade_text(row.armor_grade) + (row.armor_grade == 0 ? ""
                                                                    : row.upper_only     ? " chest"
                                                                                     : " armor");
            vars[prefix + "stones"] = std::to_string(row.stone_count) +
                                      (row.grade3 ? " Mystery / Blood" : " elemental") +
                                      (row.same_stone_type ? "*" : "");
            vars[prefix + "gold"] = std::to_string(row.gold);
        }
        ui_fill_theme_vars(m_v1, vars);
        return vars;
    }

    void MixWindow::draw_materials(engine::IEngine& engine,
                                   WidgetTools& ui,
                                   const FreetypeFont& font) const {
        class MaterialHost final : public UiViewHost {
          public:
            MaterialHost(std::size_t page, std::size_t pages) : m_page(page), m_pages(pages) {}

            bool button_enabled(std::string_view action) const override {
                if (action == "mix.list.previous") {
                    return m_page > 0;
                }
                if (action == "mix.list.next") {
                    return m_page + 1 < m_pages;
                }
                return true;
            }

          private:
            std::size_t m_page;
            std::size_t m_pages;
        };
        MaterialHost host(m_material_page, material_page_count());
        host.set_pointer_hit(m_hover_widget, false);
        UiView::draw(engine,
                     ui,
                     m_v1,
                     m_x,
                     m_y,
                     m_w,
                     m_h,
                     m_scale,
                     material_variables(),
                     font,
                     font,
                     &host,
                     true);
    }

    void MixWindow::draw(engine::IEngine& engine,
                         WidgetTools& ui,
                         const FreetypeFont& font,
                         const Bag& bag,
                         const ItemDb& catalog) const {
        if (!m_open) {
            return;
        }
        if (m_show_materials) {
            draw_materials(engine, ui, font);
            return;
        }
        const float s = std::max(0.25f, m_scale);
        const bool can_make = can_make_mix(catalog);

        class MixHost : public UiViewHost {
          public:
            MixHost(const std::array<Slot, MixWindow::kSlotCount>& slots,
                    const Bag& bag,
                    const ItemDb& catalog,
                    bool can_make)
                : m_slots(slots), m_bag(bag), m_catalog(catalog), m_can_make(can_make) {}

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int /*index*/, UiSlotIcon& out) const override {
                int slot = -1;
                for (int i = 0; i < MixWindow::kSlotCount; ++i) {
                    if (bind == kSlotIds[i]) {
                        slot = i;
                        break;
                    }
                }
                if (slot < 0) {
                    return false;
                }
                const Slot& filled = m_slots[static_cast<std::size_t>(slot)];
                if (!filled.filled()) {
                    out.set_empty(true);
                    return true;
                }
                const ItemStack* found = find_stack(m_bag, filled.iid());
                const int index = found ? found->index : filled.index();
                if (index == 0) {
                    out.set_empty(true);
                    return true;
                }
                out.set_file(m_catalog.icon_filename(index));
                out.set_empty(out.file().empty());
                return true;
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                if (action == "mix.start") {
                    return m_can_make;
                }
                return true;
            }

          private:
            const std::array<Slot, MixWindow::kSlotCount>& m_slots;
            const Bag& m_bag;
            const ItemDb& m_catalog;
            bool m_can_make = false;
        };

        UiVarMap vars;
        vars["ui.title"] = "Mix Master";
        vars["mix.title"] = "Mix Master";
        vars["mix.mode"] = m_mode == 1 ? "Imperial Mixing" : "Elemental Mixing";
        vars["mix.placement_hint"] = "Place a weapon in the center and materials around it.";
        vars["mix.weapon_label"] = "Weapon";
        vars["mix.remove_hint"] = "Click a placed item to remove it.";
        vars["mix.status"] = can_make ? "Recipe ready. Start mixing when you are ready."
                                      : "Add matching materials to complete a recipe.";
        if (!m_slots[kCenterSlot].filled()) {
            vars["mix.status"] = "Place the weapon you want to mix in the center.";
        } else if (!m_mix_catalog || !m_mix_catalog->ready()) {
            vars["mix.status"] = "Mixing recipes are unavailable.";
        }
        vars["ui.start_mixing"] = "Start Mixing";
        vars["mix.can_start_label"] = vars["ui.start_mixing"];
        vars["mix.can_start"] = can_make ? "true" : "false";
        ui_fill_theme_vars(m_v1, vars);

        MixHost host(m_slots, bag, catalog, can_make);
        host.set_pointer_hit(m_hover_widget, m_button_pressed && m_hover_widget == "mix_start");
        UiView::draw(engine, ui, m_v1, m_x, m_y, m_w, m_h, s, vars, font, font, &host, true);
    }

}  // namespace openkal::client
