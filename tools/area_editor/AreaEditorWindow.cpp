/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "AreaEditorWindow.h"

#include "AreaCanvas.h"
#include "openkal/engine/engine.h"
#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Coords.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

namespace {

    using openkal::area_editor::AttrEditor;
    using openkal::area_editor::BakeResult;
    using openkal::area_editor::LayerId;

    constexpr std::size_t kMaxResidentAreas = 128;
    constexpr int kStreamPreviewResolution = 768;
    constexpr int kCacheRetentionMargin = 3;
    constexpr int kAreaClassificationBatchSize = 4;
    constexpr int kFullyBlockedRole = Qt::UserRole + 1;

    [[nodiscard]] QImage rgba_image(const std::vector<std::uint8_t>& rgba, int width, int height) {
        if (rgba.empty() || width <= 0 || height <= 0) {
            return {};
        }
        return QImage(rgba.data(), width, height, width * 4, QImage::Format_RGBA8888).copy();
    }

    [[nodiscard]] QImage overlay_image(const AttrEditor& editor) {
        std::vector<std::uint8_t> rgba;
        editor.build_overlay_rgba(rgba);
        return rgba_image(
            rgba, openkal::area_editor::kOverlaySize, openkal::area_editor::kOverlaySize);
    }

    void make_attribute_background(BakeResult& bake, int resolution) {
        constexpr int kCheckerSize = 32;
        bake.image.width = resolution;
        bake.image.height = resolution;
        bake.image.rgba.resize(static_cast<std::size_t>(resolution) * resolution * 4);
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                const bool light = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0;
                const std::uint8_t value = light ? 96 : 72;
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * resolution + static_cast<std::size_t>(x)) * 4;
                bake.image.rgba[offset + 0] = value;
                bake.image.rgba[offset + 1] = value;
                bake.image.rgba[offset + 2] = value;
                bake.image.rgba[offset + 3] = 255;
            }
        }
    }

    [[nodiscard]] QString compact_path(const std::string& path) {
        return QDir::toNativeSeparators(QString::fromStdString(path));
    }

    [[nodiscard]] std::string area_file_name(int patch_x, int patch_y) {
        return QString("n_%1_%2.akarea")
            .arg(patch_x, 3, 10, QChar('0'))
            .arg(patch_y, 3, 10, QChar('0'))
            .toStdString();
    }

    void mark_area_item_gray(QListWidget& list,
                             QListWidgetItem& item,
                             const std::string& path,
                             const QString& reason) {
        QColor background = list.palette().color(QPalette::Mid);
        background.setAlpha(96);
        item.setBackground(QBrush(background));
        item.setForeground(list.palette().brush(QPalette::Disabled, QPalette::Text));
        item.setData(kFullyBlockedRole, true);
        item.setToolTip(QString("%1\n%2").arg(compact_path(path), reason));
    }

    [[nodiscard]] std::string narootuh_area_name() {
        int world_tile_x = 0;
        int world_tile_y = 0;
        int cell_x = 0;
        int cell_y = 0;
        int local_tile_x = 0;
        int local_tile_y = 0;
        openkal::coords::pixel_to_tile(openkal::coords::kNarootuhServerX,
                                       openkal::coords::kNarootuhServerY,
                                       world_tile_x,
                                       world_tile_y);
        openkal::coords::world_tile_to_cell(
            world_tile_x, world_tile_y, cell_x, cell_y, local_tile_x, local_tile_y);
        return area_file_name(cell_x, cell_y);
    }

    struct LocalRect {
        int min_x = 0;
        int min_y = 0;
        int max_x = -1;
        int max_y = -1;
        bool valid = false;
    };

    [[nodiscard]] LocalRect
    local_selection_rect(int patch_x, int patch_y, const AreaCanvas::WorldSelection& selection) {
        LocalRect rect;
        const double origin_x = static_cast<double>(patch_x) * openkal::area_editor::kOverlaySize;
        const double origin_y = static_cast<double>(patch_y) * openkal::area_editor::kOverlaySize;
        const double x0 = selection.min_x - origin_x;
        const double y0 = selection.min_y - origin_y;
        const double x1 = selection.max_x - origin_x;
        const double y1 = selection.max_y - origin_y;
        if (x1 <= 0.0 || y1 <= 0.0 || x0 >= openkal::area_editor::kOverlaySize ||
            y0 >= openkal::area_editor::kOverlaySize) {
            return rect;
        }
        rect.min_x = std::clamp(static_cast<int>(std::floor(std::max(0.0, x0))),
                                0,
                                openkal::area_editor::kOverlaySize - 1);
        rect.min_y = std::clamp(static_cast<int>(std::floor(std::max(0.0, y0))),
                                0,
                                openkal::area_editor::kOverlaySize - 1);
        rect.max_x = std::clamp(static_cast<int>(std::ceil(std::min(
                                    static_cast<double>(openkal::area_editor::kOverlaySize), x1))) -
                                    1,
                                0,
                                openkal::area_editor::kOverlaySize - 1);
        rect.max_y = std::clamp(static_cast<int>(std::ceil(std::min(
                                    static_cast<double>(openkal::area_editor::kOverlaySize), y1))) -
                                    1,
                                0,
                                openkal::area_editor::kOverlaySize - 1);
        rect.valid = rect.max_x >= rect.min_x && rect.max_y >= rect.min_y;
        return rect;
    }

    void expand_rect_to_tiles(LocalRect& rect) {
        if (!rect.valid) {
            return;
        }
        rect.min_x = rect.min_x / openkal::map::AreaCell::kSubcellsPerTile *
                     openkal::map::AreaCell::kSubcellsPerTile;
        rect.min_y = rect.min_y / openkal::map::AreaCell::kSubcellsPerTile *
                     openkal::map::AreaCell::kSubcellsPerTile;
        rect.max_x = std::min(openkal::area_editor::kOverlaySize - 1,
                              (rect.max_x / openkal::map::AreaCell::kSubcellsPerTile) *
                                      openkal::map::AreaCell::kSubcellsPerTile +
                                  openkal::map::AreaCell::kSubcellsPerTile - 1);
        rect.max_y = std::min(openkal::area_editor::kOverlaySize - 1,
                              (rect.max_y / openkal::map::AreaCell::kSubcellsPerTile) *
                                      openkal::map::AreaCell::kSubcellsPerTile +
                                  openkal::map::AreaCell::kSubcellsPerTile - 1);
    }

    [[nodiscard]] std::vector<std::vector<QPointF>>
    contour_points(const std::vector<std::vector<openkal::area_editor::ObjectMaskPoint>>& loops) {
        std::vector<std::vector<QPointF>> converted;
        converted.reserve(loops.size());
        for (const auto& loop : loops) {
            std::vector<QPointF> points;
            points.reserve(loop.size());
            for (const auto& point : loop) {
                points.emplace_back(point.x, point.y);
            }
            converted.push_back(std::move(points));
        }
        return converted;
    }

    void mark_preview_cell(QImage& image, int x, int y, const QColor& color) {
        if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
            return;
        }
        uchar* pixel = image.scanLine(y) + static_cast<std::ptrdiff_t>(x) * 4;
        pixel[0] = static_cast<uchar>(color.red());
        pixel[1] = static_cast<uchar>(color.green());
        pixel[2] = static_cast<uchar>(color.blue());
        pixel[3] = static_cast<uchar>(color.alpha());
    }

}  // namespace

AreaEditorWindow::AreaEditorWindow(openkal::engine::IEngine& engine,
                                   AreaEditorOptions options,
                                   QWidget* parent)
    : QMainWindow(parent), m_engine(engine), m_options(std::move(options)) {
    setWindowTitle("OpenKal Area Editor");
    resize(1480, 900);
    setDockNestingEnabled(true);

    m_canvas = new AreaCanvas(this);
    setCentralWidget(m_canvas);
    setup_actions();
    setup_layer_dock();
    setup_area_dock();

    m_canvas->set_paint_handler([this](int patch_x, int patch_y, int x, int y, bool erase) {
        paint_at(patch_x, patch_y, x, y, erase);
    });
    m_canvas->set_auto_handler([this](int patch_x, int patch_y, int x, int y, bool erase) {
        auto_paint_at(patch_x, patch_y, x, y, erase);
    });
    m_canvas->set_brush_handler([this](int delta) {
        m_brush_spin->setValue(std::clamp(m_brush_spin->value() + delta, 0, 32));
    });
    m_canvas->set_view_handler([this](int min_x, int min_y, int max_x, int max_y) {
        view_changed(min_x, min_y, max_x, max_y);
    });
    m_canvas->set_selection_handler([this]() { refresh_selection_preview(); });
    m_canvas->set_hover_handler([this](bool valid, int patch_x, int patch_y, int x, int y) {
        m_hover_valid = valid;
        m_hover_patch_x = patch_x;
        m_hover_patch_y = patch_y;
        m_hover_subcell_x = x;
        m_hover_subcell_y = y;
        if (m_canvas->tool() == AreaCanvas::Tool::Object) {
            refresh_object_hover(valid, patch_x, patch_y, x, y);
        } else {
            show_cursor_info(valid, patch_x, patch_y, x, y);
        }
    });
    m_canvas->set_reject_handler(
        [this]() { statusBar()->showMessage("Select a layer before painting"); });
    m_canvas->set_tool(AreaCanvas::Tool::Navigate);
    m_canvas->set_layer_armed(false);
    m_canvas->set_contours_visible(true);
    install_hotkeys();

    statusBar()->showMessage("Navigate mode — select a layer and Draw or Remove to edit");
    refresh_window_state();
    if (load_profile(m_options.profile_path)) {
        const std::string initial_area = m_options.initial_area_path.empty()
                                             ? narootuh_area_name()
                                             : m_options.initial_area_path;
        if (activate_area(initial_area, true) && !m_options.initial_output_path.empty()) {
            active_area()->save_path = m_options.initial_output_path;
        }
    }
}

