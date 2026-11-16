/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/PacketWriter.h"

#include "openkal/protocol/Cipher.h"

#include <cstdarg>
#include <cstring>

namespace openkal::protocol {

    void PacketWriter::write_u8(std::uint8_t value) {
        m_payload.push_back(value);
    }

    void PacketWriter::write_u16(std::uint16_t value) {
        m_payload.push_back(static_cast<std::uint8_t>(value & 0xff));
        m_payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    }

    void PacketWriter::write_u32(std::uint32_t value) {
        write_u16(static_cast<std::uint16_t>(value & 0xffff));
        write_u16(static_cast<std::uint16_t>((value >> 16) & 0xffff));
    }

    void PacketWriter::write_i32(std::int32_t value) {
        write_u32(static_cast<std::uint32_t>(value));
    }

    void PacketWriter::write_u64(std::uint64_t value) {
        write_u32(static_cast<std::uint32_t>(value & 0xffffffffu));
        write_u32(static_cast<std::uint32_t>((value >> 32) & 0xffffffffu));
    }

    void PacketWriter::write_string(std::string_view value) {
        m_payload.insert(m_payload.end(), value.begin(), value.end());
        m_payload.push_back(0);
    }

    void PacketWriter::write_bytes(const void* data, std::size_t size) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        m_payload.insert(m_payload.end(), p, p + size);
    }

    bool PacketWriter::write_format(std::string_view fmt, ...) {
        va_list va;
        va_start(va, fmt);
        bool ok = true;
        for (char c : fmt) {
            switch (c) {
            case 'b':
            case 'B':
                write_u8(static_cast<std::uint8_t>(va_arg(va, int)));
                break;
            case 'w':
            case 'W':
                write_u16(static_cast<std::uint16_t>(va_arg(va, int)));
                break;
            case 'd':
                write_u32(va_arg(va, std::uint32_t));
                break;
            case 'I':
                write_u64(va_arg(va, std::uint64_t));
                break;
            case 's':
            case 'S': {
                const char* str = va_arg(va, const char*);
                if (!str) {
                    write_u8(0);
                } else {
                    write_string(str);
                }
                break;
            }
            case 'm':
            case 'M': {
                const void* ptr = va_arg(va, void*);
                const int size = va_arg(va, int);
                if (size < 0 || !ptr) {
                    ok = false;
                    break;
                }
                write_bytes(ptr, static_cast<std::size_t>(size));
                break;
            }
            default:
                ok = false;
                break;
            }
            if (!ok) {
                break;
            }
        }
        va_end(va);
        return ok;
    }

    std::vector<std::uint8_t> PacketWriter::frame(std::uint8_t type) const {
        const std::size_t total = kPacketReservedLength + m_payload.size();
        if (total > kMaxPacketLength) {
            return {};
        }
        std::vector<std::uint8_t> out;
        out.reserve(total);
        const auto size = static_cast<std::uint16_t>(total);
        out.push_back(static_cast<std::uint8_t>(size & 0xff));
        out.push_back(static_cast<std::uint8_t>((size >> 8) & 0xff));
        out.push_back(type);
        out.insert(out.end(), m_payload.begin(), m_payload.end());
        PacketCipherRegistry::encode(out.data(), out.size());
        return out;
    }

}  // namespace openkal::protocol
