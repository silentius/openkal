/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/RestFoodBuff.h"

#include "openkal/protocol/BuffState.h"

namespace openkal::server {
    namespace {

        /** compatibility behavior timer interval used by related compatibility states. */
        constexpr std::uint64_t kPulseMs = 1000;

        void clear_channel(RestFoodChannel& channel) {
            channel.active = false;
            channel.amount = 0;
            channel.expire_ms = 0;
            channel.next_pulse_ms = 0;
        }

    }  // namespace

    std::uint64_t RestFoodSet::mstate() const {
        std::uint64_t bits = 0;
        if (m_hp.active) {
            bits |= protocol::kCmsHp;
        }
        if (m_mp.active) {
            bits |= protocol::kCmsMp;
        }
        return bits;
    }

    bool RestFoodSet::active() const {
        return m_hp.active || m_mp.active;
    }

    bool RestFoodSet::apply_channel(RestFoodChannel& channel,
                                    int amount,
                                    int duration_sec,
                                    std::uint64_t now_ms) {
        if (amount <= 0 || duration_sec <= 0) {
            return false;
        }
        const bool was_active = channel.active;
        channel.active = true;
        channel.amount = amount;
        channel.expire_ms = now_ms + static_cast<std::uint64_t>(duration_sec) * 1000ull;
        if (!was_active || channel.next_pulse_ms == 0 || channel.next_pulse_ms <= now_ms) {
            channel.next_pulse_ms = now_ms + kPulseMs;
        }
        return true;
    }

    bool RestFoodSet::apply_hp(int amount, int duration_sec, std::uint64_t now_ms) {
        return apply_channel(m_hp, amount, duration_sec, now_ms);
    }

    bool RestFoodSet::apply_mp(int amount, int duration_sec, std::uint64_t now_ms) {
        return apply_channel(m_mp, amount, duration_sec, now_ms);
    }

    void RestFoodSet::cancel_hp() {
        clear_channel(m_hp);
    }

    void RestFoodSet::cancel_mp() {
        clear_channel(m_mp);
    }

    void RestFoodSet::cancel_all() {
        clear_channel(m_hp);
        clear_channel(m_mp);
    }

    void RestFoodSet::tick_channel(RestFoodChannel& channel,
                                   std::uint64_t now_ms,
                                   int& add,
                                   bool& expired) {
        add = 0;
        expired = false;
        if (!channel.active) {
            return;
        }
        if (now_ms >= channel.next_pulse_ms) {
            add = channel.amount;
            channel.next_pulse_ms += kPulseMs;
            if (channel.next_pulse_ms <= now_ms) {
                channel.next_pulse_ms = now_ms + kPulseMs;
            }
        }
        if (now_ms >= channel.expire_ms) {
            expired = true;
            clear_channel(channel);
        }
    }

    RestFoodPulse RestFoodSet::tick(std::uint64_t now_ms) {
        RestFoodPulse out;
        tick_channel(m_hp, now_ms, out.hp_add, out.hp_expired);
        tick_channel(m_mp, now_ms, out.mp_add, out.mp_expired);
        return out;
    }

}  // namespace openkal::server
