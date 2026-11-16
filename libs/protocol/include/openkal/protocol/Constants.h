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

namespace openkal::protocol {

    // Shared constants for OpenKal's wire-compatible client and server.
    inline constexpr std::uint16_t kServerPort = 30001;
    inline constexpr std::uint32_t kProtocolSeed = 3793;
    inline constexpr std::uint32_t kProtocolMagic = 9052;

    inline constexpr std::size_t kPacketReservedLength = 3;  // WORD size + BYTE type
    inline constexpr std::size_t kMaxPacketLength = 8000;
    /** Hard cap for queued stream bytes before frame parsing catches up. */
    inline constexpr std::size_t kMaxReceiveBufferLength = kMaxPacketLength * 4;

    // Packet XOR tables are NOT shipped here. See openkal::protocol::IPacketCipher
    // to plug in an external library later.

#pragma pack(push, 1)
    /** Packed wire header shared by all framed packets (intentional DTO). */
    struct PacketHeader {
        /** Total bytes including this header. */
        std::uint16_t size = 0;

        /** Opcode (C2S / S2C). */
        std::uint8_t type = 0;
    };
#pragma pack(pop)

    static_assert(sizeof(PacketHeader) == kPacketReservedLength, "packet header size");

}  // namespace openkal::protocol
