/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/PacketReader.h"

#include "openkal/protocol/Cipher.h"
#include "openkal/protocol/Constants.h"
#include "openkal/protocol/FrameParser.h"

#include <cstdarg>
#include <cstring>

namespace openkal::protocol {

    PacketReader::PacketReader(const std::uint8_t* data, std::size_t size)
        : m_current(data), m_end(data ? data + size : nullptr), m_ok(data != nullptr || size == 0) {
    }

    PacketReader::PacketReader(const std::vector<std::uint8_t>& data)
        : PacketReader(data.data(), data.size()) {}

    bool PacketReader::read_u8(std::uint8_t& out) {
        if (!m_ok || remaining() < 1) {
            m_ok = false;
            return false;
        }
        out = *m_current++;
        return true;
    }

    bool PacketReader::read_u16(std::uint16_t& out) {
        if (!m_ok || remaining() < 2) {
            m_ok = false;
            return false;
        }
        out = static_cast<std::uint16_t>(m_current[0] |
                                         (static_cast<std::uint16_t>(m_current[1]) << 8));
        m_current += 2;
        return true;
    }

    bool PacketReader::read_u32(std::uint32_t& out) {
        std::uint16_t lo = 0;
        std::uint16_t hi = 0;
        if (!read_u16(lo) || !read_u16(hi)) {
            return false;
        }
        out = static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 16);
        return true;
    }

    bool PacketReader::read_i32(std::int32_t& out) {
        std::uint32_t bits = 0;
        if (!read_u32(bits)) {
            return false;
        }
        out = static_cast<std::int32_t>(bits);
        return true;
    }

    bool PacketReader::read_u64(std::uint64_t& out) {
        std::uint32_t lo = 0;
        std::uint32_t hi = 0;
        if (!read_u32(lo) || !read_u32(hi)) {
            return false;
        }
        out = static_cast<std::uint64_t>(lo) | (static_cast<std::uint64_t>(hi) << 32);
        return true;
    }

    bool PacketReader::read_string(std::string& out) {
        out.clear();
        if (!m_ok) {
            return false;
        }
        while (m_current < m_end) {
            const char c = static_cast<char>(*m_current++);
            if (c == '\0') {
                return true;
            }
            out.push_back(c);
        }
        m_ok = false;
        return false;
    }

    bool PacketReader::read_bytes(void* dst, std::size_t size) {
        if (!m_ok || remaining() < size) {
            m_ok = false;
            return false;
        }
        std::memcpy(dst, m_current, size);
        m_current += size;
        return true;
    }

    bool PacketReader::read_format(std::string_view fmt, ...) {
        va_list va;
        va_start(va, fmt);
        bool ok = true;
        for (char c : fmt) {
            switch (c) {
            case 'b': {
                std::uint8_t* p = va_arg(va, std::uint8_t*);
                ok = p && read_u8(*p);
                break;
            }
            case 'B': {
                int* p = va_arg(va, int*);
                std::uint8_t v = 0;
                ok = p && read_u8(v);
                if (ok) {
                    *p = v;
                }
                break;
            }
            case 'w': {
                std::uint16_t* p = va_arg(va, std::uint16_t*);
                ok = p && read_u16(*p);
                break;
            }
            case 'W': {
                int* p = va_arg(va, int*);
                std::uint16_t v = 0;
                ok = p && read_u16(v);
                if (ok) {
                    *p = v;
                }
                break;
            }
            case 'd': {
                std::uint32_t* p = va_arg(va, std::uint32_t*);
                ok = p && read_u32(*p);
                break;
            }
            case 'I': {
                std::uint64_t* p = va_arg(va, std::uint64_t*);
                ok = p && read_u64(*p);
                break;
            }
            case 's': {
                std::string* p = va_arg(va, std::string*);
                ok = p && read_string(*p);
                break;
            }
            case 'S': {
                char* p = va_arg(va, char*);
                std::string tmp;
                ok = p && read_string(tmp);
                if (ok) {
                    std::memcpy(p, tmp.c_str(), tmp.size() + 1);
                }
                break;
            }
            case 'm': {
                void* p = va_arg(va, void*);
                const int size = va_arg(va, int);
                ok = p && size >= 0 && read_bytes(p, static_cast<std::size_t>(size));
                break;
            }
            case 'M': {
                const std::uint8_t** p = va_arg(va, const std::uint8_t**);
                const int size = va_arg(va, int);
                if (!p || size < 0 || remaining() < static_cast<std::size_t>(size)) {
                    ok = false;
                    break;
                }
                *p = m_current;
                m_current += size;
                break;
            }
            default:
                ok = false;
                break;
            }
            if (!ok) {
                m_ok = false;
                break;
            }
        }
        va_end(va);
        return ok && m_ok;
    }

    bool FrameParser::try_pop(const std::uint8_t* data,
                              std::size_t size,
                              std::size_t& consumed,
                              std::uint8_t& type,
                              std::vector<std::uint8_t>& payload) {
        return pop(data, size, consumed, type, payload) == FrameParseStatus::Complete;
    }

    FrameParseStatus FrameParser::pop(const std::uint8_t* data,
                                      std::size_t size,
                                      std::size_t& consumed,
                                      std::uint8_t& type,
                                      std::vector<std::uint8_t>& payload) {
        consumed = 0;
        payload.clear();
        if (size == 0) {
            return FrameParseStatus::Incomplete;
        }
        if (!data) {
            return FrameParseStatus::Invalid;
        }
        if (size < sizeof(std::uint16_t)) {
            return FrameParseStatus::Incomplete;
        }
        const std::uint16_t frame_size =
            static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
        if (frame_size < kPacketReservedLength || frame_size > kMaxPacketLength) {
            return FrameParseStatus::Invalid;
        }
        if (size < frame_size) {
            return FrameParseStatus::Incomplete;
        }
        std::vector<std::uint8_t> frame(data, data + frame_size);
        PacketCipherRegistry::decode(frame.data(), frame.size());
        type = frame[2];
        payload.assign(frame.begin() + static_cast<std::ptrdiff_t>(kPacketReservedLength),
                       frame.end());
        consumed = frame_size;
        return FrameParseStatus::Complete;
    }

    bool try_pop_frame(const std::uint8_t* data,
                       std::size_t size,
                       std::size_t& consumed,
                       std::uint8_t& type,
                       std::vector<std::uint8_t>& payload) {
        return FrameParser::try_pop(data, size, consumed, type, payload);
    }

}  // namespace openkal::protocol
