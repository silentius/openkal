/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WorldMapWidget.h"

#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace openkal::client {
    namespace {
        constexpr float kCloseWidth = 112.0f;
        constexpr float kCloseHeight = 30.0f;
        constexpr float kCloseMargin = 16.0f;

        void draw_camera_fallback(
            engine::IEngine& engine, float cx, float cy, float yaw_rad, float scale) {
            const float s = std::max(0.25f, scale);
            const float dx = std::sin(yaw_rad);
            const float dy = -std::cos(yaw_rad);
            const float len = 11.0f * s;
            const float px = -dy;
            const float py = dx;
            engine.draw_ui_triangle(cx + dx * len,
                                    cy + dy * len,
                                    cx - dx * len * 0.55f + px * len * 0.42f,
                                    cy - dy * len * 0.55f + py * len * 0.42f,
                                    cx - dx * len * 0.55f - px * len * 0.42f,
                                    cy - dy * len * 0.55f - py * len * 0.42f,
                                    1.0f,
                                    0.95f,
                                    0.2f,
                                    1.0f);
        }

        std::string tile_filename(int patch_x, int patch_y, int variant) {
            char name[80];
            switch (variant) {
            case 0:
                std::snprintf(
                    name, sizeof(name), "MiniMap/e/minimap_n_e_%03d_%03d.gtx", patch_x, patch_y);
                break;
            case 1:
                std::snprintf(
                    name, sizeof(name), "MiniMap/e/minimap_n_%03d_%03d.gtx", patch_x, patch_y);
                break;
            default:
                std::snprintf(
                    name, sizeof(name), "MiniMap/minimap_n_%03d_%03d.gtx", patch_x, patch_y);
                break;
            }
            return name;
        }

        std::string find_tile_name(WidgetTools& ui, int patch_x, int patch_y) {
            for (int variant = 0; variant < 3; ++variant) {
                const std::string name = tile_filename(patch_x, patch_y, variant);
                const auto info = ui.tex_info(name);
                if (info.handle() && info.width() >= 16 && info.height() >= 16) {
                    return name;
                }
            }
            return {};
        }

        struct VisibleTile {
            int patch_x = 0;
            int patch_y = 0;
            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;
            float distance_squared = 0.0f;
        };

        struct TileLoadProgress {
            std::size_t resolved = 0;
            std::size_t total = 0;
        };

        [[nodiscard]] std::size_t tile_index(int patch_x, int patch_y) {
            return static_cast<std::size_t>(patch_x / 2) * 32 +
                   static_cast<std::size_t>(patch_y / 2);
        }

        TileLoadProgress draw_overworld_tiles(WidgetTools& ui,
                                              float screen_w,
                                              float screen_h,
                                              const MinimapWidgetData& data,
                                              WorldMapWidget::TileLoadState& tile_state) {
            constexpr float kZoom = 5.0f;
            constexpr std::size_t kTilesPerFrame = 4;
            const float player_x = MinimapWidget::atlas_x(data.world_x());
            const float player_y = MinimapWidget::atlas_y(data.world_z());
            const float view_w = MinimapWidget::kAtlasSize / kZoom;
            const float view_h = MinimapWidget::kAtlasSize / kZoom;
            const float view_left = player_x - view_w * 0.5f;
            const float view_top = player_y - view_h * 0.5f;
            const float view_right = view_left + view_w;
            const float view_bottom = view_top + view_h;
            std::vector<VisibleTile> visible;
            visible.reserve(144);
            for (int patch_x = 0; patch_x < map::kPatchGrid; patch_x += 2) {
                for (int patch_y = 0; patch_y < map::kPatchGrid; patch_y += 2) {
                    float left = 0.0f;
                    float top = 0.0f;
                    float right = 0.0f;
                    float bottom = 0.0f;
                    MinimapWidget::tile_rect(patch_x, patch_y, left, top, right, bottom);
                    if (right <= view_left || bottom <= view_top || left >= view_right ||
                        top >= view_bottom) {
                        continue;
                    }
                    const float center_x = (left + right) * 0.5f - player_x;
                    const float center_y = (top + bottom) * 0.5f - player_y;
                    visible.push_back({patch_x,
                                       patch_y,
                                       left,
                                       top,
                                       right,
                                       bottom,
                                       center_x * center_x + center_y * center_y});
                }
            }

            std::sort(
                visible.begin(), visible.end(), [](const VisibleTile& lhs, const VisibleTile& rhs) {
                    return lhs.distance_squared < rhs.distance_squared;
                });
            std::size_t remaining_loads = kTilesPerFrame;
            for (const VisibleTile& tile : visible) {
                const std::size_t index = tile_index(tile.patch_x, tile.patch_y);
                if (!tile_state.resolved(index)) {
                    tile_state.resolve(index, find_tile_name(ui, tile.patch_x, tile.patch_y));
                    if (--remaining_loads == 0) {
                        break;
                    }
                }
            }

            TileLoadProgress progress;
            progress.total = visible.size();
            for (const VisibleTile& tile : visible) {
                const std::size_t index = tile_index(tile.patch_x, tile.patch_y);
                if (tile_state.resolved(index)) {
                    ++progress.resolved;
                }
                const std::string& name = tile_state.tile_name(index);
                if (name.empty()) {
                    continue;
                }
                const float draw_left = std::max(tile.left, view_left);
                const float draw_top = std::max(tile.top, view_top);
                const float draw_right = std::min(tile.right, view_right);
                const float draw_bottom = std::min(tile.bottom, view_bottom);
                const float u0 = (draw_left - tile.left) / MinimapWidget::kTileSize;
                const float v0 = (draw_top - tile.top) / MinimapWidget::kTileSize;
                const float u1 = (draw_right - tile.left) / MinimapWidget::kTileSize;
                const float v1 = (draw_bottom - tile.top) / MinimapWidget::kTileSize;
                ui.draw_tex_uv(name,
                               (draw_left - view_left) * screen_w / view_w,
                               (draw_top - view_top) * screen_h / view_h,
                               (draw_right - draw_left) * screen_w / view_w,
                               (draw_bottom - draw_top) * screen_h / view_h,
                               u0,
                               v0,
                               u1,
                               v1);
            }
            return progress;
        }
    }  // namespace

    void WorldMapWidget::marker_position(
        float screen_w, float screen_h, const MinimapWidgetData& data, float& out_x, float& out_y) {
        const float atlas_size = std::max(1.0f, MinimapWidget::kAtlasSize);
        out_x = std::clamp(MinimapWidget::atlas_x(data.world_x()) / atlas_size, 0.0f, 1.0f) *
                std::max(0.0f, screen_w);
        out_y = std::clamp(MinimapWidget::atlas_y(data.world_z()) / atlas_size, 0.0f, 1.0f) *
                std::max(0.0f, screen_h);
    }

    bool
    WorldMapWidget::close_contains(float mouse_x, float mouse_y, float screen_w, float screen_h) {
        const float x = std::max(0.0f, screen_w - kCloseWidth - kCloseMargin);
        const float y = std::max(0.0f, screen_h - kCloseHeight - kCloseMargin);
        return WidgetTools::point_in_rect(mouse_x, mouse_y, x, y, kCloseWidth, kCloseHeight);
    }

    void WorldMapWidget::draw(WidgetTools& ui,
                              engine::IEngine& engine,
                              const FreetypeFont& font,
                              float screen_w,
                              float screen_h,
                              const MinimapWidgetData& data,
                              TileLoadState& tile_state) {
        if (screen_w <= 0.5f || screen_h <= 0.5f) {
            return;
        }

        // The viewport uses the same minimap sections as the HUD, centred on the
        // player at 3x zoom. Only a few unresolved sections are loaded per frame.
        engine.draw_ui_rect(0.0f, 0.0f, screen_w, screen_h, 0.01f, 0.02f, 0.04f, 0.82f);
        const TileLoadProgress load_progress =
            draw_overworld_tiles(ui, screen_w, screen_h, data, tile_state);

        const float marker_x = screen_w * 0.5f;
        const float marker_y = screen_h * 0.5f;
        const float marker_size = std::clamp(std::min(screen_w, screen_h) * 0.035f, 16.0f, 32.0f);
        const auto camera = ui.tex_info("camera.GTX");
        if (camera.handle()) {
            ui.draw_tex_rotated("camera.GTX",
                                marker_x - marker_size * 0.5f,
                                marker_y - marker_size * 0.5f,
                                marker_size,
                                marker_size,
                                data.camera_yaw_rad());
        } else {
            draw_camera_fallback(
                engine, marker_x, marker_y, data.camera_yaw_rad(), marker_size / 20.0f);
        }
        const auto hero = ui.tex_info("mmaphero.GTX");
        if (hero.handle()) {
            ui.draw_tex_rotated("mmaphero.GTX",
                                marker_x - marker_size * 0.5f,
                                marker_y - marker_size * 0.5f,
                                marker_size,
                                marker_size,
                                data.player_yaw_rad() + 3.14159265358979323846f);
        } else {
            engine.draw_ui_rect(
                marker_x - 4.0f, marker_y - 4.0f, 8.0f, 8.0f, 1.0f, 0.20f, 0.20f, 1.0f);
        }

        const float close_x = std::max(0.0f, screen_w - kCloseWidth - kCloseMargin);
        const float close_y = std::max(0.0f, screen_h - kCloseHeight - kCloseMargin);
        engine.draw_ui_rect(
            close_x, close_y, kCloseWidth, kCloseHeight, 0.02f, 0.02f, 0.03f, 0.78f);
        if (font.ready()) {
            font.draw_centered(engine,
                               close_x + kCloseWidth * 0.5f,
                               close_y + 8.0f,
                               "Close (M / Esc)",
                               0.92f,
                               0.86f,
                               0.67f,
                               0.56f,
                               1.0f);
            if (load_progress.resolved < load_progress.total) {
                const float loading_w = 240.0f;
                const float loading_h = 32.0f;
                const float loading_x = std::max(0.0f, (screen_w - loading_w) * 0.5f);
                engine.draw_ui_rect(
                    loading_x, 16.0f, loading_w, loading_h, 0.01f, 0.01f, 0.02f, 0.78f);
                const std::string progress =
                    "Loading map: " + std::to_string(load_progress.resolved) + " / " +
                    std::to_string(load_progress.total);
                font.draw_centered(
                    engine, screen_w * 0.5f, 24.0f, progress, 0.92f, 0.86f, 0.67f, 0.56f, 1.0f);
            }
        }
    }
}  // namespace openkal::client
