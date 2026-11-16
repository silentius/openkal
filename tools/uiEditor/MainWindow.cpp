/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MainWindow.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

    constexpr const char* kPaletteTypes[] = {"text",
                                             "button",
                                             "icon_slot",
                                             "icon_grid",
                                             "tabs",
                                             "progress",
                                             "image",
                                             "fill",
                                             "border",
                                             "input",
                                             "list",
                                             "canvas"};

    constexpr const char* kPresets[] = {"info_menu",
                                        "info_character",
                                        "info_inventory",
                                        "info_mix",
                                        "dragon_dialog",
                                        "dragon_compact"};

    QSpinBox* make_spin(QWidget* parent) {
        auto* s = new QSpinBox(parent);
        s->setRange(-4096, 4096);
        s->setSingleStep(1);
        return s;
    }

    QDoubleSpinBox* make_dspin(QWidget* parent) {
        auto* s = new QDoubleSpinBox(parent);
        s->setRange(0.0, 512.0);
        s->setDecimals(1);
        s->setSingleStep(0.1);
        return s;
    }

}  // namespace

MainWindow::MainWindow(std::string repo_root,
                       std::string configuration_root,
                       std::string data_root,
                       QWidget* parent)
    : QMainWindow(parent), m_repo_root(std::move(repo_root)),
      m_configuration_root(std::move(configuration_root)), m_data_root(std::move(data_root)) {
    m_bmps.set_search_dirs({
        openkal::platform::join_path(OPENKAL_RUNTIME_DATA_ROOT, "UI"),
        openkal::platform::join_path(OPENKAL_RUNTIME_DATA_ROOT, "HyperText"),
        openkal::platform::join_path(m_data_root, "UI"),
        openkal::platform::join_path(m_data_root, "HyperText"),
    });
    setup_ui();
    reload_catalog();
    m_doc.set_id("untitled");
    m_doc.set_kind(openkal::client::UiKind::Window);
    openkal::client::ui_apply_chrome_preset(m_doc, "info_character");
    m_canvas->set_document(&m_doc);
    refresh_tree();
    rebuild_inspector();
    setWindowTitle(tr("ui_editor — untitled"));
}