void AreaEditorWindow::closeEvent(QCloseEvent* event) {
    if (confirm_unsaved()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void AreaEditorWindow::setup_actions() {
    auto* file_menu = menuBar()->addMenu("&File");
    file_menu->addAction(
        "Open &Profile…", QKeySequence::Open, this, [this]() { open_profile_dialog(); });
    m_save_action =
        file_menu->addAction("&Save", QKeySequence::Save, this, [this]() { save_areas(); });
    m_save_as_action = file_menu->addAction(
        "Save Active Area &As…", QKeySequence::SaveAs, this, [this]() { save_area_as(); });
    file_menu->addSeparator();
    m_reload_action = file_menu->addAction(
        "&Reload Active Area", QKeySequence::Refresh, this, [this]() { reload_area(); });
    file_menu->addSeparator();
    file_menu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    auto* edit_menu = menuBar()->addMenu("&Edit");
    m_undo_action = edit_menu->addAction("&Undo", QKeySequence::Undo, this, [this]() { undo(); });
    m_clear_action =
        edit_menu->addAction("Clear Active Layer…", this, [this]() { clear_active_layer(); });

    auto* view_menu = menuBar()->addMenu("&View");
    view_menu->addAction("&Fit Active Area", m_canvas, &AreaCanvas::fit_to_view);
    m_rebake_action =
        view_menu->addAction("&Rebake Active Area", this, [this]() { rebake_area(); });

    auto* toolbar = addToolBar("Area tools");
    toolbar->setMovable(false);
    auto* tool_group = new QActionGroup(this);
    tool_group->setExclusive(true);
    m_navigate_action = toolbar->addAction("Navigate");
    m_navigate_action->setCheckable(true);
    m_navigate_action->setChecked(true);
    tool_group->addAction(m_navigate_action);
    m_draw_action = toolbar->addAction("Draw");
    m_draw_action->setCheckable(true);
    tool_group->addAction(m_draw_action);
    m_remove_action = toolbar->addAction("Remove");
    m_remove_action->setCheckable(true);
    tool_group->addAction(m_remove_action);
    m_object_action = toolbar->addAction("Object");
    m_object_action->setCheckable(true);
    m_object_action->setToolTip("With Draw, click fills an object. With Remove, click clears it.");
    m_rectangle_action = toolbar->addAction("Rectangle");
    m_rectangle_action->setCheckable(true);
    tool_group->addAction(m_rectangle_action);
    connect(m_navigate_action, &QAction::triggered, this, [this]() {
        if (m_object_action != nullptr && m_object_action->isChecked()) {
            const QSignalBlocker blocker(m_object_action);
            m_object_action->setChecked(false);
        }
        m_canvas->set_tool(AreaCanvas::Tool::Navigate);
        clear_object_highlight();
        if (m_canvas->selection().present) {
            m_preview_matches = false;
            refresh_selection_preview();
        } else {
            show_cursor_info(m_hover_valid,
                             m_hover_patch_x,
                             m_hover_patch_y,
                             m_hover_subcell_x,
                             m_hover_subcell_y);
        }
    });
    connect(m_draw_action, &QAction::triggered, this, [this]() { sync_paint_tool(); });
    connect(m_remove_action, &QAction::triggered, this, [this]() { sync_paint_tool(); });
    connect(m_object_action, &QAction::toggled, this, [this](bool checked) {
        if (checked && !m_draw_action->isChecked() && !m_remove_action->isChecked()) {
            m_draw_action->setChecked(true);
        }
        sync_paint_tool();
    });
    connect(m_rectangle_action, &QAction::triggered, this, [this]() {
        if (m_object_action != nullptr && m_object_action->isChecked()) {
            const QSignalBlocker blocker(m_object_action);
            m_object_action->setChecked(false);
        }
        m_canvas->set_tool(AreaCanvas::Tool::Rectangle);
        clear_object_highlight();
        if (m_canvas->selection().present) {
            m_preview_matches = false;
            refresh_selection_preview();
        } else {
            statusBar()->showMessage("Drag a rectangle. Enter applies it, Escape clears it", 4000);
        }
    });

    toolbar->addSeparator();
    toolbar->addAction(m_undo_action);
    toolbar->addAction(m_clear_action);
    toolbar->addSeparator();
    toolbar->addAction(m_save_action);
    toolbar->addAction(m_rebake_action);
}

void AreaEditorWindow::setup_layer_dock() {
    auto* dock = new QDockWidget("Layers and Tools", this);
    dock->setObjectName("area_editor_layers");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto* scroll = new QScrollArea(dock);
    scroll->setWidgetResizable(true);
    auto* host = new QWidget(scroll);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* brush_group = new QGroupBox("Brush", host);
    auto* brush_layout = new QFormLayout(brush_group);
    m_brush_spin = new QSpinBox(brush_group);
    m_brush_spin->setRange(0, 32);
    m_brush_spin->setValue(0);
    m_brush_spin->setSuffix(" radius");
    brush_layout->addRow("Size", m_brush_spin);
    auto* fit_button = new QPushButton("Fit active area", brush_group);
    brush_layout->addRow(fit_button);
    connect(fit_button, &QPushButton::clicked, m_canvas, &AreaCanvas::fit_to_view);
    connect(m_brush_spin, &QSpinBox::valueChanged, this, [this](int value) {
        for (auto& [key, area] : m_loaded_areas) {
            (void) key;
            area->editor->layers().brush_radius = value;
        }
        m_canvas->set_brush_radius(value);
    });
    layout->addWidget(brush_group);

    auto* view_group = new QGroupBox("View", host);
    auto* view_layout = new QVBoxLayout(view_group);
    m_contours_check = new QCheckBox("Object contours", view_group);
    m_contours_check->setChecked(true);
    m_grid_check = new QCheckBox("Grid when zoomed in", view_group);
    view_layout->addWidget(m_contours_check);
    view_layout->addWidget(m_grid_check);
    connect(m_contours_check, &QCheckBox::toggled, m_canvas, &AreaCanvas::set_contours_visible);
    connect(m_grid_check, &QCheckBox::toggled, m_canvas, &AreaCanvas::set_grid_visible);
    layout->addWidget(view_group);

    auto* selection_group = new QGroupBox("Rectangle", host);
    auto* selection_layout = new QVBoxLayout(selection_group);
    m_fit_radio = new QRadioButton("Fit to object contours", selection_group);
    m_fill_radio = new QRadioButton("Fill rectangle", selection_group);
    m_fit_radio->setChecked(true);
    m_selection_erase = new QCheckBox("Erase", selection_group);
    m_apply_button = new QPushButton("Apply selection", selection_group);
    m_apply_button->setEnabled(false);
    selection_layout->addWidget(m_fit_radio);
    selection_layout->addWidget(m_fill_radio);
    selection_layout->addWidget(m_selection_erase);
    selection_layout->addWidget(m_apply_button);
    connect(m_fit_radio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_preview_matches = false;
            refresh_selection_preview();
        }
    });
    connect(m_fill_radio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_preview_matches = false;
            refresh_selection_preview();
        }
    });
    connect(m_selection_erase, &QCheckBox::toggled, this, [this](bool checked) {
        set_erase_tool(checked);
    });
    connect(m_apply_button, &QPushButton::clicked, this, [this]() { apply_selection(); });
    layout->addWidget(selection_group);

    auto* layer_group = new QGroupBox("Attribute layer", host);
    m_layer_group = new QButtonGroup(this);
    m_layer_group->setExclusive(true);
    auto* layer_layout = new QVBoxLayout(layer_group);
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        const auto layer = static_cast<LayerId>(index);
        const auto& info = openkal::area_editor::layer_info(layer);
        auto* row = new QWidget(layer_group);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 1, 0, 1);
        auto* swatch = new QLabel(row);
        swatch->setFixedSize(16, 16);
        swatch->setStyleSheet(QString("background: rgb(%1,%2,%3); border: 1px solid #555;")
                                  .arg(static_cast<int>(info.r * 255.0f))
                                  .arg(static_cast<int>(info.g * 255.0f))
                                  .arg(static_cast<int>(info.b * 255.0f)));
        auto* radio = new QRadioButton(QString("%1  %2").arg(index + 1).arg(info.name), row);
        m_layer_group->addButton(radio, index);
        auto* visible = new QCheckBox("Visible", row);
        visible->setChecked(index == 0);
        m_layer_radios[static_cast<std::size_t>(index)] = radio;
        m_layer_visibility[static_cast<std::size_t>(index)] = visible;
        row_layout->addWidget(swatch);
        row_layout->addWidget(radio, 1);
        row_layout->addWidget(visible);
        layer_layout->addWidget(row);
        connect(radio, &QRadioButton::toggled, this, [this, layer](bool checked) {
            if (checked) {
                set_active_layer(layer);
            }
        });
        connect(visible, &QCheckBox::toggled, this, [this, layer](bool checked) {
            set_layer_visible(layer, checked);
        });
    }
    layout->addWidget(layer_group);

    auto* hint =
        new QLabel("Navigate is selected and does not edit\n"
                   "Mouse wheel: zoom\n"
                   "Left drag in Navigate, right/middle drag, or Space: pan\n"
                   "Draw paints and Remove erases. Object applies that to a clicked contour\n"
                   "Rectangle, then Enter: apply the active layer\n"
                   "A: toggle Object, or apply it to the object under the cursor\n"
                   "1-8: select a layer, 0: clear the layer\n"
                   "Shift+wheel: brush size",
                   host);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: palette(mid);");
    layout->addWidget(hint);
    layout->addStretch(1);
    scroll->setWidget(host);
    dock->setWidget(scroll);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void AreaEditorWindow::setup_area_dock() {
    auto* dock = new QDockWidget("Areas", this);
    dock->setObjectName("area_editor_areas");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto* host = new QWidget(dock);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(8, 8, 8, 8);

    m_profile_label = new QLabel("No profile loaded", host);
    m_profile_label->setWordWrap(true);
    m_profile_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_profile_label);

    auto* open_profile = new QPushButton("Open profile…", host);
    connect(open_profile, &QPushButton::clicked, this, [this]() { open_profile_dialog(); });
    layout->addWidget(open_profile);

    m_area_search = new QLineEdit(host);
    m_area_search->setClearButtonEnabled(true);
    m_area_search->setPlaceholderText("Search areas…");
    layout->addWidget(m_area_search);

    m_area_count = new QLabel(host);
    layout->addWidget(m_area_count);

    m_area_list = new QListWidget(host);
    m_area_list->setAlternatingRowColors(true);
    m_area_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_area_list, 1);

    connect(m_area_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        filter_area_list(text);
    });
    connect(m_area_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) {
            (void) activate_area(item->data(Qt::UserRole).toString().toStdString(), true);
        }
    });

    dock->setWidget(host);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void AreaEditorWindow::open_profile_dialog() {
    if (!confirm_unsaved()) {
        return;
    }
    const QString start = m_profile_path.empty() ? QString::fromStdString(m_options.repo_root)
                                                 : QString::fromStdString(m_profile_path);
    const QString path = QFileDialog::getOpenFileName(
        this, "Open OpenKal Profile", start, "OpenKal profile (*.profile);;All files (*)");
    if (!path.isEmpty()) {
        (void) load_profile(path.toStdString());
    }
}

