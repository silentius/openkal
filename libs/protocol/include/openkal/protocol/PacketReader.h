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

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::protocol {

    /** Reads primitive values from a packet payload. */
    class PacketReader {
      public:
        /** Creates a reader over a byte range.
         * @param data Start of the byte range.
         * @param size Number of bytes in the range.
         */
        explicit PacketReader(const std::uint8_t* data, std::size_t size);

        /** Creates a reader over a byte vector.
         * @param data Bytes to read.
         */
        explicit PacketReader(const std::vector<std::uint8_t>& data);

        /** Reports whether all reads have succeeded.
         * @return True while the reader is valid.
         */
        [[nodiscard]] bool ok() const {
            return m_ok;
        }

        /** Gets the number of unread bytes.
         * @return Number of unread bytes.
         */
        [[nodiscard]] std::size_t remaining() const {
            return m_end > m_current ? static_cast<std::size_t>(m_end - m_current) : 0;
        }

        /** Reads an unsigned byte. @param out Destination. @return True on success.
         */
        [[nodiscard]] bool read_u8(std::uint8_t& out);

        /** Reads a little-endian unsigned word. @param out Destination. @return True
         * on success. */
        [[nodiscard]] bool read_u16(std::uint16_t& out);

        /** Reads a little-endian signed double word. @param out Destination.
         * @return True on success.
         */
        [[nodiscard]] bool read_i32(std::int32_t& out);

        /** Reads a little-endian unsigned double word. @param out Destination.
         * @return True on success.
         */
        [[nodiscard]] bool read_u32(std::uint32_t& out);

        /** Reads a little-endian unsigned quad word. @param out Destination. @return
         * True on success. */
        [[nodiscard]] bool read_u64(std::uint64_t& out);

        /** Reads a NUL-terminated string. @param out Destination. @return True on
         * success. */
        [[nodiscard]] bool read_string(std::string& out);

        /** Reads raw bytes. @param dst Destination. @param size Byte count. @return
         * True on success. */
        [[nodiscard]] bool read_bytes(void* dst, std::size_t size);

        /** Reads values using the legacy packet format letters.
         * @param fmt Format containing b, B, w, W, d, I, s, S, m, or M.
         * @return True on success.
         */
        [[nodiscard]] bool read_format(std::string_view fmt, ...);

      private:
        /** Current read position. */
        const std::uint8_t* m_current = nullptr;

        /** End of the readable range. */
        const std::uint8_t* m_end = nullptr;

        /** Accumulated read status. */
        bool m_ok = true;
    };

}  // namespace openkal::protocol
