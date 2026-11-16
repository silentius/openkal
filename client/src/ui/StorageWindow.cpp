/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/StorageWindow.h"

#include "openkal/client/UiView.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    namespace {
        [[nodiscard]] ItemTipExtras item_tip_extras(const ItemStack& stack) {
            ItemTipExtras extras;
            extras.x_attack = stack.x_attack;
            extras.x_magic = stack.x_magic;
            extras.x_hit = stack.x_hit;
            extras.x_defense = stack.x_defense;
            extras.x_dodge = stack.x_dodge;
            extras.upgrade_level = stack.upgrade_level;
            extras.random_stat_type =
                static_cast<protocol::RandomItemStatType>(stack.random_stat_type);
            extras.random_stat_value = stack.random_stat_value;
            extras.mix_type = stack.mix_type;
            extras.bof = stack.bof;
            extras.qigong_perforated = stack.qigong_perforated;
            extras.qigong_effect = stack.qigong_effect;
            extras.dragon_effect = stack.dragon_effect;
            extras.egg_level = std::max(1, stack.transform_level);
            extras.egg_exp = std::max(0, stack.transform_exp);
            extras.account_bound = stack.account_bound;
            extras.character_bound = stack.character_bound;
            extras.enchant_protected = stack.enchant_protected;
            return extras;
        }

        [[nodiscard]] ItemStack item_stack_of(const protocol::StorageItem& item) {
            ItemStack stack;
            stack.iid = item.iid;
            stack.index = item.index;
            stack.prefix = item.prefix;
            stack.count = static_cast<int>(item.count);
            stack.cur_end = protocol::insert_cur_end(item.info);
            stack.max_end = protocol::insert_max_end(item.info);
            stack.x_attack = item.charm.x_attack;
            stack.x_magic = item.charm.x_magic;
            stack.x_hit = item.charm.x_hit;
            stack.x_defense = item.charm.x_defense;
            stack.x_dodge = item.charm.x_dodge;
            stack.upgrade_level = item.charm.upgrade_level;
            stack.upgrade_rate = item.charm.upgrade_rate;
            stack.mix_type = item.charm.mix_type;
            stack.bof = item.charm.bof;
            stack.qigong_perforated = item.charm.qigong_perforated;
            stack.qigong_effect = item.charm.qigong_effect;
            stack.dragon_effect = item.charm.dragon_effect;
            stack.random_stat_type = item.charm.random_stat_type;
            stack.random_stat_value = item.charm.random_stat_value;
            stack.transform_level = item.transform_level;
            stack.transform_exp = static_cast<int>(item.transform_exp);
            stack.account_bound = protocol::insert_account_bound(item.info);
            stack.character_bound = protocol::insert_character_bound(item.info);
            stack.enchant_protected = (item.info & protocol::kInsertEnchantProtected) != 0;
            return stack;
        }
    }  // namespace

    void StorageWindow::reload_document() const {
        UiView::reload(m_v1, m_layout);
    }

    void StorageWindow::sync_from_layout() {
        reload_document();
        if (m_v1.window_w() > 0.0f) {
            m_w = m_v1.window_w();
        }
        if (m_v1.window_h() > 0.0f) {
            m_h = m_v1.window_h();
        }
    }

    void StorageWindow::close() {
        m_open = false;
        m_dragging = false;
        m_selected_side = Side::None;
        m_selected_iid = 0;
        m_marked_inventory_iids.clear();
        m_marked_storage_iids.clear();
        m_pending_transfers.clear();
        m_quantity_kind = QuantityKind::None;
        m_quantity_iid = 0;
        m_hover_item_index = 0;
    }

    void StorageWindow::show(const protocol::StorageInfo& info) {
        sync_from_layout();
        const bool same_keeper = m_open && m_npc_id == info.npc_id;
        m_npc_id = info.npc_id;
        m_storage = {};
        m_quantity_kind = QuantityKind::None;
        m_quantity_iid = 0;
        for (const protocol::StorageItem& item : info.items) {
            if (item.slot < m_storage.size()) {
                m_storage[item.slot] = item_stack_of(item);
            }
        }
        if (!same_keeper) {
            m_inventory_page = 0;
            m_storage_page = 0;
            m_selected_side = Side::None;
            m_selected_iid = 0;
            m_marked_inventory_iids.clear();
            m_marked_storage_iids.clear();
            m_pending_transfers.clear();
        }
        m_marked_storage_iids.erase(
            std::remove_if(m_marked_storage_iids.begin(),
                           m_marked_storage_iids.end(),
                           [&](std::uint32_t iid) {
                               return std::none_of(
                                   m_storage.begin(),
                                   m_storage.end(),
                                   [iid](const ItemStack& item) { return item.iid == iid; });
                           }),
            m_marked_storage_iids.end());
        if (m_selected_side == Side::Storage) {
            m_selected_iid = m_marked_storage_iids.empty() ? 0 : m_marked_storage_iids.back();
            if (m_selected_iid == 0) {
                m_selected_side = Side::None;
            }
        }
        m_open = true;
    }

    void StorageWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float scale = std::max(0.25f, m_scale);
        m_x = std::max(8.0f, (screen_w - m_w * scale) * 0.5f);
        m_y = std::max(8.0f, (screen_h - m_h * scale) * 0.42f);
    }

    bool StorageWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        reload_document();
        const float scale = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(
            mx, my, m_x, m_y, m_w * scale, m_h * scale, scale, UiView::frame_style(m_v1));
    }

    const ItemStack* StorageWindow::inventory_item(const Bag& bag, int cell) const {
        int remaining = m_inventory_page * kSlotsPerPage + cell;
        if (remaining < 0) {
            return nullptr;
        }
        // The storage inventory keeps ordinary zero-value items. In particular,
        // unequipped items with no NPC sell value must remain visible here.
        for (const auto* section : {&bag.normal, &bag.quest}) {
            const int first = 0;
            for (int i = first; i < static_cast<int>(section->size()); ++i) {
                const auto& item = (*section)[i];
                if (item.iid == 0 || item.index <= 0 || item.count <= 0 || item.equipped) {
                    continue;
                }
                if (section == &bag.quest && i >= kOtherSectionSlots) {
                    continue;
                }
                if (remaining-- == 0) {
                    return &item;
                }
            }
        }
        return nullptr;
    }

    const ItemStack* StorageWindow::selected_item(const Bag& bag) const {
        if (m_selected_side == Side::Inventory) {
            return bag.find_instance(m_selected_iid);
        }
        if (m_selected_side == Side::Storage) {
            const auto found =
                std::find_if(m_storage.begin(), m_storage.end(), [&](const ItemStack& item) {
                    return item.iid == m_selected_iid;
                });
            return found == m_storage.end() ? nullptr : &*found;
        }
        return nullptr;
    }

    void StorageWindow::set_hover(const ItemStack* item, float mx, float my) {
        m_hover_item_index = item ? item->index : 0;
        m_hover_item_count = item ? item->count : 0;
        m_hover_prefix = item ? item->prefix : 0;
        m_hover_cur_endurance = item && item->cur_end > 0 ? item->cur_end : -1;
        m_hover_max_endurance = item && item->max_end > 0 ? item->max_end : -1;
        m_hover_extras = item ? item_tip_extras(*item) : ItemTipExtras{};
        m_hover_mx = mx;
        m_hover_my = my;
    }

    void StorageWindow::update_pointer(const engine::FrameInput& input, const Bag& bag) {
        set_hover(nullptr, input.mouse_x, input.mouse_y);
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (quantity_open()) {
            m_dragging = false;
            return;
        }
        if (!input.mouse_left) {
            m_dragging = false;
        } else if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
            return;
        }
        reload_document();
        const UiHit hit = UiView::hit(m_v1,
                                      nullptr,
                                      input.mouse_x,
                                      input.mouse_y,
                                      m_x,
                                      m_y,
                                      m_w * m_scale,
                                      m_h * m_scale,
                                      m_scale,
                                      vars(bag),
                                      nullptr,
                                      true);
        if (hit.kind() != UiHitKind::IconGridCell) {
            return;
        }
        if (hit.bind() == "storage.inventory") {
            set_hover(inventory_item(bag, hit.index()), input.mouse_x, input.mouse_y);
        } else if (hit.bind() == "storage.items") {
            const int absolute = m_storage_page * protocol::kStorageSlotsPerRegister + hit.index();
            set_hover(absolute >= 0 && absolute < static_cast<int>(m_storage.size())
                          ? &m_storage[static_cast<std::size_t>(absolute)]
                          : nullptr,
                      input.mouse_x,
                      input.mouse_y);
        }
    }

    UiVarMap StorageWindow::vars(const Bag& bag) const {
        UiVarMap values;
        values["storage.title"] = "Account Storage";
        values["storage.inventory_label"] = "Inventory";
        values["storage.storage_label"] = "Storage";
        values["storage.inventory_tab"] = "inventory";
        values["storage.storage_tab"] = "storage";
        values["storage.inventory_page"] = std::to_string(m_inventory_page);
        values["storage.page"] = std::to_string(m_storage_page);
        values["storage.money"] = std::to_string(std::max<std::int64_t>(0, bag.money));
        values["storage.fee"] = "60 Geon per transfer · CTRL: select multiple";
        const int used = static_cast<int>(
            std::count_if(m_storage.begin(), m_storage.end(), [](const ItemStack& item) {
                return item.iid != 0;
            }));
        values["storage.count_text"] =
            std::to_string(used) + "/" + std::to_string(protocol::kStorageCapacity);
        values["storage.selected_name"] = "Select an item";
        values["storage.quantity"] = std::to_string(m_quantity);
        values["storage.quantity_open"] = quantity_open() ? "true" : "false";
        values["storage.quantity_prompt"] =
            m_quantity_kind == QuantityKind::Withdraw ? "Withdraw quantity" : "Deposit quantity";
        const std::size_t marked = m_selected_side == Side::Inventory
                                       ? m_marked_inventory_iids.size()
                                       : m_marked_storage_iids.size();
        if (marked > 1) {
            values["storage.selected_name"] = std::to_string(marked) + " items marked";
            ui_fill_theme_vars(m_v1, values);
            return values;
        }
        if (const ItemStack* item = selected_item(bag)) {
            values["storage.selected_name"] = "Item #" + std::to_string(item->index);
        }
        ui_fill_theme_vars(m_v1, values);
        return values;
    }

    bool StorageWindow::queue_marked_deposits(const Bag& bag) {
        m_pending_transfers.clear();
        for (const std::uint32_t iid : m_marked_inventory_iids) {
            const auto* found = bag.find_instance(iid);
            if (found && !found->equipped && found->count > 0) {
                m_pending_transfers.emplace_back(iid, static_cast<std::uint32_t>(found->count));
            }
        }
        m_marked_inventory_iids.clear();
        m_selected_side = Side::None;
        m_selected_iid = 0;
        return !m_pending_transfers.empty();
    }

    bool StorageWindow::queue_marked_withdrawals() {
        m_pending_transfers.clear();
        for (const std::uint32_t iid : m_marked_storage_iids) {
            const auto found =
                std::find_if(m_storage.begin(), m_storage.end(), [iid](const ItemStack& item) {
                    return item.iid == iid;
                });
            if (found != m_storage.end() && found->count > 0) {
                m_pending_transfers.emplace_back(iid, static_cast<std::uint32_t>(found->count));
            }
        }
        m_marked_storage_iids.clear();
        m_selected_side = Side::None;
        m_selected_iid = 0;
        return !m_pending_transfers.empty();
    }

    bool StorageWindow::take_pending_transfer(std::uint32_t& iid, std::uint32_t& count) {
        if (m_pending_transfers.empty()) {
            return false;
        }
        iid = m_pending_transfers.front().first;
        count = m_pending_transfers.front().second;
        m_pending_transfers.erase(m_pending_transfers.begin());
        return true;
    }

    void
    StorageWindow::begin_quantity(QuantityKind kind, std::uint32_t iid, std::uint32_t maximum) {
        m_quantity_kind = kind;
        m_quantity = 1;
        m_quantity_max = std::max<std::uint32_t>(1, maximum);
        m_quantity_iid = iid;
        m_quantity_typing_started = false;
        m_dragging = false;
    }

    StorageClickResult StorageWindow::confirm_quantity() {
        if (m_quantity_kind == QuantityKind::None || m_quantity_iid == 0) {
            return StorageClickResult::Consumed;
        }
        m_pending_transfers.clear();
        m_pending_transfers.emplace_back(m_quantity_iid,
                                         std::clamp(m_quantity, 1u, m_quantity_max));
        const StorageClickResult result = m_quantity_kind == QuantityKind::Deposit
                                              ? StorageClickResult::Deposit
                                              : StorageClickResult::Withdraw;
        m_marked_inventory_iids.clear();
        m_marked_storage_iids.clear();
        m_selected_side = Side::None;
        m_selected_iid = 0;
        m_quantity_kind = QuantityKind::None;
        m_quantity_iid = 0;
        return result;
    }

    StorageClickResult StorageWindow::handle_keyboard(const engine::FrameInput& input) {
        if (!m_open || !quantity_open()) {
            return StorageClickResult::Ignored;
        }
        if (input.key_escape_pressed) {
            m_quantity_kind = QuantityKind::None;
            m_quantity_iid = 0;
            return StorageClickResult::Consumed;
        }
        if (input.key_backspace_pressed) {
            m_quantity_typing_started = true;
            m_quantity = std::max<std::uint32_t>(1, m_quantity / 10);
        }
        if (input.key_delete_pressed) {
            m_quantity = 1;
            m_quantity_typing_started = false;
        }
        for (int i = 0; i < input.text_char_count; ++i) {
            const char ch = input.text_chars[i];
            if (ch < '0' || ch > '9') {
                continue;
            }
            if (!m_quantity_typing_started) {
                m_quantity = 0;
                m_quantity_typing_started = true;
            }
            const std::uint64_t next =
                static_cast<std::uint64_t>(m_quantity) * 10u + static_cast<std::uint64_t>(ch - '0');
            m_quantity =
                static_cast<std::uint32_t>(std::clamp<std::uint64_t>(next, 1u, m_quantity_max));
        }
        return input.key_enter_pressed ? confirm_quantity() : StorageClickResult::Consumed;
    }

    StorageClickResult StorageWindow::handle_click(
        float mx, float my, WidgetTools& ui, const Bag& bag, bool ctrl_pressed) {
        if (!m_open) {
            return StorageClickResult::Ignored;
        }
        reload_document();
        const UiHit hit = UiView::hit(m_v1,
                                      &ui,
                                      mx,
                                      my,
                                      m_x,
                                      m_y,
                                      m_w * m_scale,
                                      m_h * m_scale,
                                      m_scale,
                                      vars(bag),
                                      nullptr,
                                      true);
        if (hit.kind() == UiHitKind::None) {
            return StorageClickResult::Ignored;
        }
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            close();
            return StorageClickResult::Close;
        }
        if (hit.kind() == UiHitKind::TitleDrag && !quantity_open()) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return StorageClickResult::Consumed;
        }
        if (quantity_open()) {
            if (hit.action() == "storage.quantity.decrease" && m_quantity > 1) {
                --m_quantity;
            } else if (hit.action() == "storage.quantity.increase" && m_quantity < m_quantity_max) {
                ++m_quantity;
            } else if (hit.action() == "storage.quantity.all") {
                m_quantity = m_quantity_max;
            } else if (hit.action() == "storage.quantity.cancel") {
                m_quantity_kind = QuantityKind::None;
                m_quantity_iid = 0;
            } else if (hit.action() == "storage.quantity.confirm") {
                return confirm_quantity();
            }
            return StorageClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::IconGridCell && hit.bind() == "storage.inventory") {
            if (const ItemStack* item = inventory_item(bag, hit.index())) {
                m_marked_storage_iids.clear();
                const auto marked = std::find(
                    m_marked_inventory_iids.begin(), m_marked_inventory_iids.end(), item->iid);
                if (!ctrl_pressed) {
                    m_marked_inventory_iids.clear();
                    m_marked_inventory_iids.push_back(item->iid);
                } else if (marked == m_marked_inventory_iids.end()) {
                    m_marked_inventory_iids.push_back(item->iid);
                } else {
                    m_marked_inventory_iids.erase(marked);
                }
                m_selected_side = Side::Inventory;
                m_selected_iid =
                    m_marked_inventory_iids.empty() ? 0 : m_marked_inventory_iids.back();
                if (m_selected_iid == 0) {
                    m_selected_side = Side::None;
                }
            }
        } else if (hit.kind() == UiHitKind::IconGridCell && hit.bind() == "storage.items") {
            const int absolute = m_storage_page * protocol::kStorageSlotsPerRegister + hit.index();
            if (absolute >= 0 && absolute < static_cast<int>(m_storage.size())) {
                const ItemStack& item = m_storage[static_cast<std::size_t>(absolute)];
                if (item.iid != 0) {
                    m_marked_inventory_iids.clear();
                    const auto marked = std::find(
                        m_marked_storage_iids.begin(), m_marked_storage_iids.end(), item.iid);
                    if (!ctrl_pressed) {
                        m_marked_storage_iids.clear();
                        m_marked_storage_iids.push_back(item.iid);
                    } else if (marked == m_marked_storage_iids.end()) {
                        m_marked_storage_iids.push_back(item.iid);
                    } else {
                        m_marked_storage_iids.erase(marked);
                    }
                    m_selected_side = Side::Storage;
                    m_selected_iid =
                        m_marked_storage_iids.empty() ? 0 : m_marked_storage_iids.back();
                    if (m_selected_iid == 0) {
                        m_selected_side = Side::None;
                    }
                }
            }
        } else if (hit.kind() == UiHitKind::Tab && hit.bind() == "storage.inventory_page") {
            const int page = hit.tab_id().empty() ? -1 : hit.tab_id()[0] - '0';
            if (page >= 0 && page < kMaxRegisters && bag.compact_register_enabled[page]) {
                m_inventory_page = page;
            }
        } else if (hit.kind() == UiHitKind::Tab && hit.bind() == "storage.page") {
            const int page = hit.tab_id().empty() ? -1 : hit.tab_id()[0] - '0';
            if (page >= 0 && page < protocol::kStorageRegisters) {
                m_storage_page = page;
            }
        } else if (hit.action() == "storage.deposit" && m_selected_side == Side::Inventory) {
            if (m_marked_inventory_iids.size() > 1) {
                return queue_marked_deposits(bag) ? StorageClickResult::Deposit
                                                  : StorageClickResult::Consumed;
            }
            if (const ItemStack* item = selected_item(bag); item && item->count > 1) {
                begin_quantity(
                    QuantityKind::Deposit, item->iid, static_cast<std::uint32_t>(item->count));
            } else if (queue_marked_deposits(bag)) {
                return StorageClickResult::Deposit;
            }
        } else if (hit.action() == "storage.withdraw" && m_selected_side == Side::Storage) {
            if (m_marked_storage_iids.size() > 1) {
                return queue_marked_withdrawals() ? StorageClickResult::Withdraw
                                                  : StorageClickResult::Consumed;
            }
            if (const ItemStack* item = selected_item(bag); item && item->count > 1) {
                begin_quantity(
                    QuantityKind::Withdraw, item->iid, static_cast<std::uint32_t>(item->count));
            } else if (queue_marked_withdrawals()) {
                return StorageClickResult::Withdraw;
            }
        }
        return StorageClickResult::Consumed;
    }

    void StorageWindow::sync_fonts(engine::IEngine& engine) {
        reload_document();
        const int body_size = std::clamp(m_v1.font_pixel_size(), 8, 48);
        const UiWidget* title = m_v1.find_widget("title");
        const int title_size = title && title->pixel_size() > 0 ? title->pixel_size() : body_size;
        if (!m_body_font.ready() || m_body_font.pixel_size() != body_size) {
            m_body_font.destroy(engine);
            (void) m_body_font.load(engine, FreetypeFont::find_system_sans(), body_size);
        }
        if (!m_title_font.ready() || m_title_font.pixel_size() != title_size) {
            m_title_font.destroy(engine);
            (void) m_title_font.load(engine, FreetypeFont::find_system_sans_bold(), title_size);
        }
    }

    void StorageWindow::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_title_font.destroy(engine);
    }

    void StorageWindow::draw(engine::IEngine& engine,
                             WidgetTools& ui,
                             const FreetypeFont& font,
                             const Bag& bag,
                             const ItemDb& catalog) const {
        if (!m_open) {
            return;
        }
        reload_document();
        class StorageHost final : public UiViewHost {
          public:
            StorageHost(const StorageWindow& window, const Bag& bag, const ItemDb& catalog)
                : m_window(window), m_bag(bag), m_catalog(catalog) {}

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
                const ItemStack* item = nullptr;
                if (bind == "storage.inventory") {
                    item = m_window.inventory_item(m_bag, index);
                } else if (bind == "storage.items") {
                    const int absolute =
                        m_window.m_storage_page * protocol::kStorageSlotsPerRegister + index;
                    if (absolute >= 0 && absolute < static_cast<int>(m_window.m_storage.size())) {
                        item = &m_window.m_storage[static_cast<std::size_t>(absolute)];
                    }
                } else {
                    return false;
                }
                if (!item || item->iid == 0 || item->index <= 0) {
                    out.set_empty(true);
                    return true;
                }
                out.set_file(m_catalog.icon_filename(item->index, item->transform_level));
                out.set_count(item->count);
                out.set_account_bound(item->account_bound);
                out.set_character_bound(item->character_bound);
                out.set_enchant_protected(item->enchant_protected);
                const auto& marked = bind == "storage.inventory" ? m_window.m_marked_inventory_iids
                                                                 : m_window.m_marked_storage_iids;
                out.set_selected(std::find(marked.begin(), marked.end(), item->iid) !=
                                 marked.end());
                out.set_empty(out.file().empty());
                return true;
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                if (action == "storage.deposit") {
                    return m_window.m_selected_side == Side::Inventory &&
                           !m_window.m_marked_inventory_iids.empty();
                }
                if (action == "storage.withdraw") {
                    return m_window.m_selected_side == Side::Storage &&
                           !m_window.m_marked_storage_iids.empty();
                }
                return true;
            }

          private:
            const StorageWindow& m_window;
            const Bag& m_bag;
            const ItemDb& m_catalog;
        };

        UiVarMap values = vars(bag);
        const std::size_t marked = m_selected_side == Side::Inventory
                                       ? m_marked_inventory_iids.size()
                                       : m_marked_storage_iids.size();
        if (marked <= 1) {
            if (const ItemStack* item = selected_item(bag)) {
                if (const ItemDef* def = catalog.find(item->index)) {
                    values["storage.selected_name"] = def->name();
                }
            }
        }
        const StorageHost host(*this, bag, catalog);
        const FreetypeFont& body = m_body_font.ready() ? m_body_font : font;
        UiView::draw(engine,
                     ui,
                     m_v1,
                     m_x,
                     m_y,
                     m_w * m_scale,
                     m_h * m_scale,
                     m_scale,
                     values,
                     body,
                     m_title_font.ready() ? m_title_font : body,
                     &host,
                     false);
    }

    void StorageWindow::draw_tooltip(WidgetTools& ui,
                                     engine::IEngine& engine,
                                     const FreetypeFont& font,
                                     const ItemDb& catalog,
                                     float screen_w,
                                     float screen_h,
                                     int player_clazz,
                                     int player_level) const {
        if (!m_open || m_hover_item_index <= 0) {
            return;
        }
        const ItemDef* def = catalog.find(m_hover_item_index);
        const FreetypeFont& tip = font.ready() ? font : m_body_font;
        if (!def || !tip.ready()) {
            return;
        }
        const std::string markup = ItemTooltip::make_tip(*def,
                                                         catalog,
                                                         std::max(1, m_hover_item_count),
                                                         player_clazz,
                                                         player_level,
                                                         m_hover_prefix,
                                                         m_hover_cur_endurance,
                                                         m_hover_max_endurance,
                                                         m_hover_extras);
        ItemTooltip::draw(ui,
                          engine,
                          tip,
                          ItemTooltip::parse_markup(markup),
                          m_hover_mx,
                          m_hover_my,
                          screen_w,
                          screen_h);
    }

}  // namespace openkal::client
