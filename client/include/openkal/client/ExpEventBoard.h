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

#include "openkal/client/FreetypeFont.h"
#include "openkal/protocol/ExpEventState.h"

#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Procedural 3D noticeboard with a FreeType event display on both sides.
     * The owning engine must remain alive until destroy() or this object's destruction.
     */
    class ExpEventBoard final {
      public:
        /** Creates a lazy GPU resource owner.
         * @param engine Engine used for geometry, materials and the sign surface.
         */
        explicit ExpEventBoard(engine::IEngine& engine);

        /** Releases any remaining resources before the engine context closes. */
        ~ExpEventBoard();
        ExpEventBoard(const ExpEventBoard&) = delete;
        ExpEventBoard& operator=(const ExpEventBoard&) = delete;

        /** Updates the surface only when its server snapshot or item label changes.
         * Call before begin_frame(), since this uses a temporary offscreen target.
         * @param state Authoritative event snapshot.
         * @param item_name English name of the donation item.
         * @return True when the board is ready to draw.
         */
        [[nodiscard]] bool update(const protocol::ExpEventStateFields& state,
                                  std::string_view item_name);

        /** Draws the solid model and both inset display surfaces.
         * @param model World placement; local Y is up, base is at Y=0.
         */
        void draw(const engine::Mat4& model) const;

        /** Releases GPU resources while the engine context is still active. */
        void destroy();

      private:
        struct Part {
            engine::MeshHandle mesh;
            engine::TextureHandle texture;
        };
        [[nodiscard]] bool initialize();
        [[nodiscard]] bool render_surface(const protocol::ExpEventStateFields& state,
                                          std::string_view item_name);

        engine::IEngine& m_engine;
        std::vector<Part> m_parts;
        engine::MeshHandle m_panel;
        engine::TextureHandle m_surface;
        /** Cached slate material beneath the live FreeType lettering. */
        engine::TextureHandle m_backdrop;
        FreetypeFont m_body_font;
        FreetypeFont m_title_font;
        std::vector<std::uint8_t> m_last_snapshot;
        std::string m_last_item_name;
    };
}  // namespace openkal::client
