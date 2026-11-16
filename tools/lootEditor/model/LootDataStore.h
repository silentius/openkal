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

#include "model/LootMonsterStore.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/PrefixDb.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

/** Session paths for loot editor data roots. */
struct LootEditorPaths {
    /** Repository root. */
    std::string repo_root;

    /** Root containing the external groups, itemgroups, and monsters directories. */
    std::string loot_config_root;

    /** Explicit config/groups override. Leave empty to use loot_config_root/groups. */
    std::string groups_dir;

    /** Explicit config/itemgroups override. Leave empty to use loot_config_root/itemgroups. */
    std::string itemgroups_dir;

    /** Explicit config/monsters override. Leave empty to use loot_config_root/monsters. */
    std::string monsters_dir;

    /** Client data root (HyperText). */
    std::string data_root;

    /** When true, data_root is not restored from QSettings on startup. */
    bool data_root_locked = false;

    /** When true, loot_config_root is not restored from QSettings on startup. */
    bool loot_config_root_locked = false;
};

/** Central editable loot-table session shared across tabs. */
class LootDataStore : public QObject {
    Q_OBJECT

  public:
    /** Constructs an empty store.
     * @param parent Qt parent object.
     */
    explicit LootDataStore(QObject* parent = nullptr);

    /** Loads all JSON tables and the item catalog.
     * @param paths Data root paths.
     * @return True when groups and itemgroups loaded.
     */
    [[nodiscard]] bool load(const LootEditorPaths& paths);

    /** Returns configured paths.
     * @return Current path set.
     */
    [[nodiscard]] const LootEditorPaths& paths() const {
        return m_paths;
    }

    /** Returns the drop table database.
     * @return Item/group database.
     */
    [[nodiscard]] openkal::server::ItemGroupDb& drop_db() {
        return m_drop_db;
    }

    /** Returns the drop table database.
     * @return Item/group database.
     */
    [[nodiscard]] const openkal::server::ItemGroupDb& drop_db() const {
        return m_drop_db;
    }

    /** Returns the drop roller bound to the database.
     * @return Drop roller.
     */
    [[nodiscard]] const openkal::server::DropRoller& roller() const {
        return *m_roller;
    }

    /** Returns the item catalog.
     * @return Item database.
     */
    [[nodiscard]] const openkal::client::ItemDb& items() const {
        return m_items;
    }

    /** Returns the prefix catalog.
     * @return Prefix database.
     */
    [[nodiscard]] const openkal::client::PrefixDb& prefixes() const {
        return m_items.prefixes();
    }

    /** Returns the monster index.
     * @return Monster store.
     */
    [[nodiscard]] LootMonsterStore& monsters() {
        return m_monsters;
    }

    /** Returns the monster index.
     * @return Monster store.
     */
    [[nodiscard]] const LootMonsterStore& monsters() const {
        return m_monsters;
    }

    /** HyperText directory from the loaded item catalog.
     * @return HyperText path.
     */
    [[nodiscard]] std::string hypertext_dir() const {
        return m_items.hypertext_dir();
    }

    /** Selected monster index, or -1 when none.
     * @return Monster index.
     */
    [[nodiscard]] int selected_monster() const {
        return m_selected_monster;
    }

    /** Sets the selected monster and emits selectionChanged.
     * @param index Monster index, or -1.
     */
    void set_selected_monster(int index);

    /** Marks a group dirty after edit.
     * @param index Group index.
     */
    void mark_group_dirty(int index);

    /** Marks an itemgroup dirty after edit.
     * @param index Itemgroup index.
     */
    void mark_itemgroup_dirty(int index);

    /** Returns whether any unsaved changes exist.
     * @return True when dirty.
     */
    [[nodiscard]] bool is_dirty() const;

    /** Writes all dirty groups, itemgroups, and monsters.
     * @return True when every write succeeded.
     */
    [[nodiscard]] bool export_dirty();

    /** Clears dirty flags without writing. */
    void clear_dirty();

    /** Returns dirty group indices.
     * @return Sorted indices.
     */
    [[nodiscard]] std::vector<int> dirty_groups() const;

    /** Returns dirty itemgroup indices.
     * @return Sorted indices.
     */
    [[nodiscard]] std::vector<int> dirty_itemgroups() const;

    /** Upserts a group and marks it dirty.
     * @param def Group definition.
     */
    void upsert_group(openkal::server::GroupDef def);

