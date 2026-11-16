/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::map {

    /** One independently culled mesh entry from MAP_PART. */
    class IndoorPart {
      public:
        /** Constructs an empty part. */
        IndoorPart() = default;

        /** @return Part mesh name from the map string table. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /** @param value Part mesh name. */
        void set_name(std::string value) {
            m_name = std::move(value);
        }

        /** @return Axis-aligned maximum in map-local space. */
        [[nodiscard]] const std::array<float, 3>& maximum() const {
            return m_maximum;
        }

        /** @param value Axis-aligned maximum. */
        void set_maximum(std::array<float, 3> value) {
            m_maximum = value;
        }

        /** @return Axis-aligned minimum in map-local space. */
        [[nodiscard]] const std::array<float, 3>& minimum() const {
            return m_minimum;
        }

        /** @param value Axis-aligned minimum. */
        void set_minimum(std::array<float, 3> value) {
            m_minimum = value;
        }

      private:
        /** Part mesh name. */
        std::string m_name;

        /** AABB maximum. */
        std::array<float, 3> m_maximum{};

        /** AABB minimum. */
        std::array<float, 3> m_minimum{};
    };

    /** One BSP region whose .gb lives beside the indoor .map (MRegion). */
    class IndoorRegion {
      public:
        /** Constructs an empty region. */
        IndoorRegion() = default;

        /** @return Region mesh stem (e.g. d_p01). */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /** @param value Region mesh stem. */
        void set_name(std::string value) {
            m_name = std::move(value);
        }

        /** @return Axis-aligned maximum (server X / world Y / server Z). */
        [[nodiscard]] const std::array<float, 3>& maximum() const {
            return m_maximum;
        }

        /** @param value Axis-aligned maximum. */
        void set_maximum(std::array<float, 3> value) {
            m_maximum = value;
        }

        /** @return Axis-aligned minimum (server X / world Y / server Z). */
        [[nodiscard]] const std::array<float, 3>& minimum() const {
            return m_minimum;
        }

        /** @param value Axis-aligned minimum. */
        void set_minimum(std::array<float, 3> value) {
            m_minimum = value;
        }

        /** @return Submesh count (MAP_REGION.part_count). */
        [[nodiscard]] std::uint32_t part_count() const {
            return m_part_count;
        }

        /** @param value Submesh count. */
        void set_part_count(std::uint32_t value) {
            m_part_count = value;
        }

        /** @return Per-submesh map records in GB mesh order. */
        [[nodiscard]] const std::vector<IndoorPart>& parts() const {
            return m_parts;
        }

        /** @param value Part record appended in file order. */
        void add_part(IndoorPart value) {
            m_parts.push_back(std::move(value));
        }

        /** @return Bit index of this region's PVS list (leaf_list << 3). */
        [[nodiscard]] std::uint32_t pvs_bit_index() const {
            return m_pvs_bit_index;
        }

        /** @param value Bit index into the string-table PVS blob. */
        void set_pvs_bit_index(std::uint32_t value) {
            m_pvs_bit_index = value;
        }

      private:
        /** Region mesh stem. */
        std::string m_name;

        /** AABB maximum. */
        std::array<float, 3> m_maximum{};

        /** AABB minimum. */
        std::array<float, 3> m_minimum{};

        /** GB submesh count used when consuming PVS bits. */
        std::uint32_t m_part_count = 0;

        /** Per-submesh names and bounds in file order. */
        std::vector<IndoorPart> m_parts;

        /** PVS bit index (MRegion::runtime state). */
        std::uint32_t m_pvs_bit_index = 0;
    };

    /** One placed decoration object (MAP_OBJECT) with a 3x4 D3DX matrix. */
    class IndoorObject {
      public:
        /** Constructs an empty object. */
        IndoorObject() = default;

        /** @return Relative GB path from the string table (e.g. lib\\column.gb). */
        [[nodiscard]] const std::string& path() const {
            return m_path;
        }

        /** @param value Relative GB path. */
        void set_path(std::string value) {
            m_path = std::move(value);
        }

        /** @return D3DX 3x4 matrix: three basis rows then translation. */
        [[nodiscard]] const std::array<float, 12>& matrix() const {
            return m_matrix;
        }

        /** @param value D3DX 3x4 matrix. */
        void set_matrix(std::array<float, 12> value) {
            m_matrix = value;
        }

      private:
        /** Relative GB path. */
        std::string m_path;

        /** D3DX 3x4 transform. */
        std::array<float, 12> m_matrix{};
    };

    /** One BSP split (MAP_NODE / MNode). */
    class IndoorBspNode {
      public:
        /** Constructs an empty node. */
        IndoorBspNode() = default;

        /** @return Plane coefficients A,B,C,D (D3DXPLANE). */
        [[nodiscard]] const std::array<float, 4>& plane() const {
            return m_plane;
        }

        /** @param value Plane coefficients A,B,C,D. */
        void set_plane(std::array<float, 4> value) {
            m_plane = value;
        }

        /** @return Leaf index, or -1. */
        [[nodiscard]] int leaf() const {
            return m_leaf;
        }

        /** @param value Leaf index, or -1. */
        void set_leaf(int value) {
            m_leaf = value;
        }

        /** @return Back-child node index, or -1. */
        [[nodiscard]] int side0() const {
            return m_side0;
        }

        /** @param value Back-child node index, or -1. */
        void set_side0(int value) {
            m_side0 = value;
        }

        /** @return Front-child node index, or -1. */
        [[nodiscard]] int side1() const {
            return m_side1;
        }

        /** @param value Front-child node index, or -1. */
        void set_side1(int value) {
            m_side1 = value;
        }

      private:
        /** Split plane. */
        std::array<float, 4> m_plane{};

        /** Attached leaf, or -1. */
        int m_leaf = -1;

        /** Child on the negative side, or -1. */
        int m_side0 = -1;

        /** Child on the positive side, or -1. */
        int m_side1 = -1;
    };

    /** One BSP leaf (MAP_LEAF / MLeaf) with regions visible from this cell. */
    class IndoorBspLeaf {
      public:
        /** Constructs an empty leaf. */
        IndoorBspLeaf() = default;

        /** @return Bit index of this leaf's object PVS list. */
        [[nodiscard]] std::uint32_t pvs_bit_index() const {
            return m_pvs_bit_index;
        }

        /** @param value Bit index into the string-table PVS blob. */
        void set_pvs_bit_index(std::uint32_t value) {
            m_pvs_bit_index = value;
        }

        /** One region visible from a leaf and its first per-part PVS bit. */
        class RegionLink {
          public:
            /** Constructs an empty link. */
            RegionLink() = default;

            /**
             * Constructs a leaf-to-region visibility link.
             * @param region_index Region index.
             * @param part_pvs_bit_index First part bit.
             */
            RegionLink(int region_index, std::uint32_t part_pvs_bit_index)
                : m_region_index(region_index), m_part_pvs_bit_index(part_pvs_bit_index) {}

            /** @return Index into IndoorMap::regions(). */
            [[nodiscard]] int region_index() const {
                return m_region_index;
            }

            /** @return First bit of this leaf/region part-visibility sequence. */
            [[nodiscard]] std::uint32_t part_pvs_bit_index() const {
                return m_part_pvs_bit_index;
            }

          private:
            /** Region index. */
            int m_region_index = -1;

            /** First per-part visibility bit. */
            std::uint32_t m_part_pvs_bit_index = 0;
        };

        /** @return Region links visible from this leaf. */
        [[nodiscard]] const std::vector<RegionLink>& regions() const {
            return m_regions;
        }

        /**
         * Appends a region visible from this leaf.
         * @param region_index Index into IndoorMap::regions().
         * @param part_pvs_bit_index First per-part visibility bit for this leaf.
         */
        void add_region(int region_index, std::uint32_t part_pvs_bit_index) {
            m_regions.emplace_back(region_index, part_pvs_bit_index);
        }

      private:
        /** Object-PVS bit index (MLeaf::runtime state). */
        std::uint32_t m_pvs_bit_index = 0;

        /** Regions whose PVS includes this leaf and their per-part bit offsets. */
        std::vector<RegionLink> m_regions;
    };

    /**
     * Client indoor visual map (`MAP_HEADER` v2). Parses regions, objects, and the
     * BSP used by MMap::OnSetCamera / RenderMap / RenderObject.
     * Coordinates match MMap::Load: X/Z are server pixels, Y is already
     * client height.
     */
    class IndoorMap {
      public:
        /**
         * Loads a `.map` file.
         * @param path Path to the indoor map binary.
         * @return True when the header and object table parsed.
         */
        [[nodiscard]] bool load(std::string_view path);

        /** Clears all parsed data. */
        void clear();

        /** @return True when no file is loaded. */
        [[nodiscard]] bool empty() const {
            return m_path.empty();
        }

        /** @return Source file path. */
        [[nodiscard]] const std::string& path() const {
            return m_path;
        }

        /** @return File stem used for region GB folders (tomb from tomb.map). */
        [[nodiscard]] const std::string& stem() const {
            return m_stem;
        }

        /** @return AABB maximum. */
        [[nodiscard]] const std::array<float, 3>& maximum() const {
            return m_maximum;
        }

        /** @return AABB minimum. */
        [[nodiscard]] const std::array<float, 3>& minimum() const {
            return m_minimum;
        }

        /** @return Region meshes that make up the dungeon rooms. */
        [[nodiscard]] const std::vector<IndoorRegion>& regions() const {
            return m_regions;
        }

        /** @return Placed decoration objects. */
        [[nodiscard]] const std::vector<IndoorObject>& objects() const {
            return m_objects;
        }

        /** @return BSP split nodes. */
        [[nodiscard]] const std::vector<IndoorBspNode>& nodes() const {
            return m_nodes;
        }

        /** @return BSP leaves with per-leaf visible regions. */
        [[nodiscard]] const std::vector<IndoorBspLeaf>& leaves() const {
            return m_leaves;
        }

        /**
         * Walks the BSP like MMap::OnSetCamera.
         * @param x Map-local X (server pixels).
         * @param y World Y.
         * @param z Map-local Z (server pixels).
         * @return Leaf index, or -1 when the probe is outside the tree.
         */
        [[nodiscard]] int camera_leaf(float x, float y, float z) const;

        /**
         * Pre-order PVS walk used by RenderObject / InsertRegion.
         * @param pvs_bit_index Start bit (leaf or region runtime state).
         * @param out Receives leaf indices whose PVS bit was set.
         */
        void collect_pvs_leaves(std::uint32_t pvs_bit_index, std::vector<int>& out) const;

        /**
         * Reads the per-submesh visibility sequence stored for one leaf/region link.
         * Invalid indices or truncated PVS data fail so callers can draw conservatively.
         * @param link Leaf association carrying the region and bit offset.
         * @param out Receives one byte per region part (zero hidden, one visible).
         * @return True when the complete visibility sequence was available.
         */
        [[nodiscard]] bool collect_region_part_visibility(const IndoorBspLeaf::RegionLink& link,
                                                          std::vector<std::uint8_t>& out) const;

        /**
         * Inserts a bounding sphere into every leaf it straddles (MMap::InsertObject).
         * @param x Sphere center X (map-local).
         * @param y Sphere center Y.
         * @param z Sphere center Z (map-local).
         * @param radius Sphere radius.
         * @param object_index Value stored in each hit leaf's list.
         * @param leaf_objects Per-leaf object index lists (size = leaves().size()).
         */
        void insert_object(float x,
                           float y,
                           float z,
                           float radius,
                           int object_index,
                           std::vector<std::vector<int>>& leaf_objects) const;

        /**
         * Strips `Map\\` and returns the last path component.
         * @param mapname map definition mapname field (e.g. Map\\tomb).
         * @return Stem (tomb).
         */
        [[nodiscard]] static std::string stem_from_mapname(std::string_view mapname);

        /**
         * Resolves `Map\\tomb` to `{map_root}/tomb.map` (or `{map_root}/tomb/tomb.map`).
         * @param map_root Indoor map directory (profile map_root).
         * @param mapname map definition mapname field.
         * @return Existing .map path, or empty when missing.
         */
        [[nodiscard]] static std::string resolve_map_file(const std::string& map_root,
                                                          std::string_view mapname);

      private:
        /**
         * Reads one PVS bit and advances the index (MMap::GetBit).
         * @param bit_index In/out bit cursor into runtime state.
         * @return True when the bit is set.
         */
        [[nodiscard]] bool pvs_bit(std::uint32_t& bit_index) const;

        /**
         * Recurses InsertRegion from node_index using the region's PVS stream.
         * @param region_index Region being inserted.
         * @param node_index Current BSP node.
         * @param bit_index In/out PVS bit cursor.
         */
        void insert_region(int region_index, int node_index, std::uint32_t& bit_index);

        /**
         * Recurses collect_pvs_leaves.
         * @param node_index Current BSP node.
         * @param bit_index In/out PVS bit cursor.
         * @param out Leaf indices with a set bit.
         */
        void collect_pvs_leaves_node(int node_index,
                                     std::uint32_t& bit_index,
                                     std::vector<int>& out) const;

        /**
         * Recurses insert_object.
         * @param node_index Current BSP node.
         * @param x Sphere center X.
         * @param y Sphere center Y.
         * @param z Sphere center Z.
         * @param radius Sphere radius.
         * @param object_index Value stored in hit leaves.
         * @param leaf_objects Per-leaf object lists.
         */
        void insert_object_node(int node_index,
                                float x,
                                float y,
                                float z,
                                float radius,
                                int object_index,
                                std::vector<std::vector<int>>& leaf_objects) const;

        /** Source file path. */
        std::string m_path;

        /** File stem. */
        std::string m_stem;

        /** AABB maximum. */
        std::array<float, 3> m_maximum{};

        /** AABB minimum. */
        std::array<float, 3> m_minimum{};

        /** Room meshes. */
        std::vector<IndoorRegion> m_regions;

        /** Decoration objects. */
        std::vector<IndoorObject> m_objects;

        /** BSP nodes. */
        std::vector<IndoorBspNode> m_nodes;

        /** BSP leaves. */
        std::vector<IndoorBspLeaf> m_leaves;

        /** String table; PVS bit lists live here (runtime state). */
        std::vector<std::uint8_t> m_pvs;
    };

}  // namespace openkal::map
