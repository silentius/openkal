/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MinimapWidget.h"

#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"
#include "openkal/engine/engine.h"
#include "openkal/map/Coords.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace openkal::client {
    namespace {

        class CircleClip {
          public:
            CircleClip(engine::IEngine* renderer, float cx, float cy, float radius)
                : m_renderer(radius > 0.0f ? renderer : nullptr) {
                if (m_renderer) {
                    m_renderer->set_ui_clip_circle(cx, cy, radius);
                }
            }
            ~CircleClip() {
                clear();
            }
            CircleClip(const CircleClip&) = delete;
            CircleClip& operator=(const CircleClip&) = delete;
            void clear() {
                if (m_renderer) {
                    m_renderer->set_ui_clip_circle(0, 0, 0);
                    m_renderer = nullptr;
                }
            }

          private:
            engine::IEngine* m_renderer;
        };

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

        std::string dungeon_tile_filename(const std::string& stem, int index, bool upper_ext) {
            char name[128];
            std::snprintf(name,
                          sizeof(name),
                          "MiniMap/minimap_Dungeon_%s_%03d.%s",
                          stem.c_str(),
                          index,
                          upper_ext ? "GTX" : "gtx");
            return name;
        }

        std::string find_dungeon_tile_name(WidgetTools& ui, const std::string& stem, int index) {
            for (int variant = 0; variant < 2; ++variant) {
                const std::string name = dungeon_tile_filename(stem, index, variant == 0);
                const auto info = ui.tex_info(name);
                if (info.handle() && info.width() >= 16 && info.height() >= 16) {
                    return name;
                }
            }
            return {};
        }

        void draw_dungeon_tiles(WidgetTools& ui,
                                const MinimapWidgetData& data,
                                float inner_x,
                                float inner_y,
                                float inner_w,
                                float inner_h,
                                float zoom) {
            const float ax = MinimapWidget::dungeon_atlas_x(data.world_x(),
                                                            data.origin_patch(),
                                                            data.origin_cell_x(),
                                                            data.map_left(),
                                                            data.correction());
            const float ay = MinimapWidget::dungeon_atlas_y(data.world_z(),
                                                            data.origin_patch(),
                                                            data.origin_cell_y(),
                                                            data.map_bottom(),
                                                            data.correction(),
                                                            data.size_y());
            const float view_w = inner_w / zoom;
            const float view_h = inner_h / zoom;
            const float view_l = ax - view_w * 0.5f;
            const float view_t = ay - view_h * 0.5f;
            const float view_r = view_l + view_w;
            const float view_b = view_t + view_h;
            const int tile_w = std::max(1, data.tile_width());
            const int tile_h = std::max(1, data.tile_height());
            const int cols = std::max(1, data.size_x() / tile_w);
            const int rows = std::max(1, data.size_y() / tile_h);
            const int max_tiles = cols * rows;
            const float tile_wf = static_cast<float>(tile_w);
            const float tile_hf = static_cast<float>(tile_h);
            for (int index = 0; index < max_tiles; ++index) {
                const std::string name = find_dungeon_tile_name(ui, data.mapname(), index);
                if (name.empty()) {
                    break;
                }
                float tl = 0.0f;
                float tt = 0.0f;
                float tr = 0.0f;
                float tb = 0.0f;
                MinimapWidget::dungeon_tile_rect(
                    index, tile_w, tile_h, data.size_x(), tl, tt, tr, tb);
                const float il = std::max(tl, view_l);
                const float it = std::max(tt, view_t);
                const float ir = std::min(tr, view_r);
                const float ib = std::min(tb, view_b);
                if (ir - il < 0.5f || ib - it < 0.5f) {
                    continue;
                }
                const float u0 = (il - tl) / tile_wf;
                const float v0 = (it - tt) / tile_hf;
                const float u1 = (ir - tl) / tile_wf;
                const float v1 = (ib - tt) / tile_hf;
                ui.draw_tex_uv(name,
                               inner_x + (il - view_l) * zoom,
                               inner_y + (it - view_t) * zoom,
                               (ir - il) * zoom,
                               (ib - it) * zoom,
                               u0,
                               v0,
                               u1,
                               v1);
            }
        }

        constexpr float kZoomPlusX = 185.0f;
        constexpr float kZoomPlusY = 147.0f;
        constexpr float kZoomMinusX = 185.0f;
        constexpr float kZoomMinusY = 170.0f;
        constexpr float kZoomButtonSize = 16.0f;

        bool scaled_hit_rect(float panel_x,
                             float panel_y,
                             float panel_w,
                             float panel_h,
                             const UiLayout* layout,
                             std::string_view key,
                             float native_x,
                             float native_y,
                             float native_w,
                             float native_h,
                             float& x,
                             float& y,
                             float& w,
                             float& h) {
            UiDocument doc;
            UiView::reload(doc, layout);
            if (const UiWidget* btn = doc.find_widget(std::string(key))) {
                native_x = btn->x();
                native_y = btn->y();
                if (btn->w() > 0.0f) {
                    native_w = btn->w();
                }
                if (btn->h() > 0.0f) {
                    native_h = btn->h();
                }
            }
            const float sx =
                panel_w /
                std::max(1.0f, doc.window_w() > 0 ? doc.window_w() : MinimapWidget::kNativeWidth);
            const float sy =
                panel_h /
                std::max(1.0f, doc.window_h() > 0 ? doc.window_h() : MinimapWidget::kNativeHeight);
            x = panel_x + native_x * sx;
            y = panel_y + native_y * sy;
            w = native_w * sx;
            h = native_h * sy;
            return w > 0.5f && h > 0.5f;
        }

        void draw_camera_fallback(
            engine::IEngine& engine, float cx, float cy, float yaw_rad, float scale) {
            const float s = std::max(0.25f, scale);
            const float dx = std::sin(yaw_rad);
            const float dy = -std::cos(yaw_rad);
            const float len = 9.0f * s;
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

    }  // namespace

    bool MinimapWidget::zoom_plus_rect(float panel_x,
                                       float panel_y,
                                       float panel_w,
                                       float panel_h,
                                       const UiLayout* layout,
                                       float& x,
                                       float& y,
                                       float& w,
                                       float& h) {
        return scaled_hit_rect(panel_x,
                               panel_y,
                               panel_w,
                               panel_h,
                               layout,
                               "zoom_plus",
                               kZoomPlusX,
                               kZoomPlusY,
                               kZoomButtonSize,
                               kZoomButtonSize,
                               x,
                               y,
                               w,
                               h);
    }

    bool MinimapWidget::zoom_minus_rect(float panel_x,
                                        float panel_y,
                                        float panel_w,
                                        float panel_h,
                                        const UiLayout* layout,
                                        float& x,
                                        float& y,
                                        float& w,
                                        float& h) {
        return scaled_hit_rect(panel_x,
                               panel_y,
                               panel_w,
                               panel_h,
                               layout,
                               "zoom_minus",
                               kZoomMinusX,
                               kZoomMinusY,
                               kZoomButtonSize,
                               kZoomButtonSize,
                               x,
                               y,
                               w,
                               h);
    }

    bool MinimapWidget::contains(
        float mx, float my, float x, float y, float w, float h, const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const UiWidget* map = doc.find_widget("map");
        if (!map || map->shape() != "circle") {
            return WidgetTools::point_in_rect(mx, my, x, y, w, h);
        }
        const float sx = w / std::max(1.0f, doc.window_w());
        const float sy = h / std::max(1.0f, doc.window_h());
        const float dx = mx - x - (map->x() + map->w() * 0.5f) * sx;
        const float dy = my - y - (map->y() + map->h() * 0.5f) * sy;
        const float radius =
            std::min(map->w() * sx, map->h() * sy) * 0.5f + 8.0f * std::min(sx, sy);
        if (dx * dx + dy * dy <= radius * radius) {
            return true;
        }
        // Named plaques and zoom controls are independent of the transparent frame corners.
        for (const char* id : {"area_plate", "coordinates_plate", "zoom_plus", "zoom_minus"}) {
            const UiWidget* widget = doc.find_widget(id);
            if (widget && widget->visible() &&
                WidgetTools::point_in_rect(mx,
                                           my,
                                           x + widget->x() * sx,
                                           y + widget->y() * sy,
                                           widget->w() * sx,
                                           widget->h() * sy)) {
                return true;
            }
        }
        return false;
    }

    std::string MinimapWidget::coordinates_text(const MinimapWidgetData& data) {
        int tx = 0;
        int ty = 0;
        coords::pixel_to_tile(coords::to_server(data.world_x(), data.origin_patch()),
                              coords::to_server(data.world_z(), data.origin_patch()),
                              tx,
                              ty);
        return std::to_string(tx) + ", " + std::to_string(ty);
    }

    void MinimapWidget::draw(WidgetTools& ui,
                             const FreetypeFont& font,
                             const FreetypeFont& bold,
                             float x,
                             float y,
                             float w,
                             float h,
                             const MinimapWidgetData& data,
                             const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const std::string bg =
            !doc.chrome_file().empty() ? doc.chrome_file() : "main-mini map-open.bmp";
        const auto frame = ui.tex_info(bg);
        const float native_w =
            doc.window_w() > 0
                ? doc.window_w()
                : (frame.width() > 0 ? static_cast<float>(frame.width()) : kNativeWidth);
        const float native_h =
            doc.window_h() > 0
                ? doc.window_h()
                : (frame.height() > 0 ? static_cast<float>(frame.height()) : kNativeHeight);
        const float sx = w / std::max(1.0f, native_w);
        const float sy = h / std::max(1.0f, native_h);

        float inset_l = 19.0f * sx;
        float inset_t = 42.0f * sy;
        float inset_r = 23.0f * sx;
        float inset_b = 13.0f * sy;
        const UiWidget* map_widget = doc.find_widget("map");
        const bool circular = map_widget && map_widget->shape() == "circle";
        if (const UiWidget* map = map_widget) {
            inset_l = map->x() * sx;
            inset_t = map->y() * sy;
            inset_r = std::max(0.0f, native_w - map->x() - map->w()) * sx;
            inset_b = std::max(0.0f, native_h - map->y() - map->h()) * sy;
        }
        const float zoom = static_cast<float>(sanitize_zoom(data.zoom()));

        float inner_x = x + inset_l;
        float inner_y = y + inset_t;
        float inner_w = std::max(1.0f, w - inset_l - inset_r);
        float inner_h = std::max(1.0f, h - inset_t - inset_b);
        if (circular) {
            const float diameter = std::min(inner_w, inner_h);
            inner_x += (inner_w - diameter) * 0.5f;
            inner_y += (inner_h - diameter) * 0.5f;
            inner_w = inner_h = diameter;
        }
        CircleClip clip(ui.renderer(),
                        inner_x + inner_w * 0.5f,
                        inner_y + inner_h * 0.5f,
                        circular ? inner_w * 0.5f : 0.0f);

        if (ui.renderer()) {
            ui.renderer()->draw_ui_rect(
                inner_x, inner_y, inner_w, inner_h, 0.05f, 0.07f, 0.1f, 1.0f);
        }

        if (data.dungeon() && !data.mapname().empty()) {
            draw_dungeon_tiles(ui, data, inner_x, inner_y, inner_w, inner_h, zoom);
        } else {
            const float ax = atlas_x(data.world_x());
            const float ay = atlas_y(data.world_z());
            const float view_w = inner_w / zoom;
            const float view_h = inner_h / zoom;
            const float view_l = ax - view_w * 0.5f;
            const float view_t = ay - view_h * 0.5f;
            const float view_r = view_l + view_w;
            const float view_b = view_t + view_h;

            for (int patch_x = 0; patch_x < map::kPatchGrid; patch_x += 2) {
                for (int patch_y = 0; patch_y < map::kPatchGrid; patch_y += 2) {
                    float tl = 0.0f;
                    float tt = 0.0f;
                    float tr = 0.0f;
                    float tb = 0.0f;
                    tile_rect(patch_x, patch_y, tl, tt, tr, tb);
                    const float il = std::max(tl, view_l);
                    const float it = std::max(tt, view_t);
                    const float ir = std::min(tr, view_r);
                    const float ib = std::min(tb, view_b);
                    if (ir - il < 0.5f || ib - it < 0.5f) {
                        continue;
                    }
                    const std::string name = find_tile_name(ui, patch_x, patch_y);
                    if (name.empty()) {
                        continue;
                    }
                    const float u0 = (il - tl) / kTileSize;
                    const float v0 = (it - tt) / kTileSize;
                    const float u1 = (ir - tl) / kTileSize;
                    const float v1 = (ib - tt) / kTileSize;
                    ui.draw_tex_uv(name,
                                   inner_x + (il - view_l) * zoom,
                                   inner_y + (it - view_t) * zoom,
                                   (ir - il) * zoom,
                                   (ib - it) * zoom,
                                   u0,
                                   v0,
                                   u1,
                                   v1);
                }
            }
        }

        const float cx = inner_x + inner_w * 0.5f;
        const float cy = inner_y + inner_h * 0.5f;
        const float marker = 16.0f * std::min(sx, sy);
        const auto camera = ui.tex_info("camera.GTX");
        if (camera.handle()) {
            ui.draw_tex_rotated("camera.GTX",
                                cx - marker * 0.5f,
                                cy - marker * 0.5f,
                                marker,
                                marker,
                                data.camera_yaw_rad());
        } else if (ui.renderer()) {
            draw_camera_fallback(*ui.renderer(), cx, cy, data.camera_yaw_rad(), std::min(sx, sy));
        }
        const auto hero = ui.tex_info("mmaphero.GTX");
        if (hero.handle()) {
            ui.draw_tex_rotated("mmaphero.GTX",
                                cx - marker * 0.5f,
                                cy - marker * 0.5f,
                                marker,
                                marker,
                                data.player_yaw_rad() + 3.14159265358979323846f);
        } else if (ui.renderer()) {
            ui.renderer()->draw_ui_rect(
                cx - 3.0f * sx, cy - 3.0f * sy, 6.0f * sx, 6.0f * sy, 1.0f, 0.2f, 0.2f, 1.0f);
        }

        clip.clear();
        if (!doc.widgets().empty() && ui.renderer()) {
            UiVarMap vars;
            vars["minimap.area_name"] = !data.area_name().empty()
                                            ? data.area_name()
                                            : (data.dungeon() ? "Dungeon" : "Naraeha");
            vars["minimap.coordinates"] = coordinates_text(data);
            vars["minimap.north"] = "N";
            vars["minimap.zoom_in_label"] = "+";
            vars["minimap.zoom_out_label"] = "−";
            ui_fill_theme_vars(doc, vars);
            UiView::draw(*ui.renderer(),
                         ui,
                         doc,
                         x,
                         y,
                         w,
                         h,
                         std::min(sx, sy),
                         vars,
                         font,
                         bold,
                         nullptr,
                         false);
        } else {
            ui.draw_tex(bg, x, y, w, h);
        }
    }

}  // namespace openkal::client
