/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiView.h"

#include "openkal/client/TextWrap.h"
#include "openkal/client/UiLayout.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

namespace openkal::client {
    namespace {

        void draw_ttf(engine::IEngine& engine,
                      const FreetypeFont& font,
                      float x,
                      float y,
                      const std::string& text,
                      float r,
                      float g,
                      float b,
                      float scale,
                      const std::vector<UiTextColorRun>& colors = {},
                      std::size_t text_offset = 0) {
            if (!font.ready() || text.empty()) {
                return;
            }
            const auto draw =
                [&](std::size_t start, std::size_t end, float cr, float cg, float cb) {
                    if (start < end) {
                        font.draw(
                            engine,
                            x + font.text_width(std::string_view(text).substr(0, start), scale),
                            y,
                            text.substr(start, end - start),
                            cr,
                            cg,
                            cb,
                            scale);
                    }
                };
            std::size_t start = 0;
            for (const auto& color : colors) {
                if (color.start + color.length <= text_offset ||
                    color.start >= text_offset + text.size()) {
                    continue;
                }
                const auto begin = std::max(color.start, text_offset) - text_offset;
                const auto end =
                    std::min(color.start + color.length, text_offset + text.size()) - text_offset;
                draw(start, begin, r, g, b);
                float cr = r, cg = g, cb = b;
                (void) ui_parse_hex_color(color.color, cr, cg, cb);
                draw(begin, end, cr, cg, cb);
                start = end;
            }
            draw(start, text.size(), r, g, b);
        }

        void resolve_color(const std::string& raw,
                           const UiVarMap& vars,
                           float fallback_r,
                           float fallback_g,
                           float fallback_b,
                           float& r,
                           float& g,
                           float& b) {
            r = fallback_r;
            g = fallback_g;
            b = fallback_b;
            const std::string interpolated = ui_interpolate(raw, vars);
            (void) ui_parse_hex_color(interpolated, r, g, b);
        }

        [[nodiscard]] bool widget_visible(const UiWidget& w, const UiVarMap& vars) {
            return w.visible() && ui_eval_condition(w.visible_when(), vars);
        }

        [[nodiscard]] std::string lookup_var(const UiVarMap& vars, const std::string& key) {
            const auto it = vars.find(key);
            return it == vars.end() ? std::string{} : it->second;
        }

        [[nodiscard]] int grid_capacity(const UiWidget& w) {
            return std::max(1, w.cols()) * std::max(1, w.rows());
        }

        void slot_cell_rect(const UiWidget& w,
                            int index,
                            float gx,
                            float gy,
                            float scale,
                            float cell_w,
                            float cell_h,
                            float& sx,
                            float& sy,
                            float& sw,
                            float& sh) {
            const int cols = std::max(1, w.cols());
            const int col = index % cols;
            const int row = index / cols;
            sx = gx + static_cast<float>(col) * w.pitch_x() * scale;
            sy = gy + static_cast<float>(row) * w.pitch_y() * scale;
            sw = cell_w * scale;
            sh = cell_h * scale;
        }

    }  // namespace

    void UiView::draw_item_binding(WidgetTools& ui,
                                   float x,
                                   float y,
                                   float w,
                                   float h,
                                   bool account_bound,
                                   bool character_bound) {
        if ((!account_bound && !character_bound) || !ui.tex("accBound.bmp")) {
            return;
        }
        if (character_bound) {
            // Mirror only the right half, preserving the frame's opacity.
            ui.draw_tex_uv("accBound.bmp", x, y, w * 0.5f, h, 1, 0, 0.5f, 1);
            ui.draw_tex_uv("accBound.bmp", x + w * 0.5f, y, w * 0.5f, h, 0.5f, 0, 1, 1);
        } else {
            ui.draw_tex("accBound.bmp", x, y, w, h);
        }
    }

