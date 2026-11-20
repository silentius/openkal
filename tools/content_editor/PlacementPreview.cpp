/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "PlacementPreview.h"

#include "MapView.h"
#include "openkal/client/MonsterCatalog.h"
#include "openkal/client/NpcCatalog.h"
#include "openkal/map/Coords.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QOpenGLContext>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace openkal::content_editor {
    namespace {
        using Preview = world_editor::WorldViewport::Preview;

        constexpr float kDungeonRadius = 800;
        constexpr int kWorldExtent = map::kPatchGrid * map::kWorldSize;

        platform::Json json_value(const QJsonObject& object) {
            return platform::Json::parse(
                QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString());
        }

        void npc_model(Preview& scene,
                       const platform::Json& npc,
                       const Catalog& catalog,
                       const std::string& data) {
            const int shape = npc.at("shape").as_int();
            if (shape <= 0 || data.empty()) {
                return;
            }
            auto appearance = npc.at("appearance");
            if (!appearance.is_object()) {
                for (const auto& entry : catalog.records()) {
                    if (entry.kind == Kind::Npc && entry.object.value("shape").toInt() == shape &&
                        entry.object.value("appearance").isObject()) {
                        appearance = json_value(entry.object.value("appearance").toObject());
                        break;
                    }
                }
            }
            const auto rig = client::NpcCatalog::read_appearance(appearance);
            scene.parts = client::NpcCatalog::clothes_gb_paths(data, shape);
            scene.skeleton = client::NpcCatalog::bone_gb(data, rig.bone_no());
            scene.motion = client::NpcCatalog::idle_gb(data, rig.bone_no(), rig.idle_letter());
            scene.scale = rig.scale();
        }

        void monster_model(Preview& scene,
                           const Record& record,
                           const Catalog& catalog,
                           const std::string& data,
                           const QStringList& resource_roots) {
            const auto definition = json_value(record.object);
            auto appearance = definition.at("appearance");
            int clothes = definition.at("index").as_int();
            if (!appearance.is_object()) {
                const int inherited = definition.at("clothesIndex").as_int();
                if (inherited > 0) {
                    clothes = inherited;
                    if (const auto* base =
                            catalog.find(Kind::Monster, QString::number(inherited))) {
                        appearance = json_value(base->object.value("appearance").toObject());
                    }
                }
            }
            const auto rig = client::MonsterCatalog::read_appearance(appearance);
            scene.scale = rig.scale();
            const auto& asset = rig.owned_model();
            if (!asset.empty()) {
                if (asset.compare(0, 12, "RuntimeData/") == 0 &&
                    asset.find("..") == std::string::npos) {
                    for (const auto& root : resource_roots) {
                        const auto path = platform::join_path(root.toStdString(), asset);
                        if (platform::file_exists(path)) {
                            scene.parts.push_back(path);
                            break;
                        }
                    }
                }
                return;
            }
            if (data.empty() || clothes <= 0) {
                return;
            }
            if (definition.at("appearance").is_object() && rig.clothes_index() > 0) {
                clothes = rig.clothes_index();
            }
            scene.parts = client::MonsterCatalog::clothes_gb_paths(data, clothes, false);
            scene.skeleton = client::MonsterCatalog::bone_gb(data, rig.bone_no());
            scene.motion =
                client::MonsterCatalog::motion_gb(data, rig.bone_no(), 0, rig.idle_letter(), 1);
        }

        bool in_world(QPoint point) {
            return point.x() >= 0 && point.x() < kWorldExtent && point.y() >= 0 &&
                   point.y() < kWorldExtent;
        }

        std::optional<int> spawn_center(const QJsonValue& low, const QJsonValue& high) {
            constexpr auto limit = kWorldExtent / map::kTileSize;
            const auto first = low.toInteger(-1);
            const auto last = high.toInteger(-1);
            if (first < 0 || first >= limit || last < 0 || last >= limit) {
                return std::nullopt;
            }
            return static_cast<int>((first + last) * map::kTileSize / 2);
        }

        std::string
        dungeon_model(const std::string& root, const std::string& stem, std::string path) {
            path = platform::Path::normalize_separators(std::move(path));
            if (path.empty()) {
                return {};
            }
            if (!QString::fromStdString(path).endsWith(".gb", Qt::CaseInsensitive)) {
                path += ".gb";
            }
            const auto direct = platform::resolve_path_case(root, path);
            const auto slash = path.find_last_of('/');
            return platform::file_exists(direct)
                       ? direct
                       : platform::resolve_path_case(
                             root,
                             stem + '/' + path.substr(slash == std::string::npos ? 0 : slash + 1));
        }

        QSurfaceFormat preview_format() {
            QSurfaceFormat format;
            format.setVersion(3, 3);
            format.setProfile(QSurfaceFormat::CoreProfile);
            format.setDepthBufferSize(24);
            format.setStencilBufferSize(8);
            return format;
        }
    }  // namespace

    PlacementPreview::PlacementPreview(QWidget* parent) : QWidget(parent) {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 6, 0, 0);
        m_heading = new QLabel(this);
        m_heading->setTextFormat(Qt::PlainText);
        m_heading->setWordWrap(true);
        m_layout->addWidget(m_heading);
        m_status = new QLabel(this);
        m_status->setTextFormat(Qt::PlainText);
        m_status->setWordWrap(true);
        m_layout->addWidget(m_status);
        create_viewport();
        clear();
    }

    void PlacementPreview::set_assets(const QString& data,
                                      const QString& map_root,
                                      const QString& maps_subdirectory,
                                      QStringList resource_roots) {
        m_data = data;
        m_map_root = map_root;
        m_maps_subdirectory = maps_subdirectory;
        m_resource_roots = std::move(resource_roots);
        m_resource_roots.removeAll(QString());
        m_resource_roots.removeDuplicates();
        m_world = {};
        m_world_available = false;
        m_dungeon_path.clear();
        m_dungeon = {};
        if (m_view) {
            QString error;
            m_world_available = m_world.open(m_data, {}, error, m_maps_subdirectory);
            m_view->reset_scene();
        }
        clear();
    }

    void PlacementPreview::create_viewport() {
        QOpenGLContext probe;
        const auto format = preview_format();
        probe.setFormat(format);
        if (!probe.create() || probe.isOpenGLES() ||
            std::make_pair(probe.format().majorVersion(), probe.format().minorVersion()) <
                std::make_pair(3, 3)) {
            return;
        }
        // Adding the first GL widget after show() can recreate the top-level window.
        m_view = std::make_unique<world_editor::WorldViewport>(m_world, this);
        m_view->setFormat(format);
        m_view->setMinimumSize(240, 230);
        m_view->on_message = [this](const QString& message) { m_status->setText(message); };
        m_layout->insertWidget(1, m_view.get(), 1);
    }

    void PlacementPreview::show_entity(const Record& record,
                                       int spawn,
                                       const Catalog& catalog,
                                       MapView& map) {
        if (m_selected == record.path && m_definition == record.object && m_spawn == spawn) {
            return;
        }
        const bool recenter =
            m_selected != record.path ||
            m_definition.value("appearance") != record.object.value("appearance") ||
            m_definition.value("shape") != record.object.value("shape");
        m_selected = record.path;
        m_definition = record.object;
        m_spawn = spawn;
        m_heading->setText("3D preview · " + title(record));

        Preview scene;
        int index = 0;
        std::optional<QPoint> coordinates;
        std::optional<int> elevation;
        QString note;
        if (record.kind == Kind::Npc) {
            if (const auto effective = catalog.resolve_npc(record.object)) {
                const auto npc = json_value(*effective);
                npc_model(scene, npc, catalog, m_data.toStdString());
                index = npc.at("map").as_int();
                const auto& xy = npc.at("xy").array();
                if (xy.size() >= 2) {
                    const QPoint point(xy.at(0).as_int(-1), xy.at(1).as_int(-1));
                    if (in_world(point)) {
                        coordinates = point;
                        if (xy.size() >= 3) {
                            elevation = xy.at(2).as_int();
                        }
                        const auto& dir = npc.at("dir").array();
                        scene.yaw = client::NpcCatalog::yaw_from_dir(
                            coords::kDefaultOriginPatch,
                            point.x(),
                            point.y(),
                            dir.size() >= 2 ? dir.at(0).as_int() : point.x(),
                            dir.size() >= 2 ? dir.at(1).as_int() : point.y() + 1000);
                    }
                }
            } else {
                note = "The relative NPC placement cannot be resolved.";
            }
        } else {
            monster_model(scene, record, catalog, m_data.toStdString(), m_resource_roots);
            const auto spawns = record.object.value("spawns").toArray();
            const int row = spawn < 0 ? 0 : spawn;
            const auto placement = spawns.at(row).toObject();
            const auto rect = placement.value("rect").toArray();
            const auto x = spawn_center(rect.at(0), rect.at(2));
            const auto y = spawn_center(rect.at(1), rect.at(3));
            if (x && y) {
                coordinates = QPoint(*x, *y);
                index = map.spawn_map(placement.value("map").toInt(), *coordinates);
            }
        }

        if (!std::isfinite(scene.scale) || scene.scale <= 0) {
            scene.scale = 1;
        }
        const bool available = static_cast<bool>(m_view);
        if (coordinates) {
            const auto location = index == 0 ? QString("Overworld") : QString("Map %1").arg(index);
            m_heading->setText(m_heading->text() + QString("  ·  %1  ·  X %2  Y %3")
                                                       .arg(location)
                                                       .arg(coordinates->x())
                                                       .arg(coordinates->y()));
            if (!elevation) {
                const auto heights = map.elevations(index, *coordinates);
                if (!heights.isEmpty()) {
                    elevation = heights.front();
                }
            }
            if (elevation) {
                scene.position = {coords::to_client(coordinates->x()),
                                  coords::to_client_y(*elevation),
                                  coords::to_client(coordinates->y())};
                m_heading->setText(m_heading->text() + QString("  Z %1").arg(*elevation));
                if (available) {
                    if (const auto* cell = map.indoor_cell(index, *coordinates)) {
                        load_surroundings(scene, *cell);
                        if (scene.surroundings.empty()) {
                            note = "The dungeon surroundings are unavailable.";
                        }
                    } else {
                        load_terrain(scene);
                        if (!scene.outdoor) {
                            note = "The outdoor terrain is unavailable at this position.";
                        }
                    }
                }
            } else {
                note = "No ground height is available at this position.";
            }
        } else if (note.isEmpty()) {
            note = record.kind == Kind::Npc ? "This NPC has no position."
                                            : "This monster has no spawn position.";
        }
        if (available) {
            m_status->setText(
                note.isEmpty() ? "Left-drag to orbit · Right-drag to pan · Scroll to zoom" : note);
            m_view->set_preview(std::move(scene), recenter);
        } else {
            m_status->setText("3D preview requires OpenGL 3.3.");
        }
        show();
    }

    void PlacementPreview::clear() {
        m_selected.clear();
        if (m_view) {
            m_heading->setText("3D preview");
            m_status->setText("Select an NPC or monster to preview.");
            m_view->clear_preview();
        } else {
            hide();
        }
    }

    void PlacementPreview::load_terrain(Preview& scene) {
        if (m_world_available) {
            const auto coordinate =
                world_editor::WorldDocument::coordinate_at(scene.position.x(), scene.position.z());
            scene.outdoor = m_world.load(coordinate) != nullptr;
        }
    }

    void PlacementPreview::load_surroundings(Preview& scene, const map::MapInfo& cell) {
        if (m_map_root.isEmpty()) {
            return;
        }
        const auto root = m_map_root.toStdString();
        const auto path = map::IndoorMap::resolve_map_file(root, cell.mapname());
        if (path.empty()) {
            return;
        }
        if (path != m_dungeon_path) {
            if (!m_dungeon.load(path)) {
                m_dungeon_path.clear();
                return;
            }
            m_dungeon_path = path;
        }
        const auto origin =
            QVector3D(coords::to_client(coords::cell_origin_pixels(coords::even_cell(cell.x()))),
                      0,
                      coords::to_client(coords::cell_origin_pixels(coords::even_cell(cell.y()))));
        const auto local = scene.position - origin;
        QMatrix4x4 transform;
        transform.translate(origin);
        for (const auto& region : m_dungeon.regions()) {
            const auto& min = region.minimum();
            const auto& max = region.maximum();
            if (local.x() < min[0] - kDungeonRadius || local.x() > max[0] + kDungeonRadius ||
                local.z() < min[2] - kDungeonRadius || local.z() > max[2] + kDungeonRadius) {
                continue;
            }
            if (const auto file = dungeon_model(root, m_dungeon.stem(), region.name());
                !file.empty()) {
                scene.surroundings.push_back({file, transform});
            }
        }
        for (const auto& object : m_dungeon.objects()) {
            const auto& m = object.matrix();
            if (std::abs(m[9] - local.x()) > kDungeonRadius ||
                std::abs(m[11] - local.z()) > kDungeonRadius) {
                continue;
            }
            const auto file = dungeon_model(root, m_dungeon.stem(), object.path());
            if (file.empty()) {
                continue;
            }
            const QMatrix4x4 placement(m[0],
                                       m[3],
                                       m[6],
                                       m[9],
                                       m[1],
                                       m[4],
                                       m[7],
                                       m[10],
                                       m[2],
                                       m[5],
                                       m[8],
                                       m[11],
                                       0,
                                       0,
                                       0,
                                       1);
            scene.surroundings.push_back({file, transform * placement});
        }
    }

}  // namespace openkal::content_editor
