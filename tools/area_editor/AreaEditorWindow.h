/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "bake.h"
#include "layers.h"

#include <QMainWindow>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class QAction;
class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QString;

class AreaCanvas;

namespace openkal::engine {
    class IEngine;
}

/** Startup configuration for the Qt area editor. */
struct AreaEditorOptions {
    std::string repo_root;
    std::string profile_path;
    std::string data_root_override;
    std::string initial_area_path;
    std::string initial_output_path;
    int bake_resolution = 4000;
};

/** Qt main window for streamed multi-area attribute editing. */
class AreaEditorWindow final : public QMainWindow {
  public:
    /** Reports the overall startup progress and the current task. */
    using StartupProgress = std::function<void(int, const QString&)>;

    /** Creates the editor and loads the configured runtime profile.
     * @param engine Hidden OpenGL engine used for top-down bakes.
     * @param options Startup paths and bake resolution.
     * @param parent Optional Qt parent.
     * @param progress Optional observer for the startup steps.
     */
    AreaEditorWindow(openkal::engine::IEngine& engine,
                     AreaEditorOptions options,
                     QWidget* parent = nullptr,
                     const StartupProgress& progress = {});

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    struct LoadedArea {
        int patch_x = 0;
        int patch_y = 0;
        std::string source_path;
        std::string save_path;
        openkal::area_editor::BakePaths bake_paths;
        openkal::area_editor::BakeResult bake;
        std::unique_ptr<openkal::area_editor::AttrEditor> editor;
        bool wrote_backup = false;
    };

    void setup_actions();
    void setup_layer_dock();
    void setup_area_dock();
    void open_profile_dialog();
    [[nodiscard]] bool load_profile(const std::string& profile_path);
    void refresh_area_list();
    void schedule_area_classification();
    void process_area_classification(std::uint64_t generation);
    void filter_area_list(const QString& filter);
    [[nodiscard]] bool
    activate_area(const std::string& path, bool center, const StartupProgress& progress = {});
    [[nodiscard]] LoadedArea* load_area_document(const std::string& path,
                                                 bool show_errors,
                                                 int bake_resolution,
                                                 const StartupProgress& progress = {});
    void reload_area();
    void rebake_area();
    void save_areas();
    void save_area_as();
    [[nodiscard]] bool save_to(LoadedArea& area, const std::string& path, bool allow_temp_fallback);
    void undo();
    void clear_active_layer();
    void install_hotkeys();
    void clear_layer_selection();
    void set_active_layer(openkal::area_editor::LayerId layer);
    void set_layer_visible(openkal::area_editor::LayerId layer, bool visible);
    void set_erase_tool(bool erase);
    void sync_paint_tool();
    void paint_at(int patch_x, int patch_y, int x, int y, bool erase);
    void auto_paint_at(int patch_x, int patch_y, int subcell_x, int subcell_y, bool erase);
    void refresh_object_hover(bool valid, int patch_x, int patch_y, int subcell_x, int subcell_y);
    void clear_object_highlight();
    void publish_contours(const LoadedArea& area);
    void refresh_selection_preview();
    void apply_selection();
    void show_cursor_info(bool valid, int patch_x, int patch_y, int subcell_x, int subcell_y);
    void refresh_overlay(LoadedArea& area);
    void refresh_all_overlays();
    void refresh_window_state();
    void clear_loaded_areas();
    void select_active_area_in_list();
    [[nodiscard]] bool confirm_unsaved();
    void view_changed(int min_x, int min_y, int max_x, int max_y);
    void schedule_visible_areas();
    void process_load_queue();
    void trim_area_cache();
    void set_active_area(std::uint64_t key, bool center);
    [[nodiscard]] LoadedArea* active_area();
    [[nodiscard]] const LoadedArea* active_area() const;
    [[nodiscard]] std::string resolve_area_path(const std::string& path) const;
    [[nodiscard]] static std::uint64_t area_key(int patch_x, int patch_y);

    openkal::engine::IEngine& m_engine;
    AreaEditorOptions m_options;
    std::string m_profile_path;
    std::string m_data_root;
    std::string m_area_directory;
    std::string m_map_catalog_path;
    std::vector<std::pair<int, int>> m_world_layout;
    std::unordered_map<std::uint64_t, std::string> m_area_paths;
    std::unordered_map<std::uint64_t, std::unique_ptr<LoadedArea>> m_loaded_areas;
    std::deque<std::pair<QListWidgetItem*, std::string>> m_area_classification_queue;
    std::deque<std::uint64_t> m_load_queue;
    std::unordered_set<std::uint64_t> m_queued_areas;
    std::uint64_t m_active_area_key = 0;
    bool m_has_active_area = false;
    bool m_starting = true;
    bool m_area_classification_scheduled = false;
    bool m_load_scheduled = false;
    std::uint64_t m_area_list_generation = 0;
    int m_visible_min_x = 0;
    int m_visible_min_y = 0;
    int m_visible_max_x = -1;
    int m_visible_max_y = -1;
    bool m_erase_tool = false;
    bool m_layer_armed = false;
    bool m_preview_matches = false;
    int m_preview_min_x = 0;
    int m_preview_min_y = 0;
    int m_preview_max_x = -1;
    int m_preview_max_y = -1;
    int m_preview_layer = -1;
    bool m_preview_fit = true;
    bool m_preview_erase = false;
    bool m_hover_valid = false;
    bool m_object_highlight_visible = false;
    int m_hover_patch_x = 0;
    int m_hover_patch_y = 0;
    int m_hover_subcell_x = 0;
    int m_hover_subcell_y = 0;
    int m_object_preview_patch_x = -1;
    int m_object_preview_patch_y = -1;
    int m_object_preview_layer = -1;
    bool m_object_preview_erase = false;
    const openkal::area_editor::ObjectMask* m_object_preview_mask = nullptr;

    AreaCanvas* m_canvas = nullptr;
    QLineEdit* m_area_search = nullptr;
    QListWidget* m_area_list = nullptr;
    QLabel* m_area_count = nullptr;
    QLabel* m_profile_label = nullptr;
    QSpinBox* m_brush_spin = nullptr;
    QAction* m_save_action = nullptr;
    QAction* m_save_as_action = nullptr;
    QAction* m_reload_action = nullptr;
    QAction* m_rebake_action = nullptr;
    QAction* m_undo_action = nullptr;
    QAction* m_clear_action = nullptr;
    QAction* m_object_action = nullptr;
    QAction* m_navigate_action = nullptr;
    QAction* m_draw_action = nullptr;
    QAction* m_remove_action = nullptr;
    QAction* m_rectangle_action = nullptr;
    QButtonGroup* m_layer_group = nullptr;
    QCheckBox* m_contours_check = nullptr;
    QCheckBox* m_grid_check = nullptr;
    QCheckBox* m_selection_erase = nullptr;
    QRadioButton* m_fit_radio = nullptr;
    QRadioButton* m_fill_radio = nullptr;
    QPushButton* m_apply_button = nullptr;
    std::array<QRadioButton*, static_cast<int>(openkal::area_editor::LayerId::Count)>
        m_layer_radios{};
    std::array<QCheckBox*, static_cast<int>(openkal::area_editor::LayerId::Count)>
        m_layer_visibility{};
};
