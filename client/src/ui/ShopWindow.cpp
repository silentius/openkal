/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/client/ShopWindow.h"

#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <limits>
#include <string>

namespace openkal::client {
    namespace {
        constexpr int kGridCapacity = 36;
        constexpr int kOfferCapacity = 35;

        [[nodiscard]] std::string money_text(std::int64_t value) {
            return std::to_string(std::max<std::int64_t>(0, value));
        }

        [[nodiscard]] ItemTipExtras hover_extras_for(const ItemStack& stack) {
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
            return extras;
        }
    }  // namespace

    void ShopWindow::reload_document() const {
        UiView::reload(m_v1, m_layout);
    }

    void ShopWindow::sync_from_layout() {
        reload_document();
        if (m_v1.window_w() > 0.0f) {
            m_w = m_v1.window_w();
        }
        if (m_v1.window_h() > 0.0f) {
            m_h = m_v1.window_h();
        }
    }

    void ShopWindow::close() {
        m_open = false;
        m_dragging = false;
        m_hover_widget.clear();
        m_pointer_pressed = false;
        m_hover_item_index = 0;
        m_hover_item_count = 0;
        m_hover_prefix = 0;
        m_hover_cur_endurance = -1;
        m_hover_max_endurance = -1;
        m_hover_extras = {};
        m_quantity_kind = QuantityKind::None;
        m_pending_quantity = 0;
        m_marked_inventory_iids.clear();
        m_pending_sales.clear();
    }

    void ShopWindow::show(const protocol::ShopItemList& list) {
        sync_from_layout();
        m_npc_id = list.npc_id;
        m_honor_shop = list.honor;
        m_offers.clear();
        std::copy_if(
            list.offers.begin(),
            list.offers.end(),
            std::back_inserter(m_offers),
            [](const protocol::ShopOffer& offer) { return offer.index != 0 && offer.buy != 0; });
        m_selected_side = Side::None;
        m_selected_iid = 0;
        m_selected_index = 0;
        m_offers_page = 0;
        m_inventory_register = 0;
        m_quantity_kind = QuantityKind::None;
        m_marked_inventory_iids.clear();
        m_pending_sales.clear();
        m_hover_item_index = 0;
        m_hover_item_count = 0;
        m_hover_prefix = 0;
        m_hover_cur_endurance = -1;
        m_hover_max_endurance = -1;
        m_hover_extras = {};
        m_open = true;
    }

    void ShopWindow::center_on_screen(float screen_w, float screen_h) {
        sync_from_layout();
        const float s = std::max(0.25f, m_scale);
        m_x = std::max(8.0f, (screen_w - m_w * s) * 0.5f);
        m_y = std::max(8.0f, (screen_h - m_h * s) * 0.42f);
    }

    bool ShopWindow::contains(float mx, float my) const {
        if (!m_open) {
            return false;
        }
        reload_document();
        const float s = std::max(0.25f, m_scale);
        return WidgetTools::window_outer_hit(
            mx, my, m_x, m_y, m_w * s, m_h * s, s, UiView::frame_style(m_v1));
    }

