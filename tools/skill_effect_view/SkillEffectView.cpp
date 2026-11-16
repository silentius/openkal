/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkillEffectView.h"

#include "openkal/platform/Log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

namespace openkal::skill_effect_view {
    namespace {

        constexpr float kPi = 3.14159265f;
        constexpr float kFovRad = 50.0f * kPi / 180.0f;
        constexpr std::uint32_t kPreviewEntity = 1;
        constexpr int kTabCount = kBrowseModeCount;

        [[nodiscard]] std::vector<engine::VertexP3T2>
        to_engine_verts(const terrain::GbMeshCpu& mesh) {
            std::vector<engine::VertexP3T2> verts;
            verts.reserve(mesh.pos_uv.size() / 5);
            for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                verts.push_back({mesh.pos_uv[i],
                                 mesh.pos_uv[i + 1],
                                 mesh.pos_uv[i + 2],
                                 mesh.pos_uv[i + 3],
                                 mesh.pos_uv[i + 4]});
            }
            return verts;
        }

        void bounds_from_meshes(const std::vector<terrain::GbMeshCpu>& meshes,
                                const terrain::GbModel& cpu,
                                float& min_x,
                                float& min_y,
                                float& min_z,
                                float& max_x,
                                float& max_y,
                                float& max_z) {
            min_x = min_y = min_z = std::numeric_limits<float>::max();
            max_x = max_y = max_z = std::numeric_limits<float>::lowest();
            bool any = false;
            for (const auto& mesh : meshes) {
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

        [[nodiscard]] bool point_in_rect(float px, float py, float x, float y, float w, float h) {
            return px >= x && px < x + w && py >= y && py < y + h;
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

    SkillEffectView::SkillEffectView(std::string data_root,
                                     std::string skills_dir,
                                     std::string monsters_dir,
                                     std::string npcs_dir,
                                     float ui_scale)
        : m_data_root(std::move(data_root)), m_skills_dir(std::move(skills_dir)),
          m_monsters_dir(std::move(monsters_dir)), m_npcs_dir(std::move(npcs_dir)),
          m_ui_scale(std::clamp(ui_scale, 0.5f, 8.0f)),
          m_font_scale(std::max(1.0f, 0.85f * m_ui_scale)) {}

    SkillEffectView::~SkillEffectView() {
        destroy_gpu();
        if (m_engine) {
            m_font.destroy(*m_engine);
            m_engine->shutdown();
        }
    }

    int SkillEffectView::run() {
        const std::size_t assets = m_catalog.scan_assets(m_data_root);
        const std::size_t skills = m_catalog.load_skills(m_skills_dir);
        log::info("effect_view data=" + m_data_root + " skills=" + m_skills_dir +
                  " gb=" + std::to_string(assets) + " named=" + std::to_string(skills));
        if (!init_window()) {
            log::error("effect_view: engine init failed (no display / GLFW?)");
            return 1;
        }
        if (!m_sprite_fx.init(*m_engine, m_data_root)) {
            log::warn("effect_view: SpriteFx init failed");
        }
        m_catalog.set_scripts(m_sprite_fx.sprite_names(), m_sprite_fx.fx_names());
        const std::size_t mobs = m_catalog.load_users(m_monsters_dir, m_npcs_dir);
        log::info("effect_view sprites=" + std::to_string(m_catalog.sprites().size()) + " fx=" +
                  std::to_string(m_catalog.fxs().size()) + " mobs=" + std::to_string(mobs));
        create_ground();
        if (m_catalog.visible_count() > 0) {
            select_visible(0);
        } else {
            m_status = "no named skill effects — try Models / Sprite / Fx";
        }

        engine::FrameInput in;
        float time_sec = 0.0f;
        while (m_engine->poll(in)) {
            if (in.quit) {
                break;
            }
            time_sec += in.dt;
            m_anim_ms += in.dt * 1000.0f;
            if (m_current) {
                const float clip = m_current->anim().duration_ms();
                if (clip > 1.0f && m_anim_ms >= clip) {
                    m_anim_ms = std::fmod(m_anim_ms, clip);
                }
                skin_preview(*m_current, m_anim_ms);
            }
            const float panel_w = 520.0f * m_ui_scale;
            const PanelLayout layout = make_layout(panel_w, static_cast<float>(in.height));
            m_visible_rows = layout.m_visible_rows;
            handle_input(in, layout);
            if (m_quit) {
                break;
            }
            m_engine->begin_frame(0.08f, 0.09f, 0.12f);
            draw_preview(in, panel_w, time_sec);
            draw_ui(in, layout);
            m_engine->end_frame();
        }
        return 0;
    }

    bool SkillEffectView::init_window() {
        const int win_w = std::max(1280, static_cast<int>(std::lround(1680.0f * m_ui_scale)));
        const int win_h = std::max(720, static_cast<int>(std::lround(920.0f * m_ui_scale)));
        m_engine.reset(engine::create_engine());
        if (!m_engine || !m_engine->init("openkal effect_view", win_w, win_h)) {
            return false;
        }
        // Keep the OS pointer visible. CursorMode::Free hides it for the in-game
        // software cursor, which this tool does not draw.
        const std::string font_path = client::FreetypeFont::find_system_sans();
        if (font_path.empty() ||
            !m_font.load(*m_engine, font_path, client::FreetypeFont::kTipPixelSize)) {
            log::warn("effect_view: TrueType font missing — list labels may be blank");
        }
        return true;
    }

    void SkillEffectView::create_ground() {
        constexpr int kCells = 8;
        constexpr int kPx = 64;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kPx * kPx * 4));
        for (int y = 0; y < kPx; ++y) {
            for (int x = 0; x < kPx; ++x) {
                const int cx = x * kCells / kPx;
                const int cy = y * kCells / kPx;
                const bool dark = ((cx + cy) & 1) != 0;
                const std::uint8_t c = dark ? 36 : 52;
                const std::size_t o = static_cast<std::size_t>((y * kPx + x) * 4);
                rgba[o + 0] = c;
                rgba[o + 1] = static_cast<std::uint8_t>(c + 4);
                rgba[o + 2] = static_cast<std::uint8_t>(c + 10);
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

    void SkillEffectView::destroy_gpu() {
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
        m_sprite_fx.destroy(*m_engine);
        m_fx_preview = false;
        m_current = nullptr;
    }

    GpuPreview* SkillEffectView::get_or_load(const std::string& gb_path) {
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
        gpu.bind_meshes() = cpu.meshes;
        gpu.set_bones(cpu.bones);
        if (!cpu.anims.empty()) {
            gpu.set_anim(cpu.anims[0]);
        }
        gpu.set_billboard(gpu.bind_meshes().size() == 1 &&
                          gpu.bind_meshes()[0].pos_uv.size() <= 5 * 4 + 1);

        float min_x = 0, min_y = 0, min_z = 0, max_x = 0, max_y = 0, max_z = 0;
        bounds_from_meshes(gpu.bind_meshes(), cpu, min_x, min_y, min_z, max_x, max_y, max_z);
        gpu.set_bounds(min_x, min_y, min_z, max_x, max_y, max_z);

        int tex_ok = 0;
        int tex_missing = 0;
        for (const auto& mesh : gpu.bind_meshes()) {
            GpuPart part;
            part.set_alpha_blend(mesh.alpha_blend);
            part.set_alpha_test(mesh.alpha_test);
            const auto verts = to_engine_verts(mesh);
            part.set_mesh(m_engine->create_mesh(
                verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size()));

            std::string tex_name;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(cpu.materials.size())) {
                tex_name = cpu.materials[static_cast<std::size_t>(mesh.material_index)].texture;
                part.set_uv_material(cpu.materials[static_cast<std::size_t>(mesh.material_index)]);
                part.set_uv_key_times(cpu.material_key_times);
            }

            const std::string tex_key = gb_path + "|" + tex_name;
            auto tit = m_textures.find(tex_key);
            if (tit != m_textures.end()) {
                part.set_texture(tit->second);
                ++tex_ok;
            } else {
                terrain::ImageRgba img;
                const bool loaded =
                    terrain::load_gb_texture(gb_path, tex_name, img) && !img.rgba.empty();
                if (!loaded) {
                    img.width = img.height = 1;
                    img.rgba = {180, 220, 255, 200};
                    ++tex_missing;
                } else {
                    ++tex_ok;
                }
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

    SkillEffectView::PanelLayout SkillEffectView::make_layout(float panel_w, float win_h) const {
        const float s = m_ui_scale;
        PanelLayout layout;
        layout.m_panel_w = panel_w;
        layout.m_pad = 16.0f * s;
        layout.m_title_h = 44.0f * s;
        layout.m_tab_h = 36.0f * s;
        layout.m_tabs_y = layout.m_title_h;
        layout.m_tab_w =
            (panel_w - layout.m_pad * 2.0f - 6.0f * s * static_cast<float>(kTabCount - 1)) /
            static_cast<float>(kTabCount);
        layout.m_filter_h = 40.0f * s;
        layout.m_filter_y = layout.m_title_h + layout.m_tab_h + 8.0f * s;
        layout.m_chip_h = 34.0f * s;
        const float chip_row_h =
            m_catalog.mode() == BrowseMode::Skills ? layout.m_chip_h + 8.0f * s : 0.0f;
        layout.m_chips_y = layout.m_filter_y + layout.m_filter_h;
        layout.m_chip_w = (panel_w - layout.m_pad * 2.0f - 16.0f * s) / 3.0f;
        layout.m_status_h = 128.0f * s;
        layout.m_status_y = win_h - layout.m_status_h;
        const float line_h = m_font.ready() ? m_font.line_height() * m_font_scale : 18.0f * s;
        layout.m_row_h = line_h + 10.0f * s;
        layout.m_list_top =
            layout.m_title_h + layout.m_tab_h + 8.0f * s + layout.m_filter_h + chip_row_h;
        layout.m_list_h = std::max(layout.m_row_h, layout.m_status_y - layout.m_list_top);
        layout.m_visible_rows = std::max(1, static_cast<int>(layout.m_list_h / layout.m_row_h));
        return layout;
    }

    void SkillEffectView::fit_camera(const GpuPreview& preview) {
        m_target = engine::Vec3{preview.center_x(), preview.center_y(), preview.center_z()};
        const float half_fov = kFovRad * 0.5f;
        m_fit_distance = preview.radius() / std::max(0.12f, std::tan(half_fov)) * 1.35f;
        m_distance = m_fit_distance;
        m_yaw = 0.7f;
        m_pitch = -0.38f;
    }

    void SkillEffectView::handle_input(const engine::FrameInput& in, const PanelLayout& layout) {
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

        if (in.key_space_pressed) {
            m_anim_ms = 0.0f;
            if (m_fx_preview) {
                m_sprite_fx.rewind(kPreviewEntity);
            }
        }

        if (in.key_left || in.key_right) {
            if (m_slot_hold <= 0.0f) {
                select_slot(m_slot + (in.key_right ? 1 : -1));
            }
            m_slot_hold += in.dt;
        } else {
            m_slot_hold = 0.0f;
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
            m_catalog.set_filter(std::move(filter));
            m_scroll = 0;
            if (m_catalog.visible().empty()) {
                m_selected = -1;
                m_current = nullptr;
                clear_fx_preview();
                m_status = "no matches";
            } else {
                select_visible(0);
            }
        }

        const float s = m_ui_scale;
        const bool over_panel = layout.over_panel(in.mouse_x);
        const bool over_list = layout.over_list(in.mouse_x, in.mouse_y);
        m_hover = -1;
        if (over_list) {
            const int row = static_cast<int>((in.mouse_y - layout.m_list_top) / layout.m_row_h);
            const int index = m_scroll + row;
            if (row >= 0 && row < layout.m_visible_rows && index >= 0 &&
                index < static_cast<int>(m_catalog.visible_count())) {
                m_hover = index;
            }
        }

        if (in.mouse_left_clicked && over_panel) {
            for (int t = 0; t < kTabCount; ++t) {
                if (point_in_rect(in.mouse_x,
                                  in.mouse_y,
                                  tab_x(layout, t),
                                  layout.m_tabs_y,
                                  layout.m_tab_w,
                                  layout.m_tab_h)) {
                    set_mode(static_cast<BrowseMode>(t));
                }
            }
            if (m_catalog.mode() == BrowseMode::Skills) {
                for (int g = 0; g < 3; ++g) {
                    const float cx =
                        layout.m_pad + static_cast<float>(g) * (layout.m_chip_w + 8.0f * s);
                    if (point_in_rect(in.mouse_x,
                                      in.mouse_y,
                                      cx,
                                      layout.m_chips_y,
                                      layout.m_chip_w,
                                      layout.m_chip_h)) {
                        select_slot(g);
                    }
                }
            }
            if (m_hover >= 0) {
                select_visible(m_hover);
            }
        }

        if (over_panel && in.mouse_scroll != 0.0f) {
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

        const bool over_preview = !over_panel;
        if (over_preview && in.mouse_scroll != 0.0f && (m_current || m_fx_preview)) {
            const float factor = (in.mouse_scroll > 0.0f) ? 0.88f : 1.12f;
            m_distance =
                std::clamp(m_distance * factor, m_fit_distance * 0.08f, m_fit_distance * 12.0f);
        }
        if (over_preview && in.mouse_left) {
            m_yaw += in.mouse_dx * 0.005f;
            m_pitch = std::clamp(m_pitch + in.mouse_dy * 0.005f, -1.45f, 1.45f);
        }
        if (over_preview && (in.mouse_right || in.mouse_middle) && (m_current || m_fx_preview)) {
            const float pan_scale = m_distance * 0.0015f;
            m_target.x -= in.mouse_dx * pan_scale;
            m_target.y += in.mouse_dy * pan_scale;
        }
        if (in.key_r_pressed && (m_current || m_fx_preview)) {
            if (m_current) {
                fit_camera(*m_current);
            } else {
                fit_fx_camera(m_fx_radius);
            }
        }
    }

    void SkillEffectView::move_selection(int delta) {
        if (m_catalog.visible().empty()) {
            return;
        }
        const int last = static_cast<int>(m_catalog.visible_count()) - 1;
        const int next = std::clamp((m_selected < 0 ? 0 : m_selected) + delta, 0, last);
        select_visible(next);
    }

    void SkillEffectView::ensure_selection_visible() {
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

    void SkillEffectView::select_visible(int visible_index) {
        if (visible_index < 0 || visible_index >= static_cast<int>(m_catalog.visible_count())) {
            m_selected = -1;
            m_current = nullptr;
            clear_fx_preview();
            return;
        }
        m_selected = visible_index;
        if (m_catalog.mode() == BrowseMode::Skills) {
            const SkillRow& skill = m_catalog.visible_skill(visible_index);
            int slot = m_slot;
            if (skill.effect(slot).empty()) {
                slot = 0;
                for (int g = 0; g < 3; ++g) {
                    if (!skill.effect(g).empty()) {
                        slot = g;
                        break;
                    }
                }
            }
            m_slot = slot;
        }
        load_current_preview();
        ensure_selection_visible();
    }

    void SkillEffectView::load_current_preview() {
        m_anim_ms = 0.0f;
        m_current = nullptr;
        clear_fx_preview();
        if (m_selected < 0 || m_selected >= static_cast<int>(m_catalog.visible_count())) {
            return;
        }
        if (m_catalog.mode() == BrowseMode::Sprite || m_catalog.mode() == BrowseMode::Fx ||
            m_catalog.mode() == BrowseMode::Mobs) {
            (void) load_fx_preview();
            return;
        }
        if (m_catalog.mode() == BrowseMode::Models) {
            const AssetEntry& asset = m_catalog.visible_asset(m_selected);
            m_current = get_or_load(asset.absolute_path());
            if (!m_current) {
                m_status = "failed to load " + asset.folder() + "/" + asset.relative_path();
                return;
            }
            fit_camera(*m_current);
            const std::string used = join_used_by(asset.used_by());
            char line[384];
            std::snprintf(line,
                          sizeof(line),
                          "%s/%s  clip=%.0fms  tex=%d/%d%s%s",
                          asset.folder().c_str(),
                          asset.stem().c_str(),
                          m_current->anim().duration_ms(),
                          m_current->textures_ok(),
                          m_current->textures_ok() + m_current->textures_missing(),
                          used.empty() ? "" : "  used by: ",
                          used.c_str());
            m_status = line;
            return;
        }

        const SkillRow& skill = m_catalog.visible_skill(m_selected);
        const std::string& stem = skill.effect(m_slot);
        if (stem.empty()) {
            m_status = skill.list_label() + "  " + EffectCatalog::slot_name(m_slot) + " (empty)";
            return;
        }
        const AssetEntry* asset = m_catalog.find_asset(stem);
        if (!asset) {
            m_status = skill.list_label() + "  " + stem + "  MISSING .gb";
            return;
        }
        m_current = get_or_load(asset->absolute_path());
        if (!m_current) {
            m_status = "failed to load " + asset->absolute_path();
            return;
        }
        fit_camera(*m_current);
        char line[384];
        std::snprintf(line,
                      sizeof(line),
                      "%s  %s=%s  (%s/%s)  clip=%.0fms  tex=%d/%d",
                      skill.list_label().c_str(),
                      EffectCatalog::slot_name(m_slot),
                      stem.c_str(),
                      asset->folder().c_str(),
                      asset->relative_path().c_str(),
                      m_current->anim().duration_ms(),
                      m_current->textures_ok(),
                      m_current->textures_ok() + m_current->textures_missing());
        m_status = line;
    }

    void SkillEffectView::select_slot(int gubun) {
        const int next = std::clamp(gubun, 0, 2);
        if (next == m_slot && m_current) {
            return;
        }
        m_slot = next;
        if (m_catalog.mode() == BrowseMode::Skills) {
            load_current_preview();
        }
    }

    void SkillEffectView::set_mode(BrowseMode mode) {
        if (mode == m_catalog.mode()) {
            return;
        }
        m_catalog.set_mode(mode);
        m_scroll = 0;
        m_slot = 0;
        if (m_catalog.visible().empty()) {
            m_selected = -1;
            m_current = nullptr;
            clear_fx_preview();
            switch (mode) {
            case BrowseMode::Skills:
                m_status = "no named skill effects";
                break;
            case BrowseMode::Models:
                m_status = "no .gb files";
                break;
            case BrowseMode::Sprite:
                m_status = "no particle shaders in Effect/Sprite";
                break;
            case BrowseMode::Fx:
                m_status = "no FX shaders in Effect/Fx";
                break;
            case BrowseMode::Mobs:
                m_status = "no monster/NPC particle or fx attaches";
                break;
            }
            return;
        }
        select_visible(0);
    }

    void SkillEffectView::clear_fx_preview() {
        m_sprite_fx.detach(kPreviewEntity);
        m_fx_preview = false;
    }

    void SkillEffectView::fit_fx_camera(float radius) {
        m_fx_radius = std::max(8.0f, radius);
        m_target = engine::Vec3{0.0f, m_fx_radius * 0.35f, 0.0f};
        const float half_fov = kFovRad * 0.5f;
        m_fit_distance = m_fx_radius / std::max(0.12f, std::tan(half_fov)) * 1.6f;
        m_distance = m_fit_distance;
        m_yaw = 0.7f;
        m_pitch = -0.38f;
    }

    std::string SkillEffectView::join_used_by(const std::vector<std::string>& names) {
        std::string used;
        for (const auto& name : names) {
            if (!used.empty()) {
                used += ", ";
            }
            used += name;
        }
        return used;
    }

    float SkillEffectView::tab_x(const PanelLayout& layout, int index) const {
        return layout.m_pad + static_cast<float>(index) * (layout.m_tab_w + 6.0f * m_ui_scale);
    }

    bool SkillEffectView::load_fx_preview() {
        if (!m_engine) {
            return false;
        }
        const float origin_y = 16.0f;
        float radius = 24.0f;
        if (m_catalog.mode() == BrowseMode::Sprite) {
            const ScriptRow& row = m_catalog.visible_script(m_selected);
            if (!m_sprite_fx.has_sprite(row.name())) {
                m_status = row.name() + "  MISSING Effect/Sprite shader";
                return false;
            }
            m_sprite_fx.attach_particle(
                *m_engine, kPreviewEntity, row.name(), -1, 0.0f, 0.0f, 0.0f);
            radius = m_sprite_fx.sprite_radius(*m_engine, row.name());
            const std::string used = join_used_by(row.used_by());
            m_status = "sprite  " + row.name();
            if (!used.empty()) {
                m_status += "  used by: " + used;
            }
        } else if (m_catalog.mode() == BrowseMode::Fx) {
            const ScriptRow& row = m_catalog.visible_script(m_selected);
            if (!m_sprite_fx.has_fx(row.name())) {
                m_status = row.name() + "  MISSING Effect/Fx shader";
                return false;
            }
            m_sprite_fx.attach_fx(*m_engine, kPreviewEntity, row.name(), -1);
            radius = m_sprite_fx.fx_radius(*m_engine, row.name());
            const auto models = m_sprite_fx.fx_models(row.name());
            const auto particles = m_sprite_fx.fx_particles(row.name());
            if (!models.empty()) {
                if (const AssetEntry* asset = m_catalog.find_asset(models.front())) {
                    m_current = get_or_load(asset->absolute_path());
                }
            }
            std::string detail = "fx  " + row.name();
            if (!particles.empty()) {
                detail += "  particles=";
                for (std::size_t i = 0; i < particles.size(); ++i) {
                    if (i > 0) {
                        detail += ",";
                    }
                    detail += particles[i];
                }
            }
            if (!models.empty()) {
                detail += "  models=";
                for (std::size_t i = 0; i < models.size(); ++i) {
                    if (i > 0) {
                        detail += ",";
                    }
                    detail += models[i];
                }
            }
            const std::string used = join_used_by(row.used_by());
            if (!used.empty()) {
                detail += "  used by: " + used;
            }
            m_status = std::move(detail);
        } else {
            const MobRow& mob = m_catalog.visible_mob(m_selected);
            std::string missing;
            for (const auto& attach : mob.particles()) {
                if (!m_sprite_fx.has_sprite(attach.name())) {
                    if (!missing.empty()) {
                        missing += ", ";
                    }
                    missing += attach.name();
                    continue;
                }
                m_sprite_fx.attach_particle(*m_engine,
                                            kPreviewEntity,
                                            attach.name(),
                                            attach.effect_bone(),
                                            attach.ox(),
                                            attach.oy(),
                                            attach.oz());
                radius = std::max(radius, m_sprite_fx.sprite_radius(*m_engine, attach.name()));
            }
            for (const auto& attach : mob.fx()) {
                if (!m_sprite_fx.has_fx(attach.name())) {
                    if (!missing.empty()) {
                        missing += ", ";
                    }
                    missing += attach.name();
                    continue;
                }
                m_sprite_fx.attach_fx(
                    *m_engine, kPreviewEntity, attach.name(), attach.effect_bone());
                radius = std::max(radius, m_sprite_fx.fx_radius(*m_engine, attach.name()));
            }
            std::string detail = mob.list_label();
            for (const auto& attach : mob.particles()) {
                detail += "  p=";
                detail += attach.name();
            }
            for (const auto& attach : mob.fx()) {
                detail += "  fx=";
                detail += attach.name();
            }
            if (!missing.empty()) {
                detail += "  MISSING: " + missing;
            }
            m_status = std::move(detail);
        }

        m_fx_preview = m_sprite_fx.has(kPreviewEntity) || m_current != nullptr;
        m_sprite_fx.follow_all(kPreviewEntity, 0.0f, origin_y, 0.0f);
        if (m_current) {
            fit_camera(*m_current);
        } else {
            fit_fx_camera(radius);
        }
        if (!m_fx_preview) {
            if (m_status.find("MISSING") == std::string::npos) {
                m_status += "  (no live emitters)";
            }
            return false;
        }
        return true;
    }

    void SkillEffectView::skin_preview(GpuPreview& preview, float time_ms) {
        if (preview.parts().empty()) {
            return;
        }
        std::vector<std::array<float, 16>> skin_mats;
        const auto& clip = preview.anim();
        if (clip.keyframe_count > 0 && !preview.bones().empty()) {
            if (!terrain::eval_skin_matrices_at_time(
                    preview.bones(), clip, time_ms, true, skin_mats)) {
                return;
            }
        }
        for (std::size_t i = 0; i < preview.parts().size(); ++i) {
            terrain::GbMeshCpu work = preview.bind_meshes()[i];
            if (!skin_mats.empty()) {
                terrain::skin_mesh(work, skin_mats);
            }
            const auto verts = to_engine_verts(work);
            if (preview.parts()[i].mesh() &&
                verts.size() == preview.bind_meshes()[i].pos_uv.size() / 5) {
                m_engine->update_mesh(preview.parts()[i].mesh(), verts.data(), verts.size());
            }
        }
    }

    void
    SkillEffectView::draw_preview(const engine::FrameInput& in, float panel_w, float time_sec) {
        const float win_w = static_cast<float>(in.width);
        const float win_h = static_cast<float>(in.height);
        const float pane_x = panel_w;
        const float pane_w = std::max(1.0f, win_w - panel_w);
        const float znear = std::max(0.25f, m_distance * 0.01f);
        const float zfar =
            m_distance * 40.0f + (m_current ? m_current->radius() * 8.0f : m_fx_radius * 8.0f);
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

        if (m_fx_preview) {
            m_sprite_fx.follow_all(kPreviewEntity, 0.0f, 16.0f, 0.0f);
            m_sprite_fx.update(in.dt, eye.x, eye.y, eye.z);
        }

        if ((m_current || m_fx_preview) && m_ground_mesh && m_ground_tex) {
            const float ground_s =
                std::max(40.0f, m_current ? m_current->radius() * 3.0f : m_fx_radius * 3.0f);
            const float gx = m_current ? m_current->center_x() : 0.0f;
            const float gy = m_current ? m_current->min_y() : 0.0f;
            const float gz = m_current ? m_current->center_z() : 0.0f;
            const auto ground =
                engine::Mat4::multiply(engine::Mat4::translation(gx, gy, gz),
                                       engine::Mat4::scaling(ground_s, 1.0f, ground_s));
            m_engine->draw_mesh_textured(m_ground_mesh, m_ground_tex, ground, false, true);
        }

        if (m_current) {
            engine::Mat4 model = engine::Mat4::identity();
            if (m_current->billboard()) {
                const float hy = m_yaw * 0.5f;
                model = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
            }
            for (const auto& part : m_current->parts()) {
                if (!part.mesh() || !part.texture()) {
                    continue;
                }
                float uv_u = 0.0f;
                float uv_v = 0.0f;
                float opacity = 1.0f;
                if (!terrain::sample_material_uv(
                        part.uv_material(), part.uv_key_times(), m_anim_ms, uv_u, uv_v, opacity)) {
                    uv_u = 0.0f;
                    uv_v = 0.0f;
                }
                (void) time_sec;
                const bool blend = part.alpha_blend() || opacity < 0.999f;
                m_engine->draw_mesh_textured(part.mesh(),
                                             part.texture(),
                                             model,
                                             true,
                                             true,
                                             part.alpha_test() && !blend,
                                             true,
                                             uv_u,
                                             uv_v,
                                             1.0f,
                                             1.0f,
                                             1.0f,
                                             opacity);
            }
        }

        if (m_fx_preview) {
            m_sprite_fx.draw(*m_engine);
        }
    }

    void SkillEffectView::draw_ui(const engine::FrameInput& in, const PanelLayout& layout) {
        const float s = m_ui_scale;
        const float win_h = static_cast<float>(in.height);
        const float pad = layout.m_pad;
        const float panel_w = layout.m_panel_w;
        m_visible_rows = layout.m_visible_rows;
        ensure_selection_visible();

        m_engine->draw_ui_rect(0.0f, 0.0f, panel_w, win_h, 0.07f, 0.08f, 0.10f, 0.96f);
        m_engine->draw_ui_rect(
            panel_w - 2.0f * s, 0.0f, 2.0f * s, win_h, 0.20f, 0.22f, 0.26f, 1.0f);

        draw_text_fit(pad, pad * 0.55f, panel_w - pad * 2.0f, "EFFECTS", 0.95f, 0.92f, 0.75f);
        char count_line[64];
        std::snprintf(count_line,
                      sizeof(count_line),
                      "%d / %d",
                      static_cast<int>(m_catalog.visible_count()),
                      m_catalog.mode_total());
        if (m_font.ready()) {
            const float title_w = m_font.text_width("EFFECTS  ", m_font_scale);
            m_font.draw(*m_engine,
                        pad + title_w,
                        pad * 0.55f,
                        count_line,
                        0.65f,
                        0.68f,
                        0.72f,
                        m_font_scale);
        }

        for (int t = 0; t < kTabCount; ++t) {
            const bool on = static_cast<int>(m_catalog.mode()) == t;
            m_engine->draw_ui_rect(tab_x(layout, t),
                                   layout.m_tabs_y,
                                   layout.m_tab_w,
                                   layout.m_tab_h,
                                   on ? 0.28f : 0.12f,
                                   on ? 0.34f : 0.14f,
                                   on ? 0.42f : 0.18f,
                                   1.0f);
            if (m_font.ready()) {
                m_font.draw_centered(*m_engine,
                                     tab_x(layout, t) + layout.m_tab_w * 0.5f,
                                     layout.m_tabs_y + 6.0f * s,
                                     EffectCatalog::mode_name(static_cast<BrowseMode>(t)),
                                     0.95f,
                                     0.96f,
                                     0.98f,
                                     m_font_scale * 0.92f);
            }
        }

        m_engine->draw_ui_rect(pad,
                               layout.m_filter_y,
                               panel_w - pad * 2.0f,
                               layout.m_filter_h - 6.0f * s,
                               0.04f,
                               0.045f,
                               0.055f,
                               1.0f);
        std::string filter_text = m_catalog.filter();
        if (filter_text.empty()) {
            draw_text_fit(pad + 8.0f * s,
                          layout.m_filter_y + 6.0f * s,
                          panel_w - pad * 2.0f - 16.0f * s,
                          "type to filter",
                          0.40f,
                          0.42f,
                          0.46f);
        } else {
            filter_text.push_back('_');
            draw_text_fit(pad + 8.0f * s,
                          layout.m_filter_y + 6.0f * s,
                          panel_w - pad * 2.0f - 16.0f * s,
                          filter_text,
                          0.92f,
                          0.94f,
                          0.96f);
        }

        if (m_catalog.mode() == BrowseMode::Skills) {
            const SkillRow* skill = nullptr;
            if (m_selected >= 0 && m_selected < static_cast<int>(m_catalog.visible_count())) {
                skill = &m_catalog.visible_skill(m_selected);
            }
            for (int g = 0; g < 3; ++g) {
                const float cx = pad + static_cast<float>(g) * (layout.m_chip_w + 8.0f * s);
                const bool on = g == m_slot;
                const std::string stem = skill ? skill->effect(g) : std::string{};
                const bool missing = !stem.empty() && m_catalog.find_asset(stem) == nullptr;
                const float r = missing ? 0.45f : (on ? 0.30f : 0.14f);
                const float gg = missing ? 0.16f : (on ? 0.38f : 0.16f);
                const float b = missing ? 0.16f : (on ? 0.28f : 0.20f);
                m_engine->draw_ui_rect(
                    cx, layout.m_chips_y, layout.m_chip_w, layout.m_chip_h, r, gg, b, 1.0f);
                std::string label = EffectCatalog::slot_name(g);
                label += ' ';
                label += stem.empty() ? "-" : stem;
                draw_text_fit(cx + 6.0f * s,
                              layout.m_chips_y + 6.0f * s,
                              layout.m_chip_w - 12.0f * s,
                              label,
                              0.95f,
                              0.96f,
                              0.98f);
            }
        }

        const int total = static_cast<int>(m_catalog.visible_count());
        for (int row = 0; row < layout.m_visible_rows; ++row) {
            const int index = m_scroll + row;
            if (index >= total) {
                break;
            }
            const float y = layout.m_list_top + static_cast<float>(row) * layout.m_row_h;
            std::string label;
            switch (m_catalog.mode()) {
            case BrowseMode::Skills:
                label = m_catalog.visible_skill(index).list_label();
                break;
            case BrowseMode::Models: {
                const AssetEntry& asset = m_catalog.visible_asset(index);
                label = asset.folder() + "/" + asset.stem();
                if (!asset.used_by().empty()) {
                    label += "  (" + asset.used_by().front() + ")";
                }
                break;
            }
            case BrowseMode::Sprite:
            case BrowseMode::Fx:
                label = m_catalog.visible_script(index).list_label();
                break;
            case BrowseMode::Mobs:
                label = m_catalog.visible_mob(index).list_label();
                break;
            }
            draw_row(
                0.0f, y, panel_w, layout.m_row_h, label, index == m_selected, index == m_hover);
        }

        if (total > layout.m_visible_rows) {
            const float track_x = panel_w - 10.0f * s;
            m_engine->draw_ui_rect(
                track_x, layout.m_list_top, 6.0f * s, layout.m_list_h, 0.12f, 0.13f, 0.15f, 1.0f);
            const float thumb_h =
                std::max(16.0f * s,
                         layout.m_list_h * static_cast<float>(layout.m_visible_rows) /
                             static_cast<float>(total));
            const float max_scroll = static_cast<float>(total - layout.m_visible_rows);
            const float thumb_y =
                layout.m_list_top +
                (layout.m_list_h - thumb_h) *
                    (max_scroll > 0.0f ? static_cast<float>(m_scroll) / max_scroll : 0.0f);
            m_engine->draw_ui_rect(track_x, thumb_y, 6.0f * s, thumb_h, 0.55f, 0.58f, 0.62f, 1.0f);
        }

        m_engine->draw_ui_rect(
            0.0f, layout.m_status_y, panel_w, layout.m_status_h, 0.05f, 0.055f, 0.07f, 1.0f);
        if (!m_status.empty()) {
            draw_text_fit(pad,
                          layout.m_status_y + 10.0f * s,
                          panel_w - pad * 2.0f,
                          m_status,
                          0.80f,
                          0.82f,
                          0.86f);
        }
        draw_text_fit(pad,
                      layout.m_status_y + 48.0f * s,
                      panel_w - pad * 2.0f,
                      "LMB orbit  RMB pan  wheel zoom  R reset  Space replay",
                      0.50f,
                      0.52f,
                      0.56f);
        draw_text_fit(pad,
                      layout.m_status_y + 78.0f * s,
                      panel_w - pad * 2.0f,
                      "click tabs  type filter  Up/Down select  Esc quit",
                      0.50f,
                      0.52f,
                      0.56f);

        if (!m_current && !m_fx_preview && m_catalog.mode() == BrowseMode::Skills &&
            m_catalog.skills().empty()) {
            draw_text_fit(panel_w + 32.0f * s,
                          win_h * 0.45f,
                          win_h,
                          "No JSON effect names — open Models / Sprite / Fx",
                          0.85f,
                          0.7f,
                          0.5f);
        }
    }

    void SkillEffectView::draw_row(
        float x, float y, float w, float h, const std::string& label, bool selected, bool hovered) {
        const float s = m_ui_scale;
        if (selected) {
            m_engine->draw_ui_rect(x, y, w, h, 0.22f, 0.32f, 0.42f, 0.95f);
        } else if (hovered) {
            m_engine->draw_ui_rect(x, y, w, h, 0.14f, 0.18f, 0.24f, 0.90f);
        }
        const float line_h = m_font.ready() ? m_font.line_height() * m_font_scale : 18.0f * s;
        const float text_y = y + std::max(2.0f, (h - line_h) * 0.5f);
        draw_text_fit(x + 14.0f * s, text_y, w - 28.0f * s, label, 0.95f, 0.96f, 0.98f);
    }

    void SkillEffectView::draw_text_fit(
        float x, float y, float max_w, const std::string& text, float r, float g, float b) {
        if (!m_font.ready() || text.empty()) {
            return;
        }
        if (m_font.text_width(text, m_font_scale) <= max_w) {
            m_font.draw(*m_engine, x, y, text, r, g, b, m_font_scale);
            return;
        }
        std::string clipped = text;
        const std::string ellipsis = "...";
        while (clipped.size() > 1 && m_font.text_width(clipped + ellipsis, m_font_scale) > max_w) {
            clipped.pop_back();
        }
        m_font.draw(*m_engine, x, y, clipped + ellipsis, r, g, b, m_font_scale);
    }

    engine::Mat4 SkillEffectView::pane_perspective(float fovy_rad,
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

}  // namespace openkal::skill_effect_view
