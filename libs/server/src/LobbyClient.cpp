/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/LobbyClient.h"

#include "openkal/protocol/FrameParser.h"

#include <chrono>
#include <thread>
#include <utility>

namespace openkal::server {
    bool LobbyClient::request(LobbyOperation op,
                              const protocol::PacketWriter& payload,
                              std::vector<std::uint8_t>& out) {
        m_error.clear();
        protocol::PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(op));
        w.write_bytes(payload.payload().data(), payload.payload().size());
        if (!m_session || !m_session->send_all(w.frame(kLobbyRequest))) {
            m_error = "Connection closed.";
            return false;
        }
        std::vector<std::uint8_t> rx;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!m_session->recv_into(rx) || !m_session->alive()) {
                break;
            }
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            std::vector<std::uint8_t> bytes;
            const auto status =
                protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, bytes);
            if (status == protocol::FrameParseStatus::Invalid) {
                break;
            }
            if (status == protocol::FrameParseStatus::Complete) {
                protocol::PacketReader r(bytes);
                std::uint8_t operation = 0, success = 0;
                std::string message;
                if (consumed != rx.size() || type != kLobbyResponse || !r.read_u8(operation) ||
                    operation != static_cast<std::uint8_t>(op) || !r.read_u8(success) ||
                    !r.read_string(message) || message.size() > 256) {
                    break;
                }
                if (!success) {
                    m_error = message;
                    return false;
                }
                out.assign(bytes.end() - static_cast<std::ptrdiff_t>(r.remaining()), bytes.end());
                return true;
            }
            if (m_wait && !m_wait()) {
                m_error = "Request cancelled.";
                close();
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        m_error = "Server disconnected or timed out.";
        close();
        return false;
    }
    bool LobbyClient::connect(const std::string& host, std::uint16_t port, TlsConfig tls) {
        close();
        m_session = connect_tcp(host, port, std::move(tls));
        if (!m_session) {
            m_error = "Could not establish a trusted TLS connection.";
            return false;
        }
        protocol::PacketWriter w;
        std::vector<std::uint8_t> bytes;
        if (!request(LobbyOperation::Capabilities, w, bytes)) {
            return false;
        }
        protocol::PacketReader r(bytes);
        std::uint8_t version = 0, allowed = 0;
        if (!r.read_u8(version) || version != 1 || !r.read_u8(allowed) || r.remaining()) {
            m_error = "Unsupported lobby version.";
            close();
            return false;
        }
        m_registration_allowed = allowed == 1;
        return true;
    }
    bool LobbyClient::connect_local(const std::string& path) {
        close();
        m_session = connect_unix(path);
        protocol::PacketWriter writer;
        std::vector<std::uint8_t> bytes;
        if (!request(LobbyOperation::Capabilities, writer, bytes)) {
            return false;
        }
        protocol::PacketReader reader(bytes);
        std::uint8_t version = 0, allowed = 0;
        return reader.read_u8(version) && version == 1 && reader.read_u8(allowed) &&
               !reader.remaining();
    }

    bool LobbyClient::register_account(const std::string& name,
                                       const std::string& email,
                                       const std::string& password) {
        protocol::PacketWriter w;
        w.write_string(name);
        w.write_string(email);
        w.write_string(password);
        std::vector<std::uint8_t> bytes;
        return request(LobbyOperation::Register, w, bytes);
    }
    bool LobbyClient::login(const std::string& name, const std::string& password) {
        protocol::PacketWriter w;
        w.write_string(name);
        w.write_string(password);
        std::vector<std::uint8_t> bytes;
        if (!request(LobbyOperation::Login, w, bytes)) {
            return false;
        }
        protocol::PacketReader r(bytes);
        return r.read_i32(m_account_id) && m_account_id > 0 && !r.remaining();
    }
    std::vector<CharacterRecord> LobbyClient::characters(LobbyOperation op) {
        protocol::PacketWriter w;
        std::vector<std::uint8_t> bytes;
        if (!request(op, w, bytes)) {
            return {};
        }
        protocol::PacketReader r(bytes);
        std::uint8_t count = 0;
        if (!r.read_u8(count) || count > kMaxCharactersPerAccount) {
            m_error = "Invalid character list.";
            return {};
        }
        std::vector<CharacterRecord> out;
        for (unsigned i = 0; i < count; ++i) {
            CharacterRecord c;
            if (!read_lobby_character(r, c) || c.account_id != m_account_id) {
                return {};
            }
            out.push_back(c);
        }
        return !r.remaining() ? out : std::vector<CharacterRecord>{};
    }
    std::vector<CharacterRecord> LobbyClient::list_characters(std::int32_t account) {
        return account == m_account_id ? characters(LobbyOperation::Characters)
                                       : std::vector<CharacterRecord>{};
    }
    std::vector<CharacterRecord> LobbyClient::list_deleted_characters(std::int32_t account,
                                                                      std::int64_t) {
        return account == m_account_id ? characters(LobbyOperation::Deleted)
                                       : std::vector<CharacterRecord>{};
    }
    std::optional<CharacterRecord> LobbyClient::load_character(std::int32_t id) {
        for (const auto& c : characters(LobbyOperation::Characters)) {
            if (c.id == id) {
                return c;
            }
        }
        return std::nullopt;
    }
    bool LobbyClient::save_character(const CharacterRecord& c) {
        protocol::PacketWriter w;
        write_lobby_character(w, c);
        std::vector<std::uint8_t> b;
        return c.id == 0 && request(LobbyOperation::Create, w, b);
    }
    std::vector<ItemRecord> LobbyClient::load_items(std::int32_t id) {
        protocol::PacketWriter w;
        w.write_i32(id);
        std::vector<std::uint8_t> b;
        if (!request(LobbyOperation::Items, w, b)) {
            return {};
        }
        protocol::PacketReader r(b);
        std::uint8_t n = 0;
        if (!r.read_u8(n) || n > 20) {
            return {};
        }
        std::vector<ItemRecord> out;
        for (unsigned i = 0; i < n; ++i) {
            ItemRecord item;
            if (!read_lobby_item(r, item) || item.character_id != id) {
                return {};
            }
            out.push_back(item);
        }
        return !r.remaining() ? out : std::vector<ItemRecord>{};
    }
    bool
    LobbyClient::schedule_character_deletion(std::int32_t id, std::int32_t account, std::int64_t) {
        protocol::PacketWriter w;
        w.write_i32(id);
        std::vector<std::uint8_t> b;
        return account == m_account_id && request(LobbyOperation::Delete, w, b);
    }
    bool LobbyClient::restore_character(std::int32_t id, std::int32_t account, std::int64_t) {
        protocol::PacketWriter w;
        w.write_i32(id);
        std::vector<std::uint8_t> b;
        return account == m_account_id && request(LobbyOperation::Restore, w, b);
    }
    std::optional<CharacterRecord> LobbyClient::select(std::int32_t id) {
        protocol::PacketWriter w;
        w.write_i32(id);
        std::vector<std::uint8_t> b;
        if (!request(LobbyOperation::Select, w, b)) {
            return std::nullopt;
        }
        protocol::PacketReader r(b);
        CharacterRecord c;
        if (!read_lobby_character(r, c) || r.remaining()) {
            return std::nullopt;
        }
        return c;
    }
    std::unique_ptr<ISession> LobbyClient::release_session() {
        m_account_id = 0;
        return std::move(m_session);
    }
    void LobbyClient::close() {
        m_session.reset();
        m_account_id = 0;
        m_registration_allowed = false;
    }
    const std::string& LobbyClient::error() const {
        return m_error;
    }
    bool LobbyClient::registration_allowed() const {
        return m_registration_allowed;
    }
    bool LobbyClient::connected() const {
        return m_session && m_session->alive();
    }
    std::int32_t LobbyClient::account_id() const {
        return m_account_id;
    }
    void LobbyClient::set_wait_callback(std::function<bool()> callback) {
        m_wait = std::move(callback);
    }
}  // namespace openkal::server
