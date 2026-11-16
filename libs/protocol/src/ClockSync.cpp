/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ClockSync.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <unordered_set>
#include <utility>

namespace openkal::protocol {
    namespace {
        bool valid(const ClockSyncFields& fields) {
            // Keep arithmetic and calendar conversion within years 1970--9999.
            constexpr std::uint64_t max_time = 253402300799ULL;
            if (fields.unix_seconds == 0 || fields.unix_seconds > max_time ||
                fields.online_seconds > max_time || fields.utc_offset_seconds < -86400 ||
                fields.utc_offset_seconds > 86400 || fields.events.size() > ClockSync::kMaxEvents) {
                return false;
            }
            std::unordered_set<std::string> ids;
            for (const auto& event : fields.events) {
                if (event.id.empty() || event.id.size() > 32 || event.name.empty() ||
                    event.name.size() > 48 || event.id.find('\0') != std::string::npos ||
                    event.name.find('\0') != std::string::npos || !ids.insert(event.id).second ||
                    event.starts_at < fields.unix_seconds ||
                    event.following_at <= event.starts_at || event.following_at > max_time) {
                    return false;
                }
            }
            return true;
        }
    }  // namespace

    std::vector<std::uint8_t> ClockSync::frame(const ClockSyncFields& fields) {
        if (!valid(fields)) {
            return {};
        }
        PacketWriter writer;
        writer.write_u64(fields.unix_seconds);
        writer.write_i32(fields.utc_offset_seconds);
        writer.write_u64(fields.online_seconds);
        writer.write_u8(static_cast<std::uint8_t>(fields.events.size()));
        for (const auto& event : fields.events) {
            writer.write_string(event.id);
            writer.write_string(event.name);
            writer.write_u64(event.starts_at);
            writer.write_u64(event.following_at);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::ClockSync));
    }

    bool ClockSync::parse(const std::uint8_t* data, std::size_t size, ClockSyncFields& fields) {
        if (!data || size < 21 || size > 2048) {
            return false;
        }
        PacketReader reader(data, size);
        ClockSyncFields decoded;
        std::uint8_t count = 0;
        if (!reader.read_u64(decoded.unix_seconds) ||
            !reader.read_i32(decoded.utc_offset_seconds) ||
            !reader.read_u64(decoded.online_seconds) || !reader.read_u8(count) ||
            count > kMaxEvents) {
            return false;
        }
        for (std::uint8_t i = 0; i < count; ++i) {
            ClockEvent event;
            if (!reader.read_string(event.id) || !reader.read_string(event.name) ||
                !reader.read_u64(event.starts_at) || !reader.read_u64(event.following_at)) {
                return false;
            }
            decoded.events.push_back(std::move(event));
        }
        if (reader.remaining() || !valid(decoded)) {
            return false;
        }
        fields = std::move(decoded);
        return true;
    }
}  // namespace openkal::protocol