    void UiView::draw_enchant_protection(
        engine::IEngine& engine, float x, float y, float w, float h, double seconds) {
        if (w <= 0 || h <= 0) {
            return;
        }
        const double cycle = seconds / 2.0;
        const float phase = static_cast<float>(cycle - std::floor(cycle));
        constexpr float halo_width = 0.16f;
        constexpr float trail_length = 0.55f;
        const float position = phase * (2.0f + 2.0f * trail_length) - trail_length;
        const int rows = std::clamp(static_cast<int>(std::ceil(h)), 1, 128);
        const float row_height = h / static_cast<float>(rows);
        for (int row = 0; row < rows; ++row) {
            // Constant x/w + y/h forms a full diagonal band across the icon.
            const float center = position - (static_cast<float>(row) + 0.5f) / rows;
            const float py = y + row * row_height;
            const auto span = [&](float begin, float end, float r, float g, float b, float alpha) {
                const float left = std::clamp(center + begin, 0.0f, 1.0f);
                const float right = std::clamp(center + end, 0.0f, 1.0f);
                if (right > left) {
                    engine.draw_ui_rect(
                        x + left * w, py, (right - left) * w, row_height, r, g, b, alpha);
                }
            };
            constexpr int trail_steps = 16;
            for (int step = 0; step < trail_steps; ++step) {
                const float begin = static_cast<float>(step) / trail_steps;
                const float end = static_cast<float>(step + 1) / trail_steps;
                const float strength = (begin + end) * 0.5f;
                span(-trail_length + begin * trail_length,
                     -trail_length + end * trail_length,
                     0.45f,
                     0.75f,
                     1.0f,
                     0.34f * strength * strength);
            }
            span(-halo_width, halo_width, 0.35f, 0.65f, 1.0f, 0.12f);
            span(-0.08f, 0.08f, 0.65f, 0.85f, 1.0f, 0.22f);
            span(-0.025f, 0.025f, 0.9f, 0.97f, 1.0f, 0.65f);
        }
    }

    WindowFrameStyle UiView::frame_style(const UiDocument& doc) {
        WindowFrameStyle style = doc.chrome_preset() == "info_menu"
                                     ? WindowFrameStyle::info_menu(doc.chrome_close_button())
                                     : WindowFrameStyle{};
        style.set_padding(doc.pad_left(), doc.pad_top(), doc.pad_right(), doc.pad_bottom());
        style.set_close_enabled(doc.chrome_close_button());
        if (doc.close_override() && doc.chrome_preset() != "info_menu") {
            style.set_close_box(doc.close_ox(), doc.close_oy(), doc.close_w(), doc.close_h());
        }
        const auto& f = doc.chrome_files();
        style.set_slice_files(f.upleft(),
                              f.upcenter(),
                              doc.chrome_preset() == "info_menu" && !doc.chrome_close_button()
                                  ? "info-menu-top right - not close.bmp"
                                  : f.upright(),
                              f.left(),
                              f.right(),
                              f.downleft(),
                              f.downcenter(),
                              f.downright());
        style.set_exit(f.exit_file(), doc.close_ox(), doc.close_oy());
        style.set_draw_exit(doc.chrome_preset() != "info_menu" && doc.chrome_close_button() &&
                            doc.kind() == UiKind::Window &&
                            doc.chrome_kind() == UiChromeKind::NineSlice);
        style.set_title_drag_h(doc.title_drag_h());
        style.set_tile_edges(doc.chrome_edges() == UiChromeEdges::Tile);
        style.set_repeat_edges(doc.chrome_edges() == UiChromeEdges::Repeat);
        return style;
    }

    const UiDocument& UiView::document(const UiLayout* layout) {
        if (!layout) {
            static const UiDocument empty;
            return empty;
        }
        if (!layout->m_document) {
            auto doc = std::make_shared<UiDocument>();
            (void) doc->load(layout->root());
            layout->m_document = std::move(doc);
        }
        return *layout->m_document;
    }

    void UiView::reload(UiDocument& doc, const UiLayout* layout) {
        doc = document(layout);
    }

    void UiView::widget_rect(const UiWidget& w,
                             WidgetTools* ui,
                             float client_x,
                             float client_y,
                             float client_w,
                             float client_h,
                             float scale,
                             float& sx,
                             float& sy,
                             float& sw,
                             float& sh) {
        const float s = std::max(0.25f, scale);
        sx = w.x_from_right() >= 0 ? client_x + client_w - w.x_from_right() * s
                                   : client_x + w.x() * s;
        if (w.y_from_bottom() >= 0.0f) {
            sy = client_y + client_h - w.y_from_bottom() * s;
        } else {
            sy = client_y + w.y() * s;
        }
        float lw = w.w();
        float lh = w.h();
        if ((lw <= 0.0f || lh <= 0.0f) && ui) {
            std::string file = w.file_default();
            if (file.empty()) {
                file = w.file();
            }
            if (file.empty()) {
                file = w.file_off();
            }
            if (!file.empty()) {
                const auto info = ui->tex_info(file);
                if (lw <= 0.0f && info.width() > 0) {
                    lw = static_cast<float>(info.width());
                }
                if (lh <= 0.0f && info.height() > 0) {
                    lh = static_cast<float>(info.height());
                }
            }
        }
        if (lw <= 0.0f) {
            lw = 16.0f;
        }
        if (lh <= 0.0f) {
            lh = 16.0f;
        }
        sw = lw * s;
        sh = lh * s;
    }

