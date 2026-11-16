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

#include "EffectCatalog.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/SpriteFx.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/GbModel.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::skill_effect_view {

    /** GPU submesh of a previewed skill/effect .gb. */
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
        /** Uploaded mesh, re-skinned every frame when the clip plays. */
        engine::MeshHandle m_mesh{};

        /** Uploaded albedo. */
        engine::TextureHandle m_texture{};

        /** Alpha-blend flag. */
        bool m_alpha_blend = false;

        /** Alpha-test flag. */
        bool m_alpha_test = false;

        /** Material copied for UV sampling. */
        terrain::GbMaterial m_uv_material;

        /** Material key times copied for UV sampling. */
        std::vector<std::uint16_t> m_uv_key_times;
    };

    /** Cached GPU model plus bind-pose data used to loop the first clip. */
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

        /** @return Bind-pose meshes used as the skinning source. */
        [[nodiscard]] const std::vector<terrain::GbMeshCpu>& bind_meshes() const {
            return m_bind_meshes;
        }

        /** @return Mutable bind-pose meshes. */
        [[nodiscard]] std::vector<terrain::GbMeshCpu>& bind_meshes() {
            return m_bind_meshes;
        }

        /** @return Skeleton bones. */
        [[nodiscard]] const std::vector<terrain::GbBone>& bones() const {
            return m_bones;
        }

        /**
         * Stores the skeleton.
         * @param value Bones in skeleton order.
         */
        void set_bones(std::vector<terrain::GbBone> value) {
            m_bones = std::move(value);
        }

        /** @return First animation clip. */
        [[nodiscard]] const terrain::GbAnimClip& anim() const {
            return m_anim;
        }

        /**
         * Stores the first clip.
         * @param value Animation clip.
         */
        void set_anim(terrain::GbAnimClip value) {
            m_anim = std::move(value);
        }

        /** @return True when the model should face the camera. */
        [[nodiscard]] bool billboard() const {
            return m_billboard;
        }

        /**
         * Sets the camera-facing flag.
         * @param value True to billboard.
         */
        void set_billboard(bool value) {
            m_billboard = value;
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

        /** Bind-pose meshes in load order. */
        std::vector<terrain::GbMeshCpu> m_bind_meshes;

        /** Skeleton bones. */
        std::vector<terrain::GbBone> m_bones;

        /** First animation clip. */
        terrain::GbAnimClip m_anim;

        /** True when instances should face the camera. */
        bool m_billboard = false;

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

    /** Interactive skill/effect browser: names on the left, looping 3D preview on the
     * right. */
    class SkillEffectView {
      public:
        /**
         * Builds a viewer for the given client data and skill JSON.
         * @param data_root Client data directory (parent of Skill/ and Effect/).
         * @param skills_dir config/skills directory.
         * @param monsters_dir config/monsters directory.
         * @param npcs_dir config/npcs directory.
         * @param ui_scale UI multiplier (2 is comfortable on 4K).
         */
        SkillEffectView(std::string data_root,
                        std::string skills_dir,
                        std::string monsters_dir,
                        std::string npcs_dir,
                        float ui_scale);

        /** Releases GPU resources owned by the viewer. */
        ~SkillEffectView();

        SkillEffectView(const SkillEffectView&) = delete;
        SkillEffectView& operator=(const SkillEffectView&) = delete;

        /**
         * Runs the window until the user quits.
         * @return 0 on a clean exit, 1 on init failure.
         */
        [[nodiscard]] int run();

      private:
        /** Pixel layout of the left panel, shared by hit-testing and drawing. */
        class PanelLayout {
          public:
            /** @return True when x is inside the left panel. */
            [[nodiscard]] bool over_panel(float x) const {
                return x < m_panel_w;
            }

            /**
             * True when the pointer is over a list row (not tabs/filter/status).
             * @param x Pointer X.
             * @param y Pointer Y.
             * @return True when a list row can be clicked.
             */
            [[nodiscard]] bool over_list(float x, float y) const {
                return over_panel(x) && y >= m_list_top && y < m_status_y;
            }

            /** Left-panel width. */
            float m_panel_w = 0.0f;

            /** Outer padding. */
            float m_pad = 0.0f;

            /** Title strip height. */
            float m_title_h = 0.0f;

            /** Tab button height. */
            float m_tab_h = 0.0f;

            /** Tab row top. */
            float m_tabs_y = 0.0f;

            /** Width of one tab. */
            float m_tab_w = 0.0f;

            /** Filter box top. */
            float m_filter_y = 0.0f;

            /** Filter strip height. */
            float m_filter_h = 0.0f;

            /** Gubun-chip row top. */
            float m_chips_y = 0.0f;

            /** Gubun-chip height. */
            float m_chip_h = 0.0f;

            /** Gubun-chip width. */
            float m_chip_w = 0.0f;

            /** List top. */
            float m_list_top = 0.0f;

            /** List height. */
            float m_list_h = 0.0f;

            /** One list row height. */
            float m_row_h = 0.0f;

            /** Status strip top. */
            float m_status_y = 0.0f;

            /** Status strip height. */
            float m_status_h = 0.0f;

            /** Rows that fit the list. */
            int m_visible_rows = 1;
        };

        /**
         * Builds the left-panel layout for the current scale and window size.
         * @param panel_w Left-panel width in pixels.
         * @param win_h Framebuffer height.
         * @return Layout used for input and drawing.
         */
        [[nodiscard]] PanelLayout make_layout(float panel_w, float win_h) const;

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
        [[nodiscard]] GpuPreview* get_or_load(const std::string& gb_path);

        /**
         * Frames the orbit camera around a preview.
         * @param preview Model to fit.
         */
        void fit_camera(const GpuPreview& preview);

        /**
         * Handles list, filter, slot, and orbit input.
         * @param in Frame input.
         * @param layout Left-panel layout.
         */
        void handle_input(const engine::FrameInput& in, const PanelLayout& layout);

        /**
         * Moves the list selection and keeps it in view.
         * @param delta Row delta.
         */
        void move_selection(int delta);

        /** Ensures the selected row is inside the scroll window. */
        void ensure_selection_visible();

        /**
         * Selects a visible row and loads its current slot / asset.
         * @param visible_index Index into catalog.visible().
         */
        void select_visible(int visible_index);

        /** Reloads the 3D preview for the current skill slot or asset. */
        void load_current_preview();

        /**
         * Selects a gubun slot on the current skill.
         * @param gubun 0 prep / 1 fire / 2 hit.
         */
        void select_slot(int gubun);

        /**
         * Switches the left list mode.
         * @param mode Skills, Models, Sprite, Fx, or Mobs.
         */
        void set_mode(BrowseMode mode);

        /** Detaches the looping sprite/FX preview entity. */
        void clear_fx_preview();

        /**
         * Frames the orbit camera around a world-space radius at the origin.
         * @param radius Bounding radius.
         */
        void fit_fx_camera(float radius);

        /**
         * Attaches looping sprites/FX for the current Sprite, Fx, or Mobs row.
         * @return True when at least one emitter is live.
         */
        [[nodiscard]] bool load_fx_preview();

        /**
         * Joins used-by labels for the status line.
         * @param names User labels.
         * @return Comma-separated names, or empty.
         */
        [[nodiscard]] static std::string join_used_by(const std::vector<std::string>& names);

        /**
         * Left-pixel of a tab button.
         * @param layout Panel layout.
         * @param index Tab index 0..4.
         * @return Tab left X.
         */
        [[nodiscard]] float tab_x(const PanelLayout& layout, int index) const;

        /**
         * Skins live meshes to the current clip time.
         * @param preview Model to update.
         * @param time_ms Clip time in milliseconds.
         */
        void skin_preview(GpuPreview& preview, float time_ms);

        /**
         * Draws the 3D preview into the right pane.
         * @param in Frame input.
         * @param panel_w Left-panel width in pixels.
         * @param time_sec Wall-clock time for UV scroll fallback.
         */
        void draw_preview(const engine::FrameInput& in, float panel_w, float time_sec);

        /**
         * Draws the left list, tabs, filter, slots, and status.
         * @param in Frame input.
         * @param layout Left-panel layout.
         */
        void draw_ui(const engine::FrameInput& in, const PanelLayout& layout);

        /**
         * Draws a single list row.
         * @param x Row left.
         * @param y Row top.
         * @param w Row width.
         * @param h Row height.
         * @param label Row text.
         * @param selected True when this is the current selection.
         * @param hovered True when the pointer is over this row.
         */
        void draw_row(float x,
                      float y,
                      float w,
                      float h,
                      const std::string& label,
                      bool selected,
                      bool hovered);

        /**
         * Draws TrueType text, truncating with an ellipsis when it exceeds max_w.
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

        /** config/skills directory. */
        std::string m_skills_dir;

        /** config/monsters directory. */
        std::string m_monsters_dir;

        /** config/npcs directory. */
        std::string m_npcs_dir;

        /** UI multiplier (2 ≈ 4K). */
        float m_ui_scale = 2.0f;

        /** TrueType display scale. */
        float m_font_scale = 1.6f;

        /** Indexed skills and .gb files. */
        EffectCatalog m_catalog;

        /** Engine backend. */
        std::unique_ptr<engine::IEngine> m_engine;

        /** UI typeface. */
        client::FreetypeFont m_font;

        /** Looping sprite / FX preview (same loader as the game client). */
        client::SpriteFx m_sprite_fx;

        /** True when the right pane is showing looping sprites/FX. */
        bool m_fx_preview = false;

        /** Framing radius for looping FX when no .gb is loaded. */
        float m_fx_radius = 40.0f;

        /** GPU models keyed by absolute .gb path. */
        std::unordered_map<std::string, GpuPreview> m_models;

        /** Textures keyed by gb_path|texture_name. */
        std::unordered_map<std::string, engine::TextureHandle> m_textures;

        /** Currently displayed model, or nullptr. */
        GpuPreview* m_current = nullptr;

        /** Selected visible-row index, or -1. */
        int m_selected = -1;

        /** Active gubun slot while browsing skills. */
        int m_slot = 0;

        /** First visible list row. */
        int m_scroll = 0;

        /** Number of list rows that fit the panel. */
        int m_visible_rows = 1;

        /** Hovered visible-row index, or -1. */
        int m_hover = -1;

        /** Playback cursor in milliseconds. */
        float m_anim_ms = 0.0f;

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

        /** Left/Right slot-key hold timer (rising-edge only). */
        float m_slot_hold = 0.0f;

        /** Checker ground mesh. */
        engine::MeshHandle m_ground_mesh{};

        /** Checker ground texture. */
        engine::TextureHandle m_ground_tex{};

        /** Status line under the list. */
        std::string m_status;

        /** Set when Escape is pressed with an empty filter. */
        bool m_quit = false;
    };

}  // namespace openkal::skill_effect_view