    void ShopWindow::update_pointer(const engine::FrameInput& input,
                                    const Bag& bag,
                                    const ItemDb& catalog) {
        m_hover_widget.clear();
        m_pointer_pressed = input.mouse_left;
        m_hover_item_index = 0;
        m_hover_item_count = 0;
        m_hover_prefix = 0;
        m_hover_cur_endurance = -1;
        m_hover_max_endurance = -1;
        m_hover_extras = {};
        m_hover_mx = input.mouse_x;
        m_hover_my = input.mouse_y;
        if (!m_open) {
            m_dragging = false;
            return;
        }
        if (!input.mouse_left) {
            m_dragging = false;
        } else if (m_dragging) {
            m_x = input.mouse_x - m_drag_dx;
            m_y = input.mouse_y - m_drag_dy;
        }
        if (m_dragging) {
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
                                      vars(bag, catalog),
                                      nullptr,
                                      true);
        m_hover_widget = hit.widget_id();
        if (m_quantity_kind != QuantityKind::None || hit.kind() != UiHitKind::IconGridCell) {
            return;
        }
        if (hit.bind() == "shop.offers") {
            const int absolute = m_offers_page * kOfferCapacity + hit.index();
            if (absolute >= 0 && absolute < static_cast<int>(m_offers.size())) {
                m_hover_item_index = m_offers[static_cast<std::size_t>(absolute)].index;
                m_hover_item_count = 1;
            }
        } else if (hit.bind() == "shop.inventory") {
            const auto slots = inventory_slots(bag, catalog);
            const int absolute = inventory_page(slots.size()) * kGridCapacity + hit.index();
            if (absolute >= 0 && absolute < static_cast<int>(slots.size())) {
                const ItemStack& stack = *slots[static_cast<std::size_t>(absolute)];
                m_hover_item_index = stack.index;
                m_hover_item_count = stack.count;
                m_hover_prefix = stack.prefix;
                m_hover_cur_endurance = stack.cur_end > 0 ? stack.cur_end : -1;
                m_hover_max_endurance = stack.max_end > 0 ? stack.max_end : -1;
                m_hover_extras = hover_extras_for(stack);
            }
        }
        const auto pet = m_pet_tooltip_info.find(m_hover_item_index);
        if (pet != m_pet_tooltip_info.end()) {
            m_hover_extras.pet_heal_per_two_seconds = pet->second.heal_per_two_seconds;
            m_hover_extras.pet_auto_pickup = pet->second.auto_pickup;
            m_hover_extras.lifetime_duration_seconds = pet->second.duration_seconds;
        }
    }

    ShopClickResult ShopWindow::handle_keyboard(const engine::FrameInput& input) {
        if (!m_open || m_quantity_kind == QuantityKind::None) {
            return ShopClickResult::Ignored;
        }
        if (input.key_escape_pressed) {
            m_quantity_kind = QuantityKind::None;
            return ShopClickResult::Consumed;
        }
        if (input.key_backspace_pressed) {
            m_quantity_pristine = false;
            m_quantity_typing_started = true;
            m_quantity = static_cast<std::uint16_t>(std::max(1, static_cast<int>(m_quantity) / 10));
        }
        if (input.key_delete_pressed) {
            m_quantity = 1;
            m_quantity_typing_started = false;
            m_quantity_pristine = true;
        }
        for (int i = 0; i < input.text_char_count; ++i) {
            const char ch = input.text_chars[i];
            if (ch < '0' || ch > '9') {
                continue;
            }
            m_quantity_pristine = false;
            if (!m_quantity_typing_started) {
                m_quantity = 0;
                m_quantity_typing_started = true;
            }
            const unsigned int next =
                static_cast<unsigned int>(m_quantity) * 10u + static_cast<unsigned int>(ch - '0');
            m_quantity = static_cast<std::uint16_t>(
                std::clamp(next, 1u, static_cast<unsigned int>(m_quantity_max)));
        }
        if (!input.key_enter_pressed) {
            return ShopClickResult::Consumed;
        }
        m_pending_quantity = m_quantity;
        const ShopClickResult result =
            m_quantity_kind == QuantityKind::Buy ? ShopClickResult::Buy : ShopClickResult::Sell;
        if (m_quantity_kind == QuantityKind::Sell) {
            queue_pending_sale();
        }
        m_quantity_kind = QuantityKind::None;
        return result;
    }

    std::vector<const ItemStack*> ShopWindow::inventory_slots(const Bag& bag,
                                                              const ItemDb& catalog) {
        std::vector<const ItemStack*> slots;
        for (const auto* section : {&bag.normal, &bag.quest}) {
            for (const ItemStack& stack : *section) {
                const auto* def = catalog.find(stack.index);
                if (sellable(stack, catalog) && def && !def->is_quest()) {
                    slots.push_back(&stack);
                }
            }
        }
        return slots;
    }

    int ShopWindow::inventory_page(std::size_t count) const {
        const int last = count == 0 ? 0 : static_cast<int>((count - 1) / kGridCapacity);
        return std::clamp(m_inventory_register, 0, last);
    }

