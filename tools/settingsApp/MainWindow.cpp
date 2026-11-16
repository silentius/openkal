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

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <set>
#include <utility>

namespace {

    using openkal::settings::BloomQuality;
    using openkal::settings::DisplayMode;
    using openkal::settings::QualityPreset;
    using openkal::settings::ShadowQuality;

    int display_mode_index(DisplayMode mode) {
        switch (mode) {
        case DisplayMode::Maximized:
            return 1;
        case DisplayMode::Borderless:
            return 2;
        case DisplayMode::Windowed:
        default:
            return 0;
        }
    }

    int preset_index(QualityPreset preset) {
        switch (preset) {
        case QualityPreset::Low:
            return 0;
        case QualityPreset::Medium:
            return 1;
        case QualityPreset::High:
            return 2;
        case QualityPreset::Ultra:
            return 3;
        case QualityPreset::Custom:
        default:
            return 4;
        }
    }

    QualityPreset preset_from_index(int index) {
        switch (index) {
        case 0:
            return QualityPreset::Low;
        case 2:
            return QualityPreset::High;
        case 3:
            return QualityPreset::Ultra;
        case 4:
            return QualityPreset::Custom;
        case 1:
        default:
            return QualityPreset::Medium;
        }
    }

    int monster_render_limit_index(int limit) {
        switch (limit) {
        case 10:
            return 0;
        case 50:
            return 2;
        case 0:
            return 3;
        case 25:
        default:
            return 1;
        }
    }

    int monster_render_limit_from_index(int index) {
        switch (index) {
        case 0:
            return 10;
        case 2:
            return 50;
        case 3:
            return 0;
        case 1:
        default:
            return 25;
        }
    }

}  // namespace

MainWindow::MainWindow(std::string profile_path, QWidget* parent) : QMainWindow(parent) {
    build_ui();
    (void) load_profile(profile_path);
}