bool AreaEditorWindow::load_profile(const std::string& profile_path) {
    std::string error;
    const auto runtime =
        openkal::platform::RuntimeProfile::load_for_tool(m_options.repo_root, profile_path, &error);
    if (!runtime) {
        QMessageBox::warning(this, "Profile load failed", QString::fromStdString(error));
        statusBar()->showMessage(QString::fromStdString(error));
        return false;
    }

    const std::string map_catalog_path =
        openkal::platform::join_path(runtime->configuration.configuration_root, "mapinfo.json");
    openkal::map::MapCatalog map_catalog;
    if (!map_catalog.load(map_catalog_path)) {
        QMessageBox::warning(this,
                             "map definition load failed",
                             QString("Could not build the world layout from %1")
                                 .arg(compact_path(map_catalog_path)));
        statusBar()->showMessage("Could not load mapinfo.json");
        return false;
    }

    std::vector<std::pair<int, int>> world_layout;
    std::unordered_set<std::uint64_t> layout_keys;
    for (const auto& entry : map_catalog.entries()) {
        if (entry.kind() != 1 || entry.x() < 0 || entry.y() < 0 ||
            entry.x() >= openkal::map::kPatchGrid || entry.y() >= openkal::map::kPatchGrid) {
            continue;
        }
        if (layout_keys.insert(area_key(entry.x(), entry.y())).second) {
            world_layout.emplace_back(entry.x(), entry.y());
        }
    }
    if (world_layout.empty()) {
        QMessageBox::warning(
            this, "Empty map definition", "mapinfo.json has no outdoor area entries.");
        statusBar()->showMessage("mapinfo.json has no outdoor areas");
        return false;
    }
    std::sort(world_layout.begin(), world_layout.end());

    clear_loaded_areas();
    m_profile_path = runtime->profile_path;
    m_data_root = m_options.data_root_override.empty() ? runtime->profile.data_root()
                                                       : m_options.data_root_override;
    m_area_directory = runtime->profile.server_map_dir();
    m_map_catalog_path = map_catalog_path;
    m_world_layout = std::move(world_layout);
    m_profile_label->setText(QString("Profile: %1\nMap catalog: %2\nArea directory: %3")
                                 .arg(compact_path(m_profile_path),
                                      compact_path(m_map_catalog_path),
                                      compact_path(m_area_directory)));
    refresh_area_list();
    statusBar()->showMessage(
        QString("Loaded %1 outdoor areas from mapinfo.json").arg(m_world_layout.size()), 5000);
    return true;
}

void AreaEditorWindow::refresh_area_list() {
    ++m_area_list_generation;
    m_area_classification_scheduled = false;
    m_area_classification_queue.clear();
    m_area_list->clear();
    m_area_paths.clear();
    for (const auto& [patch_x, patch_y] : m_world_layout) {
        const std::string name = area_file_name(patch_x, patch_y);
        const std::string path = openkal::platform::join_path(m_area_directory, name);
        m_area_paths[area_key(patch_x, patch_y)] = path;
        auto* item = new QListWidgetItem(QString::fromStdString(name), m_area_list);
        item->setData(Qt::UserRole, QString::fromStdString(path));
        item->setData(kFullyBlockedRole, false);
        item->setToolTip(compact_path(path));
        if (openkal::platform::file_exists(path)) {
            m_area_classification_queue.emplace_back(item, path);
        } else {
            mark_area_item_gray(*m_area_list, *item, path, "Missing area file; treated as blocked");
        }
    }
    filter_area_list(m_area_search->text());
    select_active_area_in_list();
    schedule_area_classification();
}

void AreaEditorWindow::schedule_area_classification() {
    if (m_area_classification_scheduled || m_area_classification_queue.empty()) {
        return;
    }
    m_area_classification_scheduled = true;
    const std::uint64_t generation = m_area_list_generation;
    QTimer::singleShot(0, this, [this, generation]() { process_area_classification(generation); });
}

void AreaEditorWindow::process_area_classification(std::uint64_t generation) {
    if (generation != m_area_list_generation) {
        return;
    }
    m_area_classification_scheduled = false;
    for (int index = 0;
         index < kAreaClassificationBatchSize && !m_area_classification_queue.empty();
         ++index) {
        auto [item, path] = std::move(m_area_classification_queue.front());
        m_area_classification_queue.pop_front();
        openkal::map::AreaCell cell;
        if (!openkal::map::AreaMapCodec::load(path, cell) ||
            !openkal::area_editor::is_fully_blocked(cell)) {
            continue;
        }

        mark_area_item_gray(*m_area_list, *item, path, "Fully blocked area");
    }
    schedule_area_classification();
}

void AreaEditorWindow::filter_area_list(const QString& filter) {
    const QString needle = filter.trimmed();
    int visible = 0;
    for (int row = 0; row < m_area_list->count(); ++row) {
        QListWidgetItem* item = m_area_list->item(row);
        const bool matches = needle.isEmpty() || item->text().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!matches);
        visible += matches ? 1 : 0;
    }
    m_area_count->setText(QString("%1 of %2 areas").arg(visible).arg(m_area_list->count()));
}

bool AreaEditorWindow::activate_area(const std::string& path, bool center) {
    const std::string resolved_path = resolve_area_path(path);
    int patch_x = 0;
    int patch_y = 0;
    if (!openkal::area_editor::parse_patch_name(resolved_path, patch_x, patch_y)) {
        QMessageBox::warning(
            this, "Invalid area", "Could not parse patch indices from the file name.");
        return false;
    }
    const std::uint64_t key = area_key(patch_x, patch_y);
    if (m_loaded_areas.find(key) == m_loaded_areas.end()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        LoadedArea* loaded = load_area_document(resolved_path, true, m_options.bake_resolution);
        QApplication::restoreOverrideCursor();
        if (!loaded) {
            return false;
        }
    }
    set_active_area(key, center);
    LoadedArea* area = active_area();
    if (area && area->bake.image.width < m_options.bake_resolution) {
        rebake_area();
    }
    schedule_visible_areas();
    return true;
}

