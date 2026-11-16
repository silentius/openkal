/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/TestControl.h"

#include "openkal/protocol/FrameParser.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace openkal::server {
    std::string test_identity(std::string_view prefix, int ordinal) {
        std::ostringstream out;
        out << prefix << std::setfill('0') << std::setw(4) << ordinal;
        return out.str();
    }

    std::string test_character_snapshot(const CharacterRecord& character) {
        protocol::PacketWriter writer;
        write_lobby_character(writer, character);
        constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (auto byte : writer.payload()) {
            out += hex[byte >> 4];
            out += hex[byte & 15];
        }
        return out;
    }

    std::optional<CharacterRecord> read_test_character(std::string_view snapshot) {
        if (snapshot.empty() || snapshot.size() > 4096 || snapshot.size() % 2) {
            return std::nullopt;
        }
        const auto digit = [](char c) {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            return -1;
        };
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < snapshot.size(); i += 2) {
            const auto a = digit(snapshot[i]), b = digit(snapshot[i + 1]);
            if (a < 0 || b < 0) {
                return std::nullopt;
            }
            bytes.push_back(static_cast<std::uint8_t>((a << 4) | b));
        }
        protocol::PacketReader reader(bytes);
        CharacterRecord record;
        if (!read_lobby_character(reader, record) || reader.remaining()) {
            return std::nullopt;
        }
        return record;
    }

    std::vector<std::uint8_t> frame_test_request(std::string_view token,
                                                 const platform::Json& command) {
        protocol::PacketWriter writer;
        writer.write_string(token);
        writer.write_string(platform::Json::stringify(command, 0));
        return writer.frame(kTestRequest);
    }

    bool TestControlClient::connect(const std::string& host,
                                    std::uint16_t port,
                                    std::string token,
                                    TlsConfig tls,
                                    int receive_buffer) {
        m_token = std::move(token);
        m_session = connect_tcp(host, port, std::move(tls), receive_buffer);
        return m_session != nullptr;
    }

    platform::Json TestControlClient::request(const platform::Json& command,
                                              const std::function<void()>& pump) {
        auto error = [](const char* message) {
            platform::Json result;
            result.set_type(platform::Json::Type::Object);
            result.object()["error"].set_type(platform::Json::Type::String);
            result.object()["error"].string() = message;
            return result;
        };
        if (!m_session || !m_session->send_all(frame_test_request(m_token, command))) {
            return error("Test-control connection is closed.");
        }
        std::vector<std::uint8_t> rx;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!m_session->recv_into(rx) || rx.size() > 131072) {
                break;
            }
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            const auto state =
                protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
            if (state == protocol::FrameParseStatus::Invalid) {
                break;
            }
            if (state == protocol::FrameParseStatus::Complete) {
                if (type != kTestResponse || consumed != rx.size()) {
                    break;
                }
                protocol::PacketReader reader(payload);
                std::string json;
                if (!reader.read_string(json) || reader.remaining()) {
                    break;
                }
                try {
                    return platform::Json::parse(json);
                } catch (...) {
                    break;
                }
            }
            if (pump) {
                pump();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        m_session.reset();
        return error("Test-control request failed or timed out.");
    }

    std::unique_ptr<ISession> TestControlClient::release_session() {
        return std::move(m_session);
    }
}  // namespace openkal::server
