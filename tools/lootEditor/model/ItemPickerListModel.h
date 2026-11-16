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

#include <QAbstractListModel>
#include <QPixmap>
#include <QString>
#include <QStyledItemDelegate>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ItemIconCache;
class LootDataStore;

/** Ordered item IDs with lazily resolved presentation roles for one picker refresh. */
class ItemPickerListModel final : public QAbstractListModel {
  public:
    /** Borrows the picker session objects and copies the stable occupied-ID set.
     * The store and icon cache must outlive this model.
     * @param store Source of current item metadata.
     * @param icons Caller-owned base icon cache.
     * @param occupied_items Stable set of IDs already used by the active group.
     * @param parent Qt owner of this model.
     */
    ItemPickerListModel(const LootDataStore& store,
                        const ItemIconCache& icons,
                        const std::unordered_set<int>& occupied_items,
                        QObject* parent = nullptr);

    /** Replaces the visible order and invalidates all lazily resolved roles.
     * @param indices Item IDs in their required display order.
     */
    void replace_items(std::vector<int> indices);

    /** Reports the number of visible items.
     * @param parent Parent index; valid parents have no children.
     * @return Number of top-level rows, or zero for a valid parent.
     */
    int rowCount(const QModelIndex& parent = {}) const override;

    /** Resolves a role on first request against the current session, without retaining definitions.
     * Cached roles remain valid until replace_items is called.
     * @param index Requested model row.
     * @param role Qt presentation or item-ID role.
     * @return Role value, or an invalid variant for an invalid row or unsupported role.
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /** Reports the read-only selection capabilities of a row.
     * @param index Requested model row.
     * @return Enabled/selectable flags for a valid row, otherwise no flags.
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

  private:
    /** Only roles actually requested during the current refresh occupy presentation storage. */
    struct Presentation {
        std::optional<QString> label;
        std::optional<QString> tooltip;
        std::optional<QPixmap> icon;
    };

    [[nodiscard]] QPixmap compose_icon(int item_index) const;
    [[nodiscard]] bool contains(const QModelIndex& index) const;

    const LootDataStore& m_store;
    const ItemIconCache& m_icons;
    const std::unordered_set<int> m_occupied_items;
    std::vector<int> m_indices;
    mutable std::unordered_map<int, Presentation> m_presentation;
};

/** Keeps tile sizing independent of decoration and tooltip role resolution. */
class ItemPickerTileDelegate final : public QStyledItemDelegate {
  public:
    /** Constructs the fixed-size tile delegate.
     * @param parent Qt owner of this delegate.
     */
    explicit ItemPickerTileDelegate(QObject* parent = nullptr);

    /** Returns the existing picker tile size without fetching model roles.
     * @param option Current view style options.
     * @param index Item being laid out.
     * @return The 88 by 96 logical-pixel tile size.
     */
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
