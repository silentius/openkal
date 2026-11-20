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
#include "MapView.h"
#include "ContentAudit.h"

#include <QMainWindow>

class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;
class QTreeWidget;

namespace openkal::content_editor {
    class JsonEditor;
    class PlacementPreview;

    /** Paths provided by the launch options or the selected OpenKal profile. */
    struct Workspace {
        QString config;
        QString base;
        QString data;
        QString map;
        QString maps_subdirectory = "MAPS";
        QString external_config;
        QString archived_config;
    };

    /** Main window for catalog editing and map-based placement. */
    class ContentWindow : public QMainWindow {
      public:
        /** Creates a window and loads the selected workspace.
         * @param workspace Loose configuration and client asset directories.
         * @param parent Owning widget.
         */
        explicit ContentWindow(Workspace workspace, QWidget* parent = nullptr);

        /** Updates workspace paths from an OpenKal profile.
         * @param workspace Current configuration and asset paths.
         * @param path Selected profile file, or empty to use the runtime default.
         * @param config_override Explicit loose configuration directory supplied at launch.
         * @return Error message, or an empty string on success.
         */
        [[nodiscard]] static QString read_profile(Workspace& workspace,
                                                  const QString& path,
                                                  const QString& config_override = {});

      protected:
        /** @param event Close event to accept after resolving edits. */
        void closeEvent(QCloseEvent* event) override;

      private:
        void build_ui();
        void build_menus();
        void load_workspace(QString selected = {});
        void load_map();
        void open_directory();
        void open_assets();
        void open_profile();
        void refresh_list();
        void filter_list();
        void sync_list_selection(bool reveal);
        void open_record(const QString& path, int spawn = -1);
        void show_record(std::optional<Record> record, int spawn = -1);
        void create_record(bool duplicate = false, bool same_quest = false);
        void delete_record();
        [[nodiscard]] bool save();
        [[nodiscard]] bool confirm_leave();
        [[nodiscard]] bool dirty() const;
        [[nodiscard]] Kind kind() const;
        [[nodiscard]] std::optional<QJsonObject> current_object();
        void set_category(Kind kind);
        void populate_maps();
        void populate_regions(int previous = 0);
        void refresh_selection();
        void refresh_npc_quests(const QJsonObject& object);
        void open_npc_quest();
        void set_npc_quest();
        void refresh_markers();
        [[nodiscard]] bool set_map(int index, QPoint center);
        void locate(bool close = false);
        void begin_placement(MapView::Mode mode, int row = -1);
        void place_point(QPoint point);
        void draw_spawn(QPoint first, QPoint last);
        void remove_spawn();
        void edit_teleport();
        void edit_quest_teleport(const JsonPath& path);
        void open_shop();
        void show_content_audit();
        void show_references();
        void select_reference(bool replace);
        void description(const QString& text);
        void error(const QString& text);

        Workspace m_workspace;
        Catalog m_catalog;
        ContentAudit m_content_audit;
        std::optional<Record> m_record;
        QString m_last_path;
        QStringList m_diagnostics;
        int m_spawn_index = -1;
        int m_placement_row = -1;
        MapView::Mode m_map_mode = MapView::Mode::Select;
        JsonPath m_teleport_path;
        QComboBox* m_category;
        QLineEdit* m_search;
        QCheckBox* m_only_orphans;
        QListWidget* m_list;
        QLabel* m_title;
        QLabel* m_file;
        QLabel* m_map_hint;
        QLabel* m_coordinates;
        QWidget* m_npc_quests;
        QTreeWidget* m_quest_links;
        QPushButton* m_open_quest;
        QPushButton* m_set_quest;
        QJsonObject m_npc_quest_fields;
        JsonEditor* m_editor;
        QWidget* m_map_panel;
        MapView* m_map;
        PlacementPreview* m_preview;
        QComboBox* m_maps;
        QComboBox* m_regions;
        QComboBox* m_spawns;
        QAction* m_new;
        QAction* m_save;
        QAction* m_duplicate;
        QAction* m_delete;
        QAction* m_locate;
        QAction* m_place;
        QAction* m_add_spawn;
        QAction* m_redraw;
        QAction* m_remove_spawn;
        QAction* m_teleport;
        QAction* m_cancel_placement;
        QAction* m_references;
        QAction* m_follow;
        QAction* m_link;
        QAction* m_shop;
        QTimer* m_edits;
    };

}  // namespace openkal::content_editor
