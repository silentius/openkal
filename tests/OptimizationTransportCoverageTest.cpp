/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Linker wrappers script only a reserved fake descriptor; real socket calls pass through.
#include "../libs/server/src/net/Transport.cpp"
#include "test_util.h"

#include <deque>

namespace {
    int scripted_fd = -2;
    struct Result {
        ssize_t bytes;
        int error = 0;
    };
    struct Script {
        std::deque<Result> sends;
        std::deque<Result> receives;
        std::deque<int> polls;
        std::vector<std::uint8_t> delivered;
        std::vector<std::size_t> receive_limits;
        int send_calls = 0;
        int poll_calls = 0;
        int timeout = 0;
        bool flags_ok = true;
        bool exhausted = false;
    };
    Script script;

    class UnsupportedSession final : public openkal::server::ISession {
      public:
        int blocking_calls = 0;
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            return true;
        }
        void close() override {}
        bool send_all(const std::uint8_t*, std::size_t) override {
            ++blocking_calls;
            return true;
        }
        bool recv_into(std::vector<std::uint8_t>&) override {
            return true;
        }
    };

    Result take(std::deque<Result>& values) {
        if (values.empty()) {
            script.exhausted = true;
            return {-1, EIO};
        }
        const auto next = values.front();
        values.pop_front();
        errno = next.error;
        return next;
    }
}  // namespace

extern "C" {
ssize_t __real_send(int fd, const void* data, size_t size, int flags);
ssize_t __real_recv(int fd, void* data, size_t size, int flags);
int __real_poll(struct pollfd* fds, nfds_t count, int timeout);

ssize_t __wrap_send(int fd, const void* data, size_t size, int flags) {
    if (fd != scripted_fd) {
        return __real_send(fd, data, size, flags);
    }
    ++script.send_calls;
    script.flags_ok = script.flags_ok && flags == MSG_NOSIGNAL;
    const auto result = take(script.sends);
    if (result.bytes > 0) {
        const auto count = std::min(size, static_cast<std::size_t>(result.bytes));
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        script.delivered.insert(script.delivered.end(), bytes, bytes + count);
        return static_cast<ssize_t>(count);
    }
    return result.bytes;
}

ssize_t __wrap_recv(int fd, void* data, size_t size, int flags) {
    if (fd != scripted_fd) {
        return __real_recv(fd, data, size, flags);
    }
    script.receive_limits.push_back(size);
    const auto result = take(script.receives);
    if (result.bytes > 0) {
        const auto count = std::min(size, static_cast<std::size_t>(result.bytes));
        std::memset(data, 0xA5, count);
        return static_cast<ssize_t>(count);
    }
    return result.bytes;
}

int __wrap_poll(struct pollfd* fds, nfds_t count, int timeout) {
    if (count != 1 || fds[0].fd != scripted_fd) {
        return __real_poll(fds, count, timeout);
    }
    ++script.poll_calls;
    script.timeout = timeout;
    script.flags_ok = script.flags_ok && fds[0].events == POLLOUT;
    if (script.polls.empty()) {
        script.exhausted = true;
        return -1;
    }
    const int result = script.polls.front();
    script.polls.pop_front();
    return result;
}
}