void MainWindow::setup_ui() {
    auto* file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(tr("&New…"), QKeySequence::New, this, &MainWindow::new_file);
    file_menu->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::open_file);
    file_menu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::save_file);
    file_menu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::save_file_as);

    auto* view_menu = menuBar()->addMenu(tr("&View"));
    auto* snap = view_menu->addAction(tr("Snap to 32 px grid"));
    snap->setCheckable(true);
    connect(snap, &QAction::toggled, this, &MainWindow::toggle_grid_snap);

    auto* edit_menu = menuBar()->addMenu(tr("&Edit"));
    edit_menu->addAction(
        tr("Delete widget"), QKeySequence::Delete, this, &MainWindow::delete_selected);
    edit_menu->addAction(tr("Move up (back)"), this, &MainWindow::move_selected_up);
    edit_menu->addAction(tr("Move down (front)"), this, &MainWindow::move_selected_down);

    m_canvas = new UiCanvas(m_bmps, this);
    setCentralWidget(m_canvas);
    connect(m_canvas, &UiCanvas::selection_changed, this, &MainWindow::on_canvas_selection);
    connect(m_canvas, &UiCanvas::geometry_changed, this, [this]() {
        mark_dirty();
        rebuild_inspector();
    });

    auto* file_dock = new QDockWidget(tr("Document"), this);
    auto* file_box = new QWidget(file_dock);
    auto* file_form = new QFormLayout(file_box);
    m_win_w_spin = make_spin(file_box);
    m_win_h_spin = make_spin(file_box);
    m_preset_combo = new QComboBox(file_box);
    for (const char* preset : kPresets) {
        m_preset_combo->addItem(QString::fromUtf8(preset));
    }
    m_close_button_check = new QCheckBox(tr("Enabled"), file_box);
    m_edges_combo = new QComboBox(file_box);
    m_edges_combo->addItem(tr("tile"), QStringLiteral("tile"));
    m_edges_combo->addItem(tr("stretch"), QStringLiteral("stretch"));
    m_edges_combo->addItem(tr("repeat"), QStringLiteral("repeat"));
    m_pad_l_spin = make_dspin(file_box);
    m_pad_t_spin = make_dspin(file_box);
    m_pad_r_spin = make_dspin(file_box);
    m_pad_b_spin = make_dspin(file_box);
    m_inventory_preview = new QComboBox(file_box);
    m_inventory_preview->addItem(tr("General"), QStringLiteral("normal"));
    m_inventory_preview->addItem(tr("Other"), QStringLiteral("quest"));
    m_inventory_preview->setCurrentIndex(0);
    file_form->addRow(tr("Inventory preview"), m_inventory_preview);
    connect(m_inventory_preview, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_filling) {
            reload_catalog();
        }
    });
    file_form->addRow(tr("Client W"), m_win_w_spin);
    file_form->addRow(tr("Client H"), m_win_h_spin);
    file_form->addRow(tr("Chrome preset"), m_preset_combo);
    file_form->addRow(tr("Built-in close button"), m_close_button_check);
    file_form->addRow(tr("Chrome edges"), m_edges_combo);
    file_form->addRow(tr("Pad left (TL X)"), m_pad_l_spin);
    file_form->addRow(tr("Pad top (TL Y)"), m_pad_t_spin);
    file_form->addRow(tr("Pad right"), m_pad_r_spin);
    file_form->addRow(tr("Pad bottom"), m_pad_b_spin);
    connect(m_win_w_spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_filling) {
            return;
        }
        m_doc.set_window_w(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    connect(m_win_h_spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_filling) {
            return;
        }
        m_doc.set_window_h(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    connect(m_preset_combo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_filling) {
            return;
        }
        openkal::client::ui_apply_chrome_preset(m_doc, text.toStdString());
        m_canvas->update();
        mark_dirty();
        rebuild_inspector();
    });
    connect(m_edges_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_filling) {
            return;
        }
        m_doc.set_chrome_edges(index == 2   ? openkal::client::UiChromeEdges::Repeat
                               : index == 1 ? openkal::client::UiChromeEdges::Stretch
                                            : openkal::client::UiChromeEdges::Tile);
        m_canvas->update();
        mark_dirty();
    });
    connect(m_close_button_check, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_filling) {
            return;
        }
        m_doc.set_chrome_close_button(enabled);
        m_canvas->update();
        mark_dirty();
    });
    connect(m_pad_l_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_filling) {
            return;
        }
        m_doc.set_pad_left(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    connect(m_pad_t_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_filling) {
            return;
        }
        m_doc.set_pad_top(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    connect(m_pad_r_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_filling) {
            return;
        }
        m_doc.set_pad_right(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    connect(m_pad_b_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_filling) {
            return;
        }
        m_doc.set_pad_bottom(static_cast<float>(v));
        m_canvas->update();
        mark_dirty();
    });
    file_dock->setWidget(file_box);
    addDockWidget(Qt::LeftDockWidgetArea, file_dock);

    auto* tree_dock = new QDockWidget(tr("Widgets"), this);
    auto* tree_box = new QWidget(tree_dock);
    auto* tree_layout = new QVBoxLayout(tree_box);
    m_tree = new QTreeWidget(tree_box);
    m_tree->setHeaderLabels({tr("Id"), tr("Type")});
    m_tree->setRootIsDecorated(false);
    connect(m_tree,
            &QTreeWidget::currentItemChanged,
            this,
            [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
                if (!item || m_filling) {
                    return;
                }
                const int index = item->data(0, Qt::UserRole).toInt();
                m_canvas->set_selected_index(index);
                rebuild_inspector();
            });
    auto* tree_btns = new QHBoxLayout();
    auto* up = new QPushButton(tr("Up"), tree_box);
    auto* down = new QPushButton(tr("Down"), tree_box);
    auto* del = new QPushButton(tr("Delete"), tree_box);
    connect(up, &QPushButton::clicked, this, &MainWindow::move_selected_up);
    connect(down, &QPushButton::clicked, this, &MainWindow::move_selected_down);
    connect(del, &QPushButton::clicked, this, &MainWindow::delete_selected);
    tree_btns->addWidget(up);
    tree_btns->addWidget(down);
    tree_btns->addWidget(del);
    tree_layout->addWidget(m_tree);
    tree_layout->addLayout(tree_btns);
    tree_dock->setWidget(tree_box);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);

    auto* pal_dock = new QDockWidget(tr("Palette"), this);
    m_palette = new QListWidget(pal_dock);
    for (const char* t : kPaletteTypes) {
        m_palette->addItem(QString::fromUtf8(t));
    }
    connect(m_palette, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        add_from_palette();
    });
    pal_dock->setWidget(m_palette);
    addDockWidget(Qt::RightDockWidgetArea, pal_dock);

    auto* insp_dock = new QDockWidget(tr("Inspector"), this);
    auto* insp_box = new QWidget(insp_dock);
    m_form = new QFormLayout(insp_box);
    m_id_edit = new QLineEdit(insp_box);
    m_type_edit = new QLineEdit(insp_box);
    m_type_edit->setReadOnly(true);
    m_x_spin = make_spin(insp_box);
    m_y_spin = make_spin(insp_box);
    m_w_spin = make_spin(insp_box);
    m_h_spin = make_spin(insp_box);
    m_label_ox_spin = make_spin(insp_box);
    m_label_oy_spin = make_spin(insp_box);
    m_text_edit = new QLineEdit(insp_box);
    m_align_combo = new QComboBox(insp_box);
    m_align_combo->addItem(tr("left"), QStringLiteral("left"));
    m_align_combo->addItem(tr("center"), QStringLiteral("center"));
    m_align_combo->addItem(tr("right"), QStringLiteral("right"));
    m_var_combo = new QComboBox(insp_box);
    m_var_combo->setEditable(true);
    m_color_edit = new QLineEdit(insp_box);
    m_file_edit = new QLineEdit(insp_box);
    m_action_edit = new QLineEdit(insp_box);
    m_bind_edit = new QLineEdit(insp_box);
    m_viewport_edit = new QLineEdit(insp_box);
    m_scroll_bind_edit = new QLineEdit(insp_box);
    m_fill_mode_combo = new QComboBox(insp_box);
    m_fill_mode_combo->addItems({"lcr", "crop"});
    m_direction_combo = new QComboBox(insp_box);
    m_direction_combo->addItems({"left_to_right", "bottom_to_top"});
    m_shape_combo = new QComboBox(insp_box);
    m_shape_combo->addItems({"rect", "circle"});
    m_visible_when_edit = new QLineEdit(insp_box);
    m_pending_when_edit = new QLineEdit(insp_box);
    m_visible_check = new QCheckBox(tr("Visible"), insp_box);
    m_multiline_check = new QCheckBox(tr("Wrap multiple lines"), insp_box);
    m_close_ox_spin = make_dspin(insp_box);
    m_close_oy_spin = make_dspin(insp_box);
    m_close_w_spin = make_dspin(insp_box);
    m_close_h_spin = make_dspin(insp_box);
    m_close_ox_spin->setRange(-512.0, 512.0);
    m_close_oy_spin->setRange(-512.0, 512.0);
    m_form->addRow(tr("Id"), m_id_edit);
    m_form->addRow(tr("Type"), m_type_edit);
    m_form->addRow(tr("X"), m_x_spin);
    m_form->addRow(tr("Y"), m_y_spin);
    m_form->addRow(tr("W"), m_w_spin);
    m_form->addRow(tr("H"), m_h_spin);
    m_form->addRow(tr("Close ox"), m_close_ox_spin);
    m_form->addRow(tr("Close oy"), m_close_oy_spin);
    m_form->addRow(tr("Close w"), m_close_w_spin);
    m_form->addRow(tr("Close h"), m_close_h_spin);
    m_form->addRow(tr("Label ox"), m_label_ox_spin);
    m_form->addRow(tr("Label oy"), m_label_oy_spin);
    m_form->addRow(tr("Text"), m_text_edit);
    m_form->addRow(tr("Align"), m_align_combo);
    m_form->addRow(tr("Multiline input"), m_multiline_check);
    m_form->addRow(tr("${…}"), m_var_combo);
    m_form->addRow(tr("Color"), m_color_edit);
    m_form->addRow(tr("File"), m_file_edit);
    m_form->addRow(tr("Action"), m_action_edit);
    m_form->addRow(tr("Bind"), m_bind_edit);
    m_form->addRow(tr("Viewport"), m_viewport_edit);
    m_form->addRow(tr("Scroll bind"), m_scroll_bind_edit);
    m_form->addRow(tr("Progress mode"), m_fill_mode_combo);
    m_form->addRow(tr("Fill direction"), m_direction_combo);
    m_form->addRow(tr("Canvas shape"), m_shape_combo);
    m_form->addRow(tr("visible_when"), m_visible_when_edit);
    m_form->addRow(tr("pending_when"), m_pending_when_edit);
    m_form->addRow(m_visible_check);
    auto apply_fn = [this]() { apply_inspector(); };
    connect(m_id_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_text_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(
        m_align_combo, &QComboBox::currentIndexChanged, this, [this](int) { apply_inspector(); });
    connect(m_color_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_file_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_action_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_bind_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_viewport_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_scroll_bind_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_fill_mode_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        apply_inspector();
    });
    connect(m_direction_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        apply_inspector();
    });
    connect(
        m_shape_combo, &QComboBox::currentIndexChanged, this, [this](int) { apply_inspector(); });
    connect(m_visible_when_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_pending_when_edit, &QLineEdit::editingFinished, this, apply_fn);
    connect(m_visible_check, &QCheckBox::toggled, this, apply_fn);
    connect(m_multiline_check, &QCheckBox::toggled, this, apply_fn);
    connect(m_x_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_y_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_w_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_h_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_label_ox_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_label_oy_spin, qOverload<int>(&QSpinBox::valueChanged), this, apply_fn);
    connect(m_close_ox_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, apply_fn);
    connect(m_close_oy_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, apply_fn);
    connect(m_close_w_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, apply_fn);
    connect(m_close_h_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, apply_fn);
    connect(m_var_combo, &QComboBox::activated, this, [this](int) {
        const QString key = m_var_combo->currentText();
        if (key.isEmpty() || m_filling) {
            return;
        }
        const QString token = QStringLiteral("${%1}").arg(key);
        m_text_edit->insert(token);
        apply_inspector();
    });
    insp_dock->setWidget(insp_box);
    addDockWidget(Qt::RightDockWidgetArea, insp_dock);

    statusBar()->showMessage(
        tr("Ctrl+wheel zoom · drag the top-left chrome bitmap to move it · arrows nudge pad"));
}

