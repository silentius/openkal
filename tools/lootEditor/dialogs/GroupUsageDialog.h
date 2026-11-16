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

#include <QDialog>
#include <vector>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

/** Lists every itemgroup that references a loot group. */
class GroupUsageDialog : public QDialog {
    Q_OBJECT

  public:
    /** Opens the usage dialog for one group.
     * @param group_index Group index to look up.
     * @param store Session store.
     * @param parent Qt parent.
     */
    GroupUsageDialog(int group_index, LootDataStore* store, QWidget* parent = nullptr);

    /** @return True when the group was edited and saved. */
    [[nodiscard]] bool group_changed() const {
        return m_group_changed;
    }

    /** @return True when an itemgroup was edited and saved. */
    [[nodiscard]] bool itemgroup_changed() const {
        return m_itemgroup_changed;
    }

  private slots:
    /** Opens the group editor for the looked-up group. */
    void edit_group();

    /** Opens the itemgroup editor for the highlighted row. */
    void edit_selected_itemgroup();

    /** Handles double-click on an itemgroup row.
     * @param item Clicked tree item.
     * @param column Clicked column (unused).
     */
    void on_item_double_clicked(QTreeWidgetItem* item, int column);

  private:
    /** One itemgroup hit with the chance used for this group. */
    struct UsageRow {
        /** Itemgroup index. */
        int itemgroup_index = 0;

        /** Chance percent of the group reference. */
        float chance = 0.0f;
    };

    /** Collects itemgroups that reference runtime state.
     * @return Matching rows sorted by itemgroup index.
     */
    [[nodiscard]] std::vector<UsageRow> matching_itemgroups() const;

    /** Formats monster names that roll an itemgroup.
     * @param itemgroup_index Itemgroup index.
     * @return Comma-separated monster labels, or "—".
     */
    [[nodiscard]] QString monsters_using(int itemgroup_index) const;

    /** Fills the tree from the current drop database. */
    void populate();

    /** Group being inspected. */
    int m_group_index = 0;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** True after a successful group save. */
    bool m_group_changed = false;

    /** True after a successful itemgroup save. */
    bool m_itemgroup_changed = false;

    /** Status / summary line. */
    QLabel* m_status = nullptr;

    /** Usage table. */
    QTreeWidget* m_tree = nullptr;
};
