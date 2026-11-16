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
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <string_view>

namespace {
    using namespace openkal::server;
    using openkal::test::expect;
    using Clock = HostConnection::Clock;

    struct Request {
        std::uintptr_t address;
        std::size_t size;
    };

    struct State {
        bool open = true;
        bool stalled = false;
        int blocking_calls = 0;
        int receive_calls = 0;
        std::deque<SendResult> results;
        std::vector<Request> requests;
        std::vector<std::uint8_t> wire;
    };

    class ScriptedSession final : public ISession {
      public:
        explicit ScriptedSession(std::shared_ptr<State> state) : m_state(std::move(state)) {}
        int fd() const override {
            return m_state->open ? 42 : -1;
        }
        bool alive() const override {
            return m_state->open;
        }
        void close() override {
            m_state->open = false;
        }
        bool send_all(const std::uint8_t*, std::size_t) override {
            ++m_state->blocking_calls;
            return false;
        }
        SendResult try_send(const std::uint8_t* data, std::size_t size) override {
            m_state->requests.push_back({reinterpret_cast<std::uintptr_t>(data), size});
            auto result = m_state->stalled
                              ? SendResult{SendStatus::WouldBlock, 0, SendReadiness::Write}
                              : SendResult{SendStatus::Progress, size, SendReadiness::Write};
            if (!m_state->results.empty()) {
                result = m_state->results.front();
                m_state->results.pop_front();
            }
            if (result.status == SendStatus::Progress && result.bytes <= size) {
                m_state->wire.insert(m_state->wire.end(), data, data + result.bytes);
            }
            return result;
        }
        bool recv_into(std::vector<std::uint8_t>& buffer) override {
            ++m_state->receive_calls;
            buffer.push_back(0xAC);
            return m_state->open;
        }

      private:
        std::shared_ptr<State> m_state;
    };

    struct Fixture {
        std::shared_ptr<State> state = std::make_shared<State>();
        HostConnection connection{std::make_unique<ScriptedSession>(state)};
    };

    void ordering_and_readiness(Clock::time_point now) {
        Fixture f;
        expect(f.connection.enqueue({}, now) && !f.connection.pending(),
               "empty output does not occupy queue capacity or invoke the transport");
        expect(f.connection.enqueue({1, 2, 3}, now) && f.connection.enqueue({4, 5}, now),
               "two packets are accepted without claiming transmission");
        expect(f.state->wire.empty() && f.connection.pending_bytes() == 5,
               "enqueue is explicit acceptance and never performs a synchronous write");
        f.state->results = {{SendStatus::Progress, 1}, {SendStatus::Interrupted, 0}};
        expect(f.connection.flush(0, now) && f.connection.pending_bytes() == 4 &&
                   f.state->requests.size() == 2 && f.state->wire == std::vector<std::uint8_t>{1},
               "partial progress and interruption retain the exact unsent FIFO suffix");
        expect(f.connection.flush(POLLIN, now) && f.state->requests.size() == 2,
               "a write retry does not run on unrelated read readiness");
        expect(f.connection.flush(POLLOUT, now) && !f.connection.pending() &&
                   f.state->wire == std::vector<std::uint8_t>({1, 2, 3, 4, 5}) &&
                   f.state->blocking_calls == 0,
               "write readiness finishes every byte once without send_all");

        Fixture tls;
        tls.connection.enqueue({10, 11, 12, 13}, now);
        tls.state->results = {{SendStatus::WouldBlock, 0, SendReadiness::Read}};
        tls.connection.flush(0, now, 2);
        tls.connection.enqueue({14}, now);
        const auto first = tls.state->requests.front();
        expect(tls.connection.wanted_events() == POLLIN && tls.connection.flush(POLLOUT, now) &&
                   tls.state->requests.size() == 1,
               "WANT_READ exposes read interest and ignores write-only readiness");
        tls.connection.flush(POLLIN, now, 1);
        expect(tls.state->requests.size() == 1,
               "a smaller later budget defers a pinned TLS request instead of changing its size");
        expect(tls.connection.flush(POLLIN, now) && !tls.connection.pending() &&
                   tls.state->requests[1].address == first.address &&
                   tls.state->requests[1].size == first.size &&
                   tls.state->wire == std::vector<std::uint8_t>({10, 11, 12, 13, 14}),
               "retry keeps its pointer and length stable across later enqueues and budgets");
    }

