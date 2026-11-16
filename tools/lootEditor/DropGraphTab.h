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

#include "graph/DropGraphScene.h"
#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"

#include <QWidget>
#include <vector>

class QGraphicsView;
class QToolBar;

/** Tab 2: draggable drop-table graph editor. */
class DropGraphTab : public QWidget {
    Q_OBJECT

  public:
    /** Creates the graph tab.
     * @param store Shared session data.
     * @param parent Qt parent.
     */
    explicit DropGraphTab(LootDataStore* store, QWidget* parent = nullptr);

  public slots:
    /** Rebuilds the graph for the current selection. */
    void rebuild();

  private slots:
    /** Creates a new inner group. */
    void new_group();

    /** Creates a new outer itemgroup. */
    void new_itemgroup();

    /** Deletes the selected graph node. */
    void delete_selected();

    /** Opens the find-item-in-groups dialog. */
    void find_item_in_groups();

  private:
    /** Returns the preferred itemgroup for add-target selection.
     * @return Selected graph itemgroup index, monster's first itemgroup, or -1.
     */
    [[nodiscard]] int preferred_itemgroup() const;

    /** Itemgroups of the selected monster that reference a group.
     * @param group_index Group index.
     * @return Sorted itemgroup indices.
     */
    [[nodiscard]] std::vector<int> current_itemgroups_using(int group_index) const;

    /** All itemgroups in the drop DB that reference a group.
     * @param group_index Group index.
     * @return Sorted itemgroup indices.
     */
    [[nodiscard]] std::vector<int> all_itemgroups_using(int group_index) const;

    /** Removes a group reference from one itemgroup.
     * @param itemgroup_index Itemgroup index.
     * @param group_index Group index to strip.
     * @return True when the itemgroup was updated.
     */
    [[nodiscard]] bool unlink_group_from_itemgroup(int itemgroup_index, int group_index);

    /** Unlinks / optionally deletes a selected group node.
     * @param group_index Group index.
     * @return True when any change was applied.
     */
    [[nodiscard]] bool delete_group_selection(int group_index);

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Item icon cache. */
    std::unique_ptr<ItemIconCache> m_icons;

    /** Graph scene. */
    DropGraphScene* m_scene = nullptr;

    /** Graph view. */
    QGraphicsView* m_view = nullptr;

    /** Editor toolbar. */
    QToolBar* m_toolbar = nullptr;
};
