/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// One target reaches unchanged internal helpers; the other links the production object.
#ifdef OPENKAL_TLS_PUBLIC_ONLY
#include "../libs/server/src/net/TlsSession.h"
#include "openkal/protocol/Constants.h"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include "../libs/server/src/net/TlsSession.cpp"
#endif
#include "test_util.h"

#include <cstring>
#include <deque>
#include <fcntl.h>
#include <thread>

namespace test = openkal::test;

namespace {
    struct PollStep {
        int result = 1;
        int error = 0;
        short events = POLLOUT;
    };

    struct WriteStep {
        bool success = true;
        std::size_t count = 0;
        int error = SSL_ERROR_NONE;
        bool expire = false;
        int system_error = 0;
    };

    struct ReadStep {
        std::string bytes;
        int error = SSL_ERROR_WANT_READ;
        std::size_t expected_capacity = 4096;
    };

    struct IoStep {
        int count = -1;
        int error = EAGAIN;
    };

    struct Script {
        std::deque<PollStep> polls;
        std::deque<WriteStep> writes;
        std::deque<ReadStep> reads;
        std::deque<IoStep> sends;
        std::deque<IoStep> receives;
        std::vector<std::uint8_t> sent;
        std::vector<std::uint8_t> expected_output;
        int ssl_error = SSL_ERROR_NONE;
        int poll_calls = 0;
        int read_calls = 0;
        std::vector<const void*> write_buffers;
        std::vector<std::size_t> write_sizes;
    };

    Script g_script;
    bool g_mock = false;

    void reset() {
        g_script = {};
        g_mock = true;
    }

    std::unique_ptr<openkal::server::TlsSession> session() {
        openkal::server::TlsContext context(SSL_CTX_new(TLS_server_method()), SSL_CTX_free);
        test::expect(context != nullptr, "create owned in-memory TLS policy");
        const int fd = ::open("/dev/null", O_RDWR);
        test::expect(fd >= 0, "own a harmless descriptor for scripted TLS");
        return std::make_unique<openkal::server::TlsSession>(fd, context, true, "");
    }
}  // namespace

extern "C" {
int __real_poll(pollfd*, nfds_t, int);
int __real_SSL_write_ex(SSL*, const void*, std::size_t, std::size_t*);
int __real_SSL_read_ex(SSL*, void*, std::size_t, std::size_t*);
int __real_SSL_get_error(const SSL*, int);
ssize_t __real_send(int, const void*, std::size_t, int);
ssize_t __real_recv(int, void*, std::size_t, int);

int __wrap_poll(pollfd* fds, nfds_t count, int timeout) {
    if (!g_mock) {
        return __real_poll(fds, count, timeout);
    }
    test::expect(count == 1 && timeout >= 1 && timeout <= 1000,
                 "TLS wait respects its positive remaining deadline");
    ++g_script.poll_calls;
    test::expect(!g_script.polls.empty(), "readiness follows an explicit script");
    if (g_script.polls.empty()) {
        return 0;
    }
    const auto step = g_script.polls.front();
    g_script.polls.pop_front();
    errno = step.error;
    fds[0].revents = step.events;
    return step.result;
}

int __wrap_SSL_write_ex(SSL* ssl, const void* data, std::size_t size, std::size_t* written) {
    if (!g_mock) {
        return __real_SSL_write_ex(ssl, data, size, written);
    }
    test::expect(!g_script.writes.empty(), "TLS writes follow an explicit script");
    if (g_script.writes.empty()) {
        g_script.ssl_error = SSL_ERROR_SSL;
        return 0;
    }
    const auto step = g_script.writes.front();
    g_script.writes.pop_front();
    g_script.write_buffers.push_back(data);
    g_script.write_sizes.push_back(size);
    if (step.expire) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    }
    g_script.ssl_error = step.error;
    errno = step.system_error;
    *written = step.count;
    test::expect(step.count <= size, "scripted TLS progress fits the unsent suffix");
    const bool correct_size = size == g_script.expected_output.size() - g_script.sent.size();
    test::expect(correct_size, "each TLS write requests exactly the remaining suffix length");
    test::expect(
        correct_size && data &&
            std::memcmp(data, g_script.expected_output.data() + g_script.sent.size(), size) == 0,
        "each TLS write starts at the next unsent byte");
    if (!correct_size || !data) {
        g_script.ssl_error = SSL_ERROR_SSL;
        return 0;
    }
    if (step.success && step.count <= size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        g_script.sent.insert(g_script.sent.end(), bytes, bytes + step.count);
    }
    return step.success ? 1 : 0;
}

