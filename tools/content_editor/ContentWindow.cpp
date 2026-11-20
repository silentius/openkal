/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ContentWindow.h"

#include "JsonEditor.h"
#include "PlacementPreview.h"
#include "QuestEditor.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <utility>

namespace openkal::content_editor {
    namespace {
        std::optional<QPoint> position(const QJsonValue& value) {
            const auto xy = value.toArray();
            if (xy.size() < 2 || xy.at(0).toInteger(-1) < 0 || xy.at(1).toInteger(-1) < 0 ||
                xy.at(0).toInteger() >= map::kPatchGrid * map::kWorldSize ||
                xy.at(1).toInteger() >= map::kPatchGrid * map::kWorldSize) {
                return std::nullopt;
            }
            return QPoint(xy.at(0).toInt(), xy.at(1).toInt());
        }

        std::optional<std::pair<QPoint, QPoint>> spawn_bounds(const QJsonObject& spawn) {
            const auto rect = spawn.value("rect").toArray();
            constexpr int tile_limit = map::kPatchGrid * map::kTileCount;
            if (rect.size() < 4) {
                return std::nullopt;
            }
            for (int i = 0; i < 4; ++i) {
                if (rect.at(i).toInteger(-1) < 0 || rect.at(i).toInteger() >= tile_limit) {
                    return std::nullopt;
                }
            }
            return std::make_pair(QPoint(rect.at(0).toInt(), rect.at(1).toInt()) * map::kTileSize,
                                  QPoint(rect.at(2).toInt(), rect.at(3).toInt()) * map::kTileSize);
        }

        QString new_id(QWidget* parent, Kind kind) {
            bool ok = false;
            const int upper = kind == Kind::Quest || kind == Kind::Task || kind == Kind::Item
                                  ? 65535
                                  : std::numeric_limits<int>::max();
            const int value = QInputDialog::getInt(
                parent, "New definition", "ID", 1, kind == Kind::Quest ? 0 : 1, upper, 1, &ok);
            return ok ? QString::number(value) : QString{};
        }

        QString existing_config(const QString& path) {
            const QDir directory(path);
            const auto nested = directory.filePath("config");
            return QFileInfo(nested).isDir() ? nested : directory.absolutePath();
        }

        QString relative_item_directory(const QJsonObject& item) {
            QStringList subfolders;
            for (const auto& part : item.value("class").toArray()) {
                const auto name = part.toString();
                if (name.isEmpty() || name == "." || name == ".." || name.contains('/') ||
                    name.contains('\\') || name.contains(':')) {
                    return {};
                }
                subfolders.append(name);
            }
            return subfolders.join('/');
        }

        void details(QWidget* parent,
                     const QString& title,
                     const QString& text,
                     const QStringList& rows) {
            QMessageBox dialog(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
            if (!rows.isEmpty()) {
                dialog.setDetailedText(rows.join('\n'));
            }
            dialog.exec();
        }
    }  // namespace

    ContentWindow::ContentWindow(Workspace workspace, QWidget* parent)
        : QMainWindow(parent), m_workspace(std::move(workspace)) {
        build_ui();
        build_menus();
        load_workspace();
        resize(1540, 900);
    }

    QString ContentWindow::read_profile(Workspace& workspace,
                                        const QString& path,
                                        const QString& config_override) {
        const auto selected = path.isEmpty()
                                  ? platform::Profile::default_path(OPENKAL_RESOURCE_ROOT)
                                  : path.toStdString();
        auto loose_root = config_override;
        const auto loose_profile = platform::Profile::load(selected);
        if (loose_root.isEmpty() && loose_profile && loose_profile->configuration_root().empty()) {
            loose_root = workspace.external_config;
        }

        std::string error;
        const auto runtime = platform::RuntimeProfile::load_for_tool(
            OPENKAL_RESOURCE_ROOT, selected, &error, loose_root.toStdString());
        if (!runtime) {
            return QString::fromStdString(error);
        }
        const auto& configuration = runtime->configuration;
        if (configuration.archive_path && !runtime->requested_configuration_root.empty()) {
            const auto root = QString::fromStdString(runtime->requested_configuration_root);
            return "The profile's configuration directory does not exist: " +
                   QDir(OPENKAL_RESOURCE_ROOT).absoluteFilePath(root);
        }

        workspace.data = QString::fromStdString(runtime->profile.data_root());
        workspace.map = QString::fromStdString(runtime->profile.map_root());
        workspace.maps_subdirectory = QString::fromStdString(runtime->profile.maps_subdir());
        workspace.config.clear();
        workspace.archived_config.clear();
        const auto root = QString::fromStdString(configuration.configuration_root);
        if (!configuration.archive_path) {
            workspace.config = existing_config(root);
        } else if (!workspace.external_config.isEmpty()) {
            workspace.config = workspace.external_config;
            platform::ConfigArchive::unmount();
        } else {
            workspace.archived_config = root;
        }
        return {};
    }

