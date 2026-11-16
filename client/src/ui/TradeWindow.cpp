/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TradeWindow.h"

#include "openkal/client/ItemDb.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace openkal::client {
    namespace {
        int number(const std::string& text) {
            if (text.empty()) {
                return 0;
            }
            int value = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? value : -1;
        }
        void edit_number(std::string& text, bool& pristine, const engine::FrameInput& in) {
            if (in.key_backspace_pressed) {
                if (pristine) {
                    text.clear();
                } else if (!text.empty()) {
                    text.pop_back();
                }
                pristine = false;
            }
            for (int i = 0; i < in.text_char_count; ++i) {
                if (in.text_chars[i] >= '0' && in.text_chars[i] <= '9') {
                    if (pristine) {
                        text.clear();
                        pristine = false;
                    }
                    if (text.size() < 10) {
                        text += in.text_chars[i];
                    }
                }
            }
        }
    }  // namespace
    class TradeWindow::Host final : public UiViewHost {
      public:
        Host(const TradeWindow& window, const ItemDb* items = nullptr)
            : m_window(window), m_items(items) {}
        bool button_enabled(std::string_view action) const override {
            if (action == "trade.reject" || action == "trade.cancel") {
                return true;
            }
            if (m_window.m_pending_operation) {
                return false;
            }
            if (action == "trade.accept") {
                return !m_window.m_state.inviter;
            }
            if (action == "trade.confirm") {
                return !m_window.m_money_dirty && !m_window.m_state.confirmed;
            }
            return true;
        }
        bool resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
            const auto* item = m_window.item_at(bind, index);
            if (!item) {
                return true;
            }
            const int count =
                bind == "trade.inventory" ? m_window.remaining(item->iid) : item->count;
            if (count <= 0) {
                return true;
            }
            if (m_items) {
                out.set_file(m_items->icon_filename(
                    item->index,
                    std::max(1, item->attribute(protocol::TradeAttribute::AnimalLevel))));
            }
            out.set_count(count);
            out.set_high_contrast_count(true);
            out.set_empty(false);
            out.set_grayscale((item->flags & protocol::kTradeable) == 0);
            out.set_equipped((item->flags & protocol::kTradeEquipped) != 0);
            out.set_account_bound((item->flags & protocol::kTradeAccountBound) != 0);
            out.set_character_bound((item->flags & protocol::kTradeCharacterBound) != 0);
            out.set_enchant_protected(item->attribute(protocol::TradeAttribute::Protected) != 0);
            return true;
        }

      private:
        const TradeWindow& m_window;
        const ItemDb* m_items;
    };
    TradeWindow::TradeWindow(UiLayoutStore& layouts) : m_layouts(layouts) {}
    void TradeWindow::restore_offer() {
        m_offer.clear();
        for (const auto& item : m_state.own) {
            m_offer.push_back({item.iid, item.count, static_cast<std::uint8_t>(item.slot)});
        }
        if (!m_money_dirty) {
            m_money = std::to_string(m_state.money);
        }
    }
    void TradeWindow::update(const protocol::TradeState& state,
                             std::uint64_t serial,
                             float width,
                             float height,
                             float scale) {
        m_screen_w = width;
        m_screen_h = height;
        if (serial != m_serial) {
            const bool new_trade = state.token != m_state.token || !state.locked();
            const bool acknowledged =
                m_pending_operation && state.acknowledged == m_pending_operation;
            const bool changed = state.revision != m_state.revision;
            m_state = state;
            m_serial = serial;
            if (new_trade) {
                m_pending_operation = 0;
                m_money_dirty = false;
                m_money_focus = false;
                m_scroll = 0;
                m_quantity_iid = 0;
                m_notice.clear();
                reset_drag();
                m_last_click_iid = 0;
            }
            if (acknowledged) {
                m_pending_operation = 0;
            }
            if (new_trade || acknowledged || (changed && !m_pending_operation)) {
                restore_offer();
            }
        }
        UiView::reload(m_document,
                       &m_layouts.get(m_state.phase == protocol::TradePhase::Invitation
                                          ? "trade_invitation"
                                          : "trade"));
        UiView::reload(m_quantity_document, &m_layouts.get("trade_quantity"));
        const float w = m_document.window_w() + m_document.pad_left() + m_document.pad_right();
        const float h = m_document.window_h() + m_document.pad_top() + m_document.pad_bottom();
        m_scale = std::max(.1f, std::min({scale, (width - 16) / w, (height - 16) / h}));
        m_x = (width - w * m_scale) / 2 + m_document.pad_left() * m_scale;
        m_y = (height - h * m_scale) / 2 + m_document.pad_top() * m_scale;
        const float qw = m_quantity_document.window_w() + m_quantity_document.pad_left() +
                         m_quantity_document.pad_right();
        const float qh = m_quantity_document.window_h() + m_quantity_document.pad_top() +
                         m_quantity_document.pad_bottom();
        m_quantity_x = (width - qw * m_scale) / 2 + m_quantity_document.pad_left() * m_scale;
        m_quantity_y = (height - qh * m_scale) / 2 + m_quantity_document.pad_top() * m_scale;
        const auto* grid = m_document.find_widget("inventory_grid");
        const auto* viewport = m_document.find_widget("inventory_viewport");
        m_max_scroll = grid && viewport
                           ? std::max(0.f,
                                      std::ceil(m_state.inventory_slots / 6.f) * grid->pitch_y() -
                                          viewport->h())
                           : 0;
        m_scroll = std::clamp(m_scroll, 0.f, m_max_scroll);
    }
    UiVarMap TradeWindow::variables() const {
        return {{"trade.title", "Trade with " + m_state.partner},
                {"trade.inventory_label", "Inventory"},
                {"trade.other_label", m_state.partner + " offers"},
                {"trade.own_label", "Your offer"},
                {"trade.status", m_notice.empty() ? m_state.status : m_notice},
                {"trade.invitation",
                 m_state.inviter ? "Waiting for acceptance..."
                                 : "Would you like to trade with this player?"},
                {"trade.money", m_money + (m_money_focus ? "|" : "")},
                {"trade.partner_money", std::to_string(m_state.partner_money)},
                {"trade.own_status",
                 m_money_dirty       ? "Editing offer"
                 : m_state.confirmed ? "Accepted"
                                     : "Not accepted"},
                {"trade.other_status", m_state.partner_confirmed ? "Accepted" : "Not accepted"},
                {"trade.inventory_scroll", std::to_string(m_scroll)},
                {"trade.scroll",
                 std::to_string(static_cast<int>(m_scroll / 48) + 1) + " / " +
                     std::to_string(static_cast<int>(m_max_scroll / 48) + 1)},
                {"trade.quantity_title", "Item quantity"},
                {"trade.quantity_prompt",
                 "Amount (" + std::string(m_quantity_own ? "0" : "1") + " - " +
                     std::to_string(m_quantity_max) + ")"},
                {"trade.quantity", m_quantity + "|"}};
    }
    UiHit TradeWindow::hit(const engine::FrameInput& in, WidgetTools& ui, bool quantity) const {
        const auto& doc = quantity ? m_quantity_document : m_document;
        Host host(*this);
        return UiView::hit(doc,
                           &ui,
                           in.mouse_x,
                           in.mouse_y,
                           quantity ? m_quantity_x : m_x,
                           quantity ? m_quantity_y : m_y,
                           doc.window_w() * m_scale,
                           doc.window_h() * m_scale,
                           m_scale,
                           variables(),
                           &host);
    }
    const protocol::TradeItem* TradeWindow::item_at(std::string_view bind, int slot) const {
        const auto* list = bind == "trade.inventory" ? &m_state.available
                           : bind == "trade.own"     ? &m_state.own
                           : bind == "trade.other"   ? &m_state.other
                                                     : nullptr;
        if (!list) {
            return nullptr;
        }
        const auto found = std::find_if(
            list->begin(), list->end(), [slot](const auto& item) { return item.slot == slot; });
        return found == list->end() ? nullptr : &*found;
    }
    int TradeWindow::remaining(std::uint32_t iid) const {
        const auto item = std::find_if(m_state.available.begin(),
                                       m_state.available.end(),
                                       [iid](const auto& v) { return v.iid == iid; });
        if (item == m_state.available.end()) {
            return 0;
        }
        const auto offer = std::find_if(
            m_offer.begin(), m_offer.end(), [iid](const auto& v) { return v.iid == iid; });
        return std::max(0, item->count - (offer == m_offer.end() ? 0 : offer->count));
    }
    int TradeWindow::free_offer_slot() const {
        for (int i = 0; i < static_cast<int>(protocol::kTradeItems); ++i) {
            if (std::none_of(
                    m_offer.begin(), m_offer.end(), [i](const auto& v) { return v.slot == i; })) {
                return i;
            }
        }
        return -1;
    }
    std::optional<protocol::TradeRequest> TradeWindow::command(protocol::TradeCommand kind) {
        if (m_pending_operation && kind != protocol::TradeCommand::Cancel) {
            return std::nullopt;
        }
        protocol::TradeRequest request;
        request.command = kind;
        request.token = m_state.token;
        request.revision = m_state.revision;
        request.operation = m_next_operation++;
        m_pending_operation = request.operation;
        return request;
    }
    std::optional<protocol::TradeRequest> TradeWindow::publish() {
        const int money = number(m_money);
        if (money < 0 || money > 2000000000) {
            m_notice = "Enter a Geon amount from 0 to 2,000,000,000.";
            return std::nullopt;
        }
        auto request = command(protocol::TradeCommand::Offer);
        if (request) {
            request->items = m_offer;
            request->money = money;
            m_money_dirty = false;
            m_notice.clear();
        }
        return request;
    }
    void TradeWindow::reset_drag() {
        m_drag_iid = 0;
        m_drag_moved = false;
    }
    std::optional<protocol::TradeRequest>
    TradeWindow::edit_item(std::uint32_t iid, int slot, bool own) {
        const auto item = std::find_if(m_state.available.begin(),
                                       m_state.available.end(),
                                       [iid](const auto& v) { return v.iid == iid; });
        if (item == m_state.available.end() || !(item->flags & protocol::kTradeable) || slot < 0 ||
            slot >= static_cast<int>(protocol::kTradeItems)) {
            return std::nullopt;
        }
        m_quantity_iid = iid;
        m_quantity_slot = slot;
        m_quantity_own = own;
        m_quantity_pristine = true;
        m_quantity_max = own ? item->count : remaining(iid);
        if (m_quantity_max <= 0) {
            m_quantity_iid = 0;
            return std::nullopt;
        }
        const auto offered = std::find_if(
            m_offer.begin(), m_offer.end(), [iid](const auto& v) { return v.iid == iid; });
        const int initial = own && offered != m_offer.end() ? offered->count : m_quantity_max;
        m_quantity = std::to_string(initial);
        if (item->count == 1) {
            return apply_quantity(own ? 0 : 1);
        }
        return std::nullopt;
    }
    std::optional<protocol::TradeRequest> TradeWindow::apply_quantity(int count) {
        if (!m_quantity_iid || count < (m_quantity_own ? 0 : 1) || count > m_quantity_max) {
            return std::nullopt;
        }
        const auto found = std::find_if(
            m_offer.begin(), m_offer.end(), [&](const auto& v) { return v.iid == m_quantity_iid; });
        const int value =
            m_quantity_own ? count : count + (found == m_offer.end() ? 0 : found->count);
        if (found != m_offer.end()) {
            m_offer.erase(found);
        }
        if (value) {
            m_offer.push_back({m_quantity_iid, value, static_cast<std::uint8_t>(m_quantity_slot)});
        }
        m_quantity_iid = 0;
        return publish();
    }
    std::optional<protocol::TradeRequest> TradeWindow::handle(const engine::FrameInput& in,
                                                              WidgetTools& ui) {
        if (!m_state.locked()) {
            return std::nullopt;
        }
        m_clock += std::max(0.f, in.dt);
        if (in.key_escape_pressed) {
            if (m_quantity_iid) {
                m_quantity_iid = 0;
                return std::nullopt;
            }
            return command(protocol::TradeCommand::Cancel);
        }
        if (m_quantity_iid) {
            if (m_pending_operation) {
                return std::nullopt;
            }
            edit_number(m_quantity, m_quantity_pristine, in);
            const auto pointer = hit(in, ui, true);
            if (in.key_enter_pressed ||
                (in.mouse_left_clicked && pointer.action() == "trade.quantity_accept")) {
                return apply_quantity(number(m_quantity));
            }
            if (in.mouse_left_clicked && pointer.action() == "trade.quantity_cancel") {
                m_quantity_iid = 0;
            }
            return std::nullopt;
        }
        const auto pointer = hit(in, ui);
        if (in.mouse_left_clicked &&
            (pointer.kind() == UiHitKind::Close || pointer.action() == "trade.reject" ||
             pointer.action() == "trade.cancel")) {
            return command(protocol::TradeCommand::Cancel);
        }
        if (m_state.phase == protocol::TradePhase::Invitation) {
            if (in.mouse_left_clicked && pointer.action() == "trade.accept" && !m_state.inviter) {
                return command(protocol::TradeCommand::Accept);
            }
            return std::nullopt;
        }
        if (m_money_focus) {
            const auto old = m_money;
            edit_number(m_money, m_money_pristine, in);
            if (old != m_money) {
                m_money_dirty = true;
            }
            if (in.key_enter_pressed) {
                m_money_focus = false;
            }
        }
        if (m_pending_operation) {
            reset_drag();
            return std::nullopt;
        }
        const bool inventory_pointer =
            pointer.bind() == "trade.inventory" || pointer.widget_id() == "inventory_viewport";
        if (in.mouse_scroll && inventory_pointer && !m_drag_iid) {
            m_scroll = std::clamp(m_scroll - in.mouse_scroll * 48, 0.f, m_max_scroll);
        }
        if (in.mouse_left_clicked && pointer.action() == "trade.scroll_up") {
            m_scroll = std::max(0.f, m_scroll - 48);
        }
        if (in.mouse_left_clicked && pointer.action() == "trade.scroll_down") {
            m_scroll = std::min(m_max_scroll, m_scroll + 48);
        }
        if (in.mouse_left_clicked) {
            m_money_focus = pointer.action() == "trade.money";
            if (m_money_focus) {
                m_money_pristine = true;
            }
            if (!m_pending_operation && pointer.action() == "trade.confirm" && !m_money_dirty &&
                !m_state.confirmed) {
                return command(protocol::TradeCommand::Confirm);
            }
            if (!m_pending_operation && pointer.kind() == UiHitKind::IconGridCell &&
                pointer.bind() != "trade.other") {
                const auto* item = item_at(pointer.bind(), pointer.index());
                const bool own = pointer.bind() == "trade.own";
                if (item && (item->flags & protocol::kTradeable) &&
                    (own || remaining(item->iid) > 0)) {
                    const bool twice = m_last_click_iid == item->iid && m_last_click_own == own &&
                                       m_clock - m_last_click <= .35;
                    m_last_click_iid = item->iid;
                    m_last_click_own = own;
                    m_last_click = m_clock;
                    if (twice) {
                        reset_drag();
                        m_last_click_iid = 0;
                        auto existing =
                            std::find_if(m_offer.begin(), m_offer.end(), [&](const auto& v) {
                                return v.iid == item->iid;
                            });
                        const int slot =
                            existing != m_offer.end() ? existing->slot : free_offer_slot();
                        return edit_item(item->iid, slot, own);
                    }
                    m_drag_iid = item->iid;
                    m_drag_own = own;
                    m_drag_x = in.mouse_x;
                    m_drag_y = in.mouse_y;
                    m_drag_moved = false;
                }
            }
        }
        if (in.mouse_right_clicked && !m_pending_operation && pointer.bind() == "trade.own") {
            const auto* item = item_at(pointer.bind(), pointer.index());
            if (item) {
                m_offer.erase(std::remove_if(m_offer.begin(),
                                             m_offer.end(),
                                             [&](const auto& v) { return v.iid == item->iid; }),
                              m_offer.end());
                return publish();
            }
        }
        if (m_drag_iid) {
            const float dx = in.mouse_x - m_drag_x, dy = in.mouse_y - m_drag_y;
            m_drag_moved = m_drag_moved || dx * dx + dy * dy >= 16 * m_scale * m_scale;
            if (!in.mouse_left && !in.mouse_left_clicked) {
                const auto iid = m_drag_iid;
                const bool own = m_drag_own, moved = m_drag_moved;
                reset_drag();
                if (moved) {
                    m_last_click_iid = 0;
                    if (pointer.kind() == UiHitKind::IconGridCell &&
                        pointer.bind() == "trade.own") {
                        auto source = std::find_if(m_offer.begin(),
                                                   m_offer.end(),
                                                   [iid](const auto& v) { return v.iid == iid; });
                        auto destination =
                            std::find_if(m_offer.begin(), m_offer.end(), [&](const auto& v) {
                                return v.slot == pointer.index();
                            });
                        if (own && source != m_offer.end()) {
                            if (destination != m_offer.end()) {
                                std::swap(source->slot, destination->slot);
                            } else {
                                source->slot = static_cast<std::uint8_t>(pointer.index());
                            }
                            return publish();
                        }
                        if (destination == m_offer.end() || destination->iid == iid) {
                            return edit_item(iid, pointer.index(), false);
                        }
                    } else if (own && inventory_pointer) {
                        m_offer.erase(std::remove_if(m_offer.begin(),
                                                     m_offer.end(),
                                                     [iid](const auto& v) { return v.iid == iid; }),
                                      m_offer.end());
                        return publish();
                    }
                }
            }
        }
        if (m_money_dirty && !m_pending_operation) {
            return publish();
        }
        return std::nullopt;
    }
    void TradeWindow::send_failed() {
        m_pending_operation = 0;
        m_money_dirty = false;
        restore_offer();
        m_notice = "Could not send the trade request.";
    }
    void TradeWindow::draw(engine::IEngine& engine,
                           WidgetTools& ui,
                           const FreetypeFont& font,
                           const ItemDb& items,
                           const engine::FrameInput& input) const {
        if (!m_state.locked()) {
            return;
        }
        engine.draw_ui_rect(0, 0, m_screen_w, m_screen_h, 0, 0, 0, .7f);
        auto vars = variables();
        Host host(*this, &items);
        const auto pointer = hit(input, ui);
        host.set_pointer_hit(pointer.widget_id(), input.mouse_left);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_document.window_w() * m_scale,
                     m_document.window_h() * m_scale,
                     m_scale,
                     vars,
                     font,
                     font,
                     &host,
                     true);
        if (m_quantity_iid) {
            engine.draw_ui_rect(0, 0, m_screen_w, m_screen_h, 0, 0, 0, .5f);
            const auto quantity_pointer = hit(input, ui, true);
            host.set_pointer_hit(quantity_pointer.widget_id(), input.mouse_left);
            UiView::draw(engine,
                         ui,
                         m_quantity_document,
                         m_quantity_x,
                         m_quantity_y,
                         m_quantity_document.window_w() * m_scale,
                         m_quantity_document.window_h() * m_scale,
                         m_scale,
                         vars,
                         font,
                         font,
                         &host,
                         true);
            return;
        }
        const auto* item = item_at(pointer.bind(), pointer.index());
        if (m_drag_iid && m_drag_moved) {
            const auto source = std::find_if(m_state.available.begin(),
                                             m_state.available.end(),
                                             [&](const auto& v) { return v.iid == m_drag_iid; });
            if (source != m_state.available.end()) {
                ui.draw_item_tex(items.icon_filename(source->index),
                                 input.mouse_x - 16 * m_scale,
                                 input.mouse_y - 16 * m_scale,
                                 32 * m_scale,
                                 32 * m_scale,
                                 .85f);
            }
            return;
        }
        if (item && font.ready()) {
            if (const auto* def = items.find(item->index)) {
                using A = protocol::TradeAttribute;
                ItemTipExtras extras;
                extras.x_attack = item->attribute(A::Attack);
                extras.x_magic = item->attribute(A::Magic);
                extras.x_hit = item->attribute(A::Hit);
                extras.x_defense = item->attribute(A::Defense);
                extras.x_dodge = item->attribute(A::Dodge);
                extras.upgrade_level = item->attribute(A::UpgradeLevel);
                extras.mix_type = item->attribute(A::Mix);
                extras.bof = item->attribute(A::BeadOfFire);
                extras.qigong_perforated = item->attribute(A::Perforated);
                extras.qigong_effect = item->attribute(A::Qigong);
                extras.dragon_effect = item->attribute(A::Dragon);
                extras.random_stat_type =
                    static_cast<protocol::RandomItemStatType>(item->attribute(A::RandomType));
                extras.random_stat_value = item->attribute(A::RandomValue);
                extras.egg_level = std::max(1, item->attribute(A::AnimalLevel));
                extras.egg_exp = item->attribute(A::AnimalExp);
                extras.account_bound = (item->flags & protocol::kTradeAccountBound) != 0;
                extras.character_bound = (item->flags & protocol::kTradeCharacterBound) != 0;
                extras.enchant_protected = item->attribute(A::Protected) != 0;
                const auto markup = ItemTooltip::make_tip(*def,
                                                          items,
                                                          pointer.bind() == "trade.inventory"
                                                              ? std::max(1, remaining(item->iid))
                                                              : item->count,
                                                          -1,
                                                          1,
                                                          item->attribute(A::Prefix),
                                                          item->attribute(A::CurrentEndurance),
                                                          item->attribute(A::MaximumEndurance),
                                                          extras);
                ItemTooltip::draw(ui,
                                  engine,
                                  font,
                                  ItemTooltip::parse_markup(markup),
                                  input.mouse_x,
                                  input.mouse_y,
                                  m_screen_w,
                                  m_screen_h);
            }
        }
    }
}  // namespace openkal::client
