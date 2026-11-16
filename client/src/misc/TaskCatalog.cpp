/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TaskCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

namespace openkal::client {

    bool TaskCatalog::load_dir(const std::string& dir) {
        m_by_key.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
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
                continue;
            }
            TaskDef def;
            def.key = static_cast<std::uint16_t>(root.at("key").as_int(0));
            def.name = root.at("name").as_string();
            def.desc = root.at("desc").as_string();
            def.npc_index = root.at("npcIndex").as_int(0);
            const platform::Json& flags = root.at("flags");
            if (flags.is_array()) {
                for (const auto& fj : flags.array()) {
                    if (!fj.is_object()) {
                        continue;
                    }
                    TaskFlagDef flag;
                    flag.key = static_cast<std::uint8_t>(fj.at("key").as_int(0));
                    flag.desc = fj.at("desc").as_string();
                    flag.level = fj.at("level").as_int(0);
                    const platform::Json& item = fj.at("item");
                    if (item.is_array() && item.array().size() >= 3) {
                        flag.item_kind = item.array()[0].as_int(0);
                        flag.item_index = item.array()[1].as_int(0);
                        flag.item_count = item.array()[2].as_int(0);
                    }
                    flag.npc_desc = fj.at("npcDesc").as_string();
                    flag.npc_index = fj.at("npcIndex").as_int(0);
                    const platform::Json& xy = fj.at("xy");
                    if (xy.is_array() && xy.array().size() >= 2) {
                        flag.x = xy.array()[0].as_int(0);
                        flag.y = xy.array()[1].as_int(0);
                    }
                    def.flags.push_back(std::move(flag));
                }
            }
            if (def.key != 0) {
                m_by_key[def.key] = std::move(def);
            }
        }
        log::info("TaskCatalog: loaded " + std::to_string(m_by_key.size()) + " tasks");
        return !m_by_key.empty();
    }

    const TaskDef* TaskCatalog::find(std::uint16_t key) const {
        const auto it = m_by_key.find(key);
        return it == m_by_key.end() ? nullptr : &it->second;
    }

}  // namespace openkal::client
