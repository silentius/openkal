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

class QLineEdit;
class QListWidget;

/** Searchable prefix list for loot group entries. */
class PrefixPickerDialog : public QDialog {
    Q_OBJECT

  public:
    /** Opens a prefix picker.
     * @param store Session store for prefix names.
     * @param initial_id Pre-selected prefix id.
     * @param parent Qt parent.
     */
    PrefixPickerDialog(LootDataStore* store, int initial_id, QWidget* parent = nullptr);

    /** @return Selected prefix id (0 = none). */
    [[nodiscard]] int selected_prefix() const {
        return m_selected;
    }

  private slots:
    /** Rebuilds the prefix list from the search field. */
    void refresh_list();

    /** Accepts the highlighted prefix. */
    void accept_selection();

  private:
    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Selected prefix id. */
    int m_selected = 0;

    /** Search field. */
    QLineEdit* m_search = nullptr;

    /** Prefix list. */
    QListWidget* m_list = nullptr;
};