    bool ShopWindow::sellable(const ItemStack& stack, const ItemDb& catalog) {
        const ItemDef* def = catalog.find(stack.index);
        return stack.iid != 0 && stack.count > 0 && !stack.equipped && def && def->sell() > 0;
    }

    const ItemStack* ShopWindow::selected_inventory_stack(const Bag& bag) const {
        return bag.find_instance(m_selected_iid);
    }

    bool ShopWindow::queue_marked_sales(const Bag& bag, const ItemDb& catalog) {
        bool queued = false;
        for (const std::uint32_t iid : m_marked_inventory_iids) {
            const auto* it = bag.find_instance(iid);
            if (!it || !sellable(*it, catalog)) {
                continue;
            }
            const int limited_count =
                std::min(it->count, static_cast<int>(std::numeric_limits<std::uint16_t>::max()));
            m_pending_sales.emplace_back(iid, static_cast<std::uint16_t>(limited_count));
            queued = true;
        }
        m_marked_inventory_iids.clear();
        m_selected_iid = 0;
        m_selected_side = Side::None;
        return queued;
    }

    void ShopWindow::queue_pending_sale() {
        if (m_pending_iid != 0 && m_pending_quantity > 0) {
            m_pending_sales.emplace_back(m_pending_iid, m_pending_quantity);
        }
    }

    bool ShopWindow::take_pending_sale(std::uint32_t& iid, std::uint16_t& quantity) {
        if (m_pending_sales.empty()) {
            return false;
        }
        iid = m_pending_sales.front().first;
        quantity = m_pending_sales.front().second;
        m_pending_sales.erase(m_pending_sales.begin());
        return true;
    }

    const ItemDef* ShopWindow::selected_def(const Bag& bag, const ItemDb& catalog) const {
        if (m_selected_side == Side::Offers) {
            return catalog.find(m_selected_index);
        }
        if (m_selected_side == Side::Inventory) {
            if (const ItemStack* stack = selected_inventory_stack(bag)) {
                return catalog.find(stack->index);
            }
        }
        return nullptr;
    }

    UiVarMap ShopWindow::vars(const Bag& bag, const ItemDb& catalog) const {
        UiVarMap result;
        result["shop.honor"] = m_honor_shop ? "true" : "false";
        result["shop.title"] = m_honor_shop ? "Honor Merchant" : "Merchant Shop";
        result["shop.inventory_label"] = "Inventory";
        result["shop.offers_label"] = "Merchant";
        result["shop.money"] = money_text(m_honor_shop ? m_honor_status.reward_points : bag.money);
        result["shop.money_label"] = m_honor_shop ? "Reward Points" : "Geon";
        result["shop.honor_rank"] =
            "Your rank: " + std::string(protocol::honor_rank_name(m_honor_status.rank));
        result["shop.selected_name"] = "Select an item";
        result["shop.selected_price"] = "";
        result["shop.selection_hint"] =
            m_honor_shop ? "Select an offer for rank and price." : "CTRL: select multiple items";
        result["shop.quantity"] = std::to_string(m_quantity);
        result["shop.quantity_open"] = m_quantity_kind == QuantityKind::None ? "false" : "true";
        result["shop.buy_quantity_open"] = m_quantity_kind == QuantityKind::Buy ? "true" : "false";
        result["shop.quantity_prompt"] =
            m_quantity_kind == QuantityKind::Sell ? "Sell quantity" : "Buy quantity";
        const auto slots = inventory_slots(bag, catalog);
        result["shop.inventory_register"] = std::to_string(inventory_page(slots.size()));
        result["shop.inventory_count"] = std::to_string(slots.size());
        result["shop.offers_page"] =
            std::to_string(m_offers_page + 1) + "/" +
            std::to_string(std::max(
                1, (static_cast<int>(m_offers.size()) + kOfferCapacity - 1) / kOfferCapacity));
        if (m_selected_side == Side::Inventory && m_marked_inventory_iids.size() > 1) {
            std::int64_t total = 0;
            for (const std::uint32_t iid : m_marked_inventory_iids) {
                const auto* it = bag.find_instance(iid);
                if (it) {
                    if (const ItemDef* marked_def = catalog.find(it->index)) {
                        total += static_cast<std::int64_t>(marked_def->sell()) * it->count;
                    }
                }
            }
            result["shop.selected_name"] =
                std::to_string(m_marked_inventory_iids.size()) + " items marked";
            result["shop.selected_price"] = "Sell: " + money_text(total);
            ui_fill_theme_vars(m_v1, result);
            return result;
        }
        if (const ItemDef* def = selected_def(bag, catalog)) {
            result["shop.selected_name"] = def->name();
            std::int64_t price = 0;
            const char* verb = "Price";
            if (m_selected_side == Side::Inventory) {
                price = def->sell();
                verb = "Sell";
            } else {
                for (const protocol::ShopOffer& offer : m_offers) {
                    if (offer.index == m_selected_index) {
                        price = offer.buy;
                        if (m_honor_shop) {
                            result["shop.selection_hint"] =
                                offer.minimum_rank == protocol::HonorRank::Unranked
                                    ? "Open to all ranks"
                                    : "Required rank: " + std::string(protocol::honor_rank_name(
                                                              offer.minimum_rank));
                        }
                        break;
                    }
                }
                verb = "Buy";
            }
            result["shop.selected_price"] = std::string(verb) + ": " + money_text(price);
        }
        ui_fill_theme_vars(m_v1, result);
        return result;
    }

