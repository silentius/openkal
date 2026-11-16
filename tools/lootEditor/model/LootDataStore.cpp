/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "model/LootDataStore.h"

#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

LootDataStore::LootDataStore(QObject* parent) : QObject(parent) {
    m_roller = std::make_unique<openkal::server::DropRoller>(m_drop_db);
}

bool LootDataStore::load(const LootEditorPaths& paths) {
    m_paths = paths;
    if (m_paths.groups_dir.empty()) {
        m_paths.groups_dir = openkal::platform::join_path(m_paths.loot_config_root, "groups");
    }
    if (m_paths.itemgroups_dir.empty()) {
        m_paths.itemgroups_dir =
            openkal::platform::join_path(m_paths.loot_config_root, "itemgroups");
    }
    if (m_paths.monsters_dir.empty()) {
        m_paths.monsters_dir = openkal::platform::join_path(m_paths.loot_config_root, "monsters");
    }
    m_dirty_groups.clear();
    m_dirty_itemgroups.clear();
    m_deleted_groups.clear();
    m_deleted_itemgroups.clear();
    m_selected_monster = -1;

    const bool groups_ok = m_drop_db.load_groups_dir(m_paths.groups_dir);
    const bool ig_ok = m_drop_db.load_itemgroups_dir(m_paths.itemgroups_dir);
    (void) m_monsters.load_dir(m_paths.monsters_dir);

    if (!m_items.load(
            openkal::platform::join_path(m_paths.repo_root, "config"), m_paths.data_root, {})) {
        openkal::log::warn("LootDataStore: item catalog load failed");
    }

    refresh_roller();
    emit dataChanged();
    emit dirtyChanged(false);
    return groups_ok && ig_ok;
}

void LootDataStore::set_selected_monster(int index) {
    if (m_selected_monster == index) {
        return;
    }
    m_selected_monster = index;
    emit selectionChanged(index);
}

void LootDataStore::mark_group_dirty(int index) {
    if (index > 0) {
        m_dirty_groups.insert(index);
        notify_dirty();
    }
}

void LootDataStore::mark_itemgroup_dirty(int index) {
    if (index > 0) {
        m_dirty_itemgroups.insert(index);
        notify_dirty();
    }
}

bool LootDataStore::is_dirty() const {
    return !m_dirty_groups.empty() || !m_dirty_itemgroups.empty() || !m_deleted_groups.empty() ||
           !m_deleted_itemgroups.empty() || !m_monsters.dirty_monsters().empty();
}

namespace {
    /** Builds the on-disk table filename for a group/itemgroup index.
     * @param index Table index.
     * @return Filename such as 0042.json.
     */
    [[nodiscard]] std::string table_filename(int index) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d.json", index);
        return buf;
    }

    /** Deletes a group/itemgroup JSON file when present.
     * @param dir Config directory.
     * @param index Table index.
     * @return True when absent or successfully removed.
     */
    [[nodiscard]] bool delete_table_file(const std::string& dir, int index) {
        if (dir.empty() || index <= 0) {
            return false;
        }
        const std::filesystem::path path = std::filesystem::path(dir) / table_filename(index);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return true;
        }
        return std::filesystem::remove(path, ec);
    }
}  // namespace

bool LootDataStore::export_dirty() {
    bool ok = true;
    for (int idx : dirty_groups()) {
        const openkal::server::GroupDef* def = m_drop_db.find_group(idx);
        if (!def) {
            continue;
        }
        if (!openkal::server::ItemGroupDb::save_group(m_paths.groups_dir, *def)) {
            ok = false;
            continue;
        }
        m_dirty_groups.erase(idx);
    }
    for (int idx : dirty_itemgroups()) {
        const openkal::server::ItemGroupDef* def = m_drop_db.find_itemgroup(idx);
        if (!def) {
            continue;
        }
        if (!openkal::server::ItemGroupDb::save_itemgroup(m_paths.itemgroups_dir, *def)) {
            ok = false;
            continue;
        }
        m_dirty_itemgroups.erase(idx);
    }
    for (int idx : std::vector<int>(m_deleted_groups.begin(), m_deleted_groups.end())) {
        if (!delete_table_file(m_paths.groups_dir, idx)) {
            ok = false;
            continue;
        }
        m_deleted_groups.erase(idx);
    }
    for (int idx : std::vector<int>(m_deleted_itemgroups.begin(), m_deleted_itemgroups.end())) {
        if (!delete_table_file(m_paths.itemgroups_dir, idx)) {
            ok = false;
            continue;
        }
        m_deleted_itemgroups.erase(idx);
    }
    if (!m_monsters.export_dirty()) {
        ok = false;
    }
    notify_dirty();
    return ok;
}

