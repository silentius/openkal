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
#include "openkal/platform/Json.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::protocol {
    /** Maximum number of Unicode scalars in a guild name or rank title. */
    inline constexpr std::size_t kGuildLabelMaximum = 14;

    /** Project-owned operations under the reserved Guild opcode. */
    enum class GuildCommand : std::uint8_t {
        Snapshot,
        Ranking,
        Search,
        Detail,
        Members,
        Applications,
        MyApplications,
        Create,
        Apply,
        Withdraw,
        Accept,
        Reject,
        Leave,
        Expel,
        Rank,
        Permissions,
        Title,
        Invitation,
        Announcement,
        Transfer,
        FormationReply,
        DailyReward
    };
    /** Public views and observer-only identity updates are separate message kinds. */
    enum class GuildMessage : std::uint8_t { View, Display };
    /** Authenticated actor identity is deliberately absent from requests. */
    struct GuildRequest {
        GuildCommand command = GuildCommand::Snapshot;
        std::uint64_t request_id = 0;
        std::uint32_t guild = 0, target = 0, page = 0, value = 0;
        std::uint64_t revision = 0, expected_fee = 0;
        std::string text;
    };
    /** Versioned response with a bounded, independently defined JSON view document. */
    struct GuildResponse {
        GuildMessage kind = GuildMessage::View;
        std::uint64_t request_id = 0;
        platform::Json data;
    };
    /** Validates Unicode scalars and rejects embedded controls.
     * @param text UTF-8 input. @param maximum Maximum code points.
     * @param multiline Whether LF line breaks are allowed. @return Valid text. */
    [[nodiscard]] bool
    guild_text_valid(std::string_view text, std::size_t maximum, bool multiline = false);
    /** Validates nonempty guild names and rank titles without whitespace.
     * @param text UTF-8 input.
     * @return Whether the label contains at most 14 scalars and no whitespace.
     */
    [[nodiscard]] bool guild_label_valid(std::string_view text);
    /** @param request Operation. @return Framed bytes, or empty for invalid input. */
    [[nodiscard]] std::vector<std::uint8_t> frame_guild_request(const GuildRequest& request);
    /** @param data Payload. @param size Length. @param out Destination. @return Valid complete
     * input. */
    [[nodiscard]] bool
    parse_guild_request(const std::uint8_t* data, std::size_t size, GuildRequest& out);
    /** @param response View. @return Framed bytes, or empty when oversized/invalid. */
    [[nodiscard]] std::vector<std::uint8_t> frame_guild_response(const GuildResponse& response);
    /** @param data Payload. @param size Length. @param out Destination. @return Valid complete
     * input. */
    [[nodiscard]] bool
    parse_guild_response(const std::uint8_t* data, std::size_t size, GuildResponse& out);
}  // namespace openkal::protocol
