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
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::protocol {
    inline constexpr std::size_t kTradeItems = 12;
    inline constexpr std::size_t kTradeInventorySlots = 216;
    enum class TradeCommand : std::uint8_t { Invite, Accept, Offer, Confirm, Cancel, Inventory };
    enum class TradePhase : std::uint8_t { Closed, Invitation, Open, Completed };
    /** Requested quantity and destination cell within one participant's offer. */
    struct TradeAmount {
        std::uint32_t iid = 0;
        std::int32_t count = 0;
        std::uint8_t slot = 0;
        /** @param other Other amount. @return Whether instance, quantity and cell match. */
        bool operator==(const TradeAmount& other) const {
            return iid == other.iid && count == other.count && slot == other.slot;
        }
    };
    /** OpenKal-owned command. Tokens and revisions reject stale confirmations. */
    struct TradeRequest {
        TradeCommand command = TradeCommand::Cancel;
        std::uint32_t target = 0, token = 0, revision = 0, operation = 0;
        std::int32_t money = 0;
        std::vector<TradeAmount> items;
    };
    /** Compact, lossless instance attributes for item tooltips and offer validation. */
    enum class TradeAttribute : std::size_t {
        Prefix,
        CurrentEndurance,
        MaximumEndurance,
        Attack,
        Magic,
        Hit,
        Defense,
        Dodge,
        UpgradeLevel,
        UpgradeRate,
        Mix,
        BeadOfFire,
        Perforated,
        Qigong,
        Dragon,
        RandomType,
        RandomValue,
        AnimalLevel,
        AnimalExp,
        Protected,
        Count
    };
    inline constexpr std::uint8_t kTradeable = 1, kTradeAccountBound = 2, kTradeCharacterBound = 4,
                                  kTradeEquipped = 8;
    /** Authoritative item or offered portion. Inventory cells retain their positions. */
    struct TradeItem {
        std::uint32_t iid = 0;
        int index = 0, count = 0;
        std::uint16_t slot = 0;
        std::uint8_t flags = kTradeable;
        std::array<std::int32_t, static_cast<std::size_t>(TradeAttribute::Count)> attributes{};
        std::uint64_t pet_expire_at = 0, ride_expire_at = 0;
        /** @param field Attribute identifier. @return Stored attribute. */
        int attribute(TradeAttribute field) const {
            return attributes[static_cast<std::size_t>(field)];
        }
    };
    /** Recipient-relative snapshot, also used for the modal invitation. */
    struct TradeState {
        TradePhase phase = TradePhase::Closed;
        std::uint32_t token = 0, revision = 0, acknowledged = 0;
        std::uint16_t inventory_slots = 0;
        std::string partner, status;
        bool inviter = false, confirmed = false, partner_confirmed = false;
        /** In Completed snapshots, partner_money and other are the committed receipt. */
        std::int32_t money = 0, partner_money = 0;
        std::vector<TradeItem> own, other, available;
        /** @return Whether all non-trade client traffic must be blocked. */
        bool locked() const {
            return phase == TradePhase::Invitation || phase == TradePhase::Open;
        }
    };
    /** @param request Command. @return Framed AskTrade packet, empty for invalid input. */
    [[nodiscard]] std::vector<std::uint8_t> frame_trade_request(const TradeRequest& request);

    /** @param data Payload. @param size Length. @param out Parsed command.
     * @return Whether the complete command is valid. */
    [[nodiscard]] bool
    parse_trade_request(const std::uint8_t* data, std::size_t size, TradeRequest& out);

    /** @param state Snapshot. @return Framed OpenTrade packet. */
    [[nodiscard]] std::vector<std::uint8_t> frame_trade_state(const TradeState& state);

    /** @param data Payload. @param size Length. @param out Parsed snapshot.
     * @return Whether the complete snapshot is valid. */
    [[nodiscard]] bool
    parse_trade_state(const std::uint8_t* data, std::size_t size, TradeState& out);

    /** @param type Client opcode. @return Whether it belongs to the trade command channel. */
    [[nodiscard]] bool is_trade_packet(std::uint8_t type);
}  // namespace openkal::protocol