    /** Upserts an itemgroup and marks it dirty.
     * @param def Itemgroup definition.
     */
    void upsert_itemgroup(openkal::server::ItemGroupDef def);

    /** Removes a group when unreferenced.
     * @param index Group index.
     * @return True when removed.
     */
    [[nodiscard]] bool remove_group(int index);

    /** Removes an itemgroup.
     * @param index Itemgroup index.
     * @return True when removed.
     */
    [[nodiscard]] bool remove_itemgroup(int index);

    /** Updates monster itemGroups.
     * @param index Monster index.
     * @param item_groups Itemgroup assignments.
     * @return True when updated.
     */
    [[nodiscard]] bool set_monster_item_groups(int index,
                                               std::vector<std::pair<int, int>> item_groups);

    /** Resolves an item display name.
     * @param item_index item definition index.
     * @return Display name.
     */
    [[nodiscard]] QString item_name(int item_index) const;

    /** Resolves a prefix display name.
     * @param prefix_id Prefix index.
     * @return Prefix name, or empty.
     */
    [[nodiscard]] QString prefix_name(int prefix_id) const;

    /** Returns truncated prefix label for compact graph overlays.
     * @param prefix_id Prefix index.
     * @param max_chars Maximum characters including ellipsis.
     * @return Short prefix name, or empty when none.
     */
    [[nodiscard]] QString prefix_short_label(int prefix_id, int max_chars = 12) const;

    /** Builds a rich tooltip for an item (name, stats, description).
     * @param item_index item definition index.
     * @return Multi-line tooltip text.
     */
    [[nodiscard]] QString item_tooltip(int item_index) const;

    /** Builds compact overlay lines for a changeprefix talisman.
     * @param item_index item definition index.
     * @param max_name_chars Truncation length for each prefix name.
     * @return One line per roll outcome ("Name rate%"), or empty when not a prefix talisman.
     */
    [[nodiscard]] QStringList changeprefix_overlay_lines(int item_index,
                                                         int max_name_chars = 10) const;

    /** Finds groups that contain a given item definition index.
     * @param item_index item definition index.
     * @return Matching group indices sorted ascending.
     */
    [[nodiscard]] std::vector<int> groups_containing_item(int item_index) const;

    /** Finds itemgroups that can drop a given item definition via nested groups.
     * @param item_index item definition index.
     * @return Matching itemgroup indices sorted ascending.
     */
    [[nodiscard]] std::vector<int> itemgroups_containing_item(int item_index) const;

    /** Finds groups that contain any of the given item definition indices.
     * @param item_indices item definition indices.
     * @return Matching group indices sorted ascending.
     */
    [[nodiscard]] std::vector<int>
    groups_containing_any(const std::vector<int>& item_indices) const;

    /** Finds itemgroups that can drop any of the given item definitions via nested groups.
     * @param item_indices item definition indices.
     * @return Matching itemgroup indices sorted ascending.
     */
    [[nodiscard]] std::vector<int>
    itemgroups_containing_any(const std::vector<int>& item_indices) const;

  signals:
    /** Emitted when the selected monster changes. */
    void selectionChanged(int monster_index);

    /** Emitted after any table or monster edit. */
    void dataChanged();

    /** Emitted when dirty state changes. */
    void dirtyChanged(bool dirty);

  private:
    /** Rebuilds the drop roller after database edits. */
    void refresh_roller();

    /** Emits dirtyChanged when needed. */
    void notify_dirty();

    /** Configured paths. */
    LootEditorPaths m_paths;

    /** Drop tables. */
    openkal::server::ItemGroupDb m_drop_db;

    /** Drop roller (rebuilt on edits). */
    std::unique_ptr<openkal::server::DropRoller> m_roller;

    /** Item catalog. */
    openkal::client::ItemDb m_items;

    /** Monster index. */
    LootMonsterStore m_monsters;

    /** Selected monster index. */
    int m_selected_monster = -1;

    /** Dirty group indices. */
    std::unordered_set<int> m_dirty_groups;

    /** Dirty itemgroup indices. */
    std::unordered_set<int> m_dirty_itemgroups;

    /** Group indices removed and pending JSON file deletion on save. */
    std::unordered_set<int> m_deleted_groups;

    /** Itemgroup indices removed and pending JSON file deletion on save. */
    std::unordered_set<int> m_deleted_itemgroups;
};
