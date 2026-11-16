/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ArrowHitFeedback.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    std::uint64_t ArrowHitFeedback::launch(std::uint32_t attacker,
                                           std::uint32_t target,
                                           const engine::Vec3& origin,
                                           float scale) {
        if (attacker == 0 || target == 0) {
            return 0;
        }
        Shot shot;
        shot.token = ++m_next_token;
        shot.impact = {attacker, target, 0, origin, {}, scale};
        const auto result = std::find_if(m_results.begin(), m_results.end(), [&](const Result& r) {
            return r.attacker == attacker && r.target == target;
        });
        if (result != m_results.end()) {
            shot.flags = result->flags;
            m_results.erase(result);
        }
        m_shots.push_back(shot);
        return shot.token;
    }

    void
    ArrowHitFeedback::confirm(std::uint32_t attacker, std::uint32_t target, std::uint8_t flags) {
        if (attacker == 0 || target == 0) {
            return;
        }
        const auto shot = std::find_if(m_shots.begin(), m_shots.end(), [&](const Shot& s) {
            return s.impact.attacker == attacker && s.impact.target == target && !s.flags;
        });
        if (shot != m_shots.end()) {
            shot->flags = flags;
        } else {
            m_results.push_back({attacker, target, flags, 0});
        }
    }

    void ArrowHitFeedback::arrive(std::uint64_t token, const engine::Vec3& position) {
        const auto shot = std::find_if(
            m_shots.begin(), m_shots.end(), [&](const Shot& s) { return s.token == token; });
        if (shot != m_shots.end()) {
            shot->arrived = true;
            shot->impact.position = position;
        }
    }

    void ArrowHitFeedback::update(float dt) {
        if (!std::isfinite(dt) || dt <= 0) {
            return;
        }
        for (auto& shot : m_shots) {
            shot.age += dt;
        }
        for (auto& result : m_results) {
            result.age += dt;
        }
        m_shots.erase(std::remove_if(m_shots.begin(),
                                     m_shots.end(),
                                     [](const Shot& s) { return s.age >= 5.0f; }),
                      m_shots.end());
        m_results.erase(std::remove_if(m_results.begin(),
                                       m_results.end(),
                                       [](const Result& r) { return r.age >= 5.0f; }),
                        m_results.end());
    }

    bool ArrowHitFeedback::take(Impact& out) {
        const auto shot = std::find_if(m_shots.begin(), m_shots.end(), [](const Shot& s) {
            return s.arrived && s.flags.has_value();
        });
        if (shot == m_shots.end()) {
            return false;
        }
        out = shot->impact;
        out.flags = *shot->flags;
        m_shots.erase(shot);
        return true;
    }
}  // namespace openkal::client
