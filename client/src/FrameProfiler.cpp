/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/FrameProfiler.h"

#include "openkal/platform/Log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace openkal::client {

    namespace {

        std::pair<float, float> percentiles(std::vector<float>& samples) {
            if (samples.empty()) {
                return {0.0f, 0.0f};
            }
            std::sort(samples.begin(), samples.end());
            const std::size_t p95_index = (samples.size() - 1) * 95 / 100;
            return {samples[p95_index], samples.back()};
        }

    }  // namespace

    FrameProfiler::FrameProfiler() : m_last_log(std::chrono::steady_clock::now()) {
        const char* environment = std::getenv("OPENKAL_FRAME_STATS");
        m_enabled = environment != nullptr && environment[0] != '\0' && environment[0] != '0';
        m_frame_ms.reserve(120);
        m_busy_ms.reserve(120);
        m_update_ms.reserve(120);
        m_world_ms.reserve(120);
        m_ui_ms.reserve(120);
        m_present_ms.reserve(120);
    }

    bool FrameProfiler::enabled() const {
        return m_enabled;
    }

    void FrameProfiler::record_world(std::size_t instanced_parts,
                                     std::size_t instance_draws,
                                     std::size_t monster_instances,
                                     std::size_t monster_skins) {
        if (!m_enabled) {
            return;
        }
        m_instanced_parts += instanced_parts;
        m_instance_draws += instance_draws;
        m_monster_instances += monster_instances;
        m_monster_skins += monster_skins;
        ++m_instance_samples;
    }

    void FrameProfiler::record_timing(float frame_ms,
                                      float busy_ms,
                                      float update_ms,
                                      float world_ms,
                                      float ui_ms,
                                      float present_ms) {
        if (!m_enabled) {
            return;
        }
        m_frame_ms.push_back(frame_ms);
        m_busy_ms.push_back(busy_ms);
        m_update_ms.push_back(update_ms);
        m_world_ms.push_back(world_ms);
        m_ui_ms.push_back(ui_ms);
        m_present_ms.push_back(present_ms);
    }

    void FrameProfiler::log_if_due(std::string_view frame_summary) {
        if (!m_enabled) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - m_last_log < std::chrono::seconds(1)) {
            return;
        }
        m_last_log = now;
        log::info(std::string("frame_stats: ") + std::string(frame_summary));

        const auto [frame_p95, frame_max] = percentiles(m_frame_ms);
        const auto [busy_p95, busy_max] = percentiles(m_busy_ms);
        const auto [update_p95, update_max] = percentiles(m_update_ms);
        const auto [world_p95, world_max] = percentiles(m_world_ms);
        const auto [ui_p95, ui_max] = percentiles(m_ui_ms);
        const auto [present_p95, present_max] = percentiles(m_present_ms);
        const double sample_divisor =
            m_instance_samples > 0 ? static_cast<double>(m_instance_samples) : 1.0;
        const double average_instanced_parts =
            static_cast<double>(m_instanced_parts) / sample_divisor;
        const double average_instance_draws =
            static_cast<double>(m_instance_draws) / sample_divisor;
        const double average_monster_instances =
            static_cast<double>(m_monster_instances) / sample_divisor;
        const double average_monster_skins = static_cast<double>(m_monster_skins) / sample_divisor;
        std::array<char, 512> profile_line{};
        std::snprintf(profile_line.data(),
                      profile_line.size(),
                      "frame_profile: frame p95 %.1fms max %.1fms  busy p95 %.1fms max "
                      "%.1fms  phases p95/max update %.1f/%.1fms world %.1f/%.1fms ui "
                      "%.1f/%.1fms present %.1f/%.1fms  indoor instances %.1f draws %.1f "
                      "saved %.1f  monster skins %.1f/%.1f",
                      static_cast<double>(frame_p95),
                      static_cast<double>(frame_max),
                      static_cast<double>(busy_p95),
                      static_cast<double>(busy_max),
                      static_cast<double>(update_p95),
                      static_cast<double>(update_max),
                      static_cast<double>(world_p95),
                      static_cast<double>(world_max),
                      static_cast<double>(ui_p95),
                      static_cast<double>(ui_max),
                      static_cast<double>(present_p95),
                      static_cast<double>(present_max),
                      average_instanced_parts,
                      average_instance_draws,
                      average_instanced_parts - average_instance_draws,
                      average_monster_skins,
                      average_monster_instances);
        log::info(profile_line.data());

        m_frame_ms.clear();
        m_busy_ms.clear();
        m_update_ms.clear();
        m_world_ms.clear();
        m_ui_ms.clear();
        m_present_ms.clear();
        m_instanced_parts = 0;
        m_instance_draws = 0;
        m_instance_samples = 0;
        m_monster_instances = 0;
        m_monster_skins = 0;
    }

}  // namespace openkal::client
