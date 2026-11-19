/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "WorldViewport.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/GtxLoader.h"

#include <QApplication>
#include <QCursor>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuaternion>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace openkal::world_editor {
    namespace {

        constexpr int kSide = map::kMapSize;
        constexpr int kSamples = map::kHeightSamples;
        constexpr float kCell = map::kTerrainCellSize;
        constexpr float kExtent = terrain::kOplPatchScale;
        constexpr float kMaxOrbitDistance = terrain::kOplFullScale * 4.0f;
        constexpr float kWalkSpeed = 60.0f;
        constexpr float kSprintMultiplier = 2.0f;
        constexpr float kWalkEyeHeight = 17.0f;
        constexpr float kLookSensitivity = 0.003f;

        constexpr char kVertexShader[] = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
uniform mat4 uCamera;
uniform mat4 uModel;
uniform vec3 uLightingOrigin;
out vec2 texcoord;
out vec3 world;
out vec3 lightingPosition;
void main() {
    vec4 p = uModel * vec4(position, 1.0);
    world = p.xyz;
    lightingPosition = p.xyz - uLightingOrigin;
    texcoord = uv;
    gl_Position = uCamera * p;
}
)";

        constexpr char kFragmentShader[] = R"(#version 330 core
in vec2 texcoord;
in vec3 world;
in vec3 lightingPosition;
uniform sampler2D uColor;
uniform sampler2D uDetail;
uniform bool uTerrain;
uniform bool uBase;
uniform bool uCutout;
uniform bool uSelected;
uniform bool uActive;
uniform bool uPatchEdges;
uniform bool uWater;
uniform float uRepeat;
uniform float uWaterUvScale;
uniform vec3 uEye;
uniform vec2 uBrush;
uniform float uRadius;
out vec4 fragment;
void main() {
    if (uWater) {
        vec2 uv = world.xz * uWaterUvScale;
        vec3 primary = texture(uColor, uv).rgb;
        vec3 secondary = texture(uColor, uv * 1.7 + vec2(0.13, 0.21)).rgb;
        vec3 water = mix(primary, secondary, 0.35) * vec3(0.55, 0.70, 0.80);
        float facing = max(normalize(uEye - world).y, 0.0);
        float fresnel = pow(1.0 - facing, 2.5);
        fragment = vec4(mix(water, vec3(0.55, 0.72, 0.95), fresnel * 0.72), 0.40);
        return;
    }
    vec4 color = texture(uColor, texcoord);
    if (uCutout && color.a < 0.45) {
        discard;
    }
    vec3 normal = normalize(cross(dFdx(lightingPosition), dFdy(lightingPosition)));
    float lighting = 0.68 + 0.32 * abs(dot(normal, normalize(vec3(-0.38, 0.85, 0.35))));
    if (uTerrain) {
        vec3 detail = texture(uDetail, texcoord * uRepeat).rgb;
        color.rgb = min(color.rgb * detail * 2.0, 1.0) * lighting;
        if (uPatchEdges) {
            vec2 edge = min(texcoord, 1.0 - texcoord) / max(fwidth(texcoord), vec2(1e-6));
            float border = 1.0 - smoothstep(0.7, 1.8, min(edge.x, edge.y));
            vec3 line = uActive ? vec3(0.26, 0.85, 1.0) : vec3(0.40, 0.62, 0.69);
            color.rgb = mix(color.rgb, line, border * 0.86);
        }
        if (uRadius > 0.0) {
            float d = distance(world.xz, uBrush);
            float aa = max(1.0, fwidth(d));
            float ring = 1.0 - smoothstep(aa * 0.75, aa * 2.5, abs(d - uRadius));
            color.rgb = mix(color.rgb, vec3(1.0, 0.84, 0.39), ring);
        }
        if (uBase) {
            color.a = 1.0;
        }
    } else {
        color.rgb *= lighting;
        if (uSelected) {
            color.rgb = mix(color.rgb, vec3(1.0, 0.73, 0.19), 0.58);
        }
    }
    fragment = color;
}
)";

        QMatrix4x4 transform(QVector3D position, const terrain::OplInstance& object) {
            QMatrix4x4 result;
            result.translate(position);
            result.rotate(QQuaternion(object.qw, object.qx, object.qy, object.qz));
            result.scale(object.sx, object.sy, object.sz);
            return result;
        }

        bool intersect_box(QVector3D origin,
                           QVector3D direction,
                           QVector3D lo,
                           QVector3D hi,
                           float& first,
                           float& last) {
            for (int axis = 0; axis < 3; ++axis) {
                if (std::abs(direction[axis]) < 1e-8f) {
                    if (origin[axis] < lo[axis] || origin[axis] > hi[axis]) {
                        return false;
                    }
                    continue;
                }
                float a = (lo[axis] - origin[axis]) / direction[axis];
                float b = (hi[axis] - origin[axis]) / direction[axis];
                if (a > b) {
                    std::swap(a, b);
                }
                first = std::max(first, a);
                last = std::min(last, b);
                if (first > last) {
                    return false;
                }
            }
            return true;
        }

        float intersect_triangle(
            QVector3D origin, QVector3D direction, QVector3D a, QVector3D b, QVector3D c) {
            const auto ab = b - a;
            const auto ac = c - a;
            const auto p = QVector3D::crossProduct(direction, ac);
            const float determinant = QVector3D::dotProduct(ab, p);
            if (std::abs(determinant) < 1e-6f) {
                return -1;
            }
            const auto t = origin - a;
            const float u = QVector3D::dotProduct(t, p) / determinant;
            const auto q = QVector3D::crossProduct(t, ab);
            const float v = QVector3D::dotProduct(direction, q) / determinant;
            constexpr float epsilon = 0.0002f;
            if (u < -epsilon || v < -epsilon || u + v > 1 + epsilon) {
                return -1;
            }
            return QVector3D::dotProduct(ac, q) / determinant;
        }

        float intersect_terrain(const terrain::HeightPatch& patch,
                                QVector3D origin,
                                QVector3D direction,
                                float max_distance) {
            const float x0 = (patch.patch_x - map::kPatchGrid / 2) * kExtent;
            const float z0 = (patch.patch_y - map::kPatchGrid / 2) * kExtent;
            float first = 0;
            float last = max_distance;
            if (!intersect_box(origin,
                               direction,
                               {x0, 0, z0},
                               {x0 + kExtent, 65535, z0 + kExtent},
                               first,
                               last)) {
                return max_distance;
            }
            const auto entry = origin + direction * first;
            int x = std::clamp(static_cast<int>((entry.x() - x0) / kCell), 0, kSide - 1);
            int z = std::clamp(static_cast<int>((entry.z() - z0) / kCell), 0, kSide - 1);
            const int dx = direction.x() < 0 ? -1 : 1;
            const int dz = direction.z() < 0 ? -1 : 1;
            const auto next_edge = [](float start, float ray, float delta, int cell, int step) {
                return std::abs(delta) < 1e-8f
                           ? std::numeric_limits<float>::infinity()
                           : (start + (cell + (step > 0 ? 1 : 0)) * kCell - ray) / delta;
            };
            // Traverse only the cells crossed by the ray, in near-to-far order.
            while (x >= 0 && z >= 0 && x < kSide && z < kSide) {
                const int offset = z * kSamples + x;
                const auto vertex = [&](int ox, int oz) {
                    return QVector3D(x0 + (x + ox) * kCell,
                                     patch.heights[offset + oz * kSamples + ox],
                                     z0 + (z + oz) * kCell);
                };
                const auto b = vertex(1, 0);
                const auto c = vertex(0, 1);
                const float a_hit = intersect_triangle(origin, direction, vertex(0, 0), c, b);
                const float b_hit = intersect_triangle(origin, direction, b, c, vertex(1, 1));
                if (a_hit >= 0 && a_hit <= last) {
                    max_distance = a_hit;
                }
                if (b_hit >= 0 && b_hit < max_distance && b_hit <= last) {
                    return b_hit;
                }
                if (a_hit >= 0 && a_hit <= last) {
                    return max_distance;
                }
                const float tx = next_edge(x0, origin.x(), direction.x(), x, dx);
                const float tz = next_edge(z0, origin.z(), direction.z(), z, dz);
                if (std::min(tx, tz) > last) {
                    break;
                }
                if (tx <= tz) {
                    x += dx;
                }
                if (tz <= tx) {
                    z += dz;
                }
            }
            return max_distance;
        }

        bool terrain_tool(WorldViewport::Tool tool) {
            using Tool = WorldViewport::Tool;
            return tool == Tool::Raise || tool == Tool::Lower || tool == Tool::Flatten ||
                   tool == Tool::Texture;
        }

        bool navigation_key(int key) {
            return key == Qt::Key_W || key == Qt::Key_A || key == Qt::Key_S || key == Qt::Key_D;
        }

        QVector3D chunk_center(QPoint coordinate, float height = 0) {
            return {(coordinate.x() - map::kPatchGrid / 2 + 0.5f) * kExtent,
                    height,
                    (coordinate.y() - map::kPatchGrid / 2 + 0.5f) * kExtent};
        }

        bool intersects_view(const QMatrix4x4& view, QPoint coordinate, float low, float high) {
            const auto center = chunk_center(coordinate);
            unsigned outside = 0x3f;
            for (float x : {center.x() - kExtent / 2, center.x() + kExtent / 2}) {
                for (float z : {center.z() - kExtent / 2, center.z() + kExtent / 2}) {
                    for (float y : {low, high}) {
                        const auto point = view * QVector4D(x, y, z, 1);
                        const auto w = point.w();
                        const unsigned flags =
                            (point.x() < -w ? 1u : 0u) | (point.x() > w ? 2u : 0u) |
                            (point.y() < -w ? 4u : 0u) | (point.y() > w ? 8u : 0u) |
                            (point.z() < -w ? 16u : 0u) | (point.z() > w ? 32u : 0u);
                        outside &= flags;
                        if (outside == 0) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

    }  // namespace

    struct WorldViewport::Offscreen {
        QOpenGLContext context;
        QOffscreenSurface surface;
    };

    WorldViewport::WorldViewport(WorldDocument& document, QWidget* parent)
        : QOpenGLWidget(parent), m_document(document) {
        setMinimumSize(550, 400);
        setFocusPolicy(Qt::StrongFocus);
        setContextMenuPolicy(Qt::PreventContextMenu);
        setMouseTracking(true);
        m_available_chunks = m_document.coordinates();
        m_chunk_timer.setSingleShot(true);
        connect(&m_chunk_timer, &QTimer::timeout, this, &WorldViewport::process_chunk_queue);
        m_navigation_timer.setInterval(16);
        m_navigation_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_navigation_timer, &QTimer::timeout, this, &WorldViewport::update_navigation);
    }

    WorldViewport::~WorldViewport() {
        if (m_walk_mouse_captured) {
            releaseMouse();
        }
        if (context()) {
            QObject::disconnect(context(), nullptr, this, nullptr);
        }
        if (m_offscreen) {
            auto* previous = QOpenGLContext::currentContext();
            auto* surface = previous ? previous->surface() : nullptr;
            if (m_offscreen->context.makeCurrent(&m_offscreen->surface)) {
                destroy_renderer();
                m_offscreen->context.doneCurrent();
                if (previous && surface) {
                    previous->makeCurrent(surface);
                }
            }
        } else {
            release_gl();
        }
    }

    QImage WorldViewport::render_map(const QRectF& area, QSize size) {
        if (area.isEmpty() || size.isEmpty() || context() || isVisible()) {
            return {};
        }
        if (!m_offscreen_attempted) {
            m_offscreen_attempted = true;
            auto target = std::make_unique<Offscreen>();
            QSurfaceFormat format;
            format.setVersion(3, 3);
            format.setProfile(QSurfaceFormat::CoreProfile);
            target->context.setFormat(format);
            if (target->context.create() && !target->context.isOpenGLES()) {
                target->surface.setFormat(target->context.format());
                target->surface.create();
                if (target->surface.isValid()) {
                    m_offscreen = std::move(target);
                }
            }
        }
        if (!m_offscreen) {
            return {};
        }

        // Include objects whose geometry crosses the boundary of the view.
        constexpr float margin = 768.0f;
        const auto props = area.adjusted(-margin, -margin, margin, margin);
        const auto first = WorldDocument::coordinate_at(props.left(), props.top());
        const auto last = WorldDocument::coordinate_at(props.right(), props.bottom());
        m_visible.clear();
        for (const auto& cell : m_available_chunks) {
            if (cell.x() >= first.x() && cell.x() <= last.x() && cell.y() >= first.y() &&
                cell.y() <= last.y() && m_document.load(cell)) {
                m_visible.push_back(cell);
            }
        }
        m_document.evict_distant(m_visible);
        if (m_visible.empty()) {
            return {};
        }

        auto* previous = QOpenGLContext::currentContext();
        auto* previous_surface = previous ? previous->surface() : nullptr;
        if (!m_offscreen->context.makeCurrent(&m_offscreen->surface)) {
            return {};
        }
        if (!m_program) {
            initialize_renderer();
        }
        QImage image;
        if (m_program) {
            QOpenGLFramebufferObject frame(size, QOpenGLFramebufferObject::CombinedDepthStencil);
            if (frame.isValid() && frame.bind()) {
                m_map_area = props;
                m_target = {static_cast<float>(area.center().x()),
                            0,
                            static_cast<float>(area.center().y())};
                m_terrain_step = 1;
                glViewport(0, 0, size.width(), size.height());
                QMatrix4x4 view;
                view.scale(-1, 1, 1);
                view.lookAt(eye(), m_target, {0, 0, 1});
                QMatrix4x4 projection;
                projection.ortho(-area.width() / 2,
                                 area.width() / 2,
                                 -area.height() / 2,
                                 area.height() / 2,
                                 1,
                                 150000);
                render_scene(projection * view, true);
                image = frame.toImage();
            }
        }
        m_offscreen->context.doneCurrent();
        if (previous && previous_surface) {
            previous->makeCurrent(previous_surface);
        }
        return image;
    }

    void WorldViewport::set_brush(Brush value) {
        if (value.tool != m_brush.tool) {
            finish_edit();
        }
        m_brush = std::move(value);
        update();
    }

    void WorldViewport::set_preview(Preview scene, bool recenter) {
        const bool changed_environment = m_preview && m_preview->outdoor != scene.outdoor;
        if (!m_preview || recenter) {
            m_frame_preview = true;
            m_yaw = 0.45f - scene.yaw;
            m_pitch = 0.33f;
        } else {
            m_target += scene.position - m_preview->position;
        }
        if (changed_environment && !scene.outdoor) {
            m_chunk_timer.stop();
            m_chunk_queue.clear();
            m_requested_chunks.clear();
            m_visible.clear();
        }
        m_preview_key = "preview:" + scene.skeleton + '|' + scene.motion;
        for (const auto& path : scene.parts) {
            m_preview_key += '|' + path;
        }
        m_preview = std::move(scene);
        update();
    }

    void WorldViewport::clear_preview() {
        set_preview({}, false);
        m_frame_preview = false;
    }

    bool WorldViewport::preview_model_available() const {
        const auto found = m_models.find(m_preview_key);
        return m_preview && found != m_models.end() &&
               found->second.geometry == ModelGeometry::Mesh;
    }

    void WorldViewport::focus_chunk(QPoint coordinate) {
        finish_edit();
        set_walking(false);
        m_orbit_pivot.reset();
        load_neighborhood(coordinate);
        const auto* chunk = m_document.chunk(coordinate);
        const float y = chunk ? chunk->terrain.heights[(kSamples / 2) * (kSamples + 1)] : 0;
        m_target = chunk_center(coordinate, y);
        m_distance = kExtent * 1.9f;
        m_hover.reset();
        update_visible_chunks(camera());
        clear_selection();
    }

    void WorldViewport::load_neighborhood(QPoint coordinate) {
        m_active = coordinate;
        for (int z = coordinate.y() - 1; z <= coordinate.y() + 1; ++z) {
            for (int x = coordinate.x() - 1; x <= coordinate.x() + 1; ++x) {
                (void) m_document.load({x, z});
            }
        }
    }

    void WorldViewport::fit_world() {
        if (m_available_chunks.empty()) {
            return;
        }
        finish_edit();
        set_walking(false);
        stop_navigation();
        QPoint minimum = m_available_chunks.front();
        QPoint maximum = minimum;
        for (const auto coordinate : m_available_chunks) {
            minimum.setX(std::min(minimum.x(), coordinate.x()));
            minimum.setY(std::min(minimum.y(), coordinate.y()));
            maximum.setX(std::max(maximum.x(), coordinate.x()));
            maximum.setY(std::max(maximum.y(), coordinate.y()));
        }
        constexpr float half_height = 65535.0f / 2.0f;
        m_target = (chunk_center(minimum, half_height) + chunk_center(maximum, half_height)) / 2;
        const float half_x = (maximum.x() - minimum.x() + 1) * kExtent / 2;
        const float half_z = (maximum.y() - minimum.y() + 1) * kExtent / 2;
        const float radius = QVector3D(half_x, half_height, half_z).length();
        const float aspect = static_cast<float>(width()) / std::max(1, height());
        const float half_fov = std::atan(std::tan(24.0f * 0.0174532925f) * std::min(1.0f, aspect));
        m_distance = std::min(kMaxOrbitDistance, radius * 1.06f / std::sin(half_fov));
        m_yaw = kDefaultYaw;
        m_pitch = 1.45f;
        m_hover.reset();
        setFocus();
        update();
    }

    void WorldViewport::update_visible_chunks(const QMatrix4x4& camera_matrix) {
        std::vector<QPoint> requested;
        std::vector<QPoint> visible;
        for (auto coordinate : m_available_chunks) {
            if (m_corrupt_chunks.contains(coordinate)) {
                continue;
            }
            const auto key = std::make_pair(coordinate.x(), coordinate.y());
            const auto* chunk = m_document.chunk(coordinate);
            auto known = m_chunk_heights.find(key);
            if (chunk && (known == m_chunk_heights.end() ||
                          known->second.revision != chunk->height_revision)) {
                const auto& heights = chunk->terrain.heights;
                const auto [low, high] = std::minmax_element(heights.begin(), heights.end());
                const HeightBounds bounds{
                    chunk->height_revision,
                    static_cast<float>(*low),
                    std::max(static_cast<float>(*high) + kCell * 8, map::kWaterPosY)};
                known = m_chunk_heights.insert_or_assign(key, bounds).first;
            }
            const auto bounds = known == m_chunk_heights.end() ? HeightBounds{} : known->second;
            if (intersects_view(camera_matrix, coordinate, bounds.minimum, bounds.maximum)) {
                requested.push_back(coordinate);
                if (chunk) {
                    visible.push_back(coordinate);
                }
            }
        }

        if (requested != m_requested_chunks) {
            m_requested_chunks = std::move(requested);
            auto retained = m_requested_chunks;
            retained.push_back(m_active);
            m_document.evict_distant(retained);
            std::vector<QPoint> pending;
            for (auto coordinate : m_requested_chunks) {
                if (!m_document.chunk(coordinate)) {
                    pending.push_back(coordinate);
                }
            }
            const auto center = m_walking ? m_walking_feet : m_target;
            std::stable_sort(pending.begin(), pending.end(), [&](QPoint left, QPoint right) {
                return (chunk_center(left) - center).lengthSquared() <
                       (chunk_center(right) - center).lengthSquared();
            });
            m_chunk_queue.assign(pending.begin(), pending.end());
            if (!m_chunk_queue.empty() && !m_chunk_timer.isActive()) {
                m_chunk_timer.start(0);
            }
        }
        if (visible != m_visible) {
            m_visible = std::move(visible);
            if (m_selected && !selection().first) {
                clear_selection();
            }
            if (!m_walking && underMouse()) {
                m_hover = ground_hit(pointer_ray(mapFromGlobal(QCursor::pos())));
            }
            if (on_visible_chunks) {
                on_visible_chunks();
            }
        }
    }

    void WorldViewport::process_chunk_queue() {
        for (int i = 0; i < 3 && !m_chunk_queue.empty(); ++i) {
            const auto coordinate = m_chunk_queue.front();
            m_chunk_queue.pop_front();
            if (!m_document.load(coordinate)) {
                m_corrupt_chunks.insert(coordinate);
            }
        }
        update();
        if (!m_chunk_queue.empty()) {
            m_chunk_timer.start(12);
        }
    }

    bool WorldViewport::shows_objects(QPoint coordinate) const {
        if (!m_map_area.isEmpty()) {
            return true;
        }
        if (!m_walking && m_distance > 6 * kExtent) {
            return false;
        }
        const auto center = m_walking ? m_walking_feet : m_target;
        const auto difference = coordinate - WorldDocument::coordinate_at(center.x(), center.z());
        return std::max(std::abs(difference.x()), std::abs(difference.y())) <= 1;
    }

    void WorldViewport::set_walking(bool enabled) {
        if (enabled == m_walking) {
            return;
        }
        stop_navigation();
        if (enabled) {
            auto start = m_target;
            auto height = ground_height(start);
            if (!height && m_document.chunk(m_active)) {
                start = chunk_center(m_active);
                height = ground_height(start);
            }
            if (!height) {
                if (on_walk_mode) {
                    on_walk_mode(false, false);
                }
                if (on_message) {
                    on_message("Load an outdoor chunk to enter Walk Mode.");
                }
                return;
            }

            finish_edit();
            m_walking = true;
            start.setY(*height);
            m_walking_pitch = 0;
            set_walk_position(start);
            m_hover.reset();
            clear_selection();
            setFocus();
            // Qt cannot warp the global pointer on Wayland.
            if (!QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive)) {
                grabMouse(QCursor(Qt::BlankCursor));
                m_walk_mouse_captured = mouseGrabber() == this;
                if (m_walk_mouse_captured) {
                    QCursor::setPos(mapToGlobal(rect().center()));
                }
            }
        } else {
            m_walking = false;
            m_target = m_walking_feet;
            if (m_walk_mouse_captured) {
                m_walk_mouse_captured = false;
                releaseMouse();
            }
        }
        if (on_walk_mode) {
            on_walk_mode(m_walking, m_walk_mouse_captured);
        }
        update();
    }

    void WorldViewport::set_invert_mouse_y(bool enabled) {
        m_invert_mouse_y = enabled;
    }

    void WorldViewport::set_water_visible(bool visible) {
        m_water_visible = visible;
        update();
    }

    void WorldViewport::set_invisible_placements_visible(bool visible) {
        m_invisible_placements_visible = visible;
        if (const auto* placement = selection().first) {
            const auto found = m_models.find(placement->object.name);
            if (found != m_models.end() && !shows_model(found->second)) {
                clear_selection();
            }
        }
        update();
    }

    void WorldViewport::reset_scene() {
        finish_edit();
        set_walking(false);
        m_orbit_pivot.reset();
        m_chunk_timer.stop();
        m_chunk_queue.clear();
        m_requested_chunks.clear();
        m_corrupt_chunks.clear();
        m_chunk_heights.clear();
        m_available_chunks = m_document.coordinates();
        m_visible.clear();
        m_active = {-1, -1};
        m_hover.reset();
        m_scene_reset = true;
        clear_selection();
    }

    void WorldViewport::clear_selection() {
        select(0);
    }

    void WorldViewport::remove_selection() {
        finish_edit();
        if (m_selected && m_document.remove(m_selected)) {
            m_document.end_edit();
            clear_selection();
        }
    }

    void WorldViewport::set_selection_height(float height) {
        finish_edit();
        const auto [placement, coordinate] = selection();
        if (placement) {
            auto position = WorldDocument::position(coordinate, placement->object);
            position.setY(height);
            if (m_document.move(m_selected, position)) {
                m_document.end_edit();
                select(m_selected);
            }
        }
    }

    void WorldViewport::finish_edit() {
        end_pointer_edit(true);
    }

    void WorldViewport::end_pointer_edit(bool commit) {
        m_left_press.reset();
        if (m_editing) {
            if (commit) {
                m_document.end_edit();
            } else {
                m_document.cancel_edit();
                report_selection();
                update();
            }
            m_editing = false;
            m_dragging = false;
            m_last_stamp.reset();
            if (on_change) {
                on_change();
            }
        }
    }

    QString WorldViewport::placement_warning() const {
        QStringList failed;
        for (const auto coordinate : m_visible) {
            if (!m_document.chunk(coordinate)->objects_readable) {
                failed.append(WorldDocument::stem(coordinate));
            }
        }
        return failed.isEmpty() ? QString()
                                : QString("Unreadable placement files: %1.").arg(failed.join(", "));
    }

    QVector3D WorldViewport::eye() const {
        if (!m_map_area.isEmpty()) {
            return m_target + QVector3D(0, 80000, 0);
        }
        if (m_walking) {
            return m_walking_feet + QVector3D(0, kWalkEyeHeight, 0);
        }
        const float horizontal = std::cos(m_pitch) * m_distance;
        return m_target + QVector3D(std::sin(m_yaw) * horizontal,
                                    std::sin(m_pitch) * m_distance,
                                    -std::cos(m_yaw) * horizontal);
    }

    QVector3D WorldViewport::view_direction() const {
        const float pitch = m_walking ? m_walking_pitch : -m_pitch;
        const float horizontal = std::cos(pitch);
        return {-std::sin(m_yaw) * horizontal, std::sin(pitch), std::cos(m_yaw) * horizontal};
    }

    void WorldViewport::pan_camera(float right, float forward) {
        const float sine = std::sin(m_yaw);
        const float cosine = std::cos(m_yaw);
        const auto offset =
            right * QVector3D(cosine, 0, sine) + forward * QVector3D(-sine, 0, cosine);
        if (m_walking) {
            walk_camera(offset);
        } else {
            m_target += offset;
        }
    }

    void WorldViewport::drag_camera(QPointF from, QPointF to) {
        if (const auto anchor = ground_hit(pointer_ray(from))) {
            if (const auto current = plane_hit(pointer_ray(to), anchor->y())) {
                const auto offset = *anchor - *current;
                // Near the horizon, a small mouse movement can project far beyond the map.
                if (offset.length() <= m_distance) {
                    m_target += offset;
                    return;
                }
            }
        }
        const auto delta = to - from;
        const float speed = m_distance / std::max(1, height());
        pan_camera(-speed * static_cast<float>(delta.x()),
                   speed * static_cast<float>(delta.y()) / std::sin(m_pitch));
    }

    void WorldViewport::orbit_camera(QPointF delta) {
        const auto pivot = m_orbit_pivot.value_or(m_target);
        const auto offset_in_view = view_transform().mapVector(m_target - pivot);
        const float vertical = m_invert_mouse_y ? -1.0f : 1.0f;
        m_yaw += static_cast<float>(delta.x()) * 0.006f;
        m_pitch =
            std::clamp(m_pitch + vertical * static_cast<float>(delta.y()) * 0.005f, 0.13f, 1.55f);

        // Rotate the camera target around the pivot with the same change in view orientation.
        m_target = pivot + view_transform().inverted().mapVector(offset_in_view);
    }

    void WorldViewport::zoom_camera(QPointF anchor, float scale) {
        if (m_preview) {
            m_distance = std::clamp(m_distance * scale, 3.0f, 900.0f);
            update();
            return;
        }
        const float distance = std::clamp(m_distance * scale, 90.0f, kMaxOrbitDistance);
        if (m_walking || m_editing || m_left_press || m_available_chunks.empty() ||
            distance == m_distance) {
            return;
        }
        const auto point = ground_hit(pointer_ray(anchor));
        m_distance = distance;
        if (point) {
            if (const auto current = plane_hit(pointer_ray(anchor), point->y())) {
                m_target += *point - *current;
            }
        }
        m_hover = underMouse() ? ground_hit(pointer_ray(anchor)) : std::nullopt;
        update();
    }

    QPointF WorldViewport::zoom_anchor() const {
        const auto pointer = mapFromGlobal(QCursor::pos());
        return rect().contains(pointer) ? pointer : rect().center();
    }

    std::optional<float> WorldViewport::ground_height(QVector3D position) const {
        const auto* chunk =
            m_document.chunk(WorldDocument::coordinate_at(position.x(), position.z()));
        if (!chunk) {
            return std::nullopt;
        }
        // Use the same triangle split as the editor rendering and picking.
        constexpr float start = 65536.0f;
        position.setY(start);
        const auto distance = intersect_terrain(chunk->terrain, position, {0, -1, 0}, start + 1);
        return distance <= start ? std::optional<float>(start - distance) : std::nullopt;
    }

    void WorldViewport::walk_camera(QVector3D offset) {
        auto next = m_walking_feet + offset;
        const auto height = ground_height(next);
        const auto max_step = std::max(3.0f, offset.length() * 1.25f);
        if (!height || std::abs(*height - m_walking_feet.y()) > max_step) {
            return;
        }
        next.setY(*height);
        set_walk_position(next);
    }

    void WorldViewport::set_walk_position(QVector3D position) {
        m_walking_feet = position;
        const auto coordinate = WorldDocument::coordinate_at(position.x(), position.z());
        if (coordinate != m_active) {
            load_neighborhood(coordinate);
            if (on_walk_chunk) {
                on_walk_chunk(coordinate);
            }
        }
    }

    void WorldViewport::update_navigation() {
        const auto seconds = std::min(0.05f, m_navigation_clock.restart() / 1000.0f);
        if (m_editing || m_left_press || m_available_chunks.empty()) {
            return;
        }
        const bool w = m_navigation_keys.contains(Qt::Key_W);
        const bool s = m_navigation_keys.contains(Qt::Key_S);
        if (!m_walking && w != s) {
            const float direction = w ? -1.0f : 1.0f;
            zoom_camera(zoom_anchor(), std::exp(direction * 1.7f * seconds));
        }
        const int right =
            m_navigation_keys.contains(Qt::Key_D) - m_navigation_keys.contains(Qt::Key_A);
        const int forward = m_walking ? w - s : 0;
        if (!right && !forward) {
            return;
        }
        const float speed =
            m_walking ? kWalkSpeed * (m_walk_fast ? kSprintMultiplier : 1.0f) : m_distance * 0.55f;
        const float step = speed * seconds / std::hypot(right, forward);
        pan_camera(right * step, forward * step);
        if (!m_walking && underMouse()) {
            m_hover = ground_hit(pointer_ray(mapFromGlobal(QCursor::pos())));
        }
        update();
    }

    void WorldViewport::stop_navigation() {
        m_navigation_keys.clear();
        m_navigation_timer.stop();
        m_walk_fast = false;
        m_orbit_pivot.reset();
    }

    QMatrix4x4 WorldViewport::view_transform() const {
        const auto position = eye();
        QMatrix4x4 view;
        // Match the client's camera-space X reflection.
        view.scale(-1.0f, 1.0f, 1.0f);
        view.lookAt(
            position, m_walking ? position + view_direction() * 128.0f : m_target, {0, 1, 0});
        return view;
    }

    QMatrix4x4 WorldViewport::camera() const {
        QMatrix4x4 projection;
        projection.perspective(m_walking ? 65.0f : 48.0f,
                               static_cast<float>(width()) / std::max(1, height()),
                               m_walking ? 1.0f : std::max(m_distance / 1800.0f, 1.0f),
                               view_far_distance());
        return projection * view_transform();
    }

    float WorldViewport::view_far_distance() const {
        return m_walking ? 3.0f * kExtent : std::max(4.0f * kExtent, 2.6f * m_distance);
    }

    WorldViewport::Ray WorldViewport::pointer_ray(QPointF position) const {
        const auto inverse = camera().inverted();
        const float x = static_cast<float>(2.0 * position.x() / std::max(1, width()) - 1.0);
        const float y = static_cast<float>(1.0 - 2.0 * position.y() / std::max(1, height()));
        const auto near = (inverse * QVector4D(x, y, -1, 1)).toVector3DAffine();
        return {near, (near - eye()).normalized()};
    }

    std::optional<QVector3D> WorldViewport::ground_hit(const Ray& ray) const {
        const float max_distance = view_far_distance();
        float nearest = max_distance;
        for (const auto coordinate : m_visible) {
            const auto& patch = m_document.chunk(coordinate)->terrain;
            nearest = intersect_terrain(patch, ray.origin, ray.direction, nearest);
        }
        if (nearest < max_distance) {
            return ray.origin + ray.direction * nearest;
        }
        return std::nullopt;
    }

    std::optional<QVector3D> WorldViewport::plane_hit(const Ray& ray, float height) {
        if (std::abs(ray.direction.y()) < 1e-5f) {
            return std::nullopt;
        }
        const float distance = (height - ray.origin.y()) / ray.direction.y();
        if (distance < 0.0f) {
            return std::nullopt;
        }
        auto point = ray.origin + ray.direction * distance;
        point.setY(height);
        return point;
    }

    WorldViewport::Pick WorldViewport::object_hit(const Ray& ray) const {
        Pick result;
        const auto ground = ground_hit(ray);
        float nearest = ground ? (*ground - ray.origin).length() + 0.5f : view_far_distance();
        for (const auto coordinate : m_visible) {
            if (!shows_objects(coordinate)) {
                continue;
            }
            for (const auto& placement : m_document.chunk(coordinate)->objects) {
                const auto& object = placement.object;
                const auto found = m_models.find(object.name);
                if (found == m_models.end() || !shows_model(found->second)) {
                    continue;
                }
                const auto world = WorldDocument::position(coordinate, object);
                bool invertible = false;
                const auto inverse = transform(world, object).inverted(&invertible);
                if (!invertible) {
                    continue;
                }
                float first = 0;
                float last = nearest;
                if (intersect_box(inverse.map(ray.origin),
                                  inverse.mapVector(ray.direction),
                                  found->second.minimum,
                                  found->second.maximum,
                                  first,
                                  last)) {
                    nearest = first;
                    result = {placement.id, world};
                }
            }
        }
        return result;
    }

    void WorldViewport::stamp(QVector3D center) {
        QString error;
        if (m_brush.tool == Tool::Texture && m_brush.material >= 0) {
            m_document.paint(center,
                             m_brush.radius,
                             m_brush.opacity,
                             static_cast<std::uint8_t>(m_brush.material),
                             error);
        } else if (m_brush.tool != Tool::Texture) {
            HeightMode mode = HeightMode::Raise;
            if (m_brush.tool == Tool::Lower) {
                mode = HeightMode::Lower;
            } else if (m_brush.tool == Tool::Flatten) {
                mode = HeightMode::Flatten;
            }
            m_document.sculpt(
                center, m_brush.radius, m_brush.height_step, mode, m_brush.level, error);
        }
        if (!error.isEmpty() && on_message) {
            on_message(error);
        }
    }

    void WorldViewport::select(std::uint64_t id) {
        m_selected = id;
        report_selection();
        if (on_change) {
            on_change();
        }
        update();
    }

    std::pair<const Placement*, QPoint> WorldViewport::selection() const {
        if (m_selected) {
            for (const auto coordinate : m_visible) {
                for (const auto& placement : m_document.chunk(coordinate)->objects) {
                    if (placement.id == m_selected) {
                        return {&placement, coordinate};
                    }
                }
            }
        }
        return {};
    }

    void WorldViewport::report_selection() {
        if (!on_selection) {
            return;
        }
        const auto [placement, coordinate] = selection();
        if (!placement) {
            on_selection({}, std::nullopt);
            return;
        }
        const auto p = WorldDocument::position(coordinate, placement->object);
        on_selection(QString("%1\n%2  ·  X %3  Z %4")
                         .arg(QString::fromStdString(placement->object.name),
                              WorldDocument::stem(coordinate))
                         .arg(p.x(), 0, 'f', 1)
                         .arg(p.z(), 0, 'f', 1),
                     p.y());
    }

    void WorldViewport::mousePressEvent(QMouseEvent* event) {
        m_last_mouse = event->position();
        setFocus(Qt::MouseFocusReason);
        if (m_preview) {
            return;
        }
        if (m_walking) {
            return;
        }
        if (event->button() == Qt::LeftButton && event->buttons() == Qt::LeftButton) {
            m_left_press = event->position();
        } else if (event->buttons().testFlag(Qt::LeftButton)) {
            end_pointer_edit(false);
        } else {
            finish_edit();
        }
        if (!m_orbit_pivot && event->buttons().testFlag(Qt::LeftButton) &&
            event->buttons().testFlag(Qt::RightButton)) {
            m_orbit_pivot = ground_hit(pointer_ray(event->position())).value_or(m_target);
        }
    }

    void WorldViewport::start_pointer_edit(QPointF position) {
        const auto ray = pointer_ray(position);
        m_hover = ground_hit(ray);
        if (m_brush.tool == Tool::Select) {
            const auto hit = object_hit(ray);
            select(hit.id);
            if (hit.id && m_hover) {
                m_drag_offset = hit.position - *m_hover;
                m_dragging = m_editing = true;
                m_document.begin_edit();
            }
        } else if (m_hover && m_brush.tool == Tool::Place) {
            const auto id = m_document.place(*m_hover, m_brush.asset);
            m_document.end_edit();
            select(id);
            if (!id && on_message) {
                on_message("Choose a catalog model and a chunk with a readable placement file.");
            }
        } else if (m_hover && terrain_tool(m_brush.tool)) {
            m_editing = true;
            m_last_stamp = m_hover;
            m_document.begin_edit();
            stamp(*m_hover);
        }
        update();
    }

    void WorldViewport::mouseMoveEvent(QMouseEvent* event) {
        const QPointF previous = m_last_mouse;
        QPointF delta = event->position() - previous;
        m_last_mouse = event->position();
        if (m_preview) {
            if (event->buttons().testFlag(Qt::LeftButton)) {
                orbit_camera(delta);
            } else if (event->buttons().testFlag(Qt::RightButton)) {
                const auto speed = m_distance / std::max(1, height());
                pan_camera(-speed * static_cast<float>(delta.x()),
                           speed * static_cast<float>(delta.y()));
            }
            update();
            return;
        }
        if (m_walking) {
            if (m_walk_mouse_captured) {
                const auto center = mapToGlobal(rect().center());
                delta = event->globalPosition().toPoint() - center;
                if (delta.isNull()) {
                    return;
                }
                QCursor::setPos(center);
            } else if (!event->buttons().testFlag(Qt::RightButton)) {
                return;
            }
            m_yaw -= static_cast<float>(delta.x()) * kLookSensitivity;
            const float vertical = m_invert_mouse_y ? -1.0f : 1.0f;
            m_walking_pitch = std::clamp(
                m_walking_pitch - vertical * static_cast<float>(delta.y()) * kLookSensitivity,
                -1.50f,
                1.50f);
            update();
            return;
        }
        const auto buttons = event->buttons();
        const bool right_drag = buttons.testFlag(Qt::RightButton);
        if (right_drag && buttons.testFlag(Qt::LeftButton)) {
            orbit_camera(delta);
        } else if (right_drag || buttons.testFlag(Qt::MiddleButton)) {
            drag_camera(previous, event->position());
        } else if (m_left_press && buttons == Qt::LeftButton &&
                   (event->position() - *m_left_press).manhattanLength() >=
                       QApplication::startDragDistance()) {
            const auto origin = *m_left_press;
            m_left_press.reset();
            if (m_brush.tool != Tool::Place) {
                start_pointer_edit(origin);
            }
        }

        m_hover = ground_hit(pointer_ray(event->position()));
        if (m_dragging && m_hover && event->buttons().testFlag(Qt::LeftButton)) {
            const auto target = *m_hover + m_drag_offset;
            const auto owner = WorldDocument::coordinate_at(target.x(), target.z());
            if (std::find(m_visible.begin(), m_visible.end(), owner) != m_visible.end() &&
                m_document.move(m_selected, target)) {
                report_selection();
            } else if (on_message) {
                on_message(
                    "Objects can only be moved into a visible chunk with a readable placement file.");
            }
        } else if (m_editing && !m_dragging && event->buttons().testFlag(Qt::LeftButton)) {
            if (!m_hover) {
                m_last_stamp.reset();
            } else if (!m_last_stamp) {
                stamp(*m_hover);
                m_last_stamp = m_hover;
            } else {
                const QVector3D offset = *m_hover - *m_last_stamp;
                const float length = std::hypot(offset.x(), offset.z());
                const float spacing = std::max(8.0f, m_brush.radius * 0.22f);
                const int count = static_cast<int>(length / spacing);
                // Large pointer jumps can cross a gap where there is no visible terrain.
                if (count > 64) {
                    stamp(*m_hover);
                    m_last_stamp = m_hover;
                } else if (count > 0) {
                    const auto start = *m_last_stamp;
                    const auto step = offset * (spacing / length);
                    for (int i = 1; i <= count; ++i) {
                        stamp(start + step * i);
                    }
                    m_last_stamp = start + step * count;
                }
            }
        }
        update();
    }

    void WorldViewport::mouseReleaseEvent(QMouseEvent* event) {
        m_last_mouse = event->position();
        if (m_preview) {
            return;
        }
        if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
            m_orbit_pivot.reset();
        }
        if (event->button() == Qt::LeftButton) {
            if (m_left_press && !event->buttons()) {
                start_pointer_edit(*m_left_press);
            }
            finish_edit();
        }
    }

    void WorldViewport::wheelEvent(QWheelEvent* event) {
        const float scale = std::pow(0.82f, static_cast<float>(event->angleDelta().y()) / 120.0f);
        zoom_camera(event->position(), scale);
    }

    void WorldViewport::leaveEvent(QEvent*) {
        m_hover.reset();
        m_last_stamp.reset();
        update();
    }

    void WorldViewport::keyPressEvent(QKeyEvent* event) {
        if (m_walking && event->key() == Qt::Key_Escape) {
            set_walking(false);
            event->accept();
            return;
        }
        if (m_walking && event->key() == Qt::Key_Shift) {
            m_walk_fast = true;
            event->accept();
            return;
        }
        constexpr auto shortcuts = Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
        if (!navigation_key(event->key()) || (event->modifiers() & shortcuts)) {
            QOpenGLWidget::keyPressEvent(event);
            return;
        }
        if (!event->isAutoRepeat()) {
            if (m_walking) {
                m_walk_fast = m_walk_fast || event->modifiers().testFlag(Qt::ShiftModifier);
            } else if (event->key() == Qt::Key_W && !m_navigation_keys.contains(Qt::Key_S)) {
                zoom_camera(zoom_anchor(), 0.9f);
            } else if (event->key() == Qt::Key_S && !m_navigation_keys.contains(Qt::Key_W)) {
                zoom_camera(zoom_anchor(), 1.0f / 0.9f);
            }
            m_navigation_keys.insert(event->key());
            if (!m_navigation_timer.isActive()) {
                m_navigation_clock.start();
                m_navigation_timer.start();
            }
        }
        event->accept();
    }

    void WorldViewport::keyReleaseEvent(QKeyEvent* event) {
        if (m_walking && event->key() == Qt::Key_Shift) {
            if (!event->isAutoRepeat()) {
                m_walk_fast = false;
            }
            event->accept();
            return;
        }
        if (!m_navigation_keys.contains(event->key())) {
            QOpenGLWidget::keyReleaseEvent(event);
            return;
        }
        if (!event->isAutoRepeat()) {
            m_navigation_keys.remove(event->key());
            if (m_navigation_keys.isEmpty()) {
                m_navigation_timer.stop();
            }
        }
        event->accept();
    }

    void WorldViewport::focusOutEvent(QFocusEvent* event) {
        finish_edit();
        stop_navigation();
        set_walking(false);
        QOpenGLWidget::focusOutEvent(event);
    }

    void WorldViewport::initializeGL() {
        connect(context(),
                &QOpenGLContext::aboutToBeDestroyed,
                this,
                &WorldViewport::release_gl,
                Qt::DirectConnection);
        initialize_renderer();
    }

    void WorldViewport::initialize_renderer() {
        if (!initializeOpenGLFunctions()) {
            if (on_message) {
                on_message("The viewport requires an OpenGL 3.3 core context.");
            }
            return;
        }

        auto program = std::make_unique<QOpenGLShaderProgram>();
        if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) ||
            !program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader) ||
            !program->link()) {
            if (on_message) {
                on_message(QString("Could not initialize the 3D viewport: %1").arg(program->log()));
            }
            return;
        }
        m_program = std::move(program);
        terrain::ImageRgba white;
        white.width = white.height = 1;
        white.rgba = {255, 255, 255, 255};
        m_white = upload_texture(white, false);

        constexpr float marker[] = {-36, 0,   -36, 0,   0,  36, 0,   -36, 1,   0,  36, 0,  36,  1,
                                    1,   -36, 0,   36,  0,  1,  -36, 100, -36, 0,  0,  36, 100, -36,
                                    1,   0,   36,  100, 36, 1,  1,   -36, 100, 36, 0,  1};
        const std::vector<std::uint32_t> indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                    0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
                                                    2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
        upload_mesh(m_marker, marker, std::size(marker), indices);

        constexpr float water_plane[] = {
            0,       map::kWaterPosY, 0,       0, 0, kExtent, map::kWaterPosY, 0,       0, 0,
            kExtent, map::kWaterPosY, kExtent, 0, 0, 0,       map::kWaterPosY, kExtent, 0, 0,
        };
        upload_mesh(m_water, water_plane, std::size(water_plane), {0, 1, 2, 0, 2, 3});
    }

    void WorldViewport::release_gl() {
        if (!m_program || !isValid()) {
            return;
        }
        makeCurrent();
        destroy_renderer();
        doneCurrent();
    }

    void WorldViewport::destroy_renderer() {
        clear_scene_gl();
        free_mesh(m_marker);
        free_mesh(m_water);
        free_texture(m_white);
        m_program.reset();
    }

    void WorldViewport::clear_scene_gl() {
        free_texture(m_water_texture);
        for (auto& [key, chunk] : m_terrain) {
            free_mesh(chunk.mesh);
            for (auto& texture : chunk.maps) {
                free_texture(texture);
            }
        }
        m_terrain.clear();
        for (auto& [key, detail] : m_details) {
            free_texture(detail.texture);
        }
        m_details.clear();
        for (auto& [name, model] : m_models) {
            for (auto& part : model.parts) {
                free_mesh(part.mesh);
                free_texture(part.texture);
            }
        }
        m_models.clear();
    }

    void WorldViewport::free_mesh(Mesh& mesh) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vertices);
        glDeleteBuffers(1, &mesh.indices);
        mesh = {};
    }

    void WorldViewport::free_texture(GLuint& texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    void WorldViewport::draw_mesh(const Mesh& mesh) {
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
    }

    void WorldViewport::upload_mesh(Mesh& mesh,
                                    const float* vertices,
                                    std::size_t float_count,
                                    const std::vector<std::uint32_t>& indices,
                                    bool dynamic) {
        const bool create = mesh.vao == 0;
        if (create) {
            glGenVertexArrays(1, &mesh.vao);
            glGenBuffers(1, &mesh.vertices);
            glGenBuffers(1, &mesh.indices);
        }
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
        glBufferData(GL_ARRAY_BUFFER,
                     float_count * sizeof(float),
                     vertices,
                     dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        if (create) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         indices.size() * sizeof(std::uint32_t),
                         indices.data(),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  5 * sizeof(float),
                                  reinterpret_cast<void*>(3 * sizeof(float)));
            mesh.count = static_cast<GLsizei>(indices.size());
        }
        glBindVertexArray(0);
    }

    GLuint WorldViewport::upload_texture(const terrain::ImageRgba& image, bool repeat) {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     image.width,
                     image.height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     image.rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, repeat ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        if (repeat) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        return texture;
    }

    WorldViewport::Detail WorldViewport::detail(std::uint8_t material) {
        const auto found = m_details.find(material);
        if (found != m_details.end()) {
            return found->second;
        }
        terrain::ImageRgba image;
        Detail result;
        const auto& table = m_document.layers();
        if (material < table.size()) {
            const auto& layer = table[material];
            result.repeat = static_cast<float>(std::max(1, layer.scale_u));
            for (const char* extension : {".gtx", ".dds"}) {
                if (m_document.textures().isEmpty()) {
                    break;
                }
                const auto file = platform::resolve_path_case(m_document.textures().toStdString(),
                                                              layer.texture_name + extension);
                if (platform::file_exists(file) && terrain::load_gtx_rgba(file, image)) {
                    break;
                }
            }
        }
        if (image.rgba.empty()) {
            image.width = image.height = 1;
            image.rgba = {128, 128, 128, 255};
        }
        result.texture = upload_texture(image, true);
        return m_details.emplace(material, result).first->second;
    }

    const WorldViewport::Model& WorldViewport::model(const std::string& name) {
        auto [entry, inserted] = m_models.try_emplace(name);
        auto& result = entry->second;
        if (!inserted) {
            return result;
        }
        result.minimum = {-36, 0, -36};
        result.maximum = {36, 100, 36};

        const auto path =
            QFileInfo(QString::fromStdString(name)).isAbsolute()
                ? name
                : platform::resolve_client_asset(m_document.root().toStdString(), name, ".gb");
        if (path.empty()) {
            result.geometry = ModelGeometry::Invisible;
        } else {
            add_model_parts(result, path);
        }
        return result;
    }

    const WorldViewport::Model& WorldViewport::preview_model(const std::string& key) {
        auto [entry, inserted] = m_models.try_emplace(key);
        if (inserted) {
            terrain::GbModel skeleton;
            terrain::GbModel motion;
            const bool rigged = !m_preview->skeleton.empty() && !m_preview->motion.empty() &&
                                terrain::load_gb(m_preview->skeleton, skeleton) &&
                                terrain::load_gb(m_preview->motion, motion);
            for (const auto& path : m_preview->parts) {
                add_model_parts(
                    entry->second, path, rigged ? &skeleton : nullptr, rigged ? &motion : nullptr);
            }
        }
        return entry->second;
    }

    void WorldViewport::add_model_parts(Model& result,
                                        const std::string& path,
                                        const terrain::GbModel* skeleton,
                                        const terrain::GbModel* motion) {
        terrain::GbModel source;
        if (!platform::file_exists(path) || !terrain::load_gb(path, source)) {
            return;
        }
        if (result.geometry == ModelGeometry::Unavailable) {
            result.geometry = ModelGeometry::Invisible;
        }
        if (!skeleton || !motion ||
            !terrain::GbSkin::skin_with_skeleton_anim(source, *skeleton, *motion)) {
            terrain::GbSkin::apply_embedded_idle(source);
        }
        for (const auto& mesh : source.meshes) {
            if (mesh.pos_uv.size() < 15 || mesh.indices.empty()) {
                continue;
            }
            auto& part = result.parts.emplace_back();
            upload_mesh(part.mesh, mesh.pos_uv.data(), mesh.pos_uv.size(), mesh.indices);
            if (mesh.material_index >= 0 &&
                static_cast<std::size_t>(mesh.material_index) < source.materials.size()) {
                const auto& material = source.materials[mesh.material_index];
                terrain::ImageRgba texture;
                if (!material.texture.empty() &&
                    terrain::load_gb_texture(path, material.texture, texture)) {
                    part.texture = upload_texture(texture, true);
                }
            }
            part.blend = mesh.alpha_blend;
            part.cutout = mesh.alpha_test;
            for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                const QVector3D vertex(mesh.pos_uv[i], mesh.pos_uv[i + 1], mesh.pos_uv[i + 2]);
                if (result.geometry != ModelGeometry::Mesh) {
                    result.minimum = result.maximum = vertex;
                    result.geometry = ModelGeometry::Mesh;
                } else {
                    for (int axis = 0; axis < 3; ++axis) {
                        result.minimum[axis] = std::min(result.minimum[axis], vertex[axis]);
                        result.maximum[axis] = std::max(result.maximum[axis], vertex[axis]);
                    }
                }
            }
        }
        if (result.geometry == ModelGeometry::Mesh) {
            // Flat geometry still needs a usable picking volume.
            for (int axis = 0; axis < 3; ++axis) {
                if (result.maximum[axis] - result.minimum[axis] < 2) {
                    result.minimum[axis] -= 1;
                    result.maximum[axis] += 1;
                }
            }
        }
    }

    void WorldViewport::frame_preview() {
        const auto& asset = preview_model(m_preview_key);
        const auto& scene = *m_preview;
        m_target = scene.position + QVector3D(0, 8, 0);
        m_distance = 48;
        if (asset.geometry == ModelGeometry::Mesh) {
            const auto center_y = (asset.minimum.y() + asset.maximum.y()) * scene.scale * 0.5f;
            m_target.setY(scene.position.y() + center_y);
            m_distance = std::clamp(
                (asset.maximum - asset.minimum).length() * scene.scale * 1.65f, 16.0f, 650.0f);
        } else if (on_message) {
            on_message("No renderable model was found for this selection.");
        }
        m_frame_preview = false;
    }

    bool WorldViewport::shows_model(const Model& model) const {
        return model.geometry != ModelGeometry::Invisible || m_invisible_placements_visible;
    }

    void WorldViewport::update_terrain(TerrainGpu& gpu, const WorldChunk& chunk) {
        const auto& patch = chunk.terrain;
        const bool resolution_changed = gpu.sample_step != m_terrain_step;
        if (resolution_changed && gpu.mesh.vao) {
            free_mesh(gpu.mesh);
        }
        if (!gpu.mesh.vao || gpu.height_revision != chunk.height_revision) {
            std::vector<terrain::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            patch.build_mesh(vertices, indices, m_terrain_step);
            std::vector<float> packed;
            packed.reserve(vertices.size() * 5);
            for (const auto& vertex : vertices) {
                packed.insert(
                    packed.end(),
                    {vertex.position.x, vertex.position.y, vertex.position.z, vertex.u, vertex.v});
            }
            upload_mesh(gpu.mesh, packed.data(), packed.size(), indices, true);
            gpu.height_revision = chunk.height_revision;
            gpu.below_water = std::any_of(patch.heights.begin(),
                                          patch.heights.end(),
                                          [](auto height) { return height < map::kWaterPosY; });
        }
        if (resolution_changed || gpu.maps.empty() ||
            gpu.texture_revision != chunk.texture_revision) {
            for (auto& texture : gpu.maps) {
                free_texture(texture);
            }
            gpu.maps.clear();
            constexpr auto source_cells = kSide * kSide;
            const int side = kSide / m_terrain_step;
            const int cells = side * side;
            const auto source_cell = [this, side](int cell) {
                const int row = cell / side * m_terrain_step + m_terrain_step / 2;
                const int col = cell % side * m_terrain_step + m_terrain_step / 2;
                return row * kSide + col;
            };
            terrain::ImageRgba color;
            color.width = color.height = side;
            color.rgba.resize(cells * 4, 255);
            for (int cell = 0; cell < cells; ++cell) {
                const int source = source_cell(cell) * 3;
                color.rgba[cell * 4] = patch.color_rgb[source + 2];
                color.rgba[cell * 4 + 1] = patch.color_rgb[source + 1];
                color.rgba[cell * 4 + 2] = patch.color_rgb[source];
            }
            gpu.maps.push_back(upload_texture(color, false));
            for (std::size_t i = 1; i < patch.layers.size(); ++i) {
                for (int cell = 0; cell < cells; ++cell) {
                    color.rgba[cell * 4 + 3] =
                        patch.alphas[(i - 1) * source_cells + source_cell(cell)];
                }
                gpu.maps.push_back(upload_texture(color, false));
            }
            gpu.texture_revision = chunk.texture_revision;
        }
        gpu.sample_step = m_terrain_step;
    }

    void WorldViewport::draw_terrain(const QMatrix4x4& camera_matrix) {
        m_program->setUniformValue("uCamera", camera_matrix);
        m_program->setUniformValue("uModel", QMatrix4x4());
        m_program->setUniformValue("uTerrain", 1);
        m_program->setUniformValue("uCutout", 0);
        const bool ring = m_hover && terrain_tool(m_brush.tool);
        m_program->setUniformValue("uRadius", ring ? m_brush.radius : 0.0f);
        if (ring) {
            m_program->setUniformValue("uBrush", QVector2D(m_hover->x(), m_hover->z()));
        }

        std::set<std::pair<int, int>> visible;
        for (const auto coordinate : m_visible) {
            const auto key = std::make_pair(coordinate.x(), coordinate.y());
            visible.insert(key);
            const auto& chunk = *m_document.chunk(coordinate);
            auto& gpu = m_terrain[key];
            glActiveTexture(GL_TEXTURE0);
            update_terrain(gpu, chunk);
            m_program->setUniformValue("uActive", coordinate == m_active ? 1 : 0);
            for (std::size_t i = 0; i < gpu.maps.size(); ++i) {
                glActiveTexture(GL_TEXTURE1);
                const auto surface = detail(chunk.terrain.layers[i]);
                glBindTexture(GL_TEXTURE_2D, surface.texture);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gpu.maps[i]);
                glDepthMask(i == 0 ? GL_TRUE : GL_FALSE);
                if (i == 0) {
                    glDisable(GL_BLEND);
                } else {
                    glEnable(GL_BLEND);
                }
                m_program->setUniformValue("uBase", i == 0 ? 1 : 0);
                m_program->setUniformValue("uRepeat", surface.repeat);
                draw_mesh(gpu.mesh);
            }
        }
        for (auto it = m_terrain.begin(); it != m_terrain.end();) {
            if (!visible.count(it->first)) {
                free_mesh(it->second.mesh);
                for (auto& texture : it->second.maps) {
                    free_texture(texture);
                }
                it = m_terrain.erase(it);
            } else {
                ++it;
            }
        }
    }

    void WorldViewport::draw_objects(const QMatrix4x4& camera_matrix) {
        struct Draw {
            const Part* part;
            QMatrix4x4 transform;
            float depth;
            bool selected;
        };
        std::vector<Draw> transparent;
        std::set<std::string> visible_models;
        m_program->setUniformValue("uCamera", camera_matrix);
        m_program->setUniformValue("uTerrain", 0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glActiveTexture(GL_TEXTURE0);

        const auto eye_position = eye();
        const auto view_forward = view_direction();
        const auto draw = [&](const Part& part, const QMatrix4x4& matrix, bool selected) {
            m_program->setUniformValue("uModel", matrix);
            m_program->setUniformValue("uSelected", selected ? 1 : 0);
            m_program->setUniformValue("uCutout", part.cutout ? 1 : 0);
            glBindTexture(GL_TEXTURE_2D, part.texture ? part.texture : m_white);
            draw_mesh(part.mesh);
        };
        const auto enqueue = [&](const Model& resource,
                                 const QMatrix4x4& matrix,
                                 QVector3D position,
                                 bool selected) {
            for (const auto& part : resource.parts) {
                if (part.blend) {
                    transparent.push_back(
                        {&part,
                         matrix,
                         QVector3D::dotProduct(position - eye_position, view_forward),
                         selected});
                } else {
                    draw(part, matrix, selected);
                }
            }
        };
        for (const auto coordinate : m_visible) {
            if (!shows_objects(coordinate)) {
                continue;
            }
            for (const auto& placement : m_document.chunk(coordinate)->objects) {
                const auto& object = placement.object;
                const auto position = WorldDocument::position(coordinate, object);
                if (!std::isfinite(position.x()) || !std::isfinite(position.y()) ||
                    !std::isfinite(position.z())) {
                    continue;
                }
                constexpr float preview_radius = 1200;
                if (m_preview && (position - m_preview->position).lengthSquared() >
                                     preview_radius * preview_radius) {
                    continue;
                }
                if (!m_map_area.isEmpty() && !m_map_area.contains(position.x(), position.z())) {
                    continue;
                }
                visible_models.insert(object.name);
                const auto& resource = model(object.name);
                if (!shows_model(resource)) {
                    continue;
                }
                const auto matrix = transform(position, object);
                const bool selected = placement.id == m_selected;
                if (resource.geometry == ModelGeometry::Mesh) {
                    enqueue(resource, matrix, position, selected);
                } else {
                    draw(Part{m_marker, 0, false, false}, matrix, selected);
                }
            }
        }
        if (m_preview) {
            for (const auto& object : m_preview->surroundings) {
                visible_models.insert(object.path);
                enqueue(
                    model(object.path), object.transform, object.transform.map(QVector3D{}), false);
            }
            visible_models.insert(m_preview_key);
            QMatrix4x4 matrix;
            matrix.translate(m_preview->position);
            matrix.rotate(m_preview->yaw * (180.0f / 3.14159265f), 0, 1, 0);
            matrix.scale(m_preview->scale);
            enqueue(preview_model(m_preview_key), matrix, m_preview->position, false);
        }
        std::sort(transparent.begin(), transparent.end(), [](const Draw& a, const Draw& b) {
            return a.depth > b.depth;
        });
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        for (const auto& item : transparent) {
            draw(*item.part, item.transform, item.selected);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        for (auto it = m_models.begin(); it != m_models.end();) {
            if (!visible_models.count(it->first)) {
                for (auto& part : it->second.parts) {
                    free_mesh(part.mesh);
                    free_texture(part.texture);
                }
                it = m_models.erase(it);
            } else {
                ++it;
            }
        }
    }

    void WorldViewport::draw_water() {
        const auto position = eye();
        if (!m_water_visible || position.y() < map::kWaterPosY) {
            return;
        }
        const auto wet_chunk = [this](QPoint coordinate) {
            return m_terrain.at({coordinate.x(), coordinate.y()}).below_water;
        };
        if (std::none_of(m_visible.begin(), m_visible.end(), wet_chunk)) {
            return;
        }

        glActiveTexture(GL_TEXTURE0);
        if (!m_water_texture) {
            const auto path = platform::resolve_path_case(m_document.root().toStdString(),
                                                          "OBJECTS/common/tex/water_001.dds");
            terrain::ImageRgba texture;
            if (!platform::file_exists(path) || !terrain::load_gtx_rgba(path, texture) ||
                texture.rgba.empty()) {
                texture.width = texture.height = 1;
                texture.rgba = {36, 112, 168, 255};
            }
            m_water_texture = upload_texture(texture, true);
        }

        m_program->setUniformValue("uWater", 1);
        m_program->setUniformValue("uEye", position);
        m_program->setUniformValue("uWaterUvScale", 1.0f / (kCell * 4.0f));
        glBindTexture(GL_TEXTURE_2D, m_water_texture);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LESS);
        for (const auto coordinate : m_visible) {
            if (wet_chunk(coordinate)) {
                QMatrix4x4 model;
                model.translate((coordinate.x() - map::kPatchGrid / 2) * kExtent,
                                0.0f,
                                (coordinate.y() - map::kPatchGrid / 2) * kExtent);
                m_program->setUniformValue("uModel", model);
                draw_mesh(m_water);
            }
        }
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void WorldViewport::paintGL() {
        if (!m_program) {
            return;
        }
        if (m_scene_reset) {
            clear_scene_gl();
            m_scene_reset = false;
        }
        if (m_frame_preview) {
            frame_preview();
        }
        const auto matrix = camera();
        const bool outdoor = !m_preview || m_preview->outdoor;
        if (outdoor) {
            update_visible_chunks(matrix);
        }
        const auto count = m_requested_chunks.size();
        // Use a common sample step so adjacent patches share the same edge vertices.
        if (m_walking || (m_distance <= 2.5f * kExtent && count <= 18)) {
            m_terrain_step = 1;
        } else if (m_distance <= 6 * kExtent && count <= 45) {
            m_terrain_step = 2;
        } else if (m_distance <= 14 * kExtent && count <= 100) {
            m_terrain_step = 4;
        } else {
            m_terrain_step = 8;
        }
        render_scene(matrix, outdoor);
    }

    void WorldViewport::render_scene(const QMatrix4x4& matrix, bool outdoor) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        // Map images can have gaps where the original atlas should show through.
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        const bool map = !m_map_area.isEmpty();
        glClearColor(
            map ? 0.0f : 0.085f, map ? 0.0f : 0.115f, map ? 0.0f : 0.15f, map ? 0.0f : 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_program->bind();
        m_program->setUniformValue("uColor", 0);
        m_program->setUniformValue("uDetail", 1);
        m_program->setUniformValue("uWater", 0);
        m_program->setUniformValue("uPatchEdges", !map);
        // Keep derivatives precise when inspecting a small object far from world origin.
        m_program->setUniformValue("uLightingOrigin",
                                   map ? m_target : (m_preview ? eye() : QVector3D{}));
        if (outdoor) {
            draw_terrain(matrix);
        }
        draw_objects(matrix);
        if (outdoor) {
            draw_water();
        }
        glBindVertexArray(0);
        m_program->release();
    }

}  // namespace openkal::world_editor
