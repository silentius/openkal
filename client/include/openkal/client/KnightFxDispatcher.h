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

#include "openkal/client/SkillPresenter.h"

#include <unordered_map>
#include <vector>

namespace openkal::client {

    class CombatFx;
    class SpriteFx;

    /** Routes Knight skill mappings through SpriteFx. */
    class KnightFxDispatcher final : public IKnightFxDispatcher {
      public:
        /**
         * Constructs a dispatcher over existing render backends.
         * @param sprite_fx Sprite/FX script renderer.
         * @param combat_fx Combat audio backend.
         */
        KnightFxDispatcher(SpriteFx& sprite_fx, CombatFx& combat_fx);

        /**
         * Dispatches one Knight effect.
         * @param engine Engine used to create particle resources.
         * @param dispatch Effect asset and attachment metadata.
         * @param ends Caster and target endpoints.
         * @return True when the request had a usable entity or world endpoint.
         */
        [[nodiscard]] bool dispatch(engine::IEngine& engine,
                                    const SkillFxDispatch& dispatch,
                                    const SkillPresentEndpoints& ends) override;

        /**
         * Removes prep emitters retained for one caster.
         * @param entity Caster entity key.
         */
        void clear_looping(std::uint32_t entity) override;

        /**
         * Attaches or removes the Perfect Defense duration sphere (`davi_ef120`).
         * Independent of skill-prep looping so later knight fires do not drop it.
         * @param engine Engine used to load particle and clip resources.
         * @param owner Character net id the buff belongs to. 0 with `active` false
         *        drops every attached sphere.
         * @param active True while the Perfect Defense bstate is present.
         * @param x World X at the character origin.
         * @param y World Y.
         * @param z World Z.
         * @param yaw Character facing in radians.
         */
        void set_perfect_defense(engine::IEngine& engine,
                                 std::uint32_t owner,
                                 bool active,
                                 float x,
                                 float y,
                                 float z,
                                 float yaw);

        /**
         * Repositions the Perfect Defense sphere for one character.
         * @param owner Character net id.
         * @param x World X at the character origin.
         * @param y World Y.
         * @param z World Z.
         * @param yaw Character facing in radians.
         */
        void follow_perfect_defense(std::uint32_t owner, float x, float y, float z, float yaw);

      private:
        /** Sprite/FX script renderer; not owned. */
        SpriteFx* m_sprite_fx = nullptr;

        /** Combat audio backend; not owned. */
        CombatFx* m_combat_fx = nullptr;

        /** Synthetic SpriteFx keys retained per caster during skill preparation. */
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_looping_entities;

        /** SpriteFx / clip-follow keys for Perfect Defense, keyed by owner net id. */
        std::unordered_map<std::uint32_t, std::uint32_t> m_perfect_defense_entities;

        /** Next synthetic SpriteFx entity key. */
        std::uint32_t m_next_entity = 0xE0000000u;

        /** Next synthetic key for Perfect Defense (away from prep looping). */
        std::uint32_t m_next_buff_entity = 0xD1000000u;
    };

}  // namespace openkal::client
