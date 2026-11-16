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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Aggregates optional per-frame performance diagnostics for the client. */
    class FrameProfiler {
      public:
        /** Reads OPENKAL_FRAME_STATS and initializes an empty sample window. */
        FrameProfiler();

        /** Reports whether diagnostic collection is enabled.
         * @return True when OPENKAL_FRAME_STATS requests collection.
         */
        [[nodiscard]] bool enabled() const;

        /** Adds world-rendering counters for the current frame.
         * @param instanced_parts Number of parts rendered through instancing.
         * @param instance_draws Number of instanced draw calls.
         * @param monster_instances Number of live monster instances.
         * @param monster_skins Number of monster skin updates.
         */
        void record_world(std::size_t instanced_parts,
                          std::size_t instance_draws,
                          std::size_t monster_instances,
                          std::size_t monster_skins);

        /** Adds timing samples for the current frame.
         * @param frame_ms Complete frame duration.
         * @param busy_ms Renderer busy duration.
         * @param update_ms Simulation and input duration.
         * @param world_ms World rendering duration.
         * @param ui_ms UI rendering duration.
         * @param present_ms Frame presentation duration.
         */
        void record_timing(float frame_ms,
                           float busy_ms,
                           float update_ms,
                           float world_ms,
                           float ui_ms,
                           float present_ms);

        /** Logs and clears a completed one-second sample window when due.
         * @param frame_summary Current frame's diagnostic summary.
         */
        void log_if_due(std::string_view frame_summary);

      private:
        bool m_enabled = false;
        std::uint64_t m_instanced_parts = 0;
        std::uint64_t m_instance_draws = 0;
        std::uint64_t m_instance_samples = 0;
        std::uint64_t m_monster_instances = 0;
        std::uint64_t m_monster_skins = 0;
        std::chrono::steady_clock::time_point m_last_log;
        std::vector<float> m_frame_ms;
        std::vector<float> m_busy_ms;
        std::vector<float> m_update_ms;
        std::vector<float> m_world_ms;
        std::vector<float> m_ui_ms;
        std::vector<float> m_present_ms;
    };

}  // namespace openkal::client
