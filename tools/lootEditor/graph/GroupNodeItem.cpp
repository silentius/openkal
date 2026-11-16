/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "graph/GroupNodeItem.h"

#include "graph/DropGraphScene.h"

#include <QBrush>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QTimer>

namespace {

    constexpr qreal kWidth = 220.0;
    constexpr qreal kHeight = 220.0;
    constexpr int kMaxTiles = 8;

    /** Adds a readable prefix badge on top of an item icon tile.
     * @param parent Group node item.
     * @param x Tile x position.
     * @param y Tile y position.
     * @param text Prefix label text.
     */
    void add_prefix_badge(QGraphicsItem* parent, qreal x, qreal y, const QString& text) {
        if (text.isEmpty()) {
            return;
        }
        auto* shadow = new QGraphicsSimpleTextItem(text, parent);
        shadow->setPos(x + 2, y + 2);
        QFont sf = shadow->font();
        sf.setPointSize(6);
        sf.setBold(true);
        shadow->setFont(sf);
        shadow->setBrush(QColor(20, 20, 20));
        shadow->setZValue(3);

        auto* badge = new QGraphicsSimpleTextItem(text, parent);
        badge->setPos(x + 1, y + 1);
        QFont bf = badge->font();
        bf.setPointSize(6);
        bf.setBold(true);
        badge->setFont(bf);
        badge->setBrush(QColor(255, 230, 120));
        badge->setZValue(4);
    }

}  // namespace

GroupNodeItem::GroupNodeItem(const openkal::server::GroupDef& def,
                             LootDataStore* store,
                             ItemIconCache* icons,
                             DropGraphScene* scene)
    : QGraphicsRectItem(0, 0, kWidth, kHeight), m_index(def.index), m_store(store), m_icons(icons),
      m_scene(scene), m_def(def) {
    setBrush(QBrush(QColor(50, 80, 50)));
    setPen(QPen(QColor(100, 180, 100), 2.0));
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setZValue(1.0);
    rebuild_content();
}

QPointF GroupNodeItem::center_scene() const {
    return sceneBoundingRect().center();
}

QVariant GroupNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged && m_scene && scene() != nullptr) {
        m_scene->update_edges();
    }
    return QGraphicsRectItem::itemChange(change, value);
}

void GroupNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    event->accept();
    DropGraphScene* scene = m_scene;
    const int index = m_index;
    if (!scene) {
        return;
    }
    // Defer dialog open: rebuild() inside show_group_usage can destroy this item;
    // calling the base mouseDoubleClickEvent afterward would use freed memory.
    QTimer::singleShot(0, scene, [scene, index]() { scene->show_group_usage(index); });
}

void GroupNodeItem::rebuild_content() {
    for (QGraphicsItem* child : childItems()) {
        delete child;
    }

    QString title = QString("#%1").arg(m_index);
    if (!m_def.name.empty()) {
        title += " " + QString::fromStdString(m_def.name);
    }
    auto* header = new QGraphicsSimpleTextItem(title, this);
    header->setPos(8, 6);
    QFont f = header->font();
    f.setBold(true);
    f.setPointSize(9);
    header->setFont(f);
    header->setBrush(Qt::white);

    const int shown = std::min(kMaxTiles, static_cast<int>(m_def.entries.size()));
    for (int i = 0; i < shown; ++i) {
        const auto& e = m_def.entries[static_cast<std::size_t>(i)];
        const int col = i % 4;
        const int row = i / 4;
        const qreal x = 10 + col * 52;
        const qreal y = 34 + row * 52;

        QPixmap px;
        QString tip;
        QString overlay;
        if (e.kind == openkal::server::GroupEntry::Kind::Money) {
            px = m_icons->money_icon();
            overlay = QString("$%1").arg(e.amount);
            tip = QString("Money\nAmount: %1\nChance: %2%").arg(e.amount).arg(e.chance, 0, 'f', 2);
        } else {
            px = m_icons->icon(e.item_index);
            overlay = QString("×%1\n%2%").arg(e.count).arg(e.chance, 0, 'f', 1);
            if (e.prefix != 0) {
                const QString pfx = m_store->prefix_short_label(e.prefix, 10);
                overlay = pfx + "\n" + overlay;
            }
            tip = m_store->item_tooltip(e.item_index);
            tip += QString("\nCount: %1\nChance: %2%").arg(e.count).arg(e.chance, 0, 'f', 2);
            if (e.prefix != 0) {
                tip += "\nPrefix: " + m_store->prefix_name(e.prefix);
            }
        }

        auto* tile = new QGraphicsPixmapItem(px, this);
        tile->setPos(x, y);
        tile->setToolTip(tip);

        if (e.kind == openkal::server::GroupEntry::Kind::Item && e.prefix != 0) {
            add_prefix_badge(this, x, y, m_store->prefix_short_label(e.prefix, 9));
        }

        auto* label = new QGraphicsSimpleTextItem(overlay, this);
        label->setPos(x, y + 28);
        QFont lf = label->font();
        lf.setPointSize(7);
        label->setFont(lf);
        label->setBrush(QColor(230, 230, 200));
    }

    if (static_cast<int>(m_def.entries.size()) > kMaxTiles) {
        const int more = static_cast<int>(m_def.entries.size()) - kMaxTiles;
        auto* extra = new QGraphicsSimpleTextItem(QString("+%1 more").arg(more), this);
        extra->setPos(8, kHeight - 22);
        extra->setBrush(QColor(180, 180, 160));
    }
}
