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

#include "openkal/protocol/Constants.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::protocol {

    /** Builds packet payloads and frames. */
    class PacketWriter {
      public:
        /** Clears the payload. */
        void clear() {
            m_payload.clear();
        }

        /** Writes an unsigned byte. @param value Value to write. */
        void write_u8(std::uint8_t value);

        /** Writes a little-endian unsigned word. @param value Value to write. */
        void write_u16(std::uint16_t value);

        /** Writes a little-endian signed double word. @param value Value to write. */
        void write_i32(std::int32_t value);

        /** Writes a little-endian unsigned double word. @param value Value to write. */
        void write_u32(std::uint32_t value);

        /** Writes a little-endian unsigned quad word. @param value Value to write. */
        void write_u64(std::uint64_t value);

        /** Writes a NUL-terminated string. @param value Value to write. */
        void write_string(std::string_view value);

        /** Writes raw bytes. @param data Source bytes. @param size Byte count. */
        void write_bytes(const void* data, std::size_t size);

        /** Writes values using legacy packet format letters.
         * @param fmt Format containing b, B, w, W, d, I, s, S, m, or M.
         * @return True on success.
         */
        [[nodiscard]] bool write_format(std::string_view fmt, ...);

        /** Gets the payload. @return Read-only payload bytes. */
        [[nodiscard]] const std::vector<std::uint8_t>& payload() const {
            return m_payload;
        }

        /** Gets the mutable payload. @return Mutable payload bytes. */
        [[nodiscard]] std::vector<std::uint8_t>& payload() {
            return m_payload;
        }

        /** Builds a size-prefixed frame.
         * @param type Packet opcode.
         * @return Framed bytes, or an empty vector when oversized.
         */
        [[nodiscard]] std::vector<std::uint8_t> frame(std::uint8_t type) const;

      private:
        /** Accumulated payload bytes. */
        std::vector<std::uint8_t> m_payload;
    };

}  // namespace openkal::protocol
