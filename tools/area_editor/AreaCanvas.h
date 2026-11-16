/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "layers.h"

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QWidget>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

/** Interactive streamed world canvas composed from area patch images. */
class AreaCanvas final : public QWidget {
  public:
    /** Pointer mode. Painting requires Draw or Remove together with an armed layer. */
    enum class Tool {
        Navigate,
        Draw,
        Remove,
        Rectangle,
        Object,
    };

    /** Axis-aligned selection in overlay world coordinates (one unit per subcell). */
    struct WorldSelection {
        bool present = false;
        double min_x = 0.0;
        double min_y = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;
    };

    using PaintHandler = std::function<void(int, int, int, int, bool)>;
    using AutoHandler = std::function<void(int, int, int, int, bool)>;
    using BrushHandler = std::function<void(int)>;
    using ViewHandler = std::function<void(int, int, int, int)>;
    using SelectionHandler = std::function<void()>;
    using HoverHandler = std::function<void(bool, int, int, int, int)>;
    using RejectHandler = std::function<void()>;

    /** Creates an empty world canvas.
     * @param parent Optional Qt parent.
     */
    explicit AreaCanvas(QWidget* parent = nullptr);

    /** Adds or replaces one rendered world patch.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     * @param bake Top-down terrain and object image.
     * @param overlay Transparent attribute overlay.
     */
    void set_tile(int patch_x, int patch_y, QImage bake, QImage overlay);

    /** Replaces one patch's transparent attribute overlay.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     * @param overlay Transparent attribute overlay.
     */
    void set_tile_overlay(int patch_x, int patch_y, QImage overlay);

    /** Replaces the sharp object contours for one patch.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     * @param collision Closed head-height collision loops in subcell coordinates.
     * @param display Closed full-projection loops in subcell coordinates.
     */
    void set_tile_contours(int patch_x,
                           int patch_y,
                           std::vector<std::vector<QPointF>> collision,
                           std::vector<std::vector<QPointF>> display);

    /** Replaces the transient selection highlight for one patch.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     * @param preview Transparent highlight, or a null image to clear it.
     */
    void set_tile_preview(int patch_x, int patch_y, QImage preview);

    /** Clears every patch highlight. */
    void clear_previews();

    /** Removes one rendered patch from the canvas.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     */
    void remove_tile(int patch_x, int patch_y);

    /** Clears all displayed patch data. */
    void clear_tiles();

    /** Selects and optionally centers a patch.
     * @param patch_x Outdoor patch X coordinate.
     * @param patch_y Outdoor patch Y coordinate.
     * @param center Whether to center and fit the patch.
     */
    void set_active_patch(int patch_x, int patch_y, bool center);

    /** Selects the active layer used for cursor conversion.
     * @param layer Active layer.
     */
    void set_layer(openkal::area_editor::LayerId layer);

    /** Arms or disarms painting. A visible layer can stay unarmed.
     * @param armed True when a layer radio is selected.
     */
    void set_layer_armed(bool armed);

    /** Selects Navigate, Draw, Remove, Rectangle, or Object.
     * @param tool Active pointer tool.
     */
    void set_tool(Tool tool);

    /** Reports the active pointer tool.
     * @return Current tool.
     */
    [[nodiscard]] Tool tool() const {
        return m_tool;
    }

    /** Shows or hides object silhouette lines.
     * @param visible True to draw collision and display contours.
     */
    void set_contours_visible(bool visible);

    /** Shows or hides the editing grid.
     * @param visible True to draw a tile or subcell grid when zoomed in.
     */
    void set_grid_visible(bool visible);

    /** Fills the current rectangle in addition to its outline.
     * @param enabled True to tint the selection.
     * @param snap_to_tiles True to expand the tint to whole tiles.
     * @param color Fill color, including alpha.
     */
    void set_selection_fill(bool enabled, bool snap_to_tiles, const QColor& color);

    /** Sets the brush radius displayed on the canvas.
     * @param radius Radius in active-layer units.
     */
    void set_brush_radius(int radius);

    /** Selects drawing or removal.
     * @param erase True for removal.
     */
    void set_erase(bool erase);

