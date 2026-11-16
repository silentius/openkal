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

#include <QMainWindow>
#include <QString>
#include <vector>

namespace openkal::client {
    class ItemDb;
    class ItemDef;
    class PrefixDef;
}  // namespace openkal::client

class ItemIconCache;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QScrollArea;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

/** Standalone browser for the loaded item catalog. */
class ItemViewerWindow final : public QMainWindow {
  public:
    /** Creates the item browser.
     * @param catalog Loaded item catalog.
     * @param icons Icon cache associated with catalog.
     * @param parent Optional Qt parent.
     */
    ItemViewerWindow(const openkal::client::ItemDb& catalog,
                     ItemIconCache& icons,
                     QWidget* parent = nullptr);

  private:
    /** Catalog tabs matching the Loot Editor item picker. */
    enum class Tab {
        All,
        Knight,
        Mage,
        Archer,
        Thief,
        Quest,
        Charm,
        Consumable,
        Ornament,
        Pet,
        Other,
    };

    /** Rebuilds the visible icon grid from the current search string. */
    void rebuild_item_grid();

    /** Builds grade groups for the active class tab. */
    void rebuild_grade_groups(int selected_index);

    /** Adds an item tile to a grid.
     * @param grid Destination icon grid.
     * @param item_index item definition index.
     */
    void add_item_tile(QListWidget* grid, int item_index);

    /** Returns the currently selected index from either visible item view.
     * @return Item index, or -1 when no item is selected.
     */
    [[nodiscard]] int current_item_index() const;

    /** Checks whether an item belongs to a tab and matches the search input.
     * @param item_index item definition index.
     * @param filter Search input.
     * @param tab Active catalog tab.
     * @return True when the item is visible.
     */
    [[nodiscard]] bool passes_filter(int item_index, const QString& filter, Tab tab) const;

    /** Returns whether a tab uses grouped grade rows.
     * @param tab Catalog tab.
     * @return True for a player-class tab.
     */
    [[nodiscard]] static bool is_class_tab(Tab tab);

    /** Shows every available item field in the right-hand detail pane.
     * @param item_index item definition index to show.
     */
    void show_item_details(int item_index);

    /** Appends a name/value row to the detail table.
     * @param name Field name.
     * @param value Field value.
     * @return Created table row.
     */
    QTreeWidgetItem* add_field(const QString& name, const QString& value);

    /** Appends an expandable full prefix definition.
     * @param label Prefix roll label.
     * @param prefix_id Prefix index to resolve.
     */
    void add_prefix_details(const QString& label, int prefix_id);

    /** Formats a resolved prefix index and display name.
     * @param prefix_id Prefix index.
     * @return Prefix display text.
     */
    [[nodiscard]] QString prefix_label(int prefix_id) const;

    /** Formats a true/false value for the information table.
     * @param value Boolean value.
     * @return Human-readable value.
     */
    [[nodiscard]] static QString yes_no(bool value);

    /** Item definitions backing the browser. */
    const openkal::client::ItemDb& m_catalog;

    /** HyperText icon cache backing the browser. */
    ItemIconCache& m_icons;

    /** All non-money item indices in sorted order. */
    std::vector<int> m_indices;

    /** Search field above the icon grid. */
    QLineEdit* m_search = nullptr;

    /** Tabs that classify the catalog like the Loot Editor picker. */
    QTabBar* m_tabs = nullptr;

    /** Left-hand selectable item grid. */
    QListWidget* m_item_grid = nullptr;

    /** Scrollable grade groups for player-class tabs. */
    QScrollArea* m_grade_scroll = nullptr;

    /** Host widget for dynamic grade groups. */
    QWidget* m_grade_host = nullptr;

    /** Layout that owns dynamic grade groups. */
    QVBoxLayout* m_grade_layout = nullptr;

    /** Item grids currently shown in grade groups. */
    std::vector<QListWidget*> m_grade_grids;

    /** Status text below the search field. */
    QLabel* m_status = nullptr;

    /** Selected item's icon. */
    QLabel* m_preview = nullptr;

    /** Selected item's title. */
    QLabel* m_title = nullptr;

    /** Name/value fields for the selected item. */
    QTreeWidget* m_details = nullptr;

    /** Description section label. */
    QLabel* m_description_label = nullptr;

    /** Full item description without truncation. */
    QPlainTextEdit* m_description = nullptr;
};
