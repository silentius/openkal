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

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace openkal::client {
    namespace {
        constexpr std::array<const char*, 2> kPaneBinds{"storage.inventory", "storage.items"};
        constexpr std::array<const char*, 2> kGridIds{"inventory", "storage_items"};
        constexpr std::array<const char*, 2> kTrackIds{"inventory_scroll_track",
                                                       "storage_scroll_track"};
        constexpr std::string_view kFilterAction = "storage.filter.";

        int storage_filter_index(std::string_view action) {
            return action.substr(0, kFilterAction.size()) == kFilterAction
                       ? inventory_filter_index(action.substr(kFilterAction.size()))
                       : -1;
        }

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
        m_scroll_drag = Side::None;
        m_balance.reset();
        m_pending_money.reset();
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
        if (!same_keeper || m_quantity_kind == QuantityKind::Deposit ||
            m_quantity_kind == QuantityKind::Withdraw) {
            m_quantity_kind = QuantityKind::None;
            m_quantity_iid = 0;
        }
        for (const protocol::StorageItem& item : info.items) {
            if (item.slot < m_storage.size()) {
                m_storage[item.slot] = item_stack_of(item);
            }
        }
        if (!same_keeper) {
            for (auto& pane : m_panes) {
                pane.row = 0;
                pane.filters = 0;
            }
            m_scroll_drag = Side::None;
            m_balance.reset();
            m_pending_money.reset();
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

    void StorageWindow::set_balance(const protocol::StorageBalance& balance) {
        if (m_open && balance.npc_id == m_npc_id && balance.geon >= 0) {
            m_balance = balance.geon;
        }
    }

    void StorageWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float scale = std::max(0.25f, m_scale);
        const auto frame = UiView::frame_style(m_v1);
        const float outer_w = (m_w + frame.pad_left() + frame.pad_right()) * scale;
        const float outer_h = (m_h + frame.pad_top() + frame.pad_bottom()) * scale;
        m_x = frame.pad_left() * scale + std::max(0.0f, (screen_w - outer_w) * 0.5f);
        m_y = frame.pad_top() * scale + std::max(0.0f, (screen_h - outer_h) * 0.5f);
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

    StorageWindow::Side StorageWindow::side_from_bind(std::string_view bind) {
        const auto found = std::find(kPaneBinds.begin(), kPaneBinds.end(), bind);
        return found == kPaneBinds.end() ? Side::None
                                         : static_cast<Side>(found - kPaneBinds.begin());
    }

    StorageWindow::PaneViews StorageWindow::make_views(const Bag& bag,
                                                       const ItemDb& catalog) const {
        PaneViews views;
        const auto matches = [&](const ItemStack& item, unsigned filters) {
            return item.iid != 0 && item.index > 0 && item.count > 0 &&
                   (filters == 0 ||
                    (filters & (1u << inventory_filter_category(catalog.find(item.index)))) != 0);
        };
        auto& inventory = views[0];
        const auto add_inventory = [&](const auto& section, int limit) {
            const int count = std::min(limit, static_cast<int>(section.size()));
            for (int i = 0; i < count; ++i) {
                const auto& item = section[i];
                if (!item.equipped && matches(item, m_panes[0].filters)) {
                    inventory.items[inventory.slots++] = &item;
                }
            }
        };
        add_inventory(bag.normal, kBagSlots);
        add_inventory(bag.quest, kOtherSectionSlots);
        if (m_panes[0].filters == 0) {
            const int enabled =
                static_cast<int>(std::count(std::begin(bag.compact_register_enabled),
                                            std::end(bag.compact_register_enabled),
                                            true));
            inventory.slots = std::max(inventory.slots, enabled * kSlotsPerPage);
        }
        auto& storage = views[1];
        for (int i = 0; i < protocol::kStorageCapacity; ++i) {
            const auto& item = m_storage[i];
            if (matches(item, m_panes[1].filters)) {
                const int cell = m_panes[1].filters == 0 ? i : storage.slots++;
                storage.items[cell] = &item;
            }
        }
        if (m_panes[1].filters == 0) {
            storage.slots = protocol::kStorageCapacity;
        }
        const auto name = [&catalog](const ItemStack& item) -> std::string_view {
            const auto* definition = catalog.find(item.index);
            return definition ? definition->name() : std::string_view{};
        };
        for (std::size_t i = 0; i < views.size(); ++i) {
            if (!m_panes[i].sorted) {
                continue;
            }
            auto& view = views[i];
            const auto end = view.items.begin() + view.slots;
            const auto occupied = std::remove(view.items.begin(), end, nullptr);
            std::stable_sort(view.items.begin(), occupied, [&](const auto* a, const auto* b) {
                return inventory_stack_precedes(*a, *b, name(*a), name(*b));
            });
            std::fill(occupied, end, nullptr);
        }
        return views;
    }

    int StorageWindow::visible_rows(Side side) const {
        const auto* grid = m_v1.find_widget(kGridIds[static_cast<std::size_t>(side)]);
        const auto* viewport = grid ? m_v1.find_widget(grid->viewport()) : nullptr;
        if (!grid || !viewport || grid->pitch_y() <= 0) {
            return 0;
        }
        const float available = viewport->h() - std::max(grid->frame_h(), grid->cell_h());
        return available < 0 ? 0 : 1 + static_cast<int>(available / grid->pitch_y());
    }

    int StorageWindow::max_scroll_row(Side side, const PaneView& view) const {
        const auto* grid = m_v1.find_widget(kGridIds[static_cast<std::size_t>(side)]);
        const int visible = visible_rows(side);
        if (!grid || grid->cols() <= 0 || visible <= 0) {
            return 0;
        }
        const int rows = (view.slots + grid->cols() - 1) / grid->cols();
        return std::max(0, std::min(grid->rows(), rows) - visible);
    }

    void StorageWindow::scroll_to_pointer(Side side, float my, const PaneView& view) {
        const auto index = static_cast<std::size_t>(side);
        const auto* track = m_v1.find_widget(kTrackIds[index]);
        if (track && track->h() > 0) {
            const float ratio = (my - m_y - track->y() * m_scale) / (track->h() * m_scale);
            m_panes[index].row = static_cast<int>(
                std::round(std::clamp(ratio, 0.0f, 1.0f) * max_scroll_row(side, view)));
        }
    }

    std::uint64_t StorageWindow::max_money(Side side, const Bag& bag) const {
        if (!m_balance) {
            return 0;
        }
        const auto wallet = std::max<std::int64_t>(0, bag.money);
        const auto maximum =
            side == Side::Inventory
                ? std::min(wallet, std::numeric_limits<std::int64_t>::max() - *m_balance)
                : std::min(*m_balance,
                           std::max<std::int64_t>(0, protocol::kStorageWalletLimit - wallet));
        return static_cast<std::uint64_t>(maximum);
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

    void StorageWindow::update_pointer(const engine::FrameInput& input,
                                       const Bag& bag,
                                       const ItemDb& catalog) {
        m_hover_widget.clear();
        m_pointer_pressed = input.mouse_left;
        set_hover(nullptr, input.mouse_x, input.mouse_y);
        const bool editing_quantity = quantity_open();
        if (!input.mouse_left || !m_open || editing_quantity) {
            m_dragging = false;
            m_scroll_drag = Side::None;
        }
        if (!m_open) {
            return;
        }
        if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
            return;
        }

        reload_document();
        const auto views = make_views(bag, catalog);
        if (m_scroll_drag != Side::None) {
            scroll_to_pointer(
                m_scroll_drag, input.mouse_y, views[static_cast<std::size_t>(m_scroll_drag)]);
            return;
        }
        for (std::size_t i = 0; i < views.size(); ++i) {
            const auto side = static_cast<Side>(i);
            const int maximum = max_scroll_row(side, views[i]);
            auto& row = m_panes[i].row;
            row = std::min(row, maximum);
            if (editing_quantity || input.mouse_scroll == 0) {
                continue;
            }
            const auto* grid = m_v1.find_widget(kGridIds[i]);
            const auto* viewport = grid ? m_v1.find_widget(grid->viewport()) : nullptr;
            if (viewport && WidgetTools::point_in_rect(input.mouse_x,
                                                       input.mouse_y,
                                                       m_x + viewport->x() * m_scale,
                                                       m_y + viewport->y() * m_scale,
                                                       viewport->w() * m_scale,
                                                       viewport->h() * m_scale)) {
                const int delta = static_cast<int>(std::copysign(
                    std::max(1.0f, std::abs(input.mouse_scroll)), input.mouse_scroll));
                row = std::clamp(row - delta, 0, maximum);
            }
        }
        const UiHit hit = UiView::hit(m_v1,
                                      nullptr,
                                      input.mouse_x,
                                      input.mouse_y,
                                      m_x,
                                      m_y,
                                      m_w * m_scale,
                                      m_h * m_scale,
                                      m_scale,
                                      vars(bag, views),
                                      nullptr,
                                      true);
        if (hit.kind() == UiHitKind::Button &&
            (!editing_quantity || hit.action() == "window.close" ||
             hit.action().rfind("storage.quantity.", 0) == 0)) {
            m_hover_widget = hit.widget_id();
        }
        const auto side = side_from_bind(hit.bind());
        if (!editing_quantity && hit.kind() == UiHitKind::IconGridCell && side != Side::None &&
            hit.index() >= 0) {
            const auto& view = views[static_cast<std::size_t>(side)];
            if (hit.index() < view.slots) {
                set_hover(view.items[hit.index()], input.mouse_x, input.mouse_y);
            }
        }
    }

    UiVarMap StorageWindow::vars(const Bag& bag, const PaneViews& views) const {
        UiVarMap values;
        values["storage.title"] = "Account Storage";
        values["storage.inventory_label"] = "Inventory";
        values["storage.storage_label"] = "Storage";
        values["storage.sort_label"] = "Sort";
        values["storage.money"] = std::to_string(std::max<std::int64_t>(0, bag.money));
        values["storage.account_money"] = m_balance ? std::to_string(*m_balance) : "-";
        values["storage.fee"] = "Item deposit: 60 Geon | Ctrl-click to mark more";
        for (std::size_t i = 0; i < views.size(); ++i) {
            const auto side = static_cast<Side>(i);
            const auto* grid = m_v1.find_widget(kGridIds[i]);
            const int row = std::min(m_panes[i].row, max_scroll_row(side, views[i]));
            const std::string prefix = kPaneBinds[i];
            values[prefix + ".scroll"] = std::to_string(grid ? row * grid->pitch_y() : 0.0f);
            for (std::size_t f = 0; f < kInventoryFilterIds.size(); ++f) {
                values[prefix + ".filter." + kInventoryFilterIds[f]] =
                    (m_panes[i].filters & (1u << f)) ? "true" : "false";
            }
        }
        const int used = static_cast<int>(
            std::count_if(m_storage.begin(), m_storage.end(), [](const ItemStack& item) {
                return item.iid != 0;
            }));
        values["storage.count_text"] =
            std::to_string(used) + "/" + std::to_string(protocol::kStorageCapacity);
        values["storage.selected_name"] = "Select an item";
        values["storage.quantity"] = std::to_string(m_quantity);
        values["storage.quantity_open"] = quantity_open() ? "true" : "false";
        values["storage.quantity_hint"] = "Type an amount, then press Enter";
        switch (m_quantity_kind) {
        case QuantityKind::DepositMoney:
            values["storage.quantity_prompt"] = "Deposit Geon";
            break;
        case QuantityKind::WithdrawMoney:
            values["storage.quantity_prompt"] = "Withdraw Geon";
            break;
        case QuantityKind::Withdraw:
            values["storage.quantity_prompt"] = "Withdraw quantity";
            break;
        default:
            values["storage.quantity_prompt"] = "Deposit quantity";
            break;
        }
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

    std::optional<protocol::StorageMoneyTransfer> StorageWindow::take_pending_money() {
        return std::exchange(m_pending_money, std::nullopt);
    }

    void
    StorageWindow::begin_quantity(QuantityKind kind, std::uint32_t iid, std::uint64_t maximum) {
        m_quantity_kind = kind;
        m_quantity = 1;
        m_quantity_max = std::max<std::uint64_t>(1, maximum);
        m_quantity_iid = iid;
        m_quantity_typing_started = false;
        m_dragging = false;
        m_scroll_drag = Side::None;
    }

    StorageClickResult StorageWindow::confirm_quantity() {
        if (!quantity_open() || m_quantity == 0) {
            return StorageClickResult::Consumed;
        }
        const auto kind = std::exchange(m_quantity_kind, QuantityKind::None);
        const std::uint64_t amount = std::clamp<std::uint64_t>(m_quantity, 1, m_quantity_max);
        if (kind == QuantityKind::DepositMoney || kind == QuantityKind::WithdrawMoney) {
            m_pending_money = protocol::StorageMoneyTransfer{
                m_npc_id, kind == QuantityKind::DepositMoney, static_cast<std::int64_t>(amount)};
            return StorageClickResult::MoneyTransfer;
        }
        m_pending_transfers.clear();
        m_pending_transfers.emplace_back(std::exchange(m_quantity_iid, 0),
                                         static_cast<std::uint32_t>(amount));
        m_marked_inventory_iids.clear();
        m_marked_storage_iids.clear();
        m_selected_side = Side::None;
        m_selected_iid = 0;
        return kind == QuantityKind::Deposit ? StorageClickResult::Deposit
                                             : StorageClickResult::Withdraw;
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
            m_quantity /= 10;
        }
        if (input.key_delete_pressed) {
            m_quantity = 0;
            m_quantity_typing_started = false;
        }
        const auto append_digit = [this](unsigned digit) {
            if (!m_quantity_typing_started) {
                m_quantity = 0;
                m_quantity_typing_started = true;
            }
            m_quantity = m_quantity > m_quantity_max / 10
                             ? m_quantity_max
                             : std::min(m_quantity * 10 + digit, m_quantity_max);
        };
        if (!input.key_ctrl) {
            if (input.text_char_count > 0) {
                for (int i = 0; i < input.text_char_count; ++i) {
                    const char ch = input.text_chars[i];
                    if (ch >= '0' && ch <= '9') {
                        append_digit(static_cast<unsigned>(ch - '0'));
                    }
                }
            } else {
                for (unsigned digit = 0; digit < 10; ++digit) {
                    if (input.key_digit_pressed[digit]) {
                        append_digit(digit);
                    }
                }
            }
        }
        return input.key_enter_pressed ? confirm_quantity() : StorageClickResult::Consumed;
    }

    StorageClickResult StorageWindow::handle_click(float mx,
                                                   float my,
                                                   WidgetTools& ui,
                                                   const Bag& bag,
                                                   const ItemDb& catalog,
                                                   bool ctrl_pressed) {
        if (!m_open) {
            return StorageClickResult::Ignored;
        }
        reload_document();
        const auto views = make_views(bag, catalog);
        const UiHit hit = UiView::hit(m_v1,
                                      &ui,
                                      mx,
                                      my,
                                      m_x,
                                      m_y,
                                      m_w * m_scale,
                                      m_h * m_scale,
                                      m_scale,
                                      vars(bag, views),
                                      nullptr,
                                      true);
        if (hit.kind() == UiHitKind::None) {
            return StorageClickResult::Ignored;
        }
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            close();
            return StorageClickResult::Close;
        }
        const auto& action = hit.action();
        if (quantity_open()) {
            if (action == "storage.quantity.decrease" && m_quantity > 1) {
                --m_quantity;
            } else if (action == "storage.quantity.increase" && m_quantity < m_quantity_max) {
                ++m_quantity;
            } else if (action == "storage.quantity.all") {
                m_quantity = m_quantity_max;
                m_quantity_typing_started = false;
            } else if (hit.kind() == UiHitKind::Input && hit.bind() == "storage.quantity") {
                m_quantity_typing_started = false;
            } else if (action == "storage.quantity.cancel") {
                m_quantity_kind = QuantityKind::None;
                m_quantity_iid = 0;
            } else if (action == "storage.quantity.confirm") {
                return confirm_quantity();
            }
            return StorageClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::TitleDrag) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return StorageClickResult::Consumed;
        }
        const auto side = side_from_bind(hit.bind());
        if (side != Side::None) {
            const auto i = static_cast<std::size_t>(side);
            auto& pane = m_panes[i];
            const auto& view = views[i];
            auto& marked =
                side == Side::Inventory ? m_marked_inventory_iids : m_marked_storage_iids;
            if (const int category = storage_filter_index(action); category >= 0) {
                pane.filters ^= 1u << category;
                pane.row = 0;
                marked.clear();
                if (m_selected_side == side) {
                    m_selected_side = Side::None;
                    m_selected_iid = 0;
                }
            } else if (action == "storage.sort") {
                pane.sorted = true;
                pane.row = 0;
            } else if (action == "storage.scroll.track") {
                m_scroll_drag = side;
                scroll_to_pointer(side, my, view);
            } else if (action == "storage.scroll.up" || action == "storage.scroll.down") {
                pane.row = std::clamp(pane.row + (action == "storage.scroll.up" ? -1 : 1),
                                      0,
                                      max_scroll_row(side, view));
            } else if (hit.kind() == UiHitKind::IconGridCell && hit.index() >= 0 &&
                       hit.index() < view.slots) {
                if (const auto* item = view.items[hit.index()]) {
                    auto& other =
                        side == Side::Inventory ? m_marked_storage_iids : m_marked_inventory_iids;
                    other.clear();
                    const auto found = std::find(marked.begin(), marked.end(), item->iid);
                    if (!ctrl_pressed) {
                        marked.assign(1, item->iid);
                    } else if (found == marked.end()) {
                        marked.push_back(item->iid);
                    } else {
                        marked.erase(found);
                    }
                    m_selected_side = marked.empty() ? Side::None : side;
                    m_selected_iid = marked.empty() ? 0 : marked.back();
                }
            }
            return StorageClickResult::Consumed;
        }
        if (action == "storage.money.deposit" || action == "storage.money.withdraw") {
            const bool deposit = action == "storage.money.deposit";
            const auto maximum = max_money(deposit ? Side::Inventory : Side::Storage, bag);
            if (maximum > 0) {
                begin_quantity(
                    deposit ? QuantityKind::DepositMoney : QuantityKind::WithdrawMoney, 0, maximum);
            }
        } else if (action == "storage.deposit" && m_selected_side == Side::Inventory) {
            if (m_marked_inventory_iids.size() == 1) {
                if (const auto* item = selected_item(bag); item && item->count > 1) {
                    begin_quantity(QuantityKind::Deposit, item->iid, item->count);
                    return StorageClickResult::Consumed;
                }
            }
            if (queue_marked_deposits(bag)) {
                return StorageClickResult::Deposit;
            }
        } else if (action == "storage.withdraw" && m_selected_side == Side::Storage) {
            if (m_marked_storage_iids.size() == 1) {
                if (const auto* item = selected_item(bag); item && item->count > 1) {
                    begin_quantity(QuantityKind::Withdraw, item->iid, item->count);
                    return StorageClickResult::Consumed;
                }
            }
            if (queue_marked_withdrawals()) {
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
        const auto views = make_views(bag, catalog);
        class StorageHost final : public UiViewHost {
          public:
            StorageHost(const StorageWindow& window,
                        const Bag& bag,
                        const ItemDb& catalog,
                        const PaneViews& views)
                : m_window(window), m_bag(bag), m_catalog(catalog), m_views(views) {
                set_pointer_hit(window.m_hover_widget, window.m_pointer_pressed);
            }

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
                const auto side = side_from_bind(bind);
                if (side == Side::None) {
                    return false;
                }
                const auto& view = m_views[static_cast<std::size_t>(side)];
                const auto* item = index >= 0 && index < view.slots ? view.items[index] : nullptr;
                if (!item || item->iid == 0 || item->index <= 0) {
                    out.set_empty(true);
                    return true;
                }
                out.set_file(m_catalog.icon_filename(item->index, item->transform_level));
                out.set_count(item->count);
                out.set_account_bound(item->account_bound);
                out.set_character_bound(item->character_bound);
                out.set_enchant_protected(item->enchant_protected);
                const auto& marked = side == Side::Inventory ? m_window.m_marked_inventory_iids
                                                             : m_window.m_marked_storage_iids;
                out.set_selected(std::find(marked.begin(), marked.end(), item->iid) !=
                                 marked.end());
                out.set_empty(out.file().empty());
                return true;
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                if (action == "storage.quantity.confirm") {
                    return m_window.m_quantity > 0;
                }
                if (action == "storage.deposit") {
                    return m_window.m_selected_side == Side::Inventory &&
                           !m_window.m_marked_inventory_iids.empty();
                }
                if (action == "storage.withdraw") {
                    return m_window.m_selected_side == Side::Storage &&
                           !m_window.m_marked_storage_iids.empty();
                }
                if (action == "storage.money.deposit" || action == "storage.money.withdraw") {
                    const auto side =
                        action == "storage.money.deposit" ? Side::Inventory : Side::Storage;
                    return m_window.max_money(side, m_bag) > 0;
                }
                return true;
            }

          private:
            const StorageWindow& m_window;
            const Bag& m_bag;
            const ItemDb& m_catalog;
            const PaneViews& m_views;
        };

        UiVarMap values = vars(bag, views);
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
        const FreetypeFont& body = m_body_font.ready() ? m_body_font : font;
        if (const auto* widget = m_v1.find_widget("selected_name");
            widget && widget->w() > 0 && body.ready()) {
            auto& name = values["storage.selected_name"];
            const float text_scale =
                widget->pixel_size() > 0
                    ? static_cast<float>(widget->pixel_size()) / body.pixel_size()
                    : 1.0f;
            if (body.text_width(name, text_scale) > widget->w()) {
                const float max_width =
                    std::max(1.0f, widget->w() - body.text_width("...", text_scale));
                const auto lines = layout_text_lines(body, name, max_width, text_scale);
                name.resize(lines.front().length);
                if (!name.empty() && name.back() == ' ') {
                    name.pop_back();
                }
                name += "...";
            }
        }
        const StorageHost host(*this, bag, catalog, views);
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
                     true);
        if (!quantity_open()) {
            for (std::size_t i = 0; i < views.size(); ++i) {
                const auto* track = m_v1.find_widget(kTrackIds[i]);
                const auto side = static_cast<Side>(i);
                const int visible = visible_rows(side);
                if (!track || visible == 0) {
                    continue;
                }
                const int maximum = max_scroll_row(side, views[i]);
                const float height = std::max(18.0f, track->h() * visible / (maximum + visible));
                const int row = std::min(m_panes[i].row, maximum);
                const float ratio = maximum > 0 ? static_cast<float>(row) / maximum : 0;
                engine.draw_ui_rect(m_x + (track->x() + 2) * m_scale,
                                    m_y + (track->y() + (track->h() - height) * ratio) * m_scale,
                                    (track->w() - 4) * m_scale,
                                    height * m_scale,
                                    .65f,
                                    .52f,
                                    .32f,
                                    1.0f);
            }
        }
    }

    void StorageWindow::draw_tooltip(WidgetTools& ui,
                                     engine::IEngine& engine,
                                     const FreetypeFont& font,
                                     const ItemDb& catalog,
                                     float screen_w,
                                     float screen_h,
                                     int player_clazz,
                                     int player_level) const {
        const FreetypeFont& tip = font.ready() ? font : m_body_font;
        if (!m_open || !tip.ready()) {
            return;
        }
        if (const auto* widget = m_v1.find_widget(m_hover_widget)) {
            if (const int category = storage_filter_index(widget->action()); category >= 0) {
                ItemTooltip::draw(ui,
                                  engine,
                                  tip,
                                  ItemTooltip::parse_markup(kInventoryFilterLabels[category]),
                                  m_hover_mx,
                                  m_hover_my,
                                  screen_w,
                                  screen_h);
                return;
            }
        }
        const ItemDef* def = catalog.find(m_hover_item_index);
        if (!def) {
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
