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

#include "openkal/protocol/FrameParser.h"

namespace openkal::protocol {

    /** Drains a stream batch without moving its suffix after each frame.
     * The buffer must not be modified independently during this object's lifetime.
     */
    class FrameDrain final {
      public:
        /** Starts a drain over existing stream bytes.
         * @param buffer Stream storage retained by the caller.
         */
        explicit FrameDrain(std::vector<std::uint8_t>& buffer) : m_buffer(buffer) {}

        /** Removes the consumed prefix, including on an early return or exception. */
        ~FrameDrain() {
            compact();
        }

        FrameDrain(const FrameDrain&) = delete;
        FrameDrain& operator=(const FrameDrain&) = delete;

        /** Decodes and logically consumes one complete frame.
         * @param type Receives the opcode on success.
         * @param payload Receives the decoded payload; its capacity can be reused.
         * @return Complete, Incomplete, or Invalid with the parser's existing semantics.
         */
        [[nodiscard]] FrameParseStatus pop(std::uint8_t& type, std::vector<std::uint8_t>& payload) {
            const auto remaining = m_buffer.size() - m_consumed;
            const auto* data = remaining ? m_buffer.data() + m_consumed : nullptr;
            std::size_t consumed = 0;
            const auto status = FrameParser::pop(data, remaining, consumed, type, payload);
            m_consumed += consumed;
            return status;
        }

        /** Discards the entire batch, including any malformed or incomplete suffix. */
        void discard();

        /** Removes the consumed prefix once and resets the cursor.
         * @return Number of suffix bytes moved; zero when no move is needed.
         */
        std::size_t compact();

      private:
        /** Caller-owned stream storage. */
        std::vector<std::uint8_t>& m_buffer;

        /** Complete frame bytes consumed since the previous compaction. */
        std::size_t m_consumed = 0;
    };

}  // namespace openkal::protocol
