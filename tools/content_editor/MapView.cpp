/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MapView.h"

#include "WorldViewport.h"
#include "openkal/client/MinimapWidget.h"
#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/platform/File.h"
#include "openkal/terrain/GtxLoader.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace openkal::content_editor {
    namespace {
        constexpr int kExtent = map::kPatchGrid * map::kWorldSize;
        constexpr int kGroup = 2 * map::kWorldSize;
        constexpr qreal kOverworldRatio =
            map::kTerrainCellSize / client::MinimapWidget::kPixelsPerCell;
        constexpr qreal kDungeonRatio =
            map::kTerrainCellSize / client::MinimapWidget::kDungeonPixelsPerCell;

        QString number(int value) {
            return QString::number(value).rightJustified(3, '0');
        }

        QPointF scene_point(QPoint point) {
            return {static_cast<qreal>(point.x()), -static_cast<qreal>(point.y())};
        }

        QRectF scene_rect(QPoint a, QPoint b) {
            return QRectF(scene_point(a), scene_point(b)).normalized();
        }

        QRectF marker_rect(const Marker& marker, qreal minimum) {
            auto rect = scene_rect(marker.start, marker.end);
            if (rect.width() < minimum) {
                rect.adjust(-minimum / 2, 0, minimum / 2, 0);
            }
            if (rect.height() < minimum) {
                rect.adjust(0, -minimum / 2, 0, minimum / 2);
            }
            return rect;
        }

        QString map_stem(std::string name) {
            const auto slash = name.find_last_of("/\\");
            return QString::fromStdString(name.substr(slash == std::string::npos ? 0 : slash + 1));
        }
    }  // namespace

    MapView::MapView(QWidget* parent) : QGraphicsView(parent), m_images(64 * 1024) {
        setScene(new QGraphicsScene(this));
        setFrameShape(QFrame::NoFrame);
        setMouseTracking(true);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setRenderHint(QPainter::SmoothPixmapTransform);
        setTransformationAnchor(QGraphicsView::NoAnchor);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setMinimumSize(400, 300);
        m_detail_timer.setSingleShot(true);
        m_detail_timer.setInterval(45);
        connect(&m_detail_timer, &QTimer::timeout, this, &MapView::render_map_detail);
    }

    MapView::~MapView() = default;

    void MapView::load(const QString& data,
                       const QString& maps,
                       const QString& config,
                       const QString& base,
                       const QString& maps_subdirectory) {
        m_data = data;
        m_map_root = maps;
        m_maps_subdirectory = maps_subdirectory;
        m_detail_timer.stop();
        m_detail_pending.reset();
        m_detail_completed.reset();
        m_detail_image = {};
        m_detail_renderer.reset();
        m_detail_world.reset();
        m_detail_unavailable = false;
        m_maps.clear();
        m_sizes.clear();
        m_layers.clear();
        m_height.patches.clear();
        m_height_cell.reset();
        m_indoor.clear();
        m_images.clear();
        m_outdoor_tiles.clear();
        m_markers.clear();
        const auto load_catalog = [&](auto& catalog, const QString& filename) {
            const auto path = QDir(config).absoluteFilePath(filename);
            if (!config.isEmpty() && platform::File::exists(path.toStdString()) &&
                catalog.load_json(path.toStdString())) {
                return;
            }
            const auto fallback = QDir(base).absoluteFilePath(filename);
            if (!base.isEmpty() && fallback != path &&
                platform::File::exists(fallback.toStdString())) {
                (void) catalog.load_json(fallback.toStdString());
            }
        };
        load_catalog(m_maps, "mapinfo.json");
        load_catalog(m_sizes, "ui/dungeon_minimaps.json");

        if (!data.isEmpty()) {
            const QDir terrain(QDir(data).filePath(m_maps_subdirectory));
            const auto environment = terrain.filePath("n.env");
            if (QFileInfo(environment).isFile()) {
                (void) terrain::load_env_layers(environment.toStdString(), m_layers);
            }
            const QDir atlas(QDir(data).filePath("HyperText/MiniMap"));
            for (int x = 0; x < map::kPatchGrid; x += 2) {
                for (int y = 0; y < map::kPatchGrid; y += 2) {
                    const auto name = number(x) + '_' + number(y) + ".gtx";
                    QStringList files;
                    for (const auto& relative :
                         {"e/minimap_n_e_" + name, "e/minimap_n_" + name, "minimap_n_" + name}) {
                        const auto file = atlas.filePath(relative);
                        if (QFileInfo(file).isFile()) {
                            files.append(file);
                        }
                    }
                    if (!files.isEmpty()) {
                        float left, top, right, bottom;
                        client::MinimapWidget::tile_rect(x, y, left, top, right, bottom);
                        m_outdoor_tiles.append({QRectF(left * kOverworldRatio,
                                                       top * kOverworldRatio - kExtent,
                                                       (right - left) * kOverworldRatio,
                                                       (bottom - top) * kOverworldRatio),
                                                files});
                    }
                }
            }
        }
        show_map(0, 0);
    }

    QMap<int, QString> MapView::maps() const {
        QMap<int, QString> result{{0, "Overworld"}};
        for (const auto& info : m_maps.entries()) {
            if (info.index() <= 0) {
                continue;
            }
            const auto name = m_maps.has_indoor_mapname(info.index())
                                  ? QString::fromStdString(std::string(
                                        client::MinimapWidget::dungeon_area_name(info.index())))
                                  : QString("Instance");
            result[info.index()] = QString("%1 (map %2)").arg(name).arg(info.index());
        }
        return result;
    }

    QVector<MapView::Region> MapView::atlas_regions(int index) const {
        QVector<Region> result;
        QSet<QString> visited;
        for (const auto& info : m_maps.entries()) {
            if (info.index() != index || info.mapname().empty()) {
                continue;
            }
            const int x = coords::even_cell(info.x());
            const int y = coords::even_cell(info.y());
            if (x < 0 || x >= map::kPatchGrid || y < 0 || y >= map::kPatchGrid) {
                continue;
            }
            const auto stem = map_stem(info.mapname());
            const auto key = QString("%1:%2:%3").arg(x).arg(y).arg(stem);
            if (visited.contains(key)) {
                continue;
            }
            visited.insert(key);
            const auto* data = m_sizes.find(stem.toStdString());
            result.append({x,
                           y,
                           stem,
                           data ? *data : map::MiniMapSize::dungeon_defaults(stem.toStdString())});
        }
        return result;
    }

    QStringList MapView::regions(int index) const {
        QStringList result;
        for (const auto& region : atlas_regions(index)) {
            result.append(QString("%1  (%2, %3)").arg(region.stem).arg(region.x).arg(region.y));
        }
        return result;
    }

    int MapView::map_id() const {
        return m_index;
    }

    int MapView::region_at(int index, QPoint position) const {
        const auto options = atlas_regions(index);
        const auto* info =
            m_maps.find_cell(index, position.x() / map::kWorldSize, position.y() / map::kWorldSize);
        for (int i = 0; i < options.size(); ++i) {
            const auto& option = options.at(i);
            if (position.x() >= option.x * map::kWorldSize &&
                position.y() >= option.y * map::kWorldSize &&
                position.x() < option.x * map::kWorldSize + kGroup &&
                position.y() < option.y * map::kWorldSize + kGroup &&
                (!info || info->mapname().empty() || map_stem(info->mapname()) == option.stem)) {
                return i;
            }
        }
        return 0;
    }

    std::optional<MapView::Region> MapView::current_region() const {
        const auto options = atlas_regions(m_index);
        return m_region < options.size() ? std::optional<Region>(options.at(m_region))
                                         : std::nullopt;
    }

    void MapView::show_map(int index, int region) {
        m_index = index;
        m_region = std::max(0, region);
        m_start.reset();
        m_tiles.clear();
        m_bounds = {};
        if (const auto indoor = current_region()) {
            const auto& size = indoor->size;
            const int cols = std::max(1, size.size_x() / std::max(1, size.tile_width()));
            const int rows = std::max(1, size.size_y() / std::max(1, size.tile_height()));
            const qreal x =
                indoor->x * map::kWorldSize -
                (static_cast<qreal>(size.map_left()) * map::kMapSize + size.correction()) *
                    kDungeonRatio;
            const qreal y = -indoor->y * map::kWorldSize +
                            (static_cast<qreal>(size.map_bottom()) * map::kMapSize +
                             size.correction() - size.size_y()) *
                                kDungeonRatio;
            m_bounds = {x, y, size.size_x() * kDungeonRatio, size.size_y() * kDungeonRatio};
            if (!m_data.isEmpty()) {
                const auto atlas = QDir(m_data).filePath("HyperText/MiniMap/");
                const int count = std::min(cols, 64) * std::min(rows, 64);
                for (int tile = 0; tile < count; ++tile) {
                    float l, t, r, b;
                    client::MinimapWidget::dungeon_tile_rect(
                        tile, size.tile_width(), size.tile_height(), size.size_x(), l, t, r, b);
                    const auto filename = QDir(atlas).filePath("minimap_Dungeon_" + indoor->stem +
                                                               '_' + number(tile));
                    m_tiles.append({QRectF(x + l * kDungeonRatio,
                                           y + t * kDungeonRatio,
                                           (r - l) * kDungeonRatio,
                                           (b - t) * kDungeonRatio),
                                    {filename + ".GTX", filename + ".gtx"}});
                }
            }
        } else {
            m_tiles = m_outdoor_tiles;
            if (index != 0) {
                for (const auto& entry : m_maps.entries()) {
                    if (entry.index() == index) {
                        m_bounds |= QRectF(entry.x() * map::kWorldSize,
                                           -(entry.y() + 1) * map::kWorldSize,
                                           map::kWorldSize,
                                           map::kWorldSize);
                    }
                }
            }
            if (m_bounds.isEmpty()) {
                for (const auto& tile : m_tiles) {
                    m_bounds |= tile.bounds;
                }
            }
        }
        if (m_bounds.isEmpty()) {
            m_bounds = {0, -kExtent, kExtent, kExtent};
        }
        const qreal margin = std::max(m_bounds.width(), m_bounds.height());
        setSceneRect(m_bounds.adjusted(-margin, -margin, margin, margin));
        fit();
    }

    void MapView::set_markers(QVector<Marker> markers) {
        m_markers = std::move(markers);
        viewport()->update();
    }

    void MapView::set_mode(Mode mode) {
        m_mode = mode;
        m_start.reset();
        viewport()->setCursor(mode == Mode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        viewport()->update();
    }

    void MapView::set_terrain_detail(bool enabled) {
        m_terrain_detail = enabled;
        viewport()->update();
    }

    void MapView::fit() {
        m_fitted = true;
        fitInView(m_bounds, Qt::KeepAspectRatio);
        viewport()->update();
    }

    void MapView::focus(QPoint first, std::optional<QPoint> second, qreal minimum_margin) {
        m_fitted = false;
        auto rect = scene_rect(first, second.value_or(first));
        const auto margin = std::max({rect.width() * 0.18, rect.height() * 0.18, minimum_margin});
        fitInView(rect.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
    }

    QPoint MapView::pixel_at(QPoint viewport) const {
        const auto scene = mapToScene(viewport);
        return {qRound(scene.x()), qRound(-scene.y())};
    }

    bool MapView::can_place(QPoint point) const {
        if (point.x() < 0 || point.y() < 0 || point.x() >= kExtent || point.y() >= kExtent ||
            !m_bounds.contains(scene_point(point))) {
            return false;
        }
        if (const auto region = current_region()) {
            const auto* entry =
                m_maps.find_cell(m_index, point.x() / map::kWorldSize, point.y() / map::kWorldSize);
            return entry && map_stem(entry->mapname()) == region->stem &&
                   region_at(m_index, point) == m_region;
        }
        if (m_maps.find_cell(m_index, point.x() / map::kWorldSize, point.y() / map::kWorldSize)) {
            return true;
        }
        return m_index == 0 &&
               (std::any_of(
                    m_tiles.cbegin(),
                    m_tiles.cend(),
                    [&](const Tile& tile) { return tile.bounds.contains(scene_point(point)); }) ||
                !terrain_path({point.x() / map::kWorldSize, point.y() / map::kWorldSize})
                     .isEmpty());
    }

    QString MapView::terrain_path(QPoint cell) const {
        if (!m_data.isEmpty()) {
            const auto name =
                m_maps_subdirectory + "/n_" + number(cell.x()) + '_' + number(cell.y());
            for (const auto* extension : {".kcm", ".KCM"}) {
                const auto path = QDir(m_data).filePath(name + extension);
                if (QFileInfo(path).isFile()) {
                    return path;
                }
            }
        }
        return {};
    }

    QVector<int> MapView::elevations(QPoint point) {
        return can_place(point) ? elevations(m_index, point) : QVector<int>{};
    }

    const map::MapInfo* MapView::indoor_cell(int index, QPoint point) const {
        const auto* cell =
            m_maps.find_cell(index, point.x() / map::kWorldSize, point.y() / map::kWorldSize);
        return cell && !cell->mapname().empty() ? cell : nullptr;
    }

    QVector<int> MapView::elevations(int index, QPoint point) {
        if (point.x() < 0 || point.y() < 0 || point.x() >= kExtent || point.y() >= kExtent) {
            return {};
        }
        if (const auto* cell = indoor_cell(index, point)) {
            const auto stem = map_stem(cell->mapname());
            return dungeon_elevations(
                {coords::even_cell(cell->x()), coords::even_cell(cell->y()), stem, {}}, point);
        }
        if (m_data.isEmpty() || m_maps.has_indoor_mapname(index)) {
            return {};
        }
        const QPoint cell(point.x() / map::kWorldSize, point.y() / map::kWorldSize);
        if (m_height_cell != cell) {
            m_height_cell = cell;
            m_height.patches.clear();
            terrain::HeightPatch patch;
            const auto path = terrain_path(cell);
            if (!path.isEmpty() && terrain::load_kcm(path.toStdString(), patch)) {
                m_height.patches.push_back(std::move(patch));
            }
        }
        const float x = coords::to_client(point.x());
        const float y = coords::to_client(point.y());
        return m_height.covers(x, y) ? QVector<int>{coords::to_server_z(m_height.sample(x, y))}
                                     : QVector<int>{};
    }

    QVector<int> MapView::dungeon_elevations(const Region& region, QPoint point) {
        if (m_map_root.isEmpty()) {
            return {};
        }
        const auto key = QString("%1:%2:%3").arg(region.x).arg(region.y).arg(region.stem);
        auto [entry, inserted] = m_indoor.try_emplace(key);
        if (inserted) {
            const auto root = m_map_root.toStdString();
            const auto file = map::IndoorMap::resolve_map_file(root, region.stem.toStdString());
            if (!file.empty()) {
                (void) entry->second.load(root, file, region.x, region.y);
            }
        }
        const auto* floor = entry->second.floor();
        if (!floor) {
            return {};
        }
        QVector<int> result;
        const float x = static_cast<float>(point.x());
        const float y = static_cast<float>(point.y());
        float top = std::numeric_limits<float>::lowest();
        float bottom = std::numeric_limits<float>::max();
        for (const auto& prop : floor->props()) {
            if (prop.contains_xz(x, y)) {
                top = std::max(top, prop.max_y());
                bottom = std::min(bottom, prop.min_y());
            }
        }
        float origin = top + 1.0f;
        while (origin >= bottom) {
            float hit = 0;
            if (!floor->raycast_walkable(x, origin, y, 0, -1, 0, origin - bottom + 1.0f, hit)) {
                break;
            }
            const float height = origin - hit;
            const auto z = coords::to_server_z(height);
            if (result.isEmpty() || result.back() != z) {
                result.append(z);
            }
            origin = height - 0.25f;
        }
        return result;
    }

    const Marker* MapView::marker_at(QPoint viewport) const {
        const auto point = mapToScene(viewport);
        const qreal minimum = 12 / transform().m11();
        const Marker* closest = nullptr;
        qreal area = std::numeric_limits<qreal>::max();
        for (const auto& marker : m_markers) {
            const auto rect = marker_rect(marker, minimum);
            const qreal size = rect.width() * rect.height();
            if (rect.contains(point) && (size < area || (size == area && marker.active))) {
                closest = &marker;
                area = size;
            }
        }
        return closest;
    }

    void MapView::wheelEvent(QWheelEvent* event) {
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->ignore();
            return;
        }
        m_fitted = false;
        const auto pointer = event->position();
        const auto anchor = viewportTransform().inverted().map(pointer);
        const auto old_scale = transform().m11();
        const auto factor = std::pow(1.25, delta / 120.0);
        const auto next = std::clamp(old_scale * factor, 1.0 / 2048.0, 4.0);
        scale(next / old_scale, next / old_scale);
        const auto offset = viewportTransform().map(anchor) - pointer;
        auto* horizontal = horizontalScrollBar();
        auto* vertical = verticalScrollBar();
        horizontal->setValue(horizontal->value() + qRound(offset.x()));
        vertical->setValue(vertical->value() + qRound(offset.y()));
        event->accept();
    }

    void MapView::mousePressEvent(QMouseEvent* event) {
        if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            m_fitted = false;
            m_pan = event->pos();
            viewport()->setCursor(Qt::ClosedHandCursor);
            event->accept();
        } else if (event->button() == Qt::LeftButton) {
            m_start = pixel_at(event->pos());
            m_cursor = *m_start;
            event->accept();
        } else {
            QGraphicsView::mousePressEvent(event);
        }
    }

    void MapView::mouseMoveEvent(QMouseEvent* event) {
        if (m_pan) {
            const auto delta = event->pos() - *m_pan;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            m_pan = event->pos();
        }
        m_cursor = pixel_at(event->pos());
        emit coordinates(QString("X %1   Y %2   |   Tile %3, %4")
                             .arg(m_cursor.x())
                             .arg(m_cursor.y())
                             .arg(m_cursor.x() / map::kTileSize)
                             .arg(m_cursor.y() / map::kTileSize));
        if (m_start) {
            viewport()->update();
        }
        event->accept();
    }

    void MapView::mouseReleaseEvent(QMouseEvent* event) {
        if (m_pan && (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton)) {
            m_pan.reset();
            viewport()->setCursor(m_mode == Mode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
            event->accept();
            return;
        }
        if (event->button() != Qt::LeftButton || !m_start) {
            QGraphicsView::mouseReleaseEvent(event);
            return;
        }
        const auto first = *m_start;
        const auto last = pixel_at(event->pos());
        m_start.reset();
        if (m_mode == Mode::Select) {
            if (const auto* marker = marker_at(event->pos())) {
                const auto path = marker->path;
                const int spawn = marker->spawn;
                emit marker_selected(path, spawn);
            }
        } else if (can_place(first) && can_place(last)) {
            if (m_mode == Mode::DrawSpawn) {
                const QPoint corner1(first.x(), last.y());
                const QPoint corner2(last.x(), first.y());
                if (can_place(corner1) && can_place(corner2)) {
                    emit spawn_drawn(first, last);
                } else {
                    emit coordinates("Keep the spawn area within the selected map region.");
                }
            } else {
                emit point_placed(last);
            }
        } else {
            emit coordinates("Choose a position within the selected map region.");
        }
        viewport()->update();
        event->accept();
    }

    const QImage* MapView::image(const QStringList& files) {
        const auto& key = files.front();
        if (const auto* cached = m_images.object(key)) {
            return cached;
        }
        QImage image;
        for (const auto& file : files) {
            terrain::ImageRgba rgba;
            if (QFileInfo(file).isFile() && terrain::load_gtx_rgba(file.toStdString(), rgba) &&
                rgba.width >= 16 && rgba.height >= 16) {
                image = QImage(rgba.rgba.data(), rgba.width, rgba.height, QImage::Format_RGBA8888)
                            .copy();
                break;
            }
        }
        const int cost = std::max(1, static_cast<int>(image.sizeInBytes() / 1024));
        m_images.insert(key, std::make_unique<QImage>(std::move(image)).release(), cost);
        return m_images.object(key);
    }

    void MapView::resizeEvent(QResizeEvent* event) {
        QGraphicsView::resizeEvent(event);
        if (m_fitted && !m_bounds.isEmpty()) {
            fit();
        }
    }

    const QImage* MapView::terrain_image(QPoint cell) {
        const auto key = "terrain:" + number(cell.x()) + ':' + number(cell.y());
        if (const auto* cached = m_images.object(key)) {
            return cached;
        }
        auto image = std::make_unique<QImage>();
        terrain::HeightPatch patch;
        terrain::ImageRgba rgba;
        const auto path = terrain_path(cell);
        const auto textures = QDir(m_data).filePath(m_maps_subdirectory + "/Tex");
        if (!path.isEmpty() && terrain::load_kcm(path.toStdString(), patch) &&
            patch.bake_diffuse(textures.toStdString(), m_layers, rgba, 512)) {
            *image = QImage(rgba.rgba.data(), rgba.width, rgba.height, QImage::Format_RGBA8888)
                         .flipped(Qt::Vertical);
        }
        const auto cost = std::max<qsizetype>(1, image->sizeInBytes() / 1024);
        m_images.insert(key, image.release(), cost);
        return m_images.object(key);
    }

    bool MapView::draw_terrain_detail(QPainter* painter, const QRectF& exposed) {
        if (!m_terrain_detail || transform().m11() < 0.085 || m_layers.empty() ||
            m_maps.has_indoor_mapname(m_index)) {
            return false;
        }
        const auto bounds = exposed.intersected(m_bounds);
        if (bounds.isEmpty()) {
            return false;
        }
        const auto cell = [](qreal value) {
            return static_cast<int>(std::floor(value / map::kWorldSize));
        };
        bool rendered = false;
        for (int x = cell(bounds.left()); x <= cell(bounds.right()); ++x) {
            for (int y = cell(-bounds.bottom()); y <= cell(-bounds.top()); ++y) {
                if (const auto* texture = terrain_image({x, y}); texture && !texture->isNull()) {
                    painter->drawImage(QRectF(x * map::kWorldSize,
                                              -(y + 1) * map::kWorldSize,
                                              map::kWorldSize,
                                              map::kWorldSize),
                                       *texture);
                    rendered = true;
                }
            }
        }
        return rendered;
    }

    bool MapView::draw_rendered_map(QPainter* painter) {
        if (!m_terrain_detail || transform().m11() < 0.16 || m_data.isEmpty() ||
            m_detail_unavailable || m_maps.has_indoor_mapname(m_index)) {
            m_detail_timer.stop();
            m_detail_pending.reset();
            return false;
        }
        const auto view = mapToScene(viewport()->rect()).boundingRect().intersected(m_bounds);
        if (view.isEmpty()) {
            return false;
        }
        const qreal pixels_per_unit = transform().m11() * viewport()->devicePixelRatioF();
        const qreal scale =
            std::min(pixels_per_unit, 2560.0 / std::max(view.width(), view.height()));
        const DetailRequest request{
            view, QSize(qCeil(view.width() * scale), qCeil(view.height() * scale))};
        const auto matches = [&](const std::optional<DetailRequest>& other) {
            return other && other->bounds == request.bounds && other->size == request.size;
        };
        if (!matches(m_detail_completed) && !matches(m_detail_pending)) {
            m_detail_pending = request;
            m_detail_timer.start();
        }
        if (m_detail_image.isNull() || !m_detail_bounds.intersects(view)) {
            return false;
        }
        painter->drawImage(m_detail_bounds, m_detail_image);
        return true;
    }

    void MapView::render_map_detail() {
        const auto request = std::exchange(m_detail_pending, std::nullopt);
        if (!request || !m_terrain_detail || m_maps.has_indoor_mapname(m_index)) {
            return;
        }
        m_detail_completed = request;
        if (!m_detail_world) {
            m_detail_world = std::make_unique<world_editor::WorldDocument>();
            QString error;
            if (!m_detail_world->open(m_data, {}, error, m_maps_subdirectory)) {
                m_detail_unavailable = true;
                return;
            }
            m_detail_renderer = std::make_unique<world_editor::WorldViewport>(*m_detail_world);
        }
        constexpr int origin = coords::kDefaultOriginPatch * map::kWorldSize;
        const auto& bounds = request->bounds;
        const QRectF area{
            bounds.left() - origin, -bounds.bottom() - origin, bounds.width(), bounds.height()};
        auto image = m_detail_renderer->render_map(area, request->size);
        if (!image.isNull()) {
            m_detail_bounds = bounds;
            m_detail_image = std::move(image);
            viewport()->update();
        }
    }

    void MapView::drawBackground(QPainter* painter, const QRectF& exposed) {
        painter->fillRect(exposed, QColor("#171f27"));
        bool rendered = false;
        for (const auto& tile : m_tiles) {
            if (tile.bounds.intersects(exposed)) {
                if (const auto* texture = image(tile.files); texture && !texture->isNull()) {
                    painter->drawImage(tile.bounds, *texture);
                    rendered = true;
                }
            }
        }
        painter->save();
        painter->setOpacity(rendered ? 0.52 : 1.0);
        const bool detailed = draw_terrain_detail(painter, exposed);
        painter->restore();
        const bool top_down = draw_rendered_map(painter);
        if (!rendered && !detailed && !top_down) {
            painter->save();
            painter->resetTransform();
            painter->setPen(QColor("#c3ccd5"));
            painter->drawText(
                viewport()->rect().adjusted(16, 12, -16, -12),
                Qt::AlignTop | Qt::AlignLeft,
                "No map imagery in view. Use Fit map or load compatible client assets.");
            painter->restore();
        }
    }

    void MapView::drawForeground(QPainter* painter, const QRectF& exposed) {
        const qreal zoom = transform().m11();
        const auto grid = exposed.intersected(m_bounds);
        if (zoom >= 0.45 && !grid.isEmpty()) {
            QPen pen(QColor(234, 250, 255, 45));
            pen.setCosmetic(true);
            painter->setPen(pen);
            for (qreal x = std::ceil(grid.left() / map::kTileSize) * map::kTileSize;
                 x <= grid.right();
                 x += map::kTileSize) {
                painter->drawLine(QLineF(x, grid.top(), x, grid.bottom()));
            }
            for (qreal y = std::ceil(grid.top() / map::kTileSize) * map::kTileSize;
                 y <= grid.bottom();
                 y += map::kTileSize) {
                painter->drawLine(QLineF(grid.left(), y, grid.right(), y));
            }
        }
        const qreal minimum = 8 / zoom;
        painter->setRenderHint(QPainter::Antialiasing);
        for (int pass = 0; pass <= 1; ++pass) {
            for (const auto& marker : m_markers) {
                if (marker.active != (pass == 1)) {
                    continue;
                }
                const auto rect = marker_rect(marker, minimum);
                if (!rect.intersects(exposed)) {
                    continue;
                }
                const bool npc = marker.spawn < 0;
                QColor line(npc ? "#5be4ed" : "#ffce69");
                QColor fill = line;
                fill.setAlpha(npc ? 215 : (marker.active ? 75 : 26));
                QPen pen(marker.active ? Qt::white : line, marker.active ? 2 : 1);
                pen.setCosmetic(true);
                painter->setPen(pen);
                painter->setBrush(fill);
                if (npc) {
                    painter->drawEllipse(rect);
                } else {
                    painter->drawRect(rect);
                }
                if (marker.active) {
                    painter->save();
                    painter->translate(rect.topRight());
                    painter->scale(1.0 / zoom, 1.0 / zoom);
                    painter->drawText(QPointF(6, -4), marker.name);
                    painter->restore();
                }
            }
        }
        if (m_start && m_mode == Mode::DrawSpawn) {
            QPen pen(QColor("#ffce69"), 2, Qt::DashLine);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->setBrush(QColor(255, 206, 105, 45));
            painter->drawRect(scene_rect(*m_start, m_cursor));
        }
    }

}  // namespace openkal::content_editor
