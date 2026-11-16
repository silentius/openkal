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

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/** One monster record loaded from config/monsters JSON. */
struct MonsterRecord {
    /** Monster index. */
    int index = 0;

    /** Display name. */
    std::string name;

    /** Combat/display level from stats.level, falling back to appearance.level. */
    int level = 0;

    /** monster definition / JSON stats.race (see protocol::MonsterRace). */
    int race = 0;

    /** Outer itemgroup rolls: (itemgroup index, attempts per kill). */
    std::vector<std::pair<int, int>> item_groups;

    /** Source file path on disk. */
    std::string file_path;

    /** JSON text for round-trip export of untouched fields. */
    std::string raw_json;
};

/** Loads and updates monster JSON droplist references. */
class LootMonsterStore {
  public:
    /** Clears all loaded monsters. */
    void clear();

    /** Loads every monster JSON file from a directory.
     * @param dir Path to config/monsters.
     * @return Number of monsters loaded.
     */
    [[nodiscard]] int load_dir(const std::string& dir);

    /** Returns all monsters sorted by level then name.
     * @return Sorted monster list.
     */
    [[nodiscard]] std::vector<const MonsterRecord*> sorted_monsters() const;

    /** Finds a monster by index.
     * @param index Monster index.
     * @return Monster record, or nullptr.
     */
    [[nodiscard]] MonsterRecord* find(int index);

    /** Finds a monster by index.
     * @param index Monster index.
     * @return Monster record, or nullptr.
     */
    [[nodiscard]] const MonsterRecord* find(int index) const;

    /** Updates a monster's itemGroups and marks it dirty.
     * @param index Monster index.
     * @param item_groups New itemgroup assignments.
     * @return True when the monster exists.
     */
    [[nodiscard]] bool set_item_groups(int index, std::vector<std::pair<int, int>> item_groups);

    /** Returns indices of monsters with unsaved itemGroups changes.
     * @return Dirty monster indices.
     */
    [[nodiscard]] std::vector<int> dirty_monsters() const;

    /** Marks a monster clean after export.
     * @param index Monster index.
     */
    void mark_clean(int index);

    /** Writes dirty monster JSON files (stats.itemGroups only).
     * @return True when every write succeeded.
     */
    [[nodiscard]] bool export_dirty();

    /** Clears all dirty flags. */
    void clear_dirty();

  private:
    /** Monsters keyed by index. */
    std::unordered_map<int, MonsterRecord> m_monsters;

    /** Monster indices with pending itemGroups changes. */
    std::unordered_map<int, bool> m_dirty;
};
