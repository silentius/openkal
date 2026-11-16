/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "AreaCanvas.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

    constexpr qreal kPatchUnits = openkal::area_editor::kOverlaySize;
    constexpr qreal kTileUnits = static_cast<qreal>(openkal::map::kTileCount);

    [[nodiscard]] bool paint_tool(AreaCanvas::Tool tool) {
        return tool == AreaCanvas::Tool::Draw || tool == AreaCanvas::Tool::Remove;
    }

}  // namespace

AreaCanvas::AreaCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(480, 360);
    update_cursor();
}

void AreaCanvas::set_tile(int patch_x, int patch_y, QImage bake, QImage overlay) {
    DisplayTile& tile = m_tiles[tile_key(patch_x, patch_y)];
    tile.bake = std::move(bake);
    tile.overlay = std::move(overlay);
    tile.preview = {};
    tile.collision_contours.clear();
    tile.display_contours.clear();
    update();
}

void AreaCanvas::set_tile_overlay(int patch_x, int patch_y, QImage overlay) {
    const auto found = m_tiles.find(tile_key(patch_x, patch_y));
    if (found == m_tiles.end()) {
        return;
    }
    found->second.overlay = std::move(overlay);
    update();
}

void AreaCanvas::set_tile_contours(int patch_x,
                                   int patch_y,
                                   std::vector<std::vector<QPointF>> collision,
                                   std::vector<std::vector<QPointF>> display) {
    const auto found = m_tiles.find(tile_key(patch_x, patch_y));
    if (found == m_tiles.end()) {
        return;
    }
    found->second.collision_contours = std::move(collision);
    found->second.display_contours = std::move(display);
    update();
}

void AreaCanvas::set_tile_preview(int patch_x, int patch_y, QImage preview) {
    const auto found = m_tiles.find(tile_key(patch_x, patch_y));
    if (found == m_tiles.end()) {
        return;
    }
    found->second.preview = std::move(preview);
    update();
}

void AreaCanvas::clear_previews() {
    for (auto& [key, tile] : m_tiles) {
        (void) key;
        tile.preview = {};
    }
    update();
}

void AreaCanvas::remove_tile(int patch_x, int patch_y) {
    m_tiles.erase(tile_key(patch_x, patch_y));
    update();
}

void AreaCanvas::clear_tiles() {
    const bool had_selection = m_selecting || m_has_selection;
    m_tiles.clear();
    m_active_patch_x = -1;
    m_active_patch_y = -1;
    m_has_cursor = false;
    m_view_initialized = false;
    m_has_selection = false;
    m_selecting = false;
    update();
    if (had_selection) {
        notify_selection();
    }
}

void AreaCanvas::set_active_patch(int patch_x, int patch_y, bool center) {
    m_active_patch_x = patch_x;
    m_active_patch_y = patch_y;
    if (center || !m_view_initialized) {
        const bool was_view_initialized = m_view_initialized;
        m_view_center = QPointF((static_cast<qreal>(patch_x) + 0.5) * kPatchUnits,
                                (static_cast<qreal>(patch_y) + 0.5) * kPatchUnits);
        m_view_initialized = true;
        fit_to_view();
        if (!was_view_initialized) {
            QTimer::singleShot(0, this, &AreaCanvas::fit_to_view);
        }
    } else {
        update();
    }
}

void AreaCanvas::set_layer(openkal::area_editor::LayerId layer) {
    m_layer = layer;
    m_last_paint_x = -1;
    m_last_paint_y = -1;
    update();
}

void AreaCanvas::set_layer_armed(bool armed) {
    m_layer_armed = armed;
    update();
}

void AreaCanvas::set_tool(Tool tool) {
    m_tool = tool;
    m_last_paint_x = -1;
    m_last_paint_y = -1;
    update_cursor();
    update();
}

void AreaCanvas::set_contours_visible(bool visible) {
    m_contours_visible = visible;
    update();
}

