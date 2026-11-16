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

#include "dialogs/LootAuditDialog.h"
#include "dialogs/SettingsDialog.h"
#include "model/LootPaths.h"

#include <QCloseEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(const LootEditorPaths& initial_paths, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("OpenKal Loot Editor");
    resize(1100, 760);

    setup_menu();

    auto* tabs = new QTabWidget(this);
    m_itemgroups_tab = new ItemGroupsTab(&m_store, tabs);
    m_groups_tab = new GroupsTab(&m_store, tabs);
    m_monsters_tab = new MonsterListTab(&m_store, tabs);
    m_graph_tab = new DropGraphTab(&m_store, tabs);
    m_sim_tab = new SimulatorTab(&m_store, tabs);
    tabs->addTab(m_itemgroups_tab, "ItemGroups");
    tabs->addTab(m_groups_tab, "Groups");
    tabs->addTab(m_monsters_tab, "Monsters");
    tabs->addTab(m_graph_tab, "Drop Graph");
    tabs->addTab(m_sim_tab, "Simulator");
    setCentralWidget(tabs);

    statusBar()->showMessage("Ready");

    connect(&m_store, &LootDataStore::selectionChanged, this, [this](int idx) {
        if (idx < 0) {
            statusBar()->showMessage("No monster selected");
            return;
        }
        const MonsterRecord* rec = m_store.monsters().find(idx);
        if (!rec) {
            return;
        }
        QString ig_summary;
        for (const auto& [ig, att] : rec->item_groups) {
            if (!ig_summary.isEmpty()) {
                ig_summary += ", ";
            }
            ig_summary += QString("IG %1×%2").arg(ig).arg(att);
        }
        statusBar()->showMessage(QString("%1 [%2] — %3")
                                     .arg(QString::fromStdString(rec->name))
                                     .arg(rec->level)
                                     .arg(ig_summary));
    });

    connect(&m_store, &LootDataStore::dirtyChanged, this, [this](bool dirty) {
        setWindowTitle(dirty ? "OpenKal Loot Editor *" : "OpenKal Loot Editor");
    });

    const LootEditorPaths paths = load_settings(initial_paths);
    if (!m_store.load(paths)) {
        statusBar()->showMessage("Warning: failed to load some drop tables.");
    } else {
        const std::string ht = hypertext_dir_for(m_store.paths().data_root);
        if (ht.empty()) {
            statusBar()->showMessage(
                "HyperText icons not found — set Client data in File → Settings");
        } else {
            statusBar()->showMessage(QString("HyperText: %1").arg(QString::fromStdString(ht)));
        }
    }
    save_settings();
}

void MainWindow::setup_menu() {
    auto* file = menuBar()->addMenu("&File");
    file->addAction("&Save", QKeySequence::Save, this, &MainWindow::save_all);
    file->addAction("&Reload", this, &MainWindow::reload_data);
    file->addSeparator();
    file->addAction("&Settings…", this, &MainWindow::open_settings);
    file->addSeparator();
    file->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    auto* tools = menuBar()->addMenu("&Tools");
    tools->addAction("Show Empty &ItemGroups…", this, &MainWindow::show_empty_itemgroups);
    tools->addAction("Show Empty &Groups…", this, &MainWindow::show_empty_groups);
    tools->addAction("Show Invalid &Items Groups…", this, &MainWindow::show_invalid_item_groups);
}

void MainWindow::show_empty_itemgroups() {
    LootAuditDialog dlg(LootAuditDialog::Mode::EmptyItemGroups, &m_store, this);
    dlg.exec();
}

void MainWindow::show_empty_groups() {
    LootAuditDialog dlg(LootAuditDialog::Mode::EmptyGroups, &m_store, this);
    dlg.exec();
}

void MainWindow::show_invalid_item_groups() {
    LootAuditDialog dlg(LootAuditDialog::Mode::InvalidItemGroups, &m_store, this);
    dlg.exec();
}

void MainWindow::save_all() {
    if (!m_store.is_dirty()) {
        statusBar()->showMessage("Nothing to save.");
        return;
    }
    if (m_store.export_dirty()) {
        statusBar()->showMessage("Saved all changes.");
    } else {
        QMessageBox::warning(this, "Save failed", "One or more files could not be written.");
    }
}

void MainWindow::open_settings() {
    if (!confirm_discard_or_save()) {
        return;
    }
    SettingsDialog dlg(m_store.paths(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const LootEditorPaths paths = dlg.paths();
    if (!m_store.load(paths)) {
        statusBar()->showMessage("Warning: failed to load some drop tables.");
    }
    save_settings();
    statusBar()->showMessage("Settings applied.");
}

void MainWindow::reload_data() {
    if (!confirm_discard_or_save()) {
        return;
    }
    if (!m_store.load(m_store.paths())) {
        statusBar()->showMessage("Warning: failed to load some drop tables.");
        return;
    }
    statusBar()->showMessage("Reloaded data.");
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_discard_or_save()) {
        save_settings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::save_settings() const {
    QSettings settings("OpenKal", "loot_editor");
    const LootEditorPaths& p = m_store.paths();
    settings.setValue("loot_config_root", QString::fromStdString(p.loot_config_root));
    settings.setValue("data_root", QString::fromStdString(p.data_root));
}

LootEditorPaths MainWindow::load_settings(const LootEditorPaths& defaults) const {
    QSettings settings("OpenKal", "loot_editor");
    LootEditorPaths p = defaults;
    if (!defaults.loot_config_root_locked && settings.contains("loot_config_root")) {
        p.loot_config_root = settings.value("loot_config_root").toString().toStdString();
    }
    if (!defaults.data_root_locked && settings.contains("data_root")) {
        p.data_root = settings.value("data_root").toString().toStdString();
    }
    return p;
}

bool MainWindow::confirm_discard_or_save() {
    if (!m_store.is_dirty()) {
        return true;
    }
    const auto answer =
        QMessageBox::question(this,
                              "Unsaved changes",
                              "Save changes before continuing?",
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        save_all();
        return !m_store.is_dirty();
    }
    if (answer == QMessageBox::Discard) {
        m_store.clear_dirty();
        return true;
    }
    return false;
}
