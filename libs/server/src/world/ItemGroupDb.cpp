/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/ItemGroupDb.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace openkal::server {
    namespace {

        [[nodiscard]] int chance_to_units(float chance_pct) {
            if (chance_pct <= 0.0f) {
                return 0;
            }
            const int units =
                static_cast<int>(std::lround(static_cast<double>(chance_pct) * 100.0));
            return std::clamp(units, 0, 10000);
        }

        [[nodiscard]] bool parse_group_json(const platform::Json& root, GroupDef& out) {
            out.index = root.at("index").as_int(0);
            if (out.index <= 0) {
                return false;
            }
            out.name = root.at("name").as_string();
            out.entries.clear();

            const platform::Json& entries = root.at("entries");
            if (!entries.is_array()) {
                return true;
            }
            for (const auto& e : entries.array()) {
                if (!e.is_object()) {
                    continue;
                }
                GroupEntry row;
                row.chance = e.at("chance").as_float(0.0f);
                if (chance_to_units(row.chance) <= 0) {
                    continue;
                }
                const std::string type = e.at("type").as_string("item");
                if (type == "money") {
                    row.kind = GroupEntry::Kind::Money;
                    row.amount = e.at("amount").as_int(0);
                    if (row.amount <= 0) {
                        continue;
                    }
                } else {
                    row.kind = GroupEntry::Kind::Item;
                    row.item_index = e.at("item").as_int(0);
                    row.prefix = e.at("prefix").as_int(0);
                    row.count = e.at("count").as_int(1);
                    if (row.item_index <= 0) {
                        continue;
                    }
                    if (row.count <= 0) {
                        row.count = 1;
                    }
                }
                out.entries.push_back(row);
            }
            return true;
        }

        [[nodiscard]] bool parse_itemgroup_json(const platform::Json& root, ItemGroupDef& out) {
            out.index = root.at("index").as_int(0);
            if (out.index <= 0) {
                return false;
            }
            out.name = root.at("name").as_string();
            out.entries.clear();

            const platform::Json& entries = root.at("entries");
            if (!entries.is_array()) {
                return true;
            }
            for (const auto& e : entries.array()) {
                if (!e.is_object()) {
                    continue;
                }
                ItemGroupEntry row;
                row.chance = e.at("chance").as_float(0.0f);
                row.group_index = e.at("group").as_int(0);
                if (chance_to_units(row.chance) <= 0 || row.group_index <= 0) {
                    continue;
                }
                out.entries.push_back(row);
            }
            return true;
        }

        [[nodiscard]] std::string table_filename(int index) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%04d.json", index);
            return buf;
        }

        [[nodiscard]] platform::Json group_to_json(const GroupDef& def) {
            platform::Json root;
            root.set_type(platform::Json::Type::Object);
            root.object()["index"].set_number(def.index);
            if (!def.name.empty()) {
                root.object()["name"].set_type(platform::Json::Type::String);
                root.object()["name"].string() = def.name;
            }
            platform::Json& entries = root.object()["entries"];
            entries.set_type(platform::Json::Type::Array);
            for (const auto& e : def.entries) {
                platform::Json row;
                row.set_type(platform::Json::Type::Object);
                if (e.kind == GroupEntry::Kind::Money) {
                    row.object()["type"].set_type(platform::Json::Type::String);
                    row.object()["type"].string() = "money";
                    row.object()["amount"].set_number(e.amount);
                } else {
                    row.object()["type"].set_type(platform::Json::Type::String);
                    row.object()["type"].string() = "item";
                    row.object()["item"].set_number(e.item_index);
                    row.object()["prefix"].set_number(e.prefix);
                    if (e.count != 1) {
                        row.object()["count"].set_number(e.count);
                    }
                }
                row.object()["chance"].set_number(static_cast<double>(e.chance));
                entries.array().push_back(std::move(row));
            }
            return root;
        }

        [[nodiscard]] platform::Json itemgroup_to_json(const ItemGroupDef& def) {
            platform::Json root;
            root.set_type(platform::Json::Type::Object);
            root.object()["index"].set_number(def.index);
            if (!def.name.empty()) {
                root.object()["name"].set_type(platform::Json::Type::String);
                root.object()["name"].string() = def.name;
            }
            platform::Json& entries = root.object()["entries"];
            entries.set_type(platform::Json::Type::Array);
            for (const auto& e : def.entries) {
                platform::Json row;
                row.set_type(platform::Json::Type::Object);
                row.object()["group"].set_number(e.group_index);
                row.object()["chance"].set_number(static_cast<double>(e.chance));
                entries.array().push_back(std::move(row));
            }
            return root;
        }

    }  // namespace

    void ItemGroupDb::clear() {
        m_groups.clear();
        m_itemgroups.clear();
    }

    bool ItemGroupDb::load_groups_dir(const std::string& dir) {
        m_groups.clear();
        if (!platform::is_directory(dir)) {
            log::warn("ItemGroupDb: groups dir missing " + dir);
            return false;
        }

        int loaded = 0;
        for (const auto& name : platform::list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("ItemGroupDb: bad group json " + path + ": " + err);
                continue;
            }
            GroupDef def;
            if (!parse_group_json(root, def)) {
                continue;
            }
            m_groups[def.index] = std::move(def);
            ++loaded;
        }

        log::info("ItemGroupDb: loaded " + std::to_string(loaded) + " groups from " + dir);
        return loaded > 0;
    }

    bool ItemGroupDb::load_itemgroups_dir(const std::string& dir) {
        m_itemgroups.clear();
        if (!platform::is_directory(dir)) {
            log::warn("ItemGroupDb: itemgroups dir missing " + dir);
            return false;
        }

        int loaded = 0;
        int missing_refs = 0;
        for (const auto& name : platform::list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("ItemGroupDb: bad itemgroup json " + path + ": " + err);
                continue;
            }
            ItemGroupDef def;
            if (!parse_itemgroup_json(root, def)) {
                continue;
            }
            for (const auto& e : def.entries) {
                if (m_groups.find(e.group_index) == m_groups.end()) {
                    ++missing_refs;
                }
            }
            m_itemgroups[def.index] = std::move(def);
            ++loaded;
        }

        if (missing_refs > 0) {
            log::warn("ItemGroupDb: " + std::to_string(missing_refs) +
                      " itemgroup entries reference missing groups");
        }
        const auto global_it = m_itemgroups.find(kGlobalNormalMonsterItemGroupIndex);
        if (global_it == m_itemgroups.end()) {
            log::error("ItemGroupDb: reserved global itemgroup " +
                       std::to_string(kGlobalNormalMonsterItemGroupIndex) +
                       " not loaded (invalid or missing JSON)");
        } else if (global_it->second.entries.empty()) {
            log::warn("ItemGroupDb: reserved global itemgroup " +
                      std::to_string(kGlobalNormalMonsterItemGroupIndex) +
                      " has no entries; normal-monster extra drops are disabled");
        }
        log::info("ItemGroupDb: loaded " + std::to_string(loaded) + " itemgroups from " + dir);
        return loaded > 0;
    }

    const GroupDef* ItemGroupDb::find_group(int index) const {
        const auto it = m_groups.find(index);
        return it == m_groups.end() ? nullptr : &it->second;
    }

    const ItemGroupDef* ItemGroupDb::find_itemgroup(int index) const {
        const auto it = m_itemgroups.find(index);
        return it == m_itemgroups.end() ? nullptr : &it->second;
    }

    GroupDef* ItemGroupDb::find_group_mut(int index) {
        const auto it = m_groups.find(index);
        return it == m_groups.end() ? nullptr : &it->second;
    }

    ItemGroupDef* ItemGroupDb::find_itemgroup_mut(int index) {
        const auto it = m_itemgroups.find(index);
        return it == m_itemgroups.end() ? nullptr : &it->second;
    }

    void ItemGroupDb::upsert_group(GroupDef def) {
        if (def.index <= 0) {
            return;
        }
        m_groups[def.index] = std::move(def);
    }

    bool ItemGroupDb::remove_group(int index) {
        return m_groups.erase(index) > 0;
    }

    void ItemGroupDb::upsert_itemgroup(ItemGroupDef def) {
        if (def.index <= 0) {
            return;
        }
        m_itemgroups[def.index] = std::move(def);
    }

    bool ItemGroupDb::remove_itemgroup(int index) {
        return m_itemgroups.erase(index) > 0;
    }

    bool ItemGroupDb::save_group(const std::string& dir, const GroupDef& def) {
        if (def.index <= 0 || !platform::is_directory(dir)) {
            return false;
        }
        const std::string path = platform::join_path(dir, table_filename(def.index));
        const std::string text = platform::json_stringify(group_to_json(def), 2) + "\n";
        return platform::write_file(path, text);
    }

    bool ItemGroupDb::save_itemgroup(const std::string& dir, const ItemGroupDef& def) {
        if (def.index <= 0 || !platform::is_directory(dir)) {
            return false;
        }
        const std::string path = platform::join_path(dir, table_filename(def.index));
        const std::string text = platform::json_stringify(itemgroup_to_json(def), 2) + "\n";
        return platform::write_file(path, text);
    }

    int ItemGroupDb::next_free_group_index() const {
        int max_idx = 0;
        for (const auto& [idx, _] : m_groups) {
            max_idx = std::max(max_idx, idx);
        }
        return max_idx + 1;
    }

    int ItemGroupDb::next_free_itemgroup_index() const {
        int max_idx = 0;
        for (const auto& [idx, _] : m_itemgroups) {
            if (idx >= kGlobalNormalMonsterItemGroupIndex) {
                continue;
            }
            max_idx = std::max(max_idx, idx);
        }
        return max_idx + 1;
    }

}  // namespace openkal::server
