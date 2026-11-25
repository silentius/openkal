/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/server/ShopCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace openkal::server {
    namespace {

        [[nodiscard]] bool is_json_path(const std::string& path) {
            return path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0;
        }

        bool load_honor_offers(const platform::Json& offers,
                               const std::unordered_map<int, ItemCombatDef>& item_defs,
                               ShopDefinition& shop) {
            if (!offers.is_array() || offers.array().empty() || offers.array().size() > 1024) {
                return false;
            }
            const auto read_integer = [](const platform::Json& node, double min, double max) {
                const auto number = node.number();
                return node.is_number() && number >= min && number <= max &&
                       number == std::floor(number);
            };
            for (const auto& offer : offers.array()) {
                const auto& index = offer.at("itemIndex");
                const auto& price = offer.at("rewardPoints");
                const auto& rank = offer.at("minRank");
                if (!read_integer(index, 1, std::numeric_limits<std::uint16_t>::max()) ||
                    !read_integer(price, 1, std::numeric_limits<std::uint32_t>::max()) ||
                    !read_integer(rank, 0, static_cast<int>(protocol::HonorRank::General))) {
                    return false;
                }
                const auto item = item_defs.find(index.as_int());
                if (item == item_defs.end() || item->first == kMoneyItemIndex ||
                    item->second.is_task || item->second.honor_reward_points > 0 ||
                    !shop.honor_prices
                         .emplace(item->first,
                                  HonorShopPrice{static_cast<std::uint32_t>(price.number()),
                                                 static_cast<protocol::HonorRank>(rank.as_int())})
                         .second) {
                    return false;
                }
                shop.items.push_back(item->first);
            }
            return true;
        }

    }  // namespace

    bool ShopCatalog::load_dir(const std::string& dir,
                               const std::unordered_map<int, ItemCombatDef>& item_defs) {
        m_shops.clear();
        if (dir.empty() || !platform::is_directory(dir)) {
            log::warn("ShopCatalog: missing directory " + dir);
            return false;
        }

        auto files = platform::list_files_recursive(dir);
        std::sort(files.begin(), files.end());
        bool valid = true;
        for (const std::string& rel : files) {
            if (!is_json_path(rel)) {
                continue;
            }
            const std::string path = platform::File::join(dir, rel);
            const auto bytes = platform::File::read(path);
            platform::Json root;
            std::string error;
            if (!bytes ||
                !platform::Json::parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
                !root.is_object()) {
                log::warn("ShopCatalog: invalid JSON " + path +
                          (error.empty() ? "" : ": " + error));
                valid = false;
                continue;
            }
            if (root.at("schema").as_string() != "openkal.shop/v1") {
                log::warn("ShopCatalog: unsupported schema in " + path);
                valid = false;
                continue;
            }

            ShopDefinition shop;
            shop.npc_index = root.at("npcIndex").as_int(0);
            shop.legacy_store_id = root.at("legacyStoreId").as_int(0);
            shop.currency_item = root.at("currencyItem").as_int();
            shop.skin_catalog = root.at("skinCatalog").boolean();
            if (const auto* offers = root.get("honorOffers")) {
                if (shop.npc_index <= 0 || m_shops.count(shop.npc_index) || shop.skin_catalog ||
                    shop.currency_item != 0 || root.get("items") ||
                    !load_honor_offers(*offers, item_defs, shop)) {
                    log::warn("ShopCatalog: invalid Honor offers in " + path);
                    valid = false;
                    continue;
                }
                m_shops.emplace(shop.npc_index, std::move(shop));
                continue;
            }
            if (shop.skin_catalog) {
                const int full_price = root.at("fullBodyPrice").as_int();
                const int part_price = root.at("itemPrice").as_int();
                const auto currency = item_defs.find(shop.currency_item);
                if (shop.npc_index <= 0 || m_shops.count(shop.npc_index) ||
                    shop.currency_item <= 0 || shop.currency_item > 65535 ||
                    currency == item_defs.end() || !currency->second.stackable || full_price <= 0 ||
                    part_price <= 0) {
                    valid = false;
                    continue;
                }
                std::unordered_set<int> excluded;
                const auto& exclusions = root.at("excludeItems");
                bool exclusions_valid = exclusions.is_null() || exclusions.is_array();
                for (const auto& node : exclusions.array()) {
                    const double value = node.number();
                    if (!node.is_number() || !(value >= 1 && value <= 65535)) {
                        exclusions_valid = false;
                        continue;
                    }
                    const int index = static_cast<int>(value);
                    if (value != index || !excluded.insert(index).second) {
                        exclusions_valid = false;
                    }
                }
                if (!exclusions_valid) {
                    log::warn("ShopCatalog: invalid skin exclusions in " + path);
                    valid = false;
                    continue;
                }
                for (const auto& [index, item] : item_defs) {
                    if (index <= 0 || index > 65535 || item.skin_slot == protocol::SkinSlot::None ||
                        item.class_limit < 0 || item.class_limit > 3 || excluded.count(index)) {
                        continue;
                    }
                    shop.items.push_back(index);
                    shop.prices[index] = item.full_body_skin ? full_price : part_price;
                }
                std::sort(shop.items.begin(), shop.items.end());
                if (shop.items.empty()) {
                    valid = false;
                    continue;
                }
                m_shops.emplace(shop.npc_index, std::move(shop));
                continue;
            }
            if (shop.currency_item != 0) {
                valid = false;
                continue;
            }
            const platform::Json& items = root.at("items");
            if (shop.npc_index <= 0 || !items.is_array() || items.array().empty() ||
                m_shops.find(shop.npc_index) != m_shops.end()) {
                log::warn("ShopCatalog: invalid or duplicate NPC in " + path);
                valid = false;
                continue;
            }

            std::unordered_set<int> seen;
            bool shop_valid = true;
            for (const platform::Json& node : items.array()) {
                const int index = node.as_int(0);
                const auto def_it = item_defs.find(index);
                if (index <= 0 || !seen.insert(index).second || def_it == item_defs.end() ||
                    def_it->second.buy <= 0) {
                    log::warn("ShopCatalog: invalid offered item " + std::to_string(index) +
                              " in " + path);
                    shop_valid = false;
                    continue;
                }
                shop.items.push_back(index);
            }
            if (!shop_valid || shop.items.empty()) {
                valid = false;
                continue;
            }
            m_shops.emplace(shop.npc_index, std::move(shop));
        }
        log::info("ShopCatalog: loaded " + std::to_string(m_shops.size()) + " NPC shops");
        return valid && !m_shops.empty();
    }

    const ShopDefinition* ShopCatalog::find(int npc_index) const {
        const auto it = m_shops.find(npc_index);
        return it == m_shops.end() ? nullptr : &it->second;
    }

}  // namespace openkal::server
