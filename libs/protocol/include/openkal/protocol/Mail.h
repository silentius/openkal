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
#include <string>
#include <vector>

namespace openkal::protocol {

    /** Mail commands carried by the reserved C2S/S2C mail opcodes. */
    enum class MailCommand : std::uint8_t {
        List = 0,
        Claim = 1,
        Read = 2,
        Delete = 3,
    };

    /** One server-authoritative mail entry; item_index and item_count are both zero without an
     * attachment. */
    struct MailEntry {
        std::uint32_t id = 0;
        std::uint16_t item_index = 0;
        std::uint32_t item_count = 0;
        std::uint64_t created_at = 0;
        bool claimed = false;
        bool read = false;
        std::string subject;
        std::string body;
    };

    /** Response to an item-claim request. */
    struct MailClaimResult {
        std::uint32_t id = 0;
        bool claimed = false;
    };

    /** Encodes and decodes system-mail packets. */
    class Mail final {
      public:
        Mail() = delete;

        /** Builds C2S request for the complete inbox. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t> frame_list_c2s();

        /** Builds C2S request to claim one attachment.
         * @param id Durable mail identifier.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_claim_c2s(std::uint32_t id);

        /** Builds a read-state or deletion request.
         * @param command Read or Delete operation.
         * @param id Durable mail identifier.
         * @return Framed packet, or empty for invalid arguments.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_c2s(MailCommand command,
                                                                        std::uint32_t id);

        /** Parses a C2S mail request.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param command Request kind.
         * @param id Requested mail id (zero for List).
         * @return True when the request is valid.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            MailCommand& command,
                                            std::uint32_t& id);

        /** Builds the S2C inbox snapshot.
         * @param entries Server-authoritative mail entries.
         * @return Framed packet, or empty when oversized.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_list_s2c(const std::vector<MailEntry>& entries);

        /** Builds an S2C claim result.
         * @param result Claim result.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_claim_result_s2c(const MailClaimResult& result);

        /** Parses an S2C inbox snapshot.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param entries Decoded entries.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_list_s2c(const std::uint8_t* data, std::size_t size, std::vector<MailEntry>& entries);

        /** Parses an S2C claim result.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param result Decoded result.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_claim_result_s2c(const std::uint8_t* data, std::size_t size, MailClaimResult& result);
    };

}  // namespace openkal::protocol
