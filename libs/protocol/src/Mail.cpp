/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Mail.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <limits>

namespace openkal::protocol {
    namespace {
        constexpr std::size_t kMaximumEntries = 256;
    }

    std::vector<std::uint8_t> Mail::frame_list_c2s() {
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(MailCommand::List));
        return writer.frame(static_cast<std::uint8_t>(C2S::Mail));
    }

    std::vector<std::uint8_t> Mail::frame_claim_c2s(std::uint32_t id) {
        if (id == 0) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(MailCommand::Claim));
        writer.write_u32(id);
        return writer.frame(static_cast<std::uint8_t>(C2S::Mail));
    }

    std::vector<std::uint8_t> Mail::frame_action_c2s(MailCommand command, std::uint32_t id) {
        if (id == 0 || (command != MailCommand::Read && command != MailCommand::Delete)) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(command));
        writer.write_u32(id);
        return writer.frame(static_cast<std::uint8_t>(C2S::Mail));
    }

    bool Mail::parse_c2s(const std::uint8_t* data,
                         std::size_t size,
                         MailCommand& command,
                         std::uint32_t& id) {
        if (!data || size == 0) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t raw_command = 0;
        if (!reader.read_u8(raw_command)) {
            return false;
        }
        if (raw_command == static_cast<std::uint8_t>(MailCommand::List)) {
            command = MailCommand::List;
            id = 0;
            return reader.remaining() == 0;
        }
        if ((raw_command != static_cast<std::uint8_t>(MailCommand::Claim) &&
             raw_command != static_cast<std::uint8_t>(MailCommand::Read) &&
             raw_command != static_cast<std::uint8_t>(MailCommand::Delete)) ||
            !reader.read_u32(id) || id == 0 || reader.remaining() != 0) {
            return false;
        }
        command = static_cast<MailCommand>(raw_command);
        return true;
    }

    std::vector<std::uint8_t> Mail::frame_list_s2c(const std::vector<MailEntry>& entries) {
        if (entries.size() > kMaximumEntries ||
            entries.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(MailCommand::List));
        writer.write_u16(static_cast<std::uint16_t>(entries.size()));
        for (const MailEntry& entry : entries) {
            if (entry.id == 0 || ((entry.item_index == 0) != (entry.item_count == 0))) {
                return {};
            }
            writer.write_u32(entry.id);
            writer.write_u16(entry.item_index);
            writer.write_u32(entry.item_count);
            writer.write_u64(entry.created_at);
            writer.write_u8(
                static_cast<std::uint8_t>((entry.claimed ? 1 : 0) | (entry.read ? 2 : 0)));
            writer.write_string(entry.subject);
            writer.write_string(entry.body);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::Mail));
    }

    std::vector<std::uint8_t> Mail::frame_claim_result_s2c(const MailClaimResult& result) {
        if (result.id == 0) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(MailCommand::Claim));
        writer.write_u32(result.id);
        writer.write_u8(result.claimed ? 1 : 0);
        return writer.frame(static_cast<std::uint8_t>(S2C::Mail));
    }

    bool Mail::parse_list_s2c(const std::uint8_t* data,
                              std::size_t size,
                              std::vector<MailEntry>& entries) {
        if (!data || size < 3) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t raw_command = 0;
        std::uint16_t count = 0;
        if (!reader.read_u8(raw_command) ||
            raw_command != static_cast<std::uint8_t>(MailCommand::List) ||
            !reader.read_u16(count) || count > kMaximumEntries) {
            return false;
        }
        std::vector<MailEntry> parsed;
        parsed.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            MailEntry entry;
            std::uint8_t claimed = 0;
            if (!reader.read_u32(entry.id) || !reader.read_u16(entry.item_index) ||
                !reader.read_u32(entry.item_count) || !reader.read_u64(entry.created_at) ||
                !reader.read_u8(claimed) || claimed > 3 || !reader.read_string(entry.subject) ||
                !reader.read_string(entry.body) || entry.id == 0 ||
                ((entry.item_index == 0) != (entry.item_count == 0))) {
                return false;
            }
            entry.claimed = (claimed & 1) != 0;
            entry.read = (claimed & 2) != 0;
            parsed.push_back(std::move(entry));
        }
        if (reader.remaining() != 0) {
            return false;
        }
        entries = std::move(parsed);
        return true;
    }

    bool Mail::parse_claim_result_s2c(const std::uint8_t* data,
                                      std::size_t size,
                                      MailClaimResult& result) {
        if (!data || size != 6) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t raw_command = 0;
        std::uint8_t claimed = 0;
        if (!reader.read_u8(raw_command) ||
            raw_command != static_cast<std::uint8_t>(MailCommand::Claim) ||
            !reader.read_u32(result.id) || !reader.read_u8(claimed) || result.id == 0 ||
            claimed > 1) {
            return false;
        }
        result.claimed = claimed != 0;
        return reader.remaining() == 0;
    }

}  // namespace openkal::protocol
