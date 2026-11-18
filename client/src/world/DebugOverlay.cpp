/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DebugOverlay.h"

#include "openkal/client/Floor.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/TextWrap.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"
#include "openkal/map/Coords.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace openkal::client {

    engine::MeshHandle DebugOverlay::m_fill_mesh{};
    engine::MeshHandle DebugOverlay::m_edge_mesh{};
    engine::TextureHandle DebugOverlay::m_fill_tex{};
    bool DebugOverlay::m_enabled = false;
    bool DebugOverlay::m_env_applied = false;
    FreetypeFont DebugOverlay::m_text_font;
    int DebugOverlay::m_text_pixel_size = 0;
    FreetypeFont DebugOverlay::m_fps_font;
    int DebugOverlay::m_fps_pixel_size = 0;

    namespace {

        constexpr float kFillR = 1.0f;
        constexpr float kFillG = 1.0f;
        constexpr float kFillB = 0.0f;
        constexpr float kFillA = 0.10f;

        constexpr float kEdgeR = 0.55f;
        constexpr float kEdgeG = 0.45f;
        constexpr float kEdgeB = 0.0f;

        /** Normalized edge beam half-thickness in unit-AABB space. */
        constexpr float kEdgeHalf = 0.03f;

        void append_box(std::vector<engine::VertexP3T2>& verts,
                        std::vector<std::uint32_t>& indices,
                        float x0,
                        float y0,
                        float z0,
                        float x1,
                        float y1,
                        float z1) {
            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const engine::VertexP3T2 corners[8] = {
                {x0, y0, z0, 0.0f, 0.0f},
                {x1, y0, z0, 1.0f, 0.0f},
                {x1, y1, z0, 1.0f, 1.0f},
                {x0, y1, z0, 0.0f, 1.0f},
                {x0, y0, z1, 0.0f, 0.0f},
                {x1, y0, z1, 1.0f, 0.0f},
                {x1, y1, z1, 1.0f, 1.0f},
                {x0, y1, z1, 0.0f, 1.0f},
            };
            for (const auto& c : corners) {
                verts.push_back(c);
            }
            const std::uint32_t faces[6][4] = {
                {0, 1, 2, 3},
                {5, 4, 7, 6},
                {4, 0, 3, 7},
                {1, 5, 6, 2},
                {3, 2, 6, 7},
                {4, 5, 1, 0},
            };
            for (const auto& f : faces) {
                indices.push_back(base + f[0]);
                indices.push_back(base + f[1]);
                indices.push_back(base + f[2]);
                indices.push_back(base + f[0]);
                indices.push_back(base + f[2]);
                indices.push_back(base + f[3]);
            }
        }

        void append_edge_beam(std::vector<engine::VertexP3T2>& verts,
                              std::vector<std::uint32_t>& indices,
                              float ax,
                              float ay,
                              float az,
                              float bx,
                              float by,
                              float bz) {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            // Pick a stable perpendicular for the beam cross-section.
            float px = 0.0f;
            float py = 1.0f;
            float pz = 0.0f;
            if (dy * dy > 0.9f * (dx * dx + dy * dy + dz * dz)) {
                px = 1.0f;
                py = 0.0f;
            }
            // n = normalize(cross(d, p)), m = normalize(cross(d, n))
            float nx = dy * pz - dz * py;
            float ny = dz * px - dx * pz;
            float nz = dx * py - dy * px;
            float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nlen < 1e-6f) {
                return;
            }
            nx /= nlen;
            ny /= nlen;
            nz /= nlen;
            float mx = dy * nz - dz * ny;
            float my = dz * nx - dx * nz;
            float mz = dx * ny - dy * nx;
            const float mlen = std::sqrt(mx * mx + my * my + mz * mz);
            if (mlen < 1e-6f) {
                return;
            }
            mx = mx / mlen * kEdgeHalf;
            my = my / mlen * kEdgeHalf;
            mz = mz / mlen * kEdgeHalf;
            nx *= kEdgeHalf;
            ny *= kEdgeHalf;
            nz *= kEdgeHalf;

            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const engine::VertexP3T2 corners[8] = {
                {ax - nx - mx, ay - ny - my, az - nz - mz, 0, 0},
                {ax + nx - mx, ay + ny - my, az + nz - mz, 1, 0},
                {ax + nx + mx, ay + ny + my, az + nz + mz, 1, 1},
                {ax - nx + mx, ay - ny + my, az - nz + mz, 0, 1},
                {bx - nx - mx, by - ny - my, bz - nz - mz, 0, 0},
                {bx + nx - mx, by + ny - my, bz + nz - mz, 1, 0},
                {bx + nx + mx, by + ny + my, bz + nz + mz, 1, 1},
                {bx - nx + mx, by - ny + my, bz - nz + mz, 0, 1},
            };
            for (const auto& c : corners) {
                verts.push_back(c);
            }
            const std::uint32_t faces[6][4] = {
                {0, 1, 2, 3},
                {5, 4, 7, 6},
                {4, 0, 3, 7},
                {1, 5, 6, 2},
                {3, 2, 6, 7},
                {4, 5, 1, 0},
            };
            for (const auto& f : faces) {
                indices.push_back(base + f[0]);
                indices.push_back(base + f[1]);
                indices.push_back(base + f[2]);
                indices.push_back(base + f[0]);
                indices.push_back(base + f[2]);
                indices.push_back(base + f[3]);
            }
        }

    }  // namespace

    bool DebugOverlay::enabled() {
        if (!m_env_applied) {
            const char* e = std::getenv("DEBUG");
            m_enabled = e != nullptr && e[0] != '\0' && e[0] != '0';
            m_env_applied = true;
        }
        return m_enabled;
    }

    void DebugOverlay::set_enabled(bool on) {
        m_env_applied = true;
        m_enabled = on;
    }

    std::string DebugOverlay::patch_name(int patch_x, int patch_y) {
        char name[32];
        std::snprintf(name, sizeof(name), "n_%03d_%03d", patch_x, patch_y);
        return name;
    }

    bool DebugOverlay::init(engine::IEngine& engine) {
        destroy(engine);

        std::vector<engine::VertexP3T2> fill_verts;
        std::vector<std::uint32_t> fill_idx;
        append_box(fill_verts, fill_idx, -1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f);
        m_fill_mesh = engine.create_mesh(
            fill_verts.data(), fill_verts.size(), fill_idx.data(), fill_idx.size());

        std::vector<engine::VertexP3T2> edge_verts;
        std::vector<std::uint32_t> edge_idx;
        // Bottom ring.
        append_edge_beam(edge_verts, edge_idx, -1, 0, -1, 1, 0, -1);
        append_edge_beam(edge_verts, edge_idx, 1, 0, -1, 1, 0, 1);
        append_edge_beam(edge_verts, edge_idx, 1, 0, 1, -1, 0, 1);
        append_edge_beam(edge_verts, edge_idx, -1, 0, 1, -1, 0, -1);
        // Top ring.
        append_edge_beam(edge_verts, edge_idx, -1, 1, -1, 1, 1, -1);
        append_edge_beam(edge_verts, edge_idx, 1, 1, -1, 1, 1, 1);
        append_edge_beam(edge_verts, edge_idx, 1, 1, 1, -1, 1, 1);
        append_edge_beam(edge_verts, edge_idx, -1, 1, 1, -1, 1, -1);
        // Verticals.
        append_edge_beam(edge_verts, edge_idx, -1, 0, -1, -1, 1, -1);
        append_edge_beam(edge_verts, edge_idx, 1, 0, -1, 1, 1, -1);
        append_edge_beam(edge_verts, edge_idx, 1, 0, 1, 1, 1, 1);
        append_edge_beam(edge_verts, edge_idx, -1, 0, 1, -1, 1, 1);
        m_edge_mesh = engine.create_mesh(
            edge_verts.data(), edge_verts.size(), edge_idx.data(), edge_idx.size());

        const std::uint8_t px[4] = {255, 255, 0, static_cast<std::uint8_t>(kFillA * 255.0f)};
        m_fill_tex = engine.create_texture_rgba8(px, 1, 1, true);
        return static_cast<bool>(m_fill_mesh) && static_cast<bool>(m_edge_mesh) &&
               static_cast<bool>(m_fill_tex);
    }

    void DebugOverlay::destroy(engine::IEngine& engine) {
        m_fps_font.destroy(engine);
        m_fps_pixel_size = 0;
        m_text_font.destroy(engine);
        m_text_pixel_size = 0;
        if (m_fill_mesh) {
            engine.destroy_mesh(m_fill_mesh);
            m_fill_mesh = {};
        }
        if (m_edge_mesh) {
            engine.destroy_mesh(m_edge_mesh);
            m_edge_mesh = {};
        }
        if (m_fill_tex) {
            engine.destroy_texture(m_fill_tex);
            m_fill_tex = {};
        }
    }

    void DebugOverlay::draw_text(engine::IEngine& engine,
                                 const std::vector<std::string>& lines,
                                 int width,
                                 int height) {
        if (lines.empty() || width <= 0 || height <= 0) {
            return;
        }
        const float scale = std::clamp(static_cast<float>(height) / 1080.0f, 0.82f, 2.0f);
        const int pixel_size = static_cast<int>(std::lround(22.0f * scale));
        if (m_text_pixel_size != pixel_size) {
            m_text_pixel_size = pixel_size;
            auto path = FreetypeFont::find_system_sans_bold();
            if (path.empty()) {
                path = FreetypeFont::find_system_sans();
            }
            (void) m_text_font.load(engine, path, pixel_size);
        }
        if (!m_text_font.ready()) {
            return;
        }
        const float margin = 16.0f * scale;
        const float padding = 10.0f * scale;
        const float max_width = std::min(900.0f * scale, width - 2 * (margin + padding));
        if (max_width <= 0) {
            return;
        }
        std::vector<std::string> wrapped;
        float text_width = 0;
        for (const auto& line : lines) {
            for (auto& part : wrap_text(m_text_font, line, max_width, 1.0f)) {
                text_width = std::max(text_width, m_text_font.text_width(part));
                wrapped.push_back(std::move(part));
            }
        }
        const float line_height = m_text_font.line_height() + 4.0f * scale;
        const float panel_width = text_width + 2 * padding;
        const float panel_height = static_cast<float>(wrapped.size()) * line_height + 2 * padding;
        const float x = static_cast<float>(width) - margin - panel_width;
        engine.draw_ui_rect(x, margin, panel_width, panel_height, 0.025f, 0.035f, 0.05f, 0.88f);
        float y = margin + padding;
        for (const auto& line : wrapped) {
            m_text_font.draw(engine, x + padding, y, line, 0.97f, 0.97f, 0.82f);
            y += line_height;
        }
    }

    void DebugOverlay::draw_fps(engine::IEngine& engine, float fps, int width, int height) {
        if (width <= 0 || height <= 0) {
            return;
        }
        const float scale = std::clamp(static_cast<float>(height) / 1080.0f, 0.82f, 2.0f);
        const int pixel_size = static_cast<int>(std::lround(8.0f * scale));
        if (m_fps_pixel_size != pixel_size) {
            m_fps_pixel_size = pixel_size;
            (void) m_fps_font.load(engine, FreetypeFont::find_system_sans(), pixel_size);
        }
        if (!m_fps_font.ready()) {
            return;
        }
        std::array<char, 32> text{};
        std::snprintf(text.data(),
                      text.size(),
                      "FPS: %.0f",
                      std::isfinite(fps) ? static_cast<double>(std::max(0.0f, fps)) : 0.0);
        m_fps_font.draw(engine, 5.0f, 5.0f, text.data(), 0.97f, 0.97f, 0.82f);
    }

    void DebugOverlay::draw_monster_hitbox(engine::IEngine& engine,
                                           float world_x,
                                           float base_y,
                                           float world_z,
                                           float half_extent,
                                           float height) {
        if (!m_fill_mesh || !m_edge_mesh || !m_fill_tex || half_extent < 0.01f || height < 0.01f) {
            return;
        }
        const auto S = engine::Mat4::scaling(half_extent, height, half_extent);
        const auto T = engine::Mat4::translation(world_x, base_y, world_z);
        const auto model = engine::Mat4::multiply(T, S);
        engine.draw_mesh_textured(m_fill_mesh,
                                  m_fill_tex,
                                  model,
                                  /*alpha_blend=*/true,
                                  /*two_sided=*/true,
                                  /*alpha_test=*/false,
                                  /*additive=*/false,
                                  0.0f,
                                  0.0f,
                                  kFillR,
                                  kFillG,
                                  kFillB,
                                  1.0f);
        engine.draw_mesh_lit(m_edge_mesh, model, kEdgeR, kEdgeG, kEdgeB);
    }

    namespace {

        constexpr float kOverlayAlpha = 0.28f;
        constexpr float kOverlayLift = 0.18f;
        constexpr int kAreaOverlayRadiusCells = 1;

        struct OverlayColor {
            float r;
            float g;
            float b;
        };

        constexpr std::array<OverlayColor, static_cast<int>(WorldDebugOverlay::AreaLayer::Count)>
            kAreaColors = {{
                {1.0f, 0.15f, 0.15f},  // Blocked
                {1.0f, 0.45f, 0.10f},  // NotMoveable
                {0.2f, 0.85f, 0.3f},   // Village
                {0.25f, 0.55f, 1.0f},  // Safety
                {0.95f, 0.85f, 0.2f},  // Portal
                {0.7f, 0.35f, 0.95f},  // Object
                {1.0f, 0.3f, 0.55f},   // AtkCastle
                {0.4f, 0.9f, 0.85f},   // DefCastle
            }};

        // Cyan is intentionally unused by the area editor layers.
        constexpr OverlayColor kSpawnAreaColor{0.1f, 0.9f, 0.95f};

        [[nodiscard]] std::string trim(std::string_view text) {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
                text.remove_suffix(1);
            }
            return std::string(text);
        }

        [[nodiscard]] std::string lower_ascii(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            return out;
        }

        [[nodiscard]] bool parse_positive_int(std::string_view text, int& out) {
            const std::string cleaned = trim(text);
            if (cleaned.empty()) {
                return false;
            }
            const char* first = cleaned.data();
            const char* last = first + cleaned.size();
            const auto [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc() && ptr == last && out > 0;
        }

        [[nodiscard]] std::uint32_t area_mask(WorldDebugOverlay::AreaLayer layer) {
            using Layer = WorldDebugOverlay::AreaLayer;
            switch (layer) {
            case Layer::NotMoveable:
                return map::kAttrNotMoveable;
            case Layer::Village:
                return map::kAttrVillage;
            case Layer::Safety:
                return map::kAttrSafety;
            case Layer::Portal:
                return map::kAttrPortal;
            case Layer::Object:
                return map::kAttrObject;
            case Layer::AtkCastle:
                return map::kAttrAttackCastle;
            case Layer::DefCastle:
                return map::kAttrDefenceCastle;
            case Layer::Blocked:
            case Layer::Count:
                return 0;
            }
            return 0;
        }

        void append_ground_quad(std::vector<engine::VertexP3T2>& vertices,
                                std::vector<std::uint32_t>& indices,
                                const FloorField& floor,
                                int origin_patch,
                                bool is_indoor,
                                float player_floor_y,
                                int server_x0,
                                int server_y0,
                                int server_x1,
                                int server_y1) {
            const float x0 = coords::to_client(server_x0, origin_patch);
            const float z0 = coords::to_client(server_y0, origin_patch);
            const float x1 = coords::to_client(server_x1, origin_patch);
            const float z1 = coords::to_client(server_y1, origin_patch);
            const auto ground_height = [&](float x, float z) {
                // A dungeon area tile can straddle a corridor edge. Points outside
                // the walkable deck have no floor sample; retaining the player's
                // current deck height keeps the translucent surface visible.
                return is_indoor ? floor.sample(x, z, player_floor_y)
                                 : floor.sample_terrain(x, z, 0.0f);
            };
            const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
            vertices.push_back({x0, ground_height(x0, z0) + kOverlayLift, z0, 0.0f, 0.0f});
            vertices.push_back({x1, ground_height(x1, z0) + kOverlayLift, z0, 1.0f, 0.0f});
            vertices.push_back({x1, ground_height(x1, z1) + kOverlayLift, z1, 1.0f, 1.0f});
            vertices.push_back({x0, ground_height(x0, z1) + kOverlayLift, z1, 0.0f, 1.0f});
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        [[nodiscard]] bool has_visible_layer(
            const std::array<bool, static_cast<int>(WorldDebugOverlay::AreaLayer::Count)>& layers) {
            return std::any_of(layers.begin(), layers.end(), [](bool visible) { return visible; });
        }

    }  // namespace

    const char* WorldDebugOverlay::layer_name(AreaLayer layer) {
        static constexpr std::array<const char*, static_cast<int>(AreaLayer::Count)> kNames = {
            "blocked",
            "notmovable",
            "village",
            "safety",
            "portal",
            "object",
            "atkcastle",
            "defcastle"};
        const int index = static_cast<int>(layer);
        return index >= 0 && index < static_cast<int>(kNames.size()) ? kNames[index] : "";
    }

    bool WorldDebugOverlay::layer_from_name(std::string_view name, AreaLayer& out) {
        const std::string lower = lower_ascii(trim(name));
        for (int index = 0; index < static_cast<int>(AreaLayer::Count); ++index) {
            const AreaLayer layer = static_cast<AreaLayer>(index);
            if (lower == layer_name(layer)) {
                out = layer;
                return true;
            }
        }
        return false;
    }

    std::string WorldDebugOverlay::usage() {
        return "Usage: /show|/hide area <id> | /show|/hide "
               "<blocked|notmovable|village|safety|portal|object|atkcastle|defcastle>";
    }

    bool WorldDebugOverlay::load_spawn_areas(std::string_view directory) {
        m_spawn_areas.clear();
        if (!platform::is_directory(std::string(directory))) {
            return false;
        }
        for (const std::string& name : platform::list_directory(std::string(directory))) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0 ||
                name == "bones.json" || name == "manifest.json") {
                continue;
            }
            const auto bytes =
                platform::read_file(platform::join_path(std::string(directory), name));
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            const platform::Json& spawns = root.at("spawns");
            if (!spawns.is_array()) {
                continue;
            }
            for (const auto& spawn : spawns.array()) {
                const platform::Json& rect = spawn.at("rect");
                if (!spawn.is_object() || !rect.is_array() || rect.array().size() < 4) {
                    continue;
                }
                SpawnArea area;
                area.m_area = spawn.at("area").as_int(0);
                area.m_map = spawn.at("map").as_int(0);
                area.m_x0 = rect.array()[0].as_int(0) * map::kTileSize;
                area.m_y0 = rect.array()[1].as_int(0) * map::kTileSize;
                area.m_x1 = rect.array()[2].as_int(0) * map::kTileSize;
                area.m_y1 = rect.array()[3].as_int(0) * map::kTileSize;
                if (area.m_area <= 0) {
                    continue;
                }
                if (area.m_x1 < area.m_x0) {
                    std::swap(area.m_x0, area.m_x1);
                }
                if (area.m_y1 < area.m_y0) {
                    std::swap(area.m_y0, area.m_y1);
                }
                m_spawn_areas.push_back(area);
            }
        }
        return !m_spawn_areas.empty();
    }

    void WorldDebugOverlay::mark_dirty() {
        m_dirty = true;
    }

    std::string WorldDebugOverlay::show(std::string_view args) {
        const std::string command = lower_ascii(trim(args));
        constexpr std::string_view kAreaPrefix = "area";
        if (command.compare(0, kAreaPrefix.size(), kAreaPrefix) == 0 &&
            command.size() > kAreaPrefix.size() && command[kAreaPrefix.size()] == ' ') {
            int area = 0;
            if (!parse_positive_int(std::string_view(command).substr(kAreaPrefix.size()), area)) {
                return usage();
            }
            const int matches = static_cast<int>(std::count_if(
                m_spawn_areas.begin(), m_spawn_areas.end(), [area](const SpawnArea& spawn) {
                    return spawn.m_area == area;
                }));
            if (matches == 0) {
                return "Monster spawn area #" + std::to_string(area) + " was not found.";
            }
            m_visible_areas.insert(area);
            mark_dirty();
            return "Showing " + std::to_string(matches) + " monster spawn rectangle(s) for area #" +
                   std::to_string(area) + ".";
        }
        AreaLayer layer = AreaLayer::Count;
        if (!layer_from_name(command, layer)) {
            return usage();
        }
        m_visible_layers[static_cast<int>(layer)] = true;
        mark_dirty();
        return "Showing area " + std::string(layer_name(layer)) + " zones.";
    }

    std::string WorldDebugOverlay::hide(std::string_view args) {
        const std::string command = lower_ascii(trim(args));
        constexpr std::string_view kAreaPrefix = "area";
        if (command.compare(0, kAreaPrefix.size(), kAreaPrefix) == 0 &&
            command.size() > kAreaPrefix.size() && command[kAreaPrefix.size()] == ' ') {
            int area = 0;
            if (!parse_positive_int(std::string_view(command).substr(kAreaPrefix.size()), area)) {
                return usage();
            }
            if (m_visible_areas.erase(area) == 0) {
                return "Monster spawn area #" + std::to_string(area) + " is not visible.";
            }
            mark_dirty();
            return "Hid monster spawn area #" + std::to_string(area) + ".";
        }
        AreaLayer layer = AreaLayer::Count;
        if (!layer_from_name(command, layer)) {
            return usage();
        }
        m_visible_layers[static_cast<int>(layer)] = false;
        mark_dirty();
        return "Hid area " + std::string(layer_name(layer)) + " zones.";
    }

    void WorldDebugOverlay::clear_meshes(engine::IEngine& engine) {
        for (auto& mesh : m_layer_meshes) {
            if (mesh) {
                engine.destroy_mesh(mesh);
                mesh = {};
            }
        }
        if (m_spawn_mesh) {
            engine.destroy_mesh(m_spawn_mesh);
            m_spawn_mesh = {};
        }
    }

    void WorldDebugOverlay::update(engine::IEngine& engine,
                                   const FloorField& floor,
                                   const map::WorldGrid& grid,
                                   int origin_patch,
                                   int server_x,
                                   int server_y,
                                   int map_index,
                                   bool is_indoor,
                                   float player_floor_y) {
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(server_x, server_y, tile_x, tile_y);
        int cell_x = 0;
        int cell_y = 0;
        int local_x = 0;
        int local_y = 0;
        coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_x, local_y);
        const bool indoor_floor_changed =
            is_indoor && std::fabs(player_floor_y - m_player_floor_y) > FloorField::kStepHeight;
        if (!m_dirty && cell_x == m_center_cell_x && cell_y == m_center_cell_y &&
            map_index == m_map_index && is_indoor == m_is_indoor && !indoor_floor_changed) {
            return;
        }
        clear_meshes(engine);
        m_center_cell_x = cell_x;
        m_center_cell_y = cell_y;
        m_map_index = map_index;
        m_is_indoor = is_indoor;
        m_player_floor_y = player_floor_y;
        m_dirty = false;

        if (m_visible_areas.empty() && !has_visible_layer(m_visible_layers)) {
            return;
        }
        if (!m_fill_texture) {
            const std::uint8_t pixel[4] = {
                255, 255, 255, static_cast<std::uint8_t>(kOverlayAlpha * 255.0f)};
            m_fill_texture = engine.create_texture_rgba8(pixel, 1, 1, false);
        }

        for (int layer_index = 0; layer_index < static_cast<int>(AreaLayer::Count); ++layer_index) {
            if (!m_visible_layers[layer_index]) {
                continue;
            }
            const AreaLayer layer = static_cast<AreaLayer>(layer_index);
            std::vector<engine::VertexP3T2> vertices;
            std::vector<std::uint32_t> indices;
            const std::uint32_t mask = area_mask(layer);
            const auto add_quad = [&](int x, int y, int size) {
                append_ground_quad(vertices,
                                   indices,
                                   floor,
                                   origin_patch,
                                   is_indoor,
                                   player_floor_y,
                                   x,
                                   y,
                                   x + size,
                                   y + size);
            };
            for (int dy = -kAreaOverlayRadiusCells; dy <= kAreaOverlayRadiusCells; ++dy) {
                for (int dx = -kAreaOverlayRadiusCells; dx <= kAreaOverlayRadiusCells; ++dx) {
                    const map::AreaCell* cell = grid.cell(cell_x + dx, cell_y + dy);
                    if (!cell) {
                        continue;
                    }
                    const int base_x = coords::cell_origin_pixels(cell->cell_x());
                    const int base_y = coords::cell_origin_pixels(cell->cell_y());
                    constexpr std::size_t tile_count = map::kTileCount * map::kTileCount;
                    const bool has_blocked_masks = cell->blocked_masks().size() == tile_count;
                    const bool has_movement_masks = cell->not_moveable_masks().size() == tile_count;
                    const bool fine_movement =
                        layer == AreaLayer::NotMoveable && !cell->movement_masks().empty();
                    const int cells_per_tile =
                        fine_movement ? map::kMovementCellsPerTile : map::kCellsPerTile;
                    const int cell_size = map::kTileSize / cells_per_tile;
                    const std::uint64_t full_tile = fine_movement ? ~std::uint64_t{0} : 0xffffu;
                    for (int ty = 0; ty < map::kTileCount; ++ty) {
                        for (int tx = 0; tx < map::kTileCount; ++tx) {
                            const std::uint32_t attr = cell->at(tx, ty);
                            const int px = base_x + tx * map::kTileSize;
                            const int py = base_y + ty * map::kTileSize;
                            std::uint64_t subcells = 0;
                            if (layer == AreaLayer::Blocked) {
                                subcells = has_blocked_masks ? cell->blocked_mask(tx, ty)
                                                             : attr & full_tile;
                            } else if (layer == AreaLayer::NotMoveable && has_movement_masks) {
                                subcells = fine_movement ? cell->movement_mask(tx, ty)
                                                         : cell->not_moveable_mask(tx, ty);
                            } else if ((attr & mask) != 0) {
                                subcells = full_tile;
                            }
                            if (subcells == 0) {
                                continue;
                            }
                            if (subcells == full_tile) {
                                add_quad(px, py, map::kTileSize);
                                continue;
                            }
                            for (int sy = 0; sy < cells_per_tile; ++sy) {
                                for (int sx = 0; sx < cells_per_tile; ++sx) {
                                    const int bit = sx + cells_per_tile * sy;
                                    if ((subcells & (std::uint64_t{1} << bit)) == 0) {
                                        continue;
                                    }
                                    add_quad(px + sx * cell_size, py + sy * cell_size, cell_size);
                                }
                            }
                        }
                    }
                }
            }
            if (!vertices.empty()) {
                m_layer_meshes[layer_index] = engine.create_mesh(
                    vertices.data(), vertices.size(), indices.data(), indices.size());
            }
        }

        std::vector<engine::VertexP3T2> spawn_vertices;
        std::vector<std::uint32_t> spawn_indices;
        for (const SpawnArea& area : m_spawn_areas) {
            if (m_visible_areas.count(area.m_area) == 0 || area.m_map != map_index) {
                continue;
            }
            for (int py = area.m_y0; py <= area.m_y1; py += map::kTileSize) {
                for (int px = area.m_x0; px <= area.m_x1; px += map::kTileSize) {
                    append_ground_quad(spawn_vertices,
                                       spawn_indices,
                                       floor,
                                       origin_patch,
                                       is_indoor,
                                       player_floor_y,
                                       px,
                                       py,
                                       px + map::kTileSize,
                                       py + map::kTileSize);
                }
            }
        }
        if (!spawn_vertices.empty()) {
            m_spawn_mesh = engine.create_mesh(spawn_vertices.data(),
                                              spawn_vertices.size(),
                                              spawn_indices.data(),
                                              spawn_indices.size());
        }
    }

    void WorldDebugOverlay::draw(engine::IEngine& engine) const {
        if (!m_fill_texture) {
            return;
        }
        for (int index = 0; index < static_cast<int>(AreaLayer::Count); ++index) {
            const engine::MeshHandle mesh = m_layer_meshes[index];
            if (!mesh) {
                continue;
            }
            const OverlayColor color = kAreaColors[index];
            engine.draw_mesh_textured(mesh,
                                      m_fill_texture,
                                      engine::Mat4::identity(),
                                      true,
                                      true,
                                      false,
                                      false,
                                      0.0f,
                                      0.0f,
                                      color.r,
                                      color.g,
                                      color.b,
                                      1.0f);
        }
        if (m_spawn_mesh) {
            engine.draw_mesh_textured(m_spawn_mesh,
                                      m_fill_texture,
                                      engine::Mat4::identity(),
                                      true,
                                      true,
                                      false,
                                      false,
                                      0.0f,
                                      0.0f,
                                      kSpawnAreaColor.r,
                                      kSpawnAreaColor.g,
                                      kSpawnAreaColor.b,
                                      1.0f);
        }
    }

    void WorldDebugOverlay::destroy(engine::IEngine& engine) {
        clear_meshes(engine);
        if (m_fill_texture) {
            engine.destroy_texture(m_fill_texture);
            m_fill_texture = {};
        }
    }

}  // namespace openkal::client
