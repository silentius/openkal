/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/HostRuntimeMetrics.h"
#include "test_util.h"

#include <algorithm>
#include <atomic>
#include <thread>

namespace {
    using namespace openkal::server;
    using openkal::test::expect;

    void test_aggregates() {
        HostRuntimeMetrics metrics;
        const auto empty = metrics.snapshot();
        expect(empty.enabled && empty.completed_loops == 0 && empty.scheduled_ticks == 0 &&
                   empty.dispatch_work_ns.empty() && empty.ticks.empty(),
               "a new recorder has enabled empty independent windows");

        HostRuntimeObservation observation;
        observation.dispatch_work_ns = 10;
        observation.scheduled_tick = true;
        observation.tick_duration_ns = 100;
        observation.tick_lateness_ns = 7;
        observation.tick_budget_ns = 100;
        observation.active_connections = 2;
        observation.retiring_connections = 1;
        observation.pending_output_bytes = 100;
        observation.pending_output_connections = 2;
        observation.max_peer_pending_output_bytes = 80;
        metrics.record(observation);
        observation.dispatch_work_ns = 20;
        observation.tick_duration_ns = 101;
        observation.tick_lateness_ns = 9;
        observation.active_connections = 1;
        observation.retiring_connections = 4;
        observation.pending_output_bytes = 20;
        observation.pending_output_connections = 1;
        observation.max_peer_pending_output_bytes = 20;
        metrics.record(observation);
        observation = {};
        observation.dispatch_work_ns = 30;
        observation.tick_duration_ns = 999;
        metrics.record(observation);

        auto snapshot = metrics.snapshot();
        expect(snapshot.completed_loops == 3 && snapshot.scheduled_ticks == 2 &&
                   snapshot.tick_overruns == 1,
               "only scheduled ticks count and an equal budget is not an overrun");
        expect(snapshot.dispatch_work_ns == std::vector<std::uint64_t>({10, 20, 30}) &&
                   snapshot.ticks.size() == 2 && snapshot.ticks[0].duration_ns == 100 &&
                   snapshot.ticks[0].lateness_ns == 7 && snapshot.ticks[1].duration_ns == 101 &&
                   snapshot.ticks[1].lateness_ns == 9,
               "dispatch and tick windows preserve chronological samples separately");
        expect(
            snapshot.active_connections == 0 && snapshot.retiring_connections == 0 &&
                snapshot.peak_active_connections == 2 && snapshot.peak_retiring_connections == 4 &&
                snapshot.peak_connections == 5,
            "connection peaks use actual simultaneous counts rather than summing separate peaks");
        expect(snapshot.observed_pending_output_bytes == 0 &&
                   snapshot.observed_pending_output_connections == 0 &&
                   snapshot.peak_observed_pending_output_bytes == 100 &&
                   snapshot.peak_observed_pending_output_connections == 2 &&
                   snapshot.peak_observed_peer_pending_output_bytes == 80,
               "queue observations retain sampled highs after queues drain");
        snapshot.dispatch_work_ns[0] = 999;
        snapshot.ticks[0].duration_ns = 999;
        const auto unchanged = metrics.snapshot();
        expect(unchanged.dispatch_work_ns.front() == 10 &&
                   unchanged.ticks.front().duration_ns == 100 && empty.dispatch_work_ns.empty(),
               "snapshots own their samples and cannot mutate recorder or earlier snapshots");
    }

    void test_wrap() {
        HostRuntimeMetrics metrics;
        constexpr auto count = HostRuntimeMetrics::kSampleCapacity * 3 + 17;
        for (std::size_t value = 1; value <= count; ++value) {
            HostRuntimeObservation observation;
            observation.dispatch_work_ns = value;
            observation.scheduled_tick = value % 2 == 0;
            observation.tick_duration_ns = value * 10;
            observation.tick_lateness_ns = value;
            observation.tick_budget_ns = count * 10;
            metrics.record(observation);
        }
        const auto snapshot = metrics.snapshot();
        const auto ticks = count / 2;
        expect(snapshot.completed_loops == count && snapshot.scheduled_ticks == ticks &&
                   snapshot.dispatch_work_ns.size() == HostRuntimeMetrics::kSampleCapacity &&
                   snapshot.ticks.size() == HostRuntimeMetrics::kSampleCapacity,
               "both windows stay bounded while lifetime counts survive independent wrap");
        bool ordered = true;
        for (std::size_t index = 0; index < HostRuntimeMetrics::kSampleCapacity; ++index) {
            const auto dispatch_value = count - HostRuntimeMetrics::kSampleCapacity + index + 1;
            const auto tick_value = (ticks - HostRuntimeMetrics::kSampleCapacity + index + 1) * 2;
            ordered = ordered && snapshot.dispatch_work_ns[index] == dispatch_value &&
                      snapshot.ticks[index].duration_ns == tick_value * 10 &&
                      snapshot.ticks[index].lateness_ns == tick_value;
        }
        expect(ordered, "wrapped rings retain their latest samples in oldest-first order");
    }

    void test_concurrent_snapshot() {
        HostRuntimeMetrics metrics;
        std::atomic<bool> ready{false};
        std::atomic<bool> done{false};
        constexpr std::uint64_t count = HostRuntimeMetrics::kSampleCapacity + 257;
        std::thread writer([&] {
            while (!ready.load()) {
                std::this_thread::yield();
            }
            for (std::uint64_t value = 1; value <= count; ++value) {
                HostRuntimeObservation observation;
                observation.dispatch_work_ns = value;
                observation.scheduled_tick = value % 2 == 0;
                observation.tick_duration_ns = value * 10;
                observation.tick_lateness_ns = value;
                observation.tick_budget_ns = count * 10;
                metrics.record(observation);
                if (value % 64 == 0) {
                    std::this_thread::yield();
                }
            }
            done.store(true);
        });
        ready.store(true);
        bool coherent = true;
        std::uint64_t previous = 0;
        for (int attempt = 0; attempt < 128; ++attempt) {
            const auto snapshot = metrics.snapshot();
            coherent = coherent && snapshot.completed_loops >= previous &&
                       snapshot.scheduled_ticks == snapshot.completed_loops / 2 &&
                       snapshot.dispatch_work_ns.size() ==
                           std::min<std::uint64_t>(snapshot.completed_loops,
                                                   HostRuntimeMetrics::kSampleCapacity) &&
                       snapshot.ticks.size() ==
                           std::min<std::uint64_t>(snapshot.scheduled_ticks,
                                                   HostRuntimeMetrics::kSampleCapacity);
            if (!snapshot.dispatch_work_ns.empty()) {
                coherent = coherent && snapshot.dispatch_work_ns.back() == snapshot.completed_loops;
            }
            if (!snapshot.ticks.empty()) {
                coherent = coherent &&
                           snapshot.ticks.back().duration_ns == snapshot.scheduled_ticks * 20 &&
                           snapshot.ticks.back().lateness_ns == snapshot.scheduled_ticks * 2;
            }
            previous = snapshot.completed_loops;
            if (done.load()) {
                break;
            }
        }
        writer.join();
        const auto final = metrics.snapshot();
        expect(coherent && final.completed_loops == count && final.scheduled_ticks == count / 2,
               "concurrent snapshots keep complete loop and tick observations coherent");
    }
}  // namespace

int main() {
    test_aggregates();
    test_wrap();
    test_concurrent_snapshot();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