void MainWindow::build_ui() {
    setWindowTitle(tr("OpenKal Settings"));
    resize(620, 820);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);

    auto* profile_row = new QHBoxLayout();
    m_profile_path = new QLineEdit(root);
    auto* browse = new QPushButton(tr("Browse…"), root);
    auto* reload = new QPushButton(tr("Reload"), root);
    profile_row->addWidget(new QLabel(tr("Profile:"), root));
    profile_row->addWidget(m_profile_path, 1);
    profile_row->addWidget(browse);
    profile_row->addWidget(reload);
    layout->addLayout(profile_row);

    auto* display_group = new QGroupBox(tr("Display"), root);
    auto* display_form = new QFormLayout(display_group);
    m_display_mode = new QComboBox(display_group);
    m_display_mode->addItems({tr("Windowed"), tr("Maximized window"), tr("Borderless maximized")});
    m_resolution = new QComboBox(display_group);
    m_resolution->addItem(tr("Custom"), QSize());
    std::set<std::pair<int, int>> resolutions = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
    for (QScreen* screen : QGuiApplication::screens()) {
        const QSize size = screen->size();
        resolutions.emplace(size.width(), size.height());
    }
    for (const auto& [width, height] : resolutions) {
        const QSize size(width, height);
        m_resolution->addItem(QStringLiteral("%1 × %2").arg(width).arg(height), size);
    }
    auto* size_row = new QHBoxLayout();
    m_width = new QSpinBox(display_group);
    m_height = new QSpinBox(display_group);
    m_width->setRange(640, 16384);
    m_height->setRange(480, 16384);
    size_row->addWidget(m_width);
    size_row->addWidget(new QLabel(QStringLiteral("×"), display_group));
    size_row->addWidget(m_height);
    display_form->addRow(tr("Mode"), m_display_mode);
    display_form->addRow(tr("Known resolution"), m_resolution);
    display_form->addRow(tr("Windowed size"), size_row);
    layout->addWidget(display_group);

    auto* world_group = new QGroupBox(tr("World and effects"), root);
    auto* world_form = new QFormLayout(world_group);
    m_preset = new QComboBox(world_group);
    m_preset->addItems({tr("Low"), tr("Medium"), tr("High"), tr("Ultra"), tr("Custom")});
    m_view_distance = new QSpinBox(world_group);
    m_view_distance->setRange(1, 8);
    m_preload_margin = new QSpinBox(world_group);
    m_preload_margin->setRange(0, 4);
    m_resident_limit = new QSpinBox(world_group);
    m_resident_limit->setRange(9, 1024);
    m_upload_budget = new QDoubleSpinBox(world_group);
    m_upload_budget->setRange(0.25, 12.0);
    m_upload_budget->setDecimals(2);
    m_upload_budget->setSuffix(tr(" ms/frame"));
    m_effect_distance = new QSpinBox(world_group);
    m_effect_distance->setRange(100, 2000);
    world_form->addRow(tr("Quality preset"), m_preset);
    world_form->addRow(tr("View radius (tiles)"), m_view_distance);
    world_form->addRow(tr("Preload margin (tiles)"), m_preload_margin);
    world_form->addRow(tr("Resident tile limit"), m_resident_limit);
    world_form->addRow(tr("Terrain upload budget"), m_upload_budget);
    world_form->addRow(tr("Effect distance"), m_effect_distance);
    layout->addWidget(world_group);

    auto* render_group = new QGroupBox(tr("Rendering"), root);
    auto* render_form = new QFormLayout(render_group);
    m_grass_enabled = new QCheckBox(tr("Render grass and small foliage"), render_group);
    m_bloom = new QComboBox(render_group);
    m_bloom->addItems({tr("Off"), tr("Subtle"), tr("High")});
    m_shadows = new QComboBox(render_group);
    m_shadows->addItems({tr("Off"), tr("Dynamic actors"), tr("Actors + world")});
    m_monster_render_limit = new QComboBox(render_group);
    m_monster_render_limit->addItems({tr("10"), tr("25"), tr("50"), tr("Maximum")});
    m_player_render_limit = new QComboBox(render_group);
    m_player_render_limit->addItems({tr("10"), tr("25"), tr("50"), tr("Maximum")});
    m_player_name_limit = new QComboBox(render_group);
    for (int value : {-1, 5, 10, 25, 50, 0}) {
        m_player_name_limit->addItem(
            value < 0 ? tr("Off") : (value == 0 ? tr("Maximum") : QString::number(value)), value);
    }
    render_form->addRow(QString(), m_grass_enabled);
    render_form->addRow(tr("Bloom"), m_bloom);
    render_form->addRow(tr("Ground shadows"), m_shadows);
    render_form->addRow(tr("Rendered monsters"), m_monster_render_limit);
    render_form->addRow(tr("Rendered players"), m_player_render_limit);
    render_form->addRow(tr("Player nameplates"), m_player_name_limit);
    layout->addWidget(render_group);

    auto* audio_group = new QGroupBox(tr("Audio"), root);
    auto* audio_form = new QFormLayout(audio_group);
    auto* effect_row = new QHBoxLayout();
    m_effect_volume = new QSlider(Qt::Horizontal, audio_group);
    m_effect_volume->setRange(0, 5);
    m_effect_value = new QLabel(audio_group);
    effect_row->addWidget(m_effect_volume);
    effect_row->addWidget(m_effect_value);
    auto* bgm_row = new QHBoxLayout();
    m_bgm_volume = new QSlider(Qt::Horizontal, audio_group);
    m_bgm_volume->setRange(0, 5);
    m_bgm_value = new QLabel(audio_group);
    bgm_row->addWidget(m_bgm_volume);
    bgm_row->addWidget(m_bgm_value);
    m_bgm_enabled = new QCheckBox(tr("Play background music"), audio_group);
    audio_form->addRow(tr("Effects"), effect_row);
    audio_form->addRow(tr("Background music"), bgm_row);
    audio_form->addRow(QString(), m_bgm_enabled);
    layout->addWidget(audio_group);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* save = new QPushButton(tr("Save profile"), root);
    save->setDefault(true);
    buttons->addWidget(save);
    layout->addLayout(buttons);
    m_status = new QLabel(root);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    setCentralWidget(root);

    connect(browse, &QPushButton::clicked, this, &MainWindow::browse_profile);
    connect(reload, &QPushButton::clicked, this, &MainWindow::reload_profile);
    connect(save, &QPushButton::clicked, this, &MainWindow::save_profile);
    connect(
        m_preset, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::apply_preset);
    connect(m_resolution,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::apply_resolution);
    for (QSpinBox* spin :
         {m_view_distance, m_preload_margin, m_resident_limit, m_effect_distance}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::mark_custom);
    }
    connect(m_upload_budget,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &MainWindow::mark_custom);
    connect(m_grass_enabled, &QCheckBox::toggled, this, &MainWindow::mark_custom);
    connect(
        m_bloom, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::mark_custom);
    connect(
        m_shadows, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::mark_custom);
    connect(m_monster_render_limit,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::mark_custom);
    connect(m_player_render_limit,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::mark_custom);
    connect(m_player_name_limit,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::mark_custom);
    connect(m_effect_volume, &QSlider::valueChanged, this, [this](int value) {
        m_effect_value->setText(QString::number(value));
    });
    connect(m_bgm_volume, &QSlider::valueChanged, this, [this](int value) {
        m_bgm_value->setText(QString::number(value));
    });
}