void AreaCanvas::set_grid_visible(bool visible) {
    m_grid_visible = visible;
    update();
}

void AreaCanvas::set_selection_fill(bool enabled, bool snap_to_tiles, const QColor& color) {
    m_selection_fill_enabled = enabled;
    m_selection_snap_tiles = snap_to_tiles;
    m_selection_fill = color;
    update();
}

void AreaCanvas::set_brush_radius(int radius) {
    m_brush_radius = std::clamp(radius, 0, 32);
    update();
}

void AreaCanvas::set_erase(bool erase) {
    m_erase = erase;
    update();
}

AreaCanvas::WorldSelection AreaCanvas::selection() const {
    WorldSelection result;
    if (!m_selecting && !m_has_selection) {
        return result;
    }
    result.present = true;
    result.min_x = std::min(m_selection_a.x(), m_selection_b.x());
    result.min_y = std::min(m_selection_a.y(), m_selection_b.y());
    result.max_x = std::max(m_selection_a.x(), m_selection_b.x());
    result.max_y = std::max(m_selection_a.y(), m_selection_b.y());
    return result;
}

void AreaCanvas::clear_selection() {
    if (!m_selecting && !m_has_selection) {
        return;
    }
    m_selecting = false;
    m_has_selection = false;
    notify_selection();
}

void AreaCanvas::set_paint_handler(PaintHandler handler) {
    m_paint_handler = std::move(handler);
}

void AreaCanvas::set_auto_handler(AutoHandler handler) {
    m_auto_handler = std::move(handler);
}

void AreaCanvas::set_brush_handler(BrushHandler handler) {
    m_brush_handler = std::move(handler);
}

void AreaCanvas::set_view_handler(ViewHandler handler) {
    m_view_handler = std::move(handler);
    notify_view_changed();
}

void AreaCanvas::set_selection_handler(SelectionHandler handler) {
    m_selection_handler = std::move(handler);
}

void AreaCanvas::set_hover_handler(HoverHandler handler) {
    m_hover_handler = std::move(handler);
}

void AreaCanvas::set_reject_handler(RejectHandler handler) {
    m_reject_handler = std::move(handler);
}

void AreaCanvas::fit_to_view() {
    if (!m_view_initialized || width() <= 0 || height() <= 0) {
        return;
    }
    const float sx = static_cast<float>(width()) / static_cast<float>(kPatchUnits);
    const float sy = static_cast<float>(height()) / static_cast<float>(kPatchUnits);
    m_zoom = std::max(0.005f, std::min(sx, sy));
    update();
    notify_view_changed();
}

void AreaCanvas::trigger_auto_at_cursor() {
    if (!m_auto_handler || !m_has_cursor) {
        return;
    }
    int patch_x = 0;
    int patch_y = 0;
    int subcell_x = 0;
    int subcell_y = 0;
    if (screen_to_grid(m_last_cursor,
                       openkal::area_editor::kOverlaySize,
                       patch_x,
                       patch_y,
                       subcell_x,
                       subcell_y)) {
        m_auto_handler(patch_x, patch_y, subcell_x, subcell_y, m_erase);
    }
}

void AreaCanvas::apply_object_at(const QPointF& position, bool erase) {
    if (!m_auto_handler) {
        return;
    }
    int patch_x = 0;
    int patch_y = 0;
    int subcell_x = 0;
    int subcell_y = 0;
    if (screen_to_grid(
            position, openkal::area_editor::kOverlaySize, patch_x, patch_y, subcell_x, subcell_y)) {
        m_auto_handler(patch_x, patch_y, subcell_x, subcell_y, erase);
    }
}

void AreaCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 33, 39));
    if (!m_view_initialized) {
        painter.setPen(QColor(190, 194, 202));
        painter.drawText(rect(), Qt::AlignCenter, "Select an area from the list on the right");
        return;
    }

    for (const auto& [key, tile] : m_tiles) {
        const int patch_x = static_cast<int>(key >> 32);
        const int patch_y = static_cast<int>(key & 0xffffffffu);
        const QRectF target = tile_rect(patch_x, patch_y);
        if (!target.intersects(rect())) {
            continue;
        }
        const qreal texel_px =
            tile.bake.width() > 0 ? target.width() / static_cast<qreal>(tile.bake.width()) : 1.0;
        painter.setRenderHint(QPainter::SmoothPixmapTransform, texel_px < 1.0);
        painter.drawImage(target, tile.bake);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        if (!tile.overlay.isNull()) {
            painter.drawImage(target, tile.overlay);
        }
        if (!tile.preview.isNull()) {
            painter.drawImage(target, tile.preview);
        }
        if (m_contours_visible) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            draw_loops(painter, target, tile.display_contours, QColor(64, 220, 140, 210), 1.25);
            draw_loops(painter, target, tile.collision_contours, QColor(255, 248, 220, 235), 1.8);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }
        if (m_grid_visible) {
            draw_grid(painter, target);
        }
        const bool active = patch_x == m_active_patch_x && patch_y == m_active_patch_y;
        QPen border(active ? QColor(255, 220, 70, 220) : QColor(255, 255, 255, 60),
                    active ? 2.0 : 1.0);
        border.setCosmetic(true);
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(target);
    }

    const WorldSelection selected = selection();
    if (selected.present) {
        QPointF top_left(selected.min_x, selected.min_y);
        QPointF bottom_right(selected.max_x, selected.max_y);
        if (m_selection_fill_enabled && m_selection_snap_tiles) {
            constexpr double kTile = static_cast<double>(kPatchUnits / kTileUnits);
            top_left.setX(std::floor(selected.min_x / kTile) * kTile);
            top_left.setY(std::floor(selected.min_y / kTile) * kTile);
            bottom_right.setX(std::ceil(selected.max_x / kTile) * kTile);
            bottom_right.setY(std::ceil(selected.max_y / kTile) * kTile);
        }
        const QRectF screen_rect(world_to_screen(top_left), world_to_screen(bottom_right));
        if (m_selection_fill_enabled) {
            painter.fillRect(screen_rect, m_selection_fill);
        }
        QPen selection_pen(QColor(255, 220, 70), 1.5);
        selection_pen.setCosmetic(true);
        painter.setPen(selection_pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(screen_rect);
    }

    if (!m_has_cursor || !m_layer_armed || !paint_tool(m_tool)) {
        return;
    }
    const int grid = openkal::area_editor::layer_uses_subcells(m_layer)
                         ? openkal::area_editor::kOverlaySize
                         : openkal::map::kTileCount;
    int patch_x = 0;
    int patch_y = 0;
    int grid_x = 0;
    int grid_y = 0;
    if (!screen_to_grid(m_last_cursor, grid, patch_x, patch_y, grid_x, grid_y)) {
        return;
    }
    const QRectF target = tile_rect(patch_x, patch_y);
    const auto& info = openkal::area_editor::layer_info(m_layer);
    const QColor color = m_erase ? QColor(255, 90, 90) : QColor::fromRgbF(info.r, info.g, info.b);
    QPen cursor_pen(color, 2.0);
    cursor_pen.setCosmetic(true);
    painter.setPen(cursor_pen);
    painter.setBrush(Qt::NoBrush);
    const qreal cell = target.width() / static_cast<qreal>(grid);
    const QPointF center(target.left() + (static_cast<qreal>(grid_x) + 0.5) * cell,
                         target.top() + (static_cast<qreal>(grid_y) + 0.5) * cell);
    const qreal radius = (static_cast<qreal>(m_brush_radius) + 0.5) * cell;
    painter.drawEllipse(center, radius, radius);
}

void AreaCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    notify_view_changed();
}

