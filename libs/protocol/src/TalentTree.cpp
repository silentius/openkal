/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/TalentTree.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <algorithm>

namespace openkal::protocol {
    const std::array<TalentNode, kTalentNodeCount>& talent_nodes() {
        static const auto nodes = [] {
            std::array<TalentNode, kTalentNodeCount> result{};
            int i = 0;
            auto add = [&](TalentPath path,
                           int ordinal,
                           int cost,
                           const std::string& name,
                           const std::string& description,
                           TalentBonuses bonus) {
                result[i] = {
                    path, ordinal, cost, ordinal == 0 ? -1 : i - 1, name, description, bonus};
                ++i;
            };
            const int attack[] = {5, 10, 15, 20, 30};
            const int penalty[] = {2, 4, 8, 12, 24};
            for (int branch = 0; branch < 2; ++branch) {
                const auto path = static_cast<TalentPath>(branch);
                for (int n = 0; n < 5; ++n) {
                    TalentBonuses b;
                    std::string desc;
                    if (branch == 0) {
                        b.physical = attack[n];
                        b.defense = -penalty[n];
                        desc = "Min/Max Physical +" + std::to_string(attack[n]) + ", Defense -" +
                               std::to_string(penalty[n]);
                    } else {
                        b.magical = attack[n];
                        b.mana_percent = -penalty[n];
                        desc = "Min/Max Magical +" + std::to_string(attack[n]) + ", Mana -" +
                               std::to_string(penalty[n]) + "%";
                    }
                    add(path, n, 10, branch == 0 ? "Physical Power" : "Magical Power", desc, b);
                }
                for (int n = 0; n < 5; ++n) {
                    TalentBonuses b;
                    b.explosive_percent = 5;
                    add(path,
                        n + 5,
                        10,
                        "Explosive Blow",
                        "Explosive Blow power +" + std::to_string(b.explosive_percent) + "%",
                        b);
                }
            }
            const int defense[] = {10, 25, 50};
            for (int n = 0; n < 3; ++n) {
                TalentBonuses b;
                b.defense = defense[n];
                b.mana = 50 + 25 * n;
                b.max_physical = -5;
                add(TalentPath::Defensive,
                    n,
                    10,
                    "Fortitude",
                    "Defense +" + std::to_string(b.defense) + ", Mana +" + std::to_string(b.mana) +
                        ", Max Physical -5",
                    b);
            }
            for (int n = 3; n < 5; ++n) {
                TalentBonuses b;
                b.hp_per_pulse = 10;
                add(TalentPath::Defensive, n, 10, "Auto HP", "HP +10 every 2 seconds", b);
            }
            for (int n = 0; n < 4; ++n) {
                TalentBonuses b;
                b.mp_per_pulse = 5;
                add(TalentPath::QualityOfLife, n, 5, "Auto MP", "MP +5 every 2 seconds", b);
            }
            for (int n = 4; n < 6; ++n) {
                TalentBonuses b;
                b.pickup_count = n == 4 ? 2 : 4;
                add(TalentPath::QualityOfLife,
                    n,
                    10,
                    "Auto Pick",
                    "Pick up to " + std::to_string(b.pickup_count) +
                        " nearby items every 5 seconds",
                    b);
            }
            for (int n = 0; n < 5; ++n) {
                TalentBonuses b;
                b.speed_percent = 5;
                b.evasion = -(n + 2);
                add(TalentPath::Upgrades,
                    n,
                    5,
                    "Fleet Foot",
                    "Speed +" + std::to_string(b.speed_percent) + "%, Evasion " +
                        std::to_string(b.evasion),
                    b);
            }
            TalentBonuses b;
            b.beacon = true;
            add(TalentPath::Upgrades,
                5,
                10,
                "Beacon",
                "Attract idle monsters within 20 tiles for 30 seconds. Cooldown: 120 seconds. "
                "Never steals another player's target.",
                b);
            return result;
        }();
        return nodes;
    }

