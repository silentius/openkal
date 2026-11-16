/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/StarterKit.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/server/RandomItemStat.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>

namespace openkal::server {
    namespace {

        [[nodiscard]] const char* class_file(int clazz) {
            switch (clazz) {
            case 0:
                return "knight.json";
            case 1:
                return "mage.json";
            case 2:
                return "archer.json";
            case 3:
                return "thief.json";
            default:
                return nullptr;
            }
        }

        [[nodiscard]] bool json_as_bool(const platform::Json& value, bool fallback) {
            if (value.type() == platform::Json::Type::Bool) {
                return value.boolean();
            }
            if (value.is_number()) {
                return value.as_int(0) != 0;
            }
            if (value.is_string()) {
                const std::string text = value.as_string();
                return text == "true" || text == "1";
            }
            return fallback;
        }

        [[nodiscard]] int json_int_key(const platform::Json& row,
                                       const char* snake,
                                       const char* camel,
                                       int fallback) {
            const platform::Json* value = row.get(snake);
            if (!value) {
                value = row.get(camel);
            }
            if (!value) {
                return fallback;
            }
            return value->as_int(fallback);
        }

        [[nodiscard]] bool parse_item(const platform::Json& row, StarterItem& out) {
            out.item_id = json_int_key(row, "item_id", "ItemId", 0);
            out.prefix = json_int_key(row, "prefix", "Prefix", 0);
            out.count = json_int_key(row, "count", "Count", 1);
            const platform::Json* equipped = row.get("equipped");
            if (!equipped) {
                equipped = row.get("Equipped");
            }
            out.equipped = equipped ? json_as_bool(*equipped, false) : false;
            if (out.count <= 0) {
                out.count = 1;
            }
            return out.item_id > 0;
        }

        [[nodiscard]] std::vector<StarterItem> parse_items(const platform::Json& items) {
            std::vector<StarterItem> rows;
            if (!items.is_array()) {
                return rows;
            }
            rows.reserve(items.array().size());
            for (const auto& row : items.array()) {
                if (!row.is_object()) {
                    continue;
                }
                StarterItem item;
                if (!parse_item(row, item)) {
                    continue;
                }
                rows.push_back(item);
            }
            return rows;
        }

