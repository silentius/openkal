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

#include "openkal/map/MapCatalog.h"
#include "openkal/map/MiniMapSize.h"
#include "openkal/terrain/HeightField.h"
#include "openkal/terrain/IndoorCollision.h"

#include <QCache>
#include <QGraphicsView>
#include <QImage>
#include <QMap>
#include <QTimer>

#include <map>
#include <memory>
#include <optional>

namespace openkal::world_editor {
    class WorldDocument;
    class WorldViewport;
}

namespace openkal::content_editor {

    /** Editable marker in server X/Y pixels. */
    struct Marker {
        QString path;
        QString name;
        QPoint start;
        QPoint end;
        int spawn = -1;
        bool active = false;
    };

    /** Zoomable client atlas with editor overlays in server coordinates. */
    class MapView : public QGraphicsView {
        Q_OBJECT

      public:
        /** Mouse behavior selected by the editor. */
        enum class Mode { Select, PlaceNpc, DrawSpawn, PlaceTeleport };

        /** Constructs an empty map view.
         * @param parent Parent widget.
         */
        explicit MapView(QWidget* parent = nullptr);

        /** Releases cached map images and the offscreen renderer. */
        ~MapView() override;

        /** Loads atlas paths and map metadata.
         * @param data Client data directory.
         * @param maps Client dungeon geometry directory.
         * @param config Selected loose configuration directory.
         * @param base Optional secondary configuration directory.
         * @param maps_subdirectory Terrain subdirectory under the client data directory.
         */
        void load(const QString& data,
                  const QString& maps,
                  const QString& config,
                  const QString& base,
                  const QString& maps_subdirectory = "MAPS");

        /** @return Map indices from mapinfo.json, including overworld. */
        [[nodiscard]] QMap<int, QString> maps() const;

        /** @param index Logical map ID.
         * @return Distinct atlas regions for this map.
         */
        [[nodiscard]] QStringList regions(int index) const;

        /** @return Current logical map ID. */
        [[nodiscard]] int map_id() const;

        /** Applies the server's overworld binding for monster spawn rectangles.
         * @param configured_map Map ID stored in the monster definition.
         * @param center Center of the spawn rectangle in server pixels.
         * @return Overworld for cells assigned to map 0; otherwise the configured map ID.
         */
        [[nodiscard]] int spawn_map(int configured_map, QPoint center) const;

        /** Changes the displayed atlas.
         * @param index Logical map ID.
         * @param region Atlas region index within the selected map.
         */
        void show_map(int index, int region);

        /** @param index Logical map ID.
         * @param position Server pixel position.
         * @return Atlas region index containing that position.
         */
        [[nodiscard]] int region_at(int index, QPoint position) const;

        /** Replaces the NPC markers and monster rectangles.
         * @param markers Overlay geometry for the selected map.
         */
        void set_markers(QVector<Marker> markers);

        /** Sets the next left mouse action.
         * @param mode Interaction mode.
         */
        void set_mode(Mode mode);

        /** @param enabled Show the terrain textures when zoomed in outdoors. */
        void set_terrain_detail(bool enabled);

        /** Fits the available map imagery or the current atlas region. */
        void fit();

        /** Centers on a position and zooms in for editing.
         * @param first First server pixel position.
         * @param second Opposite corner, or an empty value for an NPC.
         * @param minimum_margin Minimum padding in server pixels.
         */
        void focus(QPoint first,
                   std::optional<QPoint> second = std::nullopt,
                   qreal minimum_margin = 384.0);

        /** Finds ground elevations at a placed NPC position.
         * @param point Server X/Y in the current atlas region.
         * @return Possible server Z elevations, highest first.
         */
        [[nodiscard]] QVector<int> elevations(QPoint point);

        /** @param index Logical map ID.
         * @param point Server X/Y.
         * @return Indoor map data for the location, or nullptr for outdoor terrain.
         */
        [[nodiscard]] const map::MapInfo* indoor_cell(int index, QPoint point) const;

