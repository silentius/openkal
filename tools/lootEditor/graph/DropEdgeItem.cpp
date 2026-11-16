/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "graph/DropEdgeItem.h"

#include <QPen>

DropEdgeItem::DropEdgeItem(float chance_pct, QGraphicsItem* parent)
    : DropEdgeItem(QString("%1%").arg(chance_pct, 0, 'f', 2), false, parent) {}

DropEdgeItem::DropEdgeItem(const QString& label, bool implied, QGraphicsItem* parent)
    : QGraphicsLineItem(parent) {
    QPen pen(implied ? QColor(200, 130, 130) : QColor(140, 160, 200), 2.0);
    if (implied) {
        pen.setStyle(Qt::DashLine);
    }
    setPen(pen);
    setZValue(-1.0);
    m_label = new QGraphicsSimpleTextItem(label, this);
    m_label->setBrush(implied ? QColor(140, 40, 40) : Qt::black);
}

void DropEdgeItem::set_endpoints(const QPointF& from, const QPointF& to) {
    setLine(QLineF(from, to));
    const QPointF mid = (from + to) * 0.5;
    const QRectF bounds = m_label->boundingRect();
    m_label->setPos(mid.x() - bounds.width() * 0.5, mid.y() - bounds.height() * 0.5);
}
