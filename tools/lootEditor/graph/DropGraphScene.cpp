/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "graph/DropGraphScene.h"

#include "dialogs/GroupEditorDialog.h"
#include "dialogs/GroupUsageDialog.h"
#include "dialogs/ItemGroupEditorDialog.h"

#include <QSet>
#include <algorithm>

DropGraphScene::DropGraphScene(LootDataStore* store, ItemIconCache* icons, QObject* parent)
    : QGraphicsScene(parent), m_store(store), m_icons(icons) {}

void DropGraphScene::set_icon_cache(ItemIconCache* icons) {
    m_icons = icons;
}

void DropGraphScene::clear_graph() {
    m_clearing = true;
    m_itemgroup_nodes.clear();
    m_group_nodes.clear();
    m_edges.clear();
    clear();
    m_clearing = false;
}

void DropGraphScene::rebuild() {
    clear_graph();
    if (!m_store || m_store->selected_monster() < 0) {
        return;
    }

    const MonsterRecord* monster = m_store->monsters().find(m_store->selected_monster());
    if (!monster) {
        return;
    }

    QSet<int> group_ids;
    QVector<QPair<int, int>> implied_edges;
    int ig_col = 0;
    const int ig_count = static_cast<int>(monster->item_groups.size());

    // Show every assigned slot even when the itemgroup JSON is missing.
    for (const auto& [ig_index, attempts] : monster->item_groups) {
        const openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup(ig_index);
        openkal::server::ItemGroupDef stub;
        stub.index = ig_index;
        const bool missing = (ig == nullptr);
        if (missing) {
            ig = &stub;
        }

        auto* ig_node = new ItemGroupNodeItem(ig_index, attempts, *ig, this, missing);
        const qreal x = 40 + ig_col * 240;
        ig_node->setPos(x, 40);
        addItem(ig_node);
        m_itemgroup_nodes.insert(ig_index, ig_node);

        if (!missing) {
            for (const auto& e : ig->entries) {
                group_ids.insert(e.group_index);
            }
        } else if (m_store->drop_db().find_group(ig_index)) {
            group_ids.insert(ig_index);
            implied_edges.push_back({ig_index, ig_index});
        }
        ++ig_col;
    }

    QList<int> sorted_groups = group_ids.values();
    std::sort(sorted_groups.begin(), sorted_groups.end());

    int g_col = 0;
    for (int gid : sorted_groups) {
        const openkal::server::GroupDef* g = m_store->drop_db().find_group(gid);
        if (!g) {
            continue;
        }
        auto* g_node = new GroupNodeItem(*g, m_store, m_icons, this);
        const qreal x = 40 + g_col * 250;
        g_node->setPos(x, 280);
        addItem(g_node);
        m_group_nodes.insert(gid, g_node);
        ++g_col;
    }

    for (const auto& [ig_index, _] : monster->item_groups) {
        const openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup(ig_index);
        ItemGroupNodeItem* ig_node = m_itemgroup_nodes.value(ig_index, nullptr);
        if (!ig || !ig_node) {
            continue;
        }
        for (const auto& e : ig->entries) {
            GroupNodeItem* g_node = m_group_nodes.value(e.group_index, nullptr);
            if (!g_node) {
                continue;
            }
            auto* edge = new DropEdgeItem(e.chance);
            addItem(edge);
            m_edges.push_back({{ig_index, e.group_index}, edge});
        }
    }
    for (const auto& key : implied_edges) {
        ItemGroupNodeItem* ig_node = m_itemgroup_nodes.value(key.first, nullptr);
        GroupNodeItem* g_node = m_group_nodes.value(key.second, nullptr);
        if (!ig_node || !g_node) {
            continue;
        }
        auto* edge = new DropEdgeItem(QStringLiteral("same id"), true);
        addItem(edge);
        m_edges.push_back({key, edge});
    }

    update_edges();

    const qreal scene_w =
        std::max(800.0, 40.0 + std::max(ig_count, static_cast<int>(sorted_groups.size())) * 250.0);
    setSceneRect(0, 0, scene_w, 560);
}

void DropGraphScene::update_edges() {
    if (m_clearing) {
        return;
    }
    for (const auto& [key, edge] : m_edges) {
        ItemGroupNodeItem* ig_node = m_itemgroup_nodes.value(key.first, nullptr);
        GroupNodeItem* g_node = m_group_nodes.value(key.second, nullptr);
        if (!ig_node || !g_node || !edge) {
            continue;
        }
        edge->set_endpoints(ig_node->center_scene(), g_node->center_scene());
    }
}

void DropGraphScene::edit_itemgroup(int index) {
    openkal::server::ItemGroupDef working;
    openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(index);
    if (ig) {
        working = *ig;
    } else {
        working.index = index;
    }
    ItemGroupEditorDialog dlg(working, m_store, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        m_store->upsert_itemgroup(dlg.result_def());
        rebuild();
    }
}

void DropGraphScene::edit_group(int index) {
    openkal::server::GroupDef* g = m_store->drop_db().find_group_mut(index);
    if (!g) {
        return;
    }
    GroupEditorDialog dlg(*g, m_store, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        m_store->upsert_group(dlg.result_def());
        rebuild();
    }
}

void DropGraphScene::show_group_usage(int index) {
    if (!m_store->drop_db().find_group(index)) {
        return;
    }
    GroupUsageDialog dlg(index, m_store, nullptr);
    dlg.exec();
    if (dlg.group_changed() || dlg.itemgroup_changed()) {
        rebuild();
    }
}