    void UiView::draw_chrome(WidgetTools& ui,
                             const UiDocument& doc,
                             float client_x,
                             float client_y,
                             float client_w,
                             float client_h,
                             float scale,
                             bool fill_client) {
        const float s = std::max(0.25f, scale);
        if (fill_client && ui.renderer() && doc.kind() == UiKind::Window) {
            ui.renderer()->draw_ui_rect(
                client_x, client_y, client_w, client_h, 0.08f, 0.07f, 0.06f, 1.0f);
            if (doc.chrome_preset() == "dragon_dialog") {
                ui.draw_tex(
                    "dragon_dialog/panel_background.bmp", client_x, client_y, client_w, client_h);
            }
        }
        if (doc.chrome_kind() == UiChromeKind::Bitmap && !doc.chrome_file().empty()) {
            const auto info = ui.tex_info(doc.chrome_file());
            float bw = client_w;
            float bh = client_h;
            if (info.width() > 0 && info.height() > 0) {
                bw = static_cast<float>(info.width()) * s;
                bh = static_cast<float>(info.height()) * s;
            } else {
                if (doc.fallback_w() > 0.0f) {
                    bw = doc.fallback_w() * s;
                }
                if (doc.fallback_h() > 0.0f) {
                    bh = doc.fallback_h() * s;
                }
            }
            ui.draw_tex(doc.chrome_file(), client_x, client_y, bw, bh);
            return;
        }
        if (doc.kind() != UiKind::Window || doc.chrome_kind() != UiChromeKind::NineSlice) {
            return;
        }
        const WindowFrameStyle style = frame_style(doc);
        ui.draw_window_frame(client_x, client_y, client_w, client_h, s, style);
        ui.draw_window_close(client_x, client_y, client_w, client_h, s, style);
    }

