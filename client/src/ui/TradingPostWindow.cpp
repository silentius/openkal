/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TradingPostWindow.h"

#include "openkal/client/GeonColor.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/TextWrap.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <charconv>
#include <ctime>
#include <set>

namespace openkal::client {
    namespace {
        using namespace protocol;
        std::int64_t number(const std::string& s) {
            std::int64_t n = 0;
            auto p = std::from_chars(s.data(), s.data() + s.size(), n);
            return p.ec == std::errc{} && p.ptr == s.data() + s.size() ? n : 0;
        }
        std::string money(std::int64_t value) {
            auto s = std::to_string(value);
            for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
                s.insert(static_cast<std::size_t>(i), ",");
            }
            return s;
        }
        void
        edit(std::string& s, bool& pristine, const engine::FrameInput& in, std::int64_t maximum) {
            if (in.key_backspace_pressed) {
                if (pristine) {
                    s.clear();
                } else if (!s.empty()) {
                    s.pop_back();
                }
                pristine = false;
            }
            for (int i = 0; i < in.text_char_count; ++i) {
                if (in.text_chars[i] >= '0' && in.text_chars[i] <= '9') {
                    if (pristine) {
                        s.clear();
                        pristine = false;
                    }
                    auto next = s + in.text_chars[i];
                    if (next.size() <= 10 && number(next) <= maximum) {
                        s = std::move(next);
                    }
                }
            }
        }
        constexpr const char* filter_names[] = {"class", "type", "prefix", "grade", "order"};
        TradingPostType type_of(const ItemDef& d) {
            return trading_post_type(d.class_big(), d.class_small(), d.usable());
        }
    }  // namespace
    class TradingPostWindow::Host final : public UiViewHost {
      public:
        Host(const TradingPostWindow& w, const ItemDb* db = nullptr) : m_window(w), m_items(db) {}
        bool button_enabled(std::string_view action) const override {
            if (action == "window.close") {
                return true;
            }
            if (m_window.m_pending) {
                return false;
            }
            if (action == "trading_post.filter_prefix") {
                return trading_post_prefixes(m_window.m_filter.type);
            }
            if (action == "trading_post.prev") {
                return m_window.m_filter.page > 0;
            }
            if (action == "trading_post.next") {
                return m_window.m_view == TradingPostView::Sell
                           ? (m_window.m_filter.page + 1) * kTradingPostPageSize <
                                 m_window.m_inventory.size()
                           : m_window.m_state.more;
            }
            if (action == "trading_post.collect_all" ||
                (action == "trading_post.act" && m_window.m_view == TradingPostView::Collection)) {
                return m_window.m_view == TradingPostView::Collection &&
                       m_window.m_collection_npc &&
                       (!m_window.m_state.entries.empty() || m_window.m_state.more ||
                        m_window.m_filter.page > 0);
            }
            if (action == "trading_post.collect_selected") {
                return m_window.m_view == TradingPostView::Collection &&
                       m_window.m_collection_npc && m_window.selected();
            }
            if (action == "trading_post.act") {
                return m_window.selected() &&
                       (m_window.m_view != TradingPostView::Sell || number(m_window.m_price) > 0);
            }
            return true;
        }
        bool resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override {
            const auto* e = m_window.entry(bind, index);
            if (!e) {
                return true;
            }
            const bool proceeds = m_window.m_view == TradingPostView::Collection &&
                                  e->delivery == TradingPostDelivery::Proceeds;
            if (proceeds) {
                out.set_file("dragon_dialog/geons.bmp");
            } else if (m_items) {
                out.set_file(m_items->icon_filename(
                    e->item.index, std::max(1, e->item.attribute(TradeAttribute::AnimalLevel))));
            }
            out.set_selected(e->id == m_window.m_selected);
            out.set_count(proceeds ? 0 : e->item.count);
            out.set_high_contrast_count(true);
            out.set_empty(false);
            return true;
        }

      private:
        const TradingPostWindow& m_window;
        const ItemDb* m_items;
    };
    TradingPostWindow::TradingPostWindow(UiLayoutStore& layouts) : m_layouts(layouts) {}
    void TradingPostWindow::set_open(bool value) {
        m_open = value;
        m_center_pending = true;
        m_status.clear();
        m_refresh = value;
        m_quantity_open = false;
        m_dropdown = -1;
        m_price_focus = false;
        m_selected = 0;
        m_dragging = false;
        m_collection_npc = 0;
    }
    void TradingPostWindow::show_collection(std::uint32_t npc) {
        set_open(true);
        m_view = TradingPostView::Collection;
        m_filter.page = 0;
        m_collection_npc = npc;
    }
    std::vector<TradingPostWindow::Option> TradingPostWindow::options(int which,
                                                                      const ItemDb& db) const {
        if (which == 0) {
            return {{-1, "Any"}, {0, "Knight"}, {2, "Archer"}, {1, "Magician"}, {3, "Thief"}};
        }
        if (which == 1) {
            return {{-1, "Any"},
                    {0, "Other"},
                    {1, "Quest"},
                    {2, "Consumables"},
                    {3, "Weapon"},
                    {4, "Head Gear"},
                    {5, "Chest"},
                    {6, "Gloves"},
                    {7, "Shorts"},
                    {8, "Boots"}};
        }
        if (which == 4) {
            return {{0, "Price: highest first"},
                    {1, "Price: lowest first"},
                    {2, "Enhancement: highest first"},
                    {3, "Enhancement: lowest first"}};
        }
        std::vector<Option> out{{-1, "Any"}};
        if (which == 2) {
            for (const auto& p : db.prefixes().by_index()) {
                if (!p.second.name().empty()) {
                    out.emplace_back(p.first, p.second.name());
                }
            }
            std::sort(out.begin() + 1, out.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        } else {
            std::set<int> grades;
            for (const auto& pair : db.by_index()) {
                const auto& d = pair.second;
                if (m_filter.clazz >= 0 && d.class_limit() >= 0 &&
                    m_filter.clazz != d.class_limit()) {
                    continue;
                }
                if (m_filter.type != TradingPostType::Any && type_of(d) != m_filter.type) {
                    continue;
                }
                if (d.grade() > 0) {
                    grades.insert(d.grade());
                }
            }
            for (auto g : grades) {
                out.emplace_back(g, "Grade " + std::to_string(g));
            }
        }
        return out;
    }
    void TradingPostWindow::build_inventory(const Bag& bag, const ItemDb& db) {
        m_inventory.clear();
        for (const auto& stack : bag.normal) {
            const auto* d = db.find(stack.index);
            if (!d || !stack.iid || stack.count <= 0 || stack.equipped || stack.account_bound ||
                stack.character_bound || stack.index == 31 || d->is_task() || d->is_quest()) {
                continue;
            }
            if (m_filter.clazz >= 0 && d->class_limit() >= 0 &&
                m_filter.clazz != d->class_limit()) {
                continue;
            }
            if (m_filter.type != TradingPostType::Any && type_of(*d) != m_filter.type) {
                continue;
            }
            if (m_filter.grade >= 0 && m_filter.grade != d->grade()) {
                continue;
            }
            if (trading_post_prefixes(m_filter.type) && m_filter.prefix >= 0 &&
                m_filter.prefix != stack.prefix) {
                continue;
            }
            TradingPostEntry e;
            e.id = stack.iid;
            e.item.iid = stack.iid;
            e.item.index = stack.index;
            e.item.count = stack.count;
            e.item.attributes = {stack.prefix,
                                 stack.cur_end,
                                 stack.max_end,
                                 stack.x_attack,
                                 stack.x_magic,
                                 stack.x_hit,
                                 stack.x_defense,
                                 stack.x_dodge,
                                 stack.upgrade_level,
                                 stack.upgrade_rate,
                                 stack.mix_type,
                                 stack.bof,
                                 stack.qigong_perforated,
                                 stack.qigong_effect,
                                 stack.dragon_effect,
                                 stack.random_stat_type,
                                 stack.random_stat_value,
                                 stack.transform_level,
                                 stack.transform_exp,
                                 stack.enchant_protected ? 1 : 0};
            m_inventory.push_back(e);
        }
    }
    void TradingPostWindow::update(const TradingPostState& state,
                                   std::uint64_t serial,
                                   const engine::FrameInput& in,
                                   float scale,
                                   const Bag& bag,
                                   const ItemDb& db) {
        if (!m_open) {
            return;
        }
        const bool resized =
            m_width != static_cast<float>(in.width) || m_height != static_cast<float>(in.height);
        const auto old_scale = m_scale;
        m_width = static_cast<float>(in.width);
        m_height = static_cast<float>(in.height);
        UiView::reload(m_document, &m_layouts.get("trading_post"));
        UiView::reload(m_quantity_document, &m_layouts.get("trading_post_quantity"));
        UiView::reload(m_option_document, &m_layouts.get("trading_post_option"));
        m_scale = std::max(.2f,
                           std::min({scale,
                                     m_width / (m_document.window_w() + m_document.pad_left() +
                                                m_document.pad_right()),
                                     m_height / (m_document.window_h() + m_document.pad_top() +
                                                 m_document.pad_bottom())}));
        if (m_center_pending || resized || old_scale != m_scale) {
            m_center_pending = false;
            m_x = (m_width -
                   (m_document.window_w() + m_document.pad_left() + m_document.pad_right()) *
                       m_scale) /
                      2 +
                  m_document.pad_left() * m_scale;
            m_y = (m_height -
                   (m_document.window_h() + m_document.pad_top() + m_document.pad_bottom()) *
                       m_scale) /
                      2 +
                  m_document.pad_top() * m_scale;
        }
        m_x = std::clamp(
            m_x,
            m_document.pad_left() * m_scale,
            std::max(m_document.pad_left() * m_scale,
                     m_width - (m_document.window_w() + m_document.pad_right()) * m_scale));
        m_y = std::clamp(
            m_y,
            m_document.pad_top() * m_scale,
            std::max(m_document.pad_top() * m_scale,
                     m_height - (m_document.window_h() + m_document.pad_bottom()) * m_scale));
        m_qx = (m_width - (m_quantity_document.window_w() + m_quantity_document.pad_left() +
                           m_quantity_document.pad_right()) *
                              m_scale) /
                   2 +
               m_quantity_document.pad_left() * m_scale;
        m_qy = (m_height - (m_quantity_document.window_h() + m_quantity_document.pad_top() +
                            m_quantity_document.pad_bottom()) *
                               m_scale) /
                   2 +
               m_quantity_document.pad_top() * m_scale;
        if (serial != m_serial) {
            m_serial = serial;
            if (state.acknowledged == m_pending && m_pending) {
                m_pending = 0;
                m_state = state;
                if (m_view != TradingPostView::Sell) {
                    m_filter.page = state.page;
                }
                m_status = state.status;
                // The chosen listing may have disappeared after a competing purchase.
                if (!selected()) {
                    m_selected = 0;
                }
            }
        }
        if (m_pending) {
            m_pending_seconds += std::max(0.f, in.dt);
            if (m_pending_seconds > 15) {
                m_pending = 0;
                m_refresh = true;
                m_status = "The request timed out. Refreshing the trading post...";
            }
        }
        if (m_view == TradingPostView::Sell) {
            build_inventory(bag, db);
            const auto pages = std::max<std::size_t>(
                1, (m_inventory.size() + kTradingPostPageSize - 1) / kTradingPostPageSize);
            m_filter.page =
                static_cast<std::uint32_t>(std::min<std::size_t>(m_filter.page, pages - 1));
        }
    }
    std::pair<float, float> TradingPostWindow::dropdown_origin() const {
        if (m_dropdown < 0 || m_dropdown > 4) {
            return {m_x, m_y};
        }
        const auto* field =
            m_document.find_widget(std::string("filter_") + filter_names[m_dropdown]);
        if (!field) {
            return {m_x, m_y};
        }
        const auto height = 26.f * std::min<std::size_t>(8, m_options.size()) * m_scale;
        const auto x = m_x + field->x() * m_scale;
        auto y = m_y + (field->y() + field->h()) * m_scale;
        if (y + height > m_height) {
            y = m_y + field->y() * m_scale - height;
        }
        return {x, std::max(0.f, y)};
    }
    std::vector<TradingPostEntry> TradingPostWindow::entries() const {
        if (m_view != TradingPostView::Sell) {
            return m_state.entries;
        }
        const auto begin = std::min(m_inventory.size(),
                                    static_cast<std::size_t>(m_filter.page) * kTradingPostPageSize);
        const auto end = std::min(m_inventory.size(), begin + kTradingPostPageSize);
        return {m_inventory.begin() + static_cast<std::ptrdiff_t>(begin),
                m_inventory.begin() + static_cast<std::ptrdiff_t>(end)};
    }
    const TradingPostEntry* TradingPostWindow::selected() const {
        const auto& list = m_view == TradingPostView::Sell ? m_inventory : m_state.entries;
        const auto found = std::find_if(
            list.begin(), list.end(), [&](const auto& e) { return e.id == m_selected; });
        return found == list.end() ? nullptr : &*found;
    }
    const TradingPostEntry* TradingPostWindow::entry(std::string_view bind, int index) const {
        constexpr std::string_view prefix = "trading_post.row";
        if (bind.substr(0, prefix.size()) != prefix || bind.size() != prefix.size() + 1 ||
            index < 0 || index >= kTradingPostColumns) {
            return nullptr;
        }
        const int row = bind.back() - '0';
        if (row < 0 || row > 5) {
            return nullptr;
        }
        const auto& list = m_view == TradingPostView::Sell ? m_inventory : m_state.entries;
        const auto offset = (m_view == TradingPostView::Sell
                                 ? static_cast<std::size_t>(m_filter.page) * kTradingPostPageSize
                                 : 0) +
                            static_cast<std::size_t>(row * kTradingPostColumns + index);
        return offset < list.size() ? &list[offset] : nullptr;
    }
    UiVarMap TradingPostWindow::variables() const {
        const auto pages =
            m_view == TradingPostView::Sell
                ? std::max<std::size_t>(
                      1, (m_inventory.size() + kTradingPostPageSize - 1) / kTradingPostPageSize)
                : m_state.total_pages;
        UiVarMap v{
            {"trading_post.title", "Trading Post"},
            {"trading_post.tab_buy", "Buy"},
            {"trading_post.tab_sell", "Sell"},
            {"trading_post.tab_own", "My Offers"},
            {"trading_post.tab_collection", "Collection"},
            {"trading_post.collect_all", "Collect All"},
            {"trading_post.collect_selected", "Collect selected"},
            {"trading_post.class_label", "Class"},
            {"trading_post.type_label", "Type"},
            {"trading_post.prefix_label", "Prefix"},
            {"trading_post.grade_label", "Grade"},
            {"trading_post.order_label", "Order"},
            {"trading_post.price_label", "Unit price (Geons)"},
            {"trading_post.price",
             m_price_focus     ? m_price + "|"
             : m_price.empty() ? "Enter price"
                               : money(number(m_price))},
            {"trading_post.price_color",
             m_price.empty() && !m_price_focus ? "#9B8B70" : geon_color(number(m_price)).hex},
            {"trading_post.selected_color", "#EAE3D4"},
            {"trading_post.details_color", "#EAE3D4"},
            {"trading_post.inspect", m_view == TradingPostView::Sell ? "false" : "true"},
            {"trading_post.sell", m_view == TradingPostView::Sell ? "true" : "false"},
            {"trading_post.act",
             m_view == TradingPostView::Buy    ? "Buy selected"
             : m_view == TradingPostView::Sell ? "List selected"
             : m_view == TradingPostView::Own  ? "Withdraw offer"
             : m_collection_npc                ? "Collect All"
                                               : "Visit Trading Post Agent"},
            {"trading_post.page",
             std::to_string(m_filter.page + 1) + "/" + (pages ? std::to_string(pages) : "?")},
            {"trading_post.quantity_title",
             m_view == TradingPostView::Sell ? "Offer quantity" : "Buy quantity"},
            {"trading_post.quantity", m_quantity + "|"},
            {"trading_post.quantity_prompt",
             "Quantity (1 - " + std::to_string(m_quantity_max) + ")"}};
        for (int i = 0; i < 5; ++i) {
            v[std::string("trading_post.") + filter_names[i]] = "Any";
        }
        v["trading_post.buy"] = m_view == TradingPostView::Buy ? "true" : "false";
        v["trading_post.filters"] =
            m_view == TradingPostView::Buy || m_view == TradingPostView::Sell ? "true" : "false";
        v["trading_post.collection"] = m_view == TradingPostView::Collection ? "true" : "false";
        v["trading_post.own"] = m_view == TradingPostView::Own ? "true" : "false";
        v["trading_post.collection_help"] = "Item icons: purchases / returns.";
        v["trading_post.collection_help2"] = "Collect at a Trading Post Agent.";
        v["trading_post.own_help"] = "Select an offer to withdraw it.";
        const char* views[] = {"buy", "sell", "own", "collection"};
        for (int i = 0; i < 4; ++i) {
            v[std::string("trading_post.") + views[i] + "_color"] =
                static_cast<int>(m_view) == i ? "#F5D68A" : "#BDAE92";
        }
        const auto list = entries();
        for (int row = 0; row < 6; ++row) {
            const auto start = static_cast<std::size_t>(row * kTradingPostColumns),
                       end = std::min(start + kTradingPostColumns, list.size());
            std::string label;
            if (start < end && m_view == TradingPostView::Collection) {
                label = "Items and sale proceeds awaiting NPC collection";
            } else if (start < end && m_view != TradingPostView::Sell) {
                auto low = list[start].unit_price, high = low;
                for (auto n = start; n < end; ++n) {
                    low = std::min(low, list[n].unit_price);
                    high = std::max(high, list[n].unit_price);
                }
                label = money(low) + (low == high ? "" : " - " + money(high)) + " Geons / item";
            }
            v["trading_post.range" + std::to_string(row)] = label;
        }
        const auto* e = selected();
        if (e) {
            v["trading_post.selected"] = std::to_string(e->item.count) + " available";
            if (m_view == TradingPostView::Collection) {
                v["trading_post.selected"] =
                    e->delivery == TradingPostDelivery::Proceeds
                        ? "Sale proceeds: " + money(e->unit_price * e->item.count) + " Geons"
                    : e->delivery == TradingPostDelivery::Purchase ? "Purchased items"
                                                                   : "Withdrawn items";
            }
            const auto price = m_view == TradingPostView::Sell ? number(m_price) : e->unit_price;
            v["trading_post.details"] = money(price * e->item.count) + " Geons total";
            if (m_view != TradingPostView::Collection) {
                v["trading_post.details_color"] = geon_color(price * e->item.count).hex;
            } else if (e->delivery == TradingPostDelivery::Proceeds) {
                v["trading_post.selected_color"] = geon_color(price * e->item.count).hex;
            }
            v["trading_post.quantity_unit_price"] = "Unit price: " + money(price) + " Geons";
            v["trading_post.quantity_total"] = money(price * number(m_quantity)) + " Geons total";
        }
        std::string status = m_status;
        if (status.empty()) {
            status = m_view == TradingPostView::Collection
                         ? "Use Collect All at a Trading Post Agent, or select a delivery to "
                           "collect it individually."
                     : m_view == TradingPostView::Sell
                         ? "Select an item, enter its unit price in Geons, then choose the "
                           "quantity to list."
                         : "Select an item to inspect or trade. Offers are anonymous.";
        }
        if (m_dropdown >= 0 && m_options.size() > 8) {
            status = "Scroll the open list to see more choices.";
        }
        if (m_pending) {
            status = "Waiting for the trading post...";
        }
        const auto split = status.size() > 92 ? status.rfind(' ', 92) : std::string::npos;
        v["trading_post.status1"] = status.substr(0, split);
        v["trading_post.status2"] = split == std::string::npos ? "" : status.substr(split + 1);
        return v;
    }
    UiHit TradingPostWindow::hit(const UiDocument& doc,
                                 const engine::FrameInput& in,
                                 WidgetTools& ui,
                                 float x,
                                 float y) const {
        Host host(*this);
        return UiView::hit(doc,
                           &ui,
                           in.mouse_x,
                           in.mouse_y,
                           x,
                           y,
                           doc.window_w() * m_scale,
                           doc.window_h() * m_scale,
                           m_scale,
                           variables(),
                           &host);
    }
    std::optional<TradingPostRequest> TradingPostWindow::request(TradingPostCommand command) {
        if (m_pending) {
            return std::nullopt;
        }
        TradingPostRequest r;
        r.command = command;
        r.view = m_view;
        r.filter = m_filter;
        r.operation = m_next_operation++;
        r.npc_id = m_collection_npc;
        if (command == TradingPostCommand::CollectAll) {
            m_filter.page = 0;
            r.filter.page = 0;
        } else if (command != TradingPostCommand::Browse) {
            const auto* e = selected();
            if (!e) {
                return std::nullopt;
            }
            r.listing = e->id;
            r.iid = e->item.iid;
            r.quantity =
                command == TradingPostCommand::Cancel || command == TradingPostCommand::Collect
                    ? 1
                    : static_cast<int>(number(m_quantity));
            r.unit_price = m_view == TradingPostView::Sell ? number(m_price) : e->unit_price;
            if (command != TradingPostCommand::Cancel && command != TradingPostCommand::Collect &&
                (r.quantity <= 0 || r.unit_price <= 0)) {
                return std::nullopt;
            }
        }
        m_pending = r.operation;
        m_pending_seconds = 0;
        m_refresh = false;
        return r;
    }
    void TradingPostWindow::send_failed() {
        m_pending = 0;
        m_status = "Could not send the request. Please refresh and try again.";
    }
    std::optional<TradingPostRequest>
    TradingPostWindow::handle(const engine::FrameInput& in, WidgetTools& ui, const ItemDb& db) {
        if (!m_open) {
            return std::nullopt;
        }
        if (in.key_escape_pressed) {
            if (m_dropdown >= 0) {
                m_dropdown = -1;
            } else if (m_quantity_open) {
                m_quantity_open = false;
            } else {
                set_open(false);
            }
            return std::nullopt;
        }
        if (m_refresh && !m_pending) {
            return request(TradingPostCommand::Browse);
        }
        if (m_pending) {
            return std::nullopt;
        }
        const auto& doc = m_quantity_open ? m_quantity_document : m_document;
        const auto pointer =
            hit(doc, in, ui, m_quantity_open ? m_qx : m_x, m_quantity_open ? m_qy : m_y);
        auto action = pointer.action();
        if (m_quantity_open) {
            edit(m_quantity, m_pristine, in, m_quantity_max);
            auto n = number(m_quantity);
            if (in.mouse_left_clicked) {
                if (action == "trading_post.quantity.cancel") {
                    m_quantity_open = false;
                }
                if (action == "trading_post.quantity.decrease") {
                    m_quantity = std::to_string(std::max<std::int64_t>(1, n - 1));
                    m_pristine = false;
                }
                if (action == "trading_post.quantity.increase") {
                    m_quantity = std::to_string(std::min<std::int64_t>(m_quantity_max, n + 1));
                    m_pristine = false;
                }
                if (action == "trading_post.quantity.max") {
                    m_quantity = std::to_string(m_quantity_max);
                    m_pristine = false;
                }
                for (const auto increment : {10, 100, 1000}) {
                    if (action == "trading_post.quantity.add" + std::to_string(increment)) {
                        m_quantity = std::to_string(std::min<std::int64_t>(
                            m_quantity_max, (m_pristine ? 0 : n) + increment));
                        m_pristine = false;
                    }
                }
            }
            if (in.key_enter_pressed ||
                (in.mouse_left_clicked && action == "trading_post.quantity.confirm")) {
                if (number(m_quantity) < 1 || number(m_quantity) > m_quantity_max) {
                    m_status = "Choose a valid quantity.";
                    return std::nullopt;
                }
                m_quantity_open = false;
                return request(m_view == TradingPostView::Sell ? TradingPostCommand::Sell
                                                               : TradingPostCommand::Buy);
            }
            return std::nullopt;
        }
        if (m_dropdown >= 0) {
            const auto [x, y] = dropdown_origin();
            if (in.mouse_scroll) {
                m_option_scroll = std::clamp(m_option_scroll - static_cast<int>(in.mouse_scroll),
                                             0,
                                             std::max(0, static_cast<int>(m_options.size()) - 8));
            }
            if (in.mouse_left_clicked) {
                const int row = static_cast<int>((in.mouse_y - y) / (26 * m_scale));
                if (in.mouse_x >= x && in.mouse_x < x + 222 * m_scale && in.mouse_y >= y &&
                    row >= 0 && row < 8 &&
                    row + m_option_scroll < static_cast<int>(m_options.size())) {
                    const int value =
                        m_options[static_cast<std::size_t>(row + m_option_scroll)].first;
                    if (m_dropdown == 0) {
                        m_filter.clazz = value;
                    }
                    if (m_dropdown == 1) {
                        m_filter.type = static_cast<TradingPostType>(value);
                    }
                    if (m_dropdown == 2) {
                        m_filter.prefix = value;
                    }
                    if (m_dropdown == 3) {
                        m_filter.grade = value;
                    }
                    if (m_dropdown == 4) {
                        m_filter.order = static_cast<TradingPostOrder>(value);
                    }
                    if (m_dropdown < 2) {
                        m_filter.grade = -1;
                    }
                    if (!trading_post_prefixes(m_filter.type)) {
                        m_filter.prefix = -1;
                    }
                    m_filter.page = 0;
                    m_selected = 0;
                    m_refresh = true;
                }
                m_dropdown = -1;
            }
            return std::nullopt;
        }
        if (m_price_focus) {
            edit(m_price, m_pristine, in, kTradingPostMaxPrice);
        }
        if (in.mouse_left_clicked) {
            m_price_focus =
                m_view == TradingPostView::Sell &&
                (pointer.bind() == "trading_post.price" || action == "trading_post.price");
            if (m_price_focus) {
                m_pristine = true;
            }
            if (pointer.kind() == UiHitKind::Close || action == "window.close") {
                set_open(false);
                return std::nullopt;
            }
            if (pointer.kind() == UiHitKind::TitleDrag) {
                m_dragging = true;
                m_drag_dx = in.mouse_x - m_x;
                m_drag_dy = in.mouse_y - m_y;
            }
            for (int i = 0; i < 5; ++i) {
                if (action == std::string("trading_post.filter_") + filter_names[i] &&
                    (i != 2 || trading_post_prefixes(m_filter.type))) {
                    m_dropdown = i;
                    m_options = options(i, db);
                    m_option_scroll = 0;
                    return std::nullopt;
                }
            }
            const char* tabs[] = {"buy", "sell", "own", "collection"};
            for (int i = 0; i < 4; ++i) {
                if (action == std::string("trading_post.tab_") + tabs[i]) {
                    m_view = static_cast<TradingPostView>(i);
                    m_filter.page = 0;
                    m_selected = 0;
                    m_quantity_open = false;
                    m_state.entries.clear();
                    m_status.clear();
                    m_refresh = true;
                }
            }
            if (action == "trading_post.refresh") {
                m_refresh = true;
            }
            if (action == "trading_post.prev" && m_filter.page) {
                --m_filter.page;
                m_selected = 0;
                m_refresh = true;
            }
            if (action == "trading_post.next" && Host(*this).button_enabled(action)) {
                ++m_filter.page;
                m_selected = 0;
                m_refresh = true;
            }
            if (const auto* e = entry(pointer.bind(), pointer.index())) {
                if (m_view == TradingPostView::Sell && m_selected != e->id) {
                    m_price.clear();
                    m_price_focus = true;
                    m_pristine = true;
                }
                m_selected = e->id;
            }
            if (action == "trading_post.collect_all" && Host(*this).button_enabled(action)) {
                return request(TradingPostCommand::CollectAll);
            }
            if (action == "trading_post.collect_selected" && Host(*this).button_enabled(action)) {
                return request(TradingPostCommand::Collect);
            }
            if (action == "trading_post.act" && Host(*this).button_enabled(action)) {
                if (m_view == TradingPostView::Collection) {
                    return request(TradingPostCommand::CollectAll);
                }
                if (m_view == TradingPostView::Own) {
                    return request(TradingPostCommand::Cancel);
                }
                if (m_view != TradingPostView::Collection && selected()) {
                    m_quantity_max = selected()->item.count;
                    m_quantity = "1";
                    m_pristine = true;
                    m_quantity_open = true;
                    m_price_focus = false;
                }
            }
        }
        if (in.mouse_scroll) {
            if (in.mouse_scroll < 0 && Host(*this).button_enabled("trading_post.next")) {
                ++m_filter.page;
                m_selected = 0;
                m_refresh = true;
            } else if (in.mouse_scroll > 0 && m_filter.page) {
                --m_filter.page;
                m_selected = 0;
                m_refresh = true;
            }
        }
        if (m_dragging) {
            if (!in.mouse_left) {
                m_dragging = false;
            } else {
                m_x = in.mouse_x - m_drag_dx;
                m_y = in.mouse_y - m_drag_dy;
            }
        }
        return std::nullopt;
    }
    void TradingPostWindow::tooltip(WidgetTools& ui,
                                    engine::IEngine& engine,
                                    const FreetypeFont& font,
                                    const ItemDb& db,
                                    const engine::FrameInput& in,
                                    const TradingPostEntry& e) const {
        const auto* d = db.find(e.item.index);
        if (!d) {
            return;
        }
        const auto& item = e.item;
        if (m_view == TradingPostView::Collection && e.delivery == TradingPostDelivery::Proceeds) {
            const auto markup =
                std::string("#3Sale proceeds#n") + geon_color(e.unit_price * item.count).markup +
                money(e.unit_price * item.count) + " Geons#n#8Sold: " + std::to_string(item.count) +
                " x " + d->name() + "#nUnit sale price: " + geon_color(e.unit_price).markup +
                money(e.unit_price) + " Geons#n#2Geons awaiting NPC collection";
            ItemTooltip::draw(ui,
                              engine,
                              font,
                              ItemTooltip::parse_markup(markup),
                              in.mouse_x,
                              in.mouse_y,
                              m_width,
                              m_height);
            return;
        }
        using A = TradeAttribute;
        ItemTipExtras ex;
        ex.x_attack = item.attribute(A::Attack);
        ex.x_magic = item.attribute(A::Magic);
        ex.x_hit = item.attribute(A::Hit);
        ex.x_defense = item.attribute(A::Defense);
        ex.x_dodge = item.attribute(A::Dodge);
        ex.upgrade_level = item.attribute(A::UpgradeLevel);
        ex.mix_type = item.attribute(A::Mix);
        ex.bof = item.attribute(A::BeadOfFire);
        ex.qigong_perforated = item.attribute(A::Perforated);
        ex.qigong_effect = item.attribute(A::Qigong);
        ex.dragon_effect = item.attribute(A::Dragon);
        ex.random_stat_type = static_cast<RandomItemStatType>(item.attribute(A::RandomType));
        ex.random_stat_value = item.attribute(A::RandomValue);
        ex.egg_level = std::max(1, item.attribute(A::AnimalLevel));
        ex.egg_exp = item.attribute(A::AnimalExp);
        ex.enchant_protected = item.attribute(A::Protected) != 0;
        if (const auto pet = m_pet_info.find(item.index); pet != m_pet_info.end()) {
            ex.pet_heal_per_two_seconds = pet->second.heal_per_two_seconds;
            ex.pet_auto_pickup = pet->second.auto_pickup;
            ex.lifetime_duration_seconds = pet->second.duration_seconds;
        }
        const auto expires = std::max(item.pet_expire_at, item.ride_expire_at);
        if (expires) {
            const auto now =
                static_cast<std::uint64_t>(std::max<std::time_t>(0, std::time(nullptr)));
            ex.lifetime_expiration_known = true;
            ex.lifetime_remaining_seconds = expires > now ? expires - now : 0;
        }
        auto markup = ItemTooltip::make_tip(*d,
                                            db,
                                            item.count,
                                            -1,
                                            1,
                                            item.attribute(A::Prefix),
                                            item.attribute(A::CurrentEndurance),
                                            item.attribute(A::MaximumEndurance),
                                            ex);
        if (m_view != TradingPostView::Sell) {
            markup += std::string("#n#8Unit price: ") + geon_color(e.unit_price).markup +
                      money(e.unit_price) +
                      " Geons#n#8Stack price: " + geon_color(e.unit_price * item.count).markup +
                      money(e.unit_price * item.count) + " Geons";
        }
        if (m_view == TradingPostView::Collection) {
            markup += "#n#2" + std::string(e.delivery == TradingPostDelivery::Purchase
                                               ? "Purchased - awaiting NPC collection"
                                           : e.delivery == TradingPostDelivery::Returned
                                               ? "Withdrawn - awaiting NPC collection"
                                               : "Sale proceeds - awaiting NPC collection");
        }
        ItemTooltip::draw(ui,
                          engine,
                          font,
                          ItemTooltip::parse_markup(markup),
                          in.mouse_x,
                          in.mouse_y,
                          m_width,
                          m_height);
    }
    void TradingPostWindow::draw(engine::IEngine& engine,
                                 WidgetTools& ui,
                                 const FreetypeFont& font,
                                 const ItemDb& db,
                                 const engine::FrameInput& in) const {
        if (!m_open || m_center_pending) {
            return;
        }
        engine.draw_ui_rect(0, 0, m_width, m_height, 0, 0, 0, .55f);
        auto v = variables();
        const int selected_values[] = {m_filter.clazz,
                                       static_cast<int>(m_filter.type),
                                       m_filter.prefix,
                                       m_filter.grade,
                                       static_cast<int>(m_filter.order)};
        for (int i = 0; i < 5; ++i) {
            for (const auto& o : options(i, db)) {
                if (o.first == selected_values[i]) {
                    v[std::string("trading_post.") + filter_names[i]] = o.second + "  v";
                }
            }
        }
        if (const auto* e = selected()) {
            if (const auto* d = db.find(e->item.index)) {
                auto name = d->name();
                const bool proceeds = m_view == TradingPostView::Collection &&
                                      e->delivery == TradingPostDelivery::Proceeds;
                if (proceeds) {
                    name = "Sold: " + std::to_string(e->item.count) + " x " + name;
                }
                const auto label_scale =
                    font.pixel_size() > 0 ? 13.f / static_cast<float>(font.pixel_size()) : 1.f;
                while (!name.empty() && font.text_width(name, label_scale) > 440) {
                    auto end = name.size() - 1;
                    while (end && (static_cast<unsigned char>(name[end]) & 0xc0) == 0x80) {
                        --end;
                    }
                    name.resize(end);
                }
                if (proceeds) {
                    v["trading_post.details"] = name;
                } else {
                    v["trading_post.selected"] = name;
                    if (m_view == TradingPostView::Collection) {
                        v["trading_post.details"] =
                            std::string(e->delivery == TradingPostDelivery::Returned
                                            ? "Withdrawn items: "
                                            : "Purchased items: ") +
                            std::to_string(e->item.count) + " awaiting NPC collection";
                    }
                }
            }
        }
        Host host(*this, &db);
        const auto pointer = hit(m_document, in, ui, m_x, m_y);
        host.set_pointer_hit(pointer.widget_id(), in.mouse_left);
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_document.window_w() * m_scale,
                     m_document.window_h() * m_scale,
                     m_scale,
                     v,
                     font,
                     font,
                     &host,
                     true);
        if (m_dropdown >= 0) {
            const auto [x, y] = dropdown_origin();
            for (int row = 0; row < 8 && row + m_option_scroll < static_cast<int>(m_options.size());
                 ++row) {
                v["trading_post.option"] =
                    m_options[static_cast<std::size_t>(row + m_option_scroll)].second;
                UiView::draw(engine,
                             ui,
                             m_option_document,
                             x,
                             y + row * 26 * m_scale,
                             222 * m_scale,
                             26 * m_scale,
                             m_scale,
                             v,
                             font,
                             font,
                             nullptr,
                             false);
            }
            return;
        }
        if (m_quantity_open) {
            engine.draw_ui_rect(0, 0, m_width, m_height, 0, 0, 0, .5f);
            const auto p = hit(m_quantity_document, in, ui, m_qx, m_qy);
            host.set_pointer_hit(p.widget_id(), in.mouse_left);
            UiView::draw(engine,
                         ui,
                         m_quantity_document,
                         m_qx,
                         m_qy,
                         m_quantity_document.window_w() * m_scale,
                         m_quantity_document.window_h() * m_scale,
                         m_scale,
                         v,
                         font,
                         font,
                         &host,
                         true);
            return;
        }
        if (const auto* e = entry(pointer.bind(), pointer.index())) {
            tooltip(ui, engine, font, db, in, *e);
        }
    }
}  // namespace openkal::client
