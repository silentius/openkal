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

#include <algorithm>

namespace openkal::server {

    void HostRuntimeMetrics::record(const HostRuntimeObservation& observation) {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_totals.completed_loops;
        m_dispatch_samples[m_dispatch_next] = observation.dispatch_work_ns;
        m_dispatch_next = (m_dispatch_next + 1) % kSampleCapacity;
        m_dispatch_size = std::min(m_dispatch_size + 1, kSampleCapacity);
        if (observation.scheduled_tick) {
            ++m_totals.scheduled_ticks;
            if (observation.tick_duration_ns > observation.tick_budget_ns) {
                ++m_totals.tick_overruns;
            }
            m_tick_samples[m_tick_next] = {observation.tick_duration_ns,
                                           observation.tick_lateness_ns};
            m_tick_next = (m_tick_next + 1) % kSampleCapacity;
            m_tick_size = std::min(m_tick_size + 1, kSampleCapacity);
        }
        m_totals.active_connections = observation.active_connections;
        m_totals.retiring_connections = observation.retiring_connections;
        m_totals.peak_active_connections =
            std::max(m_totals.peak_active_connections, observation.active_connections);
        m_totals.peak_retiring_connections =
            std::max(m_totals.peak_retiring_connections, observation.retiring_connections);
        m_totals.peak_connections =
            std::max(m_totals.peak_connections,
                     observation.active_connections + observation.retiring_connections);
        m_totals.observed_pending_output_bytes = observation.pending_output_bytes;
        m_totals.observed_pending_output_connections = observation.pending_output_connections;
        m_totals.peak_observed_pending_output_bytes =
            std::max(m_totals.peak_observed_pending_output_bytes, observation.pending_output_bytes);
        m_totals.peak_observed_pending_output_connections =
            std::max(m_totals.peak_observed_pending_output_connections,
                     observation.pending_output_connections);
        m_totals.peak_observed_peer_pending_output_bytes =
            std::max(m_totals.peak_observed_peer_pending_output_bytes,
                     observation.max_peer_pending_output_bytes);
    }

    HostRuntimeMetricsSnapshot HostRuntimeMetrics::snapshot() const {
        HostRuntimeMetricsSnapshot result;
        std::array<std::uint64_t, kSampleCapacity> dispatch_samples;
        std::array<HostRuntimeTickSample, kSampleCapacity> tick_samples;
        std::size_t dispatch_next = 0;
        std::size_t dispatch_size = 0;
        std::size_t tick_next = 0;
        std::size_t tick_size = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            result = m_totals;
            dispatch_samples = m_dispatch_samples;
            tick_samples = m_tick_samples;
            dispatch_next = m_dispatch_next;
            dispatch_size = m_dispatch_size;
            tick_next = m_tick_next;
            tick_size = m_tick_size;
        }
        result.enabled = true;
        result.dispatch_work_ns.reserve(dispatch_size);
        const auto dispatch_first =
            (dispatch_next + kSampleCapacity - dispatch_size) % kSampleCapacity;
        for (std::size_t index = 0; index < dispatch_size; ++index) {
            result.dispatch_work_ns.push_back(
                dispatch_samples[(dispatch_first + index) % kSampleCapacity]);
        }
        result.ticks.reserve(tick_size);
        const auto tick_first = (tick_next + kSampleCapacity - tick_size) % kSampleCapacity;
        for (std::size_t index = 0; index < tick_size; ++index) {
            result.ticks.push_back(tick_samples[(tick_first + index) % kSampleCapacity]);
        }
        return result;
    }

}  // namespace openkal::server
