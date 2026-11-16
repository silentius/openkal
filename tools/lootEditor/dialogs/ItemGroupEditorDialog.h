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
#include "openkal/server/world/ItemGroupDb.h"

#include <QDialog>

class QLineEdit;
class QTableWidget;

/** Edits outer itemgroup group references. */
class ItemGroupEditorDialog : public QDialog {
    Q_OBJECT

  public:
    /** Creates an itemgroup editor.
     * @param def Itemgroup to edit.
     * @param store Session data.
     * @param parent Qt parent.
     */
    ItemGroupEditorDialog(openkal::server::ItemGroupDef def,
                          LootDataStore* store,
                          QWidget* parent = nullptr);

    /** @return Edited itemgroup definition. */
    [[nodiscard]] openkal::server::ItemGroupDef result_def() const {
        return m_def;
    }

  private:
    /** Loads table rows from runtime state. */
    void populate_table();

    /** Reads table rows into runtime state. */
    void apply_table();

    /** Adds an empty group reference row. */
    void add_row();

    /** Removes selected rows. */
    void remove_selected();

    /** Itemgroup being edited. */
    openkal::server::ItemGroupDef m_def;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Optional name field. */
    QLineEdit* m_name = nullptr;

    /** Reference table. */
    QTableWidget* m_table = nullptr;
};
