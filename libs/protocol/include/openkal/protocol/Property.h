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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Message identifiers used with S2C_MESSAGEV "bb". */
    enum class InfoMsg : std::uint8_t {
        /** MSG_SPECIALTYUP — payload is the new runtime state bitmask. */
        SpecialtyUp = 37,
    };

    /** New-player wandering bit (compatibility behavior starts at 0 in RAM; DB row is 1). */
    inline constexpr std::uint32_t kStartingSpecialty = 1u;

    /** First government job (Apprentice Knight / …).
     * Granted by the government-position quest around level 30.
     */
    inline constexpr std::uint32_t kSpecialtyFirstJob = 3u;

    /** Second-job path A (Knight Vagabond / Mage Hermit / …).
     * Chosen at level 50.
     */
    inline constexpr std::uint32_t kSpecialtySecondJobA = 7u;

    /** Second-job path B (Knight Commander / Mage Chairperson of Joong-Bang / …).
     * Chosen at level 50 (mutually exclusive with path A).
     */
    inline constexpr std::uint32_t kSpecialtySecondJobB = 11u;

    /** Third-job path A (Knight God of Sword / …).
     * Follows second-job path A around level 70.
     */
    inline constexpr std::uint32_t kSpecialtyThirdJobA = 23u;

    /** Third-job path B (Knight General / …).
     * Follows second-job path B around level 70.
     */
    inline constexpr std::uint32_t kSpecialtyThirdJobB = 43u;

    /**
     * Government-position title index from a specialty bitmask (highest set bit).
     * @param specialty Job bitmask (bit 0 = wandering, bit 1 = first job).
     * @return Index into config/jobs title tables.
     */
    [[nodiscard]] inline int specialty_title_index(std::uint32_t specialty) {
        int index = 0;
        while (specialty > 1u) {
            specialty >>= 1u;
            ++index;
        }
        return index;
    }

    /**
     * True when every bit of a skill grade mask is set in the player specialty.
     * Matches skill definition grade vs compatibility behavior.
     * @param specialty Player job bitmask.
     * @param grade Skill grade mask (1 / 3 / 7 / 11 / 23 / 43).
     * @return True when the player may use that grade.
     */
    [[nodiscard]] inline bool specialty_meets_grade(std::uint32_t specialty, std::uint32_t grade) {
        return grade != 0u && (specialty & grade) == grade;
    }

    /** Property identifiers in update packets.
     * Gaps through ResPalsy keep PuPoint/SuPoint/Exp/Level at 23–26.
     */
    enum class PropertyType : std::uint8_t {
        Str = 0,
        Hth,
        Int,
        Wis,
        Dex,
        Hp,
        Mp,
        CurHp,
        CurMp,
        Hit,
        Dodge,
        MinAttack,
        MaxAttack,
        MinMagic,
        MaxMagic,
        Defense,
        Absorb,
        ASpeed,
        ResFire,
        ResIce,
        ResLitning,
        ResCurse,
        ResPalsy,
        PuPoint,
        SuPoint,
        Exp,
        Level,
        Contribute = 30,
    };

    /** P_MSPEED: signed move-speed percent. */
    inline constexpr std::uint8_t kPropertyMSpeed = 32;

    /** Walk/run multiplier from the signed movement-speed percentage.
     * @param speed_value Signed percent (transform, haste, ice slow).
     * @return Factor applied to 32000/tick speed.
     */
    [[nodiscard]] inline float move_speed_factor(std::int16_t speed_value) {
        if (speed_value < 0) {
            const int den = (100 - static_cast<int>(speed_value) < 1)
                                ? 1
                                : (100 - static_cast<int>(speed_value));
            return 100.0f / static_cast<float>(den);
        }
        if (speed_value > 0) {
            return static_cast<float>(speed_value + 100) / 100.0f;
        }
        return 1.0f;
    }

    /** P_RAGE: transform rage gauge. */
    inline constexpr std::uint8_t kPropertyRage = 35;

    /** P_MORTAL — Knight Brutal Attack charge counter. */
    inline constexpr std::uint8_t kPropertyMortal = 36;

    /** Discriminator for multi-format S2C_UPDATEPROPERTY payloads. */
    enum class UpdatePropertyKind : std::uint8_t {
        Word = 0,
        Level = 1,
        Exp = 2,
        Vital = 3,
    };

    /** Decoded S2C_UPDATEPROPERTY fields. */
    struct UpdatePropertyFields {
        /** Property id. */
        std::uint8_t property = 0;
        /** Payload kind. */
        UpdatePropertyKind kind = UpdatePropertyKind::Word;
        /** Word value ("bd") for most properties including PU/SU/CurHp/CurMp. */
        std::int32_t word_value = 0;
        /** Level ("bd"). */
        std::int32_t level_value = 0;
        /** Current EXP after change ("bII"). */
        std::int64_t exp_current = 0;
        /** Signed EXP delta ("bII"). */
        std::int64_t exp_delta = 0;
        /** Current HP or MP for Vital ("bdd"). */
        std::int32_t vital_cur = 0;
        /** Max HP or MP for Vital ("bdd"). */
        std::int32_t vital_max = 0;
    };

    /** Encodes and decodes property messages. */
    class Property final {
      public:
        Property() = delete;

        /**
         * Frames a client UpdateProperty spend request.
         * @param property Property id (Str..Dex).
         * @param count Number of requested raises. One uses the legacy one-byte payload.
         * @return Framed C2S packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s_update(std::uint8_t property,
                                                                        std::uint8_t count = 1);

        /**
         * Parses a client UpdateProperty spend request.
         * @param data Payload.
         * @param size Size.
         * @param property Out property id.
         * @param count Out requested raise count.
         * @return True on success.
         */
        [[nodiscard]] static bool parse_c2s_update(const std::uint8_t* data,
                                                   std::size_t size,
                                                   std::uint8_t& property,
                                                   std::uint8_t& count);

        /** Parses a legacy or batched request while discarding its count.
         * @param data Payload.
         * @param size Size.
         * @param property Out property id.
         * @return True on success.
         */
        [[nodiscard]] static bool
        parse_c2s_update(const std::uint8_t* data, std::size_t size, std::uint8_t& property);

        /**
         * Frames a scalar UpdateProperty ("bd").
         * @param property Property id.
         * @param value Int32 value.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_update(std::uint8_t property,
                                                                    std::int32_t value);

        /**
         * Frames a level UpdateProperty ("bd").
         * @param level New level.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_update_level(std::int32_t level);

        /**
         * Frames an EXP UpdateProperty ("bII").
         * @param current EXP after the change.
         * @param delta Signed delta applied.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_update_exp(std::int64_t current,
                                                                        std::int64_t delta);

        /**
         * Frames a PU/SU word update.
         * @param property Must be PuPoint or SuPoint.
         * @param value Point total.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_update_points(std::uint8_t property,
                                                                           std::int32_t value);

        /**
         * Frames an HP or MP UpdateProperty ("bdd" = cur, max).
         * @param property Must be Hp or Mp.
         * @param current Current value.
         * @param maximum Max value.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_update_vital(std::uint8_t property, std::int32_t current, std::int32_t maximum);

        /**
         * Parses a scalar UpdateProperty ("bd") — legacy helper.
         * @param data Payload.
         * @param size Size.
         * @param property Out property.
         * @param value Out int32.
         * @return True when payload is a word update.
         */
        [[nodiscard]] static bool parse_update(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint8_t& property,
                                               std::int32_t& value);

        /**
         * Parses any supported UpdateProperty format.
         * @param data Payload.
         * @param size Size.
         * @param out Decoded fields.
         * @return True on success.
         */
        [[nodiscard]] static bool
        parse_update_any(const std::uint8_t* data, std::size_t size, UpdatePropertyFields& out);

        /**
         * Frames S2C_UPDATEMPROPERTY "dbw" for P_MSPEED.
         * @param entity_id Target entity id.
         * @param type Property id (`kPropertyMSpeed`).
         * @param value Signed percent (`runtime state`).
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_update_mproperty(std::uint32_t entity_id, std::uint8_t type, std::int16_t value);

        /**
         * Parses S2C_UPDATEMPROPERTY "dbw".
         * @param data Payload.
         * @param size Payload size.
         * @param entity_id Out entity id.
         * @param type Out property id.
         * @param value Out signed percent.
         * @return True on success.
         */
        [[nodiscard]] static bool parse_update_mproperty(const std::uint8_t* data,
                                                         std::size_t size,
                                                         std::uint32_t& entity_id,
                                                         std::uint8_t& type,
                                                         std::int16_t& value);
    };

    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_update_property(std::uint8_t property);
    [[nodiscard]] bool
    parse_c2s_update_property(const std::uint8_t* data, std::size_t size, std::uint8_t& property);
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_update_property(std::uint8_t property,
                                                                      std::int32_t value);
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_update_property_level(std::int32_t level);
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_update_property_exp(std::int64_t current,
                                                                          std::int64_t delta);
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_update_property_vital(std::uint8_t property,
                                                                            std::int32_t current,
                                                                            std::int32_t maximum);
    [[nodiscard]] bool parse_s2c_update_property(const std::uint8_t* data,
                                                 std::size_t size,
                                                 std::uint8_t& property,
                                                 std::int32_t& value);
    [[nodiscard]] bool parse_s2c_update_property_any(const std::uint8_t* data,
                                                     std::size_t size,
                                                     UpdatePropertyFields& out);

}  // namespace openkal::protocol