    void ContentWindow::build_ui() {
        auto* split = new QSplitter(this);
        split->setChildrenCollapsible(false);
        setCentralWidget(split);
        auto* browser = new QWidget(split);
        auto* browser_layout = new QVBoxLayout(browser);
        m_category = new QComboBox(browser);
        for (const auto category : kinds()) {
            m_category->addItem(label(category), static_cast<int>(category));
        }
        browser_layout->addWidget(m_category);
        m_search = new QLineEdit(browser);
        m_search->setClearButtonEnabled(true);
        m_search->setPlaceholderText("Filter by name or ID");
        browser_layout->addWidget(m_search);
        m_only_orphans = new QCheckBox("Possible orphans only", browser);
        m_only_orphans->setToolTip("Show entries with no known use in the loaded definitions.");
        browser_layout->addWidget(m_only_orphans);
        m_list = new QListWidget(browser);
        m_list->setAlternatingRowColors(true);
        m_list->setUniformItemSizes(true);
        m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        browser_layout->addWidget(m_list, 1);

        auto* inspector = new QWidget(split);
        auto* inspector_layout = new QVBoxLayout(inspector);
        m_title = new QLabel(inspector);
        QFont heading = m_title->font();
        heading.setBold(true);
        heading.setPointSize(heading.pointSize() + 4);
        m_title->setFont(heading);
        m_title->setWordWrap(true);
        m_title->setTextFormat(Qt::PlainText);
        inspector_layout->addWidget(m_title);
        m_file = new QLabel(inspector);
        m_file->setWordWrap(true);
        m_file->setTextFormat(Qt::PlainText);
        m_file->setTextInteractionFlags(Qt::TextSelectableByMouse);
        inspector_layout->addWidget(m_file);
        m_npc_quests = new QWidget(inspector);
        auto* quest_layout = new QVBoxLayout(m_npc_quests);
        quest_layout->setContentsMargins(0, 4, 0, 4);
        auto* quest_header = new QHBoxLayout;
        quest_header->addWidget(new QLabel("NPC quests", m_npc_quests));
        quest_header->addStretch();
        m_open_quest = new QPushButton("Open", m_npc_quests);
        m_set_quest = new QPushButton("Set entry step…", m_npc_quests);
        quest_header->addWidget(m_open_quest);
        quest_header->addWidget(m_set_quest);
        quest_layout->addLayout(quest_header);
        m_quest_links = new QTreeWidget(m_npc_quests);
        m_quest_links->setObjectName("NpcQuests");
        m_quest_links->setHeaderLabels({"Step", "Connection"});
        m_quest_links->setRootIsDecorated(false);
        m_quest_links->setAlternatingRowColors(true);
        m_quest_links->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_quest_links->setMinimumHeight(84);
        m_quest_links->setMaximumHeight(145);
        quest_layout->addWidget(m_quest_links);
        inspector_layout->addWidget(m_npc_quests);
        m_editor = new JsonEditor(inspector);
        m_editor->set_catalog(&m_catalog);
        inspector_layout->addWidget(m_editor, 1);

        m_map_panel = new QWidget(split);
        auto* map_panel = m_map_panel;
        auto* map_layout = new QVBoxLayout(map_panel);
        auto* options = new QHBoxLayout;
        m_maps = new QComboBox(map_panel);
        m_maps->setAccessibleName("Map");
        options->addWidget(m_maps);
        m_regions = new QComboBox(map_panel);
        m_regions->setAccessibleName("Dungeon region");
        options->addWidget(m_regions);
        auto* terrain = new QCheckBox("Detailed map", map_panel);
        terrain->setChecked(true);
        options->addWidget(terrain);
        auto* fit = new QAction("Fit map", map_panel);
        auto* map_actions = new QToolBar(map_panel);
        map_actions->addAction(fit);
        options->addWidget(map_actions);
        options->addStretch();
        map_layout->addLayout(options);
        auto* views = new QSplitter(Qt::Vertical, map_panel);
        views->setChildrenCollapsible(false);
        m_map = new MapView(views);
        m_preview = new PlacementPreview(views);
        views->addWidget(m_map);
        views->addWidget(m_preview);
        views->setStretchFactor(0, 3);
        views->setStretchFactor(1, 2);
        map_layout->addWidget(views, 1);
        m_map_hint = new QLabel(map_panel);
        m_map_hint->setWordWrap(true);
        m_map_hint->setTextFormat(Qt::PlainText);
        map_layout->addWidget(m_map_hint);
        m_spawns = new QComboBox(map_panel);
        m_spawns->setAccessibleName("Monster spawn");
        map_layout->addWidget(m_spawns);
        m_coordinates = new QLabel(this);
        statusBar()->addPermanentWidget(m_coordinates);

        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 2);
        split->setStretchFactor(2, 3);
        split->setSizes({240, 520, 780});

