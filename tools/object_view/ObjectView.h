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

#include "ObjectCatalog.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GbModel.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::object_view {

    /** GPU submesh of a previewed .gb object. */
    class GpuPart {
      public:
        /** @return Uploaded mesh handle. */
        [[nodiscard]] engine::MeshHandle mesh() const {
            return m_mesh;
        }

        /** @param value Uploaded mesh handle. */
        void set_mesh(engine::MeshHandle value) {
            m_mesh = value;
        }

        /** @return Uploaded texture handle. */
        [[nodiscard]] engine::TextureHandle texture() const {
            return m_texture;
        }

        /** @param value Uploaded texture handle. */
        void set_texture(engine::TextureHandle value) {
            m_texture = value;
        }

        /** @return True when the part uses alpha blending. */
        [[nodiscard]] bool alpha_blend() const {
            return m_alpha_blend;
        }

        /** @param value Alpha-blend flag. */
        void set_alpha_blend(bool value) {
            m_alpha_blend = value;
        }

        /** @return True when the part uses alpha testing. */
        [[nodiscard]] bool alpha_test() const {
            return m_alpha_test;
        }

        /** @param value Alpha-test flag. */
        void set_alpha_test(bool value) {
            m_alpha_test = value;
        }

        /** @return True when both faces are drawn. */
        [[nodiscard]] bool two_sided() const {
            return m_two_sided;
        }

        /** @param value Two-sided flag. */
        void set_two_sided(bool value) {
            m_two_sided = value;
        }

        /** @return Referenced texture name. */
        [[nodiscard]] const std::string& texture_name() const {
            return m_texture_name;
        }

        /** @param value Referenced texture name. */
        void set_texture_name(std::string value) {
            m_texture_name = std::move(value);
        }

        /** @return True when the texture was decoded from disk. */
        [[nodiscard]] bool texture_loaded() const {
            return m_texture_loaded;
        }

        /** @param value Texture-loaded flag. */
        void set_texture_loaded(bool value) {
            m_texture_loaded = value;
        }

        /** @return Material used for UV animation. */
        [[nodiscard]] const terrain::GbMaterial& uv_material() const {
            return m_uv_material;
        }

        /** @param value Material used for UV animation. */
        void set_uv_material(terrain::GbMaterial value) {
            m_uv_material = std::move(value);
        }

        /** @return Material key times in milliseconds. */
        [[nodiscard]] const std::vector<std::uint16_t>& uv_key_times() const {
            return m_uv_key_times;
        }

        /** @param value Material key times in milliseconds. */
        void set_uv_key_times(std::vector<std::uint16_t> value) {
            m_uv_key_times = std::move(value);
        }

      private:
        /** Uploaded mesh. */
        engine::MeshHandle m_mesh{};

        /** Uploaded albedo. */
        engine::TextureHandle m_texture{};

        /** Alpha-blend flag. */
        bool m_alpha_blend = false;

        /** Alpha-test flag. */
        bool m_alpha_test = false;

        /** Two-sided flag. */
        bool m_two_sided = false;

        /** True when load_gb_texture succeeded. */
        bool m_texture_loaded = false;

        /** Texture file stem from the GB material. */
        std::string m_texture_name;

        /** Material copied for UV sampling. */
        terrain::GbMaterial m_uv_material;

        /** Material key times copied for UV sampling. */
        std::vector<std::uint16_t> m_uv_key_times;
    };

    /** Cached GPU model plus bounds used to frame the orbit camera. */
    class GpuPreview {
      public:
        /** @return Uploaded parts. */
        [[nodiscard]] const std::vector<GpuPart>& parts() const {
            return m_parts;
        }

        /** @return Mutable uploaded parts. */
        [[nodiscard]] std::vector<GpuPart>& parts() {
            return m_parts;
        }

        /** @return Bounding-box center X. */
        [[nodiscard]] float center_x() const {
            return m_center_x;
        }

        /** @return Bounding-box center Y. */
        [[nodiscard]] float center_y() const {
            return m_center_y;
        }

        /** @return Bounding-box center Z. */
        [[nodiscard]] float center_z() const {
            return m_center_z;
        }

        /** @return Bounding-sphere radius. */
        [[nodiscard]] float radius() const {
            return m_radius;
        }

        /** @return Lowest Y used to place the ground plane. */
        [[nodiscard]] float min_y() const {
            return m_min_y;
        }

        /** @return Number of materials whose textures loaded. */
        [[nodiscard]] int textures_ok() const {
            return m_textures_ok;
        }

        /** @return Number of materials that fell back to a stub texel. */
        [[nodiscard]] int textures_missing() const {
            return m_textures_missing;
        }

        /**
         * Stores bounds from the compatibility behavior model.
         * @param min_x Minimum X.
         * @param min_y Minimum Y.
         * @param min_z Minimum Z.
         * @param max_x Maximum X.
         * @param max_y Maximum Y.
         * @param max_z Maximum Z.
         */
        void
        set_bounds(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);

        /**
         * Records texture load counts.
         * @param ok Successful texture loads.
         * @param missing Failed texture loads.
         */
        void set_texture_counts(int ok, int missing);

      private:
        /** Uploaded submeshes. */
        std::vector<GpuPart> m_parts;

        /** Bounding-box center. */
        float m_center_x = 0.0f;

        /** Bounding-box center. */
        float m_center_y = 0.0f;

        /** Bounding-box center. */
        float m_center_z = 0.0f;

        /** Bounding-sphere radius. */
        float m_radius = 50.0f;

        /** Lowest Y of the model. */
        float m_min_y = 0.0f;

        /** Successful texture loads. */
        int m_textures_ok = 0;

        /** Failed texture loads. */
        int m_textures_missing = 0;
    };

    /** Interactive .gb browser: list on the left, textured 3D preview on the
     * right. */
    class ObjectView {
      public:
        /**
         * Builds a viewer for the given client data root.
         * @param data_root Client data directory.
         * @param models_dir Absolute directory scanned when no explicit models are given.
         * @param section_label Label shown above the model list.
         * @param window_title Native window title.
         * @param ui_scale UI multiplier (2 is comfortable on 4K).
         * @param gb_paths Absolute .gb files to show; empty scans all of models_dir.
         */
        ObjectView(std::string data_root,
                   std::string models_dir,
                   std::string section_label,
                   std::string window_title,
                   float ui_scale,
                   std::vector<std::string> gb_paths);

        /** Releases GPU resources owned by the viewer. */
        ~ObjectView();

        ObjectView(const ObjectView&) = delete;
        ObjectView& operator=(const ObjectView&) = delete;

        /**
         * Runs the window until the user quits.
         * @return 0 on a clean exit, 1 on init failure.
         */
        [[nodiscard]] int run();

      private:
        /** Creates the GL window and UI font. */
        [[nodiscard]] bool init_window();

        /** Builds the checker ground mesh. */
        void create_ground();

        /** Destroys cached meshes and textures. */
        void destroy_gpu();

        /**
         * Loads (or returns a cached) GPU model for a .gb path.
         * @param gb_path Absolute .gb path.
         * @return Cached preview, or nullptr when the file has no meshes.
         */
        [[nodiscard]] const GpuPreview* get_or_load(const std::string& gb_path);

        /**
         * Frames the orbit camera around a preview.
         * @param preview Model to fit.
         */
        void fit_camera(const GpuPreview& preview);

        /**
         * Handles list, filter, and orbit input.
         * @param in Frame input.
         * @param panel_w Left-panel width in pixels.
         */
        void handle_input(const engine::FrameInput& in, float panel_w);

        /**
         * Moves the list selection and keeps it in view.
         * @param delta Row delta.
         */
        void move_selection(int delta);

        /** Ensures the selected row is inside the scroll window. */
        void ensure_selection_visible();

        /**
         * Selects a visible row and loads its model.
         * @param visible_index Index into catalog.visible().
         */
        void select_visible(int visible_index);

        /**
         * Draws the 3D preview into the right pane.
         * @param in Frame input.
         * @param panel_w Left-panel width in pixels.
         * @param time_sec Animation clock.
         */
        void draw_preview(const engine::FrameInput& in, float panel_w, float time_sec);

        /**
         * Draws the left list, filter box, and status.
         * @param in Frame input.
         * @param panel_w Left-panel width in pixels.
         */
        void draw_ui(const engine::FrameInput& in, float panel_w);

        /**
         * Draws a single list row.
         * @param x Row left.
         * @param y Row top.
         * @param w Row width.
         * @param h Row height.
         * @param entry Row model.
         * @param selected True when this is the current selection.
         */
        void draw_row(float x, float y, float w, float h, const ObjectEntry& entry, bool selected);

        /**
         * Draws bitmap-font text, truncating with an ellipsis when it exceeds max_w.
         * @param x Left.
         * @param y Top.
         * @param max_w Maximum width in pixels.
         * @param text Source string.
         * @param r Red.
         * @param g Green.
         * @param b Blue.
         */
        void draw_text_fit(
            float x, float y, float max_w, const std::string& text, float r, float g, float b);

        /**
         * Builds a perspective projection that maps NDC into a pixel pane.
         * @param fovy_rad Vertical field of view.
         * @param pane_x Pane left.
         * @param pane_y Pane top.
         * @param pane_w Pane width.
         * @param pane_h Pane height.
         * @param win_w Framebuffer width.
         * @param win_h Framebuffer height.
         * @param znear Near plane.
         * @param zfar Far plane.
         * @return Column-major projection matrix.
         */
        [[nodiscard]] static engine::Mat4 pane_perspective(float fovy_rad,
                                                           float pane_x,
                                                           float pane_y,
                                                           float pane_w,
                                                           float pane_h,
                                                           float win_w,
                                                           float win_h,
                                                           float znear,
                                                           float zfar);

        /** Client data root. */
        std::string m_data_root;

        /** Directory containing the .gb models. */
        std::string m_models_dir;

        /** Label shown above the model list. */
        std::string m_section_label;

        /** Native window title. */
        std::string m_window_title;

        /** Explicit .gb files from the command line; empty means scan runtime state. */
        std::vector<std::string> m_gb_paths;

        /** UI multiplier (2 ≈ 4K). */
        float m_ui_scale = 2.0f;

        /** Bitmap font glyph scale. */
        float m_font_scale = 4.0f;

        /** Indexed .gb files. */
        ObjectCatalog m_catalog;

        /** Engine backend. */
        std::unique_ptr<engine::IEngine> m_engine;

        /** Bitmap font / UI helper. */
        client::WidgetTools m_ui;

        /** GPU models keyed by absolute .gb path. */
        std::unordered_map<std::string, GpuPreview> m_models;

        /** Textures keyed by gb_path|texture_name. */
        std::unordered_map<std::string, engine::TextureHandle> m_textures;

        /** Currently displayed model, or nullptr. */
        const GpuPreview* m_current = nullptr;

        /** Selected visible-row index, or -1. */
        int m_selected = -1;

        /** First visible list row. */
        int m_scroll = 0;

        /** Number of list rows that fit the panel. */
        int m_visible_rows = 1;

        /** Orbit yaw in radians. */
        float m_yaw = 0.6f;

        /** Orbit pitch in radians. */
        float m_pitch = -0.35f;

        /** Orbit distance. */
        float m_distance = 200.0f;

        /** Fitted default orbit distance. */
        float m_fit_distance = 200.0f;

        /** Look-at point. */
        engine::Vec3 m_target{};

        /** Arrow-key hold timer for list repeat. */
        float m_nav_hold = 0.0f;

        /** Backspace hold timer for filter repeat. */
        float m_backspace_hold = 0.0f;

        /** Checker ground mesh. */
        engine::MeshHandle m_ground_mesh{};

        /** Checker ground texture. */
        engine::TextureHandle m_ground_tex{};

        /** Status line under the list. */
        std::string m_status;

        /** Set when Escape is pressed with an empty filter. */
        bool m_quit = false;
    };

}  // namespace openkal::object_view
