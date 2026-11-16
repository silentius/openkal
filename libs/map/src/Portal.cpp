/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/Portal.h"

#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <cctype>
#include <sstream>
#include <string>

namespace openkal::map {
    namespace {

        constexpr int kEntranceTileOffset = 6;
        constexpr int kDefaultEntranceZ = 16384;

        [[nodiscard]] std::string trim_copy(std::string_view text) {
            std::size_t begin = 0;
            while (begin < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
                ++begin;
            }
            std::size_t end = text.size();
            while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
                --end;
            }
            return std::string(text.substr(begin, end - begin));
        }

        [[nodiscard]] PortalKind kind_from_string(std::string_view text) {
            std::string lower = trim_copy(text);
            for (char& ch : lower) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (lower == "exit") {
                return PortalKind::Exit;
            }
            return PortalKind::Enter;
        }

        void infer_kind_and_dungeon(PortalRecord& rec) {
            if (rec.dungeon() == 0) {
                if (rec.kind() == PortalKind::Enter && rec.to_map() >= 1 && rec.to_map() <= 4) {
                    rec.set_dungeon(rec.to_map());
                } else if (rec.from_map() >= 1 && rec.from_map() <= 4) {
                    rec.set_dungeon(rec.from_map());
                    if (rec.to_map() == 0) {
                        rec.set_kind(PortalKind::Exit);
                    }
                } else if (rec.from_map() == 0 && rec.to_map() >= 1 && rec.to_map() <= 4) {
                    rec.set_dungeon(rec.to_map());
                    rec.set_kind(PortalKind::Enter);
                }
            }
        }

