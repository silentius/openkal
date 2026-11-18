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

#include "ObjectCatalog.h"
#include "WorldDocument.h"
#include "WorldViewport.h"

#include <QMainWindow>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace openkal::world_editor {

    /** Desktop window for editing outdoor OpenKal client data. */
    class WorldEditorWindow final : public QMainWindow {
      public:
        /** Builds the editor and opens the initial client data.
         * @param root Client data directory override.
         * @param overlay Optional directory exported by the editor.
         * @param profile_path Explicit OpenKal runtime profile, or empty for the default.
         */
        WorldEditorWindow(const QString& root,
                          const QString& overlay,
                          const QString& profile_path = {});

      protected:
        void closeEvent(QCloseEvent* event) override;

      private:
        void create_actions();
        void create_chunks_dock();
        void create_tools_dock();
        void open_dialog();
        void open_profile_dialog();
        void open_overlay_dialog();
        void
        load_profile(const QString& path, const QString& data_override, const QString& overlay);
        [[nodiscard]] bool
        open_world(const QString& root, const QString& overlay, const QString& profile_path = {});
        [[nodiscard]] bool confirm_unsaved();
        [[nodiscard]] bool export_world();
        void filter_chunks(const QString& filter);
        void filter_objects(const QString& filter);
        void sync_brush();
        void replace_texture();
        void show_navigation_controls();
        void refresh_chunk_status();
        void refresh_actions();

        WorldDocument m_document;
        QString m_profile_path;
        object_view::ObjectCatalog m_catalog;
        WorldViewport* m_view = nullptr;
        QListWidget* m_chunks = nullptr;
        QListWidget* m_objects = nullptr;
        QLineEdit* m_chunk_filter = nullptr;
        QLineEdit* m_object_filter = nullptr;
        QFormLayout* m_form = nullptr;
        QComboBox* m_tool = nullptr;
        QComboBox* m_materials = nullptr;
        QSpinBox* m_radius = nullptr;
        QSpinBox* m_height_step = nullptr;
        QSpinBox* m_level = nullptr;
        QSpinBox* m_opacity = nullptr;
        QLabel* m_chunk_status = nullptr;
        QLabel* m_profile_label = nullptr;
        QLabel* m_selection = nullptr;
        QDoubleSpinBox* m_elevation = nullptr;
        QPushButton* m_replace_texture = nullptr;
        QAction* m_export = nullptr;
        QAction* m_open_overlay = nullptr;
        QAction* m_undo = nullptr;
        QAction* m_redo = nullptr;
        QAction* m_walk_action = nullptr;
        QAction* m_fit_world = nullptr;
    };

}  // namespace openkal::world_editor
