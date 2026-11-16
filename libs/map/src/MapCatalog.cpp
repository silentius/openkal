/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/MapCatalog.h"

#include "openkal/map/Coords.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <string>

namespace openkal::map {
    namespace {

        [[nodiscard]] bool parse_json_cell(const platform::Json& obj, MapInfo& out) {
            if (!obj.is_object()) {
                return false;
            }
            out = MapInfo{};
            out.set_index(obj.at("index").as_int(0));
            out.set_kind(obj.at("kind").as_int(0));
            if (const platform::Json* x = obj.get("x"); x != nullptr) {
                out.set_x(x->as_int(0));
            }
            if (const platform::Json* y = obj.get("y"); y != nullptr) {
                out.set_y(y->as_int(0));
            }
            if (const platform::Json* xy = obj.get("xy");
                xy != nullptr && xy->is_array() && xy->array().size() >= 2) {
                out.set_x(xy->array()[0].as_int(out.x()));
                out.set_y(xy->array()[1].as_int(out.y()));
            }
            out.set_size_x(obj.at("sizeX").as_int(0));
            out.set_size_y(obj.at("sizeY").as_int(0));
            out.set_fog_end(obj.at("fogEnd").as_int(0));
            if (const platform::Json* fog = obj.get("fogColor");
                fog != nullptr && fog->is_array() && fog->array().size() >= 3) {
                out.set_fog_r(fog->array()[0].as_float(0.0f));
                out.set_fog_g(fog->array()[1].as_float(0.0f));
                out.set_fog_b(fog->array()[2].as_float(0.0f));
            }
            if (const platform::Json* pos = obj.get("initPos");
                pos != nullptr && pos->is_array() && pos->array().size() >= 2) {
                out.set_init_pos_x(pos->array()[0].as_int(0));
                out.set_init_pos_y(pos->array()[1].as_int(0));
            }
            if (const platform::Json* mapname = obj.get("mapname");
                mapname != nullptr && mapname->is_string()) {
                out.set_mapname(mapname->as_string());
            }
            if (const platform::Json* filename = obj.get("filename");
                filename != nullptr && filename->is_string()) {
                out.set_filename(filename->as_string());
            }
            return true;
        }

    }  // namespace

    void MapCatalog::clear() {
        m_entries.clear();
    }

    bool MapCatalog::load(std::string_view path) {
        return load_json(path);
    }

    bool MapCatalog::load_json(std::string_view path) {
        m_entries.clear();
        const std::string path_str(path);
        const auto bytes = platform::File::read(path_str);
        if (!bytes) {
            log::warn("MapCatalog::load_json: missing " + path_str);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool MapCatalog::load_json_text(std::string_view text) {
        m_entries.clear();
        platform::Json root;
        std::string err;
        if (!platform::json_parse(std::string(text), root, &err)) {
            log::warn("MapCatalog::load_json_text: " + err);
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
            MapInfo info;
            if (!parse_json_cell(item, info)) {
                continue;
            }
            m_entries.push_back(std::move(info));
        }
        log::info("MapCatalog::load_json: " + std::to_string(m_entries.size()) + " entries");
        return !m_entries.empty();
    }

    const MapInfo* MapCatalog::find_cell(int index, int cell_x, int cell_y) const {
        for (const auto& e : m_entries) {
            if (e.index() == index && e.x() == cell_x && e.y() == cell_y) {
                return &e;
            }
        }
        return nullptr;
    }

    const MapInfo* MapCatalog::find_index(int index) const {
        for (const auto& e : m_entries) {
            if (e.index() == index) {
                return &e;
            }
        }
        return nullptr;
    }

    const MapInfo* MapCatalog::find_named_cell(int cell_x, int cell_y) const {
        for (const auto& e : m_entries) {
            if (e.x() == cell_x && e.y() == cell_y && !e.mapname().empty()) {
                return &e;
            }
        }
        return nullptr;
    }

    int MapCatalog::index_at_pixel(int pixel_x, int pixel_y) const {
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(pixel_x, pixel_y, tile_x, tile_y);
        int cell_x = 0;
        int cell_y = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_tx, local_ty);
        const MapInfo* info = find_named_cell(cell_x, cell_y);
        return info != nullptr ? info->index() : 0;
    }

    std::optional<int> MapCatalog::cell_index_at_pixel(int pixel_x, int pixel_y) const {
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(pixel_x, pixel_y, tile_x, tile_y);
        int cell_x = 0;
        int cell_y = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_tx, local_ty);
        for (const auto& entry : m_entries) {
            if (entry.x() == cell_x && entry.y() == cell_y) {
                return entry.index();
            }
        }
        return std::nullopt;
    }

    int MapCatalog::resolve_teleport_map(int pixel_x, int pixel_y, int fallback) const {
        const int named = index_at_pixel(pixel_x, pixel_y);
        return named > 0 ? named : fallback;
    }

    bool MapCatalog::has_indoor_mapname(int index) const {
        if (index <= 0) {
            return false;
        }
        const MapInfo* info = find_index(index);
        return info != nullptr && !info->mapname().empty();
    }

    bool parse_map_catalog(const std::string& path, std::vector<MapInfo>& out) {
        MapCatalog catalog;
        if (!catalog.load(path)) {
            out.clear();
            return false;
        }
        out = catalog.entries();
        return true;
    }

}  // namespace openkal::map
