/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/NpcDefinitions.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace openkal::platform {
    std::optional<Json> resolve_npc_placement(const Json& definition, const Json* anchor) {
        const auto& placement = definition.at("placement");
        if (!placement.is_object()) {
            return definition;
        }
        const int dialog = placement.at("beside_dialog").as_int();
        const auto& offset = placement.at("offset");
        if (dialog <= 0 || !offset.is_array() || offset.array().size() != 3 || !anchor ||
            anchor->at("placement").is_object() || anchor->at("html").as_int() != dialog ||
            !anchor->at("xy").is_array() || anchor->at("xy").array().size() < 2) {
            return std::nullopt;
        }
        Json positioned = definition;
        for (const auto* key : {"map", "shape", "appearance"}) {
            positioned.object()[key] = anchor->at(key);
        }
        const auto& explicit_xy = definition.at("xy");
        const bool fixed_position = explicit_xy.is_array() && explicit_xy.array().size() == 3;
        Json xy = Json::parse("[0,0,0]");
        bool valid = true;
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& coordinates = anchor->at("xy").array();
            const long long value = fixed_position
                                        ? explicit_xy.array()[i].as_int()
                                        : (i < coordinates.size() ? coordinates[i].as_int() : 0LL) +
                                              static_cast<long long>(offset.array()[i].as_int());
            valid = valid && value >= std::numeric_limits<int>::min() &&
                    value <= std::numeric_limits<int>::max();
            xy.array()[i].set_number(static_cast<double>(value));
        }
        if (!valid) {
            return std::nullopt;
        }
        positioned.object()["xy"] = std::move(xy);
        const auto& facing = anchor->at("dir");
        if (facing.is_array() && facing.array().size() >= 2) {
            Json direction = facing;
            for (std::size_t i = 0; i < 2; ++i) {
                direction.array()[i].set_number(
                    static_cast<double>(facing.array()[i].as_int()) +
                    positioned.at("xy").array()[i].as_int() -
                    static_cast<double>(anchor->at("xy").array()[i].as_int()));
            }
            positioned.object()["dir"] = std::move(direction);
        }
        return positioned;
    }

    std::vector<Json> load_npc_definitions(const std::string& dir) {
        std::vector<Json> definitions;
        for (const auto& name : list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0 ||
                name == "bones.json" || name == "kinds.json" || name == "manifest.json") {
                continue;
            }
            const auto bytes = read_file(join_path(dir, name));
            Json root;
            if (bytes && json_parse(std::string(bytes->begin(), bytes->end()), root) &&
                root.is_object() && root.at("index").as_int() > 0) {
                definitions.push_back(std::move(root));
            }
        }
        std::sort(definitions.begin(), definitions.end(), [](const Json& a, const Json& b) {
            return a.at("index").as_int() < b.at("index").as_int();
        });
        std::vector<Json> resolved;
        for (const auto& definition : definitions) {
            if (!definition.at("placement").is_object()) {
                resolved.push_back(definition);
                continue;
            }
            const int dialog = definition.at("placement").at("beside_dialog").as_int();
            const auto anchor =
                std::find_if(definitions.begin(), definitions.end(), [dialog](const Json& row) {
                    return !row.at("placement").is_object() && row.at("html").as_int() == dialog;
                });
            if (auto positioned = resolve_npc_placement(
                    definition, anchor == definitions.end() ? nullptr : &*anchor)) {
                resolved.push_back(std::move(*positioned));
            }
        }
        return resolved;
    }
}  // namespace openkal::platform
