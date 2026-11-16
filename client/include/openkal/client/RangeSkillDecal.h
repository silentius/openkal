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

#include "openkal/engine/engine.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    class SpriteFx;

    /**
     * Ground targeting cue for RangeSkillCast (Mage storms, Explosive Burst,
     * Archer Arrow Rain / Poison Cloud).
     *
     * Same hopping-marker family as ClickMarker: a pulsing ground ring. The ring
     * is white, and the yellow jump-sphere is replaced by the storm's looping
     * prep FX (`effect_ef078` / `ef086` / `ef092`). Arrow Rain uses Fire Storm's
     * `effect_ef092`; Explosive Burst and Poison Cloud show the ring only.
     */
    class RangeSkillDecal {
      public:
        /** SpriteFx entity key reserved for the targeting preview. */
        static constexpr std::uint32_t kPreviewEntity = 0xD0000003u;

        /**
         * Builds the ring mesh and the white tint texel.
         * @param engine Engine used to create the mesh and texture.
         * @return True when the ring and texel were created.
         */
        [[nodiscard]] bool init(engine::IEngine& engine);

        /**
         * Releases the ring, texel, and any attached preview FX.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Places the cue and keeps the storm prep FX attached at this point.
         * @param engine Engine used to load FX textures on first attach.
         * @param sprite_fx Sprite FX manager that owns the preview emitters.
         * @param wx World X.
         * @param wy World Y (ground height).
         * @param wz World Z.
         * @param fx_name Looping storm prep FX, or empty to show only the ring.
         */
        void show(engine::IEngine& engine,
                  SpriteFx& sprite_fx,
                  float wx,
                  float wy,
                  float wz,
                  std::string_view fx_name);

        /**
         * Hides the ring and detaches the preview FX.
         * @param sprite_fx Sprite FX manager that owns the preview emitters.
         */
        void clear(SpriteFx& sprite_fx);

        /** @return True while the cue is drawn. */
        [[nodiscard]] bool active() const {
            return m_active;
        }

        /**
         * Advances the pulse clock while the cue is shown.
         * @param dt Frame delta in seconds.
         */
        void tick(float dt);

        /**
         * Draws the white pulsing ring at the marked position.
         * @param engine Engine used to submit the draw.
         */
        void draw(engine::IEngine& engine) const;

      private:
        /** Ground ring, centred on the origin in the XZ plane. */
        engine::MeshHandle m_ring{};

        /** 1x1 white texel used to tint the ring. */
        engine::TextureHandle m_texture{};

        /** Sprite FX manager last passed to show(); used by destroy(). */
        SpriteFx* m_sprite_fx = nullptr;

        /** Prep FX currently attached to kPreviewEntity. */
        std::string m_fx_name;

        /** World X of the marked position. */
        float m_x = 0.0f;

        /** World Y of the marked position. */
        float m_y = 0.0f;

        /** World Z of the marked position. */
        float m_z = 0.0f;

        /** Seconds shown, driving ring spin and pulse. */
        float m_age = 0.0f;

        /** True while the cue is drawn. */
        bool m_active = false;

        /** True when the texel was created here and must be destroyed here. */
        bool m_own_texture = false;
    };

}  // namespace openkal::client