        [[nodiscard]] bool
        load_json_file(const std::string& path, platform::Json& root, bool required) {
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                if (required) {
                    log::warn("StarterKit: missing " + path);
                }
                return false;
            }
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("StarterKit: failed to parse " + path +
                          (err.empty() ? std::string{} : (": " + err)));
                return false;
            }
            return true;
        }

        [[nodiscard]] std::vector<StarterItem> load_items_file(const std::string& path,
                                                               bool required) {
            platform::Json root;
            if (!load_json_file(path, root, required)) {
                return {};
            }
            return parse_items(root.at("items"));
        }

        [[nodiscard]] bool parse_spawn(const platform::Json& root, int& x, int& y, int& z) {
            const platform::Json* spawn = root.get("spawn");
            if (!spawn) {
                return false;
            }
            if (spawn->is_array() && spawn->array().size() >= 2) {
                x = spawn->array()[0].as_int(x);
                y = spawn->array()[1].as_int(y);
                if (spawn->array().size() >= 3) {
                    z = spawn->array()[2].as_int(z);
                }
                return true;
            }
            if (spawn->is_object()) {
                x = spawn->at("x").as_int(x);
                y = spawn->at("y").as_int(y);
                z = spawn->at("z").as_int(z);
                return true;
            }
            return false;
        }

        [[nodiscard]] int equipped_slot(int item_id,
                                        const std::unordered_map<int, ItemCombatDef>& item_defs) {
            const auto it = item_defs.find(item_id);
            if (it == item_defs.end()) {
                return 1;
            }
            if (it->second.is_weapon) {
                return persist_slot_of(WearKind::Weapon);
            }
            if (it->second.is_transform) {
                return persist_slot_of(WearKind::Transform);
            }
            if (it->second.wearable) {
                return persist_slot_of(wear_kind_of(it->second));
            }
            return 0;
        }

    }  // namespace

    bool StarterKit::load_dir(const std::string& dir) {
        for (auto& rows : m_items) {
            rows.clear();
        }
        if (dir.empty() || !platform::is_directory(dir)) {
            return false;
        }
        m_has_spawn = false;
        std::vector<StarterItem> common;
        {
            platform::Json root;
            const std::string common_path = platform::join_path(dir, "common.json");
            if (load_json_file(common_path, root, false)) {
                common = parse_items(root.at("items"));
                m_has_spawn = parse_spawn(root, m_spawn_x, m_spawn_y, m_spawn_z);
            }
        }
        int loaded = common.empty() && !m_has_spawn ? 0 : 1;
        for (int clazz = 0; clazz < 4; ++clazz) {
            const char* file = class_file(clazz);
            if (!file) {
                continue;
            }
            const std::string path = platform::join_path(dir, file);
            std::vector<StarterItem> rows = load_items_file(path, true);
            if (!rows.empty()) {
                ++loaded;
            }
            std::vector<StarterItem> merged;
            merged.reserve(common.size() + rows.size());
            merged.insert(merged.end(), common.begin(), common.end());
            merged.insert(merged.end(), rows.begin(), rows.end());
            m_items[static_cast<std::size_t>(clazz)] = std::move(merged);
        }
        log::info("StarterKit: loaded " + std::to_string(loaded) + " tables from " + dir +
                  " (common=" + std::to_string(common.size()) + ")");
        return loaded > 0;
    }

    const std::vector<StarterItem>& StarterKit::items_for_class(int clazz) const {
        if (clazz < 0 || clazz > 3) {
            static const std::vector<StarterItem> kEmpty;
            return kEmpty;
        }
        return m_items[static_cast<std::size_t>(clazz)];
    }

    void StarterKit::apply_spawn(CharacterRecord& character) const {
        if (!m_has_spawn) {
            return;
        }
        character.map_x = m_spawn_x;
        character.map_y = m_spawn_y;
        character.map_z = m_spawn_z;
    }

    bool StarterKit::grant_if_empty(IDataStore& store,
                                    CharacterRecord& character,
                                    const std::unordered_map<int, ItemCombatDef>& item_defs,
                                    std::atomic<std::uint32_t>* next_iid) const {
        if (character.id <= 0) {
            return false;
        }
        if (!store.load_items(character.id).empty()) {
            return false;
        }
        const auto& rows = items_for_class(character.clazz);
        if (rows.empty() && !m_has_spawn) {
            return false;
        }
        CharacterRecord granted = character;
        apply_spawn(granted);
        std::atomic<std::uint32_t> local_seed{0};
        std::atomic<std::uint32_t>* seed = next_iid;
        if (!seed) {
            local_seed.store(store.max_item_iid(), std::memory_order_relaxed);
            seed = &local_seed;
        }
        bool wrote_item = false;
        std::vector<ItemRecord> items;
        for (const StarterItem& row : rows) {
            if (row.item_id == kMoneyItemIndex) {
                granted.money += row.count;
                wrote_item = true;
                continue;
            }
            ItemRecord rec;
            rec.iid = static_cast<std::int32_t>(allocate_item_iid(*seed));
            rec.character_id = character.id;
            rec.index = row.item_id;
            rec.prefix = std::max(0, row.prefix);
            rec.count = row.count;
            rec.character_bound = true;
            rec.slot = row.equipped ? equipped_slot(row.item_id, item_defs) : 0;
            const auto def = item_defs.find(row.item_id);
            if (def != item_defs.end()) {
                const RandomItemStatRoll roll = roll_random_item_stat(def->second);
                rec.random_stat_type = static_cast<int>(roll.type);
                rec.random_stat_value = static_cast<int>(roll.value);
            }
            items.push_back(rec);
            wrote_item = true;
            if (rec.slot == 1) {
                granted.weapon_index = rec.index;
            } else if (rec.slot == 2) {
                granted.armor_index = rec.index;
            } else if (rec.slot == 3) {
                granted.transform_index = rec.index;
            }
        }
        if (!wrote_item && !m_has_spawn) {
            return false;
        }
        if (wrote_item) {
            granted.next_iid = seed->load(std::memory_order_relaxed) + 1;
        }
        if (!store.grant_starter_items(granted, items)) {
            return false;
        }
        character = std::move(granted);
        return true;
    }

}  // namespace openkal::server