void MainWindow::reload_catalog() {
    const std::string vars_path = openkal::platform::join_path(m_repo_root, "docs/ui/VARIABLES.md");
    if (const auto bytes = openkal::platform::read_file(vars_path)) {
        const std::string text(bytes->begin(), bytes->end());
        m_var_keys = openkal::client::ui_parse_variable_keys(text);
    }
    m_var_combo->clear();
    m_var_combo->addItem(QString());
    for (const auto& key : m_var_keys) {
        m_var_combo->addItem(QString::fromStdString(key));
    }
    openkal::client::UiVarMap vars;
    openkal::client::ui_fill_theme_vars(m_doc, vars);
    openkal::client::ui_fill_dummy_preview(vars);
    if (m_doc.id() == "character_info") {
        vars["ui.title"] = "Character Info";
        vars["ui.ok"] = "Apply";
    }
    m_inventory_preview->setEnabled(m_doc.id() == "inventory");
    if (m_doc.id() == "inventory") {
        const std::string tab = m_inventory_preview->currentData().toString().toStdString();
        vars["ui.title"] = "Inventory";
        vars["inventory.tab"] = tab;
        vars["inventory.normal_active"] = tab == "normal" ? "true" : "false";
        vars["inventory.quest_active"] = tab == "quest" ? "true" : "false";
        vars["inventory.bag_active"] = "true";
        for (int page = 1; page <= 6; ++page) {
            vars["inventory.page" + std::to_string(page) + "_shaded"] =
                tab == "normal" && page > 2 ? "true" : "false";
        }
    }
    m_canvas->set_vars(std::move(vars));
}

