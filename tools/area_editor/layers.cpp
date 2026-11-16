/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "layers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>

namespace openkal::area_editor {
    namespace {

        const LayerInfo kLayers[static_cast<int>(LayerId::Count)] = {
            {LayerId::Blocked, "Blocked", "1", 0, 0, 1.0f, 0.15f, 0.15f},
            {LayerId::NotMoveable,
             "NotMoveable",
             "2",
             map::kAttrNotMoveable,
             map::kAttrNotMoveable,
             1.0f,
             0.45f,
             0.1f},
            {LayerId::Village,
             "Village",
             "3",
             map::kAttrVillage,
             map::kAttrVillage,
             0.2f,
             0.85f,
             0.3f},
            {LayerId::Safety,
             "Safety",
             "4",
             map::kAttrSafety,
             map::kAttrSafety,
             0.25f,
             0.55f,
             1.0f},
            {LayerId::Portal,
             "Portal",
             "5",
             map::kAttrPortal,
             map::kAttrPortal,
             0.95f,
             0.85f,
             0.2f},
            {LayerId::Object,
             "Object",
             "6",
             map::kAttrObject,
             map::kAttrObject,
             0.7f,
             0.35f,
             0.95f},
            {LayerId::AtkCastle,
             "AtkCastle",
             "7",
             map::kAttrAttackCastle,
             map::kAttrAttackCastle,
             1.0f,
             0.3f,
             0.55f},
            {LayerId::DefCastle,
             "DefCastle",
             "8",
             map::kAttrDefenceCastle,
             map::kAttrDefenceCastle,
             0.4f,
             0.9f,
             0.85f},
        };

    }  // namespace

    const LayerInfo& layer_info(LayerId id) {
        return kLayers[static_cast<int>(id)];
    }

    const char* layer_name(LayerId id) {
        return layer_info(id).name;
    }

    bool is_fully_blocked(const map::AreaCell& grid) {
        const auto& masks = grid.blocked_masks();
        const std::size_t expected = static_cast<std::size_t>(map::kTileCount) * map::kTileCount;
        constexpr auto kFullMask = std::numeric_limits<map::AreaCell::SubcellMask>::max();
        return masks.size() == expected &&
               std::all_of(masks.begin(), masks.end(), [](map::AreaCell::SubcellMask mask) {
                   return mask == kFullMask;
               });
    }

    LayerState::LayerState() {
        for (int i = 0; i < static_cast<int>(LayerId::Count); ++i) {
            visible[i] = false;
        }
    }

    AttrEditor::AttrEditor(map::AreaCell grid) : grid_(std::move(grid)) {
        grid_.ensure_full_attrs(0u);
        grid_.ensure_blocked_mask();
        grid_.ensure_not_moveable_mask();
    }

    void AttrEditor::set_active(LayerId id) {
        if (id >= LayerId::Blocked && id < LayerId::Count) {
            layers_.active = id;
            // Selecting a layer also shows it (others stay as the user left them).
            layers_.visible[static_cast<int>(id)] = true;
        }
    }

    void AttrEditor::toggle_visibility(LayerId id) {
        const int i = static_cast<int>(id);
        if (i >= 0 && i < static_cast<int>(LayerId::Count)) {
            layers_.visible[i] = !layers_.visible[i];
        }
    }

    std::uint32_t& AttrEditor::attr_ref(int tx, int ty) {
        return grid_.attr_ref(tx, ty);
    }

    bool AttrEditor::tile_has_layer(int tx, int ty, LayerId id) const {
        if (tx < 0 || ty < 0 || tx >= map::kTileCount || ty >= map::kTileCount) {
            return false;
        }
        const std::uint32_t a = grid_.at(tx, ty);
        const LayerInfo& info = layer_info(id);
        if (id == LayerId::Blocked) {
            return grid_.blocked_mask(tx, ty) != 0;
        }
        if (id == LayerId::NotMoveable) {
            return grid_.not_moveable_mask(tx, ty) != 0;
        }
        return (a & info.paint_set) != 0;
    }

