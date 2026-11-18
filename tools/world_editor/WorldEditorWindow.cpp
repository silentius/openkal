/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "WorldEditorWindow.h"

#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

namespace openkal::world_editor {
    namespace {

        using Tool = WorldViewport::Tool;

        QLabel* wrapped_label(const QString& value, QWidget* parent) {
            auto* label = new QLabel(value, parent);
            label->setWordWrap(true);
            return label;
        }

    }  // namespace

    WorldEditorWindow::WorldEditorWindow(const QString& root,
                                         const QString& overlay,
                                         const QString& profile_path) {
        resize(1430, 860);
        m_view = new WorldViewport(m_document, this);
        setCentralWidget(m_view);
        create_actions();
        create_chunks_dock();
        create_tools_dock();
        m_view->on_message = [this](const QString& message) {
            statusBar()->showMessage(message, 8000);
        };
        m_view->on_change = [this] { refresh_actions(); };
        m_view->on_selection = [this](const QString& text, std::optional<float> height) {
            m_selection->setText(text.isEmpty() ? "No object selected" : text);
            m_elevation->setEnabled(height.has_value());
            const QSignalBlocker blocker(m_elevation);
            m_elevation->setValue(height.value_or(0));
        };
        m_view->on_walk_mode = [this](bool walking, bool captured) {
            m_walk_action->setChecked(walking);
            refresh_actions();
            if (walking) {
                statusBar()->showMessage(
                    captured
                        ? "Walk Mode · WASD to move · Shift to run · Mouse to look · Esc to exit"
                        : "Walk Mode · WASD to move · Shift to run · Right drag to look · Esc to exit");
            } else {
                show_navigation_controls();
            }
        };
        m_view->on_walk_chunk = [this](QPoint coordinate) {
            const auto items =
                m_chunks->findItems(WorldDocument::stem(coordinate), Qt::MatchExactly);
            if (!items.isEmpty()) {
                const QSignalBlocker blocker(m_chunks);
                m_chunks->setCurrentItem(items.front());
                m_chunks->scrollToItem(items.front());
            }
        };
        m_view->on_visible_chunks = [this] { refresh_chunk_status(); };

        statusBar()->showMessage("Open an OpenKal profile or a client data directory.");
        if (!root.isEmpty() && profile_path.isEmpty()) {
            (void) open_world(root, overlay);
        } else {
            load_profile(profile_path, root, overlay);
        }
        sync_brush();
        refresh_actions();
    }

    void WorldEditorWindow::create_actions() {
        auto* file = menuBar()->addMenu("&File");
        auto* toolbar = addToolBar("World");
        toolbar->setMovable(false);

        auto* open_profile = file->addAction(
            "Open &Profile…", QKeySequence::Open, this, &WorldEditorWindow::open_profile_dialog);
        file->addAction("Open Client &Data…", this, &WorldEditorWindow::open_dialog);
        m_open_overlay =
            file->addAction("Open &Export…", this, &WorldEditorWindow::open_overlay_dialog);
        m_export = file->addAction(
            "&Export Changed Files…", QKeySequence::Save, this, [this] { (void) export_world(); });
        file->addSeparator();
        file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
        toolbar->addAction(open_profile);
        toolbar->addAction(m_export);
        toolbar->addSeparator();

        auto* edit = menuBar()->addMenu("&Edit");
        const auto history = [this](bool undo) {
            m_view->finish_edit();
            if (undo) {
                m_document.undo();
            } else {
                m_document.redo();
            }
            m_view->clear_selection();
        };
        m_undo = edit->addAction("&Undo", QKeySequence::Undo, this, [history] { history(true); });
        m_redo = edit->addAction("&Redo", QKeySequence::Redo, this, [history] { history(false); });
        toolbar->addAction(m_undo);
        toolbar->addAction(m_redo);

        auto* remove = new QAction("Delete Selected Object", m_view);
        remove->setShortcut(QKeySequence(Qt::Key_Delete));
        remove->setShortcutContext(Qt::WidgetShortcut);
        connect(remove, &QAction::triggered, m_view, &WorldViewport::remove_selection);
        m_view->addAction(remove);
        edit->addSeparator();
        edit->addAction("Delete Selected Object", m_view, &WorldViewport::remove_selection);

        auto* view = menuBar()->addMenu("&View");
        m_fit_world = view->addAction("&Fit World", m_view, &WorldViewport::fit_world);
        m_walk_action = view->addAction(
            "&Walk Mode", QKeySequence(Qt::Key_F), m_view, &WorldViewport::set_walking);
        m_walk_action->setCheckable(true);
        toolbar->addSeparator();
        toolbar->addAction(m_walk_action);
        auto* invert =
            view->addAction("Invert Mouse &Y", m_view, &WorldViewport::set_invert_mouse_y);
        invert->setCheckable(true);
        auto* water =
            view->addAction("Show &Water Surface", m_view, &WorldViewport::set_water_visible);
        water->setCheckable(true);
        water->setChecked(true);
        auto* invisible = view->addAction(
            "Show Invisible &Placements", m_view, &WorldViewport::set_invisible_placements_visible);
        invisible->setCheckable(true);
    }