void MainWindow::refresh_tree() {
    m_filling = true;
    m_tree->clear();
    auto* chrome =
        new QTreeWidgetItem(m_tree, {tr("chrome"), QString::fromStdString(m_doc.chrome_preset())});
    chrome->setData(0, Qt::UserRole, -1);
    int i = 0;
    for (const auto& w : m_doc.widgets()) {
        const QString id = w.id().empty() ? tr("(unnamed)") : QString::fromStdString(w.id());
        auto* item = new QTreeWidgetItem(
            m_tree, {id, QString::fromUtf8(openkal::client::ui_widget_type_name(w.type()))});
        item->setData(0, Qt::UserRole, i);
        if (!w.visible()) {
            item->setForeground(0, QBrush(QColor(140, 140, 140)));
        }
        ++i;
    }
    const int sel = m_canvas->selected_index();
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        auto* item = m_tree->topLevelItem(row);
        if (item->data(0, Qt::UserRole).toInt() == sel) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
    m_filling = false;
}

void MainWindow::rebuild_inspector() {
    m_filling = true;
    m_win_w_spin->setValue(static_cast<int>(std::lround(m_doc.window_w())));
    m_win_h_spin->setValue(static_cast<int>(std::lround(m_doc.window_h())));
    const int preset_i = m_preset_combo->findText(QString::fromStdString(m_doc.chrome_preset()));
    if (preset_i >= 0) {
        m_preset_combo->setCurrentIndex(preset_i);
    }
    const bool framed = m_doc.kind() != openkal::client::UiKind::Hud;
    const bool built_in_info_menu = framed && m_doc.chrome_preset() == "info_menu";
    m_close_button_check->setEnabled(built_in_info_menu);
    m_close_button_check->setChecked(m_doc.chrome_close_button());
    m_edges_combo->setEnabled(framed);
    m_pad_l_spin->setEnabled(framed);
    m_pad_t_spin->setEnabled(framed);
    m_pad_r_spin->setEnabled(framed);
    m_pad_b_spin->setEnabled(framed);
    m_edges_combo->setCurrentIndex(
        m_doc.chrome_edges() == openkal::client::UiChromeEdges::Repeat    ? 2
        : m_doc.chrome_edges() == openkal::client::UiChromeEdges::Stretch ? 1
                                                                          : 0);
    m_pad_l_spin->setValue(static_cast<double>(m_doc.pad_left()));
    m_pad_t_spin->setValue(static_cast<double>(m_doc.pad_top()));
    m_pad_r_spin->setValue(static_cast<double>(m_doc.pad_right()));
    m_pad_b_spin->setValue(static_cast<double>(m_doc.pad_bottom()));
    const int sel = m_canvas->selected_index();
    const bool widget = sel >= 0 && sel < static_cast<int>(m_doc.widgets().size());
    const bool chrome = sel == -1;
    m_id_edit->setEnabled(widget);
    m_x_spin->setEnabled(widget);
    m_y_spin->setEnabled(widget);
    m_w_spin->setEnabled(widget);
    m_h_spin->setEnabled(widget);
    m_label_ox_spin->setEnabled(false);
    m_label_oy_spin->setEnabled(false);
    m_text_edit->setEnabled(widget);
    m_align_combo->setEnabled(false);
    m_multiline_check->setEnabled(false);
    m_color_edit->setEnabled(widget);
    m_file_edit->setEnabled(widget);
    m_action_edit->setEnabled(widget);
    m_bind_edit->setEnabled(widget);
    m_viewport_edit->setEnabled(widget);
    m_scroll_bind_edit->setEnabled(widget);
    m_fill_mode_combo->setEnabled(false);
    m_direction_combo->setEnabled(false);
    m_shape_combo->setEnabled(false);
    m_visible_when_edit->setEnabled(widget);
    m_pending_when_edit->setEnabled(widget);
    m_visible_check->setEnabled(widget);
    m_form->setRowVisible(m_close_ox_spin, chrome && framed && !built_in_info_menu);
    m_form->setRowVisible(m_close_oy_spin, chrome && framed && !built_in_info_menu);
    m_form->setRowVisible(m_close_w_spin, chrome && framed && !built_in_info_menu);
    m_form->setRowVisible(m_close_h_spin, chrome && framed && !built_in_info_menu);
    if (chrome && framed && !built_in_info_menu) {
        m_close_ox_spin->setValue(static_cast<double>(m_doc.close_ox()));
        m_close_oy_spin->setValue(static_cast<double>(m_doc.close_oy()));
        m_close_w_spin->setValue(static_cast<double>(m_doc.close_w()));
        m_close_h_spin->setValue(static_cast<double>(m_doc.close_h()));
    }
    if (!widget) {
        m_id_edit->clear();
        m_type_edit->setText(chrome ? tr("chrome") : QString());
        m_filling = false;
        return;
    }
    const auto& w = m_doc.widgets()[static_cast<std::size_t>(sel)];
    const bool button = w.type() == openkal::client::UiWidgetType::Button;
    const bool aligned = button || w.type() == openkal::client::UiWidgetType::Text ||
                         w.type() == openkal::client::UiWidgetType::Input;
    m_label_ox_spin->setEnabled(button);
    m_label_oy_spin->setEnabled(button);
    m_align_combo->setEnabled(aligned);
    m_multiline_check->setEnabled(w.type() == openkal::client::UiWidgetType::Input);
    m_multiline_check->setChecked(w.multiline());
    m_id_edit->setText(QString::fromStdString(w.id()));
    m_type_edit->setText(QString::fromUtf8(openkal::client::ui_widget_type_name(w.type())));
    m_x_spin->setValue(static_cast<int>(
        std::lround(w.x_from_right() >= 0 ? m_doc.window_w() - w.x_from_right() : w.x())));
    m_y_spin->setValue(static_cast<int>(
        std::lround(w.y_from_bottom() >= 0 ? m_doc.window_h() - w.y_from_bottom() : w.y())));
    m_w_spin->setValue(static_cast<int>(std::lround(w.w())));
    m_h_spin->setValue(static_cast<int>(std::lround(w.h())));
    m_label_ox_spin->setValue(static_cast<int>(std::lround(w.label_ox())));
    m_label_oy_spin->setValue(static_cast<int>(std::lround(w.label_oy())));
    m_text_edit->setText(QString::fromStdString(w.text().empty() ? w.label() : w.text()));
    {
        const int align_i = m_align_combo->findData(QString::fromStdString(w.align()));
        m_align_combo->setCurrentIndex(align_i >= 0 ? align_i : 0);
    }
    m_color_edit->setText(QString::fromStdString(w.color()));
    m_file_edit->setText(QString::fromStdString(w.file().empty() ? w.file_default() : w.file()));
    m_action_edit->setText(QString::fromStdString(w.action()));
    m_bind_edit->setText(QString::fromStdString(w.bind()));
    m_viewport_edit->setText(QString::fromStdString(w.viewport()));
    m_scroll_bind_edit->setText(QString::fromStdString(w.scroll_bind()));
    m_fill_mode_combo->setEnabled(w.type() == openkal::client::UiWidgetType::Progress);
    m_direction_combo->setEnabled(w.type() == openkal::client::UiWidgetType::Progress);
    m_fill_mode_combo->setCurrentText(QString::fromStdString(w.fill_mode()));
    m_direction_combo->setCurrentText(QString::fromStdString(w.direction()));
    m_shape_combo->setEnabled(w.type() == openkal::client::UiWidgetType::Canvas &&
                              w.canvas_kind() == "minimap");
    m_shape_combo->setCurrentText(QString::fromStdString(w.shape()));
    m_visible_when_edit->setText(QString::fromStdString(w.visible_when()));
    m_pending_when_edit->setText(QString::fromStdString(w.pending_when()));
    m_visible_check->setChecked(w.visible());
    m_filling = false;
}