int __wrap_SSL_read_ex(SSL* ssl, void* data, std::size_t size, std::size_t* count) {
    if (!g_mock) {
        return __real_SSL_read_ex(ssl, data, size, count);
    }
    ++g_script.read_calls;
    test::expect(!g_script.reads.empty(), "TLS reads follow an explicit script");
    if (g_script.reads.empty()) {
        g_script.ssl_error = SSL_ERROR_SSL;
        return 0;
    }
    const auto step = g_script.reads.front();
    g_script.reads.pop_front();
    g_script.ssl_error = step.error;
    test::expect(step.bytes.size() <= size, "read request respects remaining receive capacity");
    test::expect(size == step.expected_capacity,
                 "TLS requests exactly the remaining capped capacity");
    *count = step.bytes.size();
    if (!step.bytes.empty()) {
        std::memcpy(data, step.bytes.data(), step.bytes.size());
        return 1;
    }
    return 0;
}

int __wrap_SSL_get_error(const SSL* ssl, int result) {
    return g_mock ? g_script.ssl_error : __real_SSL_get_error(ssl, result);
}

ssize_t __wrap_send(int fd, const void* data, std::size_t size, int flags) {
    if (!g_mock) {
        return __real_send(fd, data, size, flags);
    }
    test::expect(flags == MSG_NOSIGNAL && !g_script.sends.empty(),
                 "BIO send preserves local SIGPIPE policy and explicit result");
    if (g_script.sends.empty()) {
        return 0;
    }
    const auto step = g_script.sends.front();
    g_script.sends.pop_front();
    errno = step.error;
    return step.count;
}

ssize_t __wrap_recv(int fd, void* data, std::size_t size, int flags) {
    if (!g_mock) {
        return __real_recv(fd, data, size, flags);
    }
    test::expect(flags == 0 && !g_script.receives.empty(), "BIO read follows an explicit result");
    if (g_script.receives.empty()) {
        return 0;
    }
    const auto step = g_script.receives.front();
    g_script.receives.pop_front();
    errno = step.error;
    if (step.count > 0 && static_cast<std::size_t>(step.count) <= size) {
        std::memset(data, 'x', static_cast<std::size_t>(step.count));
    }
    return step.count;
}
}