void AreaCanvas::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    m_last_mouse = event->position();
    m_last_cursor = event->position();
    m_has_cursor = true;
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton || m_space_pan) {
        m_panning = true;
        update_cursor();
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_tool == Tool::Navigate) {
        m_panning = true;
        update_cursor();
        event->accept();
        return;
    }
    if (m_tool == Tool::Rectangle) {
        QPointF world;
        if (screen_to_world(event->position(), world)) {
            m_selection_a = world;
            m_selection_b = world;
            m_selecting = true;
            m_has_selection = false;
            notify_selection();
        }
        event->accept();
        return;
    }
    if (m_tool == Tool::Object) {
        apply_object_at(event->position(), m_erase);
        event->accept();
        return;
    }
    if (!m_layer_armed) {
        m_reject_latched = true;
        if (m_reject_handler) {
            m_reject_handler();
        }
        event->accept();
        return;
    }
    paint_at(event->position());
    event->accept();
}

void AreaCanvas::mouseMoveEvent(QMouseEvent* event) {
    m_last_cursor = event->position();
    m_has_cursor = true;
    if (m_panning) {
        const QPointF delta = event->position() - m_last_mouse;
        m_view_center -= delta / static_cast<qreal>(m_zoom);
        m_last_mouse = event->position();
        update();
        notify_view_changed();
        return;
    }
    if (m_selecting) {
        QPointF world;
        if (screen_to_world(event->position(), world)) {
            m_selection_b = world;
            notify_selection();
        }
        return;
    }
    if ((event->buttons() & Qt::LeftButton) != 0 && paint_tool(m_tool)) {
        if (!m_layer_armed) {
            if (!m_reject_latched && m_reject_handler) {
                m_reject_latched = true;
                m_reject_handler();
            }
        } else {
            paint_at(event->position());
        }
    } else {
        notify_hover(event->position());
    }
    update();
}

void AreaCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_selecting) {
        QPointF world;
        if (screen_to_world(event->position(), world)) {
            m_selection_b = world;
        }
        m_selecting = false;
        const QPointF delta = m_selection_b - m_selection_a;
        m_has_selection = std::abs(delta.x()) >= 1.0 || std::abs(delta.y()) >= 1.0;
        notify_selection();
    }
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton || m_panning) {
        m_panning = false;
        update_cursor();
    }
    if (event->button() == Qt::LeftButton) {
        m_reject_latched = false;
        m_last_paint_patch_x = -1;
        m_last_paint_patch_y = -1;
        m_last_paint_x = -1;
        m_last_paint_y = -1;
    }
    QWidget::mouseReleaseEvent(event);
}

void AreaCanvas::wheelEvent(QWheelEvent* event) {
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        if (m_brush_handler && event->angleDelta().y() != 0) {
            m_brush_handler(event->angleDelta().y() > 0 ? 1 : -1);
        }
        event->accept();
        return;
    }
    if (!m_view_initialized || event->angleDelta().y() == 0) {
        return;
    }
    const QPointF cursor = event->position();
    const QPointF screen_center(width() * 0.5, height() * 0.5);
    const QPointF world_at_cursor =
        m_view_center + (cursor - screen_center) / static_cast<qreal>(m_zoom);
    const float factor = event->angleDelta().y() > 0 ? 1.15f : (1.0f / 1.15f);
    m_zoom = std::clamp(m_zoom * factor, 0.005f, 24.0f);
    m_view_center = world_at_cursor - (cursor - screen_center) / static_cast<qreal>(m_zoom);
    m_last_cursor = cursor;
    m_has_cursor = true;
    update();
    notify_view_changed();
    event->accept();
}

void AreaCanvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_F && !event->isAutoRepeat()) {
        fit_to_view();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_space_pan = true;
        update_cursor();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AreaCanvas::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_space_pan = false;
        update_cursor();
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

std::uint64_t AreaCanvas::tile_key(int patch_x, int patch_y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(patch_x)) << 32) |
           static_cast<std::uint32_t>(patch_y);
}

