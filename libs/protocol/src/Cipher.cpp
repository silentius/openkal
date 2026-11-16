/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Cipher.h"

#include "openkal/protocol/Constants.h"

#include <memory>
#include <mutex>

namespace openkal::protocol {
    namespace {

        std::mutex g_mutex;
        std::shared_ptr<IPacketCipher> g_cipher = std::make_shared<NullPacketCipher>();

    }  // namespace

    void NullPacketCipher::encode(std::uint8_t*, std::size_t) {}

    void NullPacketCipher::decode(std::uint8_t*, std::size_t) {}

    void PacketCipherRegistry::set(std::shared_ptr<IPacketCipher> cipher) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_cipher = cipher ? std::move(cipher) : std::make_shared<NullPacketCipher>();
    }

    std::shared_ptr<IPacketCipher> PacketCipherRegistry::get() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_cipher;
    }

    void PacketCipherRegistry::encode(std::uint8_t* frame, std::size_t frame_size) {
        if (!frame || frame_size < kPacketReservedLength) {
            return;
        }
        get()->encode(frame + sizeof(std::uint16_t), frame_size - sizeof(std::uint16_t));
    }

    void PacketCipherRegistry::decode(std::uint8_t* frame, std::size_t frame_size) {
        if (!frame || frame_size < kPacketReservedLength) {
            return;
        }
        get()->decode(frame + sizeof(std::uint16_t), frame_size - sizeof(std::uint16_t));
    }

    void set_packet_cipher(std::shared_ptr<IPacketCipher> cipher) {
        PacketCipherRegistry::set(std::move(cipher));
    }

    IPacketCipher& packet_cipher() {
        return *PacketCipherRegistry::get();
    }

    void encode_frame_inplace(std::uint8_t* frame, std::size_t frame_size) {
        PacketCipherRegistry::encode(frame, frame_size);
    }

    void decode_frame_inplace(std::uint8_t* frame, std::size_t frame_size) {
        PacketCipherRegistry::decode(frame, frame_size);
    }

}  // namespace openkal::protocol
