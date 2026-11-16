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

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace openkal::server {

    /** Duration and scheduling delay of one completed scheduled world tick. */
    struct HostRuntimeTickSample {
        std::uint64_t duration_ns = 0;
        std::uint64_t lateness_ns = 0;
    };

    /** Measurements collected at the end of one completed host loop. */
    struct HostRuntimeObservation {
        /** Work before and after poll, excluding its wait and metrics collection/publication. */
        std::uint64_t dispatch_work_ns = 0;
        bool scheduled_tick = false;
        /** Entire scheduled tick block, including any world-lock wait. */
        std::uint64_t tick_duration_ns = 0;
        /** Existing elapsed scheduling time minus the configured tick interval. */
        std::uint64_t tick_lateness_ns = 0;
        std::uint64_t tick_budget_ns = 0;
        std::size_t active_connections = 0;
        std::size_t retiring_connections = 0;
        std::size_t pending_output_bytes = 0;
        std::size_t pending_output_connections = 0;
        std::size_t max_peer_pending_output_bytes = 0;
    };

    /** Bounded chronological samples and lifetime aggregates, copied without the world lock. */
    struct HostRuntimeMetricsSnapshot {
        bool enabled = false;
        /** Lifetime loops reaching their final observation; startup and shutdown are excluded. */
        std::uint64_t completed_loops = 0;
        std::uint64_t scheduled_ticks = 0;
        /** Ticks whose measured work exceeded their configured interval. */
        std::uint64_t tick_overruns = 0;
        /** Active client-list entries (including handshakes) at the latest completed loop. */
        std::size_t active_connections = 0;
        std::size_t retiring_connections = 0;
        std::size_t peak_active_connections = 0;
        std::size_t peak_retiring_connections = 0;
        std::size_t peak_connections = 0;
        /** Queue observations at completed loop boundaries, not exact enqueue peaks. */
        std::size_t observed_pending_output_bytes = 0;
        std::size_t observed_pending_output_connections = 0;
        std::size_t peak_observed_pending_output_bytes = 0;
        std::size_t peak_observed_pending_output_connections = 0;
        std::size_t peak_observed_peer_pending_output_bytes = 0;
        /** Retained dispatch samples; size is the retained count, oldest first. */
        std::vector<std::uint64_t> dispatch_work_ns;
        /** Independently retained tick samples; size is the retained count, oldest first. */
        std::vector<HostRuntimeTickSample> ticks;
    };

    /** Records bounded host-loop measurements without allocating on the recording path. */
    class HostRuntimeMetrics final {
      public:
        static constexpr std::size_t kSampleCapacity = 4096;

        /** Records one completed loop and its optional scheduled tick.
         * @param observation Completed-loop measurements and connection observations.
         */
        void record(const HostRuntimeObservation& observation);

        /** Copies chronological samples and aggregate counters under a private mutex.
         * Result vectors are allocated after releasing the recorder mutex.
         * @return Independent snapshot with at most kSampleCapacity samples in each ring.
         */
        [[nodiscard]] HostRuntimeMetricsSnapshot snapshot() const;

      private:
        mutable std::mutex m_mutex;
        /** Aggregate fields only; its sample vectors remain empty. */
        HostRuntimeMetricsSnapshot m_totals;
        std::array<std::uint64_t, kSampleCapacity> m_dispatch_samples{};
        std::array<HostRuntimeTickSample, kSampleCapacity> m_tick_samples{};
        std::size_t m_dispatch_next = 0;
        std::size_t m_dispatch_size = 0;
        std::size_t m_tick_next = 0;
        std::size_t m_tick_size = 0;
    };

}  // namespace openkal::server
