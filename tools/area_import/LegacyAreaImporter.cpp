/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LegacyAreaImporter.h"

#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace openkal::area_import {
    namespace {

        constexpr std::size_t kTileBytes =
            static_cast<std::size_t>(map::kTileCount) * static_cast<std::size_t>(map::kTileCount);
        constexpr std::size_t kPlainSize =
            sizeof(std::int32_t) + sizeof(std::uint32_t) * kTileBytes;
        constexpr std::size_t kInterleavedRecord = sizeof(std::uint32_t) + sizeof(std::uint16_t);
        constexpr std::size_t kInterleavedSize =
            sizeof(std::int32_t) + kInterleavedRecord * kTileBytes;
        constexpr std::size_t kOpaqueHeaderBytes = 1078;
        constexpr std::size_t kOpaqueRecordBytesA = 12;
        constexpr std::size_t kOpaqueRecordBytesB = 13;
        constexpr std::size_t kOpaqueSizeA = kOpaqueHeaderBytes + kOpaqueRecordBytesA * kTileBytes;
        constexpr std::size_t kOpaqueSizeB = kOpaqueHeaderBytes + kOpaqueRecordBytesB * kTileBytes;
        constexpr std::int32_t kAltVersionTag = 4096;
        constexpr int kSubcellGridSize = map::kTileCount * map::AreaCell::kSubcellsPerTile;
        constexpr std::size_t kSubcellCount =
            static_cast<std::size_t>(kSubcellGridSize) * kSubcellGridSize;
        constexpr std::size_t kMinimumSmoothComponent = 9;

        [[nodiscard]] std::size_t subcell_index(int x, int y) {
            return static_cast<std::size_t>(y) * kSubcellGridSize + static_cast<std::size_t>(x);
        }

        [[nodiscard]] bool occupied(const std::vector<std::uint8_t>& cells, int x, int y) {
            if (x < 0 || y < 0 || x >= kSubcellGridSize || y >= kSubcellGridSize) {
                return true;
            }
            return cells[subcell_index(x, y)] != 0;
        }

        [[nodiscard]] std::vector<std::uint8_t>
        unpack_masks(const std::vector<map::AreaCell::SubcellMask>& masks) {
            std::vector<std::uint8_t> cells(kSubcellCount, 0);
            if (masks.size() != kTileBytes) {
                return cells;
            }
            for (int y = 0; y < kSubcellGridSize; ++y) {
                for (int x = 0; x < kSubcellGridSize; ++x) {
                    const int tile_x = x / map::AreaCell::kSubcellsPerTile;
                    const int tile_y = y / map::AreaCell::kSubcellsPerTile;
                    const unsigned bit =
                        static_cast<unsigned>(x % map::AreaCell::kSubcellsPerTile) +
                        map::AreaCell::kSubcellsPerTile *
                            static_cast<unsigned>(y % map::AreaCell::kSubcellsPerTile);
                    const std::size_t tile =
                        static_cast<std::size_t>(tile_y) * map::kTileCount + tile_x;
                    cells[subcell_index(x, y)] =
                        (masks[tile] & (map::AreaCell::SubcellMask{1} << bit)) != 0 ? 1u : 0u;
                }
            }
            return cells;
        }

        [[nodiscard]] std::vector<map::AreaCell::SubcellMask>
        pack_masks(const std::vector<std::uint8_t>& cells) {
            std::vector<map::AreaCell::SubcellMask> masks(kTileBytes, 0);
            for (int y = 0; y < kSubcellGridSize; ++y) {
                for (int x = 0; x < kSubcellGridSize; ++x) {
                    if (!occupied(cells, x, y)) {
                        continue;
                    }
                    const int tile_x = x / map::AreaCell::kSubcellsPerTile;
                    const int tile_y = y / map::AreaCell::kSubcellsPerTile;
                    const unsigned bit =
                        static_cast<unsigned>(x % map::AreaCell::kSubcellsPerTile) +
                        map::AreaCell::kSubcellsPerTile *
                            static_cast<unsigned>(y % map::AreaCell::kSubcellsPerTile);
                    const std::size_t tile =
                        static_cast<std::size_t>(tile_y) * map::kTileCount + tile_x;
                    masks[tile] |= map::AreaCell::SubcellMask{1} << bit;
                }
            }
            return masks;
        }

        [[nodiscard]] std::size_t smooth_masks(std::vector<map::AreaCell::SubcellMask>& masks) {
            if (masks.size() != kTileBytes) {
                return 0;
            }

            const std::vector<std::uint8_t> source = unpack_masks(masks);
            std::vector<std::int32_t> component(kSubcellCount, -1);
            std::vector<std::size_t> component_sizes;
            std::vector<std::size_t> queue;
            queue.reserve(kSubcellCount);
            constexpr std::array<int, 4> kDx = {0, 1, 0, -1};
            constexpr std::array<int, 4> kDy = {-1, 0, 1, 0};

            for (int y = 0; y < kSubcellGridSize; ++y) {
                for (int x = 0; x < kSubcellGridSize; ++x) {
                    const std::size_t start = subcell_index(x, y);
                    if (source[start] == 0 || component[start] >= 0) {
                        continue;
                    }
                    const std::int32_t id = static_cast<std::int32_t>(component_sizes.size());
                    component_sizes.push_back(0);
                    queue.clear();
                    queue.push_back(start);
                    component[start] = id;
                    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
                        const std::size_t current = queue[cursor];
                        ++component_sizes[static_cast<std::size_t>(id)];
                        const int current_x = static_cast<int>(current % kSubcellGridSize);
                        const int current_y = static_cast<int>(current / kSubcellGridSize);
                        for (std::size_t direction = 0; direction < kDx.size(); ++direction) {
                            const int neighbor_x = current_x + kDx[direction];
                            const int neighbor_y = current_y + kDy[direction];
                            if (neighbor_x < 0 || neighbor_y < 0 ||
                                neighbor_x >= kSubcellGridSize || neighbor_y >= kSubcellGridSize) {
                                continue;
                            }
                            const std::size_t neighbor = subcell_index(neighbor_x, neighbor_y);
                            if (source[neighbor] == 0 || component[neighbor] >= 0) {
                                continue;
                            }
                            component[neighbor] = id;
                            queue.push_back(neighbor);
                        }
                    }
                }
            }

            std::vector<std::uint8_t> smoothed = source;
            std::size_t removed = 0;
            for (int y = 0; y < kSubcellGridSize; ++y) {
                for (int x = 0; x < kSubcellGridSize; ++x) {
                    if (x == 0 || y == 0 || x == kSubcellGridSize - 1 ||
                        y == kSubcellGridSize - 1) {
                        continue;
                    }
                    const std::size_t index = subcell_index(x, y);
                    if (source[index] == 0 || component[index] < 0 ||
                        component_sizes[static_cast<std::size_t>(component[index])] <
                            kMinimumSmoothComponent) {
                        continue;
                    }

                    const bool north = occupied(source, x, y - 1);
                    const bool east = occupied(source, x + 1, y);
                    const bool south = occupied(source, x, y + 1);
                    const bool west = occupied(source, x - 1, y);
                    const bool north_east = occupied(source, x + 1, y - 1);
                    const bool south_east = occupied(source, x + 1, y + 1);
                    const bool south_west = occupied(source, x - 1, y + 1);
                    const bool north_west = occupied(source, x - 1, y - 1);
                    const int orthogonal = static_cast<int>(north) + static_cast<int>(east) +
                                           static_cast<int>(south) + static_cast<int>(west);
                    const int diagonal =
                        static_cast<int>(north_east) + static_cast<int>(south_east) +
                        static_cast<int>(south_west) + static_cast<int>(north_west);
                    const bool convex_corner =
                        orthogonal == 2 &&
                        ((north && east && north_east) || (east && south && south_east) ||
                         (south && west && south_west) || (west && north && north_west));
                    const bool supported_protrusion = orthogonal == 1 && diagonal >= 2;
                    if (!convex_corner && !supported_protrusion) {
                        continue;
                    }
                    smoothed[index] = 0;
                    ++removed;
                }
            }

            masks = pack_masks(smoothed);
            return removed;
        }

        bool
        copy_attrs(const std::uint8_t* source, std::size_t available_bytes, map::AreaCell& out) {
            out.assign_attrs(kTileBytes, 0u);
            const std::size_t wanted_bytes = out.attr_count() * sizeof(std::uint32_t);
            const std::size_t copied_bytes = std::min(wanted_bytes, available_bytes);
            if (copied_bytes != 0 && out.attrs_data() != nullptr) {
                std::memcpy(out.attrs_data(), source, copied_bytes);
            }
            return copied_bytes >= wanted_bytes - sizeof(std::uint32_t);
        }

        bool copy_interleaved_attrs(const std::uint8_t* source,
                                    std::size_t available_bytes,
                                    map::AreaCell& out) {
            out.assign_attrs(kTileBytes, 0u);
            const std::size_t records =
                std::min(out.attr_count(), available_bytes / kInterleavedRecord);
            std::uint32_t* destination = out.attrs_data();
            if (destination == nullptr) {
                return records >= out.attr_count();
            }
            for (std::size_t index = 0; index < records; ++index) {
                std::memcpy(destination + index,
                            source + index * kInterleavedRecord,
                            sizeof(std::uint32_t));
            }
            return records >= out.attr_count();
        }

        void import_opaque_cell(map::AreaCell& out) {
            out.set_version(map::kVersion);
            out.assign_attrs(kTileBytes, map::kAttrNotMoveable);
            out.ensure_blocked_mask(true);
            out.ensure_not_moveable_mask(true);
        }

    }  // namespace

    bool import_legacy_area(std::string_view path, map::AreaCell& out) {
        const std::string path_string(path);
        const auto bytes = platform::read_file(path_string);
        if (!bytes) {
            log::error("area_import: cannot read " + path_string);
            return false;
        }
        if (bytes->size() < sizeof(std::int32_t)) {
            log::error("area_import: file too small: " + path_string);
            return false;
        }
        if (bytes->size() == kOpaqueSizeA || bytes->size() == kOpaqueSizeB) {
            import_opaque_cell(out);
            log::warn("area_import: imported opaque legacy layout as blocked cell in " +
                      path_string);
            return true;
        }

        const bool interleaved = bytes->size() == kInterleavedSize;
        if (bytes->size() != kPlainSize && !interleaved) {
            log::warn("area_import: unsupported size " + std::to_string(bytes->size()) + " (need " +
                      std::to_string(kPlainSize) + " or " + std::to_string(kInterleavedSize) +
                      ") in " + path_string);
            return false;
        }

        std::int32_t source_version = 0;
        std::memcpy(&source_version, bytes->data(), sizeof(source_version));
        std::uint32_t second_word = 0;
        std::memcpy(&second_word, bytes->data() + sizeof(source_version), sizeof(second_word));

        const std::uint8_t* attributes = bytes->data() + sizeof(source_version);
        std::size_t attribute_bytes = bytes->size() - sizeof(source_version);
        std::int32_t logical_version = source_version;
        if (source_version == map::kVersion || source_version == 0) {
            logical_version = source_version;
        } else if (!interleaved && source_version == kAltVersionTag &&
                   second_word == static_cast<std::uint32_t>(map::kVersion)) {
            logical_version = map::kVersion;
            attributes = bytes->data() + 2 * sizeof(std::int32_t);
            attribute_bytes = bytes->size() - 2 * sizeof(std::int32_t);
        } else if (!interleaved && source_version == kAltVersionTag) {
            logical_version = map::kVersion;
        } else {
            log::warn("area_import: unexpected source version " + std::to_string(source_version) +
                      " in " + path_string);
        }

        out.set_version(logical_version);
        const bool copied = interleaved ? copy_interleaved_attrs(attributes, attribute_bytes, out)
                                        : copy_attrs(attributes, attribute_bytes, out);
        if (!copied) {
            log::error("area_import: attribute payload truncated: " + path_string);
            return false;
        }
        out.import_legacy_blocked_masks();
        return true;
    }

    SmoothStats smooth_imported_area(map::AreaCell& area) {
        std::vector<map::AreaCell::SubcellMask> blocked = area.blocked_masks();
        std::vector<map::AreaCell::SubcellMask> not_moveable = area.not_moveable_masks();
        SmoothStats stats;
        stats.blocked_removed = smooth_masks(blocked);
        stats.not_moveable_removed = smooth_masks(not_moveable);
        area.set_blocked_masks(std::move(blocked));
        area.set_not_moveable_masks(std::move(not_moveable));
        return stats;
    }

}  // namespace openkal::area_import