    void AttrEditor::push_undo_tile(int flat,
                                    std::uint32_t before_attr,
                                    map::AreaCell::SubcellMask before_blocked,
                                    map::AreaCell::SubcellMask before_not_moveable,
                                    UndoStroke& stroke) {
        for (const UndoTile& tile : stroke.tiles) {
            if (tile.flat == flat) {
                return;  // keep earliest previous value in this stroke
            }
        }
        stroke.tiles.push_back({flat, before_attr, before_blocked, before_not_moveable});
        dirty_ = true;
    }

    void AttrEditor::apply_attr_tile(int tx, int ty, bool erase, UndoStroke& stroke) {
        if (tx < 0 || ty < 0 || tx >= map::kTileCount || ty >= map::kTileCount) {
            return;
        }
        const int flat = ty * map::kTileCount + tx;
        std::uint32_t& a = attr_ref(tx, ty);
        const LayerInfo& info = layer_info(layers_.active);
        const std::uint32_t before = a;
        if (erase) {
            a &= ~info.paint_clear;
        } else {
            a |= info.paint_set;
        }
        if (a != before) {
            push_undo_tile(
                flat, before, grid_.blocked_mask(tx, ty), grid_.not_moveable_mask(tx, ty), stroke);
        }
    }

    void AttrEditor::apply_subcell(int sx, int sy, bool erase, UndoStroke& stroke) {
        if (sx < 0 || sy < 0 || sx >= kOverlaySize || sy >= kOverlaySize) {
            return;
        }
        const int tx = sx / map::AreaCell::kSubcellsPerTile;
        const int ty = sy / map::AreaCell::kSubcellsPerTile;
        const int flat = ty * map::kTileCount + tx;
        const std::uint32_t before_attr = grid_.at(tx, ty);
        const map::AreaCell::SubcellMask before_blocked = grid_.blocked_mask(tx, ty);
        const map::AreaCell::SubcellMask before_not_moveable = grid_.not_moveable_mask(tx, ty);
        if (layers_.active == LayerId::Blocked) {
            grid_.set_blocked_at_subcell(sx, sy, !erase);
        } else {
            grid_.set_not_moveable_at_subcell(sx, sy, !erase);
        }
        if (grid_.blocked_mask(tx, ty) != before_blocked ||
            grid_.not_moveable_mask(tx, ty) != before_not_moveable) {
            push_undo_tile(flat, before_attr, before_blocked, before_not_moveable, stroke);
        }
    }

