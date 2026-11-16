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

#include <string>

namespace openkal::client {

    /** Ground disc under the currently selected monster (KalOnline
     * Effect/Tex/circle.GTX). */
    class SelectMarker {
      public:
        /**
         * Builds the disc quad and loads circle.GTX, falling back to a soft white
         * texel.
         * @param engine Engine used to create the mesh and texture.
         * @param data_root Client data root.
         * @return True when a usable texture is bound.
         */
        [[nodiscard]] bool init(engine::IEngine& engine, const std::string& data_root);

        /**
         * Releases the quad and the owned texture, and hides the marker.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Places the marker under a target and shows it.
         * @param wx World X.
         * @param wy World Y (ground height under the target).
         * @param wz World Z.
         */
        void show(float wx, float wy, float wz);

        /** Hides the marker. */
        void clear();

        /** @return True while the marker is drawn. */
        [[nodiscard]] bool active() const {
            return m_active;
        }

        /**
         * Advances the pulse clock while the marker is shown.
         * @param dt Frame delta in seconds.
         */
        void tick(float dt);

        /**
         * Draws the additive disc at the marked position.
         * @param engine Engine used to submit the draw.
         */
        void draw(engine::IEngine& engine) const;

      private:
        /** Disc quad, centred on the origin in the XZ plane. */
        engine::MeshHandle m_quad{};

        /** Disc texture (circle.GTX, or the generated fallback). */
        engine::TextureHandle m_texture{};

        /** World X of the marked position. */
        float m_x = 0.0f;

        /** World Y of the marked position. */
        float m_y = 0.0f;

        /** World Z of the marked position. */
        float m_z = 0.0f;

        /** Seconds since the marker was created, driving the pulse. */
        float m_age = 0.0f;

        /** True while the marker is shown. */
        bool m_active = false;

        /** True when the texture was created here and must be destroyed here. */
        bool m_own_texture = false;
    };

}  // namespace openkal::client
