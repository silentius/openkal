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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Fields carried by the server code challenge. */
    struct S2CCodeFields {
        std::uint32_t protocol_test_value = 0;
        std::uint8_t decode_code = 0;
        std::uint32_t time_stamp = 0;
        std::uint32_t time_start = 0;
        std::uint32_t system_flags = 0;
        std::uint32_t event_flags = 0;
        std::uint8_t server_id = 0;
        std::uint8_t age_limit = 0;
        std::uint8_t country = 0;
    };

    /** Encodes and decodes handshake messages. */
    class Handshake final {
      public:
        Handshake() = delete;

        /** Computes the client answer version.
         * @param protocol_test_value Server challenge.
         * @param seed Protocol divisor.
         * @param magic Protocol base value.
         * @return Encoded answer version.
         */
        [[nodiscard]] static std::uint32_t
        compute_answer_version(std::uint32_t protocol_test_value,
                               std::uint32_t seed = kProtocolSeed,
                               std::uint32_t magic = kProtocolMagic);

        /** Verifies a client answer version.
         * @param protocol_test_value Server challenge.
         * @param client_version Client answer.
         * @param seed Protocol divisor.
         * @param magic Protocol base value.
         * @return True when the answer matches.
         */
        [[nodiscard]] static bool verify_answer_version(std::uint32_t protocol_test_value,
                                                        std::uint32_t client_version,
                                                        std::uint32_t seed = kProtocolSeed,
                                                        std::uint32_t magic = kProtocolMagic);

        /** Builds C2S_CONNECT. @param app_time_ms Application tick. @return Framed
         * packet. */
        [[nodiscard]] static std::vector<std::uint8_t> build_connect(std::uint32_t app_time_ms);

        /** Builds C2S_ANS_CODE. @param protocol_test_value Challenge. @return Framed
         * packet. */
        [[nodiscard]] static std::vector<std::uint8_t>
        build_answer(std::uint32_t protocol_test_value);

        /** Parses an S2C_CODE payload.
         * @param payload Payload bytes.
         * @param size Payload size.
         * @param out Decoded fields.
         * @return True on success.
         */
        [[nodiscard]] static bool
        parse_code(const std::uint8_t* payload, std::size_t size, S2CCodeFields& out);
    };

    [[nodiscard]] std::uint32_t compute_ans_code_version(std::uint32_t protocol_test_value,
                                                         std::uint32_t seed = kProtocolSeed,
                                                         std::uint32_t magic = kProtocolMagic);
    [[nodiscard]] bool verify_ans_code_version(std::uint32_t protocol_test_value,
                                               std::uint32_t client_version,
                                               std::uint32_t seed = kProtocolSeed,
                                               std::uint32_t magic = kProtocolMagic);
    [[nodiscard]] std::vector<std::uint8_t> build_c2s_connect(std::uint32_t app_time_ms);
    [[nodiscard]] std::vector<std::uint8_t> build_c2s_ans_code(std::uint32_t protocol_test_value);
    [[nodiscard]] bool
    parse_s2c_code(const std::uint8_t* payload, std::size_t size, S2CCodeFields& out);

}  // namespace openkal::protocol
