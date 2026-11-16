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

#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QVector>

class DropGraphScene;

/** Draggable inner group node showing drop entry icons. */
class GroupNodeItem : public QGraphicsRectItem {
  public:
    /** Creates a group node with entry icon tiles.
     * @param def Group definition.
     * @param store Session data for names/tooltips.
     * @param icons Icon cache.
     * @param scene Owning scene for edge updates.
     */
    GroupNodeItem(const openkal::server::GroupDef& def,
                  LootDataStore* store,
                  ItemIconCache* icons,
                  DropGraphScene* scene);

    /** @return Group index. */
    [[nodiscard]] int group_index() const {
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

    /** Opens the group editor on double-click.
     * @param event Mouse event.
     */
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  private:
    /** Rebuilds icon tiles and labels from group data. */
    void rebuild_content();

    /** Group index. */
    int m_index = 0;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Icon cache. */
    ItemIconCache* m_icons = nullptr;

    /** Owning graph scene. */
    DropGraphScene* m_scene = nullptr;

    /** Cached definition copy for display. */
    openkal::server::GroupDef m_def;
};
