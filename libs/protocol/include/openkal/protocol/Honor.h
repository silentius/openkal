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

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace openkal::protocol {
    /** Largest supported Honor or Reward Points balance. */
    inline constexpr std::int64_t kMaxHonorPoints = 999'999'999'999'999;

    /** Stable rank IDs from unranked through General. */
    enum class HonorRank : std::uint8_t {
        Unranked,
        Recruit,
        Private,
        Specialist,
        Corporal,
        Sergeant,
        Lieutenant,
        Captain,
        Major,
        Colonel,
        General,
    };

    /** Authoritative personal Honor state. */
    struct HonorStatus {
        std::int64_t honor_points = 0;
        std::int64_t weekly_honor = 0;
        std::int64_t reward_points = 0;
        HonorRank rank = HonorRank::Unranked;
    };

    /** Help text for the single-player command. */
    inline constexpr const char* kHonorUsage = "Usage: /honor +=N or /honor -=N";

    /** Parses a signed adjustment using the usual chat amount syntax.
     * @param args Text after the /honor command.
     * @param delta Signed output, changed only on success.
     * @return True for a positive addition or subtraction amount.
     */
    [[nodiscard]] bool parse_honor_adjustment(std::string_view args, std::int64_t& delta);

    /** Returns the English label for a rank.
     * @param rank Rank identifier.
     * @return Display label.
     */
    [[nodiscard]] std::string_view honor_rank_name(HonorRank rank);

    /** Frames the personal Honor snapshot.
     * @param status Authoritative state.
     * @return Server packet.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_honor_status(const HonorStatus& status);

    /** Parses the personal Honor snapshot.
     * @param data Packet payload.
     * @param size Payload length.
     * @param status Output snapshot, changed only on success.
     * @return Whether the payload is valid.
     */
    [[nodiscard]] bool
    parse_honor_status(const std::uint8_t* data, std::size_t size, HonorStatus& status);
}  // namespace openkal::protocol