    void budgets_and_failure(Clock::time_point now) {
        Fixture f;
        const std::vector<std::uint8_t> expected(200, 0x5A);
        f.connection.enqueue(expected, now);
        f.connection.flush(0, now, 17);
        expect(f.connection.pending_bytes() == 183 && f.state->wire.size() == 17,
               "one flush cannot transmit more than its byte budget");
        f.state->results = {
            {SendStatus::Progress, 1}, {SendStatus::Progress, 1}, {SendStatus::Progress, 1}};
        const auto calls = f.state->requests.size();
        f.connection.flush(0, now, 100, 3);
        expect(f.state->requests.size() == calls + 3 && f.connection.pending_bytes() == 180,
               "partial successes cannot exceed the flush attempt budget");
        f.connection.flush(0, now, 0, 64);
        f.connection.flush(0, now, 64, 0);
        expect(f.state->requests.size() == calls + 3, "zero budgets perform no transport call");
        f.connection.flush(0, now);
        expect(f.state->wire == expected, "budget boundaries preserve the complete byte stream");

        for (const auto result : {SendResult{SendStatus::Failed, 0},
                                  SendResult{SendStatus::Progress, 0},
                                  SendResult{SendStatus::Progress, 99},
                                  SendResult{SendStatus::WouldBlock, 1}}) {
            Fixture bad;
            bad.connection.enqueue({1, 2}, now);
            bad.state->results.push_back(result);
            expect(
                !bad.connection.flush(0, now) && !bad.connection.alive() &&
                    !bad.connection.pending() && bad.connection.pending_bytes() == 0,
                "fatal, zero, oversized or inconsistent progress closes and frees queued output");
        }
        Fixture overdue;
        overdue.connection.enqueue({1}, now);
        overdue.state->stalled = true;
        overdue.connection.flush(0, now);
        expect(
            overdue.connection.flush(POLLIN, now + std::chrono::milliseconds(999)) &&
                overdue.state->requests.size() == 1,
            "unready output remains pending before its absolute deadline without repeated writes");
        expect(!overdue.connection.flush(0, now + std::chrono::seconds(1)) &&
                   !overdue.connection.pending(),
               "the exact one-second deadline closes even without readiness");
        Fixture poll_error;
        poll_error.connection.enqueue({1}, now);
        expect(!poll_error.connection.flush(POLLERR, now) && poll_error.state->requests.empty(),
               "a poll error closes without attempting another write");
    }

