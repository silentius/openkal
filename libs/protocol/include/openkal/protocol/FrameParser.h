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
#include <vector>

namespace openkal::protocol {

    /** Result of parsing one frame from a stream buffer. */
    enum class FrameParseStatus {
        /** More bytes are required. */
        Incomplete,
        /** One complete frame was decoded. */
        Complete,
        /** The stream starts with an invalid frame length. */
        Invalid,
    };

    /** Parses complete frames from stream buffers. */
    class FrameParser final {
      public:
        FrameParser() = delete;

        /** Tries to extract one length-prefixed frame.
         * @param data Stream bytes.
         * @param size Number of available bytes.
         * @param consumed Receives the frame length on success.
         * @param type Receives the opcode.
         * @param payload Receives bytes following the header.
         * @return True when a complete valid frame was extracted.
         */
        [[nodiscard]] static bool try_pop(const std::uint8_t* data,
                                          std::size_t size,
                                          std::size_t& consumed,
                                          std::uint8_t& type,
                                          std::vector<std::uint8_t>& payload);

        /** Parses one frame and distinguishes malformed from incomplete input.
         * @param data Stream bytes.
         * @param size Number of available bytes.
         * @param consumed Receives the frame length on success.
         * @param type Receives the decoded opcode.
         * @param payload Receives decoded bytes following the header.
         * @return Complete, Incomplete, or Invalid.
         */
        [[nodiscard]] static FrameParseStatus pop(const std::uint8_t* data,
                                                  std::size_t size,
                                                  std::size_t& consumed,
                                                  std::uint8_t& type,
                                                  std::vector<std::uint8_t>& payload);
    };

    /** Compatibility wrapper for frame parsing.
     * @param data Stream bytes.
     * @param size Number of available bytes.
     * @param consumed Receives the frame length on success.
     * @param type Receives the opcode.
     * @param payload Receives bytes following the header.
     * @return True when a complete valid frame was extracted.
     */
    [[nodiscard]] bool try_pop_frame(const std::uint8_t* data,
                                     std::size_t size,
                                     std::size_t& consumed,
                                     std::uint8_t& type,
                                     std::vector<std::uint8_t>& payload);

}  // namespace openkal::protocol
