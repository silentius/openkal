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

#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QString>

class DropEdgeItem : public QGraphicsLineItem {
  public:
    /** Creates an edge between two node centers with a chance label.
     * @param chance_pct Drop chance label text source.
     * @param parent Scene parent item.
     */
    explicit DropEdgeItem(float chance_pct, QGraphicsItem* parent = nullptr);

    /** Creates an edge with a custom label, optionally dashed.
     * @param label Edge caption.
     * @param implied True for a same-index group hint when the itemgroup JSON is missing.
     * @param parent Scene parent item.
     */
    DropEdgeItem(const QString& label, bool implied, QGraphicsItem* parent = nullptr);

    /** Updates line endpoints when nodes move.
     * @param from Source center in scene coordinates.
     * @param to Target center in scene coordinates.
     */
    void set_endpoints(const QPointF& from, const QPointF& to);

  private:
    /** Chance label centered on the edge. */
    QGraphicsSimpleTextItem* m_label = nullptr;
};
