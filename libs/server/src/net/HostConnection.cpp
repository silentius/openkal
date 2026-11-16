/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/net/HostConnection.h"

#include <algorithm>
#include <poll.h>
#include <utility>

namespace openkal::server {

    OutputStatus OutputReceipt::status() const {
        return m_status.load(std::memory_order_acquire);
    }

    HostConnection::HostConnection(std::unique_ptr<ISession> session)
        : m_session(std::move(session)) {}

    HostConnection::~HostConnection() {
        close();
    }

    int HostConnection::fd() const {
        return m_session ? m_session->fd() : -1;
    }

    bool HostConnection::alive() const {
        return !m_closing && transport_alive();
    }

    bool HostConnection::transport_alive() const {
        return m_session && m_session->alive();
    }

    void HostConnection::close() {
        if (m_session) {
            m_session->close();
        }
        for (const auto& chunk : m_output) {
            if (chunk.receipt) {
                chunk.receipt->m_status.store(OutputStatus::Failed, std::memory_order_release);
            }
        }
        m_output.clear();
        m_retained_bytes = 0;
        m_pending_bytes = 0;
        m_retry_size = 0;
        m_closing = true;
    }

    void HostConnection::close_after_flush() {
        m_closing = true;
        if (!pending()) {
            close();
        }
    }

    bool HostConnection::enqueue(std::vector<std::uint8_t> bytes, Clock::time_point now) {
        if (!alive()) {
            return false;
        }
        if (bytes.empty()) {
            return true;
        }
        if (bytes.capacity() > kMaxQueuedBytes - m_retained_bytes ||
            m_output.size() >= kMaxQueuedChunks) {
            close();
            return false;
        }
        const auto size = bytes.size();
        const auto capacity = bytes.capacity();
        m_output.push_back({std::move(bytes), 0, now + std::chrono::seconds(1), nullptr});
        m_retained_bytes += capacity;
        m_pending_bytes += size;
        return true;
    }

    std::shared_ptr<const OutputReceipt>
    HostConnection::enqueue_tracked(std::vector<std::uint8_t> bytes, Clock::time_point now) {
        auto receipt = std::make_shared<OutputReceipt>();
        const bool empty = bytes.empty();
        if (!enqueue(std::move(bytes), now)) {
            receipt->m_status.store(OutputStatus::Failed, std::memory_order_release);
        } else if (empty) {
            receipt->m_status.store(OutputStatus::Sent, std::memory_order_release);
        } else {
            m_output.back().receipt = receipt;
        }
        return receipt;
    }

    bool HostConnection::flush(short revents,
                               Clock::time_point now,
                               std::size_t byte_budget,
                               std::size_t attempt_budget) {
        if (!transport_alive()) {
            close();
            return false;
        }
        if ((revents & (POLLERR | POLLNVAL)) != 0) {
            close();
            return false;
        }
        while (!m_output.empty()) {
            auto& front = m_output.front();
            if (now >= front.deadline) {
                close();
                return false;
            }
            if (byte_budget == 0 || attempt_budget == 0) {
                break;
            }
            if (m_retry_size != 0) {
                if (m_retry_size > byte_budget) {
                    break;
                }
                const auto wanted = m_retry_readiness == SendReadiness::Read ? POLLIN : POLLOUT;
                if ((revents & wanted) == 0) {
                    break;
                }
            }
            const auto request_size =
                m_retry_size != 0 ? m_retry_size
                                  : std::min(front.bytes.size() - front.offset, byte_budget);
            --attempt_budget;
            const auto result =
                m_session->try_send(front.bytes.data() + front.offset, request_size);
            if (result.status == SendStatus::Progress) {
                if (result.bytes == 0 || result.bytes > request_size) {
                    close();
                    return false;
                }
                m_retry_size = 0;
                front.offset += result.bytes;
                m_pending_bytes -= result.bytes;
                byte_budget -= result.bytes;
                if (front.offset == front.bytes.size()) {
                    if (front.receipt) {
                        front.receipt->m_status.store(OutputStatus::Sent,
                                                      std::memory_order_release);
                    }
                    m_retained_bytes -= front.bytes.capacity();
                    m_output.pop_front();
                }
                continue;
            }
            if ((result.status == SendStatus::WouldBlock ||
                 result.status == SendStatus::Interrupted) &&
                result.bytes == 0) {
                m_retry_size = request_size;
                m_retry_readiness = result.wait_for;
                break;
            }
            close();
            return false;
        }
        if (m_closing && !pending()) {
            close();
        }
        return transport_alive();
    }

    bool HostConnection::recv_into(std::vector<std::uint8_t>& buffer) {
        if (!transport_alive()) {
            return false;
        }
        if (pending() || m_closing) {
            return true;
        }
        if (!m_session->recv_into(buffer)) {
            close();
            return false;
        }
        return true;
    }

    bool HostConnection::pending() const {
        return !m_output.empty();
    }

    std::size_t HostConnection::pending_bytes() const {
        return m_pending_bytes;
    }

    short HostConnection::wanted_events() const {
        if (!transport_alive()) {
            return 0;
        }
        if (m_retry_size != 0) {
            return m_retry_readiness == SendReadiness::Read ? POLLIN : POLLOUT;
        }
        return static_cast<short>((m_closing ? 0 : POLLIN) | (pending() ? POLLOUT : 0));
    }

}  // namespace openkal::server