    std::uint64_t talent_bit(int node) {
        return node >= 0 && node < kTalentNodeCount ? std::uint64_t{1} << node : 0;
    }
    int talent_cost(std::uint64_t mask) {
        int total = 0;
        for (int i = 0; i < kTalentNodeCount; ++i) {
            if (mask & talent_bit(i)) {
                total += talent_nodes()[i].cost;
            }
        }
        return total;
    }
    bool valid_talents(std::uint64_t mask, int level) {
        if (level < 1 || mask >> kTalentNodeCount || talent_cost(mask) > level) {
            return false;
        }
        for (int i = 0; i < kTalentNodeCount; ++i) {
            const int previous = talent_nodes()[i].prerequisite;
            if ((mask & talent_bit(i)) && previous >= 0 && !(mask & talent_bit(previous))) {
                return false;
            }
        }
        return true;
    }
    bool can_apply_talents(std::uint64_t current, std::uint64_t desired, int level) {
        return (desired & current) == current && valid_talents(desired, level);
    }
    TalentBonuses talent_bonuses(std::uint64_t mask) {
        TalentBonuses total;
        for (int i = 0; i < kTalentNodeCount; ++i) {
            if (!(mask & talent_bit(i))) {
                continue;
            }
            const auto& b = talent_nodes()[i].bonus;
            total.physical += b.physical;
            total.magical += b.magical;
            total.max_physical += b.max_physical;
            total.defense += b.defense;
            total.mana += b.mana;
            total.mana_percent += b.mana_percent;
            total.explosive_percent += b.explosive_percent;
            total.hp_per_pulse += b.hp_per_pulse;
            total.mp_per_pulse += b.mp_per_pulse;
            total.pickup_count += b.pickup_count;
            total.speed_percent += b.speed_percent;
            total.evasion += b.evasion;
            total.beacon = total.beacon || b.beacon;
        }
        return total;
    }
    std::array<int, 5> talent_ranks(std::uint64_t mask) {
        std::array<int, 5> ranks{};
        for (int i = 0; i < kTalentNodeCount - 1; ++i) {
            if (mask & talent_bit(i)) {
                ++ranks[static_cast<int>(talent_nodes()[i].path)];
            }
        }
        return ranks;
    }
    std::vector<std::uint8_t>
    frame_talent_request(TalentCommand command, std::uint64_t expected, std::uint64_t desired) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(command));
        if (command == TalentCommand::Apply) {
            w.write_u64(expected);
            w.write_u64(desired);
        }
        return w.frame(static_cast<std::uint8_t>(C2S::TalentTree));
    }
    bool parse_talent_request(const std::uint8_t* data,
                              std::size_t size,
                              TalentCommand& command,
                              std::uint64_t& expected,
                              std::uint64_t& desired) {
        if (!data) {
            return false;
        }
        PacketReader r(data, size);
        std::uint8_t cmd = 0;
        if (!r.read_u8(cmd) || cmd > static_cast<int>(TalentCommand::Beacon)) {
            return false;
        }
        std::uint64_t e = 0, d = 0;
        if (cmd == static_cast<int>(TalentCommand::Apply) && (!r.read_u64(e) || !r.read_u64(d))) {
            return false;
        }
        if (r.remaining() != 0) {
            return false;
        }
        command = static_cast<TalentCommand>(cmd);
        expected = e;
        desired = d;
        return true;
    }
    std::vector<std::uint8_t> frame_talent_state(const TalentState& state) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(state.status));
        w.write_u64(state.mask);
        w.write_i32(state.level);
        w.write_u32(state.beacon_cooldown_ms);
        w.write_u32(state.beacon_active_ms);
        return w.frame(static_cast<std::uint8_t>(S2C::TalentTree));
    }
    bool parse_talent_state(const std::uint8_t* data, std::size_t size, TalentState& state) {
        if (!data) {
            return false;
        }
        PacketReader r(data, size);
        TalentState s;
        std::uint8_t status = 0;
        if (!r.read_u8(status) || status > static_cast<int>(TalentStatus::Unavailable) ||
            !r.read_u64(s.mask) || !r.read_i32(s.level) || !r.read_u32(s.beacon_cooldown_ms) ||
            !r.read_u32(s.beacon_active_ms) || r.remaining() || !valid_talents(s.mask, s.level) ||
            s.beacon_cooldown_ms > kBeaconCooldownMs || s.beacon_active_ms > kBeaconDurationMs) {
            return false;
        }
        s.status = static_cast<TalentStatus>(status);
        state = s;
        return true;
    }
}  // namespace openkal::protocol