int main() {
    using openkal::server::PosixSession;
    using openkal::test::expect;
    const std::vector<std::uint8_t> bytes{1, 2, 3, 4, 5, 6, 7};
    scripted_fd = ::open("/dev/null", O_RDWR);
    expect(scripted_fd >= 0, "open an owned descriptor for scripted transport syscalls");
    if (scripted_fd < 0) {
        return 1;
    }
    PosixSession session(scripted_fd);
    const auto write_case = [&](std::deque<Result> sends,
                                std::deque<int> polls,
                                bool expected,
                                std::size_t delivered) {
        script = {};
        script.sends = std::move(sends);
        script.polls = std::move(polls);
        const bool result = session.send_all(bytes.data(), bytes.size());
        expect(result == expected && !script.exhausted && script.sends.empty() &&
                   script.polls.empty() && script.flags_ok &&
                   script.delivered ==
                       std::vector<std::uint8_t>(
                           bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(delivered)),
               "scripted send preserves exact bytes, offset, outcome and syscall contract");
    };
    write_case({{7}}, {}, true, 7);
    write_case({{1}, {2}, {4}}, {}, true, 7);
    write_case({{-1, EINTR}, {2}, {-1, EINTR}, {5}}, {}, true, 7);
    write_case({{2}, {-1, EAGAIN}, {1}, {-1, EWOULDBLOCK}, {4}}, {1, 1}, true, 7);
    expect(script.poll_calls == 2 && script.timeout == 1000,
           "characterize current per-EAGAIN one-second synchronous wait policy");
    write_case({{2}, {-1, EAGAIN}}, {0}, false, 2);
    write_case({{-1, EAGAIN}}, {-1}, false, 0);
    write_case({{2}, {-1, EPIPE}}, {}, false, 2);
    write_case({{0}}, {}, false, 0);
    script = {};
    expect(session.send_all(bytes.data(), 0) && script.send_calls == 0,
           "zero-length send succeeds without a syscall");
    expect(!session.send_all(nullptr, 0) && script.send_calls == 0,
           "null send input fails before a syscall");
    using openkal::server::SendReadiness;
    using openkal::server::SendStatus;
    UnsupportedSession unsupported;
    expect(unsupported.try_send(bytes.data(), bytes.size()).status == SendStatus::Failed &&
               unsupported.blocking_calls == 0,
           "unsupported one-shot transports never fall back to a blocking send");
    for (const auto step : {Result{2},
                            Result{-1, EINTR},
                            Result{-1, EAGAIN},
                            Result{-1, EWOULDBLOCK},
                            Result{-1, EPIPE},
                            Result{0}}) {
        script = {};
        script.sends = {step};
        const auto result = session.try_send(bytes.data(), bytes.size());
        const auto expected = step.bytes > 0        ? SendStatus::Progress
                              : step.error == EINTR ? SendStatus::Interrupted
                              : step.error == EAGAIN || step.error == EWOULDBLOCK
                                  ? SendStatus::WouldBlock
                                  : SendStatus::Failed;
        expect(result.status == expected &&
                   result.bytes == (step.bytes > 0 ? static_cast<std::size_t>(step.bytes) : 0) &&
                   result.wait_for == SendReadiness::Write && script.send_calls == 1 &&
                   script.poll_calls == 0 && !script.exhausted && script.flags_ok,
               "one-shot POSIX send reports progress or retry without polling or retrying");
        expect(script.delivered ==
                   (step.bytes > 0 ? std::vector<std::uint8_t>{1, 2} : std::vector<std::uint8_t>{}),
               "one-shot POSIX write reports exactly the delivered prefix");
    }
    script = {};
    expect(session.try_send(nullptr, 0).status == SendStatus::Progress &&
               session.try_send(bytes.data(), 0).status == SendStatus::Progress &&
               session.try_send(nullptr, 1).status == SendStatus::Failed &&
               script.send_calls == 0 && script.poll_calls == 0,
           "one-shot empty and invalid buffers never enter a socket call");
    std::vector<std::uint8_t> received{0x12};
    script = {};
    script.receives = {{-1, EINTR}, {3}, {2}, {-1, EAGAIN}};
    expect(session.recv_into(received) &&
               received == std::vector<std::uint8_t>({0x12, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5}) &&
               !script.exhausted,
           "receive appends all chunks, retries EINTR and retains existing prefix");
    script = {};
    script.receives = {{-1, EWOULDBLOCK}};
    expect(session.recv_into(received) && received.size() == 6,
           "receive without available bytes preserves the buffer");
    script = {};
    script.receives = {{-1, ECONNRESET}};
    expect(!session.recv_into(received) && received.size() == 6,
           "receive error fails without discarding previously received bytes");
    script = {};
    script.receives = {{0}};
    expect(!session.recv_into(received), "orderly peer close fails receive");
    received.assign(openkal::protocol::kMaxReceiveBufferLength - 1, 0x12);
    script = {};
    script.receives = {{4096}};
    expect(session.recv_into(received) &&
               received.size() == openkal::protocol::kMaxReceiveBufferLength &&
               script.receive_limits == std::vector<std::size_t>{1} && received.back() == 0xA5,
           "receive is clipped exactly to remaining buffer capacity");
    script = {};
    expect(session.recv_into(received) && script.receive_limits.empty(),
           "full receive buffer performs no socket read");
    PosixSession closed(-1);
    expect(!closed.send_all(bytes.data(), bytes.size()) && !closed.recv_into(received),
           "closed descriptor fails send and receive without syscall attempts");
    expect(closed.try_send(bytes.data(), bytes.size()).status == SendStatus::Failed &&
               closed.try_send(nullptr, 0).status == SendStatus::Failed,
           "one-shot send rejects a closed descriptor even for an empty buffer");
    return openkal::test::g_failures ? 1 : 0;
}
