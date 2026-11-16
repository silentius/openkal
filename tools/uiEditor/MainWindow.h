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

#include "BmpStore.h"
#include "UiCanvas.h"
#include "openkal/client/UiDocument.h"

#include <QMainWindow>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

/** Qt designer for openkal.ui/v1 documents. */
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    /**
     * @param repo_root OpenKal repository root.
     * @param configuration_root Selected OpenKal configuration root.
     * @param data_root Client data root containing UI/ and HyperText/.
     * @param parent Qt parent.
     */
    MainWindow(std::string repo_root,
               std::string configuration_root,
               std::string data_root,
               QWidget* parent = nullptr);

    /**
     * Loads a v1 JSON path.
     * @param path File path.
     * @return True on success.
     */
    [[nodiscard]] bool open_document(const std::string& path);

  protected:
    /**
     * Prompts to save when dirty.
     * @param event Close event.
     */
    void closeEvent(QCloseEvent* event) override;

  private slots:
    /** Opens a JSON file. */
    void open_file();

    /** Saves the current document. */
    void save_file();

    /** Saves to a new path. */
    void save_file_as();

    /** Creates a new window document. */
    void new_file();

    /** Adds the palette type under the cursor. */
    void add_from_palette();

    /** Deletes the selected widget. */
    void delete_selected();

    /** Moves the selected widget earlier in draw order. */
    void move_selected_up();

    /** Moves the selected widget later in draw order. */
    void move_selected_down();

    /** Rebuilds the inspector from the current selection. */
    void rebuild_inspector();

    /** Writes inspector fields back into the document. */
    void apply_inspector();

    /** Syncs tree selection from the canvas. */
    void on_canvas_selection(int index);

    /** Toggles 32 px snap. */
    void toggle_grid_snap(bool on);

  private:
    /** Builds docks and menus. */
    void setup_ui();

    /** Reloads dummy vars and VARIABLE catalog. */
    void reload_catalog();

    /** Refreshes the widget tree. */
    void refresh_tree();

    /** Marks the document dirty. */
    void mark_dirty();

    /** @return True when it is safe to discard or the user saved. */
    [[nodiscard]] bool confirm_discard();

    /**
     * Loads a v1 JSON path.
     * @param path File path.
     * @return True on success.
     */
    [[nodiscard]] bool load_path(const std::string& path);

    /**
     * Writes the current document.
     * @param path Destination path.
     * @return True on success.
     */
    [[nodiscard]] bool write_path(const std::string& path);

    /**
     * Inserts a widget of the given type at (0,0).
     * @param type Widget type name.
     */
    void add_widget(const std::string& type);

    /**
     * @param prefix Id stem.
     * @return Unique widget id.
     */
    [[nodiscard]] std::string unique_id(const std::string& prefix) const;

    /** Repository root. */
    std::string m_repo_root;

    /** Selected OpenKal configuration root. */
    std::string m_configuration_root;

    /** Client data root. */
    std::string m_data_root;

    /** Current file path. */
    std::string m_path;

    /** Dirty flag. */
    bool m_dirty = false;

    /** Parsed document. */
    openkal::client::UiDocument m_doc;

    /** BMP cache. */
    BmpStore m_bmps;

    /** Catalog keys from VARIABLES.md. */
    std::vector<std::string> m_var_keys;

    /** Canvas. */
    UiCanvas* m_canvas = nullptr;

    /** Non-persistent preview of the inventory content tab. */
    QComboBox* m_inventory_preview = nullptr;

    /** Widget tree. */
    QTreeWidget* m_tree = nullptr;

    /** Palette list. */
    QListWidget* m_palette = nullptr;

    /** Inspector form. */
    QFormLayout* m_form = nullptr;

    /** Inspector id. */
    QLineEdit* m_id_edit = nullptr;

    /** Inspector type (read-only). */
    QLineEdit* m_type_edit = nullptr;

    /** Inspector X in native pixels. */
    QSpinBox* m_x_spin = nullptr;

    /** Inspector Y in native pixels. */
    QSpinBox* m_y_spin = nullptr;

    /** Inspector W in native pixels (0 = native BMP width). */
    QSpinBox* m_w_spin = nullptr;

    /** Inspector H in native pixels (0 = native BMP height). */
    QSpinBox* m_h_spin = nullptr;

    /** Button label X offset from the widget top-left. */
    QSpinBox* m_label_ox_spin = nullptr;

    /** Button label Y offset from the widget top-left. */
    QSpinBox* m_label_oy_spin = nullptr;

    /** Inspector text / bind. */
    QLineEdit* m_text_edit = nullptr;

    /** Variable picker. */
    QComboBox* m_var_combo = nullptr;

    /** Color. */
    QLineEdit* m_color_edit = nullptr;

    /** File. */
    QLineEdit* m_file_edit = nullptr;

    /** Action. */
    QLineEdit* m_action_edit = nullptr;

    /** Bind. */
    QLineEdit* m_bind_edit = nullptr;
    /** Scroll viewport widget ID. */
    QLineEdit* m_viewport_edit = nullptr;
    /** Scroll offset catalog binding. */
    QLineEdit* m_scroll_bind_edit = nullptr;
    QComboBox* m_fill_mode_combo = nullptr;
    QComboBox* m_direction_combo = nullptr;
    /** Clipping shape for minimap canvases. */
    QComboBox* m_shape_combo = nullptr;

    /** visible_when. */
    QLineEdit* m_visible_when_edit = nullptr;

    /** pending_when. */
    QLineEdit* m_pending_when_edit = nullptr;

    /** Visible checkbox. */
    QCheckBox* m_visible_check = nullptr;
    QCheckBox* m_multiline_check = nullptr;

    /** Client-box width in native pixels. */
    QSpinBox* m_win_w_spin = nullptr;

    /** Client-box height in native pixels. */
    QSpinBox* m_win_h_spin = nullptr;

    /** Chrome preset. */
    QComboBox* m_preset_combo = nullptr;

    /** Selects the close-capable variant of the built-in info-menu frame. */
    QCheckBox* m_close_button_check = nullptr;

    /** Chrome edges (tile / stretch). */
    QComboBox* m_edges_combo = nullptr;

    /** Chrome pad left. */
    QDoubleSpinBox* m_pad_l_spin = nullptr;

    /** Chrome pad top. */
    QDoubleSpinBox* m_pad_t_spin = nullptr;

    /** Chrome pad right. */
    QDoubleSpinBox* m_pad_r_spin = nullptr;

    /** Chrome pad bottom. */
    QDoubleSpinBox* m_pad_b_spin = nullptr;

    /** Close hitbox ox (chrome selection). */
    QDoubleSpinBox* m_close_ox_spin = nullptr;

    /** Close hitbox oy. */
    QDoubleSpinBox* m_close_oy_spin = nullptr;

    /** Close hitbox width. */
    QDoubleSpinBox* m_close_w_spin = nullptr;

    /** Close hitbox height. */
    QDoubleSpinBox* m_close_h_spin = nullptr;

    /** Text align (left / center / right). */
    QComboBox* m_align_combo = nullptr;

    /** Ignore inspector signals while filling. */
    bool m_filling = false;
};