void LootDataStore::clear_dirty() {
    m_dirty_groups.clear();
    m_dirty_itemgroups.clear();
    m_deleted_groups.clear();
    m_deleted_itemgroups.clear();
    m_monsters.clear_dirty();
    notify_dirty();
}

std::vector<int> LootDataStore::dirty_groups() const {
    return std::vector<int>(m_dirty_groups.begin(), m_dirty_groups.end());
}

std::vector<int> LootDataStore::dirty_itemgroups() const {
    return std::vector<int>(m_dirty_itemgroups.begin(), m_dirty_itemgroups.end());
}

void LootDataStore::upsert_group(openkal::server::GroupDef def) {
    const int idx = def.index;
    m_drop_db.upsert_group(std::move(def));
    m_deleted_groups.erase(idx);
    mark_group_dirty(idx);
    refresh_roller();
    emit dataChanged();
}

void LootDataStore::upsert_itemgroup(openkal::server::ItemGroupDef def) {
    const int idx = def.index;
    m_drop_db.upsert_itemgroup(std::move(def));
    m_deleted_itemgroups.erase(idx);
    mark_itemgroup_dirty(idx);
    refresh_roller();
    emit dataChanged();
}

bool LootDataStore::remove_group(int index) {
    if (!m_drop_db.remove_group(index)) {
        return false;
    }
    m_dirty_groups.erase(index);
    m_deleted_groups.insert(index);
    refresh_roller();
    emit dataChanged();
    notify_dirty();
    return true;
}

bool LootDataStore::remove_itemgroup(int index) {
    if (!m_drop_db.remove_itemgroup(index)) {
        return false;
    }
    m_dirty_itemgroups.erase(index);
    m_deleted_itemgroups.insert(index);
    refresh_roller();
    emit dataChanged();
    notify_dirty();
    return true;
}

bool LootDataStore::set_monster_item_groups(int index,
                                            std::vector<std::pair<int, int>> item_groups) {
    if (!m_monsters.set_item_groups(index, std::move(item_groups))) {
        return false;
    }
    emit dataChanged();
    notify_dirty();
    return true;
}

QString LootDataStore::item_name(int item_index) const {
    const openkal::client::ItemDef* def = m_items.find(item_index);
    if (!def || def->name().empty()) {
        return QString("item %1").arg(item_index);
    }
    return QString::fromStdString(def->name());
}

QString LootDataStore::prefix_name(int prefix_id) const {
    if (prefix_id <= 0) {
        return {};
    }
    const openkal::client::PrefixDef* def = m_items.prefixes().find(prefix_id);
    if (!def || def->name().empty()) {
        return QString("prefix %1").arg(prefix_id);
    }
    return QString::fromStdString(def->name());
}

QString LootDataStore::prefix_short_label(int prefix_id, int max_chars) const {
    if (prefix_id <= 0 || max_chars <= 0) {
        return {};
    }
    QString label = prefix_name(prefix_id);
    if (label.size() > max_chars) {
        label = label.left(max_chars - 1) + QChar(0x2026);
    }
    return label;
}

namespace {
    /** Appends changeprefix target and outcome lines to a tooltip.
     * @param tip Tooltip being built.
     * @param def Item definition with changeprefix data.
     * @param store Store used for prefix names.
     */
    void append_changeprefix_tooltip(QString& tip,
                                     const openkal::client::ItemDef& def,
                                     const LootDataStore& store) {
        if (!def.has_changeprefix()) {
            return;
        }

        tip += "\n\nPrefix talisman";
        const std::string& target = def.changeprefix_target();
        if (target == "weapon") {
            tip += "\nEffective on: Weapon";
        } else if (target == "defense") {
            tip += "\nEffective on: Defense";
        } else if (target == "ornament") {
            tip += "\nEffective on: Ornament";
        } else if (!target.empty()) {
            tip += "\nEffective on: " + QString::fromStdString(target);
        }

        const auto append_option = [&](int prefix_id, int shown_rate) {
            if (prefix_id == 0) {
                return;
            }
            QString name = store.prefix_name(prefix_id);
            if (name.isEmpty()) {
                name = QString("Prefix #%1").arg(prefix_id);
            }
            tip += QString("\n%1 — %2%").arg(name).arg(shown_rate);
        };

        append_option(def.changeprefix_prefix0(), def.changeprefix_rate0());
        if (def.changeprefix_prefix1() != 0) {
            const int shown = def.changeprefix_rate1() - def.changeprefix_rate0();
            append_option(def.changeprefix_prefix1(), shown);
        }
    }
}  // namespace

