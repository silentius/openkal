/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "widgets/MatchedItemsBar.h"

#include "widgets/ItemIconOverlay.h"

#include <QIcon>
#include <QListWidget>
#include <QVBoxLayout>

namespace {
    constexpr int kMatchIconSize = 48;
}  // namespace

MatchedItemsBar::MatchedItemsBar(LootDataStore* store, ItemIconCache* icons, QWidget* parent)
    : QWidget(parent), m_store(store), m_icons(icons) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListWidget::IconMode);
    m_list->setIconSize(QSize(kMatchIconSize, kMatchIconSize));
    m_list->setResizeMode(QListWidget::Adjust);
    m_list->setMovement(QListWidget::Static);
    m_list->setWrapping(true);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(4);
    m_list->setMaximumHeight(110);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list);
    setVisible(false);
}

void MatchedItemsBar::clear() {
    m_list->clear();
    setVisible(false);
}

void MatchedItemsBar::set_items(const std::vector<int>& item_indices) {
    m_list->clear();
    if (!m_store || !m_icons || item_indices.empty()) {
        setVisible(false);
        return;
    }

    for (int item_index : item_indices) {
        QString label = m_store->item_name(item_index);
        if (label.size() > 12) {
            label = label.left(10) + QChar(0x2026);
        }

        QPixmap px = m_icons->icon(item_index);
        const QStringList overlay_lines = m_store->changeprefix_overlay_lines(item_index, 9);
        if (!overlay_lines.isEmpty()) {
            px = apply_item_text_overlay(px, overlay_lines, kMatchIconSize);
        }

        auto* item = new QListWidgetItem();
        item->setIcon(QIcon(px));
        item->setText(label);
        item->setData(Qt::UserRole, item_index);
        item->setToolTip(m_store->item_tooltip(item_index));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setSizeHint(QSize(88, 96));
        m_list->addItem(item);
    }

    setVisible(true);
}