    void capacity_and_close(Clock::time_point now) {
        constexpr std::size_t limit = 1024 * 1024;
        Fixture bytes;
        expect(bytes.connection.enqueue(std::vector<std::uint8_t>(limit), now) &&
                   bytes.connection.pending_bytes() == limit,
               "the byte limit itself is accepted");
        expect(!bytes.connection.enqueue({1}, now) && !bytes.connection.pending(),
               "one byte beyond the retained capacity closes the peer and releases output");
        Fixture sparse;
        std::vector<std::uint8_t> large_capacity;
        large_capacity.reserve(limit + 1);
        large_capacity.push_back(1);
        expect(!sparse.connection.enqueue(std::move(large_capacity), now),
               "a one-byte vector with oversized capacity cannot bypass the physical memory bound");
        Fixture partial;
        partial.connection.enqueue(std::vector<std::uint8_t>(limit), now);
        partial.state->results.push_back({SendStatus::Progress, limit - 1});
        partial.connection.flush(0, now, limit, 1);
        expect(partial.connection.pending_bytes() == 1 && !partial.connection.enqueue({1}, now),
               "partly transmitted buffers remain fully charged until their storage is freed");
        Fixture chunks;
        bool accepted = true;
        for (int i = 0; i < 4096; ++i) {
            accepted = chunks.connection.enqueue({1}, now) && accepted;
        }
        expect(accepted && !chunks.connection.enqueue({1}, now) && !chunks.connection.pending(),
               "a separate chunk limit bounds metadata for many tiny packets");

        Fixture closing;
        closing.connection.enqueue({7, 8, 9}, now);
        closing.connection.close_after_flush();
        expect(!closing.connection.alive() && closing.connection.pending() &&
                   !closing.connection.enqueue({10}, now) &&
                   closing.connection.pending_bytes() == 3,
               "graceful close rejects new work without dropping an already accepted final reply");
        expect(!closing.connection.flush(0, now) && !closing.connection.pending() &&
                   closing.state->wire == std::vector<std::uint8_t>({7, 8, 9}) &&
                   !closing.state->open,
               "graceful close delivers queued bytes before closing the descriptor");
        Fixture reads;
        reads.connection.enqueue({1}, now);
        std::vector<std::uint8_t> input;
        expect(reads.connection.recv_into(input) && reads.state->receive_calls == 0,
               "pending output prevents an EOF from discarding earlier accepted replies");
        reads.connection.flush(0, now);
        expect(reads.connection.recv_into(input) && input == std::vector<std::uint8_t>{0xAC},
               "input resumes after output drains");
        HostConnection absent(nullptr);
        expect(!absent.alive() && absent.fd() == -1 && !absent.enqueue({1}, now) &&
                   !absent.flush(0, now) && !absent.recv_into(input) && absent.wanted_events() == 0,
               "a missing transport is closed without dereference");

        const auto retired = std::make_shared<State>();
        {
            HostConnection scoped(std::make_unique<ScriptedSession>(retired));
            scoped.enqueue({7, 8}, now);
            retired->stalled = true;
            scoped.flush(0, now);
            expect(scoped.pending() && retired->open, "a pending retry keeps its transport open");
        }
        expect(!retired->open,
               "destruction explicitly closes a pending transport before releasing its borrowed "
               "buffers");
    }

    void independent_peers(Clock::time_point now) {
        Fixture slow, healthy;
        slow.state->stalled = true;
        slow.connection.enqueue({1, 2, 3}, now);
        std::vector<std::uint8_t> expected;
        for (int tick = 0; tick < 128; ++tick) {
            const auto time = now + std::chrono::milliseconds(2 * tick);
            slow.connection.flush(POLLIN, time);
            const auto marker = static_cast<std::uint8_t>(tick);
            healthy.connection.enqueue({marker}, time);
            healthy.connection.flush(POLLOUT, time);
            expected.push_back(marker);
        }
        expect(slow.state->requests.size() == 1 && slow.connection.pending_bytes() == 3 &&
                   healthy.state->wire == expected && !healthy.connection.pending(),
               "an unready peer uses one initial attempt while another completes 128 independent "
               "ticks");
        expect(slow.state->blocking_calls == 0 && healthy.state->blocking_calls == 0,
               "no peer output path can invoke the synchronous send_all contract");
    }