    void AttrEditor::paint_at(int cursor_x, int cursor_y, bool erase) {
        UndoStroke stroke;
        const int r = std::max(0, layers_.brush_radius);
        const int r2 = r * r;
        if (layer_uses_subcells(layers_.active)) {
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy > r2) {
                        continue;
                    }
                    apply_subcell(cursor_x + dx, cursor_y + dy, erase, stroke);
                }
            }
        } else {
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy > r2) {
                        continue;
                    }
                    apply_attr_tile(cursor_x + dx, cursor_y + dy, erase, stroke);
                }
            }
        }
        if (!stroke.tiles.empty()) {
            undo_.push_back(std::move(stroke));
            if (static_cast<int>(undo_.size()) > kMaxUndo) {
                undo_.erase(undo_.begin());
            }
        }
    }

    std::size_t AttrEditor::paint_subcell_mask(const std::vector<std::uint32_t>& subcells,
                                               bool erase) {
        UndoStroke stroke;
        const std::size_t tile_count = static_cast<std::size_t>(map::kTileCount) * map::kTileCount;
        std::vector<std::uint8_t> captured(tile_count, 0);
        std::size_t changed = 0;

        if (!layer_uses_subcells(layers_.active)) {
            std::vector<std::uint8_t> visited(tile_count, 0);
            const LayerInfo& info = layer_info(layers_.active);
            for (const std::uint32_t subcell : subcells) {
                const int sx = static_cast<int>(subcell % kOverlaySize);
                const int sy = static_cast<int>(subcell / kOverlaySize);
                if (sy < 0 || sy >= kOverlaySize) {
                    continue;
                }
                const int tx = sx / map::AreaCell::kSubcellsPerTile;
                const int ty = sy / map::AreaCell::kSubcellsPerTile;
                const int flat = ty * map::kTileCount + tx;
                if (visited[static_cast<std::size_t>(flat)] != 0) {
                    continue;
                }
                visited[static_cast<std::size_t>(flat)] = 1;
                std::uint32_t& attr = attr_ref(tx, ty);
                const std::uint32_t before = attr;
                if (erase) {
                    attr &= ~info.paint_clear;
                } else {
                    attr |= info.paint_set;
                }
                if (attr != before) {
                    stroke.tiles.push_back({flat,
                                            before,
                                            grid_.blocked_mask(tx, ty),
                                            grid_.not_moveable_mask(tx, ty)});
                    ++changed;
                }
            }
        } else {
            for (const std::uint32_t subcell : subcells) {
                const int sx = static_cast<int>(subcell % kOverlaySize);
                const int sy = static_cast<int>(subcell / kOverlaySize);
                if (sy < 0 || sy >= kOverlaySize) {
                    continue;
                }
                const int tx = sx / map::AreaCell::kSubcellsPerTile;
                const int ty = sy / map::AreaCell::kSubcellsPerTile;
                const int flat = ty * map::kTileCount + tx;
                const auto before_blocked = grid_.blocked_mask(tx, ty);
                const auto before_not_moveable = grid_.not_moveable_mask(tx, ty);
                const bool was_set = layers_.active == LayerId::Blocked
                                         ? grid_.blocked_at_subcell(sx, sy)
                                         : grid_.not_moveable_at_subcell(sx, sy);
                if (was_set == !erase) {
                    continue;
                }
                if (captured[static_cast<std::size_t>(flat)] == 0) {
                    stroke.tiles.push_back(
                        {flat, grid_.at(tx, ty), before_blocked, before_not_moveable});
                    captured[static_cast<std::size_t>(flat)] = 1;
                }
                if (layers_.active == LayerId::Blocked) {
                    grid_.set_blocked_at_subcell(sx, sy, !erase);
                } else {
                    grid_.set_not_moveable_at_subcell(sx, sy, !erase);
                }
                ++changed;
            }
        }

        if (!stroke.tiles.empty()) {
            dirty_ = true;
            undo_.push_back(std::move(stroke));
            if (static_cast<int>(undo_.size()) > kMaxUndo) {
                undo_.erase(undo_.begin());
            }
        }
        return changed;
    }

    void AttrEditor::clear_active_layer() {
        UndoStroke stroke;
        stroke.tiles.reserve(static_cast<std::size_t>(map::kTileCount) * map::kTileCount);
        for (int ty = 0; ty < map::kTileCount; ++ty) {
            for (int tx = 0; tx < map::kTileCount; ++tx) {
                std::uint32_t& a = attr_ref(tx, ty);
                const std::uint32_t before = a;
                const map::AreaCell::SubcellMask before_blocked = grid_.blocked_mask(tx, ty);
                const map::AreaCell::SubcellMask before_not_moveable =
                    grid_.not_moveable_mask(tx, ty);
                if (layers_.active == LayerId::Blocked) {
                    grid_.set_blocked_mask(tx, ty, 0);
                } else if (layers_.active == LayerId::NotMoveable) {
                    grid_.set_not_moveable_mask(tx, ty, 0);
                } else {
                    a &= ~layer_info(layers_.active).paint_clear;
                }
                if (a != before || grid_.blocked_mask(tx, ty) != before_blocked ||
                    grid_.not_moveable_mask(tx, ty) != before_not_moveable) {
                    stroke.tiles.push_back(
                        {ty * map::kTileCount + tx, before, before_blocked, before_not_moveable});
                    dirty_ = true;
                }
            }
        }
        if (!stroke.tiles.empty()) {
            undo_.push_back(std::move(stroke));
            if (static_cast<int>(undo_.size()) > kMaxUndo) {
                undo_.erase(undo_.begin());
            }
        }
    }

    bool AttrEditor::undo() {
        if (undo_.empty()) {
            return false;
        }
        const UndoStroke stroke = std::move(undo_.back());
        undo_.pop_back();
        for (const UndoTile& tile : stroke.tiles) {
            if (tile.flat >= 0 && tile.flat < static_cast<int>(grid_.attr_count())) {
                grid_.set_attr_flat(static_cast<std::size_t>(tile.flat), tile.attr);
                const int tx = tile.flat % map::kTileCount;
                const int ty = tile.flat / map::kTileCount;
                grid_.set_blocked_mask(tx, ty, tile.blocked);
                grid_.set_not_moveable_mask(tx, ty, tile.not_moveable);
            }
        }
        dirty_ = true;
        return true;
    }

    void AttrEditor::build_overlay_rgba(std::vector<std::uint8_t>& rgba) const {
        const int n = kOverlaySize;
        rgba.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n) * 4, 0);
        for (int sy = 0; sy < n; ++sy) {
            for (int sx = 0; sx < n; ++sx) {
                const int tx = sx / map::AreaCell::kSubcellsPerTile;
                const int ty = sy / map::AreaCell::kSubcellsPerTile;
                const std::uint32_t a = grid_.at(tx, ty);

                float r = 0, g = 0, b = 0, w = 0;
                for (int li = 0; li < static_cast<int>(LayerId::Count); ++li) {
                    if (!layers_.visible[li]) {
                        continue;
                    }
                    const LayerInfo& info = kLayers[li];
                    bool hit = false;
                    if (info.id == LayerId::Blocked) {
                        hit = grid_.blocked_at_subcell(sx, sy);
                    } else if (info.id == LayerId::NotMoveable) {
                        hit = grid_.not_moveable_at_subcell(sx, sy);
                    } else {
                        hit = (a & info.paint_set) != 0;
                    }
                    if (!hit) {
                        continue;
                    }
                    r += info.r;
                    g += info.g;
                    b += info.b;
                    w += 1.0f;
                }
                if (w <= 0.0f) {
                    continue;
                }
                r = std::min(1.0f, r / w);
                g = std::min(1.0f, g / w);
                b = std::min(1.0f, b / w);
                const float alpha = std::min(0.55f, 0.28f + 0.12f * w);
                const std::size_t i = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(n) +
                                       static_cast<std::size_t>(sx)) *
                                      4;
                rgba[i + 0] = static_cast<std::uint8_t>(r * 255.0f);
                rgba[i + 1] = static_cast<std::uint8_t>(g * 255.0f);
                rgba[i + 2] = static_cast<std::uint8_t>(b * 255.0f);
                rgba[i + 3] = static_cast<std::uint8_t>(alpha * 255.0f);
            }
        }
    }

    bool parse_patch_name(const std::string& path_or_name, int& patch_x, int& patch_y) {
        std::string base = path_or_name;
        const auto slash = base.find_last_of("/\\");
        if (slash != std::string::npos) {
            base = base.substr(slash + 1);
        }
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos) {
            base = base.substr(0, dot);
        }
        for (char& c : base) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        int x = 0;
        int y = 0;
        if (std::sscanf(base.c_str(), "n_%d_%d", &x, &y) != 2) {
            return false;
        }
        if (x < 0 || y < 0 || x >= map::kPatchGrid || y >= map::kPatchGrid) {
            return false;
        }
        patch_x = x;
        patch_y = y;
        return true;
    }

}  // namespace openkal::area_editor
