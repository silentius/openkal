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

#include "WorldDocument.h"
#include "openkal/terrain/GbModel.h"

#include <QElapsedTimer>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointF>
#include <QSet>
#include <QTimer>

#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace openkal::world_editor {

    /** Interactive 3D view of the outdoor world. */
    class WorldViewport final : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
      public:
        /** Mouse interaction mode. */
        enum class Tool { Select, Raise, Lower, Flatten, Texture, Place };

        /** Current viewport editing controls. */
        struct Brush {
            Tool tool = Tool::Select;
            float radius = 256;
            float height_step = 48;
            float level = 0;
            float opacity = 0.6f;
            int material = -1;
            std::string asset;
        };

        /** An already resolved model and transform in a preview scene. */
        struct PreviewObject {
            std::string path;
            QMatrix4x4 transform;
        };

        /** Read-only preview of a creature and its surroundings. */
        struct Preview {
            bool outdoor = false;
            QVector3D position;
            float yaw = 0;
            float scale = 1;
            std::vector<std::string> parts;
            std::string skeleton;
            std::string motion;
            std::vector<PreviewObject> surroundings;
        };

        /** Shows a read-only preview in this viewport.
         * @param scene Creature and surrounding geometry.
         * @param recenter Whether to frame the creature from its default camera angle.
         */
        void set_preview(Preview scene, bool recenter);

        /** Clears the preview scene while keeping the viewport initialized. */
        void clear_preview();

        /** @return Whether a visible mesh was loaded for the current preview. */
        bool preview_model_available() const;

        /** Renders an orthographic terrain and object image with a dedicated offscreen context.
         * Use an unshown viewport for this operation.
         * @param area World X/Z bounds of the requested image. North is at the top.
         * @param size Output dimensions in pixels.
         * @return The rendered image, or an empty image if the terrain or OpenGL is unavailable.
         */
        [[nodiscard]] QImage render_map(const QRectF& area, QSize size);

        /** Creates the viewport and binds it to an existing document.
         * @param document World document with a lifetime longer than the view.
         * @param parent Optional Qt parent.
         */
        explicit WorldViewport(WorldDocument& document, QWidget* parent = nullptr);
        /** Releases the OpenGL resources owned by the viewport. */
        ~WorldViewport() override;

        /** Replaces the current brush settings.
         * @param value Active tool settings.
         */
        void set_brush(Brush value);
        /** Centers the camera on one outdoor chunk.
         * @param coordinate Coordinate of the center chunk.
         */
        void focus_chunk(QPoint coordinate);
        /** Frames the entire outdoor world. */
        void fit_world();
        /** Toggles the first-person terrain preview.
         * @param enabled True to enter walking mode, false to return to the orbit camera.
         */
        void set_walking(bool enabled);
        /** Reverses vertical mouse input in both camera modes.
         * @param enabled True to invert vertical movement.
         */
        void set_invert_mouse_y(bool enabled);
        /** Toggles the outdoor water surface preview.
         * @param visible True to draw the surface at the standard OpenKal water height.
         */
        void set_water_visible(bool visible);
        /** Toggles markers for loaded placements that have no visible mesh.
         * @param visible True to show and allow selecting invisible placements.
         */
        void set_invisible_placements_visible(bool visible);
        /** Clears cached scene resources after opening another client directory. */
        void reset_scene();
        /** Clears transient selection after undo or redo. */
        void clear_selection();
        /** Removes the selected placement as one edit. */
        void remove_selection();
        /** Moves the selected placement vertically as one edit.
         * @param height New height in client world units.
         */
        void set_selection_height(float height);
        /** Commits a pending mouse gesture. */
        void finish_edit();
        /** @return Number of loaded chunks in the view. */
        [[nodiscard]] std::size_t visible_count() const {
            return m_visible.size();
        }
        /** @return Status of placement files in the visible neighborhood. */
        [[nodiscard]] QString placement_warning() const;

        /** Fires when the edit history or selected object changes. */
        std::function<void()> on_change;
        /** Reports a user-visible operation or renderer error. */
        std::function<void(const QString&)> on_message;
        /** Displays the current selection name and world coordinates. */
        std::function<void(const QString&, std::optional<float>)> on_selection;
        /** Reports whether walking is active and whether the mouse is captured. */
        std::function<void(bool, bool)> on_walk_mode;
        /** Reports the new neighborhood center after walking into another chunk. */
        std::function<void(QPoint)> on_walk_chunk;
        /** Reports a change to the loaded chunks in the viewport. */
        std::function<void()> on_visible_chunks;

      protected:
        void initializeGL() override;
        void paintGL() override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;

      private:
        struct Offscreen;
        struct Ray {
            QVector3D origin;
            QVector3D direction;
        };
        struct Mesh {
            GLuint vao = 0;
            GLuint vertices = 0;
            GLuint indices = 0;
            GLsizei count = 0;
        };
        struct TerrainGpu {
            Mesh mesh;
            std::vector<GLuint> maps;
            std::uint64_t height_revision = 0;
            std::uint64_t texture_revision = 0;
            int sample_step = 0;
            bool below_water = false;
        };
        struct HeightBounds {
            std::uint64_t revision = 0;
            float minimum = 0;
            float maximum = 65535;
        };
        struct Part {
            Mesh mesh;
            GLuint texture = 0;
            bool blend = false;
            bool cutout = false;
        };
        enum class ModelGeometry { Unavailable, Invisible, Mesh };
        struct Model {
            std::vector<Part> parts;
            QVector3D minimum;
            QVector3D maximum;
            ModelGeometry geometry = ModelGeometry::Unavailable;
        };
        struct Pick {
            std::uint64_t id = 0;
            QVector3D position;
        };
        struct Detail {
            GLuint texture = 0;
            float repeat = 1;
        };

        [[nodiscard]] QMatrix4x4 camera() const;
        [[nodiscard]] float view_far_distance() const;
        [[nodiscard]] QMatrix4x4 view_transform() const;
        [[nodiscard]] QVector3D eye() const;
        [[nodiscard]] QVector3D view_direction() const;
        void load_neighborhood(QPoint coordinate);
        void update_visible_chunks(const QMatrix4x4& camera_matrix);
        void process_chunk_queue();
        [[nodiscard]] bool shows_objects(QPoint coordinate) const;
        void pan_camera(float right, float forward);
        void drag_camera(QPointF from, QPointF to);
        void orbit_camera(QPointF delta);
        void zoom_camera(QPointF anchor, float scale);
        [[nodiscard]] QPointF zoom_anchor() const;
        void walk_camera(QVector3D offset);
        void set_walk_position(QVector3D position);
        [[nodiscard]] std::optional<float> ground_height(QVector3D position) const;
        void update_navigation();
        void stop_navigation();
        [[nodiscard]] Ray pointer_ray(QPointF screen) const;
        [[nodiscard]] std::optional<QVector3D> ground_hit(const Ray& ray) const;
        [[nodiscard]] static std::optional<QVector3D> plane_hit(const Ray& ray, float height);
        [[nodiscard]] Pick object_hit(const Ray& ray) const;
        void start_pointer_edit(QPointF position);
        void end_pointer_edit(bool commit);
        void stamp(QVector3D center);
        void select(std::uint64_t id);
        [[nodiscard]] std::pair<const Placement*, QPoint> selection() const;
        void report_selection();
        void frame_preview();
        void initialize_renderer();
        void destroy_renderer();
        void render_scene(const QMatrix4x4& matrix, bool outdoor);
        void release_gl();
        void clear_scene_gl();
        void free_mesh(Mesh& mesh);
        void free_texture(GLuint& texture);
        void draw_mesh(const Mesh& mesh);
        void upload_mesh(Mesh& mesh,
                         const float* vertices,
                         std::size_t float_count,
                         const std::vector<std::uint32_t>& indices,
                         bool dynamic = false);
        [[nodiscard]] GLuint upload_texture(const terrain::ImageRgba& image, bool repeat);
        [[nodiscard]] Detail detail(std::uint8_t material);
        [[nodiscard]] const Model& model(const std::string& name);
        [[nodiscard]] const Model& preview_model(const std::string& key);
        void add_model_parts(Model& result,
                             const std::string& path,
                             const terrain::GbModel* skeleton = nullptr,
                             const terrain::GbModel* motion = nullptr);
        [[nodiscard]] bool shows_model(const Model& model) const;
        void update_terrain(TerrainGpu& gpu, const WorldChunk& chunk);
        void draw_terrain(const QMatrix4x4& camera_matrix);
        void draw_objects(const QMatrix4x4& camera_matrix);
        void draw_water();

        WorldDocument& m_document;
        QTimer m_chunk_timer;
        std::deque<QPoint> m_chunk_queue;
        std::vector<QPoint> m_available_chunks;
        std::vector<QPoint> m_requested_chunks;
        QSet<QPoint> m_corrupt_chunks;
        std::map<std::pair<int, int>, HeightBounds> m_chunk_heights;
        int m_terrain_step = 1;
        QTimer m_navigation_timer;
        QElapsedTimer m_navigation_clock;
        QSet<int> m_navigation_keys;
        Brush m_brush;
        QPoint m_active{-1, -1};
        std::vector<QPoint> m_visible;
        QPointF m_last_mouse;
        std::optional<QPointF> m_left_press;
        std::optional<QVector3D> m_orbit_pivot;
        QVector3D m_target;
        QVector3D m_walking_feet;
        QVector3D m_drag_offset;
        std::optional<QVector3D> m_hover;
        std::optional<QVector3D> m_last_stamp;
        std::optional<Preview> m_preview;
        std::string m_preview_key;
        QRectF m_map_area;
        std::unique_ptr<Offscreen> m_offscreen;
        bool m_offscreen_attempted = false;
        std::uint64_t m_selected = 0;
        float m_distance = 16000;
        /** The default heading faces decreasing world Z in the outdoor map. */
        static constexpr float kDefaultYaw = 3.14159265f;
        float m_yaw = kDefaultYaw;
        float m_pitch = 0.95f;
        float m_walking_pitch = 0;
        bool m_editing = false;
        bool m_dragging = false;
        bool m_walking = false;
        bool m_walk_fast = false;
        bool m_walk_mouse_captured = false;
        bool m_invert_mouse_y = false;
        bool m_water_visible = true;
        bool m_invisible_placements_visible = false;
        bool m_scene_reset = false;
        bool m_frame_preview = false;
        std::unique_ptr<QOpenGLShaderProgram> m_program;
        std::map<std::pair<int, int>, TerrainGpu> m_terrain;
        std::unordered_map<unsigned, Detail> m_details;
        std::unordered_map<std::string, Model> m_models;
        GLuint m_white = 0;
        Mesh m_marker;
        GLuint m_water_texture = 0;
        Mesh m_water;
    };

}  // namespace openkal::world_editor