    void delivery_receipts(Clock::time_point now) {
        Fixture ordered;
        const auto first = ordered.connection.enqueue_tracked({1, 2, 3}, now);
        const auto second = ordered.connection.enqueue_tracked({4, 5}, now);
        ordered.connection.enqueue({6}, now);
        expect(first->status() == OutputStatus::Pending &&
                   second->status() == OutputStatus::Pending && ordered.state->wire.empty(),
               "tracked acceptance remains pending until complete transmission");
        ordered.state->results = {{SendStatus::Progress, 1}, {SendStatus::WouldBlock, 0}};
        ordered.connection.flush(0, now);
        expect(first->status() == OutputStatus::Pending &&
                   second->status() == OutputStatus::Pending &&
                   ordered.state->wire == std::vector<std::uint8_t>{1},
               "partial progress and a blocked suffix cannot confirm delivery");
        ordered.connection.flush(POLLOUT, now, 2);
        expect(first->status() == OutputStatus::Sent && second->status() == OutputStatus::Pending &&
                   ordered.state->wire == std::vector<std::uint8_t>({1, 2, 3}),
               "only the fully transmitted tracked packet becomes sent");
        ordered.connection.flush(POLLOUT, now);
        ordered.connection.close();
        expect(first->status() == OutputStatus::Sent && second->status() == OutputStatus::Sent &&
                   ordered.state->wire == std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6}),
               "completed receipts survive later closure and ordinary queued packets");
        expect(ordered.connection.enqueue_tracked({7}, now)->status() == OutputStatus::Failed &&
                   ordered.connection.enqueue_tracked({}, now)->status() == OutputStatus::Failed,
               "tracked packets rejected by a closed connection fail immediately");

        Fixture empty;
        expect(empty.connection.enqueue_tracked({}, now)->status() == OutputStatus::Sent &&
                   !empty.connection.pending() && empty.state->requests.empty(),
               "an accepted empty tracked packet completes without a transport attempt");
        Fixture graceful;
        const auto final = graceful.connection.enqueue_tracked({8, 9}, now);
        graceful.connection.close_after_flush();
        expect(final->status() == OutputStatus::Pending &&
                   graceful.connection.enqueue_tracked({10}, now)->status() == OutputStatus::Failed,
               "graceful closure retains accepted receipts while rejecting later output");
        graceful.connection.flush(0, now);
        expect(final->status() == OutputStatus::Sent && !graceful.state->open,
               "a final reply is confirmed only after its bytes precede graceful closure");

        for (int failure = 0; failure < 4; ++failure) {
            Fixture failed;
            const auto pending = failed.connection.enqueue_tracked({1, 2}, now);
            const auto following = failed.connection.enqueue_tracked({3}, now);
            if (failure == 0) {
                failed.connection.close();
            } else if (failure == 1) {
                failed.connection.flush(0, now + std::chrono::seconds(1));
            } else if (failure == 2) {
                failed.state->results = {{SendStatus::Failed, 0}};
                failed.connection.flush(0, now);
            } else {
                failed.connection.flush(POLLERR, now);
            }
            expect(pending->status() == OutputStatus::Failed &&
                       following->status() == OutputStatus::Failed && !failed.connection.pending(),
                   "closure, deadline, transport and poll failure resolve every pending receipt");
        }
        Fixture overflow;
        const auto retained = overflow.connection.enqueue_tracked({1}, now);
        const auto rejected =
            overflow.connection.enqueue_tracked(std::vector<std::uint8_t>(1024 * 1024), now);
        expect(retained->status() == OutputStatus::Failed &&
                   rejected->status() == OutputStatus::Failed && !overflow.connection.pending(),
               "overflow fails both the rejected receipt and earlier unsent output");
        std::shared_ptr<const OutputReceipt> abandoned;
        {
            Fixture scoped;
            abandoned = scoped.connection.enqueue_tracked({1}, now);
            scoped.state->stalled = true;
            scoped.connection.flush(0, now);
        }
        expect(abandoned->status() == OutputStatus::Failed,
               "a receipt outliving its connection fails on destruction without callbacks");
    }

    void benchmark(Clock::time_point now) {
        constexpr int sweeps = 128, frames_per_sweep = 32;
        constexpr std::size_t frame_bytes = 16;
        std::cout << "# Synthetic queue-flush CPU/mock overhead; not world-tick latency or socket "
                     "throughput.\n"
                  << "peers,sweeps,frames_per_peer_sweep,median_us,p95_us,p99_us,"
                     "peak_stalled_pending_bytes,peak_healthy_pending_bytes,stalled_attempts,"
                     "healthy_attempts,healthy_wire_bytes\n";
        for (int peer_count : {1, 16, 64}) {
            std::vector<std::unique_ptr<Fixture>> peers;
            for (int i = 0; i < peer_count; ++i) {
                peers.push_back(std::make_unique<Fixture>());
            }
            peers.front()->state->stalled = true;
            std::vector<double> samples;
            samples.reserve(sweeps);
            std::size_t peak_stalled = 0, peak_healthy = 0;
            bool accepted = true, open = true;
            for (int sweep = 0; sweep < sweeps; ++sweep) {
                // Simulated queue time stays below the established one-second deadline;
                // the real steady clock measures only the flush sweep's CPU cost.
                const auto simulated = now + std::chrono::milliseconds(2 * sweep);
                for (int index = 0; index < peer_count; ++index) {
                    auto& peer = *peers[static_cast<std::size_t>(index)];
                    for (int frame = 0; frame < frames_per_sweep; ++frame) {
                        accepted = peer.connection.enqueue(
                                       std::vector<std::uint8_t>(frame_bytes, 0x5A), simulated) &&
                                   accepted;
                    }
                    auto& peak = index == 0 ? peak_stalled : peak_healthy;
                    peak = std::max(peak, peer.connection.pending_bytes());
                }
                const auto start = Clock::now();
                for (int index = 0; index < peer_count; ++index) {
                    auto& peer = *peers[static_cast<std::size_t>(index)];
                    open = peer.connection.flush(index == 0 ? POLLIN : POLLOUT, simulated) && open;
                }
                samples.push_back(
                    std::chrono::duration<double, std::micro>(Clock::now() - start).count());
            }
            std::sort(samples.begin(), samples.end());
            const auto percentile = [&](std::size_t percentage) {
                return samples[(samples.size() * percentage + 99) / 100 - 1];
            };
            std::size_t healthy_attempts = 0, healthy_wire = 0;
            bool drained = true, synchronous = false;
            for (int index = 0; index < peer_count; ++index) {
                const auto& peer = *peers[static_cast<std::size_t>(index)];
                synchronous = synchronous || peer.state->blocking_calls != 0;
                if (index == 0) {
                    continue;
                }
                healthy_attempts += peer.state->requests.size();
                healthy_wire += peer.state->wire.size();
                drained = drained && !peer.connection.pending() &&
                          peer.state->requests.size() == sweeps * frames_per_sweep &&
                          peer.state->wire.size() == sweeps * frames_per_sweep * frame_bytes &&
                          std::all_of(peer.state->wire.begin(),
                                      peer.state->wire.end(),
                                      [](auto byte) { return byte == 0x5A; });
            }
            const auto& stalled = *peers.front();
            expect(accepted && open && drained && !synchronous &&
                       stalled.state->requests.size() == 1 && stalled.state->wire.empty() &&
                       peak_stalled == sweeps * frames_per_sweep * frame_bytes &&
                       peak_healthy == (peer_count == 1 ? 0 : frames_per_sweep * frame_bytes),
                   "benchmark keeps one peer bounded and unready while every healthy peer drains");
            std::cout << peer_count << ',' << sweeps << ',' << frames_per_sweep << ',' << std::fixed
                      << std::setprecision(3) << percentile(50) << ',' << percentile(95) << ','
                      << percentile(99) << ',' << peak_stalled << ',' << peak_healthy << ','
                      << stalled.state->requests.size() << ',' << healthy_attempts << ','
                      << healthy_wire << '\n';
        }
    }
}  // namespace

int main(int argc, char** argv) {
    const auto now = Clock::time_point{} + std::chrono::seconds(100);
    ordering_and_readiness(now);
    budgets_and_failure(now);
    capacity_and_close(now);
    independent_peers(now);
    delivery_receipts(now);
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark(now);
    }
    return openkal::test::g_failures ? 1 : 0;
}
