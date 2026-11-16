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

#include <chrono>
#include <openssl/ssl.h>

namespace openkal::server {

    using TlsContext = std::shared_ptr<SSL_CTX>;

    /** Loads TLS 1.3 policy and credentials.
     * @param server Whether to load server credentials or client trust.
     * @param config Certificate, key and trust settings.
     * @return Context, or null on configuration failure.
     */
    [[nodiscard]] TlsContext create_tls_context(bool server, const TlsConfig& config);

    /** Owns one nonblocking TLS connection and its native socket. */
    class TlsSession final : public ISession {
      public:
        /** Initializes TLS state and takes ownership of fd, including on failure.
         * @param fd Connected nonblocking descriptor.
         * @param context TLS policy and credentials.
         * @param server Whether to accept rather than initiate TLS.
         * @param identity Expected server DNS name or IP for clients.
         */
        TlsSession(int fd, const TlsContext& context, bool server, const std::string& identity);
        ~TlsSession() override;

        /** Advances the handshake without waiting.
         * @return True when complete; alive distinguishes pending from failure.
         */
        [[nodiscard]] bool handshake_step();

        /** Completes the client handshake within the connection deadline.
         * @return Whether the authenticated handshake completed.
         */
        [[nodiscard]] bool handshake_wait();

        /** Gets the socket descriptor. @return Native descriptor, or -1. */
        int fd() const override;

        /** Checks connection state. @return Whether the socket remains open. */
        bool alive() const override;

        /** Releases TLS state and closes the socket. */
        void close() override;

        /** Sends authenticated bytes within a one-second deadline.
         * @param data Bytes to send.
         * @param size Byte count.
         * @return Whether all bytes were sent; failures close the connection.
         */
        bool send_all(const std::uint8_t* data, std::size_t size) override;

        /** Attempts one TLS write without polling or waiting.
         * @param data Buffer retained unchanged across pending retries.
         * @param size Byte count, unchanged across pending retries.
         * @return Accepted bytes or the readiness needed to repeat this exact write.
         */
        SendResult try_send(const std::uint8_t* data, std::size_t size) override;

        /** Appends authenticated plaintext without waiting for network input.
         * A pending try_send retry is completed before another SSL read is attempted.
         * @param buffer Receive buffer.
         * @return Whether the connection remains usable.
         */
        bool recv_into(std::vector<std::uint8_t>& buffer) override;

      private:
        int m_fd;
        std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl{nullptr, SSL_free};
        std::chrono::steady_clock::time_point m_deadline;
        int m_want = SSL_ERROR_WANT_READ;
        const std::uint8_t* m_writeRetryData = nullptr;  ///< Borrowed buffer pinned by SSL retry.
        std::size_t m_writeRetrySize = 0;
    };

}  // namespace openkal::server