        [[nodiscard]] bool parse_portal_json_object(const platform::Json& obj, PortalRecord& out) {
            if (!obj.is_object()) {
                return false;
            }
            out = PortalRecord{};
            out.set_id(obj.at("id").as_string());
            out.set_dungeon(obj.at("dungeon").as_int(0));
            if (const platform::Json* kind = obj.get("kind"); kind && kind->is_string()) {
                out.set_kind(kind_from_string(kind->as_string()));
            }
            out.set_from_map(obj.at("fromMap").as_int(0));
            if (const platform::Json* field_trigger = obj.get("fieldTrigger");
                field_trigger != nullptr && field_trigger->type() == platform::Json::Type::Bool) {
                out.set_field_trigger_enabled(field_trigger->boolean());
            }
            const platform::Json& rect = obj.at("fromRect");
            if (!rect.is_array() || rect.array().size() < 4) {
                return false;
            }
            out.set_left(rect.array()[0].as_int(0));
            out.set_top(rect.array()[1].as_int(0));
            out.set_right(rect.array()[2].as_int(0));
            out.set_bottom(rect.array()[3].as_int(0));
            if (out.right() <= out.left() || out.bottom() <= out.top()) {
                return false;
            }
            out.set_to_map(obj.at("toMap").as_int(0));
            const platform::Json& dest = obj.at("to");
            if (!dest.is_array() || dest.array().size() < 3) {
                return false;
            }
            out.set_to_x(dest.array()[0].as_int(0));
            out.set_to_y(dest.array()[1].as_int(0));
            out.set_to_z(dest.array()[2].as_int(0));
            const platform::Json& entrance = obj.at("entrance");
            if (entrance.is_array() && entrance.array().size() >= 3) {
                out.set_entrance_x(entrance.array()[0].as_int(0));
                out.set_entrance_y(entrance.array()[1].as_int(0));
                out.set_entrance_z(entrance.array()[2].as_int(0));
            }
            infer_kind_and_dungeon(out);
            out.derive_entrance_if_missing();
            return !out.id().empty() || out.dungeon() > 0;
        }

    }  // namespace

    bool PortalRecord::contains_tile(int tile_x, int tile_y) const {
        return tile_x >= m_left && tile_x < m_right && tile_y >= m_top && tile_y < m_bottom;
    }

    void PortalRecord::center_pixel(int& pixel_x, int& pixel_y) const {
        const int mid_tx = (m_left + m_right) / 2;
        const int mid_ty = (m_top + m_bottom) / 2;
        pixel_x = mid_tx * kTileSize + kTileSize / 2;
        pixel_y = mid_ty * kTileSize + kTileSize / 2;
    }

    void PortalRecord::derive_entrance_if_missing() {
        if (m_has_entrance) {
            return;
        }
        const int mid_ty = (m_top + m_bottom) / 2;
        const int ex_tile = m_left - kEntranceTileOffset;
        m_entrance_x = ex_tile * kTileSize + kTileSize / 2;
        m_entrance_y = mid_ty * kTileSize + kTileSize / 2;
        m_entrance_z = kDefaultEntranceZ;
        m_has_entrance = true;
    }

    void PortalCatalog::clear() {
        m_portals.clear();
    }

    bool PortalCatalog::load_json(std::string_view path) {
        m_portals.clear();
        const std::string path_str(path);
        const auto bytes = platform::File::read(path_str);
        if (!bytes) {
            log::warn("PortalCatalog::load_json: missing " + path_str);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool PortalCatalog::load_json_text(std::string_view text) {
        m_portals.clear();
        platform::Json root;
        std::string err;
        if (!platform::json_parse(std::string(text), root, &err)) {
            log::warn("PortalCatalog::load_json_text: " + err);
            return false;
        }
        const platform::Json* array = nullptr;
        if (root.is_array()) {
            array = &root;
        } else if (root.is_object()) {
            const platform::Json& portals = root.at("portals");
            if (portals.is_array()) {
                array = &portals;
            }
        }
        if (array == nullptr) {
            return false;
        }
        for (const auto& item : array->array()) {
            PortalRecord rec;
            if (!parse_portal_json_object(item, rec)) {
                continue;
            }
            m_portals.push_back(std::move(rec));
        }
        log::info("PortalCatalog: loaded " + std::to_string(m_portals.size()) + " portals");
        return !m_portals.empty();
    }

    const PortalRecord* PortalCatalog::find(int from_map, int tile_x, int tile_y) const {
        for (const auto& p : m_portals) {
            if (p.field_trigger_enabled() && p.from_map() == from_map &&
                p.contains_tile(tile_x, tile_y)) {
                return &p;
            }
        }
        return nullptr;
    }

    const PortalRecord* PortalCatalog::find_by_id(std::string_view id) const {
        for (const auto& p : m_portals) {
            if (p.id() == id) {
                return &p;
            }
        }
        return nullptr;
    }

    const PortalRecord* PortalCatalog::entrance_for_dungeon(int dungeon) const {
        if (dungeon < 1 || dungeon > 6) {
            return nullptr;
        }
        for (const auto& p : m_portals) {
            if (p.dungeon() == dungeon && p.kind() == PortalKind::Enter && p.from_map() == 0) {
                return &p;
            }
        }
        return nullptr;
    }

    bool PortalCatalog::parse_dungeon_arg(std::string_view args, int& dungeon) {
        const std::string token = trim_copy(args);
        if (token.size() != 2) {
            return false;
        }
        const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(token[0])));
        const char digit = token[1];
        if (letter != 'd' || digit < '1' || digit > '6') {
            return false;
        }
        dungeon = digit - '0';
        return true;
    }

    bool PortalCatalog::parse_place_arg(std::string_view args, std::string& id) {
        const std::string token = trim_copy(args);
        std::string lower = token;
        for (char& ch : lower) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lower == "emok") {
            id = "emok_enter";
            return true;
        }
        if (lower == "vod") {
            id = "vod_back";
            return true;
        }
        if (lower == "lord") {
            id = "d3_internal_17";
            return true;
        }
        return false;
    }

    const char* PortalCatalog::place_teleport_reply(std::string_view id) {
        if (id == "emok_enter") {
            return "Teleporting to Emok Island.";
        }
        if (id == "vod_back") {
            return "Teleporting to Valley of Devah.";
        }
        if (id == "d3_internal_17") {
            return "Teleporting to Doggebi Lord stage.";
        }
        return "Teleporting.";
    }

    bool PortalCatalog::parse_area_arg(std::string_view args, int& area) {
        const std::string token = trim_copy(args);
        if (token.empty()) {
            return false;
        }
        std::istringstream ss(token);
        std::string keyword;
        int value = 0;
        std::string extra;
        if (!(ss >> keyword >> value)) {
            return false;
        }
        for (char& ch : keyword) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (keyword != "area" || value <= 0) {
            return false;
        }
        if (ss >> extra) {
            return false;
        }
        area = value;
        return true;
    }

}  // namespace openkal::map