QRectF AreaCanvas::tile_rect(int patch_x, int patch_y) const {
    const QPointF screen_center(width() * 0.5, height() * 0.5);
    const QPointF top_left = screen_center + (QPointF(static_cast<qreal>(patch_x) * kPatchUnits,
                                                      static_cast<qreal>(patch_y) * kPatchUnits) -
                                              m_view_center) *
                                                 static_cast<qreal>(m_zoom);
    const qreal size = kPatchUnits * static_cast<qreal>(m_zoom);
    return {top_left, QSizeF(size, size)};
}

bool AreaCanvas::screen_to_world(const QPointF& position, QPointF& world) const {
    if (!m_view_initialized || m_zoom <= 0.0f) {
        return false;
    }
    const QPointF screen_center(width() * 0.5, height() * 0.5);
    world = m_view_center + (position - screen_center) / static_cast<qreal>(m_zoom);
    return true;
}

QPointF AreaCanvas::world_to_screen(const QPointF& world) const {
    const QPointF screen_center(width() * 0.5, height() * 0.5);
    return screen_center + (world - m_view_center) * static_cast<qreal>(m_zoom);
}

bool AreaCanvas::screen_to_grid(const QPointF& position,
                                int grid_size,
                                int& patch_x,
                                int& patch_y,
                                int& grid_x,
                                int& grid_y) const {
    QPointF world;
    if (!screen_to_world(position, world) || grid_size <= 0) {
        return false;
    }
    patch_x = static_cast<int>(std::floor(world.x() / kPatchUnits));
    patch_y = static_cast<int>(std::floor(world.y() / kPatchUnits));
    if (patch_x < 0 || patch_y < 0 || patch_x >= openkal::map::kPatchGrid ||
        patch_y >= openkal::map::kPatchGrid) {
        return false;
    }
    const qreal local_x = world.x() - static_cast<qreal>(patch_x) * kPatchUnits;
    const qreal local_y = world.y() - static_cast<qreal>(patch_y) * kPatchUnits;
    grid_x = static_cast<int>(std::floor(local_x * grid_size / kPatchUnits));
    grid_y = static_cast<int>(std::floor(local_y * grid_size / kPatchUnits));
    return grid_x >= 0 && grid_y >= 0 && grid_x < grid_size && grid_y < grid_size;
}

void AreaCanvas::paint_at(const QPointF& position) {
    if (!m_paint_handler || !m_layer_armed || !paint_tool(m_tool)) {
        return;
    }
    const int grid = openkal::area_editor::layer_uses_subcells(m_layer)
                         ? openkal::area_editor::kOverlaySize
                         : openkal::map::kTileCount;
    int patch_x = 0;
    int patch_y = 0;
    int grid_x = 0;
    int grid_y = 0;
    if (!screen_to_grid(position, grid, patch_x, patch_y, grid_x, grid_y) ||
        (patch_x == m_last_paint_patch_x && patch_y == m_last_paint_patch_y &&
         grid_x == m_last_paint_x && grid_y == m_last_paint_y)) {
        return;
    }
    m_last_paint_patch_x = patch_x;
    m_last_paint_patch_y = patch_y;
    m_last_paint_x = grid_x;
    m_last_paint_y = grid_y;
    m_paint_handler(patch_x, patch_y, grid_x, grid_y, m_erase);
}