        m_edits = new QTimer(this);
        m_edits->setSingleShot(true);
        m_edits->setInterval(120);
        connect(m_edits, &QTimer::timeout, this, &ContentWindow::refresh_selection);
        connect(m_editor, &JsonEditor::changed, this, [this] {
            setWindowModified(true);
            m_save->setEnabled(m_record && m_record->writable);
            m_edits->start();
        });
        connect(m_editor, &JsonEditor::open_record_requested, this, [this](const QString& path) {
            open_record(path);
        });
        connect(m_editor, &JsonEditor::new_step_requested, this, [this] {
            create_record(false, true);
        });
        connect(
            m_editor, &JsonEditor::teleport_requested, this, &ContentWindow::edit_quest_teleport);
        connect(m_open_quest, &QPushButton::clicked, this, &ContentWindow::open_npc_quest);
        connect(m_set_quest, &QPushButton::clicked, this, &ContentWindow::set_npc_quest);
        connect(m_quest_links, &QTreeWidget::itemActivated, this, &ContentWindow::open_npc_quest);
        connect(
            m_quest_links, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
                m_open_quest->setEnabled(item && !item->data(0, Qt::UserRole).toString().isEmpty());
            });
        connect(m_category, &QComboBox::currentIndexChanged, this, [this] {
            if (!confirm_leave() && m_record) {
                set_category(m_record->kind);
                return;
            }
            show_record(std::nullopt);
            m_search->clear();
            refresh_list();
        });
        connect(m_search, &QLineEdit::textChanged, this, &ContentWindow::filter_list);
        connect(m_only_orphans, &QCheckBox::toggled, this, &ContentWindow::filter_list);
        connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
            if (item) {
                const auto path = item->data(Qt::UserRole).toString();
                QTimer::singleShot(0, this, [this, path] {
                    const auto* current = m_list->currentItem();
                    if (current && !current->isHidden() && current->data(Qt::UserRole) == path) {
                        open_record(path);
                    }
                });
            }
        });
        connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            if (m_record && m_record->path == item->data(Qt::UserRole).toString()) {
                locate(true);
            }
        });
        connect(m_maps, &QComboBox::currentIndexChanged, this, [this] { populate_regions(); });
        connect(m_regions, &QComboBox::currentIndexChanged, this, [this](int region) {
            m_map->show_map(m_maps->currentData().toInt(), region);
            refresh_markers();
        });
        connect(m_spawns, &QComboBox::activated, this, [this](int index) {
            m_spawn_index = m_spawns->itemData(index).toInt();
            refresh_selection();
            if (m_spawn_index >= 0) {
                locate(true);
            }
        });
        connect(fit, &QAction::triggered, m_map, &MapView::fit);
        connect(terrain, &QCheckBox::toggled, m_map, &MapView::set_terrain_detail);
        connect(m_map, &MapView::coordinates, m_coordinates, &QLabel::setText);
        connect(m_map, &MapView::marker_selected, this, &ContentWindow::open_record);
        connect(m_map, &MapView::point_placed, this, &ContentWindow::place_point);
        connect(m_map, &MapView::spawn_drawn, this, &ContentWindow::draw_spawn);
    }

    void ContentWindow::build_menus() {
        auto* file = menuBar()->addMenu("&File");
        file->addAction(
            "Open configuration…", QKeySequence::Open, this, &ContentWindow::open_directory);
        file->addAction("Open profile…", this, &ContentWindow::open_profile);
        file->addAction("Open client assets…", this, &ContentWindow::open_assets);
        file->addAction("Reload configuration", QKeySequence::Refresh, this, [this] {
            if (confirm_leave()) {
                load_workspace(m_last_path);
            }
        });
        file->addAction("Configuration diagnostics…", this, [this] {
            details(this,
                    "Configuration diagnostics",
                    m_diagnostics.isEmpty()
                        ? QString("The loaded catalogs have no parse or duplicate diagnostics.")
                        : QString("%1 issues while loading definitions.").arg(m_diagnostics.size()),
                    m_diagnostics);
        });
        m_save =
            file->addAction("Save definition", QKeySequence::Save, this, [this] { (void) save(); });
        file->addAction("Quit", QKeySequence::Quit, this, &QWidget::close);

        auto* edit = menuBar()->addMenu("&Definition");
        m_new = edit->addAction("New…", QKeySequence::New, this, [this] { create_record(); });
        m_duplicate = edit->addAction("Duplicate…", this, [this] { create_record(true); });
        m_delete = edit->addAction("Delete…", this, &ContentWindow::delete_record);
        m_references = edit->addAction("Show references…", this, &ContentWindow::show_references);
        m_follow = edit->addAction("Open reference…", this, [this] { select_reference(false); });
        m_link =
            edit->addAction("Choose reference target…", this, [this] { select_reference(true); });
        m_shop = edit->addAction("Open or create NPC shop…", this, &ContentWindow::open_shop);
        edit->addSeparator();
        edit->addAction("Audit content references…", this, &ContentWindow::show_content_audit);

        auto* map = menuBar()->addMenu("&Map");
        m_locate = map->addAction("Locate on map", this, [this] { locate(); });
        m_place = map->addAction(
            "Place / move NPC", this, [this] { begin_placement(MapView::Mode::PlaceNpc); });
        m_add_spawn = map->addAction(
            "New spawn", this, [this] { begin_placement(MapView::Mode::DrawSpawn); });
        m_redraw = map->addAction("Redraw spawn", this, [this] {
            begin_placement(MapView::Mode::DrawSpawn, m_spawn_index);
        });
        m_remove_spawn = map->addAction("Remove spawn", this, &ContentWindow::remove_spawn);
        m_teleport = map->addAction("Set teleport target", this, &ContentWindow::edit_teleport);
        m_cancel_placement =
            map->addAction("Cancel placement", QKeySequence(Qt::Key_Escape), this, [this] {
                begin_placement(MapView::Mode::Select);
            });

        auto* definitions = addToolBar("Definitions");
        definitions->setMovable(false);
        definitions->setToolButtonStyle(Qt::ToolButtonTextOnly);
        definitions->addActions({m_new, m_save, m_duplicate, m_delete});
        definitions->addSeparator();
        definitions->addActions({m_references, m_follow, m_link, m_shop});
        addToolBarBreak();
        auto* placement = addToolBar("Placement");
        placement->setMovable(false);
        placement->setToolButtonStyle(Qt::ToolButtonTextOnly);
        placement->addActions({m_locate,
                               m_place,
                               m_add_spawn,
                               m_redraw,
                               m_remove_spawn,
                               m_teleport,
                               m_cancel_placement});
    }

    void ContentWindow::load_workspace(QString selected) {
        show_record(std::nullopt);
        std::string archive_error;
        if (m_workspace.archived_config.isEmpty()) {
            platform::ConfigArchive::unmount();
        } else {
            (void) platform::ConfigArchive::select(OPENKAL_RESOURCE_ROOT, {}, &archive_error);
        }
        m_diagnostics =
            m_catalog.load(m_workspace.config, m_workspace.base, m_workspace.archived_config);
        m_content_audit = audit_content_references(m_catalog);
        if (!archive_error.empty()) {
            m_diagnostics.prepend(QString::fromStdString(archive_error));
        }
        load_map();
        populate_maps();
        refresh_list();
        if (!selected.isEmpty()) {
            open_record(selected);
        }
        if (!m_diagnostics.isEmpty()) {
            description(
                QString(
                    "%1 definitions loaded with %2 issues. See File → Configuration diagnostics.")
                    .arg(m_catalog.records().size())
                    .arg(m_diagnostics.size()));
        } else if (m_catalog.root().isEmpty()) {
            description(
                "Read-only configuration. Use File → Open configuration to choose loose JSON files.");
        } else {
            description(QString("%1 definitions loaded from %2")
                            .arg(m_catalog.records().size())
                            .arg(m_catalog.root()));
        }
    }

    void ContentWindow::load_map() {
        m_map->load(m_workspace.data,
                    m_workspace.map,
                    m_workspace.archived_config.isEmpty() ? m_workspace.config
                                                          : m_workspace.archived_config,
                    m_workspace.base,
                    m_workspace.maps_subdirectory);
        QStringList resources{OPENKAL_RESOURCE_ROOT};
        for (const auto& config : {m_workspace.config, m_workspace.base}) {
            if (!config.isEmpty()) {
                resources.append(QFileInfo(config).absolutePath());
            }
        }
        m_preview->set_assets(
            m_workspace.data, m_workspace.map, m_workspace.maps_subdirectory, resources);
    }

    void ContentWindow::open_directory() {
        if (!confirm_leave()) {
            return;
        }
        const auto path = QFileDialog::getExistingDirectory(
            this, "Open loose configuration directory", m_workspace.config);
        if (!path.isEmpty()) {
            m_workspace.config = existing_config(path);
            m_workspace.archived_config.clear();
            load_workspace();
        }
    }

    void ContentWindow::open_assets() {
        const auto path =
            QFileDialog::getExistingDirectory(this,
                                              "Open directory containing data and map",
                                              QFileInfo(m_workspace.data).absolutePath());
        if (path.isEmpty()) {
            return;
        }
        const QDir directory(path);
        if (!QFileInfo(directory.filePath("data")).isDir()) {
            error("Choose the client directory containing data and map.");
            return;
        }
        m_workspace.data = directory.filePath("data");
        m_workspace.map = directory.filePath("map");
        m_workspace.maps_subdirectory = "MAPS";
        load_map();
        populate_maps();
        refresh_selection();
    }

    void ContentWindow::open_profile() {
        const auto path = QFileDialog::getOpenFileName(
            this, "Open OpenKal profile", {}, "OpenKal profiles (*.profile)");
        if (path.isEmpty() || !confirm_leave()) {
            return;
        }
        if (const auto problem = read_profile(m_workspace, path); !problem.isEmpty()) {
            error(problem);
            return;
        }
        load_workspace();
    }

    Kind ContentWindow::kind() const {
        return static_cast<Kind>(m_category->currentData().toInt());
    }

    void ContentWindow::set_category(Kind kind) {
        const QSignalBlocker blocker(m_category);
        m_category->setCurrentIndex(m_category->findData(static_cast<int>(kind)));
    }

    void ContentWindow::refresh_list() {
        const QSignalBlocker blocker(m_list);
        const int scroll = m_list->verticalScrollBar()->value();
        const auto category = kind();
        m_only_orphans->setVisible(supports_orphan_audit(category));
        const auto quest_names =
            category == Kind::Quest ? m_catalog.quest_names() : QHash<int, QString>{};
        m_list->clear();
        QVector<const Record*> records;
        for (const auto& record : m_catalog.records()) {
            if (record.kind == category) {
                records.append(&record);
            }
        }
        std::sort(records.begin(), records.end(), [](const Record* a, const Record* b) {
            const auto left = identity(a->kind, a->object).split(':');
            const auto right = identity(b->kind, b->object).split(':');
            const auto left_id = left.front().toLongLong();
            const auto right_id = right.front().toLongLong();
            if (left_id != right_id) {
                return left_id < right_id;
            }
            if (left.front() != right.front()) {
                return left.front() < right.front();
            }
            if (left.value(1).toInt() != right.value(1).toInt()) {
                return left.value(1).toInt() < right.value(1).toInt();
            }
            return a->path < b->path;
        });
        for (const auto* record : records) {
            const auto name = quest_names.value(record->object.value("index").toInt());
            auto* item = new QListWidgetItem(title(*record, name), m_list);
            item->setData(Qt::UserRole, record->path);
            const auto reason = m_content_audit.orphan_reasons.value(record->path);
            if (!reason.isEmpty()) {
                item->setBackground(QColor(220, 66, 84, 42));
                item->setToolTip("Possible orphan\n" + reason + '\n' + record->path);
                item->setData(Qt::AccessibleDescriptionRole, "Possible orphan. " + reason);
            } else {
                item->setToolTip(record->path);
            }
        }
        filter_list();
        sync_list_selection(false);
        m_list->doItemsLayout();
        m_list->verticalScrollBar()->setValue(scroll);
    }

    void ContentWindow::filter_list() {
        const auto filter = m_search->text().simplified().split(' ', Qt::SkipEmptyParts);
        const bool only_orphans = m_only_orphans->isChecked() && supports_orphan_audit(kind());
        for (int i = 0; i < m_list->count(); ++i) {
            auto* item = m_list->item(i);
            item->setHidden(
                (only_orphans &&
                 !m_content_audit.orphan_reasons.contains(item->data(Qt::UserRole).toString())) ||
                !std::all_of(filter.cbegin(), filter.cend(), [item](const QString& word) {
                    return item->text().contains(word, Qt::CaseInsensitive);
                }));
        }
    }

    void ContentWindow::sync_list_selection(bool reveal) {
        const QSignalBlocker blocker(m_list);
        if (m_record) {
            if (const auto* item = m_list->currentItem();
                item && item->data(Qt::UserRole) == m_record->path &&
                (!reveal || !item->isHidden())) {
                return;
            }
            for (int i = 0; i < m_list->count(); ++i) {
                auto* item = m_list->item(i);
                if (item->data(Qt::UserRole) != m_record->path) {
                    continue;
                }
                if (reveal && item->isHidden()) {
                    if (!m_content_audit.orphan_reasons.contains(m_record->path)) {
                        m_only_orphans->setChecked(false);
                    }
                    if (item->isHidden()) {
                        m_search->clear();
                    }
                }
                const int scroll = m_list->verticalScrollBar()->value();
                m_list->setCurrentItem(item);
                if (reveal) {
                    m_list->scrollToItem(item);
                } else {
                    m_list->verticalScrollBar()->setValue(scroll);
                }
                return;
            }
        }
        m_list->setCurrentItem(nullptr);
        m_list->clearSelection();
    }

    void ContentWindow::open_record(const QString& path, int spawn) {
        if (m_record && m_record->path == path) {
            if (spawn >= 0) {
                m_spawn_index = spawn;
            }
            sync_list_selection(true);
            locate(true);
            return;
        }
        const auto exists = std::find_if(m_catalog.records().begin(),
                                         m_catalog.records().end(),
                                         [&](const Record& record) { return record.path == path; });
        if (exists == m_catalog.records().end()) {
            return;
        }
        const Record next = *exists;
        if (!confirm_leave()) {
            sync_list_selection(false);
            return;
        }
        const bool category_changed = kind() != next.kind;
        if (category_changed) {
            set_category(next.kind);
            m_search->clear();
        }
        show_record(next, spawn);
        if (category_changed) {
            refresh_list();
        }
        sync_list_selection(true);
        if (category_changed) {
            m_list->scrollToItem(m_list->currentItem());
        }
        locate(true);
    }

    void ContentWindow::show_record(std::optional<Record> record, int spawn) {
        m_record = std::move(record);
        m_last_path = m_record ? m_record->path : QString{};
        m_spawn_index = spawn;
        m_edits->stop();
        m_editor->load(m_record ? m_record->kind : kind(),
                       m_record ? m_record->object : QJsonObject{},
                       m_record && m_record->writable);
        begin_placement(MapView::Mode::Select);
        refresh_selection();
    }

    bool ContentWindow::dirty() const {
        if (!m_record || !m_record->writable) {
            return false;
        }
        const auto current = m_editor->object();
        return m_record->original.isEmpty() || !current || *current != m_record->object;
    }

    bool ContentWindow::confirm_leave() {
        if (!dirty()) {
            return true;
        }
        const auto answer =
            QMessageBox::warning(this,
                                 "Unsaved changes",
                                 "Save the current definition?",
                                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                 QMessageBox::Save);
        return answer == QMessageBox::Discard || (answer == QMessageBox::Save && save());
    }

    void ContentWindow::create_record(bool duplicate, bool same_quest) {
        if (m_catalog.root().isEmpty() || (duplicate && !m_record)) {
            return;
        }
        const auto quest = same_quest ? current_object() : std::optional<QJsonObject>{};
        if (same_quest && !quest) {
            return;
        }
        const auto id =
            quest ? QString::number(quest->value("index").toInt()) : new_id(this, kind());
        const int current_flag = quest ? quest->value("flag").toInt(-1) : -1;
        if (id.isEmpty()) {
            return;
        }
        int flag = 0;
        if (kind() == Kind::Quest) {
            int suggested = 1;
            if (same_quest) {
                const int first = std::clamp(current_flag, -1, 255) + 1;
                suggested = -1;
                for (int offset = 0; offset < 256; ++offset) {
                    const int candidate = (first + offset) % 256;
                    if (candidate != current_flag &&
                        !m_catalog.find(Kind::Quest, id + ':' + QString::number(candidate))) {
                        suggested = candidate;
                        break;
                    }
                }
                if (suggested < 0) {
                    description("All steps for this quest ID are in use.");
                    return;
                }
            }
            bool ok = false;
            flag = QInputDialog::getInt(this, "Quest step", "Flag", suggested, 0, 255, 1, &ok);
            if (!ok) {
                return;
            }
        }
        auto object = initial_object(kind(), id, flag);
        if (duplicate) {
            const auto current = current_object();
            if (!current) {
                return;
            }
            auto copy = *current;
            copy.insert(kind() == Kind::Task     ? "key"
                        : kind() == Kind::Dialog ? "id"
                        : kind() == Kind::Shop   ? "npcIndex"
                                                 : "index",
                        id.toInt());
            if (kind() == Kind::Quest) {
                copy["flag"] = flag;
            }
            object = copy;
        }
        if (m_catalog.find(kind(), identity(kind(), object)) ||
            (same_quest && flag == current_flag)) {
            error("This identifier is already in use.");
            return;
        }
        if (confirm_leave()) {
            show_record(m_catalog.draft(kind(), object, relative_item_directory(object)));
            refresh_list();
        }
    }

    std::optional<QJsonObject> ContentWindow::current_object() {
        QString problem;
        auto object = m_editor->object(&problem);
        if (!object) {
            error(problem);
        }
        return object;
    }

    bool ContentWindow::save() {
        if (!m_record || !m_record->writable) {
            return false;
        }
        const auto object = current_object();
        if (!object) {
            return false;
        }
        auto candidate = *m_record;
        candidate.object = *object;
        if (candidate.original.isEmpty()) {
            candidate = m_catalog.draft(candidate.kind, *object, relative_item_directory(*object));
        }
        if (const auto problem = m_catalog.validate(candidate); !problem.isEmpty()) {
            error(problem);
            return false;
        }
        if (const auto unresolved = m_catalog.unresolved(candidate); !unresolved.isEmpty()) {
            QMessageBox dialog(
                QMessageBox::Warning,
                "Unresolved references",
                "Some new references are missing from the loaded catalogs. Save anyway?",
                QMessageBox::Save | QMessageBox::Cancel,
                this);
            dialog.setInformativeText(unresolved.mid(0, 4).join('\n'));
            dialog.setDetailedText(unresolved.join('\n'));
            dialog.setDefaultButton(QMessageBox::Cancel);
            if (dialog.exec() != QMessageBox::Save) {
                return false;
            }
        }
        if (const auto problem = m_catalog.save(candidate); !problem.isEmpty()) {
            error(problem);
            return false;
        }
        candidate.original = serialize(*object);
        m_record = candidate;
        m_last_path = candidate.path;
        m_content_audit = audit_content_references(m_catalog);
        refresh_list();
        refresh_selection();
        description("Saved " + title(candidate));
        return true;
    }

    void ContentWindow::delete_record() {
        if (!m_record || !m_record->writable || m_record->original.isEmpty()) {
            return;
        }
        const auto usages = m_catalog.usages(*m_record);
        if (!usages.isEmpty()) {
            details(this,
                    "Definition is in use",
                    "Update these references before deleting the definition.",
                    usages);
            return;
        }
        if (QMessageBox::question(this,
                                  "Delete definition",
                                  "Delete this file?\n" + m_record->path,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }
        if (const auto problem = m_catalog.remove(*m_record); !problem.isEmpty()) {
            error(problem);
            return;
        }
        m_content_audit = audit_content_references(m_catalog);
        show_record(std::nullopt);
        refresh_list();
        description("Definition deleted.");
    }

    void ContentWindow::populate_maps() {
        const QSignalBlocker blocker(m_maps);
        m_maps->clear();
        const auto known = m_map->maps();
        auto indices = known.keys();
        std::stable_sort(indices.begin() + 1, indices.end(), [&](int left, int right) {
            return known.value(left) < known.value(right);
        });
        for (const int index : indices) {
            m_maps->addItem(known.value(index), index);
        }
        populate_regions();
    }

    void ContentWindow::populate_regions(int previous) {
        const QSignalBlocker blocker(m_regions);
        const int index = m_maps->currentData().toInt();
        m_regions->clear();
        m_regions->addItems(m_map->regions(index));
        m_regions->setVisible(m_regions->count() > 1);
        m_regions->setCurrentIndex(previous);
        m_map->show_map(index, previous);
        refresh_markers();
    }

    void ContentWindow::refresh_selection() {
        const bool selected = m_record.has_value();
        const bool writable = selected && m_record->writable;
        const bool npc = selected && m_record->kind == Kind::Npc;
        const bool monster = selected && m_record->kind == Kind::Monster;
        const auto current = m_editor->object();
        const auto object = current.value_or(selected ? m_record->object : QJsonObject{});
        auto description_record = m_record;
        if (description_record) {
            description_record->object = object;
        }
        m_title->setText(description_record ? title(*description_record) : "Select a definition");
        m_file->setText(selected ? (writable ? m_record->path : "Read-only  ·  " + m_record->path)
                                 : m_catalog.root());
        const auto changed = dirty();
        m_save->setEnabled(writable && changed);
        m_new->setEnabled(!m_catalog.root().isEmpty());
        m_duplicate->setEnabled(selected && !m_catalog.root().isEmpty());
        m_delete->setEnabled(writable && !m_record->original.isEmpty());
        m_locate->setVisible(npc || monster);
        m_place->setVisible(npc);
        m_place->setEnabled(writable);
        m_add_spawn->setVisible(monster);
        m_add_spawn->setEnabled(writable);
        m_redraw->setVisible(monster);
        m_remove_spawn->setVisible(monster);
        m_spawns->setVisible(monster);
        m_references->setEnabled(selected);
        m_follow->setEnabled(selected);
        m_link->setEnabled(writable);
        m_shop->setVisible(npc);
        m_shop->setEnabled(!m_catalog.root().isEmpty());
        m_npc_quests->setVisible(npc);
        m_set_quest->setEnabled(npc && writable);
        if (npc) {
            refresh_npc_quests(object);
        } else {
            m_npc_quest_fields = {};
        }
        m_teleport->setVisible(npc || (selected && m_record->kind == Kind::Quest));
        m_teleport->setEnabled(writable);
        const auto spawns = object.value("spawns").toArray();
        if (m_spawn_index >= spawns.size()) {
            m_spawn_index = -1;
        }
        m_redraw->setEnabled(writable && m_spawn_index >= 0);
        m_remove_spawn->setEnabled(writable && m_spawn_index >= 0);
        const QSignalBlocker block(m_spawns);
        m_spawns->clear();
        m_spawns->addItem("Select a spawn", -1);
        for (int i = 0; i < spawns.size(); ++i) {
            const auto spawn = spawns.at(i).toObject();
            const int configured_map = spawn.value("map").toInt();
            auto location =
                configured_map == 0 ? QString("Overworld") : QString("Map %1").arg(configured_map);
            if (const auto bounds = spawn_bounds(spawn);
                bounds && configured_map != 0 &&
                m_map->spawn_map(configured_map, (bounds->first + bounds->second) / 2) == 0) {
                location = QString("Overworld (configured map %1)").arg(configured_map);
            }
            m_spawns->addItem(QString("#%1  ·  %2  ·  Count %3  ·  Respawn %4 s")
                                  .arg(i + 1)
                                  .arg(location)
                                  .arg(spawn.value("max").toInt())
                                  .arg(spawn.value("cycle").toInt()),
                              i);
        }
        m_spawns->setCurrentIndex(m_spawn_index + 1);
        setWindowTitle("OpenKal Content Editor[*]");
        setWindowModified(changed);
        refresh_markers();
        if (description_record && (npc || monster)) {
            m_preview->show_entity(*description_record, m_spawn_index, m_catalog, *m_map);
        } else {
            m_preview->clear();
        }
    }

    void ContentWindow::refresh_npc_quests(const QJsonObject& object) {
        const QJsonObject fields{
            {"index", object.value("index")},
            {"quest", object.value("quest")},
            {"html", object.value("html")},
            {"beside_dialog", value_at(object, {"placement", "beside_dialog"})}};
        if (m_npc_quest_fields == fields) {
            return;
        }
        m_npc_quest_fields = fields;
        const auto* current = m_quest_links->currentItem();
        const QString selected = current ? current->text(0) : QString{};
        m_quest_links->clear();
        m_open_quest->setEnabled(false);
        for (const auto& quest : m_catalog.npc_quests(object)) {
            auto sources = quest.sources.join(" · ");
            if (quest.path.isEmpty()) {
                sources += " · Step not loaded";
            }
            auto* row = new QTreeWidgetItem(m_quest_links, {quest.step, sources});
            row->setData(0, Qt::UserRole, quest.path);
            row->setToolTip(1, sources);
            if (quest.step == selected) {
                m_quest_links->setCurrentItem(row);
            }
        }
        if (m_quest_links->topLevelItemCount() == 0) {
            auto* row = new QTreeWidgetItem(m_quest_links, {"No quests found"});
            row->setFlags(Qt::NoItemFlags);
        } else if (!m_quest_links->currentItem()) {
            m_quest_links->setCurrentItem(m_quest_links->topLevelItem(0));
        }
    }

    void ContentWindow::open_npc_quest() {
        if (const auto* row = m_quest_links->currentItem()) {
            const auto path = row->data(0, Qt::UserRole).toString();
            if (!path.isEmpty()) {
                open_record(path);
            }
        }
    }

    void ContentWindow::set_npc_quest() {
        const auto draft = current_object();
        if (!draft) {
            return;
        }
        const auto current = draft->value("quest").toArray();
        const auto key =
            current.size() >= 2
                ? QString("%1:%2").arg(current.at(0).toInt()).arg(current.at(1).toInt())
                : QString{};
        const auto selected = pick_definition(this, m_catalog, Kind::Quest, key, "No entry step");
        if (!selected || *selected == key) {
            return;
        }
        auto updated = *draft;
        if (selected->isEmpty()) {
            updated.remove("quest");
        } else {
            const auto* step = m_catalog.find(Kind::Quest, *selected);
            if (*selected == "0:0" || (step && step->object.value("linked").toBool())) {
                error("Choose a step that can run directly from an NPC.");
                return;
            }
            const auto parts = selected->split(':');
            updated["quest"] = QJsonArray{parts.front().toInt(), parts.value(1).toInt()};
        }
        m_editor->apply(updated);
        refresh_npc_quests(updated);
    }

    void ContentWindow::refresh_markers() {
        QVector<Marker> markers;
        const int map_id = m_map->map_id();
        const int region = std::max(0, m_regions->currentIndex());
        const bool district = m_regions->count() > 1;
        const auto append_npc = [&](const Record& record, bool active) {
            if (record.kind != Kind::Npc) {
                return;
            }
            const auto effective = m_catalog.resolve_npc(record.object);
            if (effective && effective->value("map").toInt() == map_id) {
                if (const auto point = position(effective->value("xy"));
                    point && (!district || m_map->region_at(map_id, *point) == region)) {
                    markers.append({record.path, title(record), *point, *point, -1, active});
                }
            }
        };
        for (const auto& record : m_catalog.records()) {
            if (!m_record || record.path != m_record->path) {
                append_npc(record, false);
            }
        }
        if (m_record) {
            auto active = *m_record;
            active.object = m_editor->object().value_or(active.object);
            if (active.kind == Kind::Monster) {
                const auto spawns = active.object.value("spawns").toArray();
                const auto name = title(active);
                for (int i = 0; i < spawns.size(); ++i) {
                    const auto spawn = spawns.at(i).toObject();
                    const auto bounds = spawn_bounds(spawn);
                    if (!bounds) {
                        continue;
                    }
                    const auto center = (bounds->first + bounds->second) / 2;
                    if (m_map->spawn_map(spawn.value("map").toInt(), center) == map_id &&
                        (!district || m_map->region_at(map_id, center) == region)) {
                        markers.append({active.path,
                                        name,
                                        bounds->first,
                                        bounds->second,
                                        i,
                                        i == m_spawn_index});
                    }
                }
            } else {
                append_npc(active, true);
            }
        }
        m_map->set_markers(std::move(markers));
    }

    bool ContentWindow::set_map(int index, QPoint center) {
        const auto value = m_maps->findData(index);
        if (value < 0) {
            description(QString("Map %1 has no loaded map metadata.").arg(index));
            return false;
        }
        const QSignalBlocker blocker(m_maps);
        m_maps->setCurrentIndex(value);
        populate_regions(m_map->region_at(index, center));
        return true;
    }

    void ContentWindow::locate(bool close) {
        const auto category = kind();
        if (category != Kind::Npc && category != Kind::Monster) {
            return;
        }
        const auto object = current_object();
        if (!object) {
            return;
        }
        constexpr qreal close_margin = 224.0;
        if (category == Kind::Npc) {
            if (const auto effective = m_catalog.resolve_npc(*object)) {
                if (const auto point = position(effective->value("xy"));
                    point && set_map(effective->value("map").toInt(), *point)) {
                    if (close) {
                        m_map->focus(*point, {}, close_margin);
                    } else {
                        m_map->focus(*point);
                    }
                }
            }
        } else {
            const auto spawns = object->value("spawns").toArray();
            const int index = std::max(0, m_spawn_index);
            const auto spawn = spawns.at(index).toObject();
            if (const auto bounds = spawn_bounds(spawn)) {
                const QPoint center = (bounds->first + bounds->second) / 2;
                if (set_map(m_map->spawn_map(spawn.value("map").toInt(), center), center)) {
                    if (close) {
                        m_map->focus(center, {}, close_margin);
                    } else {
                        m_map->focus(bounds->first, bounds->second);
                    }
                }
                m_spawn_index = index;
                m_editor->select_path({"spawns", QString::number(m_spawn_index)});
                refresh_selection();
            }
        }
    }

    void ContentWindow::begin_placement(MapView::Mode mode, int row) {
        m_map_mode = mode;
        m_placement_row = row;
        m_cancel_placement->setEnabled(mode != MapView::Mode::Select);
        m_map->set_mode(mode);
        m_map_panel->setVisible(!m_record || m_record->kind != Kind::Quest ||
                                mode == MapView::Mode::PlaceTeleport);
        switch (mode) {
        case MapView::Mode::Select:
            m_map_hint->setText(
                "Scroll at the pointer to zoom · Right-drag to pan · Gold: selected monster spawns");
            break;
        case MapView::Mode::PlaceNpc:
            m_map_hint->setText("Click the map to place the NPC and sample its elevation.");
            break;
        case MapView::Mode::DrawSpawn:
            m_map_hint->setText("Click for a single spawn point or drag to draw a spawn area.");
            break;
        case MapView::Mode::PlaceTeleport:
            m_map_hint->setText("Click the destination on the selected map.");
            break;
        }
    }

    void ContentWindow::place_point(QPoint point) {
        if (!m_record || !m_record->writable) {
            return;
        }
        auto object = current_object();
        if (!object) {
            return;
        }
        const int map = m_map->map_id();
        if (m_map_mode == MapView::Mode::PlaceTeleport && kind() == Kind::Npc) {
            auto targets = object->value("teleports").toArray();
            const QJsonArray target{map, point.x(), point.y()};
            if (m_placement_row >= 0 && m_placement_row < targets.size()) {
                targets.replace(m_placement_row, target);
            } else {
                targets.append(target);
            }
            object->insert("teleports", targets);
            m_editor->apply(*object);
            m_editor->select_path(
                {"teleports",
                 QString::number(m_placement_row < 0 ? targets.size() - 1 : m_placement_row)});
        } else if (m_map_mode == MapView::Mode::PlaceTeleport && kind() == Kind::Quest) {
            auto action = value_at(*object, m_teleport_path).toObject();
            if (action.value("op").toString() != "teleport") {
                return;
            }
            action["map"] = map;
            action["x"] = point.x();
            action["y"] = point.y();
            m_editor->apply(replace_at(*object, m_teleport_path, action).toObject());
            m_editor->select_path(m_teleport_path);
        } else if (kind() == Kind::Npc && m_map_mode == MapView::Mode::PlaceNpc) {
            const auto heights = m_map->elevations(point);
            if (heights.isEmpty()) {
                error(
                    "No ground height is available here. Load the matching terrain or dungeon geometry and choose a covered position.");
                return;
            }
            int height = heights.front();
            if (heights.size() > 1) {
                QStringList choices;
                for (const int z : heights) {
                    choices.append(QString::number(z));
                }
                bool ok = false;
                const auto selected = QInputDialog::getItem(
                    this, "Choose dungeon floor", "Server Z", choices, 0, false, &ok);
                if (!ok) {
                    return;
                }
                height = selected.toInt();
            }
            if (object->contains("placement") &&
                QMessageBox::question(
                    this,
                    "Replace relative placement",
                    "This NPC uses relative placement. Replace it with the coordinates from the map?",
                    QMessageBox::Yes | QMessageBox::Cancel,
                    QMessageBox::Cancel) != QMessageBox::Yes) {
                return;
            }
            const auto old_definition = m_catalog.resolve_npc(*object).value_or(*object);
            if (object->value("placement").isObject()) {
                for (const auto* key : {"shape", "appearance", "dir"}) {
                    object->insert(key, old_definition.value(key));
                }
            }
            if (const auto old = position(old_definition.value("xy"))) {
                if (const auto look = position(old_definition.value("dir"))) {
                    const auto moved = *look - *old + point;
                    object->insert("dir", QJsonArray{moved.x(), moved.y()});
                }
            }
            object->remove("placement");
            object->insert("xy", QJsonArray{point.x(), point.y(), height});
            object->insert("map", map);
            m_editor->apply(*object);
            m_editor->select_path({"xy"});
            description(QString("NPC elevation set to Z %1.").arg(height));
        }
        begin_placement(MapView::Mode::Select);
        refresh_selection();
    }

    void ContentWindow::draw_spawn(QPoint first, QPoint last) {
        if (!m_record || !m_record->writable || kind() != Kind::Monster) {
            return;
        }
        auto object = current_object();
        if (!object) {
            return;
        }
        auto spawns = object->value("spawns").toArray();
        auto spawn = spawns.at(m_placement_row).toObject();
        if (spawn.isEmpty()) {
            spawn = QJsonObject{{"area", 0}, {"max", 1}, {"cycle", 30}};
        }
        spawn["map"] = m_map->map_id();
        spawn["rect"] = QJsonArray{std::min(first.x(), last.x()) / map::kTileSize,
                                   std::min(first.y(), last.y()) / map::kTileSize,
                                   std::max(first.x(), last.x()) / map::kTileSize,
                                   std::max(first.y(), last.y()) / map::kTileSize};
        if (m_placement_row >= 0 && m_placement_row < spawns.size()) {
            spawns.replace(m_placement_row, spawn);
            m_spawn_index = m_placement_row;
        } else {
            m_spawn_index = static_cast<int>(spawns.size());
            spawns.append(spawn);
        }
        object->insert("spawns", spawns);
        m_editor->apply(*object);
        m_editor->select_path({"spawns", QString::number(m_spawn_index)});
        begin_placement(MapView::Mode::Select);
        refresh_selection();
    }

    void ContentWindow::remove_spawn() {
        auto object = current_object();
        if (!object || m_spawn_index < 0) {
            return;
        }
        auto spawns = object->value("spawns").toArray();
        if (m_spawn_index >= spawns.size()) {
            return;
        }
        spawns.removeAt(m_spawn_index);
        m_spawn_index = -1;
        object->insert("spawns", spawns);
        m_editor->apply(*object);
        begin_placement(MapView::Mode::Select);
        refresh_selection();
    }

    void ContentWindow::edit_teleport() {
        const auto path = m_editor->selected_path();
        if (kind() == Kind::Npc) {
            begin_placement(MapView::Mode::PlaceTeleport,
                            path.value(0) == "teleports" && path.size() >= 2 ? path.at(1).toInt()
                                                                             : -1);
            return;
        }
        edit_quest_teleport(path);
    }

    void ContentWindow::edit_quest_teleport(const JsonPath& path) {
        const auto object = current_object();
        if (!object) {
            return;
        }
        m_teleport_path = path.mid(0, 4);
        if (value_at(*object, m_teleport_path).toObject().value("op").toString() != "teleport") {
            description("Choose a teleport action in the quest graph or in Fields.");
            return;
        }
        begin_placement(MapView::Mode::PlaceTeleport);
    }

    void ContentWindow::open_shop() {
        if (!m_record) {
            return;
        }
        const auto id = identity(Kind::Npc, m_record->object);
        if (const auto* shop = m_catalog.find(Kind::Shop, id)) {
            open_record(shop->path);
            return;
        }
        if (m_record->original.isEmpty()) {
            description("Save the NPC before creating its shop.");
            return;
        }
        if (confirm_leave()) {
            set_category(Kind::Shop);
            m_search->clear();
            show_record(m_catalog.draft(Kind::Shop, initial_object(Kind::Shop, id)));
            refresh_list();
        }
    }

    void ContentWindow::show_content_audit() {
        QDialog dialog(this);
        dialog.setWindowTitle("Content reference audit");
        dialog.resize(860, 560);
        auto* layout = new QVBoxLayout(&dialog);
        auto* scope = new QLabel(
            "Checks saved definitions in the loaded configuration. Events and client scripts can "
            "provide additional entry points.",
            &dialog);
        scope->setWordWrap(true);
        layout->addWidget(scope);

        auto* controls = new QHBoxLayout;
        auto* group = new QComboBox(&dialog);
        group->setAccessibleName("Audit findings");
        const auto add_group = [&](ContentFindingKind kind, const QString& caption) {
            const auto count =
                std::count_if(m_content_audit.findings.cbegin(),
                              m_content_audit.findings.cend(),
                              [kind](const ContentFinding& entry) { return entry.kind == kind; });
            group->addItem(QString("%1 (%2)").arg(caption).arg(count), static_cast<int>(kind));
        };
        add_group(ContentFindingKind::PossibleOrphan, "Possible orphans");
        add_group(ContentFindingKind::NoKnownRoute, "No known route");
        add_group(ContentFindingKind::MissingTarget, "Missing link targets");
        controls->addWidget(group);
        auto* search = new QLineEdit(&dialog);
        search->setPlaceholderText("Filter findings");
        search->setClearButtonEnabled(true);
        controls->addWidget(search, 1);
        layout->addLayout(controls);

        auto* results = new QTreeWidget(&dialog);
        results->setObjectName("ContentAuditResults");
        results->setHeaderLabels({"Definition", "Finding"});
        results->setRootIsDecorated(false);
        results->setAlternatingRowColors(true);
        results->setUniformRowHeights(true);
        results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        layout->addWidget(results, 1);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        auto* open = buttons->addButton("Open definition", QDialogButtonBox::AcceptRole);
        layout->addWidget(buttons);
        const auto update_open = [&] {
            const auto* current = results->currentItem();
            open->setEnabled(current && !current->isHidden());
        };
        const auto filter = [&] {
            const auto words = search->text().simplified().split(' ', Qt::SkipEmptyParts);
            for (int i = 0; i < results->topLevelItemCount(); ++i) {
                auto* item = results->topLevelItem(i);
                const auto text = item->text(0) + ' ' + item->text(1);
                item->setHidden(
                    !std::all_of(words.cbegin(), words.cend(), [&](const QString& word) {
                        return text.contains(word, Qt::CaseInsensitive);
                    }));
            }
            update_open();
        };
        const auto show_group = [&] {
            results->clear();
            const auto kind = static_cast<ContentFindingKind>(group->currentData().toInt());
            for (const auto& finding : m_content_audit.findings) {
                if (finding.kind != kind) {
                    continue;
                }
                auto* item = new QTreeWidgetItem(results, {finding.definition, finding.detail});
                item->setData(0, Qt::UserRole, finding.record_path);
                item->setData(0, Qt::UserRole + 1, finding.field);
                item->setToolTip(0, finding.record_path);
                item->setToolTip(1, finding.detail);
            }
            results->sortItems(0, Qt::AscendingOrder);
            filter();
        };
        connect(group, &QComboBox::currentIndexChanged, &dialog, show_group);
        connect(search, &QLineEdit::textChanged, &dialog, filter);
        connect(results, &QTreeWidget::currentItemChanged, &dialog, update_open);
        connect(results, &QTreeWidget::itemActivated, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        show_group();

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const auto* selected = results->currentItem();
        if (!selected || selected->isHidden()) {
            return;
        }
        const auto path = selected->data(0, Qt::UserRole).toString();
        const auto field = selected->data(0, Qt::UserRole + 1).toStringList();
        open_record(path);
        if (m_record && m_record->path == path && !field.isEmpty()) {
            const auto draft = m_editor->object();
            if (draft && value_at(*draft, field) == value_at(m_record->object, field)) {
                m_editor->select_path(field);
            } else {
                description("Save the definition to refresh this audit finding.");
            }
        }
    }

    void ContentWindow::show_references() {
        if (!m_record) {
            return;
        }
        const auto current = current_object();
        if (!current) {
            return;
        }
        auto active = *m_record;
        active.object = *current;
        auto rows = m_catalog.usages(active);
        rows.prepend("Referenced by:");
        rows.append("\nReferences:");
        for (const auto& ref : references(active)) {
            rows.append(ref.path.join(" / ") + " → " + label(ref.kind) + " " + ref.id);
        }
        details(this, "Definition references", title(active), rows);
    }

    void ContentWindow::select_reference(bool replace) {
        if (!m_record) {
            return;
        }
        auto current = current_object();
        if (!current) {
            return;
        }
        auto active = *m_record;
        active.object = *current;
        auto refs = references(active);
        const auto selected = m_editor->selected_path();
        if (const auto it =
                std::find_if(refs.begin(),
                             refs.end(),
                             [&](const Reference& ref) { return ref.path == selected; });
            it != refs.end()) {
            const auto ref = *it;
            refs = {ref};
        }
        if (refs.isEmpty()) {
            description("Select or add a reference field in the definition.");
            return;
        }
        QStringList fields;
        for (const auto& ref : refs) {
            fields.append(ref.path.join(" / ") + " → " + label(ref.kind) + " " + ref.id);
        }
        int index = 0;
        if (fields.size() > 1) {
            bool ok = false;
            const auto chosen =
                QInputDialog::getItem(this, "Reference field", "Field", fields, 0, false, &ok);
            if (!ok) {
                return;
            }
            index = static_cast<int>(fields.indexOf(chosen));
        }
        const auto& field = refs.at(index);
        if (!replace) {
            if (const auto* target = m_catalog.find(field.kind, field.id)) {
                open_record(target->path);
            } else {
                description("The target is missing from the loaded catalogs.");
            }
            return;
        }

        QStringList choices;
        QHash<QString, QString> keys;
        for (const auto& entry : m_catalog.records()) {
            if (entry.kind == field.kind) {
                choices.append(title(entry));
                keys.insert(title(entry), identity(entry.kind, entry.object));
            }
        }
        if (choices.isEmpty()) {
            description("No definitions are loaded in " + label(field.kind) + '.');
            return;
        }
        choices.sort(Qt::CaseInsensitive);
        bool ok = false;
        const auto target = QInputDialog::getItem(
            this, "Choose reference target", label(field.kind), choices, 0, true, &ok);
        if (!ok || !keys.contains(target)) {
            return;
        }
        const auto parts = keys.value(target).split(':');
        const auto ref_value = active.kind == Kind::Dialog && field.kind == Kind::Dialog
                                   ? QJsonValue(parts.front())
                                   : QJsonValue(parts.front().toInt());
        auto updated = replace_at(*current, field.path, ref_value).toObject();
        if (!field.flag_path.isEmpty()) {
            updated = replace_at(updated, field.flag_path, parts.value(1).toInt()).toObject();
        }
        m_editor->apply(updated);
        m_editor->select_path(field.path);
    }

    void ContentWindow::description(const QString& text) {
        statusBar()->showMessage(text, 9000);
    }

    void ContentWindow::error(const QString& text) {
        QMessageBox::warning(this, "OpenKal Content Editor", text);
    }

    void ContentWindow::closeEvent(QCloseEvent* event) {
        event->setAccepted(confirm_leave());
    }

}  // namespace openkal::content_editor
