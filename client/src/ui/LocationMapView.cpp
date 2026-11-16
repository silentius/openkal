/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/LocationMapView.h"

#include "openkal/map/Coords.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace openkal::client {
    void LocationMapView::load(const std::string& root) {
        m_tiles = {};
        m_dungeon_tiles.clear();
        (void) m_maps.load_json(platform::join_path(root, "mapinfo.json"));
        (void) m_minimaps.load_json(platform::join_path(root, "ui/dungeon_minimaps.json"));
    }

    void LocationMapView::draw(engine::IEngine& engine,
                               WidgetTools& ui,
                               const FreetypeFont& font,
                               float x,
                               float y,
                               float w,
                               float h,
                               float scale,
                               const std::vector<PowerbookSpawn>& spawns,
                               int selected,
                               float magnification) const {
        if (w <= 0 || h <= 0 || selected < 0 || selected >= static_cast<int>(spawns.size())) {
            return;
        }
        struct Rect {
            float x, y, w, h;
        };
        const Rect rect{x, y, w, h};
        const auto message = [&](std::string_view text) {
            font.draw(engine, x + 12 * scale, y + 12 * scale, text, .90f, .84f, .68f, scale);
        };
        const auto& focus = spawns[selected];
        const int center_x = focus.x0 + (focus.x1 - focus.x0) / 2;
        const int center_y = focus.y0 + (focus.y1 - focus.y0) / 2;
        const auto* cell =
            m_maps.find_cell(focus.map, center_x / map::kWorldSize, center_y / map::kWorldSize);
        if (!cell && m_maps.has_indoor_mapname(focus.map)) {
            cell = m_maps.find_index(focus.map);
        }
        MinimapWidgetData data;
        const bool indoor = cell && !cell->mapname().empty();
        if (indoor) {
            std::string stem = cell->mapname();
            const auto slash = stem.find_last_of("/\\");
            if (slash != std::string::npos) {
                stem.erase(0, slash + 1);
            }
            const auto* size = m_minimaps.find(stem);
            if (!size) {
                message("Dungeon map metadata unavailable.");
                return;
            }
            data.set_dungeon(*size, cell->x() & ~1, cell->y() & ~1, coords::kDefaultOriginPatch);
        }
        const auto atlas_x = [&](int sx) {
            const float world = coords::to_client(sx);
            return indoor ? MinimapWidget::dungeon_atlas_x(world,
                                                           coords::kDefaultOriginPatch,
                                                           data.origin_cell_x(),
                                                           data.map_left(),
                                                           data.correction())
                          : MinimapWidget::atlas_x(world);
        };
        const auto atlas_y = [&](int sy) {
            const float world = coords::to_client(sy);
            return indoor ? MinimapWidget::dungeon_atlas_y(world,
                                                           coords::kDefaultOriginPatch,
                                                           data.origin_cell_y(),
                                                           data.map_bottom(),
                                                           data.correction(),
                                                           data.size_y())
                          : MinimapWidget::atlas_y(world);
        };
        const float cx = atlas_x(center_x), cy = atlas_y(center_y);
        const float view_w =
            std::max({384.0f,
                      std::abs(atlas_x(focus.x1) - atlas_x(focus.x0)) * 2.4f,
                      std::abs(atlas_y(focus.y1) - atlas_y(focus.y0)) * 2.4f * rect.w / rect.h}) /
            std::max(0.1f, magnification);
        const float zoom = rect.w / view_w;
        const float view_h = rect.h / zoom;
        const float left = cx - view_w * .5f, top = cy - view_h * .5f;
        const auto draw_tile = [&](const std::string& name, float x, float y, float w, float h) {
            const float l = std::max(x, left), t = std::max(y, top);
            const float r = std::min(x + w, left + view_w), b = std::min(y + h, top + view_h);
            if (name.empty() || r <= l || b <= t) {
                return false;
            }
            ui.draw_tex_uv(name,
                           rect.x + (l - left) * zoom,
                           rect.y + (t - top) * zoom,
                           (r - l) * zoom,
                           (b - t) * zoom,
                           (l - x) / w,
                           (t - y) / h,
                           (r - x) / w,
                           (b - y) / h);
            return true;
        };
        int budget = 4;
        bool painted = false, pending = false;
        if (indoor) {
            const int tw = std::max(1, data.tile_width()), th = std::max(1, data.tile_height());
            const int count = (data.size_x() / tw) * (data.size_y() / th);
            for (int index = 0; index < count; ++index) {
                float l, t, r, b;
                MinimapWidget::dungeon_tile_rect(index, tw, th, data.size_x(), l, t, r, b);
                if (r <= left || b <= top || l >= left + view_w || t >= top + view_h) {
                    continue;
                }
                const auto key = data.mapname() + ":" + std::to_string(index);
                auto found = m_dungeon_tiles.find(key);
                if (found == m_dungeon_tiles.end()) {
                    if (budget-- <= 0) {
                        pending = true;
                        continue;
                    }
                    std::string name;
                    for (const char* extension : {"GTX", "gtx"}) {
                        char path[256];
                        std::snprintf(path,
                                      sizeof(path),
                                      "MiniMap/minimap_Dungeon_%s_%03d.%s",
                                      data.mapname().c_str(),
                                      index,
                                      extension);
                        const auto info = ui.tex_info(path);
                        if (info.handle() && info.width() >= 16 && info.height() >= 16) {
                            name = path;
                            break;
                        }
                    }
                    found = m_dungeon_tiles.emplace(key, std::move(name)).first;
                }
                painted = draw_tile(found->second, l, t, r - l, b - t) || painted;
            }
        } else {
            for (int px = 0; px < map::kPatchGrid; px += 2) {
                for (int py = 0; py < map::kPatchGrid; py += 2) {
                    float l, t, r, b;
                    MinimapWidget::tile_rect(px, py, l, t, r, b);
                    if (r <= left || b <= top || l >= left + view_w || t >= top + view_h) {
                        continue;
                    }
                    const auto index = static_cast<std::size_t>(px / 2 * 32 + py / 2);
                    if (!m_tiles.resolved(index)) {
                        if (budget-- <= 0) {
                            pending = true;
                            continue;
                        }
                        std::string name;
                        for (const char* format : {"MiniMap/e/minimap_n_e_%03d_%03d.gtx",
                                                   "MiniMap/e/minimap_n_%03d_%03d.gtx",
                                                   "MiniMap/minimap_n_%03d_%03d.gtx"}) {
                            char path[96];
                            std::snprintf(path, sizeof(path), format, px, py);
                            const auto info = ui.tex_info(path);
                            if (info.handle() && info.width() >= 16 && info.height() >= 16) {
                                name = path;
                                break;
                            }
                        }
                        m_tiles.resolve(index, std::move(name));
                    }
                    painted = draw_tile(m_tiles.tile_name(index), l, t, r - l, b - t) || painted;
                }
            }
        }
        for (std::size_t i = 0; i < spawns.size(); ++i) {
            const auto& spawn = spawns[i];
            if (spawn.map != focus.map) {
                continue;
            }
            const float l =
                std::clamp(rect.x + (atlas_x(spawn.x0) - left) * zoom, rect.x, rect.x + rect.w);
            const float r =
                std::clamp(rect.x + (atlas_x(spawn.x1) - left) * zoom, rect.x, rect.x + rect.w);
            const float t =
                std::clamp(rect.y + (atlas_y(spawn.y1) - top) * zoom, rect.y, rect.y + rect.h);
            const float b =
                std::clamp(rect.y + (atlas_y(spawn.y0) - top) * zoom, rect.y, rect.y + rect.h);
            if (r < l || b < t || r <= rect.x || l >= rect.x + rect.w || b <= rect.y ||
                t >= rect.y + rect.h) {
                continue;
            }
            const float w = std::min(std::max((spawn.x0 == spawn.x1 ? 8 : 2) * scale, r - l),
                                     rect.x + rect.w - l);
            const float h = std::min(std::max((spawn.y0 == spawn.y1 ? 8 : 2) * scale, b - t),
                                     rect.y + rect.h - t);
            const bool active = static_cast<int>(i) == selected;
            engine.draw_ui_rect(l, t, w, h, 1, .84f, .12f, active ? .30f : .16f);
            const float line = std::min({2 * scale, w, h});
            engine.draw_ui_rect(l, t, w, line, 1, .88f, .22f, .95f);
            engine.draw_ui_rect(l, t + h - line, w, line, 1, .88f, .22f, .95f);
            engine.draw_ui_rect(l, t, line, h, 1, .88f, .22f, .95f);
            engine.draw_ui_rect(l + w - line, t, line, h, 1, .88f, .22f, .95f);
        }
        if (!painted) {
            message(pending ? "Loading map..." : "Map image unavailable.");
        }
    }

}  // namespace openkal::client
