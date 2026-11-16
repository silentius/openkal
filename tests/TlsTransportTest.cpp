/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientTlsConfig.h"
#include "openkal/server/net/Transport.h"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    volatile std::sig_atomic_t g_stop = 0;
    void stop(int) {
        g_stop = 1;
    }

    int serve(bool local, const std::string& path) {
        auto acceptor = local ? openkal::server::create_unix_acceptor()
                              : openkal::server::create_tcp_acceptor();
        if (!acceptor->listen(local ? path : "127.0.0.1:0")) {
            return 2;
        }
        sockaddr_in address{};
        socklen_t size = sizeof(address);
        if (!local &&
            ::getsockname(acceptor->fd(), reinterpret_cast<sockaddr*>(&address), &size) != 0) {
            return 3;
        }
        std::cout << "READY " << (local ? 0 : ntohs(address.sin_port)) << std::endl;
        std::vector<std::unique_ptr<openkal::server::ISession>> clients;
        while (!g_stop) {
            while (auto session = acceptor->try_accept()) {
                clients.push_back(std::move(session));
            }
            for (auto it = clients.begin(); it != clients.end();) {
                std::vector<std::uint8_t> buffer;
                if (!(*it)->recv_into(buffer) || (!buffer.empty() && !(*it)->send_all(buffer))) {
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 0;
    }

    int client(std::uint16_t port,
               openkal::server::TlsConfig tls = openkal::server::tls_config_from_environment()) {
        auto session = openkal::server::connect_tcp("127.0.0.1", port, std::move(tls));
        if (!session) {
            return 2;
        }
        std::vector<std::uint8_t> expected(100000);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            expected[i] = static_cast<std::uint8_t>(i % 251);
        }
        if (!session->send_all(expected)) {
            return 3;
        }
        std::vector<std::uint8_t> received;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (received.size() < expected.size() && std::chrono::steady_clock::now() < deadline) {
            std::vector<std::uint8_t> chunk;
            if (!session->recv_into(chunk)) {
                return 4;
            }
            received.insert(received.end(), chunk.begin(), chunk.end());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return received == expected ? 0 : 5;
    }
}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGTERM, stop);
    if (argc < 2) {
        return 1;
    }
    const std::string mode = argv[1];
    if (mode == "server") {
        return serve(false, {});
    }
    if (mode == "unix" && argc == 3) {
        return serve(true, argv[2]);
    }
    if (mode == "client" && argc == 3) {
        return client(static_cast<std::uint16_t>(std::stoul(argv[2])));
    }
    if (mode == "client-auto" && argc == 6) {
        return client(
            static_cast<std::uint16_t>(std::stoul(argv[2])),
            openkal::client::resolve_client_tls_config("127.0.0.1", argv[3], argv[4], argv[5]));
    }
    return 1;
}
