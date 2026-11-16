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
#include <string>
#include <vector>

namespace openkal::protocol {
    /** Ordered branches of the project-owned talent tree. */
    enum class TalentPath : std::uint8_t { Physical, Magical, Defensive, QualityOfLife, Upgrades };

    inline constexpr int kTalentNodeCount = 37;
    /** Project-owned passive indices; distinct from common transformation skills 200–202. */
    inline constexpr int kTalentSkillFirst = 240;
    inline constexpr int kTalentSkillCount = 5;
    inline constexpr int kTalentSkillLast = kTalentSkillFirst + kTalentSkillCount - 1;
    /** Equippable self-buff unlocked by the final Upgrades node. */
    inline constexpr int kBeaconSkill = 205;
    inline constexpr int kBeaconRadiusPixels = 20 * 32;
    inline constexpr std::uint64_t kBeaconDurationMs = 30000;
    inline constexpr std::uint64_t kBeaconCooldownMs = 120000;

    /** Additive modifiers; percentage fields contain percentage points. */
    struct TalentBonuses {
        int physical = 0;
        int magical = 0;
        int max_physical = 0;
        int defense = 0;
        int mana = 0;
        int mana_percent = 0;
        int explosive_percent = 0;
        int hp_per_pulse = 0;
        int mp_per_pulse = 0;
        int pickup_count = 0;
        int speed_percent = 0;
        int evasion = 0;
        bool beacon = false;
    };

    /** One immutable node and its immediately preceding prerequisite. */
    struct TalentNode {
        TalentPath path = TalentPath::Physical;
        int ordinal = 0;
        int cost = 0;
        int prerequisite = -1;
        std::string name;
        std::string description;
        TalentBonuses bonus;
    };

    /** @return Project-owned node definitions in stable bit/index order. */
    const std::array<TalentNode, kTalentNodeCount>& talent_nodes();

    /** @param node Node index.
     * @return Selection bit, or zero for an invalid index. */
    [[nodiscard]] std::uint64_t talent_bit(int node);

    /** @param mask Selected nodes.
     * @return Total point cost. */
    [[nodiscard]] int talent_cost(std::uint64_t mask);

    /** @param mask Selection to validate.
     * @param level Character level, including the initial level-one point.
     * @return Whether all nodes, prerequisites and the budget are valid. */
    [[nodiscard]] bool valid_talents(std::uint64_t mask, int level);

    /** @param current Previously committed selection.
     * @param desired Requested selection.
     * @param level Character level.
     * @return Whether the request only adds valid, affordable nodes. */
    [[nodiscard]] bool can_apply_talents(std::uint64_t current, std::uint64_t desired, int level);

    /** @param mask Valid selected nodes.
     * @return Sum of selected modifiers. */
    [[nodiscard]] TalentBonuses talent_bonuses(std::uint64_t mask);

    /** @param mask Selected nodes.
     * @return Passive ranks for each branch, excluding the Beacon buff node. */
    [[nodiscard]] std::array<int, 5> talent_ranks(std::uint64_t mask);

    /** Commands use a separate appended opcode; existing ordinals stay stable. */
    enum class TalentCommand : std::uint8_t { Snapshot, Apply, Beacon };
    enum class TalentStatus : std::uint8_t { Ok, Invalid, Stale, SaveFailed, Unavailable };

    /** Authoritative selection and remaining Beacon times. */
    struct TalentState {
        std::uint64_t mask = 0;
        int level = 1;
        std::uint32_t beacon_cooldown_ms = 0;
        std::uint32_t beacon_active_ms = 0;
        TalentStatus status = TalentStatus::Ok;
    };

    /** @param command Request kind.
     * @param expected Previously observed selection.
     * @param desired Complete new selection for Apply.
     * @return Framed client request. */
    [[nodiscard]] std::vector<std::uint8_t> frame_talent_request(TalentCommand command,
                                                                 std::uint64_t expected = 0,
                                                                 std::uint64_t desired = 0);

    /** @param data Payload bytes.
     * @param size Payload length.
     * @param command Decoded command.
     * @param expected Decoded previous selection.
     * @param desired Decoded requested selection.
     * @return Whether the entire request is valid. */
    [[nodiscard]] bool parse_talent_request(const std::uint8_t* data,
                                            std::size_t size,
                                            TalentCommand& command,
                                            std::uint64_t& expected,
                                            std::uint64_t& desired);

    /** @param state Authoritative state.
     * @return Framed server state. */
    [[nodiscard]] std::vector<std::uint8_t> frame_talent_state(const TalentState& state);

    /** @param data Payload bytes.
     * @param size Payload length.
     * @param state Decoded state, unchanged on failure.
     * @return Whether the entire state is valid. */
    [[nodiscard]] bool
    parse_talent_state(const std::uint8_t* data, std::size_t size, TalentState& state);
}  // namespace openkal::protocol