    void UiView::draw_widgets(engine::IEngine& engine,
                              WidgetTools& ui,
                              const UiDocument& doc,
                              float client_x,
                              float client_y,
                              float client_w,
                              float client_h,
                              float scale,
                              const UiVarMap& vars,
                              const FreetypeFont& body,
                              const FreetypeFont& title,
                              const UiViewHost* host,
                              WidgetDrawPass pass) {
        const float s = std::max(0.25f, scale);
        float label_r = 0.91f, label_g = 0.86f, label_b = 0.75f;
        float value_r = 1.0f, value_g = 0.98f, value_b = 0.92f;
        float pending_r = 0.78f, pending_g = 0.88f, pending_b = 0.56f;
        (void) ui_parse_hex_color(doc.color_label(), label_r, label_g, label_b);
        (void) ui_parse_hex_color(doc.color_value(), value_r, value_g, value_b);
        (void) ui_parse_hex_color(doc.color_pending(), pending_r, pending_g, pending_b);
        (void) label_r;
        (void) label_g;
        (void) label_b;

        for (const auto& source : doc.widgets()) {
            UiWidget scrolled;
            if (!source.viewport().empty()) {
                scrolled = ui_scrolled_widget(source, doc, vars);
            }
            const auto& w = source.viewport().empty() ? source : scrolled;
            const bool chrome_title =
                w.id() == "title" || w.id() == "dialog_icon" || w.id() == "frame_handle" ||
                (doc.chrome_preset() == "dragon_dialog" && w.action() == "window.close");
            if ((pass == WidgetDrawPass::Content && chrome_title) ||
                (pass == WidgetDrawPass::ChromeTitle && !chrome_title)) {
                continue;
            }
            if (!widget_visible(w, vars)) {
                continue;
            }
            if (w.type() == UiWidgetType::Image) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (!w.file().empty()) {
                    const auto info = ui.tex_info(w.file());
                    const float iw = static_cast<float>(info.width());
                    const float ih = static_cast<float>(info.height());
                    if (iw <= 0 || ih <= 0) {
                        continue;
                    }
                    const bool cropped = w.source().w > 0 && w.source().h > 0;
                    const float ix = cropped ? std::clamp(w.source().x, 0.0f, iw) : 0;
                    const float iy = cropped ? std::clamp(w.source().y, 0.0f, ih) : 0;
                    const float tw = cropped ? std::min(w.source().x + w.source().w, iw) - ix : iw;
                    const float th = cropped ? std::min(w.source().y + w.source().h, ih) - iy : ih;
                    if (tw <= 0 || th <= 0) {
                        continue;
                    }
                    if (!w.repeat()) {
                        ui.draw_tex_uv(w.file(),
                                       sx,
                                       sy,
                                       sw,
                                       sh,
                                       ix / iw,
                                       iy / ih,
                                       (ix + tw) / iw,
                                       (iy + th) / ih,
                                       w.alpha());
                    } else {
                        for (float y = 0; y < sh; y += th * s) {
                            const float h = std::min(th * s, sh - y);
                            for (float x = 0; x < sw; x += tw * s) {
                                const float width = std::min(tw * s, sw - x);
                                ui.draw_tex_uv(w.file(),
                                               sx + x,
                                               sy + y,
                                               width,
                                               h,
                                               ix / iw,
                                               iy / ih,
                                               (ix + width / s) / iw,
                                               (iy + h / s) / ih,
                                               w.alpha());
                            }
                        }
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::Fill) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                float r = 0, g = 0, b = 0;
                resolve_color(w.color().empty() ? "#000000" : w.color(), vars, 0, 0, 0, r, g, b);
                if (ui.renderer()) {
                    ui.renderer()->draw_ui_rect(sx, sy, sw, sh, r, g, b, w.alpha());
                }
                continue;
            }
            if (w.type() == UiWidgetType::Border) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                const float t = (w.thickness() > 0.0f ? w.thickness() : 3.0f) * s;
                if (!w.border_top().empty()) {
                    ui.draw_tex(w.border_top(), sx, sy, sw, t);
                }
                if (!w.border_bottom().empty()) {
                    ui.draw_tex(w.border_bottom(), sx, sy + sh - t, sw, t);
                }
                if (!w.border_left().empty()) {
                    ui.draw_tex(w.border_left(), sx, sy, t, sh);
                }
                if (!w.border_right().empty()) {
                    ui.draw_tex(w.border_right(), sx + sw - t, sy, t, sh);
                }
                continue;
            }
            if (w.type() == UiWidgetType::Text || w.type() == UiWidgetType::Input) {
                std::vector<UiTextColorRun> colors;
                const std::string shown =
                    ui_interpolate(w.type() == UiWidgetType::Input && w.text().empty()
                                       ? std::string("${") + w.bind() + "}"
                                       : w.text(),
                                   vars,
                                   &colors);
                float r = value_r, g = value_g, b = value_b;
                resolve_color(w.color(), vars, value_r, value_g, value_b, r, g, b);
                if (!w.pending_when().empty() && ui_eval_condition(w.pending_when(), vars)) {
                    r = pending_r;
                    g = pending_g;
                    b = pending_b;
                    colors.clear();
                }
                const FreetypeFont& face =
                    ((w.id() == "title" || w.bold()) && title.ready()) ? title : body;
                const float text_scale = w.pixel_size() > 0 && face.pixel_size() > 0
                                             ? s * static_cast<float>(w.pixel_size()) /
                                                   static_cast<float>(face.pixel_size())
                                             : s;
                float x = w.x_from_right() >= 0 ? client_x + client_w - w.x_from_right() * s
                                                : client_x + w.x() * s;
                float y = client_y + w.y() * s;
                if (w.y_from_bottom() >= 0.0f) {
                    y = client_y + client_h - w.y_from_bottom() * s;
                }
                if (w.type() == UiWidgetType::Input) {
                    float sx = 0, sy = 0, sw = 0, sh = 0;
                    widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                    if (w.multiline()) {
                        const auto lines = layout_text_lines(face, shown, sw, text_scale);
                        const float step =
                            std::max(w.line_height() * s, face.line_height() * text_scale);
                        const auto offset = static_cast<std::size_t>(
                            std::max(0, std::atoi(lookup_var(vars, w.scroll_bind()).c_str())));
                        for (std::size_t i = offset;
                             i < lines.size() && (i - offset + 1) * step <= sh;
                             ++i) {
                            draw_ttf(engine,
                                     face,
                                     sx,
                                     sy + (i - offset) * step,
                                     shown.substr(lines[i].start, lines[i].length),
                                     r,
                                     g,
                                     b,
                                     text_scale,
                                     colors,
                                     lines[i].start);
                        }
                        continue;
                    }
                    x = sx + (w.align() == "center" ? sw * 0.5f : w.align() == "right" ? sw : 0);
                    y = sy + std::max(0.0f, (sh - face.line_height() * text_scale) * 0.5f);
                }
                if (w.align() == "right" && face.ready()) {
                    x -= face.text_width(shown, text_scale);
                } else if (w.align() == "center" && face.ready()) {
                    x -= face.text_width(shown, text_scale) * 0.5f;
                }
                draw_ttf(engine, face, x, y, shown, r, g, b, text_scale, colors);
                continue;
            }
            if (w.type() == UiWidgetType::Button) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                const bool on = !host || host->widget_enabled(w);
                std::string file = w.file_default();
                if (!on && !w.file_off().empty()) {
                    file = w.file_off();
                } else if (on && host && host->button_pressed(w.id()) && !w.file_on().empty()) {
                    file = w.file_on();
                } else if (on && host && host->button_hovered(w.id()) && !w.file_over().empty()) {
                    file = w.file_over();
                }
                if (!file.empty()) {
                    ui.draw_tex(file, sx, sy, sw, sh, on || !w.file_off().empty() ? 1.0f : 0.55f);
                }
                std::vector<UiTextColorRun> colors;
                const std::string lab = ui_interpolate(w.label(), vars, &colors);
                if (!lab.empty()) {
                    const FreetypeFont& face = w.bold() && title.ready() ? title : body;
                    const float text_scale = w.pixel_size() > 0 && face.pixel_size() > 0
                                                 ? s * static_cast<float>(w.pixel_size()) /
                                                       static_cast<float>(face.pixel_size())
                                                 : s;
                    float r = value_r, g = value_g, b = value_b;
                    if (!on) {
                        r = g = b = 0.55f;
                        colors.clear();
                    } else {
                        resolve_color(w.color(), vars, value_r, value_g, value_b, r, g, b);
                    }
                    float x = sx + w.label_ox() * s;
                    float y = sy + w.label_oy() * s;
                    if (w.align() == "center" || w.align() == "right") {
                        const float remaining = sw - face.text_width(lab, text_scale);
                        x += w.align() == "center" ? remaining * 0.5f : remaining;
                        y += (sh - face.line_height() * text_scale) * 0.5f;
                    }
                    draw_ttf(engine, face, x, y, lab, r, g, b, text_scale, colors);
                }
                continue;
            }
            if (w.type() == UiWidgetType::Tabs) {
                const std::string selected = lookup_var(vars, w.bind());
                for (const auto& tab : w.tabs()) {
                    float tw = tab.w();
                    float th = w.h() > 0.0f ? w.h() : 18.0f;
                    if (tw <= 0.0f || th <= 0.0f) {
                        const std::string sample =
                            tab.file_off().empty() ? tab.file_on() : tab.file_off();
                        const auto info = ui.tex_info(sample);
                        if (tw <= 0.0f && info.width() > 0) {
                            tw = static_cast<float>(info.width());
                        }
                        if (th <= 0.0f && info.height() > 0) {
                            th = static_cast<float>(info.height());
                        }
                    }
                    const float tx = client_x + tab.x() * s;
                    const float ty = client_y + w.y() * s;
                    const bool on = !selected.empty() && tab.id() == selected;
                    const std::string file = on ? tab.file_on() : tab.file_off();
                    if (!file.empty()) {
                        ui.draw_tex(file, tx, ty, tw * s, th * s);
                    }
                    const std::string lab = ui_interpolate(tab.label(), vars);
                    if (!lab.empty() && body.ready()) {
                        float r = value_r, g = value_g, b = value_b;
                        const std::string col = on ? w.color_on() : w.color_off();
                        resolve_color(col, vars, value_r, value_g, value_b, r, g, b);
                        float lx = tx + 4.0f * s;
                        float ly = ty + 4.0f * s;
                        if (w.align() == "center") {
                            lx = tx + (tw * s - body.text_width(lab, s)) * 0.5f;
                            ly = ty + (th - body.line_height()) * s * 0.5f;
                        }
                        draw_ttf(engine, body, lx, ly, lab, r, g, b, s);
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::Progress) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                float ratio = 0.0f;
                if (host) {
                    ratio = host->progress_ratio(w.bind());
                } else {
                    const std::string raw = lookup_var(vars, w.bind());
                    if (!raw.empty()) {
                        ratio = std::strtof(raw.c_str(), nullptr);
                    }
                }
                ratio = std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0.0f;
                if (!w.file().empty()) {
                    if (w.fill_mode() == "crop") {
                        if (ratio > 0.0f) {
                            const bool vertical = w.direction() == "bottom_to_top";
                            const float amount = std::round((vertical ? sh : sw) * ratio);
                            const float fraction = amount / std::max(1.0f, vertical ? sh : sw);
                            if (vertical) {
                                ui.draw_tex_uv(w.file(),
                                               sx,
                                               sy + sh - amount,
                                               sw,
                                               amount,
                                               0,
                                               1 - fraction,
                                               1,
                                               1,
                                               w.alpha());
                            } else {
                                ui.draw_tex_uv(
                                    w.file(), sx, sy, amount, sh, 0, 0, fraction, 1, w.alpha());
                            }
                        }
                    } else {
                        ui.draw_progress_lcr(w.file().c_str(), sx, sy, sw, sh, ratio);
                    }
                } else if (ui.renderer()) {
                    float r = 0.8f, g = 0.2f, b = 0.2f;
                    resolve_color(w.color(), vars, r, g, b, r, g, b);
                    ui.renderer()->draw_ui_rect(sx, sy, sw * ratio, sh, r, g, b, w.alpha());
                }
                continue;
            }
            if (w.type() == UiWidgetType::IconSlot) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                UiSlotIcon slot;
                const bool ok = host && host->resolve_slot(w.bind(), 0, slot);
                if (!w.frame_file().empty()) {
                    const float fw = (w.frame_w() > 0.0f ? w.frame_w() : w.w()) * s;
                    const float fh = (w.frame_h() > 0.0f ? w.frame_h() : w.h()) * s;
                    const float fx = sx + (sw - fw) * 0.5f;
                    const float fy = sy + (sh - fh) * 0.5f;
                    ui.draw_tex(w.frame_file(), fx, fy, fw, fh);
                }
                if ((!ok || slot.empty()) && !w.empty_file().empty() && ui.tex(w.empty_file())) {
                    ui.draw_tex(w.empty_file(), sx, sy, sw, sh);
                }
                if (ok && !slot.empty() && !slot.file().empty()) {
                    if (ui.item_tex(slot.file())) {
                        ui.draw_item_tex(slot.file(), sx, sy, sw, sh, 1.0f, slot.grayscale());
                    } else {
                        ui.draw_tex(slot.file(), sx, sy, sw, sh);
                    }
                    if (slot.enchant_protected()) {
                        const double seconds =
                            std::chrono::duration<double>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
                        draw_enchant_protection(engine, sx, sy, sw, sh, seconds);
                    }
                    draw_item_binding(
                        ui, sx, sy, sw, sh, slot.account_bound(), slot.character_bound());
                    if (slot.count() > 1) {
                        ui.draw_stack_count(&body,
                                            sx,
                                            sy,
                                            sw,
                                            sh,
                                            s,
                                            slot.count(),
                                            slot.high_contrast_count(),
                                            slot.high_contrast_count() ? 0.575f : 1.0f);
                    }
                    if (slot.level() > 0 && body.ready()) {
                        draw_ttf(engine,
                                 body,
                                 sx,
                                 sy + sh,
                                 std::to_string(slot.level()),
                                 value_r,
                                 value_g,
                                 value_b,
                                 s);
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::IconGrid) {
                float gx = 0, gy = 0, gw = 0, gh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, gx, gy, gw, gh);
                (void) gw;
                (void) gh;
                float cell_w = w.cell_w();
                float cell_h = w.cell_h();
                if (cell_w <= 0.0f) {
                    cell_w = 32.0f;
                }
                if (cell_h <= 0.0f) {
                    cell_h = 32.0f;
                }
                const int capacity = grid_capacity(w);
                float sel_r = 0.94f, sel_g = 0.82f, sel_b = 0.13f;
                if (!w.select_color().empty()) {
                    (void) ui_parse_hex_color(
                        ui_interpolate(w.select_color(), vars), sel_r, sel_g, sel_b);
                }
                for (int i = 0; i < capacity; ++i) {
                    float sx = 0, sy = 0, sw = 0, sh = 0;
                    slot_cell_rect(w, i, gx, gy, s, cell_w, cell_h, sx, sy, sw, sh);
                    if (!w.frame_file().empty()) {
                        const float fw = (w.frame_w() > 0 ? w.frame_w() : cell_w) * s;
                        const float fh = (w.frame_h() > 0 ? w.frame_h() : cell_h) * s;
                        ui.draw_tex(
                            w.frame_file(), sx + (sw - fw) * 0.5f, sy + (sh - fh) * 0.5f, fw, fh);
                    }
                    if (!w.empty_file().empty()) {
                        ui.draw_tex(w.empty_file(), sx, sy, sw, sh);
                    }
                    UiSlotIcon slot;
                    if (!host || !host->resolve_slot(w.bind(), i + w.first_slot(), slot) ||
                        slot.empty()) {
                        continue;
                    }
                    if (slot.selected() && ui.renderer()) {
                        ui.renderer()->draw_ui_rect(
                            sx - 1, sy - 1, sw + 2, sh + 2, sel_r, sel_g, sel_b, 0.85f);
                    }
                    if (!slot.file().empty()) {
                        if (ui.item_tex(slot.file())) {
                            ui.draw_item_tex(slot.file(), sx, sy, sw, sh, 1.0f, slot.grayscale());
                        } else {
                            ui.draw_tex(slot.file(), sx, sy, sw, sh);
                        }
                    }
                    if (slot.enchant_protected()) {
                        const double seconds =
                            std::chrono::duration<double>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
                        draw_enchant_protection(engine, sx, sy, sw, sh, seconds);
                    }
                    draw_item_binding(
                        ui, sx, sy, sw, sh, slot.account_bound(), slot.character_bound());
                    if (slot.equipped()) {
                        engine.draw_ui_rect(sx,
                                            sy,
                                            sw,
                                            sh,
                                            244.0f / 255.0f,
                                            212.0f / 255.0f,
                                            130.0f / 255.0f,
                                            0.5f);
                    }
                    if (slot.count() > 1) {
                        ui.draw_stack_count(&body,
                                            sx,
                                            sy,
                                            sw,
                                            sh,
                                            s,
                                            slot.count(),
                                            slot.high_contrast_count(),
                                            slot.high_contrast_count() ? 0.575f : 1.0f);
                    }
                    if (slot.level() > 0 && body.ready()) {
                        draw_ttf(engine,
                                 body,
                                 sx,
                                 sy + sh,
                                 std::to_string(slot.level()),
                                 value_r,
                                 value_g,
                                 value_b,
                                 s);
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::List) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                const float lh = (w.line_height() > 0.0f ? w.line_height() : 14.0f) * s;
                std::vector<UiListRow> rows;
                if (host) {
                    rows = host->list_rows(w.bind(), w, vars);
                }
                if (rows.empty()) {
                    const std::string shown = ui_interpolate(w.text(), vars);
                    if (!shown.empty()) {
                        UiListRow row;
                        row.set_text(shown);
                        rows.push_back(std::move(row));
                    }
                }
                float r = value_r, g = value_g, b = value_b;
                resolve_color(w.color(), vars, value_r, value_g, value_b, r, g, b);
                float y = sy;
                for (const auto& row : rows) {
                    if (y + lh > sy + sh) {
                        break;
                    }
                    draw_ttf(engine, body, sx, y, row.text(), r, g, b, s);
                    y += lh;
                }
                continue;
            }
            if (w.type() == UiWidgetType::Canvas && host) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, &ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (w.canvas_kind() == "level_orb") {
                    const float cx = client_x + (w.cx() != 0.0f ? w.cx() : w.x()) * s;
                    const float cy = client_y + (w.cy() != 0.0f ? w.cy() : w.y()) * s;
                    sx = cx;
                    sy = cy;
                    sw = w.inner() * s;
                    sh = w.inner() * s;
                }
                host->draw_canvas(engine, ui, w, sx, sy, sw, sh, s, vars, body);
            }
        }
    }

    void UiView::draw(engine::IEngine& engine,
                      WidgetTools& ui,
                      const UiDocument& doc,
                      float client_x,
                      float client_y,
                      float client_w,
                      float client_h,
                      float scale,
                      const UiVarMap& vars,
                      const FreetypeFont& body,
                      const FreetypeFont& title,
                      const UiViewHost* host,
                      bool fill_client) {
        const bool frame_foreground =
            doc.kind() == UiKind::Window && doc.chrome_kind() == UiChromeKind::NineSlice;
        if (!frame_foreground) {
            draw_chrome(ui, doc, client_x, client_y, client_w, client_h, scale, fill_client);
            draw_widgets(engine,
                         ui,
                         doc,
                         client_x,
                         client_y,
                         client_w,
                         client_h,
                         scale,
                         vars,
                         body,
                         title,
                         host);
            return;
        }
        if (fill_client && ui.renderer() && doc.kind() == UiKind::Window) {
            ui.renderer()->draw_ui_rect(
                client_x, client_y, client_w, client_h, 0.08f, 0.07f, 0.06f, 1.0f);
            if (doc.chrome_preset() == "dragon_dialog") {
                ui.draw_tex(
                    "dragon_dialog/panel_background.bmp", client_x, client_y, client_w, client_h);
            }
        }
        draw_widgets(engine,
                     ui,
                     doc,
                     client_x,
                     client_y,
                     client_w,
                     client_h,
                     scale,
                     vars,
                     body,
                     title,
                     host,
                     WidgetDrawPass::Content);
        draw_chrome(ui, doc, client_x, client_y, client_w, client_h, scale, false);
        draw_widgets(engine,
                     ui,
                     doc,
                     client_x,
                     client_y,
                     client_w,
                     client_h,
                     scale,
                     vars,
                     body,
                     title,
                     host,
                     WidgetDrawPass::ChromeTitle);
    }

    UiHit UiView::hit(const UiDocument& doc,
                      WidgetTools* ui,
                      float mx,
                      float my,
                      float client_x,
                      float client_y,
                      float client_w,
                      float client_h,
                      float scale,
                      const UiVarMap& vars,
                      const UiViewHost* host,
                      bool framed) {
        UiHit out;
        const float s = std::max(0.25f, scale);
        const WindowFrameStyle style = frame_style(doc);
        if (framed && doc.kind() == UiKind::Window) {
            if (WidgetTools::window_close_hit(
                    mx, my, client_x, client_y, client_w, client_h, s, style)) {
                out.set_kind(UiHitKind::Close);
                out.set_action("window.close");
                return out;
            }
        }

        for (auto it = doc.widgets().rbegin(); it != doc.widgets().rend(); ++it) {
            UiWidget scrolled;
            if (!it->viewport().empty()) {
                scrolled = ui_scrolled_widget(*it, doc, vars);
            }
            const auto& w = it->viewport().empty() ? *it : scrolled;
            if (!widget_visible(w, vars)) {
                continue;
            }
            if (w.type() == UiWidgetType::Tabs) {
                for (const auto& tab : w.tabs()) {
                    float tw = tab.w();
                    float th = w.h() > 0.0f ? w.h() : 18.0f;
                    tw *= s;
                    th *= s;
                    const float tx = client_x + tab.x() * s;
                    const float ty = client_y + w.y() * s;
                    if (WidgetTools::point_in_rect(mx, my, tx, ty, tw, th)) {
                        out.set_kind(UiHitKind::Tab);
                        out.set_widget_id(w.id());
                        out.set_bind(w.bind());
                        out.set_tab_id(tab.id());
                        return out;
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::Button) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    out.set_kind(UiHitKind::Button);
                    out.set_widget_id(w.id());
                    out.set_action(w.action());
                    out.set_bind(w.bind());
                    return out;
                }
                continue;
            }
            if (w.type() == UiWidgetType::IconSlot) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (!w.frame_file().empty() && w.frame_w() > 0 && w.frame_h() > 0) {
                    sx += (sw - w.frame_w() * s) * 0.5f;
                    sy += (sh - w.frame_h() * s) * 0.5f;
                    sw = w.frame_w() * s;
                    sh = w.frame_h() * s;
                }
                if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    out.set_kind(UiHitKind::IconSlot);
                    out.set_widget_id(w.id());
                    out.set_bind(w.bind());
                    out.set_index(0);
                    out.set_action(w.action());
                    return out;
                }
                continue;
            }
            if (w.type() == UiWidgetType::IconGrid) {
                float gx = 0, gy = 0, gw = 0, gh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, gx, gy, gw, gh);
                (void) gw;
                (void) gh;
                float cell_w = w.cell_w() > 0.0f ? w.cell_w() : 32.0f;
                float cell_h = w.cell_h() > 0.0f ? w.cell_h() : 32.0f;
                const int capacity = grid_capacity(w);
                for (int i = 0; i < capacity; ++i) {
                    float sx = 0, sy = 0, sw = 0, sh = 0;
                    slot_cell_rect(w, i, gx, gy, s, cell_w, cell_h, sx, sy, sw, sh);
                    if (!w.frame_file().empty() && w.frame_w() > 0 && w.frame_h() > 0) {
                        sx += (sw - w.frame_w() * s) * 0.5f;
                        sy += (sh - w.frame_h() * s) * 0.5f;
                        sw = w.frame_w() * s;
                        sh = w.frame_h() * s;
                    }
                    if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                        out.set_kind(UiHitKind::IconGridCell);
                        out.set_widget_id(w.id());
                        out.set_bind(w.bind());
                        out.set_index(i + w.first_slot());
                        return out;
                    }
                }
                continue;
            }
            if (w.type() == UiWidgetType::List) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (!WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    continue;
                }
                const float lh = (w.line_height() > 0.0f ? w.line_height() : 14.0f) * s;
                int index = -1;
                if (lh > 0.5f) {
                    index = static_cast<int>((my - sy) / lh);
                }
                std::vector<UiListRow> rows;
                if (host) {
                    rows = host->list_rows(w.bind(), w, vars);
                }
                out.set_kind(UiHitKind::ListRow);
                out.set_widget_id(w.id());
                out.set_bind(w.bind());
                out.set_index(index);
                if (index >= 0 && index < static_cast<int>(rows.size())) {
                    out.set_action(rows[static_cast<std::size_t>(index)].action());
                    out.set_tab_id(rows[static_cast<std::size_t>(index)].id());
                }
                return out;
            }
            if (w.type() == UiWidgetType::Input) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    out.set_kind(UiHitKind::Input);
                    out.set_widget_id(w.id());
                    out.set_bind(w.bind());
                    return out;
                }
                continue;
            }
            if (w.type() == UiWidgetType::Canvas) {
                float sx = 0, sy = 0, sw = 0, sh = 0;
                widget_rect(w, ui, client_x, client_y, client_w, client_h, s, sx, sy, sw, sh);
                if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                    out.set_kind(UiHitKind::Canvas);
                    out.set_widget_id(w.id());
                    out.set_bind(w.bind());
                    out.set_action(w.canvas_kind());
                    return out;
                }
            }
        }

        if (framed && doc.kind() == UiKind::Window && doc.draggable()) {
            float tx = 0, ty = 0, tw = 0, th = 0;
            WidgetTools::window_title_drag_rect(
                client_x, client_y, client_w, client_h, s, style, tx, ty, tw, th);
            if (WidgetTools::point_in_rect(mx, my, tx, ty, tw, th)) {
                out.set_kind(UiHitKind::TitleDrag);
                return out;
            }
            if (WidgetTools::window_outer_hit(
                    mx, my, client_x, client_y, client_w, client_h, s, style)) {
                out.set_kind(UiHitKind::Consume);
                return out;
            }
        } else if (WidgetTools::point_in_rect(mx, my, client_x, client_y, client_w, client_h)) {
            out.set_kind(UiHitKind::Consume);
            return out;
        }
        return out;
    }

}  // namespace openkal::client