    /** Reports the current rectangle in overlay world coordinates.
     * @return Selection. `present` is false when no rectangle is active.
     */
    [[nodiscard]] WorldSelection selection() const;

    /** Drops the current rectangle. */
    void clear_selection();

    /** Registers the continuous paint callback.
     * @param handler Callback receiving patch, local grid coordinates, and erase state.
     */
    void set_paint_handler(PaintHandler handler);

    /** Registers the automatic object callback.
     * @param handler Callback receiving patch, local 1024x1024 subcell coordinates, and whether to
     * erase.
     */
    void set_auto_handler(AutoHandler handler);

    /** Registers the Shift+wheel brush callback.
     * @param handler Callback receiving +1 or -1.
     */
    void set_brush_handler(BrushHandler handler);

    /** Registers the visible-patch callback.
     * @param handler Callback receiving inclusive minimum and maximum patch coordinates.
     */
    void set_view_handler(ViewHandler handler);

    /** Registers the rectangle-changed callback. */
    void set_selection_handler(SelectionHandler handler);

    /** Registers the cursor-moved callback.
     * @param handler Receives validity, patch, and subcell coordinates.
     */
    void set_hover_handler(HoverHandler handler);

    /** Registers the callback used when a paint gesture has no armed layer. */
    void set_reject_handler(RejectHandler handler);

    /** Fits the active patch into the viewport. */
    void fit_to_view();

    /** Runs automatic object painting at the current mouse position. */
    void trigger_auto_at_cursor();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

  private:
    struct DisplayTile {
        QImage bake;
        QImage overlay;
        QImage preview;
        std::vector<std::vector<QPointF>> collision_contours;
        std::vector<std::vector<QPointF>> display_contours;
    };

    [[nodiscard]] static std::uint64_t tile_key(int patch_x, int patch_y);
    [[nodiscard]] QRectF tile_rect(int patch_x, int patch_y) const;
    [[nodiscard]] bool screen_to_world(const QPointF& position, QPointF& world) const;
    [[nodiscard]] QPointF world_to_screen(const QPointF& world) const;
    [[nodiscard]] bool screen_to_grid(const QPointF& position,
                                      int grid_size,
                                      int& patch_x,
                                      int& patch_y,
                                      int& grid_x,
                                      int& grid_y) const;
    void paint_at(const QPointF& position);
    void apply_object_at(const QPointF& position, bool erase);
    void notify_view_changed();
    void notify_selection();
    void notify_hover(const QPointF& position);
    void update_cursor();
    void draw_loops(QPainter& painter,
                    const QRectF& target,
                    const std::vector<std::vector<QPointF>>& loops,
                    const QColor& color,
                    qreal width) const;
    void draw_grid(QPainter& painter, const QRectF& target) const;

    std::unordered_map<std::uint64_t, DisplayTile> m_tiles;
    QPointF m_view_center;
    QPointF m_last_mouse;
    QPointF m_last_cursor;
    QPointF m_selection_a;
    QPointF m_selection_b;
    QColor m_selection_fill{255, 220, 70, 70};
    float m_zoom = 0.5f;
    int m_active_patch_x = -1;
    int m_active_patch_y = -1;
    int m_brush_radius = 0;
    int m_last_paint_patch_x = -1;
    int m_last_paint_patch_y = -1;
    int m_last_paint_x = -1;
    int m_last_paint_y = -1;
    openkal::area_editor::LayerId m_layer = openkal::area_editor::LayerId::Blocked;
    Tool m_tool = Tool::Navigate;
    bool m_layer_armed = false;
    bool m_contours_visible = true;
    bool m_grid_visible = false;
    bool m_selection_fill_enabled = false;
    bool m_selection_snap_tiles = false;
    bool m_erase = false;
    bool m_panning = false;
    bool m_space_pan = false;
    bool m_selecting = false;
    bool m_has_selection = false;
    bool m_reject_latched = false;
    bool m_has_cursor = false;
    bool m_view_initialized = false;
    PaintHandler m_paint_handler;
    AutoHandler m_auto_handler;
    BrushHandler m_brush_handler;
    ViewHandler m_view_handler;
    SelectionHandler m_selection_handler;
    HoverHandler m_hover_handler;
    RejectHandler m_reject_handler;
};
