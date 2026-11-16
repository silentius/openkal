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

#include "openkal/client/CombatFx.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/Skills.h"
#include "openkal/engine/engine.h"

namespace openkal::client {

    /** Cast presentation phase matching ModelSkill gubun. */
    enum class SkillPresentPhase : std::uint8_t {
        /** Prep / charge (gubun 0). */
        Prep = 0,
        /** Fire / impact (gubun 1). */
        Fire = 1,
    };

    /** World-space endpoints for a skill cast presentation. */
    struct SkillPresentEndpoints {
        /** Origin X (wand tip / bow tip / hand). */
        float tip_x = 0.0f;

        /** Origin Y. */
        float tip_y = 0.0f;

        /** Origin Z. */
        float tip_z = 0.0f;

        /** Impact X (target spine). */
        float hit_x = 0.0f;

        /** Impact Y. */
        float hit_y = 0.0f;

        /** Impact Z. */
        float hit_z = 0.0f;

        /** Target character origin X (feet, `runtime state._41`). */
        float origin_x = 0.0f;

        /** Target character origin Y. */
        float origin_y = 0.0f;

        /** Target character origin Z. */
        float origin_z = 0.0f;

        /** Caster character origin X (feet). */
        float caster_origin_x = 0.0f;

        /** Caster character origin Y. */
        float caster_origin_y = 0.0f;

        /** Caster character origin Z. */
        float caster_origin_z = 0.0f;

        /** Optional impact scale from monster height. */
        float impact_scale = 1.0f;

        /** Optional caster entity key for attached or looping SpriteFx. */
        std::uint32_t caster_entity = 0;

        /** Optional target entity key for attached SpriteFx. */
        std::uint32_t target_entity = 0;

        /** True when tip/hit were resolved. */
        bool valid = false;
    };

    /**
     * World position for one FX attach mode.
     * @param attach Caster tip, caster origin, target spine, or target origin.
     * @param ends Resolved cast endpoints.
     * @param x Out world X.
     * @param y Out world Y.
     * @param z Out world Z.
     */
    void skill_fx_world(
        SkillFxAttach attach, const SkillPresentEndpoints& ends, float& x, float& y, float& z);

    /**
     * Entity key for one FX attach mode.
     * @param attach Caster or target attachment.
     * @param ends Resolved cast endpoints.
     * @return Caster entity or target entity.
     */
    [[nodiscard]] std::uint32_t skill_fx_entity(SkillFxAttach attach,
                                                const SkillPresentEndpoints& ends);

    /**
     * Injection point for entity-aware Knight FX rendering.
     *
     * The implementation should route FX scripts and particles through SpriteFx,
     * retaining looping prep effects against the caster entity until cast cleanup.
     */
    class IKnightFxDispatcher {
      public:
        /** Ensures derived dispatchers are destroyed through the interface. */
        virtual ~IKnightFxDispatcher() = default;

        /**
         * Dispatches one Knight ET mapping.
         * @param engine Engine used to resolve SpriteFx assets.
         * @param dispatch Asset, attachment, and lifetime metadata.
         * @param ends Current caster and target world positions.
         * @return True when the renderer accepted the dispatch.
         */
        [[nodiscard]] virtual bool dispatch(engine::IEngine& engine,
                                            const SkillFxDispatch& dispatch,
                                            const SkillPresentEndpoints& ends) = 0;

        /**
         * Removes retained preparation effects for a caster.
         * @param entity Caster entity key.
         */
        virtual void clear_looping(std::uint32_t entity) {
            (void) entity;
        }
    };

    /**
     * Injection point for entity-aware Archer FX rendering.
     *
     * The implementation routes FX scripts and particles through
     * SpriteFx while CombatFx owns the moving arrow model and ribbon.
     */
    class IArcherFxDispatcher {
      public:
        /** Ensures derived dispatchers are destroyed through the interface. */
        virtual ~IArcherFxDispatcher() = default;

        /**
         * Dispatches one Archer ET mapping.
         * @param engine Engine used to resolve SpriteFx assets.
         * @param dispatch Asset, attachment, and lifetime metadata.
         * @param ends Current caster and target world positions.
         * @return True when the renderer accepted the dispatch.
         */
        [[nodiscard]] virtual bool dispatch(engine::IEngine& engine,
                                            const SkillFxDispatch& dispatch,
                                            const SkillPresentEndpoints& ends) = 0;

        /**
         * Removes retained preparation effects for a caster.
         * @param entity Caster entity key.
         */
        virtual void clear_looping(std::uint32_t entity) {
            (void) entity;
        }
    };

    /**
     * Dispatches skill cast FX from SkillEffectTable and/or SkillDef::effect[].
     * Prefer ET_* handlers; fall back to JSON effect stems / SFX.
     */
    class SkillPresenter {
      public:
        /**
         * Binds the combat FX system used for spawns.
         * @param fx Combat FX; must outlive this presenter.
         */
        void set_combat_fx(CombatFx* fx) {
            m_fx = fx;
        }

        /**
         * Binds the entity-aware Knight SpriteFx renderer.
         * @param dispatcher Dispatcher; must outlive this presenter.
         */
        void set_knight_fx_dispatcher(IKnightFxDispatcher* dispatcher) {
            m_knight_fx_dispatcher = dispatcher;
        }

        /**
         * Binds the entity-aware Archer SpriteFx renderer.
         * @param dispatcher Dispatcher; must outlive this presenter.
         */
        void set_archer_fx_dispatcher(IArcherFxDispatcher* dispatcher) {
            m_archer_fx_dispatcher = dispatcher;
        }

        /**
         * Presents one cast phase for a skill.
         * @param engine Engine for mesh creation.
         * @param job Class / transform job id.
         * @param skill_index Skill index.
         * @param phase Prep or fire.
         * @param def Optional skill def (for effect[] / sound).
         * @param ends Tip and hit positions.
         * @return True when an FX or SFX was started.
         */
        [[nodiscard]] bool present(engine::IEngine& engine,
                                   int job,
                                   int skill_index,
                                   SkillPresentPhase phase,
                                   const SkillDef* def,
                                   const SkillPresentEndpoints& ends);

      private:
        /**
         * Spawns FX for a resolved ET_* value.
         * @param engine Engine.
         * @param et Effect type.
         * @param ends Endpoints.
         * @return True when handled.
         */
        [[nodiscard]] bool
        present_et(engine::IEngine& engine, SkillEt et, const SkillPresentEndpoints& ends);

        /** Combat FX backend; not owned. */
        CombatFx* m_fx = nullptr;

        /** Entity-aware Knight SpriteFx renderer; not owned. */
        IKnightFxDispatcher* m_knight_fx_dispatcher = nullptr;

        /** Entity-aware Archer SpriteFx renderer; not owned. */
        IArcherFxDispatcher* m_archer_fx_dispatcher = nullptr;
    };

}  // namespace openkal::client
