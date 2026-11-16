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

#include "openkal/platform/ProfileDocument.h"
#include "openkal/settings/ClientSettings.h"

#include <QMainWindow>
#include <optional>
#include <string>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;

/** Native Qt editor for the client settings stored in an OpenKal profile. */
class MainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    /** Creates the settings editor and opens a profile.
     * @param profile_path Initial profile file path.
     * @param parent Optional Qt parent.
     */
    explicit MainWindow(std::string profile_path, QWidget* parent = nullptr);

  private slots:
    void browse_profile();
    void reload_profile();
    void save_profile();
    void apply_preset(int index);
    void mark_custom();
    void apply_resolution(int index);

  private:
    void build_ui();
    bool load_profile(const std::string& path);
    void show_settings(const openkal::settings::ClientSettings& settings);
    [[nodiscard]] openkal::settings::ClientSettings collect_settings() const;
    void set_status(const QString& text, bool error = false);

    std::optional<openkal::platform::ProfileDocument> m_document;
    openkal::settings::ClientSettings m_settings;
    bool m_updating = false;

    QLineEdit* m_profile_path = nullptr;
    QComboBox* m_display_mode = nullptr;
    QComboBox* m_resolution = nullptr;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QComboBox* m_preset = nullptr;
    QSpinBox* m_view_distance = nullptr;
    QSpinBox* m_preload_margin = nullptr;
    QSpinBox* m_resident_limit = nullptr;
    QDoubleSpinBox* m_upload_budget = nullptr;
    QSpinBox* m_effect_distance = nullptr;
    QCheckBox* m_grass_enabled = nullptr;
    QComboBox* m_bloom = nullptr;
    QComboBox* m_shadows = nullptr;
    QComboBox* m_monster_render_limit = nullptr;
    QComboBox* m_player_render_limit = nullptr;
    QComboBox* m_player_name_limit = nullptr;
    QSlider* m_effect_volume = nullptr;
    QSlider* m_bgm_volume = nullptr;
    QCheckBox* m_bgm_enabled = nullptr;
    QLabel* m_effect_value = nullptr;
    QLabel* m_bgm_value = nullptr;
    QLabel* m_status = nullptr;
};