bool MainWindow::load_profile(const std::string& path) {
    std::string error;
    auto document = openkal::platform::ProfileDocument::load(path, &error);
    if (!document) {
        set_status(tr("Could not load profile: %1").arg(QString::fromStdString(error)), true);
        return false;
    }
    auto loaded = openkal::settings::load_client_settings(*document);
    m_document = std::move(*document);
    m_settings = loaded.settings;
    m_profile_path->setText(QString::fromStdString(path));
    show_settings(m_settings);
    if (loaded.warnings.empty()) {
        set_status(tr("Profile loaded."));
    } else {
        set_status(tr("Profile loaded with %1 corrected value(s).").arg(loaded.warnings.size()));
    }
    return true;
}

void MainWindow::show_settings(const openkal::settings::ClientSettings& settings) {
    m_updating = true;
    m_display_mode->setCurrentIndex(display_mode_index(settings.display.mode));
    m_width->setValue(settings.display.width);
    m_height->setValue(settings.display.height);
    const int resolution_index =
        m_resolution->findData(QSize(settings.display.width, settings.display.height));
    m_resolution->setCurrentIndex(std::max(0, resolution_index));
    m_preset->setCurrentIndex(preset_index(settings.preset));
    m_view_distance->setValue(settings.world.view_distance_tiles);
    m_preload_margin->setValue(settings.world.preload_margin_tiles);
    m_resident_limit->setValue(settings.world.resident_tile_limit);
    m_upload_budget->setValue(settings.world.upload_budget_ms);
    m_effect_distance->setValue(settings.world.effect_distance);
    m_grass_enabled->setChecked(settings.render.grass_enabled);
    m_bloom->setCurrentIndex(static_cast<int>(settings.render.bloom));
    m_shadows->setCurrentIndex(static_cast<int>(settings.render.shadows));
    m_monster_render_limit->setCurrentIndex(
        monster_render_limit_index(settings.render.max_rendered_monsters));
    m_player_render_limit->setCurrentIndex(
        monster_render_limit_index(settings.render.max_rendered_players));
    m_player_name_limit->setCurrentIndex(
        m_player_name_limit->findData(settings.render.max_player_names));
    m_effect_volume->setValue(settings.audio.effect_volume);
    m_bgm_volume->setValue(settings.audio.bgm_volume);
    m_bgm_enabled->setChecked(settings.audio.bgm_enabled);
    m_effect_value->setText(QString::number(settings.audio.effect_volume));
    m_bgm_value->setText(QString::number(settings.audio.bgm_volume));
    m_updating = false;
}

