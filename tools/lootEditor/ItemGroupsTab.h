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

#include <QWidget>
#include <memory>
#include <unordered_set>
#include <vector>

class GroupEntriesWidget;
class MatchedItemsBar;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/** Browse tab for all ItemGroups with nested Groups and entry preview. */
class ItemGroupsTab : public QWidget {
    Q_OBJECT

  public:
    /** Creates the ItemGroups browser tab.
     * @param store Shared session data.
     * @param parent Qt parent.
     */
    explicit ItemGroupsTab(LootDataStore* store, QWidget* parent = nullptr);

  public slots:
    /** Rebuilds lists from the store while preserving selection when possible. */
    void refresh();

  private slots:
    /** Opens the item picker to choose a search item. */
    void pick_search_item();

    /** Clears the item search filter. */
    void clear_search_item();

    /** Resolves the search field text into an item filter. */
    void apply_search_text();

    /** Handles ItemGroup list selection. */
    void on_itemgroup_selected();

    /** Handles Group list selection. */
    void on_group_selected();

    /** Opens ItemGroupEditorDialog for the selected ItemGroup. */
    void edit_selected_itemgroup();

    /** Opens GroupEditorDialog for the selected Group. */
    void edit_selected_group();

  private:
    /** Sets the active search items and refreshes filtered lists.
     * @param item_indices item definition indices, empty to clear.
     * @param sync_search_field When true, rewrite the search line edit.
     */
    void set_search_items(std::vector<int> item_indices, bool sync_search_field);

    /** Updates the search status label and matched-items bar. */
    void update_search_status();

    /** Selects the first group in the middle list that contains a search item. */
    void select_matching_group_if_possible();

    /** Formats an ItemGroup list label.
     * @param index ItemGroup index.
     * @return Display label.
     */
    [[nodiscard]] QString itemgroup_label(int index) const;

    /** Formats a Group list label with chance.
     * @param group_index Group index.
     * @param chance Chance percent from the parent ItemGroup entry.
     * @return Display label.
     */
    [[nodiscard]] QString group_entry_label(int group_index, float chance) const;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** HyperText icon cache. */
    std::unique_ptr<ItemIconCache> m_icons;

    /** Active search items (empty = no filter). */
    std::vector<int> m_search_items;

    /** Highlight set mirroring runtime state. */
    std::unordered_set<int> m_search_item_set;

    /** Item search field. */
    QLineEdit* m_search = nullptr;

    /** Matched items with ItemPicker-style hover tooltips. */
    MatchedItemsBar* m_matched_items = nullptr;

    /** Search status line. */
    QLabel* m_status = nullptr;

    /** Pick search item button. */
    QPushButton* m_pick_btn = nullptr;

    /** Clear search button. */
    QPushButton* m_clear_btn = nullptr;

    /** Edit ItemGroup button. */
    QPushButton* m_edit_ig_btn = nullptr;

    /** Edit Group button. */
    QPushButton* m_edit_group_btn = nullptr;

    /** ItemGroups list. */
    QListWidget* m_itemgroups = nullptr;

    /** Groups belonging to the selected ItemGroup. */
    QListWidget* m_groups = nullptr;

    /** Entries for the selected Group. */
    GroupEntriesWidget* m_entries = nullptr;
};
