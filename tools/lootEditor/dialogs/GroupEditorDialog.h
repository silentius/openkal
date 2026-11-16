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

#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QDialog>
#include <memory>
#include <unordered_set>

class QLineEdit;
class QTableWidget;

/** Edits inner group entries. */
class GroupEditorDialog : public QDialog {
    Q_OBJECT

  public:
    /** Creates a group editor.
     * @param def Group to edit.
     * @param store Session data for validation labels.
     * @param parent Qt parent.
     */
    GroupEditorDialog(openkal::server::GroupDef def,
                      LootDataStore* store,
                      QWidget* parent = nullptr);

    /** @return Edited group definition. */
    [[nodiscard]] openkal::server::GroupDef result_def() const {
        return m_def;
    }

  private:
    /** Loads table rows from runtime state. */
    void populate_table();

    /** Reads table rows into runtime state. */
    void apply_table();

    /** Opens the item picker and adds a row for the chosen item.
     * @param item_index Pre-selected item, or <=0 to pick first.
     */
    void add_item_row(int item_index = 0);

    /** Adds a money row. */
    void add_money_row();

    /** Removes selected rows. */
    void remove_selected();

    /** Updates row widgets when entry type changes.
     * @param row Table row index.
     */
    void sync_row_type(int row);

    /** Collects item indices currently listed in the editor table.
     * @return Occupied item definition indices.
     */
    [[nodiscard]] std::unordered_set<int> occupied_item_indices() const;

    /** Group being edited. */
    openkal::server::GroupDef m_def;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** HyperText icon cache. */
    std::unique_ptr<ItemIconCache> m_icons;

    /** Optional name field. */
    QLineEdit* m_name = nullptr;

    /** Entry table. */
    QTableWidget* m_table = nullptr;
};