void MainWindow::apply_inspector() {
    if (m_filling) {
        return;
    }
    const int sel = m_canvas->selected_index();
    if (sel == -1) {
        m_doc.set_close_ox(static_cast<float>(m_close_ox_spin->value()));
        m_doc.set_close_oy(static_cast<float>(m_close_oy_spin->value()));
        m_doc.set_close_w(static_cast<float>(m_close_w_spin->value()));
        m_doc.set_close_h(static_cast<float>(m_close_h_spin->value()));
        mark_dirty();
        m_canvas->update();
        return;
    }
    if (sel < 0 || sel >= static_cast<int>(m_doc.widgets().size())) {
        return;
    }
    auto& w = m_doc.widgets()[static_cast<std::size_t>(sel)];
    w.set_id(m_id_edit->text().toStdString());
    w.set_x(static_cast<float>(m_x_spin->value()));
    w.set_y(static_cast<float>(m_y_spin->value()));
    if (w.x_from_right() >= 0) {
        w.set_x_from_right(m_doc.window_w() - w.x());
    }
    if (w.y_from_bottom() >= 0) {
        w.set_y_from_bottom(m_doc.window_h() - w.y());
    }
    w.set_w(static_cast<float>(m_w_spin->value()));
    w.set_h(static_cast<float>(m_h_spin->value()));
    if (w.type() == openkal::client::UiWidgetType::Button) {
        w.set_label(m_text_edit->text().toStdString());
        w.set_file_default(m_file_edit->text().toStdString());
        w.set_label_ox(static_cast<float>(m_label_ox_spin->value()));
        w.set_label_oy(static_cast<float>(m_label_oy_spin->value()));
    } else if (w.type() == openkal::client::UiWidgetType::Text) {
        w.set_text(m_text_edit->text().toStdString());
    } else {
        w.set_text(m_text_edit->text().toStdString());
        w.set_file(m_file_edit->text().toStdString());
    }
    if (w.type() == openkal::client::UiWidgetType::Button ||
        w.type() == openkal::client::UiWidgetType::Text ||
        w.type() == openkal::client::UiWidgetType::Input) {
        w.set_align(m_align_combo->currentData().toString().toStdString());
    }
    w.set_color(m_color_edit->text().toStdString());
    w.set_multiline(m_multiline_check->isChecked());
    w.set_action(m_action_edit->text().toStdString());
    w.set_bind(m_bind_edit->text().toStdString());
    w.set_viewport(m_viewport_edit->text().toStdString());
    w.set_scroll_bind(m_scroll_bind_edit->text().toStdString());
    if (w.type() == openkal::client::UiWidgetType::Progress) {
        w.set_fill_mode(m_fill_mode_combo->currentText().toStdString());
        w.set_direction(m_direction_combo->currentText().toStdString());
    }
    w.set_visible_when(m_visible_when_edit->text().toStdString());
    if (w.type() == openkal::client::UiWidgetType::Canvas && w.canvas_kind() == "minimap") {
        w.set_shape(m_shape_combo->currentText().toStdString());
    }
    w.set_pending_when(m_pending_when_edit->text().toStdString());
    w.set_visible(m_visible_check->isChecked());
    mark_dirty();
    m_canvas->update();
    refresh_tree();
}

