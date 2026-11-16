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

    /** Routes Archer skill mappings through SpriteFx. */
    class ArcherFxDispatcher final : public IArcherFxDispatcher {
      public:
        /**
         * Constructs a dispatcher over existing render backends.
         * @param sprite_fx Sprite/FX script renderer.
         * @param combat_fx Combat audio backend.
         */
        ArcherFxDispatcher(SpriteFx& sprite_fx, CombatFx& combat_fx);

        /**
         * Dispatches one Archer effect.
         * @param engine Engine used to create particle resources.
         * @param dispatch Effect asset and attachment metadata.
         * @param ends Caster and target endpoints.
         * @return True when the request had a usable entity or world endpoint.
         */
        [[nodiscard]] bool dispatch(engine::IEngine& engine,
                                    const SkillFxDispatch& dispatch,
                                    const SkillPresentEndpoints& ends) override;

        /**
         * Advances effects deferred until an Archer projectile reaches its target.
         * @param engine Engine used to create particle resources.
         * @param dt Elapsed time in seconds.
         */
        void update(engine::IEngine& engine, float dt);

        /**
         * Follows Arrow Explosion fuses attached to one target.
         * @param entity Target entity key.
         * @param x World X.
         * @param y World Y.
         * @param z World Z.
         */
        void follow_target(std::uint32_t entity, float x, float y, float z);

        /**
         * Removes prep emitters retained for one caster.
         * @param entity Caster entity key.
         */
        void clear_looping(std::uint32_t entity) override;

      private:
        /** One effect scheduled for the arrow-impact time. */
        struct PendingImpact {
            /** Effect asset and attachment metadata. */
            SkillFxDispatch dispatch;

            /** Caster and target endpoints captured for the projectile. */
            SkillPresentEndpoints ends;

            /** Seconds remaining before impact. */
            float remaining_sec = 0.0f;
        };

        /** One compatibility behavior fuse waiting to burst. */
        struct ExploderFuse {
            /** Caster that planted the fuse. */
            std::uint32_t caster_entity = 0;

            /** Fuse target. */
            std::uint32_t target_entity = 0;

            /** Synthetic SpriteFx entity for `davi_ef131_04`. */
            std::uint32_t fx_entity = 0;

            /** Seconds remaining before `davi_ef131_05`. */
            float remaining_sec = 0.0f;

            /** Last follow X. */
            float x = 0.0f;

            /** Last follow Y. */
            float y = 0.0f;

            /** Last follow Z. */
            float z = 0.0f;
        };

        /**
         * Dispatches an effect immediately without projectile-delay handling.
         * @param engine Engine used to create particle resources.
         * @param dispatch Effect asset and attachment metadata.
         * @param ends Caster and target endpoints.
         * @return True when the request had a usable entity or world endpoint.
         */
        [[nodiscard]] bool dispatch_now(engine::IEngine& engine,
                                        const SkillFxDispatch& dispatch,
                                        const SkillPresentEndpoints& ends);

        /** Sprite/FX script renderer; not owned. */
        SpriteFx* m_sprite_fx = nullptr;

        /** Combat audio backend; not owned. */
        CombatFx* m_combat_fx = nullptr;

        /** Synthetic SpriteFx keys retained per caster during skill preparation. */
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_looping_entities;

        /** Target effects waiting for their arrows to reach the impact point. */
        std::vector<PendingImpact> m_pending_impacts;

        /** Active Arrow Explosion fuses. */
        std::vector<ExploderFuse> m_exploder_fuses;

        /** Next synthetic SpriteFx entity key. */
        std::uint32_t m_next_entity = 0xD0000000u;
    };

}  // namespace openkal::client
