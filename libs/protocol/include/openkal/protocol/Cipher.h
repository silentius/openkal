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
#include <memory>

namespace openkal::protocol {

    /** Defines the packet body cipher contract. */
    class IPacketCipher {
      public:
        /** Destroys the cipher. */
        virtual ~IPacketCipher() = default;

        /** Encodes bytes in place. @param data Bytes to encode. @param size Byte
         * count. */
        virtual void encode(std::uint8_t* data, std::size_t size) = 0;

        /** Decodes bytes in place. @param data Bytes to decode. @param size Byte
         * count. */
        virtual void decode(std::uint8_t* data, std::size_t size) = 0;
    };

    /** Implements the plaintext identity transform. */
    class NullPacketCipher final : public IPacketCipher {
      public:
        /** Leaves bytes unchanged. @param data Bytes. @param size Byte count. */
        void encode(std::uint8_t* data, std::size_t size) override;

        /** Leaves bytes unchanged. @param data Bytes. @param size Byte count. */
        void decode(std::uint8_t* data, std::size_t size) override;
    };

    /** Owns the process-wide packet cipher. */
    class PacketCipherRegistry final {
      public:
        PacketCipherRegistry() = delete;

        /** Replaces the registered cipher.
         * @param cipher Cipher to register; null restores the identity cipher.
         */
        static void set(std::shared_ptr<IPacketCipher> cipher);

        /** Gets the registered cipher.
         * @return Shared ownership of the current cipher.
         */
        [[nodiscard]] static std::shared_ptr<IPacketCipher> get();

        /** Encodes the type and payload region of a frame.
         * @param frame Frame bytes.
         * @param frame_size Frame byte count.
         */
        static void encode(std::uint8_t* frame, std::size_t frame_size);

        /** Decodes the type and payload region of a frame.
         * @param frame Frame bytes.
         * @param frame_size Frame byte count.
         */
        static void decode(std::uint8_t* frame, std::size_t frame_size);
    };

    /** Compatibility wrapper for registering a cipher. @param cipher Cipher to
     * register. */
    void set_packet_cipher(std::shared_ptr<IPacketCipher> cipher);

    /** Compatibility wrapper for accessing the cipher. @return Current cipher
     * reference. */
    [[nodiscard]] IPacketCipher& packet_cipher();

    /** Compatibility wrapper for encoding a frame. @param frame Frame bytes. @param
     * frame_size Size. */
    void encode_frame_inplace(std::uint8_t* frame, std::size_t frame_size);

    /** Compatibility wrapper for decoding a frame. @param frame Frame bytes. @param
     * frame_size Size. */
    void decode_frame_inplace(std::uint8_t* frame, std::size_t frame_size);

}  // namespace openkal::protocol