QString LootDataStore::item_tooltip(int item_index) const {
    const openkal::client::ItemDef* def = m_items.find(item_index);
    if (!def) {
        return QString("Unknown item #%1").arg(item_index);
    }

    QString tip = item_name(item_index);
    tip += QString("\n#%1").arg(item_index);

    if (!def->class_big().empty()) {
        tip += "\n" + QString::fromStdString(def->class_big());
        if (!def->class_small().empty()) {
            tip += " / " + QString::fromStdString(def->class_small());
        }
    } else if (!def->clazz().empty()) {
        tip += "\n" + QString::fromStdString(def->clazz());
    }

    if (def->level_limit() > 0) {
        tip += QString("\nRequires level %1").arg(def->level_limit());
    }
    if (def->class_limit() >= 0) {
        static const char* kClasses[] = {"Knight", "Mage", "Archer", "Thief"};
        if (def->class_limit() >= 0 && def->class_limit() < 4) {
            tip += QString("\nClass: %1").arg(kClasses[def->class_limit()]);
        }
    }

    if (def->min_attack() > 0 || def->max_attack() > 0) {
        tip += QString("\nAttack: %1–%2").arg(def->min_attack()).arg(def->max_attack());
    }
    if (def->defense() > 0) {
        tip += QString("\nDefense: %1").arg(def->defense());
    }
    if (def->min_magic() > 0 || def->max_magic() > 0) {
        tip += QString("\nMagic: %1–%2").arg(def->min_magic()).arg(def->max_magic());
    }

    append_changeprefix_tooltip(tip, *def, *this);

    if (!def->description().empty()) {
        QString desc = QString::fromStdString(def->description());
        desc.replace('#', '\n');
        if (desc.size() > 200) {
            desc = desc.left(198) + QChar(0x2026);
        }
        tip += "\n\n" + desc;
    }

    return tip;
}

QStringList LootDataStore::changeprefix_overlay_lines(int item_index, int max_name_chars) const {
    QStringList lines;
    const openkal::client::ItemDef* def = m_items.find(item_index);
    if (!def || !def->has_changeprefix()) {
        return lines;
    }

    const auto append_option = [&](int prefix_id, int shown_rate) {
        if (prefix_id == 0) {
            return;
        }
        QString name = prefix_short_label(prefix_id, max_name_chars);
        if (name.isEmpty()) {
            name = QString("#%1").arg(prefix_id);
        }
        lines << QString("%1 %2%").arg(name).arg(shown_rate);
    };

    append_option(def->changeprefix_prefix0(), def->changeprefix_rate0());
    if (def->changeprefix_prefix1() != 0) {
        const int shown = def->changeprefix_rate1() - def->changeprefix_rate0();
        append_option(def->changeprefix_prefix1(), shown);
    }
    return lines;
}

std::vector<int> LootDataStore::groups_containing_item(int item_index) const {
    std::vector<int> matches;
    if (item_index <= 0) {
        return matches;
    }
    for (const auto& [idx, def] : m_drop_db.groups()) {
        for (const auto& e : def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Item && e.item_index == item_index) {
                matches.push_back(idx);
                break;
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::vector<int> LootDataStore::itemgroups_containing_item(int item_index) const {
    std::vector<int> matches;
    if (item_index <= 0) {
        return matches;
    }

    const std::vector<int> groups = groups_containing_item(item_index);
    if (groups.empty()) {
        return matches;
    }

    std::unordered_set<int> group_set(groups.begin(), groups.end());
    for (const auto& [ig_index, ig] : m_drop_db.itemgroups()) {
        for (const auto& e : ig.entries) {
            if (group_set.count(e.group_index) > 0) {
                matches.push_back(ig_index);
                break;
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::vector<int> LootDataStore::groups_containing_any(const std::vector<int>& item_indices) const {
    std::vector<int> matches;
    if (item_indices.empty()) {
        return matches;
    }

    std::unordered_set<int> wanted(item_indices.begin(), item_indices.end());
    wanted.erase(0);
    if (wanted.empty()) {
        return matches;
    }

    for (const auto& [idx, def] : m_drop_db.groups()) {
        for (const auto& e : def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Item &&
                wanted.count(e.item_index) > 0) {
                matches.push_back(idx);
                break;
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::vector<int>
LootDataStore::itemgroups_containing_any(const std::vector<int>& item_indices) const {
    std::vector<int> matches;
    const std::vector<int> groups = groups_containing_any(item_indices);
    if (groups.empty()) {
        return matches;
    }

    std::unordered_set<int> group_set(groups.begin(), groups.end());
    for (const auto& [ig_index, ig] : m_drop_db.itemgroups()) {
        for (const auto& e : ig.entries) {
            if (group_set.count(e.group_index) > 0) {
                matches.push_back(ig_index);
                break;
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

void LootDataStore::refresh_roller() {
    m_roller = std::make_unique<openkal::server::DropRoller>(m_drop_db);
}

void LootDataStore::notify_dirty() {
    emit dirtyChanged(is_dirty());
}
