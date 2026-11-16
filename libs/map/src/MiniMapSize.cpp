/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/MiniMapSize.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <string>
#include <utility>

namespace openkal::map {
    namespace {

        [[nodiscard]] bool parse_size_object(const platform::Json& obj, MiniMapSize& out) {
            if (!obj.is_object()) {
                return false;
            }
            out = MiniMapSize{};
            const platform::Json* mapname = obj.get("mapname");
            if (mapname == nullptr || !mapname->is_string() || mapname->as_string().empty()) {
                return false;
            }
            out.set_mapname(mapname->as_string());
            if (const platform::Json* size_x = obj.get("sizeX"); size_x != nullptr) {
                out.set_size_x(size_x->as_int(out.size_x()));
            }
            if (const platform::Json* size_y = obj.get("sizeY"); size_y != nullptr) {
                out.set_size_y(size_y->as_int(out.size_y()));
            }
            if (const platform::Json* tile_w = obj.get("tileWidth"); tile_w != nullptr) {
                out.set_tile_width(tile_w->as_int(out.tile_width()));
            }
            if (const platform::Json* tile_h = obj.get("tileHeight"); tile_h != nullptr) {
                out.set_tile_height(tile_h->as_int(out.tile_height()));
            }
            if (const platform::Json* map_left = obj.get("mapLeft"); map_left != nullptr) {
                out.set_map_left(map_left->as_int(out.map_left()));
            }
            if (const platform::Json* map_bottom = obj.get("mapBottom"); map_bottom != nullptr) {
                out.set_map_bottom(map_bottom->as_int(out.map_bottom()));
            }
            if (const platform::Json* correction = obj.get("correction"); correction != nullptr) {
                out.set_correction(correction->as_int(out.correction()));
            }
            return out.size_x() > 0 && out.size_y() > 0 && out.tile_width() > 0 &&
                   out.tile_height() > 0;
        }

    }  // namespace

    MiniMapSize MiniMapSize::dungeon_defaults(std::string mapname) {
        MiniMapSize size;
        size.set_mapname(std::move(mapname));
        return size;
    }

    void MiniMapSizeCatalog::clear() {
        m_entries.clear();
    }

    bool MiniMapSizeCatalog::load_json(std::string_view path) {
        m_entries.clear();
        const std::string path_str(path);
        const auto bytes = platform::File::read(path_str);
        if (!bytes) {
            log::warn("MiniMapSizeCatalog::load_json: missing " + path_str);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool MiniMapSizeCatalog::load_json_text(std::string_view text) {
        m_entries.clear();
        platform::Json root;
        std::string err;
        if (!platform::json_parse(std::string(text), root, &err)) {
            log::warn("MiniMapSizeCatalog::load_json_text: " + err);
            return false;
        }
        const platform::Json* array = nullptr;
        if (root.is_array()) {
            array = &root;
        } else if (root.is_object()) {
            const platform::Json& maps = root.at("maps");
            if (maps.is_array()) {
                array = &maps;
            }
        }
        if (array == nullptr) {
            return false;
        }
        for (const auto& item : array->array()) {
            MiniMapSize rec;
            if (!parse_size_object(item, rec)) {
                continue;
            }
            m_entries.push_back(std::move(rec));
        }
        return !m_entries.empty();
    }

    const MiniMapSize* MiniMapSizeCatalog::find(std::string_view mapname) const {
        for (const auto& rec : m_entries) {
            if (rec.mapname() == mapname) {
                return &rec;
            }
        }
        return nullptr;
    }

}  // namespace openkal::map
