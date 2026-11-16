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

    /**
     * Click-to-move destination cue: a rotating sphere plus a ground ring.
     *
     * Packed into the client binary (no Godot PackedScene / ResourceLoader). The
     * vsh*.bin files are vertex shaders, not this marker.
     */
    class ClickMarker {
      public:
        /**
         * Builds the sphere, ring, and tint texel, releasing any earlier GPU objects.
         * @param engine Engine used to create the meshes and texture.
         * @return True when both meshes and the texel were created.
         */
        [[nodiscard]] bool init(engine::IEngine& engine);

        /**
         * Releases GPU objects and hides the marker.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Places the marker and refreshes its lifetime.
         *
         * Hop/spin keep running while the same destination is refreshed each frame;
         * they restart only when the marker was hidden or the XZ target moved.
         * @param wx World X.
         * @param wy World Y (ground height at the destination).
         * @param wz World Z.
         */
        void show(float wx, float wy, float wz);

        /** Hides the marker immediately. */
        void clear();

        /**
         * Ages the marker and fades it out once its lifetime expires.
         * @param dt Frame delta in seconds.
         */
        void tick(float dt);

        /** @return True while the marker is still visible. */
        [[nodiscard]] bool active() const {
            return m_life > 0.0f;
        }

        /**
         * Draws the rotating sphere and pulsing ring at the destination.
         * @param engine Engine used to submit the draws.
         */
        void draw(engine::IEngine& engine) const;

      private:
        /** Ground ring, centred on the origin in the XZ plane. */
        engine::MeshHandle m_ring{};

        /** UV sphere, centred on the origin. */
        engine::MeshHandle m_sphere{};

        /** 1x1 white texel used to tint both meshes. */
        engine::TextureHandle m_texture{};

        /** World X of the destination. */
        float m_x = 0.0f;

        /** World Y of the destination. */
        float m_y = 0.0f;

        /** World Z of the destination. */
        float m_z = 0.0f;

        /** Remaining lifetime in seconds; 0 hides the marker. */
        float m_life = 0.0f;

        /** Seconds since the marker was last shown, driving spin and pulse. */
        float m_age = 0.0f;

        /** True when the texel was created here and must be destroyed here. */
        bool m_own_texture = false;
    };

}  // namespace openkal::client