    void WorldEditorWindow::create_chunks_dock() {
        auto* dock = new QDockWidget("World Chunks", this);
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        auto* panel = new QWidget(dock);
        dock->setWidget(panel);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        auto* layout = new QVBoxLayout(panel);

        m_chunk_filter = new QLineEdit(panel);
        m_chunk_filter->setPlaceholderText("Find a chunk…");
        m_chunk_filter->setClearButtonEnabled(true);
        layout->addWidget(m_chunk_filter);
        m_chunks = new QListWidget(panel);
        m_chunks->setMinimumWidth(150);
        m_chunks->setUniformItemSizes(true);
        layout->addWidget(m_chunks, 1);
        m_chunk_status = wrapped_label({}, panel);
        layout->addWidget(m_chunk_status);
        m_profile_label = wrapped_label({}, panel);
        m_profile_label->setTextFormat(Qt::PlainText);
        m_profile_label->hide();
        layout->addWidget(m_profile_label);

        connect(m_chunk_filter, &QLineEdit::textChanged, this, &WorldEditorWindow::filter_chunks);
        connect(m_chunks, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            m_view->focus_chunk(item->data(Qt::UserRole).toPoint());
            m_view->setFocus();
        });
        connect(m_chunks, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
            if (!item) {
                return;
            }
            const auto coordinate = item->data(Qt::UserRole).toPoint();
            m_view->focus_chunk(coordinate);
            sync_brush();
            refresh_chunk_status();
            const auto warning = m_view->placement_warning();
            if (!m_document.chunk(coordinate)) {
                statusBar()->showMessage(QString("Could not load %1.").arg(item->text()), 8000);
            } else if (!warning.isEmpty()) {
                statusBar()->showMessage(warning, 8000);
            } else {
                show_navigation_controls();
            }
        });
    }

    void WorldEditorWindow::create_tools_dock() {
        auto* dock = new QDockWidget("Editing Tools", this);
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        auto* panel = new QWidget(dock);
        panel->setMinimumWidth(255);
        dock->setWidget(panel);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        auto* layout = new QVBoxLayout(panel);
        m_form = new QFormLayout();
        layout->addLayout(m_form);
        m_tool = new QComboBox(panel);
        for (const auto& [name, tool] : {std::pair{"Select / Move Object", Tool::Select},
                                         {"Raise Terrain", Tool::Raise},
                                         {"Lower Terrain", Tool::Lower},
                                         {"Flatten Terrain", Tool::Flatten},
                                         {"Paint Texture", Tool::Texture},
                                         {"Place Object", Tool::Place}}) {
            m_tool->addItem(name, static_cast<int>(tool));
        }
        m_form->addRow("Tool", m_tool);

        m_radius = new QSpinBox(panel);
        m_radius->setRange(32, 4096);
        m_radius->setSingleStep(32);
        m_radius->setValue(256);
        m_form->addRow("Brush radius", m_radius);
        m_height_step = new QSpinBox(panel);
        m_height_step->setRange(1, 8192);
        m_height_step->setValue(48);
        m_form->addRow("Height step", m_height_step);
        m_level = new QSpinBox(panel);
        m_level->setRange(0, 65535);
        m_form->addRow("Target height", m_level);
        m_opacity = new QSpinBox(panel);
        m_opacity->setRange(1, 100);
        m_opacity->setSuffix("%");
        m_opacity->setValue(65);
        m_form->addRow("Opacity", m_opacity);
        m_materials = new QComboBox(panel);
        m_materials->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_materials->setMinimumContentsLength(12);
        m_form->addRow("Texture", m_materials);
        m_replace_texture = new QPushButton("Replace Layer in Active Chunk…", panel);
        m_form->addRow(m_replace_texture);
        connect(
            m_replace_texture, &QPushButton::clicked, this, &WorldEditorWindow::replace_texture);

        layout->addSpacing(10);
        auto* catalog_label = new QLabel("Object Catalog", panel);
        layout->addWidget(catalog_label);
        m_object_filter = new QLineEdit(panel);
        m_object_filter->setPlaceholderText("Filter model paths…");
        m_object_filter->setClearButtonEnabled(true);
        layout->addWidget(m_object_filter);
        m_objects = new QListWidget(panel);
        m_objects->setUniformItemSizes(true);
        layout->addWidget(m_objects, 1);
        layout->addWidget(wrapped_label("Double-click a model to switch to Place Object.", panel));

        m_selection = wrapped_label("No object selected", panel);
        m_selection->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addSpacing(10);
        layout->addWidget(m_selection);
        auto* placement_form = new QFormLayout();
        m_elevation = new QDoubleSpinBox(panel);
        m_elevation->setRange(-131072, 131072);
        m_elevation->setDecimals(1);
        m_elevation->setSingleStep(16);
        m_elevation->setKeyboardTracking(false);
        m_elevation->setEnabled(false);
        placement_form->addRow("Elevation (Y)", m_elevation);
        layout->addLayout(placement_form);
        connect(m_elevation,
                &QDoubleSpinBox::valueChanged,
                m_view,
                &WorldViewport::set_selection_height);
        auto* remove = new QPushButton("Delete Selected Object", panel);
        layout->addWidget(remove);
        connect(remove, &QPushButton::clicked, m_view, &WorldViewport::remove_selection);

        connect(m_object_filter, &QLineEdit::textChanged, this, &WorldEditorWindow::filter_objects);
        connect(m_objects, &QListWidget::currentItemChanged, this, &WorldEditorWindow::sync_brush);
        connect(m_objects, &QListWidget::itemDoubleClicked, this, [this] {
            m_tool->setCurrentIndex(m_tool->findData(static_cast<int>(Tool::Place)));
            m_view->setFocus();
        });
        for (auto* box : {m_tool, m_materials}) {
            connect(box, &QComboBox::currentIndexChanged, this, &WorldEditorWindow::sync_brush);
        }
        for (auto* spin : {m_radius, m_height_step, m_level, m_opacity}) {
            connect(spin, &QSpinBox::valueChanged, this, &WorldEditorWindow::sync_brush);
        }
    }

    void WorldEditorWindow::open_dialog() {
        const auto directory =
            QFileDialog::getExistingDirectory(this, "Open Client Data", m_document.root());
        if (!directory.isEmpty() && confirm_unsaved()) {
            (void) open_world(directory, {});
        }
    }

    void WorldEditorWindow::open_profile_dialog() {
        const auto start = m_profile_path.isEmpty() ? m_document.root() : m_profile_path;
        const auto path = QFileDialog::getOpenFileName(
            this, "Open OpenKal Profile", start, "OpenKal profile (*.profile);;All files (*)");
        if (!path.isEmpty() && confirm_unsaved()) {
            load_profile(path, {}, {});
        }
    }

    void WorldEditorWindow::open_overlay_dialog() {
        const auto directory =
            QFileDialog::getExistingDirectory(this, "Open Exported Patch Directory");
        if (!directory.isEmpty() && confirm_unsaved()) {
            (void) open_world(m_document.root(), directory, m_profile_path);
        }
    }

    void WorldEditorWindow::load_profile(const QString& path,
                                         const QString& data_override,
                                         const QString& overlay) {
        const bool show_errors = !path.isEmpty() || !overlay.isEmpty() ||
                                 !qEnvironmentVariableIsEmpty("OPENKAL_PROFILE");
        std::string error;
        const auto runtime =
            platform::RuntimeProfile::load_for_tool(OPENKAL_REPO_ROOT, path.toStdString(), &error);
        if (!runtime) {
            if (show_errors) {
                QMessageBox::warning(this, "Profile Load Failed", QString::fromStdString(error));
            }
            return;
        }

        const auto root = data_override.isEmpty()
                              ? QString::fromStdString(runtime->profile.data_root())
                              : data_override;
        if (show_errors || QFileInfo(root).isDir()) {
            (void) open_world(root, overlay, QString::fromStdString(runtime->profile_path));
        }
    }

    bool WorldEditorWindow::open_world(const QString& root,
                                       const QString& overlay,
                                       const QString& profile_path) {
        m_view->finish_edit();
        QString error;
        if (!m_document.open(root, overlay, error)) {
            QMessageBox::warning(this, "Could Not Open World", error);
            return false;
        }
        m_profile_path = profile_path;
        m_profile_label->setText(QString("Profile: %1").arg(QFileInfo(m_profile_path).fileName()));
        m_profile_label->setToolTip(m_profile_path);
        m_profile_label->setVisible(!m_profile_path.isEmpty());
        m_view->reset_scene();

        m_materials->clear();
        const auto& layers = m_document.layers();
        for (std::size_t i = 0; i < std::min<std::size_t>(layers.size(), 255); ++i) {
            const auto& name = layers[i].texture_name;
            if (!name.empty()) {
                m_materials->addItem(QString("%1 · %2").arg(i).arg(QString::fromStdString(name)),
                                     static_cast<int>(i));
            }
        }
        if (!m_materials->count()) {
            m_materials->addItem("No textures found in n.env", -1);
        }

        const auto objects =
            platform::resolve_path_case(m_document.root().toStdString(), "OBJECTS");
        (void) m_catalog.scan(objects);
        m_object_filter->clear();
        filter_objects({});
        m_chunk_filter->clear();
        m_chunks->clear();
        const auto coordinates = m_document.coordinates();
        const auto closest =
            std::min_element(coordinates.begin(), coordinates.end(), [](QPoint a, QPoint b) {
                const auto center = QPoint(map::kPatchGrid / 2, map::kPatchGrid / 2);
                return (a - center).manhattanLength() < (b - center).manhattanLength();
            });
        QListWidgetItem* initial = nullptr;
        for (const auto coordinate : coordinates) {
            auto* item = new QListWidgetItem(WorldDocument::stem(coordinate), m_chunks);
            item->setData(Qt::UserRole, coordinate);
            if (coordinate == *closest) {
                initial = item;
            }
        }
        m_chunks->setCurrentItem(initial);
        m_chunks->scrollToItem(initial);
        m_view->setFocus();
        sync_brush();
        refresh_actions();
        return true;
    }

    void WorldEditorWindow::filter_chunks(const QString& filter) {
        for (int i = 0; i < m_chunks->count(); ++i) {
            auto* item = m_chunks->item(i);
            item->setHidden(!item->text().contains(filter.trimmed(), Qt::CaseInsensitive));
        }
    }

    void WorldEditorWindow::filter_objects(const QString& filter) {
        m_catalog.set_filter(filter.toStdString());
        const auto previous =
            m_objects->currentItem() ? m_objects->currentItem()->text() : QString();
        const QSignalBlocker blocker(m_objects);
        m_objects->clear();
        QListWidgetItem* selected = nullptr;
        for (std::size_t i = 0; i < m_catalog.visible_count(); ++i) {
            const auto& entry = m_catalog.visible_entry(static_cast<int>(i));
            const auto path = QString::fromStdString(entry.relative_path());
            auto* item = new QListWidgetItem(path, m_objects);
            item->setToolTip(path);
            if (path == previous) {
                selected = item;
            }
        }
        m_objects->setCurrentItem(selected ? selected : m_objects->item(0));
        m_object_filter->setPlaceholderText(m_catalog.empty() ? "No GB assets found in OBJECTS"
                                                              : "Filter model paths…");
        sync_brush();
    }

    void WorldEditorWindow::sync_brush() {
        WorldViewport::Brush brush;
        brush.tool = static_cast<Tool>(m_tool->currentData().toInt());
        brush.radius = m_radius->value();
        brush.height_step = m_height_step->value();
        brush.level = m_level->value();
        brush.opacity = m_opacity->value() / 100.0f;
        brush.material = m_materials->count() ? m_materials->currentData().toInt() : -1;
        if (const auto* item = m_objects->currentItem()) {
            brush.asset = "OBJECTS/" + item->text().toStdString();
        }

        const bool texture = brush.tool == Tool::Texture;
        const bool height =
            brush.tool == Tool::Raise || brush.tool == Tool::Lower || brush.tool == Tool::Flatten;
        m_form->setRowVisible(m_radius, texture || height);
        m_form->setRowVisible(m_height_step, height);
        m_form->setRowVisible(m_level, brush.tool == Tool::Flatten);
        m_form->setRowVisible(m_opacity, texture);
        m_form->setRowVisible(m_materials, texture);
        m_form->setRowVisible(m_replace_texture, texture);
        m_replace_texture->setEnabled(brush.material >= 0 && m_chunks->currentItem());
        m_view->set_brush(std::move(brush));
    }

    void WorldEditorWindow::replace_texture() {
        const auto* active = m_chunks->currentItem();
        if (!active) {
            return;
        }
        const auto coordinate = active->data(Qt::UserRole).toPoint();
        const auto* chunk = m_document.chunk(coordinate);
        if (!chunk) {
            return;
        }
        const int current = m_materials->currentData().toInt();
        const auto& layers = chunk->terrain.layers;
        if (std::find(layers.begin(), layers.end(), current) != layers.end()) {
            statusBar()->showMessage(
                "Choose a texture outside the active chunk's palette to replace a layer.", 8000);
            return;
        }
        QStringList choices;
        const auto& catalog = m_document.layers();
        for (auto layer : layers) {
            const auto label = layer < catalog.size()
                                   ? QString::fromStdString(catalog[layer].texture_name)
                                   : QString("Missing texture");
            choices.append(QString("%1 · %2").arg(layer).arg(label));
        }
        bool accepted = false;
        const auto choice =
            QInputDialog::getItem(this,
                                  "Replace Terrain Layer",
                                  QString("Replace a layer in %1 with %2.")
                                      .arg(active->text(), m_materials->currentText()),
                                  choices,
                                  0,
                                  false,
                                  &accepted);
        const auto index = choices.indexOf(choice);
        if (accepted && index >= 0 &&
            m_document.replace_texture(
                coordinate, layers[index], static_cast<std::uint8_t>(current))) {
            statusBar()->showMessage(QString("Replaced %1 in %2.").arg(choice, active->text()),
                                     8000);
            m_view->update();
            refresh_actions();
        }
    }

    void WorldEditorWindow::show_navigation_controls() {
        statusBar()->showMessage(
            "W/S to zoom at cursor · A/D to pan · Right drag to pan · Left + right drag to orbit · F to walk");
    }

    void WorldEditorWindow::refresh_chunk_status() {
        m_chunk_status->setText(QString("%1 visible · %2 available")
                                    .arg(m_view->visible_count())
                                    .arg(m_chunks->count()));
    }

    void WorldEditorWindow::refresh_actions() {
        const bool opened = !m_document.root().isEmpty();
        m_open_overlay->setEnabled(opened);
        m_export->setEnabled(opened);
        m_walk_action->setEnabled(opened);
        m_fit_world->setEnabled(opened);
        m_undo->setEnabled(!m_walk_action->isChecked() && m_document.can_undo());
        m_redo->setEnabled(!m_walk_action->isChecked() && m_document.can_redo());
        const auto name = opened ? QDir(m_document.root()).dirName() + " — " : QString();
        setWindowTitle(name + "OpenKal World Editor" + (m_document.dirty() ? " *" : ""));
    }

    bool WorldEditorWindow::export_world() {
        m_view->finish_edit();
        const auto destination = QFileDialog::getSaveFileName(
            this,
            "Export to a New Directory",
            QDir(m_document.root()).filePath("../openkal-world-export"),
            {},
            nullptr,
            QFileDialog::DontConfirmOverwrite);
        if (destination.isEmpty()) {
            return false;
        }
        QString error;
        const int count = m_document.export_to(destination, error);
        if (count < 0) {
            QMessageBox::warning(this, "Could Not Export World", error);
            return false;
        }
        statusBar()->showMessage(count == 0 ? "No changes to export."
                                            : QString("Exported %1 map files to %2.")
                                                  .arg(count)
                                                  .arg(QDir::toNativeSeparators(destination)),
                                 10000);
        refresh_actions();
        return true;
    }

    bool WorldEditorWindow::confirm_unsaved() {
        m_view->finish_edit();
        if (!m_document.dirty()) {
            return true;
        }
        QMessageBox dialog(QMessageBox::Warning,
                           "Unexported World Edits",
                           "Export the current edits before leaving this world?",
                           QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                           this);
        dialog.setDefaultButton(QMessageBox::Save);
        dialog.button(QMessageBox::Save)->setText("Export…");
        const auto choice = dialog.exec();
        return choice == QMessageBox::Discard || (choice == QMessageBox::Save && export_world());
    }

    void WorldEditorWindow::closeEvent(QCloseEvent* event) {
        m_view->set_walking(false);
        if (confirm_unsaved()) {
            event->accept();
        } else {
            event->ignore();
        }
    }

}  // namespace openkal::world_editor