openkal::settings::ClientSettings MainWindow::collect_settings() const {
    openkal::settings::ClientSettings settings = m_settings;
    settings.display.mode = m_display_mode->currentIndex() == 2
                                ? DisplayMode::Borderless
                                : (m_display_mode->currentIndex() == 1 ? DisplayMode::Maximized
                                                                       : DisplayMode::Windowed);
    settings.display.width = m_width->value();
    settings.display.height = m_height->value();
    settings.preset = preset_from_index(m_preset->currentIndex());
    settings.world.view_distance_tiles = m_view_distance->value();
    settings.world.preload_margin_tiles = m_preload_margin->value();
    settings.world.resident_tile_limit = m_resident_limit->value();
    settings.world.upload_budget_ms = m_upload_budget->value();
    settings.world.effect_distance = m_effect_distance->value();
    settings.render.grass_enabled = m_grass_enabled->isChecked();
    settings.render.bloom = static_cast<BloomQuality>(m_bloom->currentIndex());
    settings.render.shadows = static_cast<ShadowQuality>(m_shadows->currentIndex());
    settings.render.max_rendered_monsters =
        monster_render_limit_from_index(m_monster_render_limit->currentIndex());
    settings.render.max_rendered_players =
        monster_render_limit_from_index(m_player_render_limit->currentIndex());
    settings.render.max_player_names = m_player_name_limit->currentData().toInt();
    settings.audio.effect_volume = m_effect_volume->value();
    settings.audio.bgm_volume = m_bgm_volume->value();
    settings.audio.bgm_enabled = m_bgm_enabled->isChecked();
    return settings;
}

void MainWindow::browse_profile() {
    const QString selected =
        QFileDialog::getOpenFileName(this,
                                     tr("Open OpenKal profile"),
                                     m_profile_path->text(),
                                     tr("OpenKal profiles (*.profile);;All files (*)"));
    if (!selected.isEmpty()) {
        (void) load_profile(selected.toStdString());
    }
}

void MainWindow::reload_profile() {
    (void) load_profile(m_profile_path->text().toStdString());
}

void MainWindow::save_profile() {
    if (!m_document || m_document->path() != m_profile_path->text().toStdString()) {
        if (!load_profile(m_profile_path->text().toStdString())) {
            return;
        }
    }
    auto settings = collect_settings();
    const auto corrections = openkal::settings::validate_client_settings(settings);
    show_settings(settings);
    openkal::settings::store_client_settings(*m_document, settings);
    std::string error;
    const auto status = m_document->save(&error);
    if (status != openkal::platform::ProfileSaveStatus::Saved) {
        set_status(tr("Could not save profile: %1").arg(QString::fromStdString(error)), true);
        return;
    }
    m_settings = settings;
    set_status(corrections.empty()
                   ? tr("Profile saved.")
                   : tr("Profile saved after correcting %1 value(s).").arg(corrections.size()));
}

void MainWindow::apply_preset(int index) {
    if (m_updating || index == preset_index(QualityPreset::Custom)) {
        return;
    }
    auto settings = openkal::settings::settings_for_preset(preset_from_index(index));
    settings.display = collect_settings().display;
    settings.audio = collect_settings().audio;
    settings.ignore_party_requests = m_settings.ignore_party_requests;
    m_settings = settings;
    show_settings(m_settings);
}

void MainWindow::mark_custom() {
    if (m_updating) {
        return;
    }
    m_updating = true;
    m_preset->setCurrentIndex(preset_index(QualityPreset::Custom));
    m_updating = false;
}

void MainWindow::apply_resolution(int index) {
    if (m_updating || index <= 0) {
        return;
    }
    const QSize size = m_resolution->currentData().toSize();
    if (size.isValid()) {
        m_width->setValue(size.width());
        m_height->setValue(size.height());
    }
}

void MainWindow::set_status(const QString& text, bool error) {
    m_status->setText(text);
    m_status->setStyleSheet(error ? QStringLiteral("color: #b00020") : QString());
}
