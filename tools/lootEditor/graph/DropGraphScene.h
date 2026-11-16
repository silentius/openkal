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

#include "graph/DropEdgeItem.h"
#include "graph/GroupNodeItem.h"
#include "graph/ItemGroupNodeItem.h"
#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"

#include <QGraphicsScene>
#include <QHash>
#include <QPair>
#include <QVector>

/** Drop-table graph for the selected monster. */
class DropGraphScene : public QGraphicsScene {
    Q_OBJECT

  public:
    /** Creates an empty drop graph scene.
     * @param store Shared session data.
     * @param icons Item icon cache.
     * @param parent Qt parent.
     */
    DropGraphScene(LootDataStore* store, ItemIconCache* icons, QObject* parent = nullptr);

    /** Rebuilds the graph for the current monster selection. */
    void rebuild();

    /** Updates all edge endpoints after node movement. */
    void update_edges();

    /** Opens the itemgroup editor for a node index.
     * @param index Itemgroup index.
     */
    void edit_itemgroup(int index);

    /** Opens the group editor for a node index.
     * @param index Group index.
     */
    void edit_group(int index);

    /** Shows every itemgroup that references a group.
     * @param index Group index.
     */
    void show_group_usage(int index);

    /** Updates the icon cache pointer after catalog reload.
     * @param icons New icon cache.
     */
    void set_icon_cache(ItemIconCache* icons);

  private:
    /** Clears nodes and edges. */
    void clear_graph();

    /** Session data. */
    LootDataStore* m_store = nullptr;

    /** Icon cache. */
    ItemIconCache* m_icons = nullptr;

    /** Suppresses edge updates while the scene is being torn down. */
    bool m_clearing = false;

    /** Itemgroup nodes keyed by index. */
    QHash<int, ItemGroupNodeItem*> m_itemgroup_nodes;

    /** Group nodes keyed by index. */
    QHash<int, GroupNodeItem*> m_group_nodes;

    /** Edges from (itemgroup, group) pairs. */
    QVector<QPair<QPair<int, int>, DropEdgeItem*>> m_edges;
};
