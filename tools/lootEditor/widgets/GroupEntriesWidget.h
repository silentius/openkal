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
#include <unordered_set>

class QTreeWidget;

/** Read-only tree of group entries with icons and chance percentages. */
class GroupEntriesWidget : public QWidget {
    Q_OBJECT

  public:
    /** Creates an empty group entries view.
     * @param store Session store for names and tooltips.
     * @param icons HyperText icon cache.
     * @param parent Qt parent.
     */
    GroupEntriesWidget(LootDataStore* store, ItemIconCache* icons, QWidget* parent = nullptr);

    /** Shows all entries for a group.
     * @param group_index Group index, or <=0 to clear.
     * @param highlight_items item definition indices to bold as matches.
     */
    void show_group(int group_index, const std::unordered_set<int>& highlight_items = {});

    /** Clears the tree. */
    void clear();

  private:
    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Icon cache (non-owning). */
    ItemIconCache* m_icons = nullptr;

    /** Entry tree. */
    QTreeWidget* m_tree = nullptr;
};
