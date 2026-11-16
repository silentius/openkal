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

/** Lists empty or invalid loot tables for auditing. */
class LootAuditDialog : public QDialog {
    Q_OBJECT

  public:
    /** Which audit report to show. */
    enum class Mode {
        EmptyItemGroups,
        EmptyGroups,
        InvalidItemGroups,
    };

    /** Opens an audit results dialog.
     * @param mode Report kind.
     * @param store Session store.
     * @param parent Qt parent.
     */
    LootAuditDialog(Mode mode, LootDataStore* store, QWidget* parent = nullptr);

  private slots:
    /** Opens the editor for the highlighted row. */
    void edit_selected();

    /** Cleans up selected rows after confirmation (mode-specific). */
    void cleanup_selected();

    /** Handles double-click on a result row.
     * @param item Clicked tree item.
     * @param column Clicked column (unused).
     */
    void on_item_double_clicked(QTreeWidgetItem* item, int column);

  private:
    /** Rebuilds the result tree for runtime state. */
    void populate();

    /** Collects selected table indices from the tree.
     * @return Selected indices in row order.
     */
    [[nodiscard]] std::vector<int> selected_indices() const;

    /** Unlinks a group from every ItemGroup that references it.
     * @param group_index Group index.
     */
    void unlink_group_from_all_itemgroups(int group_index);

    /** Removes an ItemGroup assignment from every monster that uses it.
     * @param itemgroup_index ItemGroup index.
     */
    void unlink_itemgroup_from_all_monsters(int itemgroup_index);

    /** Removes invalid item rows from one group without deleting the group.
     * @param group_index Group index.
     * @return Number of entries removed.
     */
    [[nodiscard]] int strip_invalid_items_from_group(int group_index);

    /** Active report kind. */
    Mode m_mode = Mode::EmptyItemGroups;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Summary line. */
    QLabel* m_status = nullptr;

    /** Result table. */
    QTreeWidget* m_tree = nullptr;
};