AreaEditorWindow::LoadedArea* AreaEditorWindow::load_area_document(const std::string& path,
                                                                   bool show_errors,
                                                                   int bake_resolution) {
    const std::string resolved_path = resolve_area_path(path);
    int patch_x = 0;
    int patch_y = 0;
    if (!openkal::area_editor::parse_patch_name(resolved_path, patch_x, patch_y)) {
        return nullptr;
    }
    const std::uint64_t key = area_key(patch_x, patch_y);
    const auto existing = m_loaded_areas.find(key);
    if (existing != m_loaded_areas.end()) {
        return existing->second.get();
    }

    openkal::map::AreaCell cell;
    const bool source_exists = openkal::platform::file_exists(resolved_path);
    if (source_exists && !openkal::map::AreaMapCodec::load(resolved_path, cell)) {
        if (show_errors) {
            QMessageBox::warning(this,
                                 "Area load failed",
                                 QString("Could not load %1").arg(compact_path(resolved_path)));
        }
        return nullptr;
    }
    if (!source_exists) {
        cell.ensure_full_attrs();
        cell.set_blocked_masks(std::vector<openkal::map::AreaCell::SubcellMask>(
            static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount,
            0xffffu));
    }
    cell.set_cell_x(patch_x);
    cell.set_cell_y(patch_y);

    statusBar()->showMessage(QString("Streaming %1…").arg(compact_path(resolved_path)));
    const auto bake_paths = openkal::area_editor::resolve_bake_paths(resolved_path, m_data_root);
    BakeResult bake = openkal::area_editor::bake_patch_topdown(
        m_engine, bake_paths, patch_x, patch_y, bake_resolution);
    if (!bake.error.empty() || bake.image.rgba.empty()) {
        make_attribute_background(bake, bake_resolution);
    }

    auto area = std::make_unique<LoadedArea>();
    area->patch_x = patch_x;
    area->patch_y = patch_y;
    area->source_path = resolved_path;
    area->save_path = resolved_path;
    area->bake_paths = bake_paths;
    area->bake = std::move(bake);
    area->editor = std::make_unique<AttrEditor>(std::move(cell));
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        if (m_layer_radios[static_cast<std::size_t>(index)]->isChecked()) {
            area->editor->set_active(static_cast<LayerId>(index));
            break;
        }
    }
    area->editor->layers().brush_radius = m_brush_spin->value();
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        area->editor->layers().visible[index] =
            m_layer_visibility[static_cast<std::size_t>(index)]->isChecked();
    }

    QImage bake_image =
        rgba_image(area->bake.image.rgba, area->bake.image.width, area->bake.image.height);
    QImage attributes = overlay_image(*area->editor);
    area->bake.image.rgba.clear();
    area->bake.image.rgba.shrink_to_fit();
    LoadedArea* result = area.get();
    m_loaded_areas.emplace(key, std::move(area));
    m_canvas->set_tile(patch_x, patch_y, std::move(bake_image), std::move(attributes));
    publish_contours(*result);
    if (m_canvas->tool() == AreaCanvas::Tool::Object) {
        m_object_preview_mask = nullptr;
        refresh_object_hover(
            m_hover_valid, m_hover_patch_x, m_hover_patch_y, m_hover_subcell_x, m_hover_subcell_y);
    } else if (m_canvas->selection().present) {
        m_preview_matches = false;
        refresh_selection_preview();
    }
    statusBar()->showMessage(QString("Loaded %1 — %2 objects")
                                 .arg(QFileInfo(QString::fromStdString(resolved_path)).fileName())
                                 .arg(result->bake.prop_count),
                             3000);
    refresh_window_state();
    return result;
}

void AreaEditorWindow::reload_area() {
    LoadedArea* area = active_area();
    if (!area) {
        return;
    }
    if (area->editor->dirty()) {
        const auto answer =
            QMessageBox::question(this,
                                  "Unsaved changes",
                                  "Save the active area before reloading it?",
                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel ||
            (answer == QMessageBox::Save && !save_to(*area, area->save_path, true))) {
            return;
        }
    }
    const std::uint64_t key = area_key(area->patch_x, area->patch_y);
    const std::string path = area->source_path;
    m_canvas->remove_tile(area->patch_x, area->patch_y);
    m_loaded_areas.erase(key);
    if (load_area_document(path, true, m_options.bake_resolution)) {
        set_active_area(key, false);
    }
}

void AreaEditorWindow::rebake_area() {
    LoadedArea* area = active_area();
    if (!area) {
        return;
    }
    statusBar()->showMessage("Rebaking active terrain and objects…");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    BakeResult bake = openkal::area_editor::bake_patch_topdown(
        m_engine, area->bake_paths, area->patch_x, area->patch_y, m_options.bake_resolution);
    QApplication::restoreOverrideCursor();
    if (!bake.error.empty() || bake.image.rgba.empty()) {
        QMessageBox::warning(
            this,
            "Rebake failed",
            QString::fromStdString(bake.error.empty() ? "empty image" : bake.error));
        return;
    }
    QImage image = rgba_image(bake.image.rgba, bake.image.width, bake.image.height);
    bake.image.rgba.clear();
    bake.image.rgba.shrink_to_fit();
    m_object_preview_mask = nullptr;
    area->bake = std::move(bake);
    m_canvas->set_tile(
        area->patch_x, area->patch_y, std::move(image), overlay_image(*area->editor));
    publish_contours(*area);
    if (m_canvas->tool() == AreaCanvas::Tool::Object) {
        refresh_object_hover(
            m_hover_valid, m_hover_patch_x, m_hover_patch_y, m_hover_subcell_x, m_hover_subcell_y);
    } else if (m_canvas->selection().present) {
        m_preview_matches = false;
        refresh_selection_preview();
    }
    statusBar()->showMessage(QString("Rebaked %1 objects").arg(area->bake.prop_count), 5000);
}

void AreaEditorWindow::save_areas() {
    int saved = 0;
    int failed = 0;
    for (auto& [key, area] : m_loaded_areas) {
        (void) key;
        if (!area->editor->dirty()) {
            continue;
        }
        if (save_to(*area, area->save_path, true)) {
            ++saved;
        } else {
            ++failed;
        }
    }
    if (saved > 0 && failed == 0) {
        statusBar()->showMessage(QString("Saved %1 changed area(s)").arg(saved), 5000);
    }
    if (saved > 0) {
        refresh_area_list();
    }
    refresh_window_state();
}

void AreaEditorWindow::save_area_as() {
    LoadedArea* area = active_area();
    if (!area) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this,
                                                      "Save Active Area As",
                                                      QString::fromStdString(area->save_path),
                                                      "OpenKal area (*.akarea);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    int output_x = 0;
    int output_y = 0;
    if (!openkal::area_editor::parse_patch_name(path.toStdString(), output_x, output_y) ||
        output_x != area->patch_x || output_y != area->patch_y) {
        QMessageBox::warning(this,
                             "Invalid area name",
                             "The output filename must keep the active n_XXX_YYY coordinates.");
        return;
    }
    if (save_to(*area, path.toStdString(), false)) {
        area->source_path = path.toStdString();
        area->save_path = area->source_path;
        area->wrote_backup = true;
        m_area_paths[area_key(area->patch_x, area->patch_y)] = area->source_path;
        refresh_area_list();
        refresh_window_state();
    }
}

bool AreaEditorWindow::save_to(LoadedArea& area,
                               const std::string& path,
                               bool allow_temp_fallback) {
    if (!area.wrote_backup && path == area.source_path && openkal::platform::file_exists(path)) {
        std::error_code error;
        std::filesystem::copy_file(
            path, path + ".bak", std::filesystem::copy_options::overwrite_existing, error);
        if (!error) {
            area.wrote_backup = true;
        }
    }
    if (!openkal::map::AreaMapCodec::save(path, area.editor->grid())) {
        if (allow_temp_fallback) {
            std::error_code error;
            const std::filesystem::path requested(path);
            const std::filesystem::path temp_path =
                std::filesystem::temp_directory_path(error) /
                (requested.has_filename() ? requested.filename()
                                          : std::filesystem::path("edited.akarea"));
            const std::string temp = temp_path.string();
            if (!error && temp != path &&
                openkal::map::AreaMapCodec::save(temp, area.editor->grid())) {
                area.save_path = temp;
                area.editor->clear_dirty();
                QMessageBox::warning(this,
                                     "Saved to temporary directory",
                                     QString("Could not write %1. The edited area was saved to %2.")
                                         .arg(compact_path(path), compact_path(temp)));
                return true;
            }
        }
        QMessageBox::warning(
            this, "Save failed", QString("Could not write %1").arg(compact_path(path)));
        return false;
    }
    area.save_path = path;
    area.editor->clear_dirty();
    return true;
}

