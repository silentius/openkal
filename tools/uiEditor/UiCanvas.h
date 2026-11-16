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

#include "BmpStore.h"
#include "openkal/client/UiDocument.h"

#include <QPoint>
#include <QRectF>
#include <QWidget>
#include <string>
#include <unordered_set>

/** Preview canvas for an openkal.ui/v1 document. */
class UiCanvas : public QWidget {
    Q_OBJECT

  public:
    /**
     * @param bmps Shared BMP cache.
     * @param parent Qt parent.
     */
    explicit UiCanvas(BmpStore& bmps, QWidget* parent = nullptr);

    /**
     * Binds the document being edited. Not owned.
     * @param doc Document, or nullptr.
     */
    void set_document(openkal::client::UiDocument* doc);

    /**
     * Sets dummy interpolation values.
     * @param vars Catalog.
     */
    void set_vars(openkal::client::UiVarMap vars);

    /**
     * Selects chrome (-1), a widget index (>=0), or nothing (-2).
     * @param index Selection index.
     */
    void set_selected_index(int index);

    /** @return Selected widget index, -1 for chrome, -2 for none. */
    [[nodiscard]] int selected_index() const {
        return m_selected;
    }

    /**
     * Sets snap in native pixels (1 or 32).
     * @param snap Snap step.
     */
    void set_snap(int snap);

    /**
     * Shows a 32 px grid over the client box.
     * @param on True to show.
     */
    void set_show_grid(bool on);

    /**
     * Locks a widget index against dragging.
     * @param index Widget index.
     * @param locked True to lock.
     */
    void set_locked(int index, bool locked);

    /**
     * @param index Widget index.
     * @return True when locked.
     */
    [[nodiscard]] bool locked(int index) const;

  signals:
    /** Emitted when the user selects chrome or a widget.
     * @param index Selection index.
     */
    void selection_changed(int index);

    /** Emitted after a drag or client-box resize. */
    void geometry_changed();

  protected:
    /** @param event Paint event. */
    void paintEvent(QPaintEvent* event) override;

    /** @param event Mouse press. */
    void mousePressEvent(QMouseEvent* event) override;

    /** @param event Mouse move. */
    void mouseMoveEvent(QMouseEvent* event) override;

    /** @param event Mouse release. */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /** @param event Arrow-key pad nudge when chrome is selected. */
    void keyPressEvent(QKeyEvent* event) override;

    /** @param event Wheel zoom. */
    void wheelEvent(QWheelEvent* event) override;

  private:
    /** Drag kind. */
    enum class DragKind {
        None = 0,
        Widget,
        ResizeW,
        ResizeH,
        PadLeft,
        PadTop,
        PadRight,
        PadBottom,
        PadCornerTl,
    };

    /** @return Client-box origin in widget pixels. */
    [[nodiscard]] QPointF client_origin() const;

    /** @return 9-slice layout in widget pixels. */
    [[nodiscard]] openkal::client::UiNineSliceLayout nine_slice_view() const;

    /** @return Resolved top-right corner file for the selected frame variant. */
    [[nodiscard]] std::string upright_file() const;

    /** @return Outer chrome rect in widget pixels. */
    [[nodiscard]] QRectF outer_view_rect() const;

    /** @return Client-box rect in widget pixels. */
    [[nodiscard]] QRectF client_view_rect() const;

    /**
     * Hits a chrome pad edge or the client-box resize strips.
     * @param pos Widget position.
     * @return Drag kind, or None.
     */
    [[nodiscard]] DragKind hit_frame(QPointF pos) const;

    /** Updates the mouse cursor for frame / client-box handles. */
    void update_hover_cursor(QPointF pos);

    /** @return Display scale. */
    [[nodiscard]] float view_scale() const {
        return m_view_scale;
    }

    /**
     * Maps native client coordinates to widget pixels.
     * @param nx Native X.
     * @param ny Native Y.
     * @return Widget point.
     */
    [[nodiscard]] QPointF to_view(float nx, float ny) const;

    /**
     * Maps widget pixels to native client coordinates.
     * @param p Widget point.
     * @param nx Out native X.
     * @param ny Out native Y.
     */
    void from_view(QPointF p, float& nx, float& ny) const;

    /**
     * Native size of a widget, resolving BMP size when w/h are 0.
     * @param w Widget.
     * @param nw Out width.
     * @param nh Out height.
     */
    void native_size(const openkal::client::UiWidget& w, float& nw, float& nh) const;

    /**
     * Native top-left and size of interpolated text, applying align and
     * y_from_bottom like the client.
     * @param w Widget.
     * @param nx Out native X of the drawn glyphs.
     * @param ny Out native Y of the drawn glyphs.
     * @param nw Out native width.
     * @param nh Out native height.
     */
    void text_native_rect(
        const openkal::client::UiWidget& w, float& nx, float& ny, float& nw, float& nh) const;

    /**
     * Draws a 9-slice frame around the client box.
     * @param p Painter.
     */
    void draw_chrome(QPainter& p) const;

    /**
     * Tiles a pixmap along a horizontal span.
     * @param p Painter.
     * @param px Pixmap.
     * @param x Dest X.
     * @param y Dest Y.
     * @param dest_w Span width.
     * @param dest_h Span height.
     */
    void tile_h(QPainter& p, const QPixmap& px, float x, float y, float dest_w, float dest_h) const;

    /**
     * Tiles a pixmap along a vertical span.
     * @param p Painter.
     * @param px Pixmap.
     * @param x Dest X.
     * @param y Dest Y.
     * @param dest_w Span width.
     * @param dest_h Span height.
     */
    void tile_v(QPainter& p, const QPixmap& px, float x, float y, float dest_w, float dest_h) const;

    /** Snaps a native coordinate.
     * @param v Value.
     * @return Snapped value.
     */
    [[nodiscard]] float snap_v(float v) const;

    /**
     * Snaps a chrome pad to 0.1 native pixels (grid snap does not apply).
     * @param v Value.
     * @return Snapped pad.
     */
    [[nodiscard]] float snap_pad(float v) const;

    /**
     * @param kind Drag kind.
     * @return True when the kind is a pad-edge drag.
     */
    [[nodiscard]] static bool is_pad_drag(DragKind kind);

    /** BMP cache. */
    BmpStore& m_bmps;

    /** Bound document; not owned. */
    openkal::client::UiDocument* m_doc = nullptr;

    /** Dummy catalog. */
    openkal::client::UiVarMap m_vars;

    /** Selection: -2 none, -1 chrome, else widget index. */
    int m_selected = -2;

    /** Snap step in native pixels. */
    int m_snap = 1;

    /** Show 32 px grid. */
    bool m_show_grid = false;

    /** Locked widget indices. */
    std::unordered_set<int> m_locked;

    /** View scale. */
    float m_view_scale = 1.0f;

    /** Margin around the outer frame. */
    float m_margin = 24.0f;

    /** Current drag. */
    DragKind m_drag = DragKind::None;

    /** Dragged widget index. */
    int m_drag_index = -1;

    /** Native grab offset X. */
    float m_grab_nx = 0.0f;

    /** Native grab offset Y. */
    float m_grab_ny = 0.0f;

    /** Pad value at the start of a pad-edge drag. */
    float m_grab_pad = 0.0f;

    /** Second pad at the start of a corner drag (pad_top for TL). */
    float m_grab_pad2 = 0.0f;
};
