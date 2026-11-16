/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Handshake.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::uint32_t Handshake::compute_answer_version(std::uint32_t protocol_test_value,
                                                    std::uint32_t seed,
                                                    std::uint32_t magic) {
        return (protocol_test_value % seed) + magic;
    }

    bool Handshake::verify_answer_version(std::uint32_t protocol_test_value,
                                          std::uint32_t client_version,
                                          std::uint32_t seed,
                                          std::uint32_t magic) {
        return client_version >= magic &&
               client_version == compute_answer_version(protocol_test_value, seed, magic);
    }

    std::vector<std::uint8_t> Handshake::build_connect(std::uint32_t app_time_ms) {
        PacketWriter w;
        w.write_u32(app_time_ms);
        return w.frame(static_cast<std::uint8_t>(C2S::Connect));
    }

    std::vector<std::uint8_t> Handshake::build_answer(std::uint32_t protocol_test_value) {
        PacketWriter w;
        w.write_u8(1);
        w.write_u32(compute_answer_version(protocol_test_value));
        return w.frame(static_cast<std::uint8_t>(C2S::AnsCode));
    }

    bool Handshake::parse_code(const std::uint8_t* payload, std::size_t size, S2CCodeFields& out) {
        PacketReader r(payload, size);
        // Format "dbddddbbb" (9 fields) ? matches client KSocket S2C_CODE without
        // HackShield.
        return r.read_format("dbddddbbb",
                             &out.protocol_test_value,
                             &out.decode_code,
                             &out.time_stamp,
                             &out.time_start,
                             &out.system_flags,
                             &out.event_flags,
                             &out.server_id,
                             &out.age_limit,
                             &out.country);
    }

    std::uint32_t compute_ans_code_version(std::uint32_t protocol_test_value,
                                           std::uint32_t seed,
                                           std::uint32_t magic) {
        return Handshake::compute_answer_version(protocol_test_value, seed, magic);
    }

    bool verify_ans_code_version(std::uint32_t protocol_test_value,
                                 std::uint32_t client_version,
                                 std::uint32_t seed,
                                 std::uint32_t magic) {
        return Handshake::verify_answer_version(protocol_test_value, client_version, seed, magic);
    }

    std::vector<std::uint8_t> build_c2s_connect(std::uint32_t app_time_ms) {
        return Handshake::build_connect(app_time_ms);
    }

    std::vector<std::uint8_t> build_c2s_ans_code(std::uint32_t protocol_test_value) {
        return Handshake::build_answer(protocol_test_value);
    }

    bool parse_s2c_code(const std::uint8_t* payload, std::size_t size, S2CCodeFields& out) {
        return Handshake::parse_code(payload, size, out);
    }

}  // namespace openkal::protocol
