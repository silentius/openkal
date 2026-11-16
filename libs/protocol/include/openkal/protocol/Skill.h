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

#include "openkal/protocol/ItemMonster.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Beheading requires the same close approach as opening a wooden box. */
    inline constexpr int kBeheadRange = kItemMonsterOpenRange;

    /** S2C_ACTION type: generic / common. */
    inline constexpr std::uint8_t kAtCommon = 0;

    /** S2C_ACTION type: rest sit/stand. */
    inline constexpr std::uint8_t kAtRest = 3;

    /** S2C_ACTION type: skill presentation. */
    inline constexpr std::uint8_t kAtSkill = 5;

    /** S2C_ACTION type: Maguni kneel (beheadable corpse). */
    inline constexpr std::uint8_t kAtKnee = 8;

    /** S2C_ACTION type: death. */
    inline constexpr std::uint8_t kAtDie = 9;

    /** S2C_ACTION type: Maguni beheaded. */
    inline constexpr std::uint8_t kAtBeheaded = 10;

    /** S2C_ACTION type: transform wind-up (AT_PRETRANSFORM). */
    inline constexpr std::uint8_t kAtPreTransform = 12;

    /** S2C_ACTION type: transform apply/revert (AT_TRANSFORM). */
    inline constexpr std::uint8_t kAtTransform = 13;

    /** Decoded S2C_SKILL fields (intentional wire DTO). */
    struct S2CSkillFields {
        /** Skill index. */
        std::uint8_t skill = 0;

        /** Caster entity id. */
        std::uint32_t caster = 0;

        /** Target entity id. */
        std::uint32_t target = 0;

        /** True when the damage/palsy/flags tail is present. */
        bool has_damage = false;

        /** Skill level byte from the wire. */
        std::uint8_t level = 0;

        /** Hit damage when has_damage. */
        std::int32_t damage = 0;

        /** Palsy / status payload when has_damage. */
        std::int32_t palsy = 0;

        /** Hit flags when has_damage. */
        std::uint8_t flags = 0;
    };

    /** One learned skill as carried by S2C_SKILLINFO / S2C_SKILLUP. */
    struct SkillRankEntry {
        /** Skill index within the class. */
        std::uint8_t index = 0;

        /** Learned skill level. */
        std::uint8_t level = 0;
    };

    /** Encodes and decodes skill and action messages. */
    class Skill final {
      public:
        Skill() = delete;

        /**
         * Builds C2S_SKILL for a self-cast (skill index only).
         * @param skill Skill index.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_self(std::uint8_t skill);

        /**
         * Builds C2S_SKILL boost toggle (leading 0 + enable byte).
         * @param enable Non-zero to enable boost.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_boost(std::uint8_t enable);

        /**
         * Builds C2S_SKILL for a targeted cast.
         * @param skill Skill index.
         * @param kind Target kind (player/monster).
         * @param target Target entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_target(std::uint8_t skill, std::uint8_t kind, std::uint32_t target);

        /**
         * Builds C2S_SKILL for a ground-coordinate cast.
         * @param skill Skill index.
         * @param x Server world X.
         * @param y Server world Y.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_ground(std::uint8_t skill, std::int32_t x, std::int32_t y);

        /**
         * Reads the skill index and returns the remaining payload for target parsing.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param skill Out skill index.
         * @param rest Out pointer to bytes after the skill index.
         * @param rest_size Out length of rest.
         * @return True when at least one skill byte is present.
         */
        [[nodiscard]] static bool parse_client_header(const std::uint8_t* data,
                                                      std::size_t size,
                                                      std::uint8_t& skill,
                                                      const std::uint8_t*& rest,
                                                      std::size_t& rest_size);

        /**
         * Parses the targeted-cast tail (kind + target id).
         * @param data Rest bytes from parse_client_header.
         * @param size Rest length.
         * @param kind Out target kind.
         * @param target Out target entity id.
         * @return True when the tail is valid.
         */
        [[nodiscard]] static bool parse_client_target(const std::uint8_t* data,
                                                      std::size_t size,
                                                      std::uint8_t& kind,
                                                      std::uint32_t& target);

        /**
         * Parses the ground-coordinate cast tail.
         * @param data Rest bytes from parse_client_header.
         * @param size Rest length.
         * @param x Out server world X.
         * @param y Out server world Y.
         * @return True when exactly two coordinates are available.
         */
        [[nodiscard]] static bool parse_client_ground(const std::uint8_t* data,
                                                      std::size_t size,
                                                      std::int32_t& x,
                                                      std::int32_t& y);

        /**
         * Builds S2C_SKILL without a damage tail.
         * @param skill Skill index.
         * @param caster Caster entity id.
         * @param target Target entity id.
         * @param level Skill level byte.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server(std::uint8_t skill,
                                                                    std::uint32_t caster,
                                                                    std::uint32_t target,
                                                                    std::uint8_t level);

        /**
         * Builds S2C_SKILL with a damage/palsy/flags tail.
         * @param skill Skill index.
         * @param caster Caster entity id.
         * @param target Target entity id.
         * @param level Skill level byte.
         * @param damage Hit damage.
         * @param palsy Palsy / status payload.
         * @param flags Hit flags.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server_damage(std::uint8_t skill,
                                                                           std::uint32_t caster,
                                                                           std::uint32_t target,
                                                                           std::uint8_t level,
                                                                           std::int32_t damage,
                                                                           std::int32_t palsy,
                                                                           std::uint8_t flags);

        /**
         * Parses S2C_SKILL payload into fields.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param out Out decoded fields.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_server(const std::uint8_t* data, std::size_t size, S2CSkillFields& out);

        /**
         * Builds C2S_PRESKILL (cast telegraph).
         * @param skill Skill index.
         * @param target Target entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_preskill(std::uint8_t skill,
                                                                      std::uint32_t target);

        /**
         * Parses C2S_PRESKILL payload (`bd` skill + target id).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param skill Out skill index.
         * @param target Out target entity id.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_preskill(const std::uint8_t* data,
                                                 std::size_t size,
                                                 std::uint8_t& skill,
                                                 std::uint32_t& target);

        /**
         * Builds C2S_REST.
         * @param rest Non-zero to sit / rest.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_rest(std::uint8_t rest);

        /**
         * Parses C2S_REST payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param rest Out rest flag.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_rest(const std::uint8_t* data, std::size_t size, std::uint8_t& rest);

        /**
         * Builds S2C_ACTION rest (sit/stand) for an entity.
         * @param id Entity id.
         * @param rest Non-zero when sitting.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_rest(std::uint32_t id,
                                                                         std::uint8_t rest);

        /**
         * Builds S2C_ACTION Maguni kneel for an entity.
         * @param id Entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_knee(std::uint32_t id);

        /**
         * Builds S2C_ACTION death for an entity.
         * @param id Entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_die(std::uint32_t id);

        /**
         * Builds S2C_ACTION Maguni beheaded for an entity.
         * @param id Entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_action_beheaded(std::uint32_t id);

        /**
         * Builds S2C_ACTION AT_SKILL telegraph (`dbbd` id, action, skill, target).
         * @param id Caster entity id.
         * @param skill Skill index.
         * @param target Target entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_action_skill(std::uint32_t id, std::uint8_t skill, std::uint32_t target);

        /**
         * Parses S2C_ACTION AT_SKILL payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param id Out caster entity id.
         * @param skill Out skill index.
         * @param target Out target entity id.
         * @return True when the payload is an AT_SKILL telegraph.
         */
        [[nodiscard]] static bool parse_action_skill(const std::uint8_t* data,
                                                     std::size_t size,
                                                     std::uint32_t& id,
                                                     std::uint8_t& skill,
                                                     std::uint32_t& target);

        /**
         * Builds C2S_REVIVAL (empty payload).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_revival();

        /**
         * Builds C2S_SAVEREVIVALPT (`bd` point id + NPC entity id).
         * @param point Revival-point id selected by the statue dialog.
         * @param npc_id Statue entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_save_revival_point(std::uint8_t point, std::uint32_t npc_id);

        /**
         * Parses S2C_ACTION payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param id Out entity id.
         * @param action Out action type (kAt*).
         * @param arg Out optional argument (e.g. rest flag); 0 when absent.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_action(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint32_t& id,
                                               std::uint8_t& action,
                                               std::uint8_t& arg);

        /**
         * Builds S2C_SKILLUP ("bb" index, level).
         * @param index Skill index.
         * @param level Learned skill level.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_skill_up(std::uint8_t index,
                                                                      std::uint8_t level);

        /**
         * Builds C2S_LEARNSKILL / C2S_SKILLUP ("b" skill index).
         * @param skill Skill index.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_learn_skill(std::uint8_t skill);

        /**
         * Builds C2S_SKILLUP ("b" skill index).
         * @param skill Skill index.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_skill_up(std::uint8_t skill);

        /**
         * Parses a one-byte skill-index C2S payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param skill Out skill index.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_client_skill_index(const std::uint8_t* data, std::size_t size, std::uint8_t& skill);

        /**
         * Parses S2C_SKILLUP payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param index Out skill index.
         * @param level Out learned skill level.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_skill_up(const std::uint8_t* data,
                                                 std::size_t size,
                                                 std::uint8_t& index,
                                                 std::uint8_t& level);

        /**
         * Builds S2C_SKILLINFO ("b" count + count * "bb").
         * @param entries Learned skill ranks.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_skill_info(const std::vector<SkillRankEntry>& entries);

        /**
         * Parses S2C_SKILLINFO payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param entries Out learned skill ranks.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_skill_info(const std::uint8_t* data,
                                                   std::size_t size,
                                                   std::vector<SkillRankEntry>& entries);
    };

    /**
     * Builds C2S_SKILL self-cast (free wrapper for Skill::frame_client_self).
     * @param skill Skill index.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_skill_self(std::uint8_t skill);

    /**
     * Builds C2S_SKILL boost toggle (free wrapper for Skill::frame_client_boost).
     * @param enable Non-zero to enable boost.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_skill_boost(std::uint8_t enable);

    /**
     * Builds C2S_SKILL targeted cast (free wrapper for Skill::frame_client_target).
     * @param skill Skill index.
     * @param kind Target kind.
     * @param target Target entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_c2s_skill_target(std::uint8_t skill, std::uint8_t kind, std::uint32_t target);

    /**
     * Parses C2S_SKILL header (free wrapper for Skill::parse_client_header).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param skill Out skill index.
     * @param rest Out pointer to bytes after the skill index.
     * @param rest_size Out length of rest.
     * @return True when at least one skill byte is present.
     */
    [[nodiscard]] bool parse_c2s_skill_header(const std::uint8_t* data,
                                              std::size_t size,
                                              std::uint8_t& skill,
                                              const std::uint8_t*& rest,
                                              std::size_t& rest_size);

    /**
     * Parses C2S_SKILL target tail (free wrapper for Skill::parse_client_target).
     * @param data Rest bytes.
     * @param size Rest length.
     * @param kind Out target kind.
     * @param target Out target entity id.
     * @return True when the tail is valid.
     */
    [[nodiscard]] bool parse_c2s_skill_target_rest(const std::uint8_t* data,
                                                   std::size_t size,
                                                   std::uint8_t& kind,
                                                   std::uint32_t& target);

    /**
     * Builds S2C_SKILL without damage (free wrapper for Skill::frame_server).
     * @param skill Skill index.
     * @param caster Caster entity id.
     * @param target Target entity id.
     * @param level Skill level byte.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_skill(std::uint8_t skill,
                                                            std::uint32_t caster,
                                                            std::uint32_t target,
                                                            std::uint8_t level);

    /**
     * Builds S2C_SKILL with damage (free wrapper for Skill::frame_server_damage).
     * @param skill Skill index.
     * @param caster Caster entity id.
     * @param target Target entity id.
     * @param level Skill level byte.
     * @param damage Hit damage.
     * @param palsy Palsy / status payload.
     * @param flags Hit flags.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_skill_damage(std::uint8_t skill,
                                                                   std::uint32_t caster,
                                                                   std::uint32_t target,
                                                                   std::uint8_t level,
                                                                   std::int32_t damage,
                                                                   std::int32_t palsy,
                                                                   std::uint8_t flags);

    /**
     * Parses S2C_SKILL (free wrapper for Skill::parse_server).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param out Out decoded fields.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool
    parse_s2c_skill(const std::uint8_t* data, std::size_t size, S2CSkillFields& out);

    /**
     * Builds C2S_PRESKILL (free wrapper for Skill::frame_preskill).
     * @param skill Skill index.
     * @param target Target entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_preskill(std::uint8_t skill,
                                                               std::uint32_t target);

    /**
     * Parses C2S_PRESKILL (free wrapper for Skill::parse_preskill).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param skill Out skill index.
     * @param target Out target entity id.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_c2s_preskill(const std::uint8_t* data,
                                          std::size_t size,
                                          std::uint8_t& skill,
                                          std::uint32_t& target);

    /**
     * Builds S2C_ACTION AT_SKILL (free wrapper for Skill::frame_action_skill).
     * @param id Caster entity id.
     * @param skill Skill index.
     * @param target Target entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_action_skill(std::uint32_t id, std::uint8_t skill, std::uint32_t target);

    /**
     * Builds C2S_REST (free wrapper for Skill::frame_rest).
     * @param rest Non-zero to sit / rest.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_rest(std::uint8_t rest);

    /**
     * Parses C2S_REST (free wrapper for Skill::parse_rest).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param rest Out rest flag.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool
    parse_c2s_rest(const std::uint8_t* data, std::size_t size, std::uint8_t& rest);

    /**
     * Builds S2C_ACTION rest (free wrapper for Skill::frame_action_rest).
     * @param id Entity id.
     * @param rest Non-zero when sitting.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_action_rest(std::uint32_t id,
                                                                  std::uint8_t rest);

    /**
     * Builds S2C_ACTION Maguni kneel (free wrapper for Skill::frame_action_knee).
     * @param id Entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_action_knee(std::uint32_t id);

    /**
     * Builds S2C_ACTION death (free wrapper for Skill::frame_action_die).
     * @param id Entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_action_die(std::uint32_t id);

    /**
     * Builds S2C_ACTION Maguni beheaded (free wrapper for Skill::frame_action_beheaded).
     * @param id Entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_action_beheaded(std::uint32_t id);

    /**
     * Builds C2S_REVIVAL (free wrapper for Skill::frame_client_revival).
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_revival();

    /**
     * Parses S2C_ACTION (free wrapper for Skill::parse_action).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param id Out entity id.
     * @param action Out action type (kAt*).
     * @param arg Out optional argument; 0 when absent.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_s2c_action(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& id,
                                        std::uint8_t& action,
                                        std::uint8_t& arg);

}  // namespace openkal::protocol
