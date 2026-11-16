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

#include "model/LootDataStore.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QTableWidget;

/** Tab 1: searchable monster selection list. */
class MonsterListTab : public QWidget {
    Q_OBJECT

  public:
    /** Creates the monster list tab.
     * @param store Shared session data.
     * @param parent Qt parent.
     */
    explicit MonsterListTab(LootDataStore* store, QWidget* parent = nullptr);

  public slots:
    /** Refreshes the monster list from the store. */
    void refresh();

  private slots:
    /** Filters the list by search text. */
    void apply_filter();

    /** Handles monster selection. */
    void on_selection_changed();

    /** Applies itemGroups table edits to the store. */
    void apply_item_groups();

    /** Adds an itemgroup row to the editor table. */
    void add_itemgroup_row();

    /** Removes selected itemgroup rows. */
    void remove_itemgroup_rows();

    /** Steps the level filter by one entry, wrapping through All.
     * @param delta -1 for previous, +1 for next.
     */
    void step_level(int delta);

  private:
    /** Populates itemGroups editor for the selected monster. */
    void load_item_groups_editor();

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Search filter field. */
    QLineEdit* m_filter = nullptr;

    /** Level filter (All + 1–120). */
    QComboBox* m_level_filter = nullptr;

    /** Monster list widget. */
    QListWidget* m_list = nullptr;

    /** Selected monster itemGroups editor. */
    QTableWidget* m_itemgroups = nullptr;
};
