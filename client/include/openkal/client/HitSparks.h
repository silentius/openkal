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

namespace openkal::client {

    /** Bounded, batched white streaks carrying an attack's impulse away from its origin. */
    class HitSparks {
      public:
        /** Creates a project-owned procedural sprite; no external assets are used.
         * @param engine Renderer that owns the sprite texture.
         * @return True when the texture is ready.
         */
        [[nodiscard]] bool init(engine::IEngine& engine);

        /** Releases the sprite and clears live sparks.
         * @param engine Renderer used for initialization.
         */
        void destroy(engine::IEngine& engine);

        /** Emits one finite burst, capped at 256 live sparks across all hits.
         * @param impact World-space torso center of the target.
         * @param origin Attacker's world position; sparks travel horizontally away from it.
         * @param scale Monster impact scale, clamped to keep bursts small.
         * @param critical True for a slightly denser burst.
         */
        void
        spawn(const engine::Vec3& impact, const engine::Vec3& origin, float scale, bool critical);

        /** Advances and removes expired sparks.
         * @param dt Elapsed seconds.
         */
        void update(float dt);

        /** Draws all live sparks in one additive batch, respecting scene depth.
         * @param engine Renderer used for initialization.
         * @param camera World-space camera position for billboarding.
         */
        void draw(engine::IEngine& engine, const engine::Vec3& camera);

        /** @return Number of live sparks, including those outside the camera view. */
        [[nodiscard]] std::size_t particle_count() const {
            return m_sparks.size();
        }

      private:
        struct Spark {
            engine::Vec3 position;
            engine::Vec3 velocity;
            float age = 0.0f;
            float life = 0.0f;
            float size = 0.0f;
        };

        /** Procedural white diamond with transparent edges. */
        engine::TextureHandle m_texture{};
        std::vector<Spark> m_sparks;
        /** Reused vertex storage; six vertices per billboard. */
        std::vector<engine::SpriteBatchVertex> m_vertices;
    };

}  // namespace openkal::client
