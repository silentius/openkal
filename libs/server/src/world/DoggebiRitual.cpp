/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/DoggebiRitual.h"

namespace openkal::server {
    namespace {

        constexpr std::uint8_t kAllMonumentsMask =
            static_cast<std::uint8_t>((1u << kDoggebiRitualMonumentCount) - 1u);

        [[nodiscard]] std::uint8_t slot_mask(int slot) {
            return static_cast<std::uint8_t>(1u << static_cast<unsigned int>(slot));
        }

    }  // namespace

    DoggebiRitualConfig::DoggebiRitualConfig(
        int map, std::array<DoggebiRitualMonument, kDoggebiRitualMonumentCount> monuments)
        : m_map(map), m_monuments(std::move(monuments)) {}

    std::optional<DoggebiRitualConfig>
    DoggebiRitualConfig::create(const std::vector<DoggebiRitualMonument>& monuments) {
        if (monuments.size() != kDoggebiRitualMonumentCount) {
            return std::nullopt;
        }

        std::array<DoggebiRitualMonument, kDoggebiRitualMonumentCount> by_slot{};
        std::array<bool, kDoggebiRitualMonumentCount> seen{};
        const int map = monuments.front().map;
        if (map <= 0) {
            return std::nullopt;
        }

        for (const DoggebiRitualMonument& monument : monuments) {
            if (monument.npc_id == 0 || monument.map != map || monument.slot < 0 ||
                monument.slot >= static_cast<int>(kDoggebiRitualMonumentCount) ||
                monument.required_item <= 0) {
                return std::nullopt;
            }
            const std::size_t slot = static_cast<std::size_t>(monument.slot);
            if (seen[slot]) {
                return std::nullopt;
            }
            seen[slot] = true;
            by_slot[slot] = monument;
        }

        for (const bool present : seen) {
            if (!present) {
                return std::nullopt;
            }
        }
        return DoggebiRitualConfig(map, std::move(by_slot));
    }

    int DoggebiRitualConfig::map() const {
        return m_map;
    }

    const DoggebiRitualMonument* DoggebiRitualConfig::find(std::uint32_t npc_id) const {
        for (const DoggebiRitualMonument& monument : m_monuments) {
            if (monument.npc_id == npc_id) {
                return &monument;
            }
        }
        return nullptr;
    }

    const DoggebiRitualMonument& DoggebiRitualConfig::monument(std::size_t slot) const {
        return m_monuments[slot];
    }

    DoggebiRitual::DoggebiRitual(DoggebiRitualConfig config) : m_config(std::move(config)) {}

    int DoggebiRitual::map() const {
        return m_config.map();
    }

    DoggebiRitualPhase DoggebiRitual::phase() const {
        return m_phase;
    }

    std::uint8_t DoggebiRitual::active_mask() const {
        return m_active_mask;
    }

    std::uint64_t DoggebiRitual::remaining_to_spawn_ms(std::uint64_t now_ms) const {
        if (m_phase == DoggebiRitualPhase::Idle || m_phase == DoggebiRitualPhase::SpawnRequested ||
            m_phase_deadline_ms <= now_ms) {
            return 0;
        }

        const std::uint64_t current = m_phase_deadline_ms - now_ms;
        switch (m_phase) {
        case DoggebiRitualPhase::QuakeOne:
            return current + kQuakeTwoDurationMs + kQuakeThreeDurationMs + kQuakeFourDurationMs;
        case DoggebiRitualPhase::QuakeTwo:
            return current + kQuakeThreeDurationMs + kQuakeFourDurationMs;
        case DoggebiRitualPhase::QuakeThree:
            return current + kQuakeFourDurationMs;
        case DoggebiRitualPhase::QuakeFour:
            return current;
        case DoggebiRitualPhase::Idle:
        case DoggebiRitualPhase::SpawnRequested:
            return 0;
        }
        return 0;
    }

    int DoggebiRitual::required_item(std::uint32_t npc_id) const {
        const DoggebiRitualMonument* monument = m_config.find(npc_id);
        return monument ? monument->required_item : 0;
    }

    const DoggebiRitualMonument& DoggebiRitual::monument(std::size_t slot) const {
        return m_config.monument(slot);
    }