void MainWindow::on_canvas_selection(int index) {
    (void) index;
    refresh_tree();
    rebuild_inspector();
}

void MainWindow::toggle_grid_snap(bool on) {
    m_canvas->set_snap(on ? 32 : 1);
    m_canvas->set_show_grid(on);
}

void MainWindow::mark_dirty() {
    m_dirty = true;
    const QString name = m_path.empty() ? tr("untitled") : QString::fromStdString(m_path);
    setWindowTitle(tr("ui_editor — %1*").arg(name));
}

bool MainWindow::confirm_discard() {
    if (!m_dirty) {
        return true;
    }
    const auto r =
        QMessageBox::question(this,
                              tr("Unsaved changes"),
                              tr("Save the current document?"),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) {
        return false;
    }
    if (r == QMessageBox::Save) {
        save_file();
        return !m_dirty;
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_discard()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::open_document(const std::string& path) {
    return load_path(path);
}

bool MainWindow::load_path(const std::string& path) {
    const auto bytes = openkal::platform::read_file(path);
    if (!bytes) {
        QMessageBox::warning(this, tr("Open"), tr("Could not read file."));
        return false;
    }
    const std::string text(bytes->begin(), bytes->end());
    openkal::platform::Json root;
    std::string err;
    if (!openkal::platform::Json::parse(text, root, &err)) {
        QMessageBox::warning(this, tr("Open"), QString::fromStdString(err));
        return false;
    }
    openkal::client::UiDocument doc;
    if (!doc.load(root)) {
        QMessageBox::warning(this, tr("Open"), tr("Not an openkal.ui/v1 document."));
        return false;
    }
    m_doc = std::move(doc);
    m_path = path;
    m_dirty = false;
    m_canvas->set_document(&m_doc);
    reload_catalog();
    refresh_tree();
    rebuild_inspector();
    setWindowTitle(tr("ui_editor — %1").arg(QString::fromStdString(path)));
    return true;
}

bool MainWindow::write_path(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    if (base == "_template.json") {
        QMessageBox::warning(this,
                             tr("Save"),
                             tr("The editor does not write _template.json; save as another name."));
        return false;
    }
    const std::string json = openkal::platform::Json::stringify(m_doc.to_json(), 2) + "\n";
    if (!openkal::platform::write_file(path, json)) {
        QMessageBox::warning(this, tr("Save"), tr("Could not write file."));
        return false;
    }
    m_path = path;
    m_dirty = false;
    setWindowTitle(tr("ui_editor — %1").arg(QString::fromStdString(path)));
    statusBar()->showMessage(tr("Saved %1").arg(QString::fromStdString(path)), 3000);
    return true;
}

void MainWindow::open_file() {
    if (!confirm_discard()) {
        return;
    }
    const QString start =
        QString::fromStdString(openkal::platform::join_path(m_configuration_root, "ui"));
    const QString picked =
        QFileDialog::getOpenFileName(this, tr("Open UI JSON"), start, tr("JSON (*.json)"));
    if (picked.isEmpty()) {
        return;
    }
    (void) load_path(picked.toStdString());
}

void MainWindow::save_file() {
    if (m_path.empty()) {
        save_file_as();
        return;
    }
    (void) write_path(m_path);
}

void MainWindow::save_file_as() {
    const QString start = QString::fromStdString(
        m_path.empty() ? openkal::platform::join_path(m_configuration_root, "ui") : m_path);
    const QString picked =
        QFileDialog::getSaveFileName(this, tr("Save UI JSON"), start, tr("JSON (*.json)"));
    if (picked.isEmpty()) {
        return;
    }
    (void) write_path(picked.toStdString());
}

void MainWindow::new_file() {
    if (!confirm_discard()) {
        return;
    }
    QStringList kinds{tr("window"), tr("hud")};
    bool ok = false;
    const QString kind =
        QInputDialog::getItem(this, tr("New UI"), tr("Kind"), kinds, 0, false, &ok);
    if (!ok) {
        return;
    }
    QStringList presets;
    for (const char* p : kPresets) {
        presets << QString::fromUtf8(p);
    }
    const QString preset =
        QInputDialog::getItem(this, tr("New UI"), tr("Chrome preset"), presets, 1, false, &ok);
    if (!ok) {
        return;
    }
    const QString id =
        QInputDialog::getText(this, tr("New UI"), tr("Id"), QLineEdit::Normal, tr("untitled"), &ok);
    if (!ok) {
        return;
    }
    m_doc = openkal::client::UiDocument{};
    m_doc.set_id(id.toStdString());
    m_doc.set_kind(kind == QStringLiteral("hud") ? openkal::client::UiKind::Hud
                                                 : openkal::client::UiKind::Window);
    openkal::client::ui_apply_chrome_preset(m_doc, preset.toStdString());
    m_path.clear();
    m_dirty = true;
    m_canvas->set_document(&m_doc);
    reload_catalog();
    refresh_tree();
    rebuild_inspector();
    setWindowTitle(tr("ui_editor — untitled*"));
}

void MainWindow::add_from_palette() {
    auto* item = m_palette->currentItem();
    if (!item) {
        return;
    }
    add_widget(item->text().toStdString());
}

std::string MainWindow::unique_id(const std::string& prefix) const {
    int n = 1;
    while (true) {
        const std::string id = prefix + "_" + std::to_string(n);
        if (!m_doc.find_widget(id)) {
            return id;
        }
        ++n;
    }
}

void MainWindow::add_widget(const std::string& type) {
    openkal::client::UiWidget w;
    w.set_type(openkal::client::ui_widget_type_from_name(type));
    w.set_id(unique_id(type));
    if (w.type() == openkal::client::UiWidgetType::Text) {
        w.set_text("${ui.title}");
        w.set_color("${theme.colors.value}");
    } else if (w.type() == openkal::client::UiWidgetType::Button) {
        w.set_w(64.0f);
        w.set_h(22.0f);
        w.set_action("window.close");
    } else if (w.type() == openkal::client::UiWidgetType::Image) {
        w.set_w(64.0f);
        w.set_h(64.0f);
    }
    m_doc.widgets().push_back(std::move(w));
    m_canvas->set_selected_index(static_cast<int>(m_doc.widgets().size()) - 1);
    mark_dirty();
    refresh_tree();
    rebuild_inspector();
    m_canvas->update();
}

void MainWindow::delete_selected() {
    const int sel = m_canvas->selected_index();
    if (sel < 0 || sel >= static_cast<int>(m_doc.widgets().size())) {
        return;
    }
    m_doc.widgets().erase(m_doc.widgets().begin() + sel);
    m_canvas->set_selected_index(-2);
    mark_dirty();
    refresh_tree();
    rebuild_inspector();
    m_canvas->update();
}

void MainWindow::move_selected_up() {
    const int sel = m_canvas->selected_index();
    if (sel <= 0 || sel >= static_cast<int>(m_doc.widgets().size())) {
        return;
    }
    std::swap(m_doc.widgets()[static_cast<std::size_t>(sel)],
              m_doc.widgets()[static_cast<std::size_t>(sel - 1)]);
    m_canvas->set_selected_index(sel - 1);
    mark_dirty();
    refresh_tree();
    rebuild_inspector();
    m_canvas->update();
}

void MainWindow::move_selected_down() {
    const int sel = m_canvas->selected_index();
    if (sel < 0 || sel + 1 >= static_cast<int>(m_doc.widgets().size())) {
        return;
    }
    std::swap(m_doc.widgets()[static_cast<std::size_t>(sel)],
              m_doc.widgets()[static_cast<std::size_t>(sel + 1)]);
    m_canvas->set_selected_index(sel + 1);
    mark_dirty();
    refresh_tree();
    rebuild_inspector();
    m_canvas->update();
}
