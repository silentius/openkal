/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PrefixDb.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

namespace openkal::client {
    namespace {

        void read_minmax(const platform::Json& node, int& min_v, int& max_v) {
            if (node.is_object()) {
                min_v = node.at("min").as_int(0);
                max_v = node.at("max").as_int(0);
            } else if (node.is_array() && node.array().size() >= 2) {
                min_v = node.array()[0].as_int(0);
                max_v = node.array()[1].as_int(0);
            } else if (node.is_number()) {
                min_v = node.as_int(0);
                max_v = min_v;
            }
        }

        PrefixDef prefix_from_json(const platform::Json& root) {
            PrefixDef def;
            def.set_index(root.at("index").as_int(0));
            def.set_name(root.at("name").as_string());
            def.set_level(root.at("level").as_int(0));
            def.set_sell(root.at("sell").as_int(0));
            def.set_str(root.at("str").as_int(0));
            def.set_hth(root.at("hth").as_int(0));
            def.set_intel(root.at("int").as_int(0));
            def.set_wis(root.at("wis").as_int(0));
            def.set_dex(root.at("dex").as_int(0));
            def.set_hp(root.at("hp").as_int(0));
            def.set_mp(root.at("mp").as_int(0));
            def.set_defense(root.at("defense").as_int(0));
            def.set_hit(root.at("hit").as_int(0));
            def.set_resist_fire(root.at("resistfire").as_int(0));
            def.set_resist_ice(root.at("resistice").as_int(0));
            def.set_resist_lightning(root.at("resistlitning").as_int(0));
            def.set_resist_curse(root.at("resistcurse").as_int(0));
            def.set_resist_palsy(root.at("resistpalsy").as_int(0));

            int amin = 0;
            int amax = 0;
            read_minmax(root.at("attack"), amin, amax);
            def.set_min_attack(amin);
            def.set_max_attack(amax);

            int mmin = 0;
            int mmax = 0;
            read_minmax(root.at("magic"), mmin, mmax);
            def.set_min_magic(mmin);
            def.set_max_magic(mmax);
            return def;
        }

    }  // namespace

    bool PrefixDb::load(const std::string& prefixes_dir) {
        m_by_index.clear();
        if (!platform::is_directory(prefixes_dir)) {
            return false;
        }
        for (const auto& rel : platform::list_files_recursive(prefixes_dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(prefixes_dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            PrefixDef def = prefix_from_json(root);
            if (def.index() > 0) {
                m_by_index[def.index()] = std::move(def);
            }
        }
        log::info("prefixes json loaded: " + std::to_string(m_by_index.size()) + " from " +
                  prefixes_dir);
        return !m_by_index.empty();
    }

    const PrefixDef* PrefixDb::find(std::int32_t index) const {
        auto it = m_by_index.find(index);
        return it == m_by_index.end() ? nullptr : &it->second;
    }

    std::string PrefixDb::name(std::int32_t index) const {
        const PrefixDef* def = find(index);
        if (!def) {
            return {};
        }
        return def->name();
    }

}  // namespace openkal::client
