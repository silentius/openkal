/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TlsSession.h"

#include "openkal/platform/Log.h"
#include "openkal/protocol/Constants.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace openkal::server {
    namespace {
        using Clock = std::chrono::steady_clock;

        // Keep SIGPIPE local to this socket instead of changing process signal policy.
        int socket_write(BIO* bio, const char* data, int size) {
            BIO_clear_retry_flags(bio);
            const int fd = *static_cast<int*>(BIO_get_data(bio));
            const int n =
                static_cast<int>(::send(fd, data, static_cast<std::size_t>(size), MSG_NOSIGNAL));
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                BIO_set_retry_write(bio);
            }
            return n;
        }

        int socket_read(BIO* bio, char* data, int size) {
            BIO_clear_retry_flags(bio);
            const int fd = *static_cast<int*>(BIO_get_data(bio));
            const int n = static_cast<int>(::recv(fd, data, static_cast<std::size_t>(size), 0));
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                BIO_set_retry_read(bio);
            }
            return n;
        }

        const BIO_METHOD* socket_method() {
            static const auto method = [] {
                std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> value(
                    BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(), "OpenKal socket"),
                    BIO_meth_free);
                if (!value || BIO_meth_set_write(value.get(), socket_write) != 1 ||
                    BIO_meth_set_read(value.get(), socket_read) != 1 ||
                    BIO_meth_set_ctrl(value.get(), [](BIO*, int cmd, long, void*) -> long {
                        return cmd == BIO_CTRL_FLUSH ? 1 : 0;
                    }) != 1) {
                    value.reset();
                }
                return value;
            }();
            return method.get();
        }

        bool wait_socket(int fd, int error, Clock::time_point deadline) {
            while (Clock::now() < deadline) {
                const auto remaining =
                    std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now())
                        .count();
                pollfd p{
                    fd, static_cast<short>(error == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN), 0};
                const int result =
                    ::poll(&p, 1, static_cast<int>(std::max<std::int64_t>(1, remaining)));
                if (result > 0) {
                    return (p.revents & p.events) != 0;
                }
                if (result == 0 || errno != EINTR) {
                    return false;
                }
            }
            return false;
        }

        bool retryable(int error) {
            return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE;
        }
    }  // namespace

    TlsConfig tls_config_from_environment() {
        const auto env = [](const char* name) {
            const char* value = std::getenv(name);
            return value ? std::string(value) : std::string{};
        };
        return {env("OPENKAL_TLS_CERT"),
                env("OPENKAL_TLS_KEY"),
                env("OPENKAL_TLS_CA"),
                env("OPENKAL_TLS_SERVER_NAME")};
    }

    TlsContext create_tls_context(bool server, const TlsConfig& config) {
        ERR_clear_error();
        TlsContext ctx(SSL_CTX_new(server ? TLS_server_method() : TLS_client_method()),
                       SSL_CTX_free);
        if (!ctx || SSL_CTX_set_min_proto_version(ctx.get(), TLS1_3_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(ctx.get(), TLS1_3_VERSION) != 1 ||
            SSL_CTX_set_max_early_data(ctx.get(), 0) != 1) {
            log::error("TLS: could not initialize TLS 1.3");
            return nullptr;
        }
        if (server) {
            // Deployment keys must load unattended; never prompt on process stdin.
            SSL_CTX_set_default_passwd_cb(ctx.get(), [](char*, int, int, void*) { return 0; });
            if (config.certificate_file.empty() || config.private_key_file.empty() ||
                SSL_CTX_use_certificate_chain_file(ctx.get(), config.certificate_file.c_str()) !=
                    1 ||
                SSL_CTX_use_PrivateKey_file(
                    ctx.get(), config.private_key_file.c_str(), SSL_FILETYPE_PEM) != 1 ||
                SSL_CTX_check_private_key(ctx.get()) != 1) {
                log::error("TLS: configure valid OPENKAL_TLS_CERT and OPENKAL_TLS_KEY PEM files");
                return nullptr;
            }
            SSL_CTX_set_num_tickets(ctx.get(), 0);
        } else {
            SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
            const int loaded =
                config.ca_file.empty()
                    ? SSL_CTX_set_default_verify_paths(ctx.get())
                    : SSL_CTX_load_verify_locations(ctx.get(), config.ca_file.c_str(), nullptr);
            if (loaded != 1) {
                log::error("TLS: could not load client trust roots (OPENKAL_TLS_CA)");
                return nullptr;
            }
        }
        return ctx;
    }

    TlsSession::TlsSession(int fd,
                           const TlsContext& context,
                           bool server,
                           const std::string& identity)
        : m_fd(fd), m_ssl(context ? SSL_new(context.get()) : nullptr, SSL_free),
          m_deadline(Clock::now() + std::chrono::seconds(5)) {
        const auto* method = socket_method();
        if (!m_ssl || !method) {
            close();
            return;
        }
        BIO* bio = BIO_new(method);
        if (!bio) {
            close();
            return;
        }
        BIO_set_data(bio, &m_fd);
        BIO_set_init(bio, 1);
        SSL_set_bio(m_ssl.get(), bio, bio);
        if (server) {
            SSL_set_accept_state(m_ssl.get());
        } else {
            SSL_set_connect_state(m_ssl.get());
            in_addr ip{};
            const bool is_ip = ::inet_pton(AF_INET, identity.c_str(), &ip) == 1;
            SSL_set_hostflags(m_ssl.get(), X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
            const int valid =
                is_ip ? X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(m_ssl.get()), identity.c_str())
                      : SSL_set1_host(m_ssl.get(), identity.c_str());
            if (identity.empty() || valid != 1 ||
                (!is_ip && SSL_set_tlsext_host_name(m_ssl.get(), identity.c_str()) != 1)) {
                close();
            }
        }
    }

    TlsSession::~TlsSession() {
        close();
    }
    int TlsSession::fd() const {
        return m_fd;
    }
    bool TlsSession::alive() const {
        return m_fd >= 0;
    }

    void TlsSession::close() {
        m_writeRetryData = nullptr;
        m_writeRetrySize = 0;
        m_ssl.reset();
        if (m_fd >= 0) {
            ::shutdown(m_fd, SHUT_RDWR);
            ::close(m_fd);
            m_fd = -1;
        }
    }

    bool TlsSession::handshake_step() {
        if (!alive()) {
            return false;
        }
        if (Clock::now() >= m_deadline) {
            close();
            return false;
        }
        ERR_clear_error();
        const int result = SSL_do_handshake(m_ssl.get());
        if (result == 1) {
            return true;
        }
        m_want = SSL_get_error(m_ssl.get(), result);
        if (!retryable(m_want)) {
            log::warn("TLS: handshake or certificate verification failed");
            close();
        }
        return false;
    }

    bool TlsSession::handshake_wait() {
        while (alive()) {
            if (handshake_step()) {
                return true;
            }
            if (!alive() || !wait_socket(m_fd, m_want, m_deadline)) {
                close();
            }
        }
        return false;
    }

    bool TlsSession::send_all(const std::uint8_t* data, std::size_t size) {
        if (!alive() || (!data && size != 0) || m_writeRetryData) {
            return false;
        }
        const auto deadline = Clock::now() + std::chrono::seconds(1);
        std::size_t sent = 0;
        while (sent < size) {
            if (Clock::now() >= deadline) {
                close();
                return false;
            }
            std::size_t written = 0;
            ERR_clear_error();
            const int result = SSL_write_ex(m_ssl.get(), data + sent, size - sent, &written);
            if (result == 1) {
                sent += written;
                continue;
            }
            const int error = SSL_get_error(m_ssl.get(), result);
            if (!retryable(error) || !wait_socket(m_fd, error, deadline)) {
                close();
                return false;
            }
        }
        return true;
    }

    SendResult TlsSession::try_send(const std::uint8_t* data, std::size_t size) {
        if (!alive() || (!data && size != 0)) {
            return {};
        }
        if (m_writeRetryData && (data != m_writeRetryData || size != m_writeRetrySize)) {
            close();
            return {};
        }
        if (size == 0) {
            return {SendStatus::Progress, 0};
        }
        std::size_t written = 0;
        ERR_clear_error();
        errno = 0;
        const int result = SSL_write_ex(m_ssl.get(), data, size, &written);
        const int systemError = errno;
        if (result == 1) {
            m_writeRetryData = nullptr;
            m_writeRetrySize = 0;
            if (written > 0 && written <= size) {
                return {SendStatus::Progress, written};
            }
            close();
            return {};
        }
        const int error = SSL_get_error(m_ssl.get(), result);
        const bool interrupted = systemError == EINTR;
        if (retryable(error) ||
            (error == SSL_ERROR_SYSCALL &&
             (interrupted || systemError == EAGAIN || systemError == EWOULDBLOCK))) {
            m_writeRetryData = data;
            m_writeRetrySize = size;
            const auto readiness =
                error == SSL_ERROR_WANT_READ ? SendReadiness::Read : SendReadiness::Write;
            return {interrupted ? SendStatus::Interrupted : SendStatus::WouldBlock, 0, readiness};
        }
        close();
        return {};
    }

    bool TlsSession::recv_into(std::vector<std::uint8_t>& buffer) {
        if (!alive()) {
            return false;
        }
        // SSL_write retries must keep their existing arguments and operation state.
        if (m_writeRetryData) {
            return true;
        }
        std::uint8_t bytes[4096];
        while (buffer.size() < protocol::kMaxReceiveBufferLength) {
            std::size_t count = 0;
            ERR_clear_error();
            const int result = SSL_read_ex(
                m_ssl.get(),
                bytes,
                std::min(sizeof(bytes), protocol::kMaxReceiveBufferLength - buffer.size()),
                &count);
            if (result == 1) {
                buffer.insert(buffer.end(), bytes, bytes + count);
                continue;
            }
            const int error = SSL_get_error(m_ssl.get(), result);
            if (retryable(error)) {
                return true;
            }
            close();
            return false;
        }
        return true;
    }
}  // namespace openkal::server
