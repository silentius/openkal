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

#include "openkal/client/Player.h"
#include "openkal/engine/engine.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    class AnimatedActor;
    class SpriteFx;

    /**
     * Client-only Beads of Fire preview: toggles armor parts and attaches
     * `ef_setitem_*` loop FX (no stats / server).
     */
    class BofFx {
      public:
        /** Armor slots that can carry a Bead of Fire. */
        enum class Part : int {
            Helmet = 0,
            Chest = 1,
            Gloves = 2,
            Pants = 3,
            Boots = 4,
            Count = 5,
        };

        /** Looping FX entity key (bit 30 set, away from net ids). */
        static constexpr std::uint32_t kEntity = 0xD0000002u;

        /** Pieces required for the half-set FX (`01…`). */
        static constexpr int kHalfCount = 3;

        /** Pieces required for the full-set FX (`02…`). */
        static constexpr int kFullCount = 5;

        BofFx() = default;

        /**
         * Maps a chat token to an armor part.
         * @param token Part name (gloves, chest, …).
         * @param out Receives the part when true.
         * @return True when the token is a known part alias.
         */
        [[nodiscard]] static bool parse_part(std::string_view token, Part& out);

        /**
         * Builds `ef_setitem_XXYYZZ` for the local class.
         * @param full_set 1 = half, 2 = full.
         * @param level Armor tier (46, 50, 55, 60, 65).
         * @param clazz Player class (job digit).
         * @return FX shader stem.
         */
        [[nodiscard]] static std::string fx_name(int full_set, int level, Class clazz);

        /** @return Active BOF tier used for the FX name. */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /**
         * Sets the BOF armor tier used for FX lookup.
         * @param level Shipped FX tier (46, 50, 55, 60, 65).
         * @return True when the level is supported.
         */
        [[nodiscard]] bool set_level(int level);

        /**
         * Toggles one armor part on/off.
         * @param part Armor part.
         * @return New enabled state.
         */
        bool toggle_part(Part part);

        /**
         * Sets every BOF armor part on or off.
         * @param enabled Desired state.
         */
        void set_all(bool enabled);

        /** Clears every part (removes the FX on next sync). */
        void clear();

        /** @return Number of parts currently toggled on. */
        [[nodiscard]] int enabled_count() const;

        /**
         * Half/full/none from the current toggle count.
         * @return 0 = none, 1 = half, 2 = full.
         */
        [[nodiscard]] int set_completeness() const;

        /**
         * Human-readable status line for chat.
         * @return Status text.
         */
        [[nodiscard]] std::string status_line() const;

        /**
         * Rebuilds the looping FX from the current toggles or wear state.
         * @param engine Engine used to load particles.
         * @param sprite_fx Sprite / FX runtime.
         * @param clazz Local player class.
         */
        void sync(engine::IEngine& engine, SpriteFx& sprite_fx, Class clazz);

        /**
         * Applies server/wear BOF set state (clears debug override).
         * @param completeness 0 none, 1 half, 2 full.
         * @param fx_level Snapped FX tier (46–65).
         * @param engine Engine used to load particles.
         * @param sprite_fx Sprite / FX runtime.
         * @param clazz Player class.
         */
        void apply_wear(int completeness,
                        int fx_level,
                        engine::IEngine& engine,
                        SpriteFx& sprite_fx,
                        Class clazz);

        /**
         * Marks chat `/bof` as owning the FX until apply_wear or clear.
         */
        void set_debug_override(bool enabled);

        /** @return True when `/bof` is driving the FX. */
        [[nodiscard]] bool debug_override() const {
            return m_debug_override;
        }

        /**
         * Follows EFFECTBONE slots on the local player pose.
         * @param sprite_fx Sprite / FX runtime.
         * @param actor Local animated actor (last pose).
         * @param model Player model matrix.
         * @param clazz Local player class.
         * @param feet_x Player origin X (slot -1).
         * @param feet_y Player origin Y.
         * @param feet_z Player origin Z.
         */
        void follow(SpriteFx& sprite_fx,
                    const AnimatedActor& actor,
                    const engine::Mat4& model,
                    Class clazz,
                    float feet_x,
                    float feet_y,
                    float feet_z) const;

        /**
         * Detaches the preview FX.
         * @param sprite_fx Sprite / FX runtime.
         */
        void detach(SpriteFx& sprite_fx);

      private:
        /** Part display name for status / chat. */
        [[nodiscard]] static const char* part_name(Part part);

        /** True when `level` is a shipped BOF tier. */
        [[nodiscard]] static bool is_supported_level(int level);

        /** Per-part BOF toggle flags. */
        std::array<bool, static_cast<std::size_t>(Part::Count)> m_parts{};

        /** Active FX tier (default 60). */
        int m_level = 60;

        /** Last attached FX stem (empty when none). */
        std::string m_attached;

        /** True when `/bof` owns the FX instead of worn gear. */
        bool m_debug_override = false;

        /** Wear completeness when not using part toggles (-1 = use toggles). */
        int m_wear_completeness = -1;
    };

}  // namespace openkal::client
