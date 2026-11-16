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

#include <QDialog>
#include <map>
#include <unordered_set>
#include <vector>

class QLabel;
class QGroupBox;
class QLineEdit;
class QListView;
class QModelIndex;
class QScrollArea;
class QTabBar;
class QVBoxLayout;
class QWidget;

/** Modal icon grid of items, grouped into class/type tabs. */
class ItemPickerDialog : public QDialog {
    Q_OBJECT

  public:
    /** Opens an item picker.
     * @param store Session store for names and tooltips.
     * @param icons Icon cache for HyperText BMPs.
     * @param initial_index Pre-selected item index, or <=0 for none.
     * @param occupied_items Item indices already used in the active group (blue mark).
     * @param parent Qt parent.
     */
    ItemPickerDialog(LootDataStore* store,
                     ItemIconCache* icons,
                     int initial_index,
                     std::unordered_set<int> occupied_items = {},
                     QWidget* parent = nullptr);

    /** @return First selected item definition index, or -1 when none. */
    [[nodiscard]] int selected_index() const;

    /** @return All selected item definition indices in stable order. */
    [[nodiscard]] std::vector<int> selected_indices() const {
        return m_selected;
    }

  private slots:
    /** Rebuilds the visible catalog from search and active tab. */
    void refresh_list();

    /** Accepts the highlighted list item(s). */
    void accept_selection();

  private:
    /** Catalog tab categories for the picker. */
    enum class Tab : int {
        All = 0,
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

    /** Builds the sorted catalog used for filtering.
     * @return Item indices sorted by display name.
     */
    [[nodiscard]] std::vector<int> build_catalog() const;

    /** Returns true when an item belongs on the active tab and matches search.
     * @param item_index item definition index.
     * @param filter Search text.
     * @param tab Active catalog tab.
     * @return True when the item should be shown.
     */
    [[nodiscard]] bool passes_filter(int item_index, const QString& filter, Tab tab) const;

    /** Maps an item to its primary picker tab (excluding All).
     * @param item_index item definition index.
     * @return Preferred tab for highlighting an existing selection.
     */
    [[nodiscard]] Tab primary_tab_for(int item_index) const;

    /** Reports whether the tab uses grade group boxes.
     * @param tab Catalog tab.
     * @return True for Knight/Mage/Archer/Thief.
     */
    [[nodiscard]] static bool is_class_tab(Tab tab);

    /** Replaces the flat model's ordered IDs for non-class tabs.
     * @param filter Search text.
     * @param tab Active tab.
     * @return Number of shown items.
     */
    [[nodiscard]] int rebuild_flat_list(const QString& filter, Tab tab);

    /** Updates grade-grouped models, reusing existing views where grades survive.
     * @param filter Search text.
     * @param tab Active class tab.
     * @return Number of shown items.
     */
    [[nodiscard]] int rebuild_grade_groups(const QString& filter, Tab tab);

    /** Creates a configured icon list widget.
     * @param parent Qt parent.
     * @return New icon-mode list.
     */
    [[nodiscard]] QListView* make_icon_list(QWidget* parent);

    /** Wires selection signals for one icon list.
     * @param list Target list.
     */
    void wire_list(QListView* list);

    /** Selects an item index in the current view when present.
     * @param item_index item definition index.
     */
    void select_item_in_view(int item_index);

    /** Clears selection in every icon list except one.
     * @param keep List that keeps its selection, or nullptr to clear all.
     */
    void clear_selection_except(QListView* keep);

    /** Reads selected indices from all visible icon lists into runtime state. */
    void sync_selected_from_views();

    /** Updates the status line with shown / selected counts. */
    void update_status(int shown);

    /** Handles a click in an icon list (Ctrl keeps multi across grade boxes).
     * @param list Source list.
     * @param item Clicked tile.
     */
    void on_item_clicked(QListView* list, const QModelIndex& item);

    /** Gear sort key: grade, then weapon/defense, then name.
     * @param a Left item definition index.
     * @param b Right item definition index.
     * @return True when a should sort before b.
     */
    [[nodiscard]] bool gear_less(int a, int b) const;

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Icon cache. */
    ItemIconCache* m_icons = nullptr;

    /** Selected item indices (stable order). */
    std::vector<int> m_selected;

    /** True while rebuilding views and restoring their selection. */
    bool m_restoring_selection = false;

    /** Search field. */
    QLineEdit* m_search = nullptr;

    /** Category tab bar. */
    QTabBar* m_tabs = nullptr;

    /** Flat icon grid for non-class tabs. */
    QListView* m_list = nullptr;

    /** Scroll host for grade group boxes. */
    QScrollArea* m_grade_scroll = nullptr;

    /** Container inside the grade scroll area. */
    QWidget* m_grade_host = nullptr;

    /** Vertical layout of grade group boxes. */
    QVBoxLayout* m_grade_layout = nullptr;

    /** Non-owning handles to the view and its Qt-owned grade box. */
    struct GradeGroup {
        QGroupBox* box = nullptr;
        QListView* list = nullptr;
    };

    /** Current grade groups only; absent grades are removed on refresh. */
    std::map<int, GradeGroup> m_grade_groups;

    /** Status line (shown count). */
    QLabel* m_status = nullptr;

    /** Full sorted catalog. */
    std::vector<int> m_catalog;

    /** Item indices already present in the active group. */
    std::unordered_set<int> m_occupied_items;
};