    ShopClickResult ShopWindow::handle_click(float mx,
                                             float my,
                                             WidgetTools& ui,
                                             const Bag& bag,
                                             const ItemDb& catalog,
                                             bool ctrl_pressed) {
        if (!m_open) {
            return ShopClickResult::Ignored;
        }
        reload_document();
        const UiVarMap values = vars(bag, catalog);
        const UiHit hit = UiView::hit(m_v1,
                                      &ui,
                                      mx,
                                      my,
                                      m_x,
                                      m_y,
                                      m_w * m_scale,
                                      m_h * m_scale,
                                      m_scale,
                                      values,
                                      nullptr,
                                      true);
        if (hit.kind() == UiHitKind::None) {
            return ShopClickResult::Ignored;
        }
        if (hit.kind() == UiHitKind::Close || hit.action() == "window.close") {
            close();
            return ShopClickResult::Close;
        }
        if (hit.kind() == UiHitKind::TitleDrag && m_quantity_kind == QuantityKind::None) {
            m_dragging = true;
            m_drag_dx = mx - m_x;
            m_drag_dy = my - m_y;
            return ShopClickResult::Consumed;
        }

        if (m_quantity_kind != QuantityKind::None) {
            if (hit.action() == "shop.quantity.decrease" && m_quantity > 1) {
                --m_quantity;
                m_quantity_pristine = false;
            } else if (hit.action() == "shop.quantity.increase" && m_quantity < m_quantity_max) {
                ++m_quantity;
                m_quantity_pristine = false;
            } else if (m_quantity_kind == QuantityKind::Buy &&
                       (hit.action() == "shop.quantity.add10" ||
                        hit.action() == "shop.quantity.add100" ||
                        hit.action() == "shop.quantity.add1000")) {
                const int increment = hit.action() == "shop.quantity.add10"    ? 10
                                      : hit.action() == "shop.quantity.add100" ? 100
                                                                               : 1000;
                const int base = m_quantity_pristine ? 0 : m_quantity;
                m_quantity = static_cast<std::uint16_t>(
                    std::min(base + increment, static_cast<int>(m_quantity_max)));
                m_quantity_pristine = false;
            } else if (hit.action() == "shop.quantity.max") {
                m_quantity = m_quantity_max;
                m_quantity_pristine = false;
            } else if (hit.action() == "shop.quantity.cancel") {
                m_quantity_kind = QuantityKind::None;
            } else if (hit.action() == "shop.quantity.confirm") {
                m_pending_quantity = m_quantity;
                const ShopClickResult result = m_quantity_kind == QuantityKind::Buy
                                                   ? ShopClickResult::Buy
                                                   : ShopClickResult::Sell;
                if (m_quantity_kind == QuantityKind::Sell) {
                    queue_pending_sale();
                }
                m_quantity_kind = QuantityKind::None;
                return result;
            }
            return ShopClickResult::Consumed;
        }

        if (hit.kind() == UiHitKind::IconGridCell && hit.bind() == "shop.inventory") {
            if (m_honor_shop) {
                return ShopClickResult::Consumed;
            }
            const auto slots = inventory_slots(bag, catalog);
            const int absolute = inventory_page(slots.size()) * kGridCapacity + hit.index();
            if (absolute >= 0 && absolute < static_cast<int>(slots.size()) &&
                sellable(*slots[static_cast<std::size_t>(absolute)], catalog)) {
                const std::uint32_t iid = slots[static_cast<std::size_t>(absolute)]->iid;
                const auto marked =
                    std::find(m_marked_inventory_iids.begin(), m_marked_inventory_iids.end(), iid);
                if (!ctrl_pressed) {
                    m_marked_inventory_iids.clear();
                    m_marked_inventory_iids.push_back(iid);
                } else if (marked == m_marked_inventory_iids.end()) {
                    m_marked_inventory_iids.push_back(iid);
                } else {
                    m_marked_inventory_iids.erase(marked);
                }
                m_selected_side = Side::Inventory;
                m_selected_iid =
                    m_marked_inventory_iids.empty() ? 0 : m_marked_inventory_iids.back();
                m_selected_index =
                    static_cast<std::uint16_t>(slots[static_cast<std::size_t>(absolute)]->index);
            }
            return ShopClickResult::Consumed;
        }
        if (hit.kind() == UiHitKind::IconGridCell && hit.bind() == "shop.offers") {
            const int absolute = m_offers_page * kOfferCapacity + hit.index();
            if (absolute >= 0 && absolute < static_cast<int>(m_offers.size())) {
                m_selected_side = Side::Offers;
                m_selected_index = m_offers[static_cast<std::size_t>(absolute)].index;
                m_selected_iid = 0;
            }
            return ShopClickResult::Consumed;
        }
        if (hit.action() == "shop.offers.previous") {
            m_offers_page = std::max(0, m_offers_page - 1);
        } else if (hit.action() == "shop.offers.next") {
            const int last =
                m_offers.empty() ? 0 : static_cast<int>((m_offers.size() - 1) / kOfferCapacity);
            m_offers_page = std::min(last, m_offers_page + 1);
        } else if (hit.kind() == UiHitKind::Tab && hit.bind() == "shop.inventory_register") {
            const int page = hit.tab_id().empty() ? 0 : hit.tab_id()[0] - '0';
            if (page >= 0 && page < kMaxRegisters) {
                m_inventory_register = page;
                m_inventory_register = inventory_page(inventory_slots(bag, catalog).size());
            }
        } else if (hit.action() == "shop.buy" && m_selected_side == Side::Offers) {
            m_pending_index = m_selected_index;
            if (const ItemDef* def = catalog.find(m_selected_index); def && !def->is_stackable()) {
                m_pending_quantity = 1;
                return ShopClickResult::Buy;
            }
            m_quantity_kind = QuantityKind::Buy;
            m_quantity = 1;
            m_quantity_max = protocol::kMaxShopBuyQuantity;
            m_quantity_typing_started = false;
            m_quantity_pristine = true;
        } else if (!m_honor_shop && hit.action() == "shop.sell" &&
                   m_selected_side == Side::Inventory) {
            if (m_marked_inventory_iids.size() > 1) {
                return queue_marked_sales(bag, catalog) ? ShopClickResult::Sell
                                                        : ShopClickResult::Consumed;
            }
            if (const ItemStack* stack = selected_inventory_stack(bag);
                stack && sellable(*stack, catalog)) {
                m_pending_iid = stack->iid;
                if (const ItemDef* def = catalog.find(stack->index); def && !def->is_stackable()) {
                    m_pending_quantity = 1;
                    queue_pending_sale();
                    return ShopClickResult::Sell;
                }
                m_quantity_kind = QuantityKind::Sell;
                m_quantity = 1;
                m_quantity_typing_started = false;
                m_quantity_pristine = true;
                m_quantity_max = static_cast<std::uint16_t>(std::min(
                    stack->count, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
            }
        }
        return ShopClickResult::Consumed;
    }

    void ShopWindow::sync_fonts(engine::IEngine& engine) {
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

    void ShopWindow::destroy_fonts(engine::IEngine& engine) {
        m_body_font.destroy(engine);
        m_title_font.destroy(engine);
    }

    void ShopWindow::draw(engine::IEngine& engine,
                          WidgetTools& ui,
                          const FreetypeFont& font,
                          const Bag& bag,
                          const ItemDb& catalog) const {
        if (!m_open) {
            return;
        }
        reload_document();
        class ShopHost final : public UiViewHost {
          public:
            ShopHost(const ShopWindow& window, const Bag& bag, const ItemDb& catalog)
                : m_window(window), m_catalog(catalog), m_slots(inventory_slots(bag, catalog)) {
                set_pointer_hit(window.m_hover_widget, window.m_pointer_pressed);
            }

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
                if (bind == "shop.currency") {
                    out.set_file("item001.bmp");
                    out.set_empty(out.file().empty());
                    return true;
                }
                if (bind == "shop.inventory") {
                    const int absolute =
                        m_window.inventory_page(m_slots.size()) * kGridCapacity + index;
                    if (absolute < 0 || absolute >= static_cast<int>(m_slots.size())) {
                        out.set_empty(true);
                        return true;
                    }
                    const ItemStack& stack = *m_slots[static_cast<std::size_t>(absolute)];
                    out.set_file(m_catalog.icon_filename(stack.index, stack.transform_level));
                    out.set_count(stack.count);
                    out.set_selected(std::find(m_window.m_marked_inventory_iids.begin(),
                                               m_window.m_marked_inventory_iids.end(),
                                               stack.iid) !=
                                     m_window.m_marked_inventory_iids.end());
                    out.set_empty(out.file().empty());
                    return true;
                }
                if (bind == "shop.offers") {
                    index += m_window.m_offers_page * kOfferCapacity;
                    if (index < 0 || index >= static_cast<int>(m_window.m_offers.size())) {
                        out.set_empty(true);
                        return true;
                    }
                    const protocol::ShopOffer& offer =
                        m_window.m_offers[static_cast<std::size_t>(index)];
                    out.set_file(m_catalog.icon_filename(offer.index));
                    out.set_selected(m_window.m_selected_side == Side::Offers &&
                                     m_window.m_selected_index == offer.index);
                    out.set_empty(out.file().empty());
                    return true;
                }
                return false;
            }

            [[nodiscard]] bool button_enabled(std::string_view action) const override {
                if (action == "shop.offers.previous") {
                    return m_window.m_offers_page > 0;
                }
                if (action == "shop.offers.next") {
                    return (m_window.m_offers_page + 1) * kOfferCapacity <
                           static_cast<int>(m_window.m_offers.size());
                }
                if (action == "shop.buy") {
                    return m_window.m_selected_side == Side::Offers;
                }
                if (action == "shop.sell") {
                    return m_window.m_selected_side == Side::Inventory &&
                           !m_window.m_marked_inventory_iids.empty();
                }
                return true;
            }

          private:
            const ShopWindow& m_window;
            const ItemDb& m_catalog;
            const std::vector<const ItemStack*> m_slots;
        };

        UiVarMap values = vars(bag, catalog);
        const FreetypeFont& body = m_body_font.ready() ? m_body_font : font;
        // Item names can exceed the fixed detail panel even at the native font size.
        if (const UiWidget* name = m_v1.find_widget("selected_name");
            name && name->w() > 0 && body.ready()) {
            std::string& shown = values["shop.selected_name"];
            if (body.text_width(shown) > name->w()) {
                while (!shown.empty() && body.text_width(shown + "...") > name->w()) {
                    shown.pop_back();
                }
                shown += "...";
            }
        }
        const ShopHost host(*this, bag, catalog);
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
    }

    void ShopWindow::draw_tooltip(WidgetTools& ui,
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
