/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "widgets/GroupEntriesWidget.h"

#include "openkal/server/world/ItemGroupDb.h"
#include "widgets/ItemIconOverlay.h"

#include <QFont>
#include <QIcon>
#include <QTreeWidget>
#include <QVBoxLayout>

GroupEntriesWidget::GroupEntriesWidget(LootDataStore* store, ItemIconCache* icons, QWidget* parent)
    : QWidget(parent), m_store(store), m_icons(icons) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"", "Entry", "Chance %"});
    m_tree->setIconSize(QSize(32, 32));
    m_tree->setRootIsDecorated(false);
    m_tree->setColumnWidth(0, 40);
    m_tree->setColumnWidth(1, 280);
    m_tree->setUniformRowHeights(true);
    layout->addWidget(m_tree);
}

void GroupEntriesWidget::clear() {
    m_tree->clear();
}

void GroupEntriesWidget::show_group(int group_index,
                                    const std::unordered_set<int>& highlight_items) {
    m_tree->clear();
    if (!m_store || group_index <= 0) {
        return;
    }

    const openkal::server::GroupDef* def = m_store->drop_db().find_group(group_index);
    if (!def) {
        return;
    }

    for (const auto& e : def->entries) {
        auto* row = new QTreeWidgetItem();
        if (e.kind == openkal::server::GroupEntry::Kind::Money) {
            if (m_icons) {
                row->setIcon(0, QIcon(m_icons->money_icon()));
            }
            row->setText(1, QString("Money × %1").arg(e.amount));
            row->setToolTip(1, QString("Money amount %1").arg(e.amount));
        } else {
            QPixmap px;
            if (m_icons) {
                px = m_icons->icon(e.item_index);
                const QStringList overlay_lines =
                    m_store->changeprefix_overlay_lines(e.item_index, 8);
                if (!overlay_lines.isEmpty()) {
                    px = apply_item_text_overlay(px, overlay_lines, 32);
                }
                row->setIcon(0, QIcon(px));
            }
            QString label = m_store->item_name(e.item_index);
            if (e.prefix > 0) {
                label = m_store->prefix_name(e.prefix) + " " + label;
            }
            if (e.count != 1) {
                label += QString(" ×%1").arg(e.count);
            }
            if (highlight_items.count(e.item_index) > 0) {
                label += "  ← match";
                QFont f = row->font(1);
                f.setBold(true);
                row->setFont(1, f);
            }
            row->setText(1, label);
            const QString tip = m_store->item_tooltip(e.item_index);
            row->setToolTip(0, tip);
            row->setToolTip(1, tip);
        }
        row->setText(2, QString::number(e.chance, 'f', 2));
        m_tree->addTopLevelItem(row);
    }
}
