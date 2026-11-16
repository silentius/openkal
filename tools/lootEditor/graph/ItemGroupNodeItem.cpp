/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "graph/ItemGroupNodeItem.h"

#include "graph/DropGraphScene.h"

#include <QBrush>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QTimer>

namespace {
    constexpr qreal kWidth = 200.0;
    constexpr qreal kHeight = 120.0;
}  // namespace

ItemGroupNodeItem::ItemGroupNodeItem(int index,
                                     int attempts,
                                     const openkal::server::ItemGroupDef& def,
                                     DropGraphScene* scene,
                                     bool missing)
    : QGraphicsRectItem(0, 0, kWidth, kHeight), m_index(index), m_scene(scene) {
    if (missing) {
        setBrush(QBrush(QColor(110, 45, 45)));
        setPen(QPen(QColor(200, 110, 110), 2.0));
    } else {
        setBrush(QBrush(QColor(45, 70, 110)));
        setPen(QPen(QColor(100, 140, 200), 2.0));
    }
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(1.0);

    QString title = QString("#%1").arg(index);
    if (!def.name.empty()) {
        title += "\n" + QString::fromStdString(def.name);
    }
    title += QString("\n(×%1)").arg(attempts);
    if (missing) {
        title += "\n(missing JSON)";
    }

    auto* label = new QGraphicsSimpleTextItem(title, this);
    label->setPos(8, 8);
    QFont f = label->font();
    f.setPointSize(9);
    label->setFont(f);
    label->setBrush(Qt::white);

    qreal y = missing ? 72.0 : 52.0;
    for (const auto& e : def.entries) {
        auto* row = new QGraphicsSimpleTextItem(
            QString("→ group %1  %2%").arg(e.group_index).arg(e.chance, 0, 'f', 2), this);
        row->setPos(8, y);
        row->setBrush(Qt::white);
        y += 16;
        if (y > kHeight - 12) {
            break;
        }
    }
}

QPointF ItemGroupNodeItem::center_scene() const {
    return sceneBoundingRect().center();
}

QVariant ItemGroupNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged && m_scene && scene() != nullptr) {
        m_scene->update_edges();
    }
    return QGraphicsRectItem::itemChange(change, value);
}

void ItemGroupNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    event->accept();
    DropGraphScene* scene = m_scene;
    const int index = m_index;
    if (!scene) {
        return;
    }
    QTimer::singleShot(0, scene, [scene, index]() { scene->edit_itemgroup(index); });
}
