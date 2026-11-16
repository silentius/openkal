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

#include "openkal/server/world/ItemGroupDb.h"

#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QVector>

class DropGraphScene;

/** Draggable outer itemgroup node in the drop graph. */
class ItemGroupNodeItem : public QGraphicsRectItem {
  public:
    /** Creates an itemgroup node.
     * @param index Itemgroup index.
     * @param attempts Rolls per kill from monster config.
     * @param def Itemgroup definition (may be a stub when missing).
     * @param scene Owning scene for edge updates.
     * @param missing True when no itemgroup JSON exists for this index.
     */
    ItemGroupNodeItem(int index,
                      int attempts,
                      const openkal::server::ItemGroupDef& def,
                      DropGraphScene* scene,
                      bool missing = false);

    /** @return Itemgroup index. */
    [[nodiscard]] int itemgroup_index() const {
        return m_index;
    }

    /** @return Scene center point for edge drawing. */
    [[nodiscard]] QPointF center_scene() const;

  protected:
    /** Updates connected edges after drag.
     * @param change Item change type.
     * @param value New value.
     * @return Updated property.
     */
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    /** Opens the itemgroup editor on double-click.
     * @param event Mouse event.
     */
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  private:
    /** Itemgroup index. */
    int m_index = 0;

    /** Owning graph scene. */
    DropGraphScene* m_scene = nullptr;
};
