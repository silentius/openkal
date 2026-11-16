/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/net/Transport.h"

#include "TlsSession.h"
#include "openkal/platform/Log.h"
#include "openkal/protocol/Constants.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace openkal::server {
    namespace {

        bool set_nonblock(int fd) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags < 0) {
                return false;
            }
            return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
        }

        class PosixSession final : public ISession {
          public:
            explicit PosixSession(int fd) : m_fd(fd) {}
            ~PosixSession() override {
                close();
            }

            int fd() const override {
                return m_fd;
            }
            bool alive() const override {
                return m_fd >= 0;
            }

            void close() override {
                if (m_fd >= 0) {
                    ::shutdown(m_fd, SHUT_RDWR);
                    ::close(m_fd);
                    m_fd = -1;
                }
            }

            bool send_all(const std::uint8_t* data, std::size_t size) override {
                if (m_fd < 0 || !data) {
                    return false;
                }
                std::size_t sent = 0;
                while (sent < size) {
                    const ssize_t n = ::send(m_fd, data + sent, size - sent, MSG_NOSIGNAL);
                    if (n < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            pollfd p{};
                            p.fd = m_fd;
                            p.events = POLLOUT;
                            if (::poll(&p, 1, 1000) <= 0) {
                                return false;
                            }
                            continue;
                        }
                        return false;
                    }
                    if (n == 0) {
                        return false;
                    }
                    sent += static_cast<std::size_t>(n);
                }
                return true;
            }

            SendResult try_send(const std::uint8_t* data, std::size_t size) override {
                if (m_fd < 0 || (!data && size != 0)) {
                    return {};
                }
                if (size == 0) {
                    return {SendStatus::Progress, 0};
                }
                const ssize_t count = ::send(m_fd, data, size, MSG_NOSIGNAL);
                if (count > 0) {
                    return {SendStatus::Progress, static_cast<std::size_t>(count)};
                }
                if (count < 0) {
                    if (errno == EINTR) {
                        return {SendStatus::Interrupted, 0};
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return {SendStatus::WouldBlock, 0};
                    }
                }
                return {};
            }

            bool recv_into(std::vector<std::uint8_t>& buffer) override {
                if (m_fd < 0) {
                    return false;
                }
                std::uint8_t tmp[4096];
                for (;;) {
                    if (buffer.size() >= protocol::kMaxReceiveBufferLength) {
                        return true;
                    }
                    const std::size_t available = protocol::kMaxReceiveBufferLength - buffer.size();
                    const ssize_t n = ::recv(m_fd, tmp, std::min(sizeof(tmp), available), 0);
                    if (n < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            return true;
                        }
                        return false;
                    }
                    if (n == 0) {
                        return false;
                    }
                    buffer.insert(buffer.end(), tmp, tmp + n);
                }
            }

          private:
            int m_fd = -1;
        };

        class UnixAcceptor final : public IAcceptor {
          public:
            ~UnixAcceptor() override {
                close();
            }

            bool listen(const std::string& endpoint) override {
                close();
                m_path = endpoint;
                ::unlink(m_path.c_str());
                m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (m_fd < 0) {
                    log::error(std::string("unix socket: ") + std::strerror(errno));
                    return false;
                }
                sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                if (m_path.size() >= sizeof(addr.sun_path)) {
                    log::error("unix path too long: " + m_path);
                    close();
                    return false;
                }
                std::memcpy(addr.sun_path, m_path.c_str(), m_path.size() + 1);
                if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                    log::error(std::string("unix bind: ") + std::strerror(errno));
                    close();
                    return false;
                }
                if (::listen(m_fd, 16) != 0) {
                    log::error(std::string("unix listen: ") + std::strerror(errno));
                    close();
                    return false;
                }
                set_nonblock(m_fd);
                return true;
            }

            void close() override {
                if (m_fd >= 0) {
                    ::close(m_fd);
                    m_fd = -1;
                }
                if (!m_path.empty()) {
                    ::unlink(m_path.c_str());
                    m_path.clear();
                }
            }

            int fd() const override {
                return m_fd;
            }

            std::unique_ptr<ISession> try_accept() override {
                if (m_fd < 0) {
                    return nullptr;
                }
                const int cfd = ::accept(m_fd, nullptr, nullptr);
                if (cfd < 0) {
                    return nullptr;
                }
                set_nonblock(cfd);
                return std::make_unique<PosixSession>(cfd);
            }

          private:
            int m_fd = -1;
            std::string m_path;
        };

        class TcpAcceptor final : public IAcceptor {
          public:
            explicit TcpAcceptor(TlsConfig config) : m_config(std::move(config)) {}
            ~TcpAcceptor() override {
                close();
            }

            bool listen(const std::string& endpoint) override {
                close();
                m_context = create_tls_context(true, m_config);
                if (!m_context) {
                    return false;
                }
                // endpoint = host:port
                const auto colon = endpoint.rfind(':');
                if (colon == std::string::npos) {
                    return false;
                }
                const std::string host = endpoint.substr(0, colon);
                const int port = std::atoi(endpoint.substr(colon + 1).c_str());
                m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
                if (m_fd < 0) {
                    return false;
                }
                int yes = 1;
                ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
                sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(static_cast<std::uint16_t>(port));
                if (host.empty() || host == "0.0.0.0") {
                    addr.sin_addr.s_addr = INADDR_ANY;
                } else if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
                    close();
                    return false;
                }
                if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                    log::error(std::string("tcp bind: ") + std::strerror(errno));
                    close();
                    return false;
                }
                if (::listen(m_fd, 64) != 0) {
                    close();
                    return false;
                }
                if (!set_nonblock(m_fd)) {
                    close();
                    return false;
                }
                return true;
            }

            void close() override {
                m_pending.clear();
                m_context.reset();
                if (m_fd >= 0) {
                    ::close(m_fd);
                    m_fd = -1;
                }
            }

            int fd() const override {
                return m_fd;
            }

            std::unique_ptr<ISession> try_accept() override {
                if (m_fd < 0) {
                    return nullptr;
                }
                // Incomplete TLS handshakes must not stall the game loop.
                for (auto it = m_pending.begin(); it != m_pending.end();) {
                    if ((*it)->handshake_step()) {
                        auto session = std::move(*it);
                        m_pending.erase(it);
                        return session;
                    }
                    if (!(*it)->alive()) {
                        it = m_pending.erase(it);
                    } else {
                        ++it;
                    }
                }
                const int cfd = ::accept(m_fd, nullptr, nullptr);
                if (cfd < 0) {
                    return nullptr;
                }
                if (m_pending.size() >= 64 || !set_nonblock(cfd)) {
                    ::close(cfd);
                    return nullptr;
                }
                int yes = 1;
                ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
                auto session = std::make_unique<TlsSession>(cfd, m_context, true, std::string{});
                if (session->handshake_step()) {
                    return session;
                }
                if (session->alive()) {
                    m_pending.push_back(std::move(session));
                }
                return nullptr;
            }

          private:
            int m_fd = -1;
            TlsConfig m_config;
            TlsContext m_context;
            std::vector<std::unique_ptr<TlsSession>> m_pending;
        };

    }  // namespace

    std::unique_ptr<IAcceptor> Transport::create_unix_acceptor() {
        return std::make_unique<UnixAcceptor>();
    }

    std::unique_ptr<IAcceptor> Transport::create_tcp_acceptor(TlsConfig tls) {
        return std::make_unique<TcpAcceptor>(std::move(tls));
    }

    std::unique_ptr<ISession> Transport::connect_unix(const std::string& path) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return nullptr;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
            ::close(fd);
            return nullptr;
        }
        std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            log::warn(std::string("unix connect failed: ") + std::strerror(errno));
            ::close(fd);
            return nullptr;
        }
        set_nonblock(fd);
        return std::make_unique<PosixSession>(fd);
    }

    std::unique_ptr<ISession> Transport::connect_tcp(const std::string& host,
                                                     std::uint16_t port,
                                                     TlsConfig tls,
                                                     int receive_buffer) {
        if (receive_buffer < 0) {
            return nullptr;
        }
        const auto context = create_tls_context(false, tls);
        if (!context) {
            return nullptr;
        }
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return nullptr;
        }
        if (receive_buffer > 0 &&
            ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &receive_buffer, sizeof(receive_buffer)) != 0) {
            ::close(fd);
            return nullptr;
        }
        if (!set_nonblock(fd)) {
            ::close(fd);
            return nullptr;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            ::close(fd);
            return nullptr;
        }
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            if (errno != EINPROGRESS) {
                ::close(fd);
                return nullptr;
            }
            pollfd pending{fd, POLLOUT, 0};
            int error = 0;
            socklen_t length = sizeof(error);
            if (::poll(&pending, 1, 5000) <= 0 ||
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
                ::close(fd);
                return nullptr;
            }
        }
        int yes = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        auto session = std::make_unique<TlsSession>(
            fd, context, false, tls.server_name.empty() ? host : tls.server_name);
        if (!session->handshake_wait()) {
            return nullptr;
        }
        return session;
    }

    std::unique_ptr<IAcceptor> create_unix_acceptor() {
        return Transport::create_unix_acceptor();
    }

    std::unique_ptr<IAcceptor> create_tcp_acceptor(TlsConfig tls) {
        return Transport::create_tcp_acceptor(std::move(tls));
    }

    std::unique_ptr<ISession> connect_unix(const std::string& path) {
        return Transport::connect_unix(path);
    }

    std::unique_ptr<ISession>
    connect_tcp(const std::string& host, std::uint16_t port, TlsConfig tls, int receive_buffer) {
        return Transport::connect_tcp(host, port, std::move(tls), receive_buffer);
    }

}  // namespace openkal::server
