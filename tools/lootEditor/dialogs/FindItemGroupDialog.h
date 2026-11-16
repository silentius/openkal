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
#include <memory>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;

/** Finds loot groups that contain a picked item and adds one to an itemgroup. */
class FindItemGroupDialog : public QDialog {
    Q_OBJECT

  public:
    /** Opens the find-item-in-groups dialog.
     * @param store Session store.
     * @param preferred_itemgroup Preferred target itemgroup, or <=0 for none.
     * @param parent Qt parent.
     */
    FindItemGroupDialog(LootDataStore* store, int preferred_itemgroup, QWidget* parent = nullptr);

    /** @return True when a group was appended to an itemgroup. */
    [[nodiscard]] bool did_add() const {
        return m_did_add;
    }

  private slots:
    /** Opens the item picker and refreshes matching groups. */
    void pick_item();

    /** Rebuilds the group list for the current item. */
    void refresh_matches();

    /** Shows details for the highlighted matching group. */
    void on_group_selected();

    /** Appends the selected group to the chosen itemgroup. */
    void add_to_itemgroup();

  private:
    /** Collects groups that contain runtime state.
     * @return Matching group indices sorted ascending.
     */
    [[nodiscard]] std::vector<int> matching_groups() const;

    /** Formats a short summary of a group's entries.
     * @param group_index Group index.
     * @return Human-readable summary.
     */
    [[nodiscard]] QString group_summary(int group_index) const;

    /** Fills the detail tree for one group.
     * @param group_index Group index.
     */
    void populate_details(int group_index);

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Local icon cache (independent of the graph tab rebuild). */
    std::unique_ptr<ItemIconCache> m_icons;

    /** Currently searched item index, or <=0. */
    int m_item_index = 0;

    /** True after a successful add. */
    bool m_did_add = false;

    /** Item label / pick button row. */
    QLabel* m_item_icon = nullptr;
    QLabel* m_item_name = nullptr;
    QPushButton* m_pick_item = nullptr;

    /** Matching groups list. */
    QListWidget* m_groups = nullptr;

    /** Selected group contents. */
    QTreeWidget* m_details = nullptr;

    /** Status line. */
    QLabel* m_status = nullptr;

    /** Target itemgroup combo. */
    QComboBox* m_target_ig = nullptr;

    /** Chance used when appending the group ref. */
    QDoubleSpinBox* m_chance = nullptr;

    /** Add action button. */
    QPushButton* m_add = nullptr;
};
