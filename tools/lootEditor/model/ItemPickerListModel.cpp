/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "model/ItemPickerListModel.h"

#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"
#include "widgets/ItemIconOverlay.h"

#include <QApplication>
#include <QBrush>
#include <QPainter>
#include <cmath>
#include <utility>

namespace {
    constexpr int kPickerIconSize = 48;
}

ItemPickerListModel::ItemPickerListModel(const LootDataStore& store,
                                         const ItemIconCache& icons,
                                         const std::unordered_set<int>& occupied_items,
                                         QObject* parent)
    : QAbstractListModel(parent), m_store(store), m_icons(icons), m_occupied_items(occupied_items) {
}

void ItemPickerListModel::replace_items(std::vector<int> indices) {
    beginResetModel();
    m_indices = std::move(indices);
    m_presentation.clear();
    endResetModel();
}

int ItemPickerListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_indices.size());
}

bool ItemPickerListModel::contains(const QModelIndex& index) const {
    return index.isValid() && index.model() == this && index.column() == 0 && index.row() >= 0 &&
           static_cast<std::size_t>(index.row()) < m_indices.size();
}

Qt::ItemFlags ItemPickerListModel::flags(const QModelIndex& index) const {
    return contains(index) ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

QVariant ItemPickerListModel::data(const QModelIndex& index, int role) const {
    if (!contains(index)) {
        return {};
    }
    const int item_index = m_indices[static_cast<std::size_t>(index.row())];
    if (role == Qt::UserRole) {
        return item_index;
    }
    if (role == Qt::SizeHintRole) {
        return QSize(88, 96);
    }
    if (role == Qt::TextAlignmentRole) {
        return static_cast<int>(Qt::AlignHCenter | Qt::AlignTop);
    }
    const bool occupied = m_occupied_items.count(item_index) > 0;
    if (role == Qt::BackgroundRole) {
        return occupied ? QVariant(QBrush(QColor(120, 190, 255))) : QVariant();
    }
    if (role == Qt::ForegroundRole) {
        return occupied ? QVariant(QBrush(QColor(10, 40, 90))) : QVariant();
    }
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole && role != Qt::DecorationRole) {
        return {};
    }
    auto& presentation = m_presentation[item_index];
    if (role == Qt::DisplayRole) {
        if (!presentation.label) {
            QString label = m_store.item_name(item_index);
            if (label.size() > 12) {
                label = label.left(10) + QChar(0x2026);
            }
            if (occupied) {
                label = QString::fromUtf8("● ") + label;
            }
            presentation.label = std::move(label);
        }
        return *presentation.label;
    }
    if (role == Qt::ToolTipRole) {
        if (!presentation.tooltip) {
            QString tooltip = m_store.item_tooltip(item_index);
            if (occupied) {
                tooltip += "\n\nAlready in this group";
            }
            presentation.tooltip = std::move(tooltip);
        }
        return *presentation.tooltip;
    }
    if (!presentation.icon) {
        presentation.icon = compose_icon(item_index);
    }
    return *presentation.icon;
}

QPixmap ItemPickerListModel::compose_icon(int item_index) const {
    const bool occupied = m_occupied_items.count(item_index) > 0;
    QPixmap base = m_icons.icon(item_index);
    const QStringList overlay_lines = m_store.changeprefix_overlay_lines(item_index, 9);
    if (!overlay_lines.isEmpty()) {
        base = apply_item_text_overlay(base, overlay_lines, kPickerIconSize);
    }
    if (!occupied) {
        return base;
    }

    const qreal dpr = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    const int pixel = static_cast<int>(std::lround(kPickerIconSize * dpr));
    QPixmap canvas(pixel, pixel);
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QColor plate(100, 185, 255, 220);
    painter.setPen(Qt::NoPen);
    painter.setBrush(plate);
    painter.drawRoundedRect(
        QRectF(0.5, 0.5, kPickerIconSize - 1.0, kPickerIconSize - 1.0), 4.0, 4.0);

    if (!base.isNull()) {
        const int inset = 4;
        const QPixmap scaled = base.scaled(kPickerIconSize - inset * 2,
                                           kPickerIconSize - inset * 2,
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
        const int ox = (kPickerIconSize - scaled.width()) / 2;
        const int oy = (kPickerIconSize - scaled.height()) / 2;
        painter.drawPixmap(ox, oy, scaled);
    }

    const QColor mark(20, 110, 220);
    painter.setPen(QPen(mark, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(
        QRectF(1.5, 1.5, kPickerIconSize - 3.5, kPickerIconSize - 3.5), 4.0, 4.0);
    painter.end();
    return canvas;
}

ItemPickerTileDelegate::ItemPickerTileDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize ItemPickerTileDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(88, 96);
}
