/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/CastleWarService.h"
#include "openkal/server/GuildService.h"
#include "test_util.h"

namespace {
    using Json = openkal::platform::Json;
    using openkal::server::CastleWarActivity;
    using openkal::server::CastleWarPhase;
    using openkal::server::CastleWarPosition;
    using openkal::server::CastleWarRules;
    using openkal::server::CastleWarService;
    using openkal::server::CastleWarSide;
    using openkal::server::CastleWarSimulation;
    using openkal::test::expect;

    class GuildStore final : public openkal::server::IGuildStore {
      public:
        bool load_guilds(Json& output) override {
            output = m_state;
            return true;
        }

        bool commit_guilds(std::uint64_t revision,
                           const Json& next,
                           const std::vector<openkal::server::CharacterRecord>&) override {
            if (m_state.at("revision").number() != static_cast<double>(revision)) {
                return false;
            }
            m_state = next;
            return true;
        }

      private:
        Json m_state = Json::parse(R"({
            "revision":0,"next_id":10,"guilds":[
                {"id":1,"name":"Leading","key":"leading","members":[{"id":11,"rank":1}]},
                {"id":2,"name":"Allied","key":"allied","members":[{"id":12,"rank":1}]},
                {"id":3,"name":"Eastern","key":"eastern","members":[{"id":13,"rank":1}]},
                {"id":4,"name":"Western","key":"western","members":[{"id":14,"rank":1}]},
                {"id":5,"name":"South","key":"south","members":[{"id":15,"rank":1}]},
                {"id":6,"name":"North","key":"north","members":[{"id":16,"rank":1}]},
                {"id":7,"name":"Extra","key":"extra","members":[{"id":17,"rank":1}]},
                {"id":8,"name":"Next","key":"next","members":[{"id":18,"rank":1}]}
            ]
        })");
    };

    class CastleStore final : public openkal::server::ICastleWarStore {
      public:
        bool fail = false;
        int writes = 0;

        bool load_castle_war(Json& output) override {
            output = m_state;
            return true;
        }

        bool commit_castle_war(std::uint64_t revision, const Json& next) override {
            ++writes;
            if (fail || m_state.at("revision").number() != static_cast<double>(revision)) {
                return false;
            }
            m_state = next;
            return true;
        }

      private:
        Json m_state = Json::parse(R"({"revision":0})");
    };

    void castle_war() {
        GuildStore guild_store;
        CastleStore castle_store;
        openkal::server::SqliteStore characters;
        expect(characters.open(":memory:"), "open character lookup");
        openkal::server::GuildRules guild_rules;
        std::string error;
        expect(guild_rules.parse(Json::parse("{}"), error), "default guild policy");
        openkal::server::GuildService guilds(guild_rules, guild_store, characters);
        expect(guilds.restore() && guilds.is_leader(11) && guilds.membership(17) == 7,
               "restore authenticated guild identities");

        CastleWarRules rules;
        rules.enabled = true;
        constexpr std::int64_t week = 7 * 86400;
        const auto first = rules.round(1700000000);
        const auto declaration = first.declaration;
        const auto unlock = first.battle + rules.capture_unlock_seconds;
        const CastleWarPosition village{0, 120, 240, 360};
        CastleWarService initial(rules, castle_store, guilds);
        expect(initial.restore(), "load the empty siege store");
        for (std::uint32_t guild = 2; guild <= 6; ++guild) {
            expect(initial.invite(guild + 10, 1, 15, declaration).empty() &&
                       initial.accept(11, guild, declaration).empty(),
                   "allow five partner guilds in addition to the leading guild");
        }
        expect(initial.invite(17, 1, 15, declaration).empty() &&
                   !initial.accept(11, 7, declaration).empty(),
               "reject the seventh total guild");
        expect(!initial.declare_attack(12, declaration).empty(),
               "an allied guild cannot register instead of its alliance leader");
        expect(initial.declare_attack(11, declaration).empty() &&
                   initial.declare_attack(18, declaration).empty(),
               "declare separate attack alliances");
        expect(!initial.leave(12, first.preparation).empty(),
               "lock alliance changes at the end of declaration");
        expect(!initial.admit(11, 70, village, first.admission - 1).empty() &&
                   !initial.admit(11, 69, village, first.admission).empty(),
               "validate admission window and character level");
        expect(initial.admit(11, 70, village, first.admission).empty() &&
                   initial.admit(12, 70, village, first.admission).empty() &&
                   initial.admit(18, 70, village, first.admission).empty(),
               "admit registered alliance participants");
        expect(!initial.opposed(11, 18, first.admission) && !initial.hostile(11, 18, first.battle),
               "different alliances of the same faction cannot attack each other");
        expect(!initial.admit(17, 70, village, first.battle).empty(),
               "close admission at battle start");
        expect(!initial.can_damage_flag(0, 11, unlock - 1) &&
                   initial.can_damage_flag(0, 11, unlock),
               "unlock flag damage at the configured boundary");
        expect(initial.damage_flag(0, 11, 300, unlock) &&
                   initial.objective(0).hp == rules.flags[0].hit_points - 300,
               "apply partial objective damage");
        expect(initial.advance(unlock + rules.flag_regen_seconds) &&
                   initial.objective(0).hp == rules.flags[0].hit_points - 200,
               "flag recovers HP while damaged");

        castle_store.fail = true;
        expect(!initial.damage_flag(
                   0, 11, rules.flags[0].hit_points, unlock + rules.flag_regen_seconds) &&
                   initial.objective(0).side == CastleWarSide::None,
               "roll back a failed capture write");
        castle_store.fail = false;
        CastleWarService restored(rules, castle_store, guilds);
        const auto capture = unlock + rules.flag_regen_seconds + rules.protection_seconds;
        expect(
            restored.restore() && restored.active(11, capture) &&
                restored.participant(11)->return_position.z == village.z &&
                !restored.can_damage_flag(0, 11, capture),
            "restore enrollment and return position without marking disconnected players online");
        expect(restored.reconnect(11, capture - rules.protection_seconds) &&
                   restored.reconnect(12, capture - rules.protection_seconds) &&
                   restored.reconnect(18, capture - rules.protection_seconds) &&
                   !restored.can_damage_flag(0, 11, capture - 1),
               "return to camp and wait for protection expiry");
        for (std::size_t flag = 0; flag < rules.flags.size(); ++flag) {
            const auto claimant = flag == 0 ? 18 : 11;
            expect(restored.damage_flag(flag, claimant, rules.flags[flag].hit_points, capture) &&
                       !restored.can_damage_flag(flag, 12, capture),
                   "capture and protect friendly flag");
        }
        expect(restored.score()[0] == 140 && restored.advance(capture + rules.hold_seconds - 1) &&
                   restored.phase(capture + rules.hold_seconds - 1) == CastleWarPhase::Battle,
               "sum the big and three small standards and wait the full hold interval");
        expect(restored.record_activity(12, CastleWarActivity::Support, capture) &&
                   restored.advance(capture + rules.hold_seconds) &&
                   restored.last_winner() == CastleWarSide::Attack && restored.owner() == 1 &&
                   restored.honor(11) > restored.honor(12) &&
                   restored.honor(12) >= rules.honor_ally,
               "settle the winning faction and highest-scoring alliance");
        const auto award = restored.honor(11);
        expect(restored.advance(first.end) && restored.honor(11) == award,
               "scheduled end cannot settle an early victory again");
        expect(restored.set_tax(12, 2).size() > 0 && restored.set_tax(11, 2).empty() &&
                   restored.tax_percent() == 110,
               "only the castle lord controls price policy");
        for (const auto character : {11, 12, 18}) {
            expect(restored.depart(character), "clear a returned participant after settlement");
        }

        const auto next = rules.round(first.battle + week);
        expect(restored.declare_attack(18, next.declaration).empty() &&
                   !restored.declare_attack(11, next.declaration).empty() &&
                   restored.admit(12, 70, village, next.admission).empty() &&
                   restored.admit(18, 70, village, next.admission).empty(),
               "automatically register the castle owner's alliance for defense");
        expect(restored.opposed(18, 12, next.admission) &&
                   !restored.hostile(18, 12, next.admission) &&
                   restored.hostile(18, 12, next.battle),
               "only opposing active participants fight");
        expect(restored.record_kill(18, 12, next.battle) &&
                   restored.record_kill(12, 18, next.battle) &&
                   restored.record_kill(12, 18, next.battle) &&
                   restored.pvp_bonus(18, next.battle) == std::make_pair(20, 25),
               "count attacking player kills and deaths for the attacker buff");
        expect(restored.revive(12, next.battle) && !restored.hostile(18, 12, next.battle) &&
                   restored.hostile(18, 12, next.battle + rules.protection_seconds),
               "protect a defender after revival");
        expect(restored.disconnect(18, next.battle + rules.protection_seconds) &&
                   !restored.hostile(18, 12, next.battle + rules.protection_seconds),
               "discard disconnected contribution while preserving return information");
        expect(restored.record_activity(
                   12, CastleWarActivity::Support, next.battle + rules.protection_seconds) &&
                   restored.checkpoint(),
               "flush contribution before stopping with participants connected");
        const auto previous_honor = restored.honor(12);
        CastleWarService final_restart(rules, castle_store, guilds);
        expect(final_restart.restore() && final_restart.advance(next.end) &&
                   final_restart.owner() == 1 &&
                   final_restart.last_winner() == CastleWarSide::Defense &&
                   final_restart.tax_multiplier_percent() == 110 &&
                   final_restart.honor(12) > previous_honor &&
                   final_restart.honor(18) == restored.honor(18),
               "settle an overdue round using checkpointed contribution and disconnect state");

        const auto tiebreak = rules.round(next.battle + week);
        expect(final_restart.depart(12) && final_restart.depart(18) &&
                   final_restart.declare_attack(17, tiebreak.declaration).empty() &&
                   final_restart.declare_attack(18, tiebreak.declaration).empty() &&
                   final_restart.admit(11, 70, village, tiebreak.admission).empty() &&
                   final_restart.admit(17, 80, village, tiebreak.admission).empty() &&
                   final_restart.admit(18, 75, village, tiebreak.admission).empty(),
               "schedule a defense against two independent attackers");
        const auto next_capture = tiebreak.battle + rules.capture_unlock_seconds;
        for (std::size_t flag = 0; flag < rules.flags.size(); ++flag) {
            const auto attacker = flag % 2 == 0 ? 17 : 18;
            expect(final_restart.damage_flag(
                       flag, attacker, rules.flags[flag].hit_points, next_capture),
                   "attackers take all flags");
        }
        const auto last_second = next_capture + rules.hold_seconds;
        expect(!final_restart.can_damage_flag(0, 11, last_second) &&
                   !final_restart.record_kill(17, 11, last_second),
               "a completed hold ends combat before the next scheduled update");
        expect(final_restart.damage_flag(0, 11, rules.flags[0].hit_points, last_second - 1) &&
                   final_restart.advance(last_second) &&
                   final_restart.phase(last_second) == CastleWarPhase::Battle,
               "recapturing a flag before the deadline resets the hold");
        expect(final_restart.damage_flag(3, 11, rules.flags[3].hit_points, last_second) &&
                   final_restart.score() == std::array<int, 2>{60, 80},
               "defenders recover the flag score lead");
        expect(final_restart.record_kill(18, 11, last_second) &&
                   final_restart.advance(tiebreak.end) &&
                   final_restart.last_winner() == CastleWarSide::Defense,
               "faction scoring takes precedence over individual kills and levels");

        CastleStore neutral_store;
        CastleWarService neutral(rules, neutral_store, guilds);
        expect(neutral.restore() && neutral.declare_attack(17, declaration).empty() &&
                   neutral.declare_attack(18, declaration).empty() &&
                   neutral.admit(17, 70, village, first.admission).empty() &&
                   neutral.admit(18, 70, village, first.admission).empty() &&
                   neutral.damage_flag(1, 17, rules.flags[1].hit_points, unlock) &&
                   neutral.damage_flag(2, 18, rules.flags[2].hit_points, unlock) &&
                   neutral.update_level(18, 71, first.end - 1) && neutral.advance(first.end) &&
                   neutral.owner() == 8,
               "using the participant levels at settlement breaks a flag and kill tie");

        const auto sandbox_time = first.end + 1;
        const int initial_writes = neutral_store.writes;
        const auto regular_honor = neutral.honor(18);
        expect(neutral.depart(17) && neutral.depart(18), "return the regular test players");
        expect(neutral.set_tax(18, 2).empty() && neutral.tax_percent() == 110 &&
                   neutral_store.writes == initial_writes + 3,
               "save a regular territory policy before starting the simulation");
        const int regular_writes = neutral_store.writes;

        CastleWarSimulation simulation;
        simulation.attack = {{1, 2}, {3}};
        simulation.defense = {4, 5};
        simulation.declaration_seconds = 3;
        simulation.preparation_seconds = 4;
        simulation.admission_seconds = 5;
        simulation.battle_seconds = 60;
        simulation.capture_unlock_seconds = 2;
        simulation.hold_seconds = 6;
        simulation.flag_hit_points = 42;
        expect(!final_restart.simulation_start_blocker(tiebreak.admission).empty() &&
                   !final_restart.start_simulation(simulation, tiebreak.admission).empty(),
               "do not replace a regular siege during admission or battle");
        expect(!final_restart.simulation_start_blocker(tiebreak.end + 1).empty() &&
                   !final_restart.start_simulation(simulation, tiebreak.end + 1).empty(),
               "do not replace a regular siege with online returns pending");
        auto invalid = simulation;
        invalid.attack = {{1, 2, 3, 4, 5, 6, 7}};
        expect(!neutral.start_simulation(invalid, sandbox_time).empty() && !neutral.simulating(),
               "the alliance limit also applies to test rosters");
        invalid = simulation;
        invalid.defense = {3};
        expect(!neutral.start_simulation(invalid, sandbox_time).empty() && !neutral.simulating() &&
                   neutral_store.writes == regular_writes,
               "a guild cannot appear in multiple simulated alliances");
        invalid.defense = {101};
        expect(!neutral.start_simulation(invalid, sandbox_time).empty(),
               "simulation rejects nonexistent guild IDs");
        invalid = simulation;
        invalid.capture_unlock_seconds = invalid.battle_seconds;
        expect(!neutral.start_simulation(invalid, sandbox_time).empty(),
               "simulation rejects an unlock that extends beyond the battle");
        expect(neutral.simulation_start_blocker(sandbox_time).empty() &&
                   neutral.start_simulation(simulation, sandbox_time).empty() &&
                   !neutral.simulation_start_blocker(sandbox_time).empty() &&
                   neutral.simulating() && neutral.owner() == 4 && neutral.tax_percent() == 110 &&
                   !neutral.set_tax(14, 0).empty() &&
                   !neutral.declare_attack(17, sandbox_time).empty() &&
                   !neutral.leave(12, sandbox_time).empty(),
               "simulation seeds a separate owner and leaves territory prices and roster fixed");
        const auto sandbox = neutral.round(sandbox_time);
        expect(sandbox.preparation == sandbox_time + 3 && sandbox.admission == sandbox_time + 7 &&
                   sandbox.battle == sandbox_time + 12 && sandbox.end == sandbox_time + 72 &&
                   neutral.phase_ends_at(sandbox_time) == sandbox.preparation &&
                   neutral.phase(sandbox.preparation) == CastleWarPhase::Preparation &&
                   !neutral.finish_simulation(sandbox_time).empty(),
               "use the requested durations for each simulated phase");
        expect(neutral.advance(sandbox.admission) && neutral.registered(11) &&
                   neutral.registered(12) && neutral.registered(13) && neutral.registered(14) &&
                   neutral.registered(15) && !neutral.registered(18) &&
                   neutral.admit(11, 70, village, sandbox.admission).empty() &&
                   neutral.admit(12, 70, village, sandbox.admission).empty() &&
                   neutral.admit(15, 70, village, sandbox.admission).empty(),
               "admit members of the configured attack and defense alliances");
        const auto leading = neutral.participant(12);
        const auto defending = neutral.participant(15);
        expect(leading && leading->alliance == 1 && leading->side == CastleWarSide::Attack &&
                   defending && defending->alliance == 4 &&
                   defending->side == CastleWarSide::Defense,
               "use the first guild as the leader even when its ally joins the fight");
        expect(!neutral.damage_flag(0, 11, 42, sandbox.battle + 1) &&
                   neutral.objective(0).hp == 42 &&
                   neutral.damage_flag(0, 12, 42, sandbox.battle + 2) &&
                   neutral.damage_flag(1, 12, 42, sandbox.battle + 2) &&
                   neutral.finish_simulation(sandbox.battle + 3).empty() && neutral.owner() == 1 &&
                   neutral.last_winner() == CastleWarSide::Attack &&
                   neutral.score() == std::array<int, 2>{80, 60} && neutral.honor(12) > 0 &&
                   neutral.advance(sandbox.end + 3 * week) &&
                   neutral.phase(sandbox.end + 3 * week) == CastleWarPhase::Idle &&
                   neutral_store.writes == regular_writes,
               "a simulated ally's captures crown its leader using the ordinary victory rules");
        expect(neutral.restore_schedule(sandbox.end).empty() && neutral.restore_pending() &&
                   neutral.depart(11) && neutral.depart(12) &&
                   neutral.disconnect(15, sandbox.end) && neutral.advance(sandbox.end) &&
                   !neutral.restore_pending() && !neutral.simulating() && neutral.owner() == 8 &&
                   neutral.honor(18) == regular_honor && neutral.honor(12) == 0 &&
                   neutral.tax_percent() == 110 && neutral_store.writes == regular_writes,
               "restore real ownership and Honor after online players return");
        const auto offline = neutral.participant(15);
        expect(offline && offline->return_position.z == village.z &&
                   !neutral.active(15, sandbox.end) && neutral.depart(15) &&
                   !neutral.participant(15),
               "an offline simulation participant retains their return position until login");

        simulation.defense.clear();
        simulation.declaration_seconds = 0;
        simulation.preparation_seconds = 0;
        expect(neutral.start_simulation(simulation, sandbox.end + 1).empty() &&
                   neutral.phase(sandbox.end + 1) == CastleWarPhase::Admission &&
                   neutral.admit(11, 70, village, sandbox.end + 1).empty() &&
                   neutral.restore_schedule(sandbox.end + 2).empty() && neutral.restore_pending() &&
                   !neutral.active(11, sandbox.end + 2) && neutral.depart(11) &&
                   neutral.advance(sandbox.end + 2) && !neutral.simulating() &&
                   neutral_store.writes == regular_writes,
               "cancelling during admission returns participants without settling rewards");

        CastleStore pending_store;
        CastleWarService pending(rules, pending_store, guilds);
        expect(pending.restore() && pending.declare_attack(11, first.declaration).empty() &&
                   pending.admit(11, 70, village, first.admission).empty() &&
                   pending.disconnect(11, first.battle) && pending.advance(first.end),
               "settle a regular siege with an offline participant yet to return");
        const int pending_writes = pending_store.writes;
        expect(pending.start_simulation(simulation, sandbox_time).empty() &&
                   pending.participant(11) && !pending.active(11, sandbox_time) &&
                   !pending.admit(11, 70, village, sandbox_time).empty() && pending.depart(11) &&
                   !pending.participant(11) && pending_store.writes == pending_writes &&
                   pending.admit(11, 70, village, sandbox_time).empty(),
               "an offline regular return survives the simulation and can finish without writes");
        expect(pending.restore_schedule(sandbox_time).empty() && pending.depart(11) &&
                   pending.advance(sandbox_time) && !pending.simulating() &&
                   pending_store.writes == pending_writes + 1,
               "commit any regular return acknowledged during the simulation when it ends");
        CastleWarService verify_persisted(rules, pending_store, guilds);
        expect(verify_persisted.restore() && !verify_persisted.participant(11),
               "the acknowledged regular return stays cleared across restart");
    }
}  // namespace

int main() {
    castle_war();
    return openkal::test::g_failures != 0;
}