namespace {
    void one_shot_write_cases() {
        using openkal::server::SendReadiness;
        using openkal::server::SendStatus;
        const std::vector<std::uint8_t> expected{1, 2, 3, 4, 5, 6, 7};
        reset();
        auto peer = session();
        test::expect(peer->try_send(nullptr, 0).status == SendStatus::Progress &&
                         peer->try_send(expected.data(), 0).status == SendStatus::Progress &&
                         peer->try_send(nullptr, 1).status == SendStatus::Failed && peer->alive() &&
                         g_script.write_buffers.empty() && g_script.poll_calls == 0,
                     "one-shot TLS empty and invalid buffers never call SSL or poll");
        g_script.expected_output = expected;
        g_script.writes = {{true, 2},
                           {false, 0, SSL_ERROR_WANT_WRITE},
                           {false, 0, SSL_ERROR_WANT_READ},
                           {false, 0, SSL_ERROR_SYSCALL, false, EINTR},
                           {true, 5}};
        auto result = peer->try_send(expected.data(), expected.size());
        test::expect(result.status == SendStatus::Progress && result.bytes == 2 &&
                         g_script.sent == std::vector<std::uint8_t>({1, 2}),
                     "one-shot TLS reports exactly one partial write attempt");
        const auto* suffix = expected.data() + 2;
        result = peer->try_send(suffix, 5);
        test::expect(result.status == SendStatus::WouldBlock && result.bytes == 0 &&
                         result.wait_for == SendReadiness::Write,
                     "TLS WANT_WRITE requests writable readiness without waiting");
        std::vector<std::uint8_t> received{'p'};
        g_script.reads = {{"abc", SSL_ERROR_NONE}, {"", SSL_ERROR_WANT_READ}};
        test::expect(peer->recv_into(received) && received == std::vector<std::uint8_t>{'p'} &&
                         g_script.read_calls == 0,
                     "pending TLS write retry prevents interleaved SSL reads");
        test::expect(!peer->send_all(suffix, 5) && peer->alive() &&
                         g_script.write_buffers.size() == 2,
                     "blocking send cannot replace a pending one-shot TLS write");
        result = peer->try_send(suffix, 5);
        test::expect(result.status == SendStatus::WouldBlock && result.bytes == 0 &&
                         result.wait_for == SendReadiness::Read,
                     "TLS WANT_READ requests readable readiness for its write retry");
        result = peer->try_send(suffix, 5);
        test::expect(result.status == SendStatus::Interrupted && result.bytes == 0,
                     "TLS interrupted syscall returns after one attempt");
        result = peer->try_send(suffix, 5);
        test::expect(
            result.status == SendStatus::Progress && result.bytes == 5 &&
                g_script.sent == expected && g_script.poll_calls == 0 &&
                g_script.write_buffers ==
                    std::vector<const void*>({expected.data(), suffix, suffix, suffix, suffix}) &&
                g_script.write_sizes == std::vector<std::size_t>({7, 5, 5, 5, 5}),
            "pending TLS retries preserve exact pointer, length and ordered output");
        test::expect(peer->recv_into(received) &&
                         received == std::vector<std::uint8_t>({'p', 'a', 'b', 'c'}) &&
                         g_script.read_calls == 2,
                     "TLS receive resumes after write retry makes progress");
        peer->close();
        test::expect(peer->try_send(expected.data(), expected.size()).status ==
                             SendStatus::Failed &&
                         peer->try_send(nullptr, 0).status == SendStatus::Failed,
                     "one-shot TLS rejects a closed session without SSL access");

        for (auto step : {WriteStep{false, 0, SSL_ERROR_SSL},
                          WriteStep{false, 0, SSL_ERROR_ZERO_RETURN},
                          WriteStep{false, 0, SSL_ERROR_SYSCALL, false, EPIPE},
                          WriteStep{true, 0}}) {
            reset();
            peer = session();
            g_script.expected_output = expected;
            g_script.writes = {step};
            const auto failure = peer->try_send(expected.data(), expected.size());
            test::expect(failure.status == SendStatus::Failed && failure.bytes == 0 &&
                             !peer->alive() && g_script.poll_calls == 0 &&
                             g_script.write_buffers.size() == 1,
                         "fatal or zero-progress one-shot TLS output closes without waiting");
        }
        for (int error : {EAGAIN, EWOULDBLOCK}) {
            reset();
            peer = session();
            g_script.expected_output = expected;
            g_script.writes = {{false, 0, SSL_ERROR_SYSCALL, false, error}, {true, 7}};
            const auto retry = peer->try_send(expected.data(), expected.size());
            test::expect(retry.status == SendStatus::WouldBlock &&
                             retry.wait_for == SendReadiness::Write &&
                             peer->try_send(expected.data(), expected.size()).status ==
                                 SendStatus::Progress &&
                             g_script.poll_calls == 0,
                         "transient TLS syscall returns its exact retry without polling");
        }
        for (bool changePointer : {false, true}) {
            reset();
            peer = session();
            g_script.expected_output = expected;
            g_script.writes = {{false, 0, SSL_ERROR_WANT_WRITE}};
            test::expect(peer->try_send(expected.data(), expected.size()).status ==
                             SendStatus::WouldBlock,
                         "prepare a pending TLS retry");
            const auto copy = expected;
            const auto mismatch = peer->try_send(changePointer ? copy.data() : expected.data(),
                                                 expected.size() - (changePointer ? 0 : 1));
            test::expect(mismatch.status == SendStatus::Failed && !peer->alive() &&
                             g_script.write_buffers.size() == 1 && g_script.poll_calls == 0,
                         "changed retry arguments fail before violating the SSL contract");
        }
    }

    void write_cases() {
        const std::vector<std::uint8_t> expected{1, 2, 3, 4, 5, 6, 7};
        reset();
        auto peer = session();
        test::expect(peer->send_all(nullptr, 0), "empty TLS send accepts null storage");
        test::expect(!peer->send_all(nullptr, 1) && peer->alive(),
                     "invalid null payload is rejected without changing connection state");
        test::expect(peer->send_all(expected.data(), 0), "empty nonnull TLS send succeeds");
        g_script.writes = {{true, 2},
                           {false, 0, SSL_ERROR_WANT_WRITE},
                           {true, 3},
                           {false, 0, SSL_ERROR_WANT_READ},
                           {true, 2}};
        g_script.expected_output = expected;
        g_script.polls = {{-1, EINTR, 0}, {1, 0, POLLOUT}, {1, 0, POLLIN}};
        test::expect(peer->send_all(expected.data(), expected.size()),
                     "TLS retries partial writes and both readiness directions");
        test::expect(g_script.sent == expected && g_script.poll_calls == 3 && peer->alive(),
                     "TLS sends the exact suffix once in FIFO order");
        peer->close();
        test::expect(!peer->send_all(expected.data(), expected.size()), "closed TLS send fails");

        for (int error : {SSL_ERROR_SSL, SSL_ERROR_ZERO_RETURN, SSL_ERROR_SYSCALL}) {
            reset();
            peer = session();
            g_script.writes = {{false, 0, error}};
            g_script.expected_output = expected;
            test::expect(!peer->send_all(expected.data(), expected.size()) && !peer->alive(),
                         "fatal TLS write error closes the peer");
            test::expect(g_script.poll_calls == 0, "fatal TLS error never waits for readiness");
        }
        for (auto poll : {PollStep{0, 0, 0}, PollStep{-1, EINVAL, 0}, PollStep{1, 0, POLLHUP}}) {
            reset();
            peer = session();
            g_script.writes = {{false, 0, SSL_ERROR_WANT_WRITE}};
            g_script.expected_output = expected;
            g_script.polls = {poll};
            test::expect(!peer->send_all(expected.data(), expected.size()) && !peer->alive(),
                         "timeout, poll error and irrelevant readiness close pending output");
        }
        reset();
        peer = session();
        g_script.writes = {{true, 1, SSL_ERROR_NONE, true}};
        g_script.expected_output = expected;
        test::expect(!peer->send_all(expected.data(), expected.size()) && !peer->alive(),
                     "TLS absolute deadline closes after partial progress crosses it");
        test::expect(g_script.sent == std::vector<std::uint8_t>{1},
                     "deadline failure never reports or duplicates the unsent suffix");
        reset();
#ifndef OPENKAL_TLS_PUBLIC_ONLY
        test::expect(!openkal::server::wait_socket(
                         -1, SSL_ERROR_WANT_READ, std::chrono::steady_clock::now()),
                     "already expired deadline does not poll");
        test::expect(g_script.poll_calls == 0, "expired deadline has no socket wait");
#endif
    }

