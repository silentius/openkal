/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/AreaMapCodec.h"

#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::map {
    namespace {

        constexpr std::uint16_t kFormatVersion = 2;
        constexpr std::uint16_t kHeaderBytes = 40;
        constexpr std::size_t kTileEntries = static_cast<std::size_t>(kTileCount) * kTileCount;

        void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
            bytes.push_back(static_cast<std::uint8_t>(value));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        }

        void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
            for (unsigned shift = 0; shift < 32; shift += 8) {
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
            }
        }

        void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
            append_u32(bytes, static_cast<std::uint32_t>(value));
        }

        [[nodiscard]] bool read_u16(const std::vector<std::uint8_t>& bytes,
                                    std::size_t& offset,
                                    std::size_t end,
                                    std::uint16_t& out) {
            if (offset > end || end - offset < 2) {
                return false;
            }
            out = static_cast<std::uint16_t>(bytes[offset]) |
                  (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            return true;
        }

        [[nodiscard]] bool read_u32(const std::vector<std::uint8_t>& bytes,
                                    std::size_t& offset,
                                    std::size_t end,
                                    std::uint32_t& out) {
            if (offset > end || end - offset < 4) {
                return false;
            }
            out = 0;
            for (unsigned shift = 0; shift < 32; shift += 8) {
                out |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
            }
            return true;
        }

        [[nodiscard]] bool read_i32(const std::vector<std::uint8_t>& bytes,
                                    std::size_t& offset,
                                    std::size_t end,
                                    std::int32_t& out) {
            std::uint32_t value = 0;
            if (!read_u32(bytes, offset, end, value)) {
                return false;
            }
            out = static_cast<std::int32_t>(value);
            return true;
        }

        [[nodiscard]] std::uint32_t checksum(const std::vector<std::uint8_t>& bytes) {
            std::uint32_t hash = 2166136261u;
            for (const std::uint8_t byte : bytes) {
                hash ^= byte;
                hash *= 16777619u;
            }
            return hash;
        }

        void encode_attr_runs(const AreaCell& cell, std::vector<std::uint8_t>& out) {
            std::size_t index = 0;
            while (index < kTileEntries) {
                const std::uint32_t value = cell.attr_at_flat(index);
                std::uint32_t count = 1;
                while (index + count < kTileEntries && cell.attr_at_flat(index + count) == value) {
                    ++count;
                }
                append_u32(out, count);
                append_u32(out, value);
                index += count;
            }
        }

        void encode_blocked_runs(const AreaCell& cell, std::vector<std::uint8_t>& out) {
            const std::vector<AreaCell::SubcellMask>& blocked = cell.blocked_masks();
            const std::vector<AreaCell::SubcellMask>& not_moveable = cell.not_moveable_masks();
            std::size_t index = 0;
            while (index < blocked.size()) {
                const AreaCell::SubcellMask blocked_value = blocked[index];
                const AreaCell::SubcellMask not_moveable_value = not_moveable[index];
                std::uint32_t count = 1;
                while (index + count < blocked.size() && blocked[index + count] == blocked_value &&
                       not_moveable[index + count] == not_moveable_value) {
                    ++count;
                }
                append_u32(out, count);
                append_u16(out, blocked_value);
                append_u16(out, not_moveable_value);
                index += count;
            }
        }

        [[nodiscard]] bool decode_attr_runs(const std::vector<std::uint8_t>& bytes,
                                            std::size_t offset,
                                            std::size_t end,
                                            AreaCell& out) {
            out.assign_attrs(kTileEntries, 0);
            std::size_t index = 0;
            while (offset < end) {
                std::uint32_t count = 0;
                std::uint32_t value = 0;
                if (!read_u32(bytes, offset, end, count) || !read_u32(bytes, offset, end, value) ||
                    count == 0 || count > kTileEntries - index) {
                    return false;
                }
                for (std::uint32_t run = 0; run < count; ++run) {
                    out.set_attr_flat(index++, value);
                }
            }
            return offset == end && index == kTileEntries;
        }

        [[nodiscard]] bool decode_blocked_runs(const std::vector<std::uint8_t>& bytes,
                                               std::size_t offset,
                                               std::size_t end,
                                               AreaCell& out) {
            std::vector<AreaCell::SubcellMask> blocked;
            std::vector<AreaCell::SubcellMask> not_moveable;
            blocked.reserve(kTileEntries);
            not_moveable.reserve(kTileEntries);
            while (offset < end) {
                std::uint32_t count = 0;
                AreaCell::SubcellMask blocked_value = 0;
                AreaCell::SubcellMask not_moveable_value = 0;
                if (!read_u32(bytes, offset, end, count) || count == 0 ||
                    count > kTileEntries - blocked.size() ||
                    !read_u16(bytes, offset, end, blocked_value) ||
                    !read_u16(bytes, offset, end, not_moveable_value)) {
                    return false;
                }
                blocked.insert(blocked.end(), count, blocked_value);
                not_moveable.insert(not_moveable.end(), count, not_moveable_value);
            }
            if (offset != end || blocked.size() != kTileEntries ||
                not_moveable.size() != kTileEntries) {
                return false;
            }
            out.set_blocked_masks(std::move(blocked));
            out.set_not_moveable_masks(std::move(not_moveable));
            return true;
        }

        [[nodiscard]] bool valid_cell(const AreaCell& cell) {
            return cell.attr_count() == kTileEntries &&
                   cell.blocked_masks().size() == kTileEntries &&
                   cell.not_moveable_masks().size() == kTileEntries;
        }

    }  // namespace

    bool AreaMapCodec::save(std::string_view path, const AreaCell& cell) {
        if (!valid_cell(cell)) {
            log::error("AreaMapCodec::save: incomplete area cell");
            return false;
        }

        std::vector<std::uint8_t> attrs;
        std::vector<std::uint8_t> blocked;
        encode_attr_runs(cell, attrs);
        encode_blocked_runs(cell, blocked);
        const std::uint32_t attrs_size = static_cast<std::uint32_t>(attrs.size());
        const std::uint32_t blocked_size = static_cast<std::uint32_t>(blocked.size());

        std::vector<std::uint8_t> payload;
        payload.reserve(attrs.size() + blocked.size());
        payload.insert(payload.end(), attrs.begin(), attrs.end());
        payload.insert(payload.end(), blocked.begin(), blocked.end());

        std::vector<std::uint8_t> bytes;
        bytes.reserve(kHeaderBytes + payload.size());
        bytes.insert(bytes.end(), {'A', 'K', 'A', 'R'});
        append_u16(bytes, kFormatVersion);
        append_u16(bytes, kHeaderBytes);
        append_i32(bytes, cell.cell_x());
        append_i32(bytes, cell.cell_y());
        append_u16(bytes, static_cast<std::uint16_t>(kTileCount));
        append_u16(bytes, static_cast<std::uint16_t>(AreaCell::kSubcellsPerTile));
        append_u32(bytes, static_cast<std::uint32_t>(kTileEntries));
        append_u32(bytes, static_cast<std::uint32_t>(kTileEntries));
        append_u32(bytes, attrs_size);
        append_u32(bytes, blocked_size);
        append_u32(bytes, checksum(payload));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return platform::File::write(std::string(path), bytes.data(), bytes.size());
    }

    bool AreaMapCodec::load(std::string_view path, AreaCell& out) {
        const std::string path_string(path);
        const auto bytes = platform::File::read(path_string);
        if (!bytes || bytes->size() < kHeaderBytes) {
            log::error("AreaMapCodec::load: cannot read header from " + path_string);
            return false;
        }
        if ((*bytes)[0] != 'A' || (*bytes)[1] != 'K' || (*bytes)[2] != 'A' || (*bytes)[3] != 'R') {
            log::error("AreaMapCodec::load: invalid magic in " + path_string);
            return false;
        }

        std::size_t offset = 4;
        std::uint16_t version = 0;
        std::uint16_t header_bytes = 0;
        std::int32_t cell_x = 0;
        std::int32_t cell_y = 0;
        std::uint16_t tiles = 0;
        std::uint16_t blocked_cells = 0;
        std::uint32_t attr_count = 0;
        std::uint32_t blocked_count = 0;
        std::uint32_t attrs_size = 0;
        std::uint32_t blocked_size = 0;
        std::uint32_t expected_checksum = 0;
        if (!read_u16(*bytes, offset, bytes->size(), version) ||
            !read_u16(*bytes, offset, bytes->size(), header_bytes) ||
            !read_i32(*bytes, offset, bytes->size(), cell_x) ||
            !read_i32(*bytes, offset, bytes->size(), cell_y) ||
            !read_u16(*bytes, offset, bytes->size(), tiles) ||
            !read_u16(*bytes, offset, bytes->size(), blocked_cells) ||
            !read_u32(*bytes, offset, bytes->size(), attr_count) ||
            !read_u32(*bytes, offset, bytes->size(), blocked_count) ||
            !read_u32(*bytes, offset, bytes->size(), attrs_size) ||
            !read_u32(*bytes, offset, bytes->size(), blocked_size) ||
            !read_u32(*bytes, offset, bytes->size(), expected_checksum) ||
            version != kFormatVersion || header_bytes != kHeaderBytes || tiles != kTileCount ||
            blocked_cells != AreaCell::kSubcellsPerTile || attr_count != kTileEntries ||
            blocked_count != kTileEntries || offset != kHeaderBytes ||
            attrs_size > bytes->size() - offset ||
            blocked_size != bytes->size() - offset - attrs_size) {
            log::error("AreaMapCodec::load: unsupported or malformed header in " + path_string);
            return false;
        }

        const std::size_t attrs_end = offset + attrs_size;
        const std::size_t payload_end = attrs_end + blocked_size;
        std::vector<std::uint8_t> payload(bytes->begin() + static_cast<std::ptrdiff_t>(offset),
                                          bytes->begin() +
                                              static_cast<std::ptrdiff_t>(payload_end));
        if (checksum(payload) != expected_checksum ||
            !decode_attr_runs(*bytes, offset, attrs_end, out) ||
            !decode_blocked_runs(*bytes, attrs_end, payload_end, out)) {
            log::error("AreaMapCodec::load: corrupt payload in " + path_string);
            return false;
        }
        out.set_cell_x(cell_x);
        out.set_cell_y(cell_y);
        return true;
    }

}  // namespace openkal::map
