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

#include "openkal/client/ItemDb.h"
#include "openkal/engine/engine.h"

#include <string>

namespace openkal::client {

    /** Legacy money item definition index used for ground coin piles. */
    inline constexpr int kDropMoneyItemIndex = 31;

    /** Dropitem mesh number for the gray pouch (dropitem01.gb). */
    inline constexpr int kDropPouchMeshNumber = 1;

    /**
     * Dropitem mesh helpers (Data/Item/DropItem/dropitemNN.gb).
     * Selects the project-facing dropped-item animation state.
     */
    class DropItemVisual {
      public:
        /**
         * Maps an item to a Dropitem mesh number (1–11).
         * Money piles use 7–10 by stack size; gear uses 1–6 by small class.
         * @param item_index item definition index.
         * @param count Stack count or money amount.
         * @param catalog Optional item catalog for class_small.
         * @return Mesh number in 1..11.
         */
        [[nodiscard]] static int mesh_number(int item_index, int count, const ItemDb* catalog);

        /**
         * Builds an absolute path to dropitemNN.gb under data_root.
         * @param data_root Client data root.
         * @param mesh_number Mesh number from mesh_number().
         * @return Path, or empty when missing.
         */
        [[nodiscard]] static std::string gb_path(const std::string& data_root, int mesh_number);

        /**
         * True for the gray pouch mesh (dropitem01), not weapons or coin piles.
         * @param mesh_number Value from mesh_number().
         * @return True when the mesh is the pouch.
         */
        [[nodiscard]] static bool is_pouch(int mesh_number);

        /**
         * Builds the glow billboard, ground disc, and soft radial texture.
         * @param engine Engine used to create GPU objects.
         * @return True when the glow can be drawn.
         */
        [[nodiscard]] bool init(engine::IEngine& engine);

        /**
         * Releases GPU objects owned by this helper.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Draws an additive tinted halo and ground disc around a pouch.
         * @param engine Engine used to submit the draws.
         * @param wx World X of the pouch.
         * @param wy World Y of the pouch (ground + bounce).
         * @param wz World Z of the pouch.
         * @param cam_eye Camera position used to face the billboard.
         * @param age_sec Seconds since the pouch spawned, driving the pulse.
         * @param r Glow red in 0–1.
         * @param g Glow green in 0–1.
         * @param b Glow blue in 0–1.
         */
        void draw_glow(engine::IEngine& engine,
                       float wx,
                       float wy,
                       float wz,
                       const engine::Vec3& cam_eye,
                       float age_sec,
                       float r,
                       float g,
                       float b) const;

        /**
         * Draws a slightly scaled additive copy of a pouch mesh as a silhouette rim.
         * @param engine Engine used to submit the draw.
         * @param mesh Uploaded pouch submesh.
         * @param texture Diffuse texture of the submesh.
         * @param model World matrix of the unscaled pouch.
         * @param two_sided True when the source submesh is two-sided.
         * @param r Glow red in 0–1.
         * @param g Glow green in 0–1.
         * @param b Glow blue in 0–1.
         * @param age_sec Seconds since the pouch spawned, driving the pulse.
         */
        static void draw_mesh_glow(engine::IEngine& engine,
                                   engine::MeshHandle mesh,
                                   engine::TextureHandle texture,
                                   const engine::Mat4& model,
                                   bool two_sided,
                                   float r,
                                   float g,
                                   float b,
                                   float age_sec);

      private:
        /** Camera-facing unit quad centred on the origin in XY. */
        engine::MeshHandle m_billboard{};

        /** Ground disc centred on the origin in the XZ plane. */
        engine::MeshHandle m_disc{};

        /** Soft white radial glow (tinted at draw time). */
        engine::TextureHandle m_texture{};

        /** True when this object created runtime state. */
        bool m_own_texture = false;
    };

}  // namespace openkal::client