    bool DoggebiRitual::can_activate(std::uint32_t npc_id) const {
        const int slot = slot_for_npc(npc_id);
        return m_phase == DoggebiRitualPhase::Idle && slot >= 0 &&
               (m_active_mask & slot_mask(slot)) == 0;
    }

    DoggebiRitualActivation DoggebiRitual::activate(std::uint32_t npc_id, std::uint64_t now_ms) {
        const int slot = slot_for_npc(npc_id);
        if (slot < 0) {
            return DoggebiRitualActivation::UnknownMonument;
        }
        if (m_phase != DoggebiRitualPhase::Idle) {
            return DoggebiRitualActivation::SequenceActive;
        }
        const std::uint8_t mask = slot_mask(slot);
        if ((m_active_mask & mask) != 0) {
            return DoggebiRitualActivation::AlreadyActive;
        }

        m_active_mask = static_cast<std::uint8_t>(m_active_mask | mask);
        m_monument_expiry_ms[static_cast<std::size_t>(slot)] = now_ms + kMonumentLifetimeMs;
        if (m_active_mask == kAllMonumentsMask) {
            m_monument_expiry_ms.fill(0);
            begin_phase(DoggebiRitualPhase::QuakeOne, now_ms);
        }
        return DoggebiRitualActivation::Accepted;
    }

    DoggebiRitualTickResult DoggebiRitual::tick(std::uint64_t now_ms) {
        DoggebiRitualTickResult result;
        if (m_phase == DoggebiRitualPhase::Idle) {
            for (std::size_t slot = 0; slot < m_monument_expiry_ms.size(); ++slot) {
                const std::uint64_t expiry = m_monument_expiry_ms[slot];
                if (expiry == 0 || now_ms < expiry) {
                    continue;
                }
                const std::uint8_t mask = slot_mask(static_cast<int>(slot));
                m_active_mask = static_cast<std::uint8_t>(m_active_mask & ~mask);
                m_monument_expiry_ms[slot] = 0;
                result.expired_mask = static_cast<std::uint8_t>(result.expired_mask | mask);
            }
            return result;
        }

        if (m_phase == DoggebiRitualPhase::SpawnRequested || now_ms < m_phase_deadline_ms) {
            return result;
        }

        result.phase_changed = true;
        switch (m_phase) {
        case DoggebiRitualPhase::QuakeOne:
            begin_phase(DoggebiRitualPhase::QuakeTwo, now_ms);
            break;
        case DoggebiRitualPhase::QuakeTwo:
            begin_phase(DoggebiRitualPhase::QuakeThree, now_ms);
            break;
        case DoggebiRitualPhase::QuakeThree:
            begin_phase(DoggebiRitualPhase::QuakeFour, now_ms);
            break;
        case DoggebiRitualPhase::QuakeFour:
            m_phase = DoggebiRitualPhase::SpawnRequested;
            m_phase_deadline_ms = 0;
            result.spawn_requested = true;
            break;
        case DoggebiRitualPhase::Idle:
        case DoggebiRitualPhase::SpawnRequested:
            break;
        }
        return result;
    }

    void DoggebiRitual::reset() {
        m_phase = DoggebiRitualPhase::Idle;
        m_active_mask = 0;
        m_monument_expiry_ms.fill(0);
        m_phase_deadline_ms = 0;
    }

    int DoggebiRitual::slot_for_npc(std::uint32_t npc_id) const {
        const DoggebiRitualMonument* monument = m_config.find(npc_id);
        return monument ? monument->slot : -1;
    }

    void DoggebiRitual::begin_phase(DoggebiRitualPhase phase, std::uint64_t now_ms) {
        m_phase = phase;
        switch (phase) {
        case DoggebiRitualPhase::QuakeOne:
            m_phase_deadline_ms = now_ms + kQuakeOneDurationMs;
            break;
        case DoggebiRitualPhase::QuakeTwo:
            m_phase_deadline_ms = now_ms + kQuakeTwoDurationMs;
            break;
        case DoggebiRitualPhase::QuakeThree:
            m_phase_deadline_ms = now_ms + kQuakeThreeDurationMs;
            break;
        case DoggebiRitualPhase::QuakeFour:
            m_phase_deadline_ms = now_ms + kQuakeFourDurationMs;
            break;
        case DoggebiRitualPhase::Idle:
        case DoggebiRitualPhase::SpawnRequested:
            m_phase_deadline_ms = 0;
            break;
        }
    }

}  // namespace openkal::server