void AreaEditorWindow::undo() {
    LoadedArea* area = active_area();
    if (area && area->editor->undo()) {
        refresh_overlay(*area);
        refresh_window_state();
        statusBar()->showMessage("Undo", 2000);
    }
}

void AreaEditorWindow::clear_active_layer() {
    LoadedArea* area = active_area();
    if (!area || !m_layer_armed) {
        statusBar()->showMessage("Select a layer before clearing it");
        return;
    }
    const QString name =
        QString::fromUtf8(openkal::area_editor::layer_name(area->editor->layers().active));
    if (QMessageBox::question(this,
                              "Clear layer",
                              QString("Clear the entire %1 layer in the active area?").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    area->editor->clear_active_layer();
    refresh_overlay(*area);
    refresh_window_state();
}

void AreaEditorWindow::set_active_layer(LayerId layer) {
    const int index = static_cast<int>(layer);
    if (!m_layer_visibility[static_cast<std::size_t>(index)]->isChecked()) {
        const QSignalBlocker blocker(m_layer_visibility[static_cast<std::size_t>(index)]);
        m_layer_visibility[static_cast<std::size_t>(index)]->setChecked(true);
    }
    for (auto& [key, area] : m_loaded_areas) {
        (void) key;
        area->editor->set_active(layer);
        area->editor->layers().visible[index] = true;
    }
    refresh_all_overlays();
    m_layer_armed = true;
    m_canvas->set_layer(layer);
    m_canvas->set_layer_armed(true);
    m_preview_matches = false;
    m_object_preview_mask = nullptr;
    if (m_canvas->tool() == AreaCanvas::Tool::Object) {
        refresh_object_hover(
            m_hover_valid, m_hover_patch_x, m_hover_patch_y, m_hover_subcell_x, m_hover_subcell_y);
    } else {
        refresh_selection_preview();
    }
    refresh_window_state();
}

void AreaEditorWindow::install_hotkeys() {
    const auto typing = []() {
        const QWidget* focus = QApplication::focusWidget();
        return qobject_cast<const QLineEdit*>(focus) != nullptr ||
               qobject_cast<const QAbstractSpinBox*>(focus) != nullptr;
    };
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::Key_1 + index), this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, index, typing]() {
            if (typing() || m_layer_radios[static_cast<std::size_t>(index)] == nullptr) {
                return;
            }
            m_layer_radios[static_cast<std::size_t>(index)]->setChecked(true);
        });
    }
    auto* clear_layer = new QShortcut(QKeySequence(Qt::Key_0), this);
    clear_layer->setContext(Qt::WindowShortcut);
    connect(clear_layer, &QShortcut::activated, this, [this, typing]() {
        if (!typing()) {
            clear_layer_selection();
        }
    });
    const auto bind_apply = [this, typing](int key) {
        auto* shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, typing]() {
            if (!typing()) {
                apply_selection();
            }
        });
    };
    bind_apply(Qt::Key_Return);
    bind_apply(Qt::Key_Enter);
    auto* object_tool = new QShortcut(QKeySequence(Qt::Key_A), this);
    object_tool->setContext(Qt::WindowShortcut);
    connect(object_tool, &QShortcut::activated, this, [this, typing]() {
        if (typing() || m_object_action == nullptr) {
            return;
        }
        if (m_object_action->isChecked()) {
            m_canvas->trigger_auto_at_cursor();
            return;
        }
        m_object_action->setChecked(true);
    });
    auto* clear_selection = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    clear_selection->setContext(Qt::WindowShortcut);
    connect(clear_selection, &QShortcut::activated, this, [this, typing]() {
        if (!typing()) {
            m_canvas->clear_selection();
        }
    });
}

void AreaEditorWindow::clear_layer_selection() {
    m_layer_group->setExclusive(false);
    for (QRadioButton* radio : m_layer_radios) {
        if (radio != nullptr) {
            radio->setChecked(false);
        }
    }
    m_layer_group->setExclusive(true);
    m_layer_armed = false;
    m_canvas->set_layer_armed(false);
    m_preview_matches = false;
    m_object_preview_mask = nullptr;
    if (m_canvas->tool() == AreaCanvas::Tool::Object) {
        refresh_object_hover(
            m_hover_valid, m_hover_patch_x, m_hover_patch_y, m_hover_subcell_x, m_hover_subcell_y);
    } else {
        refresh_selection_preview();
    }
    refresh_window_state();
    if (!m_canvas->selection().present) {
        statusBar()->showMessage("No layer selected");
    }
}

void AreaEditorWindow::set_layer_visible(LayerId layer, bool visible) {
    for (auto& [key, area] : m_loaded_areas) {
        (void) key;
        area->editor->layers().visible[static_cast<int>(layer)] = visible;
    }
    refresh_all_overlays();
}

void AreaEditorWindow::sync_paint_tool() {
    const bool object = m_object_action != nullptr && m_object_action->isChecked();
    const bool remove = m_remove_action != nullptr && m_remove_action->isChecked();
    const bool draw = m_draw_action != nullptr && m_draw_action->isChecked();
    if (object && (draw || remove)) {
        m_canvas->set_tool(AreaCanvas::Tool::Object);
        set_erase_tool(remove);
        return;
    }
    clear_object_highlight();
    if (remove) {
        m_canvas->set_tool(AreaCanvas::Tool::Remove);
        set_erase_tool(true);
        return;
    }
    if (draw) {
        m_canvas->set_tool(AreaCanvas::Tool::Draw);
        set_erase_tool(false);
    }
}

void AreaEditorWindow::set_erase_tool(bool erase) {
    m_erase_tool = erase;
    m_canvas->set_erase(erase);
    if (m_selection_erase != nullptr && m_selection_erase->isChecked() != erase) {
        const QSignalBlocker blocker(m_selection_erase);
        m_selection_erase->setChecked(erase);
    }
    m_preview_matches = false;
    m_object_preview_mask = nullptr;
    if (m_canvas->tool() == AreaCanvas::Tool::Object) {
        refresh_object_hover(
            m_hover_valid, m_hover_patch_x, m_hover_patch_y, m_hover_subcell_x, m_hover_subcell_y);
        return;
    }
    refresh_selection_preview();
    if (!m_canvas->selection().present) {
        statusBar()->showMessage(erase ? "Erase" : "Draw", 2000);
    }
}

void AreaEditorWindow::paint_at(int patch_x, int patch_y, int x, int y, bool erase) {
    if (!m_layer_armed || m_canvas->tool() == AreaCanvas::Tool::Navigate ||
        m_canvas->tool() == AreaCanvas::Tool::Rectangle ||
        m_canvas->tool() == AreaCanvas::Tool::Object) {
        statusBar()->showMessage("Select Draw or Remove and a layer before painting");
        return;
    }
    const std::uint64_t key = area_key(patch_x, patch_y);
    const auto found = m_loaded_areas.find(key);
    if (found == m_loaded_areas.end()) {
        const auto path = m_area_paths.find(key);
        if (path != m_area_paths.end() && m_queued_areas.insert(key).second) {
            m_load_queue.push_front(key);
            if (!m_load_scheduled) {
                m_load_scheduled = true;
                QTimer::singleShot(0, this, [this]() { process_load_queue(); });
            }
        }
        statusBar()->showMessage("Area is still loading", 2000);
        return;
    }
    if (!m_has_active_area || m_active_area_key != key) {
        set_active_area(key, false);
    }
    found->second->editor->paint_at(x, y, erase);
    refresh_overlay(*found->second);
    refresh_window_state();
}

void AreaEditorWindow::auto_paint_at(
    int patch_x, int patch_y, int subcell_x, int subcell_y, bool erase) {
    if (!m_layer_armed) {
        statusBar()->showMessage("Select a layer before editing an object contour");
        return;
    }
    const std::uint64_t key = area_key(patch_x, patch_y);
    const auto found = m_loaded_areas.find(key);
    if (found == m_loaded_areas.end()) {
        statusBar()->showMessage("Area is still loading", 2000);
        return;
    }
    if (!m_has_active_area || m_active_area_key != key) {
        set_active_area(key, false);
    }
    LayerId layer = LayerId::Blocked;
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        if (m_layer_radios[static_cast<std::size_t>(index)] != nullptr &&
            m_layer_radios[static_cast<std::size_t>(index)]->isChecked()) {
            layer = static_cast<LayerId>(index);
            break;
        }
    }
    LoadedArea& area = *found->second;
    const auto* mask =
        openkal::area_editor::pick_object_mask(area.bake.object_masks, subcell_x, subcell_y);
    if (!mask) {
        statusBar()->showMessage("No object under cursor", 3000);
        return;
    }
    area.editor->set_active(layer);
    const std::size_t changed = area.editor->paint_subcell_mask(mask->footprint_cells, erase);
    if (changed != 0) {
        refresh_overlay(area);
        refresh_window_state();
    }
    const bool zone = !openkal::area_editor::layer_uses_subcells(layer);
    statusBar()->showMessage(QString("%1 %2 — %3 %4 changed")
                                 .arg(erase ? "Cleared" : "Filled")
                                 .arg(QString::fromStdString(mask->name))
                                 .arg(changed)
                                 .arg(zone ? "tiles" : "subcells"),
                             5000);
}

