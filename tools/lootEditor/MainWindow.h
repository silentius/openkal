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

#include "DropGraphTab.h"
#include "GroupsTab.h"
#include "ItemGroupsTab.h"
#include "MonsterListTab.h"
#include "SimulatorTab.h"
#include "model/LootDataStore.h"

#include <QMainWindow>

/** Main window with loot-editor tabs. */
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    /** Creates the main window and loads default paths.
     * @param initial_paths Startup data paths.
     * @param parent Qt parent.
     */
    explicit MainWindow(const LootEditorPaths& initial_paths, QWidget* parent = nullptr);

  protected:
    /** Prompts to save dirty data before closing.
     * @param event Close event.
     */
    void closeEvent(QCloseEvent* event) override;

  private slots:
    /** Saves dirty JSON files in place. */
    void save_all();

    /** Opens the path settings dialog and reloads data. */
    void open_settings();

    /** Reloads data from configured paths. */
    void reload_data();

    /** Shows ItemGroups that have no valid nested Groups. */
    void show_empty_itemgroups();

    /** Shows Groups that have no valid item or money entries. */
    void show_empty_groups();

    /** Shows Groups that reference missing item definition indices. */
    void show_invalid_item_groups();

  private:
    /** Builds the menu bar actions. */
    void setup_menu();

    /** Persists paths to QSettings. */
    void save_settings() const;

    /** Loads paths from QSettings when present.
     * @param defaults Fallback paths.
     * @return Resolved paths.
     */
    [[nodiscard]] LootEditorPaths load_settings(const LootEditorPaths& defaults) const;

    /** Confirms export when dirty.
     * @return True when safe to proceed (saved or discarded).
     */
    [[nodiscard]] bool confirm_discard_or_save();

    /** Shared session model. */
    LootDataStore m_store;

    /** Tab widgets. */
    ItemGroupsTab* m_itemgroups_tab = nullptr;
    GroupsTab* m_groups_tab = nullptr;
    MonsterListTab* m_monsters_tab = nullptr;
    DropGraphTab* m_graph_tab = nullptr;
    SimulatorTab* m_sim_tab = nullptr;
};
