/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ObjectView.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

namespace openkal::object_view {
    namespace {

        constexpr float kPi = 3.14159265f;
        constexpr float kFovRad = 50.0f * kPi / 180.0f;

        void bounds_from_meshes(const terrain::GbModel& cpu,
                                float& min_x,
                                float& min_y,
                                float& min_z,
                                float& max_x,
                                float& max_y,
                                float& max_z) {
            min_x = min_y = min_z = std::numeric_limits<float>::max();
            max_x = max_y = max_z = std::numeric_limits<float>::lowest();
            bool any = false;
            for (const auto& mesh : cpu.meshes) {
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    const float x = mesh.pos_uv[i];
                    const float y = mesh.pos_uv[i + 1];
                    const float z = mesh.pos_uv[i + 2];
                    min_x = std::min(min_x, x);
                    min_y = std::min(min_y, y);
                    min_z = std::min(min_z, z);
                    max_x = std::max(max_x, x);
                    max_y = std::max(max_y, y);
                    max_z = std::max(max_z, z);
                    any = true;
                }
            }
            if (!any) {
                min_x = cpu.min_x;
                min_y = cpu.min_y;
                min_z = cpu.min_z;
                max_x = cpu.max_x;
                max_y = cpu.max_y;
                max_z = cpu.max_z;
            }
        }

    }  // namespace

    void GpuPreview::set_bounds(
        float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
        m_min_y = min_y;
        m_center_x = 0.5f * (min_x + max_x);
        m_center_y = 0.5f * (min_y + max_y);
        m_center_z = 0.5f * (min_z + max_z);
        const float dx = max_x - min_x;
        const float dy = max_y - min_y;
        const float dz = max_z - min_z;
        m_radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(m_radius > 1.0f)) {
            m_radius = 50.0f;
        }
    }

    void GpuPreview::set_texture_counts(int ok, int missing) {
        m_textures_ok = ok;
        m_textures_missing = missing;
    }

    ObjectView::ObjectView(std::string data_root,
                           std::string models_dir,
                           std::string section_label,
                           std::string window_title,
                           float ui_scale,
                           std::vector<std::string> gb_paths)
        : m_data_root(std::move(data_root)), m_models_dir(std::move(models_dir)),
          m_section_label(std::move(section_label)), m_window_title(std::move(window_title)),
          m_gb_paths(std::move(gb_paths)), m_ui_scale(std::clamp(ui_scale, 0.5f, 8.0f)),
          m_font_scale(std::max(2.5f, 2.0f * m_ui_scale)) {}

    ObjectView::~ObjectView() {
        destroy_gpu();
        m_ui.destroy();
        if (m_engine) {
            m_engine->shutdown();
        }
    }

    int ObjectView::run() {
        const std::size_t found = m_gb_paths.empty()
                                      ? m_catalog.scan(m_models_dir)
                                      : m_catalog.load_files(m_gb_paths, m_models_dir);
        if (m_gb_paths.empty()) {
            log::info(m_window_title + " " + m_section_label + "=" + m_models_dir +
                      " gb=" + std::to_string(found));
        } else {
            log::info("object_view explicit gb=" + std::to_string(found) + " from " +
                      std::to_string(m_gb_paths.size()) + " args");
        }
        if (!init_window()) {
            log::error("object_view: engine init failed (no display / GLFW?)");
            return 1;
        }
        create_ground();
        m_ui.init(*m_engine, m_data_root);
        if (!m_catalog.empty()) {
            select_visible(0);
        } else if (m_gb_paths.empty()) {
            m_status = "no .gb files under " + m_section_label;
        } else {
            m_status = "no matching .gb files";
        }

        engine::FrameInput in;
        float time_sec = 0.0f;
        while (m_engine->poll(in)) {
            if (in.quit) {
                break;
            }
            time_sec += in.dt;
            const float panel_w = 440.0f * m_ui_scale;
            handle_input(in, panel_w);
            if (m_quit) {
                break;
            }
            m_engine->begin_frame(0.16f, 0.18f, 0.22f);
            draw_preview(in, panel_w, time_sec);
            draw_ui(in, panel_w);
            m_engine->end_frame();
        }
        return 0;
    }

    bool ObjectView::init_window() {
        const int win_w = std::max(1280, static_cast<int>(std::lround(1600.0f * m_ui_scale)));
        const int win_h = std::max(720, static_cast<int>(std::lround(900.0f * m_ui_scale)));
        const std::string title = "openkal " + m_window_title;
        m_engine.reset(engine::create_engine());
        if (!m_engine || !m_engine->init(title.c_str(), win_w, win_h)) {
            return false;
        }
        m_engine->set_cursor_mode(engine::CursorMode::Free);
        return true;
    }

    void ObjectView::create_ground() {
        constexpr int kCells = 8;
        constexpr int kPx = 64;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kPx * kPx * 4));
        for (int y = 0; y < kPx; ++y) {
            for (int x = 0; x < kPx; ++x) {
                const int cx = x * kCells / kPx;
                const int cy = y * kCells / kPx;
                const bool dark = ((cx + cy) & 1) != 0;
                const std::uint8_t c = dark ? 48 : 72;
                const std::size_t o = static_cast<std::size_t>((y * kPx + x) * 4);
                rgba[o + 0] = c;
                rgba[o + 1] = static_cast<std::uint8_t>(c + 4);
                rgba[o + 2] = static_cast<std::uint8_t>(c + 8);
                rgba[o + 3] = 255;
            }
        }
        m_ground_tex = m_engine->create_texture_rgba8(rgba.data(), kPx, kPx, true);
        const engine::VertexP3T2 verts[4] = {
            {-1.0f, 0.0f, -1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, -1.0f, 8.0f, 0.0f},
            {1.0f, 0.0f, 1.0f, 8.0f, 8.0f},
            {-1.0f, 0.0f, 1.0f, 0.0f, 8.0f},
        };
        const std::uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        m_ground_mesh = m_engine->create_mesh(verts, 4, idx, 6);
    }

    void ObjectView::destroy_gpu() {
        if (!m_engine) {
            return;
        }
        for (auto& [path, preview] : m_models) {
            (void) path;
            for (auto& part : preview.parts()) {
                if (part.mesh()) {
                    m_engine->destroy_mesh(part.mesh());
                }
            }
        }
        m_models.clear();
        for (auto& [key, tex] : m_textures) {
            (void) key;
            if (tex) {
                m_engine->destroy_texture(tex);
            }
        }
        m_textures.clear();
        if (m_ground_mesh) {
            m_engine->destroy_mesh(m_ground_mesh);
            m_ground_mesh = {};
        }
        if (m_ground_tex) {
            m_engine->destroy_texture(m_ground_tex);
            m_ground_tex = {};
        }
        m_current = nullptr;
    }

    const GpuPreview* ObjectView::get_or_load(const std::string& gb_path) {
        auto it = m_models.find(gb_path);
        if (it != m_models.end()) {
            return it->second.parts().empty() ? nullptr : &it->second;
        }

        terrain::GbModel cpu;
        if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
            m_models.emplace(gb_path, GpuPreview{});
            return nullptr;
        }

        GpuPreview gpu;
        float min_x = 0, min_y = 0, min_z = 0, max_x = 0, max_y = 0, max_z = 0;
        bounds_from_meshes(cpu, min_x, min_y, min_z, max_x, max_y, max_z);
        gpu.set_bounds(min_x, min_y, min_z, max_x, max_y, max_z);

        int tex_ok = 0;
        int tex_missing = 0;
        const bool model_is_water = cpu.option.find("Water") != std::string::npos ||
                                    cpu.option.find("water") != std::string::npos;
        for (const auto& mesh : cpu.meshes) {
            GpuPart part;
            part.set_alpha_blend(mesh.alpha_blend);
            part.set_alpha_test(mesh.alpha_test);
            part.set_two_sided(mesh.two_sided);
            std::vector<engine::VertexP3T2> verts;
            verts.reserve(mesh.pos_uv.size() / 5);
            for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                verts.push_back({mesh.pos_uv[i],
                                 mesh.pos_uv[i + 1],
                                 mesh.pos_uv[i + 2],
                                 mesh.pos_uv[i + 3],
                                 mesh.pos_uv[i + 4]});
            }
            part.set_mesh(m_engine->create_mesh(
                verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size()));

            m_engine->set_mesh_specular(part.mesh(), mesh.normals, mesh.specular_power);

            std::string tex_name;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(cpu.materials.size())) {
                tex_name = cpu.materials[static_cast<std::size_t>(mesh.material_index)].texture;
                part.set_uv_material(cpu.materials[static_cast<std::size_t>(mesh.material_index)]);
                part.set_uv_key_times(cpu.material_key_times);
            }
            part.set_texture_name(tex_name);
            if (terrain::is_waterfall_texture(tex_name) ||
                terrain::is_water_surface_texture(tex_name) || model_is_water) {
                part.set_alpha_blend(true);
            }

            const std::string tex_key = gb_path + "|" + tex_name;
            auto tit = m_textures.find(tex_key);
            if (tit != m_textures.end()) {
                part.set_texture(tit->second);
                part.set_texture_loaded(true);
                ++tex_ok;
            } else {
                terrain::ImageRgba img;
                const bool loaded =
                    terrain::load_gb_texture(gb_path, tex_name, img) && !img.rgba.empty();
                if (!loaded) {
                    img.width = img.height = 1;
                    img.rgba = {180, 160, 140, 255};
                    ++tex_missing;
                } else {
                    ++tex_ok;
                }
                part.set_texture_loaded(loaded);
                const engine::TextureHandle tex =
                    m_engine->create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
                m_textures.emplace(tex_key, tex);
                part.set_texture(tex);
            }
            gpu.parts().push_back(std::move(part));
        }
        gpu.set_texture_counts(tex_ok, tex_missing);
        auto [ins, ok] = m_models.emplace(gb_path, std::move(gpu));
        return ok && !ins->second.parts().empty() ? &ins->second : nullptr;
    }

    void ObjectView::fit_camera(const GpuPreview& preview) {
        m_target = engine::Vec3{preview.center_x(), preview.center_y(), preview.center_z()};
        const float half_fov = kFovRad * 0.5f;
        m_fit_distance = preview.radius() / std::max(0.12f, std::tan(half_fov)) * 1.25f;
        m_distance = m_fit_distance;
        m_yaw = 0.7f;
        m_pitch = -0.38f;
    }

    void ObjectView::handle_input(const engine::FrameInput& in, float panel_w) {
        if (in.key_escape_pressed) {
            if (!m_catalog.filter().empty()) {
                m_catalog.set_filter({});
                m_scroll = 0;
                if (!m_catalog.visible().empty()) {
                    select_visible(0);
                }
            } else {
                m_quit = true;
            }
            return;
        }

        std::string filter = m_catalog.filter();
        if (in.key_backspace) {
            m_backspace_hold += in.dt;
            const bool fire = in.key_backspace_pressed || m_backspace_hold > 0.35f;
            if (fire && !filter.empty()) {
                filter.pop_back();
                if (m_backspace_hold > 0.35f) {
                    m_backspace_hold = 0.28f;
                }
            }
        } else {
            m_backspace_hold = 0.0f;
        }
        for (int i = 0; i < in.text_char_count; ++i) {
            const char ch = in.text_chars[i];
            if (ch >= 32 && ch < 127 && ch != '`') {
                filter.push_back(ch);
            }
        }
        if (filter != m_catalog.filter()) {
            std::string keep;
            if (m_selected >= 0 && m_selected < static_cast<int>(m_catalog.visible_count())) {
                keep = m_catalog.visible_entry(m_selected).absolute_path();
            }
            m_catalog.set_filter(std::move(filter));
            m_scroll = 0;
            int next = 0;
            for (int i = 0; i < static_cast<int>(m_catalog.visible_count()); ++i) {
                if (m_catalog.visible_entry(i).absolute_path() == keep) {
                    next = i;
                    break;
                }
            }
            if (m_catalog.visible().empty()) {
                m_selected = -1;
                m_current = nullptr;
                m_status = "no matches";
            } else {
                select_visible(next);
            }
        }

        const bool over_list = in.mouse_x < panel_w;
        if (over_list && in.mouse_scroll != 0.0f) {
            const int delta = in.mouse_scroll > 0.0f ? -3 : 3;
            const int max_scroll =
                std::max(0, static_cast<int>(m_catalog.visible_count()) - m_visible_rows);
            m_scroll = std::clamp(m_scroll + delta, 0, max_scroll);
        }

        if (in.key_up || in.key_down) {
            m_nav_hold += in.dt;
            const bool fire = (m_nav_hold <= in.dt + 1e-4f) || m_nav_hold > 0.32f;
            if (fire) {
                move_selection(in.key_up ? -1 : 1);
                if (m_nav_hold > 0.32f) {
                    m_nav_hold = 0.26f;
                }
            }
        } else {
            m_nav_hold = 0.0f;
        }

        const float list_top = 48.0f * m_ui_scale + 44.0f * m_ui_scale;
        const float row_h = 8.0f * m_font_scale + 10.0f * m_ui_scale;
        if (in.mouse_left_clicked && over_list && in.mouse_y >= list_top) {
            const int row = static_cast<int>((in.mouse_y - list_top) / row_h);
            const int index = m_scroll + row;
            if (row >= 0 && row < m_visible_rows && index >= 0 &&
                index < static_cast<int>(m_catalog.visible_count())) {
                select_visible(index);
            }
        }

        const bool over_preview = in.mouse_x >= panel_w;
        if (over_preview && in.mouse_scroll != 0.0f && m_current) {
            const float factor = (in.mouse_scroll > 0.0f) ? 0.88f : 1.12f;
            m_distance =
                std::clamp(m_distance * factor, m_fit_distance * 0.08f, m_fit_distance * 12.0f);
        }

        if (over_preview && in.mouse_left) {
            m_yaw += in.mouse_dx * 0.005f;
            m_pitch = std::clamp(m_pitch + in.mouse_dy * 0.005f, -1.45f, 1.45f);
        }

        if (over_preview && (in.mouse_right || in.mouse_middle) && m_current) {
            const float pan_scale = m_distance * 0.0015f;
            m_target.x -= in.mouse_dx * pan_scale;
            m_target.y += in.mouse_dy * pan_scale;
        }

        if (in.key_r_pressed && m_current) {
            fit_camera(*m_current);
        }
    }

    void ObjectView::move_selection(int delta) {
        if (m_catalog.visible().empty()) {
            return;
        }
        const int last = static_cast<int>(m_catalog.visible_count()) - 1;
        const int next = std::clamp((m_selected < 0 ? 0 : m_selected) + delta, 0, last);
        select_visible(next);
    }

    void ObjectView::ensure_selection_visible() {
        if (m_selected < 0) {
            return;
        }
        if (m_selected < m_scroll) {
            m_scroll = m_selected;
        } else if (m_selected >= m_scroll + m_visible_rows) {
            m_scroll = m_selected - m_visible_rows + 1;
        }
        const int max_scroll =
            std::max(0, static_cast<int>(m_catalog.visible_count()) - m_visible_rows);
        m_scroll = std::clamp(m_scroll, 0, max_scroll);
    }

    void ObjectView::select_visible(int visible_index) {
        if (visible_index < 0 || visible_index >= static_cast<int>(m_catalog.visible_count())) {
            m_selected = -1;
            m_current = nullptr;
            return;
        }
        m_selected = visible_index;
        const ObjectEntry& entry = m_catalog.visible_entry(visible_index);
        m_current = get_or_load(entry.absolute_path());
        if (!m_current) {
            m_status = "failed to load " + entry.relative_path();
            return;
        }
        fit_camera(*m_current);
        ensure_selection_visible();
        char line[256];
        std::snprintf(line,
                      sizeof(line),
                      "%s  meshes=%zu  tex=%d/%d  r=%.1f",
                      entry.relative_path().c_str(),
                      m_current->parts().size(),
                      m_current->textures_ok(),
                      m_current->textures_ok() + m_current->textures_missing(),
                      m_current->radius());
        m_status = line;
    }

    void ObjectView::draw_preview(const engine::FrameInput& in, float panel_w, float time_sec) {
        const float win_w = static_cast<float>(in.width);
        const float win_h = static_cast<float>(in.height);
        const float pane_x = panel_w;
        const float pane_w = std::max(1.0f, win_w - panel_w);
        const float znear = std::max(0.25f, m_distance * 0.01f);
        const float zfar = m_distance * 40.0f + (m_current ? m_current->radius() * 8.0f : 1000.0f);
        const auto proj =
            pane_perspective(kFovRad, pane_x, 0.0f, pane_w, win_h, win_w, win_h, znear, zfar);

        const float cp = std::cos(m_pitch);
        const engine::Vec3 eye{m_target.x + std::sin(m_yaw) * cp * m_distance,
                               m_target.y - std::sin(m_pitch) * m_distance,
                               m_target.z + std::cos(m_yaw) * cp * m_distance};
        const auto view =
            engine::Mat4::multiply(engine::Mat4::scaling(-1.0f, 1.0f, 1.0f),
                                   engine::Mat4::look_at(eye, m_target, {0.0f, 1.0f, 0.0f}));
        m_engine->set_camera(view, proj);

        if (m_current && m_ground_mesh && m_ground_tex) {
            const float s = std::max(40.0f, m_current->radius() * 3.0f);
            const auto ground = engine::Mat4::multiply(
                engine::Mat4::translation(
                    m_current->center_x(), m_current->min_y(), m_current->center_z()),
                engine::Mat4::scaling(s, 1.0f, s));
            m_engine->draw_mesh_textured(m_ground_mesh, m_ground_tex, ground, false, true);
        }

        if (!m_current) {
            return;
        }
        const auto model = engine::Mat4::identity();
        for (int pass = 0; pass < 2; ++pass) {
            const bool want_alpha = pass == 1;
            for (const auto& part : m_current->parts()) {
                if (part.alpha_blend() != want_alpha) {
                    continue;
                }
                float uv_u = 0.0f;
                float uv_v = 0.0f;
                float opacity = 1.0f;
                const float time_ms = time_sec * 1000.0f;
                if (!terrain::sample_material_uv(
                        part.uv_material(), part.uv_key_times(), time_ms, uv_u, uv_v, opacity)) {
                    if (terrain::is_waterfall_texture(part.texture_name())) {
                        uv_v = -time_sec * 0.35f;
                    } else if (terrain::is_water_surface_texture(part.texture_name())) {
                        uv_u = time_sec * 0.03f;
                        uv_v = time_sec * 0.02f;
                    }
                }
                const bool blend = part.alpha_blend() || opacity < 0.999f;
                m_engine->draw_mesh_textured(part.mesh(),
                                             part.texture(),
                                             model,
                                             blend,
                                             part.two_sided(),
                                             part.alpha_test() && !blend,
                                             false,
                                             uv_u,
                                             uv_v,
                                             1.0f,
                                             1.0f,
                                             1.0f,
                                             opacity);
            }
        }
    }

    void ObjectView::draw_ui(const engine::FrameInput& in, float panel_w) {
        const float s = m_ui_scale;
        const float win_h = static_cast<float>(in.height);
        const float pad = 16.0f * s;
        const float title_h = 48.0f * s;
        const float filter_h = 44.0f * s;
        const float status_h = 110.0f * s;
        const float row_h = 8.0f * m_font_scale + 10.0f * s;
        const float list_top = title_h + filter_h;
        const float list_h = std::max(row_h, win_h - list_top - status_h);
        m_visible_rows = std::max(1, static_cast<int>(list_h / row_h));
        ensure_selection_visible();

        m_engine->draw_ui_rect(0.0f, 0.0f, panel_w, win_h, 0.07f, 0.08f, 0.10f, 0.96f);
        m_engine->draw_ui_rect(
            panel_w - 2.0f * s, 0.0f, 2.0f * s, win_h, 0.20f, 0.22f, 0.26f, 1.0f);

        m_ui.draw_text_glyphs(pad, pad * 0.7f, m_font_scale, m_section_label, 0.95f, 0.92f, 0.75f);
        char count_line[64];
        std::snprintf(count_line,
                      sizeof(count_line),
                      "%d / %d",
                      static_cast<int>(m_catalog.visible_count()),
                      static_cast<int>(m_catalog.size()));
        m_ui.draw_text_glyphs(pad + m_ui.text_width(m_font_scale, m_section_label + "  "),
                              pad * 0.7f,
                              m_font_scale,
                              count_line,
                              0.65f,
                              0.68f,
                              0.72f);

        const float filter_y = title_h + 4.0f * s;
        m_engine->draw_ui_rect(
            pad, filter_y, panel_w - pad * 2.0f, filter_h - 8.0f * s, 0.04f, 0.045f, 0.055f, 1.0f);
        std::string filter_text = m_catalog.filter();
        if (filter_text.empty()) {
            m_ui.draw_text_glyphs(pad + 8.0f * s,
                                  filter_y + 8.0f * s,
                                  m_font_scale,
                                  "type to filter",
                                  0.40f,
                                  0.42f,
                                  0.46f);
        } else {
            filter_text.push_back('_');
            draw_text_fit(pad + 8.0f * s,
                          filter_y + 8.0f * s,
                          panel_w - pad * 2.0f - 16.0f * s,
                          filter_text,
                          0.92f,
                          0.94f,
                          0.96f);
        }

        const int total = static_cast<int>(m_catalog.visible_count());
        for (int row = 0; row < m_visible_rows; ++row) {
            const int index = m_scroll + row;
            if (index >= total) {
                break;
            }
            const float y = list_top + static_cast<float>(row) * row_h;
            draw_row(0.0f, y, panel_w, row_h, m_catalog.visible_entry(index), index == m_selected);
        }

        if (total > m_visible_rows) {
            const float track_x = panel_w - 10.0f * s;
            const float track_h = list_h;
            m_engine->draw_ui_rect(track_x, list_top, 6.0f * s, track_h, 0.12f, 0.13f, 0.15f, 1.0f);
            const float thumb_h =
                std::max(16.0f * s,
                         track_h * static_cast<float>(m_visible_rows) / static_cast<float>(total));
            const float max_scroll = static_cast<float>(total - m_visible_rows);
            const float thumb_y =
                list_top +
                (track_h - thumb_h) *
                    (max_scroll > 0.0f ? static_cast<float>(m_scroll) / max_scroll : 0.0f);
            m_engine->draw_ui_rect(track_x, thumb_y, 6.0f * s, thumb_h, 0.55f, 0.58f, 0.62f, 1.0f);
        }

        const float status_y = win_h - status_h;
        m_engine->draw_ui_rect(0.0f, status_y, panel_w, status_h, 0.05f, 0.055f, 0.07f, 1.0f);
        if (!m_status.empty()) {
            draw_text_fit(
                pad, status_y + 10.0f * s, panel_w - pad * 2.0f, m_status, 0.80f, 0.82f, 0.86f);
        }
        m_ui.draw_text_glyphs(pad,
                              status_y + 44.0f * s,
                              m_font_scale * 0.75f,
                              "LMB orbit  RMB pan  wheel zoom  R reset",
                              0.50f,
                              0.52f,
                              0.56f);
        m_ui.draw_text_glyphs(pad,
                              status_y + 70.0f * s,
                              m_font_scale * 0.75f,
                              "Up/Down select  Esc quit",
                              0.50f,
                              0.52f,
                              0.56f);

        if (!m_current && m_catalog.empty()) {
            m_ui.draw_text_glyphs(panel_w + 32.0f * s,
                                  win_h * 0.45f,
                                  m_font_scale * 1.2f,
                                  m_gb_paths.empty() ? "No .gb files in " + m_section_label
                                                     : "No matching .gb files",
                                  0.85f,
                                  0.7f,
                                  0.5f);
        }
    }

    void ObjectView::draw_row(
        float x, float y, float w, float h, const ObjectEntry& entry, bool selected) {
        const float s = m_ui_scale;
        if (selected) {
            m_engine->draw_ui_rect(x, y, w, h, 0.22f, 0.32f, 0.42f, 0.95f);
        }
        const float text_y = y + std::max(2.0f, (h - 7.0f * m_font_scale) * 0.5f);
        std::string name = entry.relative_path();
        if (name.size() >= 3) {
            name.resize(name.size() - 3);
        }
        draw_text_fit(x + 14.0f * s, text_y, w - 28.0f * s, name, 0.95f, 0.96f, 0.98f);
    }

    void ObjectView::draw_text_fit(
        float x, float y, float max_w, const std::string& text, float r, float g, float b) {
        if (m_ui.text_width(m_font_scale, text) <= max_w) {
            m_ui.draw_text_glyphs(x, y, m_font_scale, text, r, g, b);
            return;
        }
        std::string clipped = text;
        const std::string ellipsis = "...";
        while (clipped.size() > 1 && m_ui.text_width(m_font_scale, clipped + ellipsis) > max_w) {
            clipped.pop_back();
        }
        m_ui.draw_text_glyphs(x, y, m_font_scale, clipped + ellipsis, r, g, b);
    }

    engine::Mat4 ObjectView::pane_perspective(float fovy_rad,
                                              float pane_x,
                                              float pane_y,
                                              float pane_w,
                                              float pane_h,
                                              float win_w,
                                              float win_h,
                                              float znear,
                                              float zfar) {
        const float aspect = pane_h > 1.0f ? pane_w / pane_h : 1.0f;
        const auto proj = engine::Mat4::perspective(fovy_rad, aspect, znear, zfar);
        engine::Mat4 fit = engine::Mat4::identity();
        fit.m[0] = pane_w / std::max(1.0f, win_w);
        fit.m[5] = pane_h / std::max(1.0f, win_h);
        fit.m[12] = (2.0f * pane_x + pane_w) / std::max(1.0f, win_w) - 1.0f;
        fit.m[13] = 1.0f - (2.0f * pane_y + pane_h) / std::max(1.0f, win_h);
        return engine::Mat4::multiply(fit, proj);
    }

}  // namespace openkal::object_view
