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

#include <QWidget>
#include <vector>

class QListWidget;

/** Horizontal icon strip of items with ItemPicker-style hover tooltips. */
class MatchedItemsBar : public QWidget {
    Q_OBJECT

  public:
    /** Creates an empty matched-items bar.
     * @param store Session store for names and tooltips.
     * @param icons HyperText icon cache.
     * @param parent Qt parent.
     */
    MatchedItemsBar(LootDataStore* store, ItemIconCache* icons, QWidget* parent = nullptr);

    /** Replaces the visible matched items.
     * @param item_indices item definition indices to show.
     */
    void set_items(const std::vector<int>& item_indices);

    /** Clears the strip. */
    void clear();

  private:
    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Icon cache (non-owning). */
    ItemIconCache* m_icons = nullptr;

    /** Icon list. */
    QListWidget* m_list = nullptr;
};
