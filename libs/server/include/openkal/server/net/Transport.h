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
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace openkal::server {

    /** Outcome of one nonwaiting transport write attempt. */
    enum class SendStatus { Progress, WouldBlock, Interrupted, Failed };

    /** Socket readiness needed before retrying a pending write. */
    enum class SendReadiness { Read, Write };

    /** Result of one transport write attempt. */
    struct SendResult {
        SendStatus status = SendStatus::Failed;         ///< Write outcome.
        std::size_t bytes = 0;                          ///< Bytes accepted on Progress.
        SendReadiness wait_for = SendReadiness::Write;  ///< Required readiness on WouldBlock.
    };

    /** TLS configuration for TCP; Unix-domain sockets do not use it. */
    struct TlsConfig {
        std::string certificate_file;  ///< Server PEM certificate chain.
        std::string private_key_file;  ///< Server PEM private key; never distribute to clients.
        std::string ca_file;           ///< Client trust PEM; empty uses system trust roots.
        std::string server_name;       ///< Expected certificate DNS name or IP; empty uses host.
    };

    /** Reads OPENKAL_TLS_CERT, OPENKAL_TLS_KEY, OPENKAL_TLS_CA and OPENKAL_TLS_SERVER_NAME.
     * @return TLS settings from the process environment.
     */
    [[nodiscard]] TlsConfig tls_config_from_environment();

    /** One connected Unix-domain or TCP peer. */
    class ISession {
      public:
        /** Destroys the session. */
        virtual ~ISession() = default;

        /** Returns the native socket descriptor. */
        [[nodiscard]] virtual int fd() const = 0;

        /** Reports whether the socket remains open. */
        [[nodiscard]] virtual bool alive() const = 0;

        /** Closes the socket. */
        virtual void close() = 0;

        /** Sends every byte in a buffer. */
        [[nodiscard]] virtual bool send_all(const std::uint8_t* data, std::size_t size) = 0;

        /** Sends every byte in a packet. */
        [[nodiscard]] virtual bool send_all(const std::vector<std::uint8_t>& data) {
            return send_all(data.data(), data.size());
        }

        /** Attempts one write without waiting for socket readiness.
         * After WouldBlock or Interrupted, retry the identical unchanged buffer before
         * sending other data or calling send_all. Unsupported sessions fail without
         * calling send_all.
         * @param data Buffer that remains valid until the attempt makes progress or fails.
         * @param size Byte count, unchanged across pending retries.
         * @return Outcome, accepted byte count, and readiness needed for a retry.
         */
        [[nodiscard]] virtual SendResult try_send(const std::uint8_t* data, std::size_t size) {
            (void) data;
            (void) size;
            return {};
        }

        /** Appends available bytes to a receive buffer. */
        [[nodiscard]] virtual bool recv_into(std::vector<std::uint8_t>& buffer) = 0;
    };

    /** Non-blocking server socket acceptor. */
    class IAcceptor {
      public:
        /** Destroys the acceptor. */
        virtual ~IAcceptor() = default;

        /** Binds and listens on an endpoint. */
        [[nodiscard]] virtual bool listen(const std::string& endpoint) = 0;

        /** Closes the listening socket. */
        virtual void close() = 0;

        /** Returns the native socket descriptor. */
        [[nodiscard]] virtual int fd() const = 0;

        /** Accepts one pending connection without blocking. */
        [[nodiscard]] virtual std::unique_ptr<ISession> try_accept() = 0;
    };

    /** Stateless transport factories and client connectors. */
    class Transport final {
      public:
        Transport() = delete;

        /** Creates a Unix-domain socket acceptor. */
        [[nodiscard]] static std::unique_ptr<IAcceptor> create_unix_acceptor();

        /** Creates a TLS-only TCP acceptor.
         * @param tls Server certificate and key configuration.
         * @return Acceptor; listen fails if credentials cannot be loaded.
         */
        [[nodiscard]] static std::unique_ptr<IAcceptor>
        create_tcp_acceptor(TlsConfig tls = tls_config_from_environment());

        /** Connects a Unix-domain client session. */
        [[nodiscard]] static std::unique_ptr<ISession> connect_unix(const std::string& path);

        /** Connects and authenticates a TLS 1.3 TCP server with bounded TCP and TLS waits.
         * @param host Server IPv4 address.
         * @param port Server port.
         * @param tls Client trust and expected identity configuration.
         * @param receive_buffer Optional socket receive-buffer size set before connecting; zero
         * keeps the system default.
         * @return Encrypted session, or null on connection or verification failure.
         */
        [[nodiscard]] static std::unique_ptr<ISession>
        connect_tcp(const std::string& host,
                    std::uint16_t port,
                    TlsConfig tls = tls_config_from_environment(),
                    int receive_buffer = 0);
    };

    /** Compatibility wrapper for Transport::create_unix_acceptor. */
    [[nodiscard]] std::unique_ptr<IAcceptor> create_unix_acceptor();

    /** Creates a TLS-only TCP acceptor.
     * @param tls Server certificate and key configuration.
     * @return Acceptor; listen fails if credentials cannot be loaded.
     */
    [[nodiscard]] std::unique_ptr<IAcceptor>
    create_tcp_acceptor(TlsConfig tls = tls_config_from_environment());

    /** Compatibility wrapper for Transport::connect_unix. */
    [[nodiscard]] std::unique_ptr<ISession> connect_unix(const std::string& path);

    /** Connects and authenticates a TLS 1.3 TCP server.
     * @param host Server IPv4 address.
     * @param port Server port.
     * @param tls Client trust and expected identity configuration.
     * @param receive_buffer Optional receive-buffer size set before connecting; zero keeps the
     * system default.
     * @return Encrypted session, or null on connection or verification failure.
     */
    [[nodiscard]] std::unique_ptr<ISession>
    connect_tcp(const std::string& host,
                std::uint16_t port,
                TlsConfig tls = tls_config_from_environment(),
                int receive_buffer = 0);

}  // namespace openkal::server
