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

#include "Catalog.h"
#include "WorldViewport.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/map/MapCatalog.h"

#include <QWidget>

#include <memory>

class QLabel;
class QVBoxLayout;

namespace openkal::content_editor {
    class MapView;

    /** Read-only 3D scene for a selected NPC or monster spawn. */
    class PlacementPreview final : public QWidget {
        Q_OBJECT

      public:
        /** Creates a panel that follows the editor selection.
         * @param parent Owning widget.
         */
        explicit PlacementPreview(QWidget* parent = nullptr);

        /** Changes the sources used for terrain, dungeon geometry and models.
         * @param data Client data directory.
         * @param map_root Client dungeon map directory.
         * @param maps_subdirectory Terrain directory relative to data.
         * @param resource_roots Roots that may contain RuntimeData models.
         */
        void set_assets(const QString& data,
                        const QString& map_root,
                        const QString& maps_subdirectory,
                        QStringList resource_roots);

        /** Updates the preview from the active draft.
         * @param record Selected NPC or monster definition.
         * @param spawn Selected monster spawn index, or -1 to use its first spawn.
         * @param catalog Catalog used for inherited appearance and positioning.
         * @param map Map metadata and height sampler for the workspace.
         */
        void show_entity(const Record& record, int spawn, const Catalog& catalog, MapView& map);

        /** Clears the selection from the preview. */
        void clear();

      private:
        void create_viewport();
        void load_surroundings(world_editor::WorldViewport::Preview& scene,
                               const map::MapInfo& cell);
        void load_terrain(world_editor::WorldViewport::Preview& scene);

        world_editor::WorldDocument m_world;
        QString m_data;
        QString m_map_root;
        QString m_maps_subdirectory;
        QStringList m_resource_roots;
        std::string m_dungeon_path;
        map::IndoorMap m_dungeon;
        QString m_selected;
        QJsonObject m_definition;
        int m_spawn = -1;
        bool m_world_available = false;
        QLabel* m_heading;
        QLabel* m_status;
        QVBoxLayout* m_layout;
        std::unique_ptr<world_editor::WorldViewport> m_view;
    };

}  // namespace openkal::content_editor
