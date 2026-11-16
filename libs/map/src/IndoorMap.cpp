/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/IndoorMap.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace openkal::map {
    namespace {

        constexpr std::size_t kHeaderSize = 60;
        constexpr std::size_t kLightSize = 60;
        class Cursor {
          public:
            Cursor(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}

            [[nodiscard]] bool skip(std::size_t n) {
                if (m_off + n > m_size) {
                    return false;
                }
                m_off += n;
                return true;
            }

            [[nodiscard]] bool read(void* dst, std::size_t n) {
                if (m_off + n > m_size) {
                    return false;
                }
                std::memcpy(dst, m_data + m_off, n);
                m_off += n;
                return true;
            }

            [[nodiscard]] std::uint8_t u8() {
                std::uint8_t v = 0;
                (void) read(&v, 1);
                return v;
            }

            [[nodiscard]] std::uint16_t u16() {
                std::uint16_t v = 0;
                (void) read(&v, 2);
                return v;
            }

            [[nodiscard]] std::uint32_t u32() {
                std::uint32_t v = 0;
                (void) read(&v, 4);
                return v;
            }

            [[nodiscard]] float f32() {
                float v = 0.0f;
                (void) read(&v, 4);
                return v;
            }

            [[nodiscard]] bool ok() const {
                return m_off <= m_size;
            }

          private:
            const std::uint8_t* m_data = nullptr;
            std::size_t m_size = 0;
            std::size_t m_off = 0;
        };

        [[nodiscard]] std::string c_string_at(const std::string& blob, std::uint32_t offset) {
            if (offset >= blob.size()) {
                return {};
            }
            const auto end = blob.find('\0', offset);
            if (end == std::string::npos) {
                return blob.substr(offset);
            }
            return blob.substr(offset, end - offset);
        }

        [[nodiscard]] std::string last_component(std::string_view path) {
            std::size_t slash = path.find_last_of("/\\");
            if (slash == std::string_view::npos) {
                return std::string(path);
            }
            return std::string(path.substr(slash + 1));
        }

        [[nodiscard]] std::string strip_map_prefix(std::string_view mapname) {
            std::string s(mapname);
            if (s.size() >= 4 && (s.compare(0, 4, "Map\\") == 0 || s.compare(0, 4, "Map/") == 0 ||
                                  s.compare(0, 4, "MAP\\") == 0 || s.compare(0, 4, "MAP/") == 0)) {
                s = s.substr(4);
            }
            return s;
        }

        [[nodiscard]] std::string file_stem(std::string_view path) {
            std::string name = last_component(path);
            const auto dot = name.find_last_of('.');
            if (dot != std::string::npos) {
                name = name.substr(0, dot);
            }
            return name;
        }

        [[nodiscard]] int signed_index(std::uint16_t w) {
            return static_cast<int>(static_cast<std::int16_t>(w));
        }

    }  // namespace

    void IndoorMap::clear() {
        m_path.clear();
        m_stem.clear();
        m_maximum = {};
        m_minimum = {};
        m_regions.clear();
        m_objects.clear();
        m_nodes.clear();
        m_leaves.clear();
        m_pvs.clear();
    }

    std::string IndoorMap::stem_from_mapname(std::string_view mapname) {
        return file_stem(strip_map_prefix(mapname));
    }

    std::string IndoorMap::resolve_map_file(const std::string& map_root, std::string_view mapname) {
        const std::string stem = stem_from_mapname(mapname);
        if (stem.empty()) {
            return {};
        }
        const std::string direct = platform::join_path(map_root, stem + ".map");
        if (platform::file_exists(direct)) {
            return direct;
        }
        const std::string nested =
            platform::join_path(platform::join_path(map_root, stem), stem + ".map");
        if (platform::file_exists(nested)) {
            return nested;
        }
        return {};
    }

    bool IndoorMap::load(std::string_view path) {
        clear();
        const std::string path_str(path);
        const auto bytes = platform::read_file(path_str);
        if (!bytes || bytes->size() < kHeaderSize) {
            log::warn("IndoorMap::load: missing or short " + path_str);
            return false;
        }

        Cursor cur(bytes->data(), bytes->size());
        const std::uint8_t version = cur.u8();
        const std::uint8_t region_count = cur.u8();
        const std::uint16_t node_count = cur.u16();
        (void) cur.u32();  // crc
        const std::uint16_t leaf_count = cur.u16();
        const std::uint16_t part_count = cur.u16();
        const std::uint32_t string_size = cur.u32();
        m_maximum = {cur.f32(), cur.f32(), cur.f32()};
        m_minimum = {cur.f32(), cur.f32(), cur.f32()};
        (void) cur.u32();  // option
        const std::uint16_t light_count = cur.u16();
        const std::uint16_t object_count = cur.u16();
        if (!cur.skip(12)) {
            clear();
            return false;
        }
        if (version < 2) {
            log::warn("IndoorMap::load: unsupported version " + std::to_string(version) + " in " +
                      path_str);
            clear();
            return false;
        }

        std::string strings(string_size, '\0');
        if (string_size > 0 && !cur.read(strings.data(), string_size)) {
            clear();
            return false;
        }
        m_pvs.assign(strings.begin(), strings.end());

        m_leaves.resize(leaf_count);
        for (std::uint16_t i = 0; i < leaf_count; ++i) {
            std::uint32_t leaf_list = 0;
            if (!cur.read(&leaf_list, 4)) {
                log::warn("IndoorMap::load: truncated BSP leaves in " + path_str);
                clear();
                return false;
            }
            m_leaves[i].set_pvs_bit_index(leaf_list << 3);
        }

        m_nodes.resize(node_count);
        for (std::uint16_t i = 0; i < node_count; ++i) {
            std::array<float, 4> plane{};
            std::uint16_t leaf = 0;
            std::uint16_t side0 = 0;
            std::uint16_t side1 = 0;
            if (!cur.read(plane.data(), 16) || !cur.read(&leaf, 2) || !cur.read(&side0, 2) ||
                !cur.read(&side1, 2)) {
                log::warn("IndoorMap::load: truncated BSP nodes in " + path_str);
                clear();
                return false;
            }
            IndoorBspNode node;
            node.set_plane(plane);
            node.set_leaf(signed_index(leaf));
            node.set_side0(signed_index(side0));
            node.set_side1(signed_index(side1));
            m_nodes[i] = node;
        }

        if (!cur.skip(static_cast<std::size_t>(light_count) * kLightSize)) {
            log::warn("IndoorMap::load: truncated BSP lights in " + path_str);
            clear();
            return false;
        }

        m_regions.reserve(region_count);
        std::uint32_t parts_seen = 0;
        for (std::uint8_t i = 0; i < region_count; ++i) {
            std::uint32_t name_off = 0;
            std::uint32_t region_parts = 0;
            std::array<float, 3> maximum{};
            std::array<float, 3> minimum{};
            std::uint32_t leaf_list = 0;
            if (!cur.read(&name_off, 4) || !cur.read(&region_parts, 4) ||
                !cur.read(maximum.data(), 12) || !cur.read(minimum.data(), 12) ||
                !cur.read(&leaf_list, 4) || !cur.skip(8)) {
                clear();
                return false;
            }
            IndoorRegion region;
            region.set_name(c_string_at(strings, name_off));
            region.set_maximum(maximum);
            region.set_minimum(minimum);
            region.set_part_count(region_parts);
            region.set_pvs_bit_index(leaf_list << 3);
            parts_seen += region_parts;
            for (std::uint32_t part_index = 0; part_index < region_parts; ++part_index) {
                std::uint32_t part_name_off = 0;
                std::array<float, 3> part_maximum{};
                std::array<float, 3> part_minimum{};
                if (!cur.read(&part_name_off, 4) || !cur.read(part_maximum.data(), 12) ||
                    !cur.read(part_minimum.data(), 12) || !cur.skip(8)) {
                    clear();
                    return false;
                }
                IndoorPart part;
                part.set_name(c_string_at(strings, part_name_off));
                part.set_maximum(part_maximum);
                part.set_minimum(part_minimum);
                region.add_part(std::move(part));
            }
            m_regions.push_back(std::move(region));
        }
        if (parts_seen != part_count) {
            log::warn("IndoorMap::load: part count mismatch in " + path_str + " header=" +
                      std::to_string(part_count) + " regions=" + std::to_string(parts_seen));
        }

        m_objects.reserve(object_count);
        for (std::uint16_t i = 0; i < object_count; ++i) {
            std::uint32_t path_off = 0;
            std::array<float, 12> matrix{};
            if (!cur.read(&path_off, 4) || !cur.read(matrix.data(), 48)) {
                clear();
                return false;
            }
            IndoorObject obj;
            obj.set_path(c_string_at(strings, path_off));
            obj.set_matrix(matrix);
            m_objects.push_back(std::move(obj));
        }

        if (!m_nodes.empty()) {
            for (int i = 0; i < static_cast<int>(m_regions.size()); ++i) {
                std::uint32_t bit = m_regions[static_cast<std::size_t>(i)].pvs_bit_index();
                insert_region(i, 0, bit);
            }
        }

        m_path = path_str;
        m_stem = file_stem(path_str);
        log::info("IndoorMap::load: " + m_stem + " regions=" + std::to_string(m_regions.size()) +
                  " objects=" + std::to_string(m_objects.size()) + " nodes=" +
                  std::to_string(m_nodes.size()) + " leaves=" + std::to_string(m_leaves.size()));
        return true;
    }

    bool IndoorMap::collect_region_part_visibility(const IndoorBspLeaf::RegionLink& link,
                                                   std::vector<std::uint8_t>& out) const {
        out.clear();
        const int region_index = link.region_index();
        if (region_index < 0 || region_index >= static_cast<int>(m_regions.size())) {
            return false;
        }
        const IndoorRegion& region = m_regions[static_cast<std::size_t>(region_index)];
        const std::size_t part_count = region.parts().size();
        if (part_count != region.part_count()) {
            return false;
        }
        const std::uint64_t first_bit = link.part_pvs_bit_index();
        const std::uint64_t available_bits = static_cast<std::uint64_t>(m_pvs.size()) * 8u;
        if (first_bit > available_bits || part_count > available_bits - first_bit) {
            return false;
        }
        out.reserve(part_count);
        std::uint32_t bit_index = link.part_pvs_bit_index();
        for (std::size_t i = 0; i < part_count; ++i) {
            out.push_back(pvs_bit(bit_index) ? 1u : 0u);
        }
        return true;
    }

    bool IndoorMap::pvs_bit(std::uint32_t& bit_index) const {
        const std::uint32_t n = bit_index++;
        const std::size_t byte = static_cast<std::size_t>(n >> 3);
        if (byte >= m_pvs.size()) {
            return false;
        }
        return (m_pvs[byte] & static_cast<std::uint8_t>(1u << (n & 7))) != 0;
    }

    void IndoorMap::insert_region(int region_index, int node_index, std::uint32_t& bit_index) {
        if (node_index < 0 || node_index >= static_cast<int>(m_nodes.size())) {
            return;
        }
        const IndoorBspNode& node = m_nodes[static_cast<std::size_t>(node_index)];
        if (node.leaf() >= 0 && pvs_bit(bit_index)) {
            if (node.leaf() < static_cast<int>(m_leaves.size())) {
                m_leaves[static_cast<std::size_t>(node.leaf())].add_region(region_index, bit_index);
            }
            bit_index += m_regions[static_cast<std::size_t>(region_index)].part_count();
        }
        if (node.side0() >= 0 && pvs_bit(bit_index)) {
            insert_region(region_index, node.side0(), bit_index);
        }
        if (node.side1() >= 0 && pvs_bit(bit_index)) {
            insert_region(region_index, node.side1(), bit_index);
        }
    }

    int IndoorMap::camera_leaf(float x, float y, float z) const {
        if (m_nodes.empty()) {
            return -1;
        }
        int node_index = 0;
        int found = -1;
        while (node_index >= 0 && node_index < static_cast<int>(m_nodes.size())) {
            const IndoorBspNode& node = m_nodes[static_cast<std::size_t>(node_index)];
            const auto& p = node.plane();
            const float d = p[0] * x + p[1] * y + p[2] * z + p[3];
            if (d < 0.0f) {
                node_index = node.side0();
            } else {
                if (node.side1() < 0 && node.leaf() >= 0) {
                    found = node.leaf();
                }
                node_index = node.side1();
            }
        }
        return found;
    }

    void IndoorMap::collect_pvs_leaves(std::uint32_t pvs_bit_index, std::vector<int>& out) const {
        out.clear();
        if (m_nodes.empty()) {
            return;
        }
        std::uint32_t bit = pvs_bit_index;
        collect_pvs_leaves_node(0, bit, out);
    }

    void IndoorMap::collect_pvs_leaves_node(int node_index,
                                            std::uint32_t& bit_index,
                                            std::vector<int>& out) const {
        if (node_index < 0 || node_index >= static_cast<int>(m_nodes.size())) {
            return;
        }
        const IndoorBspNode& node = m_nodes[static_cast<std::size_t>(node_index)];
        if (node.leaf() >= 0 && pvs_bit(bit_index)) {
            out.push_back(node.leaf());
        }
        if (node.side0() >= 0 && pvs_bit(bit_index)) {
            collect_pvs_leaves_node(node.side0(), bit_index, out);
        }
        if (node.side1() >= 0 && pvs_bit(bit_index)) {
            collect_pvs_leaves_node(node.side1(), bit_index, out);
        }
    }

    void IndoorMap::insert_object(float x,
                                  float y,
                                  float z,
                                  float radius,
                                  int object_index,
                                  std::vector<std::vector<int>>& leaf_objects) const {
        if (m_nodes.empty() || leaf_objects.size() != m_leaves.size()) {
            return;
        }
        insert_object_node(0, x, y, z, radius, object_index, leaf_objects);
    }

    void IndoorMap::insert_object_node(int node_index,
                                       float x,
                                       float y,
                                       float z,
                                       float radius,
                                       int object_index,
                                       std::vector<std::vector<int>>& leaf_objects) const {
        if (node_index < 0 || node_index >= static_cast<int>(m_nodes.size())) {
            return;
        }
        const IndoorBspNode& node = m_nodes[static_cast<std::size_t>(node_index)];
        const auto& p = node.plane();
        const float d = p[0] * x + p[1] * y + p[2] * z + p[3];
        if (d + radius >= 0.0f) {
            if (node.side1() < 0) {
                if (node.leaf() >= 0 && node.leaf() < static_cast<int>(leaf_objects.size())) {
                    leaf_objects[static_cast<std::size_t>(node.leaf())].push_back(object_index);
                }
                return;
            }
            insert_object_node(node.side1(), x, y, z, radius, object_index, leaf_objects);
        }
        if (d < radius) {
            if (node.side0() < 0) {
                return;
            }
            insert_object_node(node.side0(), x, y, z, radius, object_index, leaf_objects);
        }
    }

}  // namespace openkal::map
