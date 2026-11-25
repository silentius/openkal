/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Honor.h"

#include "openkal/protocol/ChatAmount.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <array>

namespace openkal::protocol {
    bool parse_honor_adjustment(std::string_view args, std::int64_t& delta) {
        ChatAmountArgs parsed;
        if (!ChatAmount::parse_args(args, parsed) || parsed.op == ChatAmountOp::Set) {
            return false;
        }
        delta = parsed.op == ChatAmountOp::Add ? parsed.value : -parsed.value;
        return true;
    }

    std::string_view honor_rank_name(HonorRank rank) {
        static constexpr std::array<std::string_view, 11> names{
            "Unranked",
            "Recruit",
            "Private",
            "Specialist",
            "Corporal",
            "Sergeant",
            "Lieutenant",
            "Captain",
            "Major",
            "Colonel",
            "General",
        };
        const auto index = static_cast<std::size_t>(rank);
        return index < names.size() ? names[index] : names.front();
    }

    std::vector<std::uint8_t> frame_honor_status(const HonorStatus& status) {
        PacketWriter writer;
        writer.write_u64(static_cast<std::uint64_t>(status.honor_points));
        writer.write_u64(static_cast<std::uint64_t>(status.weekly_honor));
        writer.write_u64(static_cast<std::uint64_t>(status.reward_points));
        writer.write_u8(static_cast<std::uint8_t>(status.rank));
        return writer.frame(static_cast<std::uint8_t>(S2C::HonorStatus));
    }

    bool parse_honor_status(const std::uint8_t* data, std::size_t size, HonorStatus& status) {
        if (!data || size != 25) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint64_t honor = 0, weekly = 0, reward = 0;
        std::uint8_t rank = 0;
        if (!reader.read_u64(honor) || !reader.read_u64(weekly) || !reader.read_u64(reward) ||
            !reader.read_u8(rank) || honor > kMaxHonorPoints || weekly > honor ||
            reward > kMaxHonorPoints || rank > static_cast<std::uint8_t>(HonorRank::General)) {
            return false;
        }
        status = {static_cast<std::int64_t>(honor),
                  static_cast<std::int64_t>(weekly),
                  static_cast<std::int64_t>(reward),
                  static_cast<HonorRank>(rank)};
        return true;
    }
}  // namespace openkal::protocol