        /** @param index Logical map ID.
         * @param point Server X/Y.
         * @return Possible server Z elevations on the specified map.
         */
        [[nodiscard]] QVector<int> elevations(int index, QPoint point);

      signals:
        /** A marker was clicked.
         * @param path Definition file.
         * @param spawn Spawn row, or -1 for an NPC.
         */
        void marker_selected(const QString& path, int spawn);

        /** A point was chosen for a placement.
         * @param point Server X/Y.
         */
        void point_placed(QPoint point);

        /** A spawn area was drawn.
         * @param first First corner in server pixels.
         * @param second Second corner in server pixels.
         */
        void spawn_drawn(QPoint first, QPoint second);

        /** The pointer moved over the map.
         * @param text Coordinates to display in the status bar.
         */
        void coordinates(const QString& text);

      protected:
        /** @param event Wheel event to handle. */
        void wheelEvent(QWheelEvent* event) override;

        /** @param event Mouse press to handle. */
        void mousePressEvent(QMouseEvent* event) override;

        /** @param event Mouse motion to handle. */
        void mouseMoveEvent(QMouseEvent* event) override;

        /** @param event Mouse release to handle. */
        void mouseReleaseEvent(QMouseEvent* event) override;

        /** @param event Resize event to handle. */
        void resizeEvent(QResizeEvent* event) override;

        /** @param painter Scene painter.
         * @param exposed Scene portion to draw.
         */
        void drawBackground(QPainter* painter, const QRectF& exposed) override;

        /** @param painter Scene painter.
         * @param exposed Scene portion to draw.
         */
        void drawForeground(QPainter* painter, const QRectF& exposed) override;

      private:
        struct Region {
            int x;
            int y;
            QString stem;
            map::MiniMapSize size;
        };

        struct Tile {
            QRectF bounds;
            QStringList files;
        };

        struct DetailRequest {
            QRectF bounds;
            QSize size;
        };

        [[nodiscard]] QVector<Region> atlas_regions(int index) const;
        [[nodiscard]] std::optional<Region> current_region() const;
        [[nodiscard]] QPoint pixel_at(QPoint viewport) const;
        [[nodiscard]] bool can_place(QPoint point) const;
        [[nodiscard]] const Marker* marker_at(QPoint viewport) const;
        [[nodiscard]] const QImage* image(const QStringList& files);
        [[nodiscard]] const QImage* terrain_image(QPoint cell);
        [[nodiscard]] QString terrain_path(QPoint cell) const;
        [[nodiscard]] bool draw_terrain_detail(QPainter* painter, const QRectF& exposed);
        [[nodiscard]] bool draw_rendered_map(QPainter* painter);
        void render_map_detail();
        [[nodiscard]] QVector<int> dungeon_elevations(const Region& region, QPoint point);

        QString m_data;
        QString m_map_root;
        QString m_maps_subdirectory;
        map::MapCatalog m_maps;
        map::MiniMapSizeCatalog m_sizes;
        std::vector<terrain::LayerInfo> m_layers;
        terrain::HeightField m_height;
        std::optional<QPoint> m_height_cell;
        std::map<QString, terrain::IndoorCollision> m_indoor;
        QCache<QString, QImage> m_images;
        std::unique_ptr<world_editor::WorldDocument> m_detail_world;
        std::unique_ptr<world_editor::WorldViewport> m_detail_renderer;
        QTimer m_detail_timer;
        std::optional<DetailRequest> m_detail_pending;
        std::optional<DetailRequest> m_detail_completed;
        QRectF m_detail_bounds;
        QImage m_detail_image;
        bool m_detail_unavailable = false;
        QVector<Tile> m_outdoor_tiles;
        QVector<Tile> m_tiles;
        QRectF m_bounds;
        QVector<Marker> m_markers;
        int m_index = 0;
        int m_region = 0;
        Mode m_mode = Mode::Select;
        bool m_terrain_detail = true;
        bool m_fitted = true;
        std::optional<QPoint> m_start;
        QPoint m_cursor;
        std::optional<QPoint> m_pan;
    };

}  // namespace openkal::content_editor