    void read_cases() {
        reset();
        auto peer = session();
        std::vector<std::uint8_t> bytes{'p'};
        g_script.reads = {
            {"abc", SSL_ERROR_NONE}, {"de", SSL_ERROR_NONE}, {"", SSL_ERROR_WANT_READ}};
        test::expect(peer->recv_into(bytes) &&
                         bytes == std::vector<std::uint8_t>({'p', 'a', 'b', 'c', 'd', 'e'}),
                     "TLS receive appends exact ordered plaintext and preserves prefix");
        g_script.reads = {{"", SSL_ERROR_WANT_WRITE}};
        test::expect(peer->recv_into(bytes) && peer->alive(),
                     "TLS read WANT_WRITE remains nonblocking");
        bytes.resize(openkal::protocol::kMaxReceiveBufferLength);
        test::expect(peer->recv_into(bytes), "full receive buffer does not read past cap");
        bytes.pop_back();
        g_script.reads = {{"z", SSL_ERROR_NONE, 1}};
        test::expect(peer->recv_into(bytes) && bytes.back() == 'z' && g_script.reads.empty(),
                     "TLS final receive request is clipped to remaining byte capacity");
        for (int error : {SSL_ERROR_SSL, SSL_ERROR_ZERO_RETURN, SSL_ERROR_SYSCALL}) {
            reset();
            peer = session();
            bytes.clear();
            g_script.reads = {{"", error}};
            test::expect(!peer->recv_into(bytes) && !peer->alive() && bytes.empty(),
                         "fatal read and clean TLS close retain no invented data");
            test::expect(!peer->recv_into(bytes), "closed TLS receive fails without SSL access");
        }
    }

#ifndef OPENKAL_TLS_PUBLIC_ONLY
    void bio_cases() {
        reset();
        const int fd = -1;
        std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(openkal::server::socket_method()),
                                                      BIO_free);
        test::expect(bio != nullptr, "create owned custom socket BIO");
        if (!bio) {
            return;
        }
        BIO_set_data(bio.get(), const_cast<int*>(&fd));
        BIO_set_init(bio.get(), 1);
        char data[4]{};
        for (int error : {EAGAIN, EWOULDBLOCK, EINTR, EPIPE}) {
            g_script.sends = {{-1, error}};
            test::expect(openkal::server::socket_write(bio.get(), data, 4) == -1,
                         "BIO preserves syscall write result");
            test::expect(static_cast<bool>(BIO_should_retry(bio.get())) == (error != EPIPE),
                         "BIO write retry flag distinguishes transient and fatal failures");
            g_script.receives = {{-1, error}};
            test::expect(openkal::server::socket_read(bio.get(), data, 4) == -1,
                         "BIO preserves syscall read result");
            test::expect(static_cast<bool>(BIO_should_retry(bio.get())) == (error != EPIPE),
                         "BIO read retry flag distinguishes transient and fatal failures");
        }
        for (int count : {0, 2}) {
            g_script.sends = {{count, 0}};
            g_script.receives = {{count, 0}};
            test::expect(openkal::server::socket_write(bio.get(), data, 4) == count &&
                             !BIO_should_retry(bio.get()),
                         "BIO clears old retry flags on write success or EOF");
            test::expect(openkal::server::socket_read(bio.get(), data, 4) == count &&
                             !BIO_should_retry(bio.get()),
                         "BIO clears old retry flags on read success or EOF");
        }
    }
#endif
}  // namespace

int main() {
    one_shot_write_cases();
    write_cases();
    read_cases();
#ifndef OPENKAL_TLS_PUBLIC_ONLY
    bio_cases();
#endif
    g_mock = false;
    return test::g_failures == 0 ? 0 : 1;
}
