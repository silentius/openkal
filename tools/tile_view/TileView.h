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

#include <memory>
#include <string>

namespace openkal::tile_view {

    /** Interactive fly-through of one outdoor tile (KCM terrain + OPL props). */
    class TileView {
      public:
        /**
         * Prepares a viewer for patch (patch_x, patch_y).
         * @param data_root Client data root.
         * @param patch_x Horizontal patch index.
         * @param patch_y Vertical patch index.
         * @param export_dir Destination for E / --export; empty disables until set.
         * @param ui_scale HUD scale (1 = 1600x900 window).
         */
        TileView(std::string data_root,
                 int patch_x,
                 int patch_y,
                 std::string export_dir,
                 float ui_scale);

        /** Releases GPU resources and shuts down the engine. */
        ~TileView();

        TileView(const TileView&) = delete;
        TileView& operator=(const TileView&) = delete;

        /**
         * Opens the window, loads the tile, and runs the fly loop.
         * @return Process exit code (0 on a clean quit).
         */
        [[nodiscard]] int run();

      private:
        /** Hidden engine, scene, and camera state. */
        class Impl;

        /** Viewer implementation. */
        std::unique_ptr<Impl> m_impl;
    };

}  // namespace openkal::tile_view
