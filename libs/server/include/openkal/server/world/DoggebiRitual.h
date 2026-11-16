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
#include <optional>
#include <utility>
#include <vector>

namespace openkal::server {

    /** Number of monuments required to begin the Doggebi Lord ritual. */
    inline constexpr std::size_t kDoggebiRitualMonumentCount = 8;

    /** One live monument participating in a Doggebi Lord ritual. */
    struct DoggebiRitualMonument {
        /** Runtime NPC entity id. */
        std::uint32_t npc_id = 0;

        /** Logical map that owns the monument. */
        int map = 0;

        /** Stable ritual slot in the range [0, 7]. */
        int slot = -1;

        /** Inventory item consumed when the monument is activated. */
        int required_item = 0;
    };

    /** Immutable monument layout used by one shared ritual instance. */
    class DoggebiRitualConfig final {
      public:
        /**
         * Validates and creates a full eight-monument ritual layout.
         * @param monuments Candidate live monuments loaded from NPC scenarios.
         * @return A layout when exactly one valid monument exists for each slot.
         */
        [[nodiscard]] static std::optional<DoggebiRitualConfig>
        create(const std::vector<DoggebiRitualMonument>& monuments);

        /** @return Map that owns the complete ritual. */
        [[nodiscard]] int map() const;

        /**
         * Finds the monument mapped to a runtime NPC id.
         * @param npc_id Runtime NPC entity id.
         * @return Monument configuration, or nullptr when it is not part of this ritual.
         */
        [[nodiscard]] const DoggebiRitualMonument* find(std::uint32_t npc_id) const;

        /**
         * Returns one monument by its ritual slot.
         * @param slot Ritual slot in the range [0, 7].
         * @return Monument layout entry.
         */
        [[nodiscard]] const DoggebiRitualMonument& monument(std::size_t slot) const;

      private:
        /**
         * Constructs a previously validated layout.
         * @param map Map owning every monument.
         * @param monuments Monuments indexed by ritual slot.
         */
        DoggebiRitualConfig(
            int map, std::array<DoggebiRitualMonument, kDoggebiRitualMonumentCount> monuments);

        /** Map owning every monument. */
        int m_map = 0;

        /** Monument configuration indexed by ritual slot. */
        std::array<DoggebiRitualMonument, kDoggebiRitualMonumentCount> m_monuments{};
    };

    /** Server-authoritative stages of the Doggebi Lord summoning sequence. */
    enum class DoggebiRitualPhase : std::uint8_t {
        /** Monuments may be activated and may expire. */
        Idle = 0,

        /** First 100-second earthquake. */
        QuakeOne,

        /** Second 140-second earthquake. */
        QuakeTwo,

        /** Third 30-second earthquake. */
        QuakeThree,

        /** Fourth 30-second earthquake. */
        QuakeFour,

        /** The host must create the boss or recover from the failed creation. */
        SpawnRequested,
    };

    /** Result of one monument activation attempt. */
    enum class DoggebiRitualActivation : std::uint8_t {
        /** The monument was activated. */
        Accepted = 0,

        /** The NPC is not a configured ritual monument. */
        UnknownMonument,

        /** The monument is already active. */
        AlreadyActive,

        /** A complete ritual is already progressing or awaiting spawn. */
        SequenceActive,
    };

    /** State changes caused by advancing the ritual clock. */
    struct DoggebiRitualTickResult {
        /** Bit mask of monuments that expired during this tick. */
        std::uint8_t expired_mask = 0;

        /** True when the ritual stage advanced during this tick. */
        bool phase_changed = false;

        /** True once when the host must attempt boss creation. */
        bool spawn_requested = false;
    };

    /** Testable, map-wide Doggebi Lord ritual state machine. */
    class DoggebiRitual final {
      public:
        /** One monument activation remains valid for ten minutes. */
        static constexpr std::uint64_t kMonumentLifetimeMs = 600000;

        /** First earthquake duration. */
        static constexpr std::uint64_t kQuakeOneDurationMs = 100000;

        /** Second earthquake duration. */
        static constexpr std::uint64_t kQuakeTwoDurationMs = 140000;

        /** Third earthquake duration. */
        static constexpr std::uint64_t kQuakeThreeDurationMs = 30000;

        /** Fourth earthquake duration. */
        static constexpr std::uint64_t kQuakeFourDurationMs = 30000;

        /**
         * Constructs an idle ritual for a validated layout.
         * @param config Immutable monument layout.
         */
        explicit DoggebiRitual(DoggebiRitualConfig config);

        /** @return Map owning the ritual. */
        [[nodiscard]] int map() const;

        /** @return Current authoritative ritual phase. */
        [[nodiscard]] DoggebiRitualPhase phase() const;

        /** @return Bit mask of currently active monuments. */
        [[nodiscard]] std::uint8_t active_mask() const;

        /**
         * Returns the remaining time until the Lord spawn is requested.
         * @param now_ms Monotonic server time in milliseconds.
         * @return Remaining ritual milliseconds, or zero outside the timed sequence.
         */
        [[nodiscard]] std::uint64_t remaining_to_spawn_ms(std::uint64_t now_ms) const;

        /**
         * Returns the configured required item for an NPC monument.
         * @param npc_id Runtime NPC entity id.
         * @return Item index, or zero when the NPC is not a monument.
         */
        [[nodiscard]] int required_item(std::uint32_t npc_id) const;

        /**
         * Returns the live monument for a ritual slot.
         * @param slot Ritual slot in the range [0, 7].
         * @return Monument configuration.
         */
        [[nodiscard]] const DoggebiRitualMonument& monument(std::size_t slot) const;

        /**
         * Checks whether an NPC may be activated before an inventory item is consumed.
         * @param npc_id Runtime NPC entity id.
         * @return True when the activation can be committed now.
         */
        [[nodiscard]] bool can_activate(std::uint32_t npc_id) const;

        /**
         * Commits a monument activation after the host has atomically consumed its item.
         * @param npc_id Runtime NPC entity id.
         * @param now_ms Monotonic server time in milliseconds.
         * @return Activation result.
         */
        [[nodiscard]] DoggebiRitualActivation activate(std::uint32_t npc_id, std::uint64_t now_ms);

        /**
         * Expires incomplete monuments and advances earthquake timers.
         * @param now_ms Monotonic server time in milliseconds.
         * @return State changes that the host must present to map players.
         */
        [[nodiscard]] DoggebiRitualTickResult tick(std::uint64_t now_ms);

        /** Resets every active monument and returns to Idle after completion or failure. */
        void reset();

      private:
        /**
         * Finds a ritual slot for a runtime NPC id.
         * @param npc_id Runtime NPC entity id.
         * @return Slot index, or -1 when unknown.
         */
        [[nodiscard]] int slot_for_npc(std::uint32_t npc_id) const;

        /**
         * Enters the next timed earthquake phase.
         * @param phase Next ritual phase.
         * @param now_ms Monotonic server time in milliseconds.
         */
        void begin_phase(DoggebiRitualPhase phase, std::uint64_t now_ms);

        /** Immutable monument layout. */
        DoggebiRitualConfig m_config;

        /** Current ritual stage. */
        DoggebiRitualPhase m_phase = DoggebiRitualPhase::Idle;

        /** Active-monument bit mask. */
        std::uint8_t m_active_mask = 0;

        /** Per-slot expiry deadlines while the ritual is idle. */
        std::array<std::uint64_t, kDoggebiRitualMonumentCount> m_monument_expiry_ms{};

        /** Deadline for the current earthquake phase. */
        std::uint64_t m_phase_deadline_ms = 0;
    };

}  // namespace openkal::server
