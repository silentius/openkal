/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WidgetTools.h"

#include "openkal/client/LatinText.h"
#include "openkal/client/UiDocument.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace openkal::client {

    WindowFrameStyle WindowFrameStyle::info_menu(bool close_button) {
        WindowFrameStyle style;
        style.set_padding(10.0f, 40.0f, 7.0f, 15.0f);
        style.set_close_enabled(close_button);
        style.set_slice_files("info-menu-top left.bmp",
                              "info-menu-top center.bmp",
                              close_button ? "info-menu-top right.bmp"
                                           : "info-menu-top right - not close.bmp",
                              "info-menu-left.bmp",
                              "info-menu-right.bmp",
                              "info-menu-bottom left.bmp",
                              "info-menu-bottom center.bmp",
                              "info-menu-bottom right.bmp");
        style.set_title_drag_h(44.0f);
        style.set_tile_edges(false);
        style.set_draw_exit(false);
        return style;
    }

    void WidgetTools::init(engine::IEngine& eng,
                           const std::string& data_root,
                           const std::string& runtime_data_root) {
        m_engine = &eng;
        // OpenKal icons override external client assets, including case variants.
        m_search_dirs = {
            platform::join_path(
                runtime_data_root.empty() ? OPENKAL_RUNTIME_DATA_ROOT : runtime_data_root, "UI"),
            platform::join_path(runtime_data_root.empty() ? OPENKAL_RUNTIME_DATA_ROOT
                                                          : runtime_data_root,
                                "HyperText"),
            platform::join_path(data_root, "UI"),
            platform::join_path(data_root, "HyperText"),
        };
        build_font();
        (void) m_dragon_brand_font.load(eng, FreetypeFont::find_system_sans_bold(), 12);
    }

    void WidgetTools::add_search_dir(std::string dir) {
        if (dir.empty()) {
            return;
        }
        for (const auto& existing : m_search_dirs) {
            if (existing == dir) {
                return;
            }
        }
        m_search_dirs.push_back(std::move(dir));
    }

    void WidgetTools::destroy() {
        if (!m_engine) {
            return;
        }
        for (auto& [k, t] : m_cache) {
            (void) k;
            if (t.handle()) {
                m_engine->destroy_texture(t.handle());
            }
        }
        m_cache.clear();
        m_resolve_cache.clear();
        m_dragon_brand_font.destroy(*m_engine);
        if (m_font_tex) {
            m_engine->destroy_texture(m_font_tex);
            m_font_tex = {};
        }
    }

    void WidgetTools::invalidate_tex(const std::string& filename) {
        if (filename.empty()) {
            return;
        }
        auto erase_key = [&](const std::string& key) {
            const auto it = m_cache.find(key);
            if (it == m_cache.end()) {
                return;
            }
            if (m_engine && it->second.handle()) {
                m_engine->destroy_texture(it->second.handle());
            }
            m_cache.erase(it);
        };
        erase_key(filename);
        erase_key(std::string("\x01item:") + filename);
        erase_key(std::string("\x01gray-item:") + filename);
        m_resolve_cache.erase(filename);
    }

    namespace {
        bool looks_like_gtx_or_dds(const std::string& name) {
            const auto dot = name.find_last_of('.');
            if (dot == std::string::npos || dot + 1 >= name.size()) {
                return false;
            }
            std::string ext = name.substr(dot);
            for (char& c : ext) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return ext == ".gtx" || ext == ".dds";
        }

        bool load_ui_image(const std::string& path, terrain::ImageRgba& img) {
            if (looks_like_gtx_or_dds(path)) {
                return terrain::load_gtx_rgba(path, img) && !img.rgba.empty();
            }
            if (WidgetTools::load_bmp_rgba(path, img) && !img.rgba.empty()) {
                return true;
            }
            return terrain::load_gtx_rgba(path, img) && !img.rgba.empty();
        }
    }  // namespace

    TexInfo WidgetTools::tex_info(const std::string& filename) {
        return load_cached_tex(filename, false);
    }

    TexInfo WidgetTools::item_tex_info(const std::string& filename) {
        return load_cached_tex(filename, true);
    }

    TexInfo
    WidgetTools::load_cached_tex(const std::string& filename, bool item_icon, bool grayscale) {
        const std::string key =
            grayscale ? (std::string("\x01gray-item:") + filename)
                      : (item_icon ? (std::string("\x01item:") + filename) : filename);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }
        terrain::ImageRgba img;
        bool ok = false;
        const std::string resolved = resolve_bmp(filename);
        const std::string& load_name = resolved.empty() ? filename : resolved;
        for (const auto& dir : m_search_dirs) {
            // Case-sensitive fast path, then case-fixed path (Linux HyperText casing
            // varies).
            std::string path = platform::join_path(dir, load_name);
            if (!platform::file_exists(path)) {
                path = platform::resolve_path_case(dir, load_name);
            }
            if (platform::file_exists(path) && load_ui_image(path, img)) {
                ok = true;
                break;
            }
        }
        TexInfo info;
        if (!ok) {
            m_cache.emplace(key, info);
            return info;
        }
        // Dark trough patches → transparent (soft edge), like Kal near-black color
        // keys.
        if (!item_icon &&
            (load_name == "skill list(re)-image2.bmp" || filename == "skill list(re)-image2.bmp" ||
             load_name == "skill list(re)-image.bmp" || filename == "skill list(re)-image.bmp")) {
            apply_dark_patch_alpha(img);
        }
        if (item_icon) {
            apply_item_icon_color_key(img);
        }
        if (grayscale) {
            for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
                const auto gray = static_cast<std::uint8_t>(
                    (299u * img.rgba[i] + 587u * img.rgba[i + 1] + 114u * img.rgba[i + 2] + 500u) /
                    1000u);
                img.rgba[i] = img.rgba[i + 1] = img.rgba[i + 2] = gray;
            }
        }
        info.set_handle(
            m_engine->create_texture_rgba8(img.rgba.data(), img.width, img.height, false));
        info.set_size(img.width, img.height);
        m_cache.emplace(key, info);
        return info;
    }

    std::string WidgetTools::resolve_bmp(const std::string& stem_or_file) const {
        if (stem_or_file.empty()) {
            return {};
        }
        const auto cached = m_resolve_cache.find(stem_or_file);
        if (cached != m_resolve_cache.end()) {
            return cached->second;
        }

        auto try_name = [&](const std::string& name) -> std::string {
            for (const auto& dir : m_search_dirs) {
                if (platform::file_exists(platform::join_path(dir, name))) {
                    return name;
                }
                const std::string fixed = platform::resolve_path_case(dir, name);
                // resolve_path_case returns full path — take basename.
                if (platform::file_exists(fixed)) {
                    const auto slash = fixed.find_last_of('/');
                    return slash == std::string::npos ? fixed : fixed.substr(slash + 1);
                }
            }
            return {};
        };

        std::string result;
        if (auto hit = try_name(stem_or_file); !hit.empty()) {
            result = std::move(hit);
        } else {
            // Append .bmp / .BMP when caller passed a stem (active006, Passive001, …).
            const bool has_dot = stem_or_file.find('.') != std::string::npos;
            if (!has_dot) {
                if (auto hit = try_name(stem_or_file + ".bmp"); !hit.empty()) {
                    result = std::move(hit);
                } else if (auto hit = try_name(stem_or_file + ".BMP"); !hit.empty()) {
                    result = std::move(hit);
                } else {
                    result = stem_or_file;
                }
            } else {
                result = stem_or_file;
            }
        }
        m_resolve_cache.emplace(stem_or_file, result);
        return result;
    }

    void WidgetTools::apply_dark_patch_alpha(terrain::ImageRgba& img) {
        if (img.width <= 0 || img.height <= 0 ||
            img.rgba.size() < static_cast<std::size_t>(img.width * img.height * 4)) {
            return;
        }
        // Kal UI often color-keys near-black (e.g. RGB(15,15,15) / sum≈45). A hard
        // threshold alone makes a sawtooth on the dithered rail; ramp alpha across
        // the edge instead. Tune these if troughs look too opaque or rails too soft:
        constexpr int kClearSum = 52;    // sum <= this → fully transparent (trough fill)
        constexpr int kOpaqueSum = 100;  // sum >= this → leave alpha
        for (int i = 0; i < img.width * img.height; ++i) {
            const std::size_t o = static_cast<std::size_t>(i * 4);
            const int r = img.rgba[o + 0];
            const int g = img.rgba[o + 1];
            const int b = img.rgba[o + 2];
            // Skip magenta (already keyed in load_bmp_rgba).
            if (r > 250 && g < 5 && b > 250) {
                continue;
            }
            const int sum = r + g + b;
            if (sum <= kClearSum) {
                img.rgba[o + 3] = 0;
            } else if (sum < kOpaqueSum) {
                const int t = sum - kClearSum;
                const int span = kOpaqueSum - kClearSum;
                img.rgba[o + 3] = static_cast<std::uint8_t>((t * 255) / span);
            }
        }
    }

    void WidgetTools::apply_item_icon_color_key(terrain::ImageRgba& img) {
        if (img.width <= 0 || img.height <= 0 ||
            img.rgba.size() < static_cast<std::size_t>(img.width * img.height * 4)) {
            return;
        }
        for (int i = 0; i < img.width * img.height; ++i) {
            const std::size_t o = static_cast<std::size_t>(i * 4);
            const int r = img.rgba[o + 0];
            const int g = img.rgba[o + 1];
            const int b = img.rgba[o + 2];
            // Item BMP fill RGB(4,4,4) and compatibility behavior RGB(15,15,15).
            if ((r <= 4 && g <= 4 && b <= 4) || (r == 15 && g == 15 && b == 15)) {
                img.rgba[o + 3] = 0;
            }
        }
    }

    void WidgetTools::draw_tex_segments(const char* segment_bmp,
                                        float x0,
                                        float y,
                                        float seg_w,
                                        float seg_h,
                                        float gap,
                                        int count) {
        if (!m_engine || count <= 0 || seg_w <= 0.5f || seg_h <= 0.5f) {
            return;
        }
        const auto info = tex_info(segment_bmp);
        if (!info.handle()) {
            return;
        }
        float x = x0;
        for (int i = 0; i < count; ++i) {
            m_engine->draw_ui_quad(info.handle(), x, y, seg_w, seg_h, 1, 1, 1, 1);
            x += seg_w + gap;
        }
    }

    void WidgetTools::draw_tex_native(const std::string& filename, float x, float y, float a) {
        const auto info = tex_info(filename);
        if (info.handle()) {
            m_engine->draw_ui_quad(info.handle(),
                                   x,
                                   y,
                                   static_cast<float>(info.width()),
                                   static_cast<float>(info.height()),
                                   1,
                                   1,
                                   1,
                                   a);
        }
    }

    void WidgetTools::draw_tex(
        const std::string& filename, float x, float y, float w, float h, float a) {
        draw_tex(filename, x, y, w, h, 1.0f, 1.0f, 1.0f, a);
    }

    void WidgetTools::draw_tex(const std::string& filename,
                               float x,
                               float y,
                               float w,
                               float h,
                               float r,
                               float g,
                               float b,
                               float a) {
        auto t = tex(filename);
        if (t) {
            m_engine->draw_ui_quad(t, x, y, w, h, r, g, b, a);
        } else {
            m_engine->draw_ui_rect(x, y, w, h, r * 0.2f, g * 0.2f, b * 0.25f, a);
        }
    }

    void WidgetTools::draw_tex_rotated(const std::string& filename,
                                       float x,
                                       float y,
                                       float w,
                                       float h,
                                       float rotation_rad,
                                       float a) {
        const auto texture = tex(filename);
        if (texture) {
            m_engine->draw_ui_quad_rotated(texture, x, y, w, h, rotation_rad, 1.0f, 1.0f, 1.0f, a);
        }
    }

    void WidgetTools::draw_item_tex(
        const std::string& filename, float x, float y, float w, float h, float a, bool grayscale) {
        auto t = load_cached_tex(filename, true, grayscale).handle();
        if (t) {
            m_engine->draw_ui_quad(t, x, y, w, h, 1.0f, 1.0f, 1.0f, a);
        } else {
            m_engine->draw_ui_rect(x, y, w, h, 0.2f, 0.2f, 0.25f, a);
        }
    }

    void WidgetTools::draw_tex_uv(const std::string& filename,
                                  float x,
                                  float y,
                                  float w,
                                  float h,
                                  float u0,
                                  float v0,
                                  float u1,
                                  float v1,
                                  float a) {
        if (!m_engine || w <= 0.5f || h <= 0.5f) {
            return;
        }
        const auto info = tex_info(filename);
        if (!info.handle()) {
            return;
        }
        m_engine->draw_ui_quad(info.handle(), x, y, w, h, 1, 1, 1, a, u0, v0, u1, v1);
    }

    void WidgetTools::window_outer_rect(float client_x,
                                        float client_y,
                                        float client_w,
                                        float client_h,
                                        float scale,
                                        const WindowFrameStyle& style,
                                        float& out_x,
                                        float& out_y,
                                        float& out_w,
                                        float& out_h) {
        const float s = std::max(0.25f, scale);
        out_x = client_x - style.pad_left() * s;
        out_y = client_y - style.pad_top() * s;
        out_w = client_w + (style.pad_left() + style.pad_right()) * s;
        out_h = client_h + (style.pad_top() + style.pad_bottom()) * s;
    }

    bool WidgetTools::window_outer_hit(float mx,
                                       float my,
                                       float client_x,
                                       float client_y,
                                       float client_w,
                                       float client_h,
                                       float scale,
                                       const WindowFrameStyle& style) {
        float ox = 0, oy = 0, ow = 0, oh = 0;
        window_outer_rect(client_x, client_y, client_w, client_h, scale, style, ox, oy, ow, oh);
        return point_in_rect(mx, my, ox, oy, ow, oh);
    }

    void WidgetTools::window_close_rect(float client_x,
                                        float client_y,
                                        float client_w,
                                        float client_h,
                                        float scale,
                                        const WindowFrameStyle& style,
                                        float& out_x,
                                        float& out_y,
                                        float& out_w,
                                        float& out_h) {
        const float s = std::max(0.25f, scale);
        if (style.close_override()) {
            out_x = client_x + client_w + style.close_ox() * s;
            out_y = client_y + style.close_oy() * s;
            out_w = style.close_w() * s;
            out_h = style.close_h() * s;
            return;
        }
        // Default: top-right corner of the outer frame (baked X in info-menu-top
        // right).
        float fx = 0, fy = 0, fw = 0, fh = 0;
        window_outer_rect(client_x, client_y, client_w, client_h, scale, style, fx, fy, fw, fh);
        out_w = 36.0f * s;
        out_h = 36.0f * s;
        out_x = fx + fw - out_w;
        out_y = fy;
        (void) fh;
    }

    bool WidgetTools::window_close_hit(float mx,
                                       float my,
                                       float client_x,
                                       float client_y,
                                       float client_w,
                                       float client_h,
                                       float scale,
                                       const WindowFrameStyle& style) {
        if (!style.close_enabled()) {
            return false;
        }
        float cx = 0, cy = 0, cw = 0, ch = 0;
        window_close_rect(client_x, client_y, client_w, client_h, scale, style, cx, cy, cw, ch);
        return point_in_rect(mx, my, cx, cy, cw, ch);
    }

    void WidgetTools::window_title_drag_rect(float client_x,
                                             float client_y,
                                             float client_w,
                                             float client_h,
                                             float scale,
                                             const WindowFrameStyle& style,
                                             float& out_x,
                                             float& out_y,
                                             float& out_w,
                                             float& out_h) {
        const float s = std::max(0.25f, scale);
        float fx = 0, fy = 0, fw = 0, fh = 0;
        window_outer_rect(client_x, client_y, client_w, client_h, scale, style, fx, fy, fw, fh);
        out_x = fx;
        out_y = fy;
        out_w = fw;
        if (style.close_enabled()) {
            float cx = 0, cy = 0, cw = 0, ch = 0;
            window_close_rect(client_x, client_y, client_w, client_h, scale, style, cx, cy, cw, ch);
            out_w = std::max(0.0f, cx - fx);
            (void) cy;
            (void) ch;
        }
        out_h = std::max(style.title_drag_h() * s, style.pad_top() * s);
        (void) fh;
    }

    void WidgetTools::draw_window_frame(float client_x,
                                        float client_y,
                                        float client_w,
                                        float client_h,
                                        float scale,
                                        const WindowFrameStyle& style) {
        // 9-slice: edges first, corners last so corners own the overlaps.
        const auto tl = tex_info(style.upleft());
        const auto tc = tex_info(style.upcenter());
        const auto tr = tex_info(style.upright());
        const auto l = tex_info(style.left());
        const auto r = tex_info(style.right());
        const auto bl = tex_info(style.downleft());
        const auto bc = tex_info(style.downcenter());
        const auto br = tex_info(style.downright());

        auto native = [](int v, float fallback) {
            return v > 0 ? static_cast<float>(v) : fallback;
        };
        const UiNineSliceLayout lay = ui_compute_nine_slice(client_x,
                                                            client_y,
                                                            client_w,
                                                            client_h,
                                                            scale,
                                                            style.pad_left(),
                                                            style.pad_top(),
                                                            style.pad_right(),
                                                            style.pad_bottom(),
                                                            native(tl.width(), 35.0f),
                                                            native(tl.height(), 53.0f),
                                                            native(tc.width(), 100.0f),
                                                            native(tc.height(), 47.0f),
                                                            native(tr.width(), 38.0f),
                                                            native(tr.height(), 53.0f),
                                                            native(l.width(), 18.0f),
                                                            native(l.height(), 30.0f),
                                                            native(r.width(), 18.0f),
                                                            native(r.height(), 30.0f),
                                                            native(bl.width(), 23.0f),
                                                            native(bl.height(), 52.0f),
                                                            native(bc.width(), 64.0f),
                                                            native(bc.height(), 16.0f),
                                                            native(br.width(), 23.0f),
                                                            native(br.height(), 52.0f),
                                                            style.tile_edges());

        auto tile_h = [&](const std::string& file,
                          float px,
                          float py,
                          float dest_w,
                          float dest_h,
                          float tile_w) {
            if (dest_w <= 1.0f || dest_h <= 1.0f || tile_w <= 1.0f) {
                return;
            }
            float remain = dest_w;
            float cx = px;
            while (remain > 0.5f) {
                const float dw = std::min(tile_w, remain);
                if (style.repeat_edges()) {
                    draw_tex_uv(file, cx, py, dw, dest_h, 0, 0, dw / tile_w, 1);
                } else {
                    draw_tex(file, cx, py, dw, dest_h);
                }
                cx += dw;
                remain -= dw;
            }
        };
        auto tile_v = [&](const std::string& file,
                          float px,
                          float py,
                          float dest_w,
                          float dest_h,
                          float tile_h_px) {
            if (dest_w <= 1.0f || dest_h <= 1.0f || tile_h_px <= 1.0f) {
                return;
            }
            float remain = dest_h;
            float cy = py;
            while (remain > 0.5f) {
                const float dh = std::min(tile_h_px, remain);
                if (style.repeat_edges()) {
                    draw_tex_uv(file, px, cy, dest_w, dh, 0, 0, 1, dh / tile_h_px);
                } else {
                    draw_tex(file, px, cy, dest_w, dh);
                }
                cy += dh;
                remain -= dh;
            }
        };

        if (style.tile_edges() || style.repeat_edges()) {
            tile_h(style.upcenter(),
                   lay.top().x(),
                   lay.top().y(),
                   lay.top().w(),
                   lay.top().h(),
                   lay.top_tile_w());
            tile_v(style.left(),
                   lay.left().x(),
                   lay.left().y(),
                   lay.left().w(),
                   lay.left().h(),
                   lay.left_tile_h());
            tile_v(style.right(),
                   lay.right().x(),
                   lay.right().y(),
                   lay.right().w(),
                   lay.right().h(),
                   lay.right_tile_h());
            tile_h(style.downcenter(),
                   lay.bot().x(),
                   lay.bot().y(),
                   lay.bot().w(),
                   lay.bot().h(),
                   lay.bot_tile_w());
        } else {
            if (lay.top().w() > 1.0f) {
                draw_tex(
                    style.upcenter(), lay.top().x(), lay.top().y(), lay.top().w(), lay.top().h());
            }
            if (lay.left().h() > 1.0f) {
                draw_tex(
                    style.left(), lay.left().x(), lay.left().y(), lay.left().w(), lay.left().h());
                draw_tex(style.right(),
                         lay.right().x(),
                         lay.right().y(),
                         lay.right().w(),
                         lay.right().h());
            }
            if (lay.bot().w() > 1.0f) {
                draw_tex(
                    style.downcenter(), lay.bot().x(), lay.bot().y(), lay.bot().w(), lay.bot().h());
            }
        }

        draw_tex(style.upleft(), lay.tl().x(), lay.tl().y(), lay.tl().w(), lay.tl().h());
        draw_tex(style.upright(), lay.tr().x(), lay.tr().y(), lay.tr().w(), lay.tr().h());
        draw_tex(style.downleft(), lay.bl().x(), lay.bl().y(), lay.bl().w(), lay.bl().h());
        draw_tex(style.downright(), lay.br().x(), lay.br().y(), lay.br().w(), lay.br().h());

        if (style.downcenter() == "dragon_dialog/frame_bottom.bmp" && m_engine) {
            const std::string key = "\x01"
                                    "dragon_footer_inlay";
            auto cached = m_cache.find(key);
            if (cached == m_cache.end()) {
                TexInfo inlay;
                for (const auto& dir : m_search_dirs) {
                    terrain::ImageRgba source;
                    if (!load_ui_image(platform::join_path(dir, "dragon_hud/inscription_panel.bmp"),
                                       source) ||
                        source.width < 32 || source.height < 98) {
                        continue;
                    }
                    // Turn the HUD's unlettered bronze center into a horizontal inlay.
                    constexpr int width = 76;
                    constexpr int height = 18;
                    std::vector<std::uint8_t> pixels(width * height * 4);
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            const auto src = ((x + 22) * source.width + y + 14) * 4;
                            const auto dst = (y * width + x) * 4;
                            std::copy_n(source.rgba.data() + src, 4, pixels.data() + dst);
                        }
                    }
                    inlay.set_handle(
                        m_engine->create_texture_rgba8(pixels.data(), width, height, false));
                    inlay.set_size(width, height);
                    break;
                }
                cached = m_cache.emplace(key, inlay).first;
            }
            const auto& inlay = cached->second;
            if (inlay.handle()) {
                const float s = std::max(0.25f, scale);
                const float tile_w = static_cast<float>(inlay.width()) * s;
                // Follow the native bottom-left ornament silhouette inside the footer band.
                constexpr int left_inset[] = {10, 10, 10, 10, 13, 14, 14, 15, 22, 23,
                                              24, 24, 24, 24, 36, 37, 38, 38, 39, 45};
                for (int row = 0; row < 20; ++row) {
                    const float width = (48.0f - left_inset[row]) * s;
                    m_engine->draw_ui_quad(inlay.handle(),
                                           client_x + left_inset[row] * s,
                                           client_y + client_h + (20.0f + row) * s,
                                           width,
                                           s,
                                           1,
                                           1,
                                           1,
                                           1,
                                           width / tile_w,
                                           row / 20.0f,
                                           0,
                                           (row + 1) / 20.0f);
                }
                const float inlay_w = std::max(0.0f, client_w - 64.0f * s);
                bool mirrored = false;
                for (float offset = 0; offset < inlay_w; offset += tile_w) {
                    const float width = std::min(tile_w, inlay_w - offset);
                    m_engine->draw_ui_quad(inlay.handle(),
                                           client_x + 48.0f * s + offset,
                                           client_y + client_h + 20.0f * s,
                                           width,
                                           20.0f * s,
                                           1,
                                           1,
                                           1,
                                           1,
                                           mirrored ? 1.0f : 0.0f,
                                           0,
                                           mirrored ? 1.0f - width / tile_w : width / tile_w,
                                           1);
                    mirrored = !mirrored;
                }
            }
        }

        if (style.downcenter() == "dragon_dialog/frame_bottom.bmp" && m_dragon_brand_font.ready() &&
            m_engine) {
            const float s = std::max(0.25f, scale);
            const std::string brand = "OpenKal";
            const float x =
                client_x + client_w - 30.0f * s - m_dragon_brand_font.text_width(brand, s);
            const float y = client_y + client_h + 22.0f * s;
            m_dragon_brand_font.draw(
                *m_engine, x + s, y + s, brand, 192.0f / 255, 160.0f / 255, 100.0f / 255, s);
            m_dragon_brand_font.draw(
                *m_engine, x, y, brand, 33.0f / 255, 28.0f / 255, 19.0f / 255, s);
        }
    }

    void WidgetTools::draw_window_vsplit(float client_x,
                                         float client_y,
                                         float client_w,
                                         float client_h,
                                         float split_x,
                                         float scale,
                                         const WindowFrameStyle& style) {
        const float s = std::max(0.25f, scale);
        float x = 0, y = 0, w = 0, h = 0;
        window_outer_rect(client_x, client_y, client_w, client_h, scale, style, x, y, w, h);
        (void) w;
        const auto l = tex_info(style.left());
        const auto tc = tex_info(style.upcenter());
        const auto bc = tex_info(style.downcenter());
        const float side_w = (l.width() > 0 ? static_cast<float>(l.width()) : 18.0f) * s;
        const float top_h = (tc.height() > 0 ? static_cast<float>(tc.height()) : 47.0f) * s;
        const float bot_h = (bc.height() > 0 ? static_cast<float>(bc.height()) : 16.0f) * s;
        const float mid_y = y + top_h;
        const float mid_h = std::max(0.0f, h - top_h - bot_h);
        if (mid_h <= 1.0f) {
            return;
        }
        const float sx = client_x + split_x - side_w * 0.5f;
        draw_tex(style.left(), sx, mid_y, side_w, mid_h);
    }

    void WidgetTools::draw_window_close(float client_x,
                                        float client_y,
                                        float client_w,
                                        float client_h,
                                        float scale,
                                        const WindowFrameStyle& style) {
        if (!style.close_enabled() || !style.draw_exit() || style.exit_file().empty()) {
            return;
        }
        const float s = std::max(0.25f, scale);
        const auto ex = tex_info(style.exit_file());
        const float ew = (ex.width() > 0 ? static_cast<float>(ex.width()) : 16.0f) * s;
        const float eh = (ex.height() > 0 ? static_cast<float>(ex.height()) : 16.0f) * s;
        float cx = 0, cy = 0, cw = 0, ch = 0;
        window_close_rect(client_x, client_y, client_w, client_h, scale, style, cx, cy, cw, ch);
        // Center the exit glyph in the close hitbox (close.ox/oy/w/h from layout).
        draw_tex(style.exit_file(), cx + (cw - ew) * 0.5f, cy + (ch - eh) * 0.5f, ew, eh);
    }

    void WidgetTools::draw_progress_lcr(
        const char* prefix, float x, float y, float full_w, float h, float ratio) {
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio > 1) {
            ratio = 1;
        }
        if (full_w <= 2.0f || h <= 0.5f) {
            return;
        }
        const std::string L = std::string(prefix) + "-L.bmp";
        const std::string C = std::string(prefix) + "-C.bmp";
        const std::string R = std::string(prefix) + "-R.bmp";
        const auto li = tex_info(L);
        const auto ci = tex_info(C);
        const auto ri = tex_info(R);
        if (!li.handle() || !ci.handle() || !ri.handle()) {
            return;
        }
        const float fill = full_w * ratio;
        if (fill < 1.0f) {
            return;
        }
        // Match KalOnline StretchBlt: L/R keep native aspect vs destination height.
        const float lw =
            std::max(1.0f, static_cast<float>(li.width()) * (h / std::max(1, li.height())));
        const float rw =
            std::max(1.0f, static_cast<float>(ri.width()) * (h / std::max(1, ri.height())));
        float x_cursor = x;
        float remain = fill;
        const float draw_l = std::min(lw, remain);
        m_engine->draw_ui_quad(li.handle(), x_cursor, y, draw_l, h, 1, 1, 1, 1);
        x_cursor += draw_l;
        remain -= draw_l;
        if (remain <= 0.0f) {
            return;
        }
        const float mid = std::max(0.0f, remain - rw);
        if (mid > 0.0f) {
            m_engine->draw_ui_quad(ci.handle(), x_cursor, y, mid, h, 1, 1, 1, 1);
            x_cursor += mid;
            remain -= mid;
        }
        if (remain > 0.0f) {
            m_engine->draw_ui_quad(ri.handle(), x_cursor, y, std::min(rw, remain), h, 1, 1, 1, 1);
        }
    }

    void WidgetTools::draw_bar(
        float x, float y, float full_w, float h, float ratio, float r, float g, float b) {
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio > 1) {
            ratio = 1;
        }
        m_engine->draw_ui_rect(x, y, full_w, h, 0.1f, 0.1f, 0.1f, 0.85f);
        m_engine->draw_ui_rect(x, y, full_w * ratio, h, r, g, b, 0.95f);
    }

    float WidgetTools::overlay_border_thickness(float scale) {
        return std::max(1.0f, 3.0f * std::max(0.25f, scale));
    }

    void
    WidgetTools::draw_overlay_panel(float x, float y, float w, float h, float scale, float alpha) {
        if (!m_engine || w <= 0.5f || h <= 0.5f) {
            return;
        }
        const float a = std::clamp(alpha, 0.0f, 1.0f);
        const float t = overlay_border_thickness(scale);
        m_engine->draw_ui_rect(x, y, w, h, 0.0f, 0.0f, 0.0f, 0.45f * a);
        draw_tex("chatting-box-top.bmp", x, y, w, t, a);
        draw_tex("chatting-box-bottom.bmp", x, y + h - t, w, t, a);
        draw_tex("chatting-box-left.bmp", x, y, t, h, a);
        draw_tex("chatting-box-right.bmp", x + w - t, y, t, h, a);
    }

    void WidgetTools::draw_cooldown_sweep(float x, float y, float size, float remaining) {
        if (!m_engine || remaining <= 0.05f || size <= 0.0f) {
            return;
        }
        // GetDelayAmount is elapsed/total (0 → 1); RunCoolTime uses that.
        const float f_rate = std::clamp(1.0f - remaining, 0.0f, 1.0f);
        const float half = size * 0.5f;
        const float cx = x + half;
        const float cy = y + half;
        struct Pt {
            float px;
            float py;
        };
        Pt buf[7] = {
            {cx, cy},
            {cx, cy},
            {cx, y},
            {cx, y},
            {cx, y},
            {cx, y},
            {cx, y},
        };
        const Pt corners[4] = {
            {x + size, y},
            {x + size, y + size},
            {x, y + size},
            {x, y},
        };

        constexpr float kOct = 0.125f;
        constexpr float kPi = 3.14159265358979323846f;
        const float fdx = std::cos(kPi / 2.0f - f_rate * kPi * 2.0f);
        const float fdy = std::sin(kPi / 2.0f - f_rate * kPi * 2.0f);
        const float feqa = (fdx == 0.0f) ? 1.0f : fdy / fdx;
        int n_vertices = 3;
        if (f_rate < kOct) {
            n_vertices = 7;
            buf[1].px += half / feqa;
            buf[1].py -= half;
        } else if (f_rate < kOct * 3.0f) {
            n_vertices = 6;
            buf[1].px += half;
            buf[1].py -= half * feqa;
        } else if (f_rate < kOct * 5.0f) {
            n_vertices = 5;
            buf[1].px += -half / feqa;
            buf[1].py -= -half;
        } else if (f_rate < kOct * 7.0f) {
            n_vertices = 4;
            buf[1].px += -half;
            buf[1].py -= -half * feqa;
        } else {
            n_vertices = 3;
            buf[1].px += half / feqa;
            buf[1].py -= half;
        }
        for (int i = 0; i < (n_vertices - 3); ++i) {
            buf[i + 2] = corners[7 - n_vertices + i];
        }

        constexpr float kA = 128.0f / 255.0f;
        for (int i = 0; i < n_vertices - 2; ++i) {
            m_engine->draw_ui_triangle(buf[0].px,
                                       buf[0].py,
                                       buf[i + 1].px,
                                       buf[i + 1].py,
                                       buf[i + 2].px,
                                       buf[i + 2].py,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       kA);
        }
    }

    void WidgetTools::draw_text_glyphs(
        float x, float y, float scale, const std::string& text, float r, float g, float b) {
        if (!m_engine || m_glyph_bits.empty() || scale < 0.25f || text.empty()) {
            return;
        }
        // Prefer atlas quads (1 draw/glyph). Fall back to per-pixel rects if upload
        // failed.
        if (m_font_tex && m_font_atlas_w > 0 && m_font_atlas_h > 0) {
            const float gw = static_cast<float>(m_font_gw) * scale;
            const float gh = static_cast<float>(m_font_gh) * scale;
            const float adv = static_cast<float>(m_font_gw + 1) * scale;
            const float inv_w = 1.0f / static_cast<float>(m_font_atlas_w);
            const float inv_h = 1.0f / static_cast<float>(m_font_atlas_h);
            float cx = x;
            LatinText::for_each_ascii(text, [&](char ascii) {
                unsigned char ch = static_cast<unsigned char>(ascii);
                if (ch < 32 || ch > 126) {
                    ch = '?';
                }
                const int gi = ch - 32;
                const int col = gi % m_font_atlas_cols;
                const int row = gi / m_font_atlas_cols;
                const float u0 = static_cast<float>(col * m_font_gw) * inv_w;
                const float v0 = static_cast<float>(row * m_font_gh) * inv_h;
                const float u1 = static_cast<float>(col * m_font_gw + m_font_gw) * inv_w;
                const float v1 = static_cast<float>(row * m_font_gh + m_font_gh) * inv_h;
                m_engine->draw_ui_quad(m_font_tex, cx, y, gw, gh, r, g, b, 1.0f, u0, v0, u1, v1);
                cx += adv;
            });
            return;
        }

        const float px = scale;
        float cx = x;
        LatinText::for_each_ascii(text, [&](char ascii) {
            unsigned char ch = static_cast<unsigned char>(ascii);
            if (ch < 32 || ch > 126) {
                ch = '?';
            }
            const int gi = ch - 32;
            for (int row = 0; row < m_font_gh; ++row) {
                const std::uint8_t bits =
                    m_glyph_bits[static_cast<std::size_t>(gi * m_font_gh + row)];
                for (int col = 0; col < m_font_gw; ++col) {
                    if (bits & (1u << (m_font_gw - 1 - col))) {
                        m_engine->draw_ui_rect(cx + static_cast<float>(col) * px,
                                               y + static_cast<float>(row) * px,
                                               px,
                                               px,
                                               r,
                                               g,
                                               b,
                                               1);
                    }
                }
            }
            cx += static_cast<float>(m_font_gw + 1) * px;
        });
    }

    float WidgetTools::text_width(float scale, const std::string& text) const {
        if (text.empty() || scale < 0.25f) {
            return 0.0f;
        }
        const float advance = static_cast<float>(m_font_gw + 1) * scale;
        std::size_t n = 0;
        LatinText::for_each_ascii(text, [&](char) { ++n; });
        if (n == 0) {
            return 0.0f;
        }
        return static_cast<float>(n) * advance - scale;
    }

    void WidgetTools::draw_text_centered(
        float cx, float y, float scale, const std::string& text, float r, float g, float b) {
        draw_text_glyphs(cx - text_width(scale, text) * 0.5f, y, scale, text, r, g, b);
    }

    void WidgetTools::build_font() {
        m_glyph_bits.assign(95 * m_font_gh, 0);
        auto set = [&](char ch, std::initializer_list<const char*> rows) {
            const int gi = static_cast<unsigned char>(ch) - 32;
            int r = 0;
            for (const char* row : rows) {
                if (r >= m_font_gh) {
                    break;
                }
                std::uint8_t bits = 0;
                for (int c = 0; c < m_font_gw && row[c]; ++c) {
                    if (row[c] != ' ' && row[c] != '.') {
                        bits |= static_cast<std::uint8_t>(1u << (m_font_gw - 1 - c));
                    }
                }
                m_glyph_bits[static_cast<std::size_t>(gi * m_font_gh + r)] = bits;
                ++r;
            }
        };
        const char* dig[10][7] = {
            {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "},
            {"  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"},
            {" ### ", "#   #", "    #", "  ## ", " #   ", "#    ", "#####"},
            {" ### ", "#   #", "    #", " ### ", "    #", "#   #", " ### "},
            {"#   #", "#   #", "#   #", "#####", "    #", "    #", "    #"},
            {"#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "},
            {" ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### "},
            {"#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "},
            {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "},
            {" ### ", "#   #", "#   #", " ####", "    #", "    #", " ### "},
        };
        for (int d = 0; d < 10; ++d) {
            set(static_cast<char>('0' + d),
                {dig[d][0], dig[d][1], dig[d][2], dig[d][3], dig[d][4], dig[d][5], dig[d][6]});
        }
        set('A', {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"});
        set('B', {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "});
        set('C', {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "});
        set('D', {"#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "});
        set('E', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"});
        set('F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "});
        set('G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "});
        set('H', {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"});
        set('I', {" ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "});
        set('J', {"  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  "});
        set('K', {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"});
        set('L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"});
        set('M', {"#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"});
        set('N', {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"});
        set('O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "});
        set('P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "});
        set('Q', {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"});
        set('R', {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"});
        set('S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "});
        set('T', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "});
        set('U', {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "});
        set('V', {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "});
        set('W', {"#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #"});
        set('X', {"#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"});
        set('Y', {"#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "});
        set('Z', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"});
        set('a', {"     ", " ### ", "    #", " ####", "#   #", "#   #", " ####"});
        set('b', {"#    ", "#    ", "#### ", "#   #", "#   #", "#   #", "#### "});
        set('c', {"     ", " ### ", "#   #", "#    ", "#    ", "#   #", " ### "});
        set('d', {"    #", "    #", " ####", "#   #", "#   #", "#   #", " ####"});
        set('e', {"     ", " ### ", "#   #", "#####", "#    ", "#   #", " ### "});
        set('f', {"  ## ", " #  #", " #   ", "###  ", " #   ", " #   ", " #   "});
        set('g', {"     ", " ####", "#   #", "#   #", " ####", "    #", " ### "});
        set('h', {"#    ", "#    ", "#### ", "#   #", "#   #", "#   #", "#   #"});
        set('i', {"  #  ", "     ", " ##  ", "  #  ", "  #  ", "  #  ", " ### "});
        set('j', {"   # ", "     ", "  ## ", "   # ", "   # ", "#  # ", " ##  "});
        set('k', {"#    ", "#    ", "#  # ", "# #  ", "##   ", "# #  ", "#  # "});
        set('l', {" ##  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "});
        set('m', {"     ", "## # ", "# # #", "# # #", "# # #", "# # #", "# # #"});
        set('n', {"     ", "#### ", "#   #", "#   #", "#   #", "#   #", "#   #"});
        set('o', {"     ", " ### ", "#   #", "#   #", "#   #", "#   #", " ### "});
        set('p', {"     ", "#### ", "#   #", "#   #", "#### ", "#    ", "#    "});
        set('r', {"     ", "# ## ", "##  #", "#    ", "#    ", "#    ", "#    "});
        set('s', {"     ", " ####", "#    ", " ### ", "    #", "    #", "#### "});
        set('t', {" #   ", " #   ", "#### ", " #   ", " #   ", " #  #", "  ## "});
        set('u', {"     ", "#   #", "#   #", "#   #", "#   #", "#  ##", " ## #"});
        set('v', {"     ", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "});
        set('w', {"     ", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #"});
        set('x', {"     ", "#   #", " # # ", "  #  ", " # # ", "#   #", "     "});
        set('y', {"     ", "#   #", "#   #", "#   #", " ####", "    #", " ### "});
        set('z', {"     ", "#####", "   # ", "  #  ", " #   ", "#####", "     "});
        set(' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "});
        set('-', {"     ", "     ", "     ", " ### ", "     ", "     ", "     "});
        set('/', {"    #", "   # ", "  #  ", " #   ", "#    ", "     ", "     "});
        set(':', {"     ", "  #  ", "     ", "     ", "  #  ", "     ", "     "});
        set('(', {"  #  ", " #   ", "#    ", "#    ", "#    ", " #   ", "  #  "});
        set(')', {"  #  ", "   # ", "    #", "    #", "    #", "   # ", "  #  "});
        set('%', {"##  #", "## # ", "  #  ", " #   ", "# ## ", "#  ##", "     "});
        set('.', {"     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "});
        for (char c = 'A'; c <= 'Z'; ++c) {
            const int gi = c - 32;
            const int lo = (c - 'A' + 'a') - 32;
            bool empty = true;
            for (int r = 0; r < m_font_gh; ++r) {
                if (m_glyph_bits[static_cast<std::size_t>(lo * m_font_gh + r)]) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                for (int r = 0; r < m_font_gh; ++r) {
                    m_glyph_bits[static_cast<std::size_t>(lo * m_font_gh + r)] =
                        m_glyph_bits[static_cast<std::size_t>(gi * m_font_gh + r)];
                }
            }
        }

        // Bake glyphs into a nearest-filtered atlas (1 textured quad per character at
        // draw time).
        if (!m_engine) {
            return;
        }
        if (m_font_tex) {
            m_engine->destroy_texture(m_font_tex);
            m_font_tex = {};
        }
        constexpr int kGlyphCount = 95;  // ASCII 32..126
        m_font_atlas_cols = 16;
        const int rows = (kGlyphCount + m_font_atlas_cols - 1) / m_font_atlas_cols;
        m_font_atlas_w = m_font_atlas_cols * m_font_gw;
        m_font_atlas_h = rows * m_font_gh;
        std::vector<std::uint8_t> rgba(
            static_cast<std::size_t>(m_font_atlas_w * m_font_atlas_h * 4), 0);
        for (int gi = 0; gi < kGlyphCount; ++gi) {
            const int cell_x = (gi % m_font_atlas_cols) * m_font_gw;
            const int cell_y = (gi / m_font_atlas_cols) * m_font_gh;
            for (int row = 0; row < m_font_gh; ++row) {
                const std::uint8_t bits =
                    m_glyph_bits[static_cast<std::size_t>(gi * m_font_gh + row)];
                for (int col = 0; col < m_font_gw; ++col) {
                    if ((bits & (1u << (m_font_gw - 1 - col))) == 0) {
                        continue;
                    }
                    const int px = cell_x + col;
                    const int py = cell_y + row;
                    const std::size_t o = static_cast<std::size_t>((py * m_font_atlas_w + px) * 4);
                    rgba[o + 0] = 255;
                    rgba[o + 1] = 255;
                    rgba[o + 2] = 255;
                    rgba[o + 3] = 255;
                }
            }
        }
        m_font_tex = m_engine->create_texture_rgba8(
            rgba.data(), m_font_atlas_w, m_font_atlas_h, false, true);
    }

}  // namespace openkal::client