void AreaEditorWindow::clear_object_highlight() {
    m_object_preview_mask = nullptr;
    if (!m_object_highlight_visible) {
        return;
    }
    m_object_highlight_visible = false;
    m_canvas->clear_previews();
}

void AreaEditorWindow::refresh_object_hover(
    bool valid, int patch_x, int patch_y, int subcell_x, int subcell_y) {
    if (m_canvas->tool() != AreaCanvas::Tool::Object) {
        return;
    }
    if (!valid) {
        clear_object_highlight();
        statusBar()->showMessage("Draw fills the object contour, Remove clears it");
        return;
    }
    const auto found = m_loaded_areas.find(area_key(patch_x, patch_y));
    if (found == m_loaded_areas.end()) {
        clear_object_highlight();
        statusBar()->showMessage("Area is still loading", 2000);
        return;
    }
    const openkal::area_editor::ObjectMask* mask = openkal::area_editor::pick_object_mask(
        found->second->bake.object_masks, subcell_x, subcell_y);
    if (mask == nullptr) {
        clear_object_highlight();
        statusBar()->showMessage("No object under cursor");
        return;
    }

    int layer_index = -1;
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        if (m_layer_radios[static_cast<std::size_t>(index)] != nullptr &&
            m_layer_radios[static_cast<std::size_t>(index)]->isChecked()) {
            layer_index = index;
            break;
        }
    }
    if (m_object_highlight_visible && mask == m_object_preview_mask &&
        patch_x == m_object_preview_patch_x && patch_y == m_object_preview_patch_y &&
        layer_index == m_object_preview_layer && m_erase_tool == m_object_preview_erase) {
        return;
    }

    QColor color(255, 220, 70, 120);
    QString layer_name = "no layer";
    const bool zone = layer_index >= 0 &&
                      !openkal::area_editor::layer_uses_subcells(static_cast<LayerId>(layer_index));
    if (layer_index >= 0) {
        const auto& info = openkal::area_editor::layer_info(static_cast<LayerId>(layer_index));
        layer_name = QString::fromUtf8(info.name);
        color = m_erase_tool ? QColor(255, 70, 70, 140)
                             : QColor::fromRgbF(info.r, info.g, info.b, 0.45);
    }

    QImage image(openkal::area_editor::kOverlaySize,
                 openkal::area_editor::kOverlaySize,
                 QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    int marked = 0;
    if (zone) {
        QPainter painter(&image);
        std::vector<std::uint8_t> tile_seen(
            static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount, 0);
        for (const std::uint32_t cell : mask->footprint_cells) {
            const int x = static_cast<int>(
                cell % static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
            const int y = static_cast<int>(
                cell / static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
            const int tile_x = x / openkal::map::AreaCell::kSubcellsPerTile;
            const int tile_y = y / openkal::map::AreaCell::kSubcellsPerTile;
            const std::size_t flat = static_cast<std::size_t>(tile_y) * openkal::map::kTileCount +
                                     static_cast<std::size_t>(tile_x);
            if (tile_seen[flat] != 0) {
                continue;
            }
            tile_seen[flat] = 1;
            painter.fillRect(tile_x * openkal::map::AreaCell::kSubcellsPerTile,
                             tile_y * openkal::map::AreaCell::kSubcellsPerTile,
                             openkal::map::AreaCell::kSubcellsPerTile,
                             openkal::map::AreaCell::kSubcellsPerTile,
                             color);
            ++marked;
        }
    } else {
        for (const std::uint32_t cell : mask->footprint_cells) {
            const int x = static_cast<int>(
                cell % static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
            const int y = static_cast<int>(
                cell / static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
            mark_preview_cell(image, x, y, color);
        }
        marked = static_cast<int>(mask->footprint_cells.size());
    }

    m_canvas->clear_previews();
    m_canvas->set_tile_preview(patch_x, patch_y, std::move(image));
    m_object_highlight_visible = true;
    m_object_preview_mask = mask;
    m_object_preview_patch_x = patch_x;
    m_object_preview_patch_y = patch_y;
    m_object_preview_layer = layer_index;
    m_object_preview_erase = m_erase_tool;
    const QString action = layer_index < 0 ? QString("select a layer, then click")
                           : m_erase_tool  ? QString("click clears %1").arg(layer_name)
                                           : QString("click fills %1").arg(layer_name);
    statusBar()->showMessage(QString("%1 — %2 %3 — %4")
                                 .arg(QString::fromStdString(mask->name))
                                 .arg(marked)
                                 .arg(zone ? "tiles" : "subcells")
                                 .arg(action));
}

void AreaEditorWindow::publish_contours(const LoadedArea& area) {
    std::vector<std::vector<QPointF>> collision;
    std::vector<std::vector<QPointF>> display;
    for (const openkal::area_editor::ObjectMask& mask : area.bake.object_masks) {
        std::vector<std::vector<QPointF>> collision_loops = contour_points(mask.collision_contours);
        std::vector<std::vector<QPointF>> display_loops = contour_points(mask.display_contours);
        collision.insert(collision.end(),
                         std::make_move_iterator(collision_loops.begin()),
                         std::make_move_iterator(collision_loops.end()));
        display.insert(display.end(),
                       std::make_move_iterator(display_loops.begin()),
                       std::make_move_iterator(display_loops.end()));
    }
    m_canvas->set_tile_contours(
        area.patch_x, area.patch_y, std::move(collision), std::move(display));
}

void AreaEditorWindow::refresh_selection_preview() {
    const AreaCanvas::WorldSelection selected = m_canvas->selection();
    if (!selected.present) {
        m_preview_matches = false;
        m_canvas->clear_previews();
        m_canvas->set_selection_fill(false, false, QColor());
        refresh_window_state();
        return;
    }

    const bool fit = m_fit_radio == nullptr || m_fit_radio->isChecked();
    const int quantized_min_x = static_cast<int>(std::floor(selected.min_x));
    const int quantized_min_y = static_cast<int>(std::floor(selected.min_y));
    const int quantized_max_x = static_cast<int>(std::ceil(selected.max_x));
    const int quantized_max_y = static_cast<int>(std::ceil(selected.max_y));
    int layer_index = -1;
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        if (m_layer_radios[static_cast<std::size_t>(index)] != nullptr &&
            m_layer_radios[static_cast<std::size_t>(index)]->isChecked()) {
            layer_index = index;
            break;
        }
    }
    if (m_preview_matches && quantized_min_x == m_preview_min_x &&
        quantized_min_y == m_preview_min_y && quantized_max_x == m_preview_max_x &&
        quantized_max_y == m_preview_max_y && fit == m_preview_fit &&
        m_erase_tool == m_preview_erase && layer_index == m_preview_layer) {
        return;
    }
    m_preview_matches = true;
    m_preview_min_x = quantized_min_x;
    m_preview_min_y = quantized_min_y;
    m_preview_max_x = quantized_max_x;
    m_preview_max_y = quantized_max_y;
    m_preview_fit = fit;
    m_preview_erase = m_erase_tool;
    m_preview_layer = layer_index;

    QColor color(255, 255, 255, 110);
    if (layer_index >= 0) {
        const auto& info = openkal::area_editor::layer_info(static_cast<LayerId>(layer_index));
        color = m_erase_tool ? QColor(255, 70, 70, 130)
                             : QColor::fromRgbF(info.r, info.g, info.b, 0.45);
    }
    const bool zone = layer_index >= 0 &&
                      !openkal::area_editor::layer_uses_subcells(static_cast<LayerId>(layer_index));
    m_canvas->set_selection_fill(false, false, QColor());
    m_canvas->clear_previews();
    int patches = 0;
    int marked = 0;
    for (auto& [key, area] : m_loaded_areas) {
        (void) key;
        LocalRect rect = local_selection_rect(area->patch_x, area->patch_y, selected);
        if (!rect.valid) {
            continue;
        }
        QImage image(openkal::area_editor::kOverlaySize,
                     openkal::area_editor::kOverlaySize,
                     QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        bool any = false;
        if (!fit) {
            if (zone) {
                expand_rect_to_tiles(rect);
            }
            QPainter painter(&image);
            painter.fillRect(QRect(rect.min_x,
                                   rect.min_y,
                                   rect.max_x - rect.min_x + 1,
                                   rect.max_y - rect.min_y + 1),
                             color);
            marked += (rect.max_x - rect.min_x + 1) * (rect.max_y - rect.min_y + 1);
            any = true;
        } else {
            std::vector<std::uint8_t> tile_seen;
            if (zone) {
                tile_seen.assign(static_cast<std::size_t>(openkal::map::kTileCount) *
                                     openkal::map::kTileCount,
                                 0);
            }
            for (const openkal::area_editor::ObjectMask& mask : area->bake.object_masks) {
                const std::vector<std::uint32_t> cells =
                    openkal::area_editor::clip_footprint_to_rect(
                        mask, rect.min_x, rect.min_y, rect.max_x, rect.max_y);
                for (const std::uint32_t cell : cells) {
                    const int x = static_cast<int>(
                        cell % static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
                    const int y = static_cast<int>(
                        cell / static_cast<std::uint32_t>(openkal::area_editor::kOverlaySize));
                    if (zone) {
                        const int tile_x = x / openkal::map::AreaCell::kSubcellsPerTile;
                        const int tile_y = y / openkal::map::AreaCell::kSubcellsPerTile;
                        const std::size_t flat =
                            static_cast<std::size_t>(tile_y) * openkal::map::kTileCount +
                            static_cast<std::size_t>(tile_x);
                        if (tile_seen[flat] != 0) {
                            continue;
                        }
                        tile_seen[flat] = 1;
                        for (int offset_y = 0; offset_y < openkal::map::AreaCell::kSubcellsPerTile;
                             ++offset_y) {
                            for (int offset_x = 0;
                                 offset_x < openkal::map::AreaCell::kSubcellsPerTile;
                                 ++offset_x) {
                                mark_preview_cell(
                                    image,
                                    tile_x * openkal::map::AreaCell::kSubcellsPerTile + offset_x,
                                    tile_y * openkal::map::AreaCell::kSubcellsPerTile + offset_y,
                                    color);
                            }
                        }
                        ++marked;
                    } else {
                        mark_preview_cell(image, x, y, color);
                        ++marked;
                    }
                    any = true;
                }
            }
        }
        if (any) {
            m_canvas->set_tile_preview(area->patch_x, area->patch_y, std::move(image));
            ++patches;
        }
    }
    const QString preview_kind = fit ? "Contour preview" : "Rectangle preview";
    const QString unit = zone && fit ? "tiles" : "subcells";
    QString message = QString("%1: %2 %3 in %4 loaded areas")
                          .arg(preview_kind)
                          .arg(marked)
                          .arg(unit)
                          .arg(patches);
    if (zone) {
        message += " — each touched tile is filled completely";
    }
    message += layer_index >= 0 ? " — Enter applies" : " — select a layer, then Enter";
    statusBar()->showMessage(message);
    refresh_window_state();
}

void AreaEditorWindow::apply_selection() {
    const AreaCanvas::WorldSelection selected = m_canvas->selection();
    if (!selected.present) {
        statusBar()->showMessage("Drag a rectangle before applying");
        return;
    }
    if (!m_layer_armed) {
        statusBar()->showMessage("Select a layer before applying the selection");
        return;
    }
    LayerId layer = LayerId::Blocked;
    for (int index = 0; index < static_cast<int>(LayerId::Count); ++index) {
        if (m_layer_radios[static_cast<std::size_t>(index)]->isChecked()) {
            layer = static_cast<LayerId>(index);
            break;
        }
    }
    const bool zone = !openkal::area_editor::layer_uses_subcells(layer);
    const bool fit = m_fit_radio != nullptr && m_fit_radio->isChecked();
    const int patch_min_x = std::clamp(
        static_cast<int>(std::floor(selected.min_x / openkal::area_editor::kOverlaySize)),
        0,
        openkal::map::kPatchGrid - 1);
    const int patch_min_y = std::clamp(
        static_cast<int>(std::floor(selected.min_y / openkal::area_editor::kOverlaySize)),
        0,
        openkal::map::kPatchGrid - 1);
    const int patch_max_x =
        std::clamp(static_cast<int>(
                       std::floor((selected.max_x - 1.0e-6) / openkal::area_editor::kOverlaySize)),
                   0,
                   openkal::map::kPatchGrid - 1);
    const int patch_max_y =
        std::clamp(static_cast<int>(
                       std::floor((selected.max_y - 1.0e-6) / openkal::area_editor::kOverlaySize)),
                   0,
                   openkal::map::kPatchGrid - 1);

    int areas_changed = 0;
    int missing = 0;
    std::size_t changed = 0;
    for (int patch_y = patch_min_y; patch_y <= patch_max_y; ++patch_y) {
        for (int patch_x = patch_min_x; patch_x <= patch_max_x; ++patch_x) {
            LocalRect rect = local_selection_rect(patch_x, patch_y, selected);
            if (!rect.valid) {
                continue;
            }
            const auto found = m_loaded_areas.find(area_key(patch_x, patch_y));
            if (found == m_loaded_areas.end()) {
                ++missing;
                continue;
            }
            std::vector<std::uint32_t> cells;
            if (fit) {
                for (const openkal::area_editor::ObjectMask& mask :
                     found->second->bake.object_masks) {
                    std::vector<std::uint32_t> clipped =
                        openkal::area_editor::clip_footprint_to_rect(
                            mask, rect.min_x, rect.min_y, rect.max_x, rect.max_y);
                    cells.insert(cells.end(), clipped.begin(), clipped.end());
                }
            } else if (zone) {
                expand_rect_to_tiles(rect);
                for (int tile_y = rect.min_y; tile_y <= rect.max_y;
                     tile_y += openkal::map::AreaCell::kSubcellsPerTile) {
                    for (int tile_x = rect.min_x; tile_x <= rect.max_x;
                         tile_x += openkal::map::AreaCell::kSubcellsPerTile) {
                        cells.push_back(static_cast<std::uint32_t>(
                            tile_y * openkal::area_editor::kOverlaySize + tile_x));
                    }
                }
            } else {
                cells = openkal::area_editor::subcells_in_rect(
                    rect.min_x, rect.min_y, rect.max_x, rect.max_y);
            }
            if (cells.empty()) {
                continue;
            }
            found->second->editor->set_active(layer);
            const std::size_t wrote =
                found->second->editor->paint_subcell_mask(cells, m_erase_tool);
            if (wrote != 0) {
                refresh_overlay(*found->second);
                changed += wrote;
                ++areas_changed;
            }
        }
    }
    refresh_window_state();
    const QString unit = zone ? "tiles" : "subcells";
    QString message = QString("%1 %2 %3 in %4 area(s)")
                          .arg(m_erase_tool ? "Erased" : "Painted")
                          .arg(changed)
                          .arg(unit)
                          .arg(areas_changed);
    if (zone) {
        message += " — each touched tile was filled completely";
    }
    if (missing > 0) {
        message += QString(" — %1 areas were not loaded").arg(missing);
    }
    statusBar()->showMessage(message, 6000);
}

void AreaEditorWindow::show_cursor_info(
    bool valid, int patch_x, int patch_y, int subcell_x, int subcell_y) {
    if (!valid || m_canvas->tool() != AreaCanvas::Tool::Navigate) {
        return;
    }
    const int tile_x = subcell_x / openkal::map::AreaCell::kSubcellsPerTile;
    const int tile_y = subcell_y / openkal::map::AreaCell::kSubcellsPerTile;
    QStringList layers;
    QString object_name = "none";
    const auto found = m_loaded_areas.find(area_key(patch_x, patch_y));
    if (found == m_loaded_areas.end()) {
        object_name = "area not loaded";
    } else {
        const openkal::map::AreaCell& grid = found->second->editor->grid();
        if (grid.blocked_at_subcell(subcell_x, subcell_y)) {
            layers << "Blocked";
        }
        if (grid.not_moveable_at_subcell(subcell_x, subcell_y)) {
            layers << "NotMoveable";
        }
        const std::uint32_t attr = grid.at(tile_x, tile_y);
        for (int index = static_cast<int>(LayerId::Village);
             index < static_cast<int>(LayerId::Count);
             ++index) {
            const auto& info = openkal::area_editor::layer_info(static_cast<LayerId>(index));
            if ((attr & info.paint_set) != 0) {
                layers << QString::fromUtf8(info.name);
            }
        }
        if (const openkal::area_editor::ObjectMask* mask = openkal::area_editor::pick_object_mask(
                found->second->bake.object_masks, subcell_x, subcell_y)) {
            object_name = QString::fromStdString(mask->name);
        }
    }
    statusBar()->showMessage(
        QString("Patch %1,%2   tile %3,%4   subcell %5,%6   layers: %7   object: %8")
            .arg(patch_x)
            .arg(patch_y)
            .arg(tile_x)
            .arg(tile_y)
            .arg(subcell_x)
            .arg(subcell_y)
            .arg(layers.isEmpty() ? "none" : layers.join(", "))
            .arg(object_name));
}

void AreaEditorWindow::refresh_overlay(LoadedArea& area) {
    m_canvas->set_tile_overlay(area.patch_x, area.patch_y, overlay_image(*area.editor));
}

void AreaEditorWindow::refresh_all_overlays() {
    for (auto& [key, area] : m_loaded_areas) {
        (void) key;
        refresh_overlay(*area);
    }
}

void AreaEditorWindow::refresh_window_state() {
    const bool loaded = active_area() != nullptr;
    int dirty_count = 0;
    for (const auto& [key, area] : m_loaded_areas) {
        (void) key;
        dirty_count += area->editor->dirty() ? 1 : 0;
    }
    m_save_action->setEnabled(dirty_count > 0);
    m_save_action->setText(dirty_count > 1 ? QString("Save %1 Areas").arg(dirty_count) : "Save");
    m_save_as_action->setEnabled(loaded);
    m_reload_action->setEnabled(loaded);
    m_rebake_action->setEnabled(loaded);
    m_undo_action->setEnabled(loaded);
    m_clear_action->setEnabled(loaded && m_layer_armed);
    if (m_apply_button != nullptr) {
        m_apply_button->setEnabled(m_canvas->selection().present);
    }
    QString title = "OpenKal Area Editor";
    if (const LoadedArea* area = active_area()) {
        title +=
            QString(" — %1").arg(QFileInfo(QString::fromStdString(area->source_path)).fileName());
    }
    if (dirty_count > 0) {
        title += QString(" * (%1 changed)").arg(dirty_count);
    }
    setWindowTitle(title);
}

void AreaEditorWindow::clear_loaded_areas() {
    m_loaded_areas.clear();
    m_load_queue.clear();
    m_queued_areas.clear();
    m_area_paths.clear();
    m_has_active_area = false;
    m_load_scheduled = false;
    m_visible_max_x = -1;
    m_visible_max_y = -1;
    m_preview_matches = false;
    m_object_preview_mask = nullptr;
    m_object_highlight_visible = false;
    m_hover_valid = false;
    m_canvas->clear_tiles();
    refresh_window_state();
}

void AreaEditorWindow::select_active_area_in_list() {
    const QSignalBlocker blocker(m_area_list);
    m_area_list->clearSelection();
    const LoadedArea* area = active_area();
    if (!area) {
        return;
    }
    for (int row = 0; row < m_area_list->count(); ++row) {
        QListWidgetItem* item = m_area_list->item(row);
        if (item->data(Qt::UserRole).toString().toStdString() == area->source_path) {
            m_area_list->setCurrentItem(item);
            m_area_list->scrollToItem(item);
            return;
        }
    }
}

bool AreaEditorWindow::confirm_unsaved() {
    int dirty_count = 0;
    for (const auto& [key, area] : m_loaded_areas) {
        (void) key;
        dirty_count += area->editor->dirty() ? 1 : 0;
    }
    if (dirty_count == 0) {
        return true;
    }
    const auto answer = QMessageBox::question(
        this,
        "Unsaved changes",
        QString("Save changes in %1 area(s) before continuing?").arg(dirty_count),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        save_areas();
        for (const auto& [key, area] : m_loaded_areas) {
            (void) key;
            if (area->editor->dirty()) {
                return false;
            }
        }
        return true;
    }
    return answer == QMessageBox::Discard;
}

void AreaEditorWindow::view_changed(int min_x, int min_y, int max_x, int max_y) {
    m_visible_min_x = min_x;
    m_visible_min_y = min_y;
    m_visible_max_x = max_x;
    m_visible_max_y = max_y;
    schedule_visible_areas();
}

void AreaEditorWindow::schedule_visible_areas() {
    if (m_visible_max_x < m_visible_min_x || m_visible_max_y < m_visible_min_y) {
        return;
    }
    struct Candidate {
        std::uint64_t key = 0;
        int distance = 0;
    };
    std::vector<Candidate> candidates;
    const int center_x = (m_visible_min_x + m_visible_max_x) / 2;
    const int center_y = (m_visible_min_y + m_visible_max_y) / 2;
    for (const auto& [key, path] : m_area_paths) {
        (void) path;
        const int patch_x = static_cast<int>(key >> 32);
        const int patch_y = static_cast<int>(key & 0xffffffffu);
        if (patch_x < m_visible_min_x || patch_x > m_visible_max_x || patch_y < m_visible_min_y ||
            patch_y > m_visible_max_y) {
            continue;
        }
        candidates.push_back({key, std::abs(patch_x - center_x) + std::abs(patch_y - center_y)});
    }
    std::sort(
        candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
            return left.distance < right.distance;
        });

    m_load_queue.clear();
    m_queued_areas.clear();
    const std::size_t count = std::min(kMaxResidentAreas, candidates.size());
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t key = candidates[index].key;
        if (m_loaded_areas.find(key) == m_loaded_areas.end()) {
            m_load_queue.push_back(key);
            m_queued_areas.insert(key);
        }
    }
    trim_area_cache();
    if (!m_load_queue.empty() && !m_load_scheduled) {
        m_load_scheduled = true;
        QTimer::singleShot(0, this, [this]() { process_load_queue(); });
    }
}

void AreaEditorWindow::process_load_queue() {
    m_load_scheduled = false;
    while (!m_load_queue.empty()) {
        const std::uint64_t key = m_load_queue.front();
        m_load_queue.pop_front();
        m_queued_areas.erase(key);
        if (m_loaded_areas.find(key) != m_loaded_areas.end()) {
            continue;
        }
        const auto path = m_area_paths.find(key);
        if (path == m_area_paths.end()) {
            continue;
        }
        const int patch_x = static_cast<int>(key >> 32);
        const int patch_y = static_cast<int>(key & 0xffffffffu);
        if (patch_x < m_visible_min_x || patch_x > m_visible_max_x || patch_y < m_visible_min_y ||
            patch_y > m_visible_max_y) {
            continue;
        }
        const int preview_resolution =
            std::min(m_options.bake_resolution, kStreamPreviewResolution);
        (void) load_area_document(path->second, false, preview_resolution);
        break;
    }
    trim_area_cache();
    if (!m_load_queue.empty()) {
        m_load_scheduled = true;
        QTimer::singleShot(25, this, [this]() { process_load_queue(); });
    }
}

void AreaEditorWindow::trim_area_cache() {
    for (auto it = m_loaded_areas.begin(); it != m_loaded_areas.end();) {
        LoadedArea& area = *it->second;
        const bool visible = area.patch_x >= m_visible_min_x - kCacheRetentionMargin &&
                             area.patch_x <= m_visible_max_x + kCacheRetentionMargin &&
                             area.patch_y >= m_visible_min_y - kCacheRetentionMargin &&
                             area.patch_y <= m_visible_max_y + kCacheRetentionMargin;
        const bool active = m_has_active_area && it->first == m_active_area_key;
        if (!visible && !active && !area.editor->dirty()) {
            m_canvas->remove_tile(area.patch_x, area.patch_y);
            it = m_loaded_areas.erase(it);
        } else {
            ++it;
        }
    }

    while (m_loaded_areas.size() > kMaxResidentAreas) {
        auto victim = m_loaded_areas.end();
        int best_distance = -1;
        const int center_x = (m_visible_min_x + m_visible_max_x) / 2;
        const int center_y = (m_visible_min_y + m_visible_max_y) / 2;
        for (auto it = m_loaded_areas.begin(); it != m_loaded_areas.end(); ++it) {
            const LoadedArea& area = *it->second;
            if ((m_has_active_area && it->first == m_active_area_key) || area.editor->dirty()) {
                continue;
            }
            const int distance =
                std::abs(area.patch_x - center_x) + std::abs(area.patch_y - center_y);
            if (distance > best_distance) {
                best_distance = distance;
                victim = it;
            }
        }
        if (victim == m_loaded_areas.end()) {
            break;
        }
        m_canvas->remove_tile(victim->second->patch_x, victim->second->patch_y);
        m_loaded_areas.erase(victim);
    }
    refresh_window_state();
}

void AreaEditorWindow::set_active_area(std::uint64_t key, bool center) {
    const auto found = m_loaded_areas.find(key);
    if (found == m_loaded_areas.end()) {
        return;
    }
    m_active_area_key = key;
    m_has_active_area = true;
    m_canvas->set_active_patch(found->second->patch_x, found->second->patch_y, center);
    select_active_area_in_list();
    refresh_window_state();
}

AreaEditorWindow::LoadedArea* AreaEditorWindow::active_area() {
    if (!m_has_active_area) {
        return nullptr;
    }
    const auto found = m_loaded_areas.find(m_active_area_key);
    return found == m_loaded_areas.end() ? nullptr : found->second.get();
}

const AreaEditorWindow::LoadedArea* AreaEditorWindow::active_area() const {
    if (!m_has_active_area) {
        return nullptr;
    }
    const auto found = m_loaded_areas.find(m_active_area_key);
    return found == m_loaded_areas.end() ? nullptr : found->second.get();
}

std::string AreaEditorWindow::resolve_area_path(const std::string& path) const {
    if (path.empty() || openkal::platform::file_exists(path) || m_area_directory.empty() ||
        std::filesystem::path(path).is_absolute()) {
        return path;
    }
    return openkal::platform::join_path(m_area_directory, path);
}

std::uint64_t AreaEditorWindow::area_key(int patch_x, int patch_y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(patch_x)) << 32) |
           static_cast<std::uint32_t>(patch_y);
}
