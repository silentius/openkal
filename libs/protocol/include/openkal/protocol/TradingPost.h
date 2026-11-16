/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include "openkal/protocol/Trade.h"

#include <string_view>

namespace openkal::protocol {
    inline constexpr int kTradingPostColumns = 10;
    inline constexpr std::size_t kTradingPostPageSize = 60;
    inline constexpr std::int64_t kTradingPostMaxPrice = 2000000000;
    enum class TradingPostCommand : std::uint8_t { Browse, Sell, Buy, Cancel, Collect, CollectAll };
    enum class TradingPostView : std::uint8_t { Buy, Sell, Own, Collection };
    enum class TradingPostOrder : std::uint8_t {
        PriceDescending,
        PriceAscending,
        EnhancementDescending,
        EnhancementAscending
    };
    enum class TradingPostType : std::int32_t {
        Any = -1,
        Other,
        Quest,
        Consumables,
        Weapon,
        HeadGear,
        Chest,
        Gloves,
        Shorts,
        Boots
    };
    enum class TradingPostDelivery : std::uint8_t { Purchase, Proceeds, Returned };
    /** Server-side search and paging; class indices retain the game catalog's order. */
    struct TradingPostFilter {
        int clazz = -1;
        TradingPostType type = TradingPostType::Any;
        int prefix = -1, grade = -1;
        TradingPostOrder order = TradingPostOrder::PriceAscending;
        std::uint32_t page = 0;
    };
    /** A command carries the displayed unit price to prevent unintended repricing. */
    struct TradingPostRequest {
        TradingPostCommand command = TradingPostCommand::Browse;
        TradingPostView view = TradingPostView::Buy;
        TradingPostFilter filter;
        std::uint64_t operation = 0, listing = 0;
        std::uint32_t iid = 0;
        /** Runtime collection NPC, validated by the server for pickup. */
        std::uint32_t npc_id = 0;
        int quantity = 1;
        std::int64_t unit_price = 0;
    };
    /** Public offer/receipt. No seller identity or source inventory IID is disclosed. */
    struct TradingPostEntry {
        std::uint64_t id = 0;
        TradeItem item;
        std::int64_t unit_price = 0;
        TradingPostDelivery delivery = TradingPostDelivery::Purchase;
    };
    /** One bounded page and the acknowledged result of a command. */
    struct TradingPostState {
        TradingPostView view = TradingPostView::Buy;
        std::uint64_t acknowledged = 0;
        std::uint32_t page = 0;
        /** Total matching pages, at least one; zero when a response has no count. */
        std::uint32_t total_pages = 1;

        bool more = false;
        int purchased = 0;
        std::string status;
        std::vector<TradingPostEntry> entries;
    };

    /** Actual rewards from committed pickup, independent of collection paging. */
    struct TradingPostPickup {
        /** Zero with quantity zero for a currency-only receipt. */
        int item_index = 0;
        int quantity = 0;
        std::int64_t geons = 0;
    };
    /** @param big Main catalog category. @param small Equipment category.
     * @param usable Whether consumable use is supported. @return Search category. */
    TradingPostType trading_post_type(std::string_view big, std::string_view small, bool usable);

    /** @param type Search category. @return Whether prefix filtering applies. */
    bool trading_post_prefixes(TradingPostType type);

    /** @param request Command. @return Framed command, or empty for invalid values. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_trading_post_request(const TradingPostRequest& request);

    /** @param data Payload. @param size Byte count. @param out Decoded command.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_trading_post_request(const std::uint8_t* data, std::size_t size, TradingPostRequest& out);

    /** @param state Page. @return Framed page, or empty for invalid values. */
    [[nodiscard]] std::vector<std::uint8_t> frame_trading_post_state(const TradingPostState& state);

    /** @param data Payload. @param size Byte count. @param out Decoded page.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_trading_post_state(const std::uint8_t* data, std::size_t size, TradingPostState& out);

    /** Builds the character's collection indicator without changing an open market page.
     * @param pending Whether items or sale proceeds await collection.
     * @return Framed indicator. */
    [[nodiscard]] std::vector<std::uint8_t> frame_trading_post_collection(bool pending);

    /** Reads a collection indicator.
     * @param data Payload. @param size Byte count. @param pending Decoded indicator.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_trading_post_collection(const std::uint8_t* data, std::size_t size, bool& pending);

    /** @param pickup Committed rewards. @return Framed receipt, or empty for invalid values. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_trading_post_pickup(const TradingPostPickup& pickup);

    /** @param data Payload. @param size Byte count. @param out Decoded receipt.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_trading_post_pickup(const std::uint8_t* data, std::size_t size, TradingPostPickup& out);
}  // namespace openkal::protocol
