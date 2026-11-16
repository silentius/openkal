/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Quest.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Quest::frame_ask_npc(std::uint32_t npc_id) {
        PacketWriter w;
        w.write_u32(npc_id);
        return w.frame(static_cast<std::uint8_t>(C2S::AskNpc));
    }

    bool Quest::parse_ask_npc(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u32(npc_id);
    }

    std::vector<std::uint8_t> Quest::frame_quest(std::uint32_t quest_key) {
        PacketWriter w;
        w.write_u32(quest_key);
        return w.frame(static_cast<std::uint8_t>(C2S::Quest));
    }

    bool Quest::parse_quest(const std::uint8_t* data, std::size_t size, std::uint32_t& quest_key) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u32(quest_key);
    }

    std::vector<std::uint8_t> Quest::frame_send_html(std::int32_t html_id, std::uint32_t npc_id) {
        PacketWriter w;
        w.write_u32(static_cast<std::uint32_t>(html_id));
        if (npc_id != 0) {
            w.write_u32(npc_id);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::SendHtml));
    }

    bool Quest::parse_send_html(const std::uint8_t* data,
                                std::size_t size,
                                std::int32_t& html_id,
                                std::uint32_t* npc_id) {
        if (!data || size < 4 || (size > 4 && size < 8)) {
            return false;
        }
        PacketReader r(data, size);
        std::uint32_t raw = 0;
        if (!r.read_u32(raw)) {
            return false;
        }
        html_id = static_cast<std::int32_t>(raw);
        if (npc_id) {
            *npc_id = 0;
            if (size >= 8 && !r.read_u32(*npc_id)) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::uint8_t> Quest::frame_save_quest(const QuestFlagEntry& entry) {
        PacketWriter w;
        w.write_u16(entry.quest);
        w.write_u8(entry.flag);
        w.write_u8(entry.clear);
        return w.frame(static_cast<std::uint8_t>(S2C::SaveQuest));
    }

    bool
    Quest::parse_save_quest(const std::uint8_t* data, std::size_t size, QuestFlagEntry& entry) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u16(entry.quest) && r.read_u8(entry.flag) && r.read_u8(entry.clear);
    }

    std::vector<std::uint8_t> Quest::frame_quest_info(const std::vector<QuestFlagEntry>& entries) {
        PacketWriter w;
        const auto count = static_cast<std::uint8_t>(entries.size() > 255 ? 255 : entries.size());
        w.write_u8(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            w.write_u16(entries[i].quest);
            w.write_u8(entries[i].flag);
            w.write_u8(entries[i].clear);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::QuestInfo));
    }

    bool Quest::parse_quest_info(const std::uint8_t* data,
                                 std::size_t size,
                                 std::vector<QuestFlagEntry>& entries) {
        entries.clear();
        if (!data || size < 1) {
            return false;
        }
        PacketReader r(data, size);
        std::uint8_t count = 0;
        if (!r.read_u8(count)) {
            return false;
        }
        entries.reserve(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            QuestFlagEntry entry;
            if (!r.read_u16(entry.quest) || !r.read_u8(entry.flag) || !r.read_u8(entry.clear)) {
                return false;
            }
            entries.push_back(entry);
        }
        return true;
    }

    std::vector<std::uint8_t> Quest::frame_create_npc(const CreateNpcInfo& info) {
        PacketWriter w;
        w.write_u32(info.id);
        w.write_u16(info.index);
        w.write_u8(info.shape);
        w.write_u32(static_cast<std::uint32_t>(info.x));
        w.write_u32(static_cast<std::uint32_t>(info.y));
        w.write_u32(static_cast<std::uint32_t>(info.z));
        w.write_u16(info.dir);
        w.write_u32(info.gstate);
        w.write_u32(info.standard);
        return w.frame(static_cast<std::uint8_t>(S2C::CreateNpc));
    }

    bool Quest::parse_create_npc(const std::uint8_t* data, std::size_t size, CreateNpcInfo& info) {
        // d w b d d d w d d = 4+2+1+4+4+4+2+4+4 = 29
        if (!data || size < 29) {
            return false;
        }
        PacketReader r(data, size);
        std::uint32_t ux = 0;
        std::uint32_t uy = 0;
        std::uint32_t uz = 0;
        if (!r.read_u32(info.id) || !r.read_u16(info.index) || !r.read_u8(info.shape) ||
            !r.read_u32(ux) || !r.read_u32(uy) || !r.read_u32(uz) || !r.read_u16(info.dir) ||
            !r.read_u32(info.gstate) || !r.read_u32(info.standard)) {
            return false;
        }
        info.x = static_cast<std::int32_t>(ux);
        info.y = static_cast<std::int32_t>(uy);
        info.z = static_cast<std::int32_t>(uz);
        return true;
    }

    std::vector<std::uint8_t> Quest::frame_remove_npc(std::uint32_t npc_id) {
        PacketWriter w;
        w.write_u32(npc_id);
        return w.frame(static_cast<std::uint8_t>(S2C::RemoveNpc));
    }

    bool
    Quest::parse_remove_npc(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u32(npc_id);
    }

}  // namespace openkal::protocol
