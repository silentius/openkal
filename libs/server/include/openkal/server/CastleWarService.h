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

#include "openkal/server/CastleWarRules.h"
#include "openkal/server/CastleWarStore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace openkal::server {
    class GuildService;

    /** Faction assigned to a guild or flag for the current siege. */
    enum class CastleWarSide { None, Attack, Defense };

    /** Nonlethal contribution from a successfully applied skill. */
    enum class CastleWarActivity { Skill, Support };

    /** Immutable enrollment and the current spawn-protection deadline. */
    struct CastleWarParticipant {
        std::uint32_t guild = 0;
        std::uint32_t alliance = 0;
        CastleWarSide side = CastleWarSide::None;
        int level = 0;
        CastleWarPosition return_position;
        std::int64_t protected_until = 0;
    };

    /** Current flag owner and health. */
    struct CastleWarObjective {
        CastleWarSide side = CastleWarSide::None;
        std::uint32_t alliance = 0;
        int hp = 0;
    };

    /** One isolated round with explicit guild alliances and short phase durations. */
    struct CastleWarSimulation {
        /** Attacking alliances, each with its leading guild ID first. */
        std::vector<std::vector<std::uint32_t>> attack;
        /** Defending alliance, with its leading guild ID first. Empty starts unclaimed. */
        std::vector<std::uint32_t> defense;
        int declaration_seconds = 5;
        int preparation_seconds = 5;
        int admission_seconds = 20;
        int battle_seconds = 120;
        int capture_unlock_seconds = 0;
        int hold_seconds = 10;
        int flag_hit_points = 100;
    };

    /** Coordinates Castle War on the world thread and persists siege results atomically. */
    class CastleWarService final {
      public:
        /** Creates a controller for an enabled multiplayer world.
         * @param rules Validated event policy.
         * @param storage Durable event store.
         * @param guilds Guild identity and leadership authority.
         */
        CastleWarService(CastleWarRules rules, ICastleWarStore& storage, GuildService& guilds);

        /** Loads the last committed event state.
         * @return Whether the state could be read and validated.
         */
        [[nodiscard]] bool restore();

        /** Advances the weekly round and processes due checkpoints or settlement.
         * @param now Unix seconds.
         * @return Whether all required writes succeeded.
         */
        [[nodiscard]] bool advance(std::int64_t now);

        /** Flushes pending scores and objective health before a graceful stop.
         * @return Whether all pending writes succeeded.
         */
        [[nodiscard]] bool checkpoint();

        /** Reports the actual phase, taking early victories into account.
         * @param now Unix seconds.
         * @return Current phase.
         */
        [[nodiscard]] CastleWarPhase phase(std::int64_t now) const;

        /** Gets the current simulation round or the current or next scheduled siege.
         * @param now Unix seconds.
         * @return Phase boundaries using the effective schedule.
         */
        [[nodiscard]] CastleWarRound round(std::int64_t now) const;

        /** Gets the next phase transition, including a pending early victory.
         * @param now Unix seconds.
         * @return Unix seconds of the transition, or zero for a resolved simulation.
         */
        [[nodiscard]] std::int64_t phase_ends_at(std::int64_t now) const;

        /** Reports a server state that prevents a test-control round from starting.
         * @param now Unix seconds.
         * @return Empty when the server can accept a new simulation, otherwise the reason.
         */
        [[nodiscard]] std::string simulation_start_blocker(std::int64_t now) const;

        /** Starts a test-control round without writing to the regular siege store.
         * @param settings Explicit alliances and shortened round durations.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string start_simulation(const CastleWarSimulation& settings,
                                                   std::int64_t now);

        /** Settles the running simulation according to the ordinary victory rules.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string finish_simulation(std::int64_t now);

        /** Cancels or closes a simulation, restoring regular state after online players return.
         * @param now Unix seconds.
         * @return Empty when regular scheduling is restored or awaiting transfers.
         */
        [[nodiscard]] std::string restore_schedule(std::int64_t now);

        /** Reports whether a simulation still owns the effective state.
         * @return Whether a simulation is running, resolved, or closing.
         */
        [[nodiscard]] bool simulating() const;

        /** Reports whether the regular schedule is waiting for transfers.
         * @return Whether the simulation is closing.
         */
        [[nodiscard]] bool restore_pending() const;

        /** Returns server-side siege configuration.
         * @return Validated event policy.
         */
        [[nodiscard]] const CastleWarRules& rules() const;

        /** Gets the current castle lord's guild.
         * @return Guild ID, or zero for an unoccupied castle.
         */
        [[nodiscard]] std::uint32_t owner() const;

        /** Gets the outcome of the last settled siege.
         * @return Winning faction, or None before any settlement.
         */
        [[nodiscard]] CastleWarSide last_winner() const;

        /** Gets the character's accumulated Castle War Honor.
         * @param character Persistent character ID.
         * @return Awarded Honor total.
         */
        [[nodiscard]] std::int64_t honor(std::uint32_t character) const;

        /** Applies to join the named leading guild's alliance.
         * @param character Authenticated applicant.
         * @param target_guild Leading guild ID.
         * @param days Requested alliance duration.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string
        invite(std::uint32_t character, std::uint32_t target_guild, int days, std::int64_t now);

        /** Accepts an alliance application submitted by a guild.
         * @param character Authenticated alliance leader.
         * @param inviting_guild Applying guild ID.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string
        accept(std::uint32_t character, std::uint32_t inviting_guild, std::int64_t now);

        /** Ends the actor guild's participation in its alliance.
         * @param character Authenticated guild leader.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string leave(std::uint32_t character, std::int64_t now);

        /** Registers the actor's guild or alliance to attack.
         * @param character Authenticated guild leader.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string declare_attack(std::uint32_t character, std::int64_t now);

        /** Admits a registered participant and saves their return position.
         * @param character Persistent character ID.
         * @param level Live character level.
         * @param return_position Position to restore when the siege ends.
         * @param now Unix seconds.
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string admit(std::uint32_t character,
                                        int level,
                                        CastleWarPosition return_position,
                                        std::int64_t now);

        /** Checks whether the character's guild is registered for the current siege.
         * @param character Persistent character ID.
         * @return Whether admission has a matching guild registration.
         */
        [[nodiscard]] bool registered(std::uint32_t character) const;

        /** Gets the participant's enrollment, including any pending return.
         * @param character Persistent character ID.
         * @return Enrollment if the siege still tracks the character.
         */
        [[nodiscard]] std::optional<CastleWarParticipant>
        participant(std::uint32_t character) const;

        /** Checks whether enrollment belongs to the current admission or battle.
         * @param character Persistent character ID.
         * @param now Unix seconds.
         * @return Whether the character has active enrollment.
         */
        [[nodiscard]] bool active(std::uint32_t character, std::int64_t now) const;

        /** Gets the revival and admission destination for a faction.
         * @param side Attack or Defense.
         * @return Configured camp position.
         */
        [[nodiscard]] CastleWarPosition camp(CastleWarSide side) const;

        /** Removes unbanked contribution after a disconnect and marks the player offline.
         * @param character Persistent character ID.
         * @param now Unix seconds.
         * @return Whether any required write succeeded.
         */
        [[nodiscard]] bool disconnect(std::uint32_t character, std::int64_t now);

        /** Resets contribution and protects an admitted player returning to their camp.
         * @param character Persistent character ID.
         * @param now Unix seconds.
         * @return Whether active enrollment was updated.
         */
        [[nodiscard]] bool reconnect(std::uint32_t character, std::int64_t now);

        /** Grants temporary protection after reviving at the faction camp.
         * @param character Persistent character ID.
         * @param now Unix seconds.
         * @return Whether active enrollment was updated.
         */
        [[nodiscard]] bool revive(std::uint32_t character, std::int64_t now);

        /** Removes the pending return once the host confirms the player's return.
         * @param character Persistent character ID.
         * @return Whether inactive enrollment was cleared or already absent.
         */
        [[nodiscard]] bool depart(std::uint32_t character);

        /** Checks whether two enrolled characters belong to opposite factions.
         * @param source First character ID.
         * @param target Second character ID.
         * @param now Unix seconds.
         * @return Whether both are enrolled on opposite sides of the active round.
         */
        [[nodiscard]] bool
        opposed(std::uint32_t source, std::uint32_t target, std::int64_t now) const;

        /** Checks whether an opposing pair may deal damage now.
         * @param source Attacking character ID.
         * @param target Target character ID.
         * @param now Unix seconds.
         * @return Whether battle is active with neither player spawn-protected.
         */
        [[nodiscard]] bool
        hostile(std::uint32_t source, std::uint32_t target, std::int64_t now) const;

        /** Records an authenticated enemy kill and death.
         * @param source Attacking character ID.
         * @param target Defeated character ID.
         * @param now Unix seconds.
         * @return Whether the active siege accepted the kill.
         */
        [[nodiscard]] bool
        record_kill(std::uint32_t source, std::uint32_t target, std::int64_t now);

        /** Records participation for a skill or allied support.
         * @param character Persistent character ID.
         * @param activity Kind of contribution.
         * @param now Unix seconds.
         * @return Whether the active siege accepted the contribution.
         */
        [[nodiscard]] bool
        record_activity(std::uint32_t character, CastleWarActivity activity, std::int64_t now);

        /** Updates a participant's level for the final tiebreaker.
         * @param character Persistent character ID.
         * @param level Current character level.
         * @param now Unix seconds.
         * @return Whether the active siege accepted the level.
         */
        [[nodiscard]] bool update_level(std::uint32_t character, int level, std::int64_t now);

        /** Gets a configured objective's current state.
         * @param flag Offset into the four configured objectives.
         * @return Current state or an empty objective for an invalid offset.
         */
        [[nodiscard]] CastleWarObjective objective(std::size_t flag) const;

        /** Checks whether a character is allowed to damage an objective.
         * @param flag Offset into the four configured objectives.
         * @param character Attacking character ID.
         * @param now Unix seconds.
         * @return Whether the attack can affect this objective.
         */
        [[nodiscard]] bool
        can_damage_flag(std::size_t flag, std::uint32_t character, std::int64_t now) const;

        /** Applies server-computed objective damage and transfers control on a finishing hit.
         * @param flag Offset into the four configured objectives.
         * @param character Attacking character ID.
         * @param damage Amount of damage.
         * @param now Unix seconds.
         * @return Whether damage was applied.
         */
        [[nodiscard]] bool
        damage_flag(std::size_t flag, std::uint32_t character, int damage, std::int64_t now);

        /** Gets the flag score for Attack and Defense, in that order.
         * @return Current team scores.
         */
        [[nodiscard]] std::array<int, 2> score() const;

        /** Gets temporary battle balance adjustments for an attacker.
         * @param character Persistent character ID.
         * @param now Unix seconds.
         * @return Percentages for PvP attack and defense.
         */
        [[nodiscard]] std::pair<int, int> pvp_bonus(std::uint32_t character,
                                                    std::int64_t now) const;

        /** Gets the current territory's NPC price policy.
         * @return Percentage of the base price.
         */
        [[nodiscard]] int tax_percent() const;

        /** Gets the castle's treasury multiplier for consecutive defenses.
         * @return Treasury accrual percentage.
         */
        [[nodiscard]] int tax_multiplier_percent() const;

        /** Changes territory price policy when requested by the castle lord.
         * @param character Authenticated character ID.
         * @param policy Low (0), middle (1), or high (2).
         * @return Empty on success or a rejection message.
         */
        [[nodiscard]] std::string set_tax(std::uint32_t character, int policy);

      private:
        struct Alliance {
            std::uint32_t leader = 0;
            std::int64_t expires = 0;
        };

        struct Application {
            int days = 0;
            std::int64_t expires = 0;
        };

        struct Registration {
            std::uint32_t alliance = 0;
            CastleWarSide side = CastleWarSide::None;
        };

        struct Player {
            CastleWarParticipant data;
            std::int64_t round = 0;
            int kills = 0;
            int deaths = 0;
            int captures = 0;
            int activity = 0;
            int skills = 0;
            bool online = false;
            bool settlement_eligible = false;
        };

        struct State {
            std::uint64_t revision = 0;
            std::uint32_t owner = 0;
            int tax_policy = 1;
            int defenses = 0;
            CastleWarSide winner = CastleWarSide::None;
            std::map<std::uint32_t, Alliance> alliances;
            std::map<std::pair<std::uint32_t, std::uint32_t>, Application> applications;
            CastleWarRound round{};
            std::set<std::uint32_t> declarations;
            std::map<std::uint32_t, Registration> registrations;
            std::map<std::uint32_t, Player> players;
            std::array<CastleWarObjective, 4> flags{};
            std::int64_t hold_since = 0;
            std::int64_t next_regen = 0;
            bool locked = false;
            bool finalized = false;
            std::map<std::uint32_t, std::int64_t> honor;
        };

        struct RegularSchedule {
            State state;
            CastleWarRules rules;
            std::int64_t last_checkpoint = 0;
            bool dirty = false;
            bool restore_pending = false;
        };

        static platform::Json encode(const State& state);
        static bool decode(const platform::Json& document, State& output);
        bool commit(State state, const std::vector<HonorGrant>& awards = {});
        bool start_scheduled_round(std::int64_t now);
        bool lock_registration(std::int64_t now);
        bool finalize(std::int64_t occurred_at);
        bool reset_in_camp(std::uint32_t character, std::int64_t now, bool reset_score);
        void regenerate(std::int64_t now);
        bool hold_complete(std::int64_t now) const;
        bool battle_open(std::int64_t now) const;
        bool restore_when_returned();
        bool alliance_changes_open(std::int64_t now) const;
        std::uint32_t leader(std::uint32_t guild, std::int64_t now) const;
        bool allied(std::uint32_t guild, std::uint32_t leader, std::int64_t now) const;
        CastleWarSide all_flags_side(const State& state) const;
        std::array<int, 2> score(const State& state) const;
        std::uint32_t select_lord(CastleWarSide winner) const;

        CastleWarRules m_rules;
        ICastleWarStore& m_storage;
        GuildService& m_guilds;
        State m_state;
        std::optional<RegularSchedule> m_regular;
        /** Offline test characters that still have an unsent return transfer. */
        std::map<std::uint32_t, CastleWarParticipant> m_simulation_returns;
        std::int64_t m_last_checkpoint = 0;
        bool m_dirty = false;
    };
}  // namespace openkal::server
