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

#include "openkal/server/net/Transport.h"

#include <atomic>
#include <chrono>
#include <deque>

namespace openkal::server {

    /** Transmission state of one tracked output packet. */
    enum class OutputStatus { Pending, Sent, Failed };

    /** Observes completion of a queued packet without taking ownership of its connection. */
    class OutputReceipt final {
      public:
        /** Initializes a pending receipt. */
        OutputReceipt() = default;

        /** Reads packet completion from any thread.
         * @return Pending until every byte is accepted by the transport, Sent afterward,
         * or Failed when the queued packet cannot be completed.
         */
        [[nodiscard]] OutputStatus status() const;

      private:
        friend class HostConnection;
        std::atomic<OutputStatus> m_status{OutputStatus::Pending};
    };

    /** Owns a host peer and its bounded, non-blocking output queue. */
    class HostConnection final {
      public:
        using Clock = std::chrono::steady_clock;

        /** Takes ownership of a connected transport.
         * @param session Connected transport, or null for a closed connection.
         */
        explicit HostConnection(std::unique_ptr<ISession> session);

        /** Closes the transport before releasing buffers borrowed by a TLS retry. */
        ~HostConnection();

        /** Returns the native descriptor, or -1 after disconnection.
         * @return Transport descriptor.
         */
        [[nodiscard]] int fd() const;

        /** Reports whether the peer still accepts application input and output.
         * @return Whether the transport is open and no graceful close was requested.
         */
        [[nodiscard]] bool alive() const;

        /** Closes the transport immediately and discards unsent output. */
        void close();

        /** Stops accepting input and closes after already queued output is sent. */
        void close_after_flush();

        /** Accepts an owned packet for later transmission, closing on queue overflow.
         * @param bytes Packet bytes; empty packets consume no queue capacity.
         * @param now Time used to set the packet's one-second deadline.
         * @return Whether the packet was accepted, without promising wire delivery.
         */
        bool enqueue(std::vector<std::uint8_t> bytes, Clock::time_point now = Clock::now());

        /** Queues a packet and provides a receipt for complete transport transmission.
         * @param bytes Packet bytes; an accepted empty packet completes immediately.
         * @param now Time used to set the packet's one-second deadline.
         * @return Receipt that fails on rejection or connection closure before full transmission.
         */
        [[nodiscard]] std::shared_ptr<const OutputReceipt>
        enqueue_tracked(std::vector<std::uint8_t> bytes, Clock::time_point now = Clock::now());

        /** Sends a bounded amount of queued output without waiting for readiness.
         * A pinned TLS retry waits until its unchanged request fits the byte budget.
         * @param revents Current poll readiness flags for this transport.
         * @param now Time used to check the front packet's deadline.
         * @param byte_budget Maximum bytes accepted during this flush.
         * @param attempt_budget Maximum transport calls.
         * @return Whether the transport remains open after flushing.
         */
        bool flush(short revents,
                   Clock::time_point now = Clock::now(),
                   std::size_t byte_budget = 64 * 1024,
                   std::size_t attempt_budget = 64);

        /** Receives available bytes after queued output has drained.
         * @param buffer Receive buffer to append to.
         * @return Whether the transport remains open.
         */
        [[nodiscard]] bool recv_into(std::vector<std::uint8_t>& buffer);

        /** Reports whether unsent output remains.
         * @return Whether the queue is nonempty.
         */
        [[nodiscard]] bool pending() const;

        /** Returns the number of bytes that have not yet been sent.
         * @return Unsent byte count.
         */
        [[nodiscard]] std::size_t pending_bytes() const;

        /** Returns the readiness flags needed for receiving or advancing output.
         * @return A combination of POLLIN and POLLOUT, or zero when closed.
         */
        [[nodiscard]] short wanted_events() const;

      private:
        /** Reports whether queued output can still use the owned transport. */
        [[nodiscard]] bool transport_alive() const;

        struct Chunk {
            std::vector<std::uint8_t> bytes;
            std::size_t offset = 0;
            Clock::time_point deadline;
            std::shared_ptr<OutputReceipt> receipt;
        };

        static constexpr std::size_t kMaxQueuedBytes = 1024 * 1024;
        static constexpr std::size_t kMaxQueuedChunks = 4096;

        std::unique_ptr<ISession> m_session;
        std::deque<Chunk> m_output;
        std::size_t m_retained_bytes = 0;  ///< Buffer capacities retained until chunks complete.
        std::size_t m_pending_bytes = 0;
        std::size_t m_retry_size = 0;
        SendReadiness m_retry_readiness = SendReadiness::Write;
        bool m_closing = false;
    };

}  // namespace openkal::server