void AreaCanvas::notify_view_changed() {
    if (!m_view_handler || !m_view_initialized || m_zoom <= 0.0f || width() <= 0 || height() <= 0) {
        return;
    }
    const qreal half_width = static_cast<qreal>(width()) * 0.5 / m_zoom;
    const qreal half_height = static_cast<qreal>(height()) * 0.5 / m_zoom;
    const int min_x =
        std::clamp(static_cast<int>(std::floor((m_view_center.x() - half_width) / kPatchUnits)),
                   0,
                   openkal::map::kPatchGrid - 1);
    const int min_y =
        std::clamp(static_cast<int>(std::floor((m_view_center.y() - half_height) / kPatchUnits)),
                   0,
                   openkal::map::kPatchGrid - 1);
    const int max_x =
        std::clamp(static_cast<int>(std::floor((m_view_center.x() + half_width) / kPatchUnits)),
                   0,
                   openkal::map::kPatchGrid - 1);
    const int max_y =
        std::clamp(static_cast<int>(std::floor((m_view_center.y() + half_height) / kPatchUnits)),
                   0,
                   openkal::map::kPatchGrid - 1);
    m_view_handler(min_x, min_y, max_x, max_y);
}

void AreaCanvas::notify_selection() {
    update();
    if (m_selection_handler) {
        m_selection_handler();
    }
}

void AreaCanvas::notify_hover(const QPointF& position) {
    if (!m_hover_handler) {
        return;
    }
    int patch_x = 0;
    int patch_y = 0;
    int subcell_x = 0;
    int subcell_y = 0;
    const bool valid = screen_to_grid(
        position, openkal::area_editor::kOverlaySize, patch_x, patch_y, subcell_x, subcell_y);
    m_hover_handler(valid, patch_x, patch_y, subcell_x, subcell_y);
}

void AreaCanvas::update_cursor() {
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_space_pan) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_tool == Tool::Navigate) {
        setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void AreaCanvas::draw_loops(QPainter& painter,
                            const QRectF& target,
                            const std::vector<std::vector<QPointF>>& loops,
                            const QColor& color,
                            qreal width) const {
    if (loops.empty()) {
        return;
    }
    QPen pen(color, width);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const qreal scale = target.width() / kPatchUnits;
    for (const std::vector<QPointF>& loop : loops) {
        if (loop.size() < 2) {
            continue;
        }
        QPolygonF polygon;
        polygon.reserve(static_cast<int>(loop.size()));
        for (const QPointF& point : loop) {
            polygon << QPointF(target.left() + point.x() * scale, target.top() + point.y() * scale);
        }
        painter.drawPolyline(polygon);
    }
}

void AreaCanvas::draw_grid(QPainter& painter, const QRectF& target) const {
    const qreal sub_px = target.width() / kPatchUnits;
    const qreal tile_px = target.width() / kTileUnits;
    const int divisions =
        sub_px >= 8.0 ? static_cast<int>(kPatchUnits) : static_cast<int>(kTileUnits);
    if ((sub_px >= 8.0 ? sub_px : tile_px) < 8.0) {
        return;
    }
    const qreal step = target.width() / static_cast<qreal>(divisions);
    const int first_x =
        std::clamp(static_cast<int>(std::floor((0.0 - target.left()) / step)), 0, divisions);
    const int last_x = std::clamp(
        static_cast<int>(std::ceil((static_cast<qreal>(width()) - target.left()) / step)),
        0,
        divisions);
    const int first_y =
        std::clamp(static_cast<int>(std::floor((0.0 - target.top()) / step)), 0, divisions);
    const int last_y = std::clamp(
        static_cast<int>(std::ceil((static_cast<qreal>(height()) - target.top()) / step)),
        0,
        divisions);
    QPen grid_pen(sub_px >= 8.0 ? QColor(255, 255, 255, 45) : QColor(255, 255, 255, 80), 1.0);
    grid_pen.setCosmetic(true);
    painter.setPen(grid_pen);
    painter.setBrush(Qt::NoBrush);
    for (int index = first_x; index <= last_x; ++index) {
        const qreal x = target.left() + step * static_cast<qreal>(index);
        painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
    }
    for (int index = first_y; index <= last_y; ++index) {
        const qreal y = target.top() + step * static_cast<qreal>(index);
        painter.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
    }
}
