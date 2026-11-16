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

#include "openkal/map/AreaCell.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::area_editor {

    enum class LayerId : int {
        Blocked = 0,
        NotMoveable = 1,
        Village = 2,
        Safety = 3,
        Portal = 4,
        Object = 5,
        AtkCastle = 6,
        DefCastle = 7,
        Count = 8,
    };

    struct LayerInfo {
        LayerId id;
        const char* name;
        const char* hotkey;         // display hint
        std::uint32_t paint_set;    // bits set on paint
        std::uint32_t paint_clear;  // bits cleared on erase
        float r, g, b;              // overlay tint
    };

    // Overlay resolution: one texel per editable subcell (4x4 per tile).
    inline constexpr int kOverlaySize = map::kTileCount * map::AreaCell::kSubcellsPerTile;

    const LayerInfo& layer_info(LayerId id);
    const char* layer_name(LayerId id);

    /** Tests whether every editable collision subcell is blocked.
     * @param grid Area cell to inspect.
     * @return True when every tile has a full blocked mask.
     */
    [[nodiscard]] bool is_fully_blocked(const map::AreaCell& grid);

    // Blocked and NotMoveable paint at subcell resolution.
    inline bool layer_uses_subcells(LayerId id) {
        return id == LayerId::Blocked || id == LayerId::NotMoveable;
    }

    struct LayerState {
        LayerId active = LayerId::Blocked;
        bool visible[static_cast<int>(LayerId::Count)];
        // Brush radius in the active layer's units (subcells for collision
        // layers, tiles otherwise).
        int brush_radius = 0;

        LayerState();
    };

    struct UndoTile {
        int flat = 0;
        std::uint32_t attr = 0;
        map::AreaCell::SubcellMask blocked = 0;
        map::AreaCell::SubcellMask not_moveable = 0;
    };

    struct UndoStroke {
        std::vector<UndoTile> tiles;
    };

    class AttrEditor {
      public:
        explicit AttrEditor(map::AreaCell grid);

        map::AreaCell& grid() {
            return grid_;
        }
        const map::AreaCell& grid() const {
            return grid_;
        }
        LayerState& layers() {
            return layers_;
        }
        const LayerState& layers() const {
            return layers_;
        }
        bool dirty() const {
            return dirty_;
        }
        void clear_dirty() {
            dirty_ = false;
        }

        void set_active(LayerId id);
        void toggle_visibility(LayerId id);

        // Cursor in subcell space (0..1023) for collision layers, tile space
        // (0..255) for other layers.
        void paint_at(int cursor_x, int cursor_y, bool erase);

        /** Applies a filled subcell mask to the active layer as one undo step.
         *
         * @param subcells Flat 1024x1024 subcell indices.
         * @param erase Whether to remove rather than set the active layer.
         * @return Number of changed subcells or tiles, depending on the active layer.
         */
        std::size_t paint_subcell_mask(const std::vector<std::uint32_t>& subcells, bool erase);

        void clear_active_layer();
        bool undo();

        // Build kOverlaySize² RGBA overlay (visible layers only).
        void build_overlay_rgba(std::vector<std::uint8_t>& rgba) const;

        bool tile_has_layer(int tx, int ty, LayerId id) const;

      private:
        void apply_attr_tile(int tx, int ty, bool erase, UndoStroke& stroke);
        void apply_subcell(int sx, int sy, bool erase, UndoStroke& stroke);
        void push_undo_tile(int flat,
                            std::uint32_t before_attr,
                            map::AreaCell::SubcellMask before_blocked,
                            map::AreaCell::SubcellMask before_not_moveable,
                            UndoStroke& stroke);
        std::uint32_t& attr_ref(int tx, int ty);

        map::AreaCell grid_;
        LayerState layers_;
        bool dirty_ = false;
        std::vector<UndoStroke> undo_;
        static constexpr int kMaxUndo = 64;
    };

    bool parse_patch_name(const std::string& path_or_name, int& patch_x, int& patch_y);

}  // namespace openkal::area_editor
