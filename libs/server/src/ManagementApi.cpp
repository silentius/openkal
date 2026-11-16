/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ManagementApi.h"

#include "openkal/protocol/Constants.h"

#include <algorithm>
#include <filesystem>
#include <openssl/crypto.h>
#include <sys/stat.h>
#include <thread>
#include <utility>

namespace openkal::server {
    using J = platform::Json;
    namespace {
        constexpr std::size_t kMaximumBytes = protocol::kMaxReceiveBufferLength;
        void text(J& object, const char* key, std::string_view value) {
            object.object()[key].set_type(J::Type::String);
            object.object()[key].string() = std::string(value);
        }
        std::vector<std::uint8_t> encode(const J& value) {
            const auto data = J::stringify(value, 0) + "\n";
            return {data.begin(), data.end()};
        }
        bool decode(const std::vector<std::uint8_t>& data, J& value) {
            return data.size() <= kMaximumBytes &&
                   J::parse(
                       std::string_view(reinterpret_cast<const char*>(data.data()), data.size()),
                       value) &&
                   value.is_object();
        }
    }  // namespace
    J management_error(std::string_view message) {
        auto result = J::parse("{}");
        text(result, "error", message);
        return result;
    }

    ManagementReply::ManagementReply(J response) : m_response(std::move(response)) {}

    ManagementReply::ManagementReply(std::function<std::optional<J>()> poll_response)
        : m_poll_response(std::move(poll_response)) {}

    std::optional<J> ManagementReply::poll() {
        if (m_response) {
            return std::exchange(m_response, std::nullopt);
        }
        if (!m_poll_response) {
            return std::nullopt;
        }
        auto response = m_poll_response();
        if (response) {
            m_poll_response = {};
        }
        return response;
    }

    bool ManagementApi::start(const std::string& path, std::string token, std::string& error) {
        close();
        error.clear();
        if (path.empty()) {
            return true;
        }
        if (token.size() < 32 || token.size() > 256 || path.front() != '/') {
            error = "Management API requires an absolute Unix socket path and a 32-256 byte token.";
            return false;
        }
        std::error_code ec;
        const auto status = std::filesystem::symlink_status(path, ec);
        if (std::filesystem::exists(status)) {
            error = "Management socket path already exists; another server or a stale socket may "
                    "own it.";
            return false;
        }
        m_acceptor = create_unix_acceptor();
        if (!m_acceptor->listen(path) || ::chmod(path.c_str(), 0600) != 0) {
            error = "Cannot create the owner-only management socket.";
            close();
            return false;
        }
        m_token = std::move(token);
        m_instance = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        m_events.clear();
        m_cursor = 0;
        return true;
    }
    void ManagementApi::close() {
        m_peers.clear();
        m_acceptor.reset();
        m_token.clear();
    }
    bool ManagementApi::enabled() const {
        return m_acceptor != nullptr;
    }
    void ManagementApi::poll(const std::function<ManagementReply(const J&)>& handler) {
        if (!m_acceptor) {
            return;
        }
        for (int n = 0; n < 8; ++n) {
            auto session = m_acceptor->try_accept();
            if (!session) {
                break;
            }
            if (m_peers.size() < 16) {
                m_peers.push_back({std::move(session), {}, std::chrono::steady_clock::now(), {}});
            }
        }
        for (auto& peer : m_peers) {
            if (std::chrono::steady_clock::now() - peer.created > std::chrono::seconds(5) ||
                !peer.session->recv_into(peer.bytes) || peer.bytes.size() >= kMaximumBytes) {
                peer.session->close();
                continue;
            }
            if (!peer.reply) {
                if (std::find(peer.bytes.begin(), peer.bytes.end(), '\n') == peer.bytes.end()) {
                    continue;
                }
                J request;
                peer.reply.emplace(management_error("Invalid management request."));
                if (decode(peer.bytes, request)) {
                    const auto token = request.at("token").as_string();
                    if (token.size() != m_token.size() ||
                        CRYPTO_memcmp(token.data(), m_token.data(), m_token.size()) != 0) {
                        peer.reply.emplace(management_error("Management authentication failed."));
                    } else {
                        request.object().erase("token");
                        try {
                            peer.reply.emplace(handler(request));
                        } catch (...) {
                            peer.reply.emplace(management_error("Management operation failed."));
                        }
                    }
                }
            }
            std::optional<J> response;
            try {
                response = peer.reply->poll();
            } catch (...) {
                response = management_error("Management operation failed.");
            }
            if (!response) {
                continue;
            }
            auto bytes = encode(*response);
            if (bytes.size() > kMaximumBytes) {
                bytes = encode(management_error("Management response exceeded its limit."));
            }
            (void) peer.session->send_all(bytes);
            peer.session->close();
        }
        m_peers.erase(std::remove_if(m_peers.begin(),
                                     m_peers.end(),
                                     [](const auto& p) { return !p.session->alive(); }),
                      m_peers.end());
    }
    void ManagementApi::record_pm(int character,
                                  std::string_view name,
                                  std::string_view body,
                                  bool outgoing) {
        auto event = J::parse("{}");
        event.object()["id"].set_number(static_cast<double>(++m_cursor));
        event.object()["character"].set_number(character);
        event.object()["outgoing"].set_type(J::Type::Bool);
        event.object()["outgoing"].set_boolean(outgoing);
        event.object()["time"].set_number(
            static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count()));
        text(event, "name", name);
        text(event, "body", body);
        m_events.push_back(std::move(event));
        if (m_events.size() > 1000) {
            m_events.pop_front();
        }
    }
    J ManagementApi::events(std::uint64_t after) const {
        auto result = J::parse(R"({"events":[],"cursor":0,"gap":false,"more":false})");
        text(result, "instance", m_instance);
        const bool gap = after > m_cursor ||
                         (!m_events.empty() && after + 1 < static_cast<std::uint64_t>(
                                                               m_events.front().at("id").number()));
        if (after > m_cursor) {
            after = 0;
        }
        auto cursor = after;
        for (const auto& event : m_events) {
            const auto id = static_cast<std::uint64_t>(event.at("id").number());
            if (id <= after) {
                continue;
            }
            if (result.object()["events"].array().size() == 25) {
                result.object()["more"].set_boolean(true);
                break;
            }
            result.object()["events"].array().push_back(event);
            cursor = id;
        }
        result.object()["cursor"].set_number(static_cast<double>(cursor));
        result.object()["gap"].set_boolean(gap);
        return result;
    }
    J management_request(const std::string& path, const std::string& token, J command) {
        auto session = connect_unix(path);
        if (!session) {
            return management_error("Cannot connect to the local management API.");
        }
        text(command, "token", token);
        auto bytes = encode(command);
        if (bytes.size() > kMaximumBytes || !session->send_all(bytes)) {
            return management_error("Cannot send management request.");
        }
        bytes.clear();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            const bool alive = session->recv_into(bytes);
            if (bytes.size() > kMaximumBytes) {
                break;
            }
            if (std::find(bytes.begin(), bytes.end(), '\n') != bytes.end()) {
                J response;
                if (decode(bytes, response)) {
                    return response;
                }
                break;
            }
            if (!alive) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return management_error(
            "Management response missing or timed out; delivery status may be unknown.");
    }
}  // namespace openkal::server
