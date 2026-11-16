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
#include "fixture_util.h"
#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/map/MapStream.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

int main() {
    using openkal::test::expect;

    {
        int tx = 0;
        int ty = 0;
        openkal::coords::pixel_to_tile(257285, 258744, tx, ty);
        expect(tx == (257285 >> 5) && ty == (258744 >> 5), "pixel_to_tile Narootuh");
        expect(openkal::coords::in_tile_rect(100, 100, 100, 100, 4), "tile rect includes center");
        expect(openkal::coords::in_tile_rect(100, 100, 228, 228, 4),
               "tile rect includes Chebyshev radius 4 corner");
        expect(!openkal::coords::in_tile_rect(100, 100, 256, 256, 4),
               "tile rect excludes Chebyshev radius 5");
        int px = 0;
        int py = 0;
        openkal::coords::tile_to_pixel(tx, ty, px, py);
        expect(px == (tx << 5) && py == (ty << 5), "tile_to_pixel roundtrip");
    }

    {
        int cx = 0;
        int cy = 0;
        int lx = 0;
        int ly = 0;
        openkal::coords::world_tile_to_cell(256 + 10, 512 + 20, cx, cy, lx, ly);
        expect(cx == 1 && cy == 2 && lx == 10 && ly == 20, "world_tile_to_cell");
    }

    expect(openkal::map::attr_mask(openkal::map::AttrBit::kNotMoveable) == 0x8000u,
           "attr_mask bit0");
    expect(openkal::map::kAttrPortal == (0x8000u << 1), "attr portal");

    const std::string root = openkal::test::temp_dir("map");
    openkal::map::AreaCell grid;
    grid.set_version(openkal::map::kVersion);
    grid.assign_attrs(256u * 256u, 0u);
    grid.ensure_blocked_mask(false);
    grid.ensure_not_moveable_mask(false);
    for (int ty = 0; ty < openkal::map::kTileCount; ++ty) {
        for (int tx = 0; tx < openkal::map::kTileCount; ++tx) {
            if (((tx + ty) & 1) != 0) {
                grid.set_not_moveable_mask(tx, ty, 0xffffu);
            }
        }
    }
    expect(grid.attr_count() == 256u * 256u, "area attribute count");
    expect(grid.version() == openkal::map::kVersion, "area version");

    int walkable = 0;
    int blocked = 0;
    grid.for_each_in_rect(0, 0, 255, 255, [&](int, int, std::uint32_t a) {
        if (openkal::map::has_attr(a, openkal::map::kAttrNotMoveable)) {
            ++blocked;
        } else {
            ++walkable;
        }
    });
    expect(walkable + blocked == 256 * 256, "rect covers all tiles");
    expect(walkable > 0 && blocked > 0, "mix of walkable/blocked");
    expect(grid.walkable(0, 0) == !grid.has_flag(0, 0, openkal::map::kAttrNotMoveable),
           "walkable vs has_flag");
    expect(openkal::map::AreaCell::kSubcellsPerTile == 4,
           "area maps use four subcells per tile axis");
    grid.set_blocked_at_subcell(3, 3, true);
    expect(grid.blocked_at_subcell(3, 3), "last blocked subcell in a tile can be set");
    expect(!grid.blocked_at_subcell(2, 3) && !grid.blocked_at_subcell(4, 3),
           "blocked subcells have independent eight-pixel precision");
    grid.set_blocked_at_subcell(3, 3, false);
    grid.set_not_moveable_at_subcell(1, 2, true);
    expect(grid.not_moveable_at_subcell(1, 2), "not-moveable subcell can be set");
    expect(!grid.not_moveable_at_subcell(0, 2) && !grid.not_moveable_at_subcell(2, 2),
           "not-moveable subcells have independent eight-pixel precision");
    grid.set_not_moveable_at_subcell(1, 2, false);

    {
        grid.set_cell_x(31);
        grid.set_cell_y(31);
        const std::string out = openkal::platform::join_path(root, "n_031_031.akarea");
        const std::string duplicate = openkal::platform::join_path(root, "n_031_031_copy.akarea");
        expect(openkal::map::AreaMapCodec::save(out, grid), "write area map");
        expect(openkal::map::AreaMapCodec::save(duplicate, grid), "write deterministic area map");
        const auto first = openkal::platform::read_file(out);
        const auto second = openkal::platform::read_file(duplicate);
        expect(first && second && *first == *second, "area map serialization is deterministic");
        expect(first && first->size() >= 6u && (*first)[4] == 2u && (*first)[5] == 0u,
               "area map writes format version 2");

        openkal::map::AreaCell again;
        expect(openkal::map::AreaMapCodec::load(out, again), "load area map");
        expect(again.cell_x() == 31 && again.cell_y() == 31, "area map coordinates");
        expect(again.attrs_equal(grid), "area map attributes roundtrip");
        expect(again.blocked_masks() == grid.blocked_masks(), "area map blocked masks roundtrip");
        expect(again.not_moveable_masks() == grid.not_moveable_masks(),
               "area map not-moveable masks roundtrip");
    }

    {
        openkal::map::AreaCell smooth;
        smooth.assign_attrs(256u * 256u, 0u);
        smooth.ensure_blocked_mask(false);
        smooth.ensure_not_moveable_mask(false);
        for (int y = 10; y <= 12; ++y) {
            for (int x = 10; x <= 12; ++x) {
                smooth.set_blocked_at_subcell(x, y, true);
                smooth.set_not_moveable_at_subcell(x + 20, y, true);
            }
        }
        for (int x = 50; x <= 58; ++x) {
            smooth.set_blocked_at_subcell(x, 50, true);
        }
        for (int y = 70; y <= 71; ++y) {
            for (int x = 70; x <= 71; ++x) {
                smooth.set_blocked_at_subcell(x, y, true);
            }
        }

        const openkal::area_import::SmoothStats stats =
            openkal::area_import::smooth_imported_area(smooth);
        expect(stats.blocked_removed == 4 && stats.not_moveable_removed == 4,
               "area smoothing removes only the four corners of each 3x3 block");
        expect(!smooth.blocked_at_subcell(10, 10) && smooth.blocked_at_subcell(11, 10) &&
                   smooth.blocked_at_subcell(11, 11),
               "area smoothing rounds a blocked square into a cross");
        expect(!smooth.not_moveable_at_subcell(30, 10) && smooth.not_moveable_at_subcell(31, 10) &&
                   smooth.not_moveable_at_subcell(31, 11),
               "area smoothing rounds an independent not-moveable square");
        expect(smooth.blocked_at_subcell(50, 50) && smooth.blocked_at_subcell(58, 50),
               "area smoothing preserves thin lines and their endpoints");
        expect(smooth.blocked_at_subcell(70, 70) && smooth.blocked_at_subcell(71, 71),
               "area smoothing preserves small components");
    }

    // external asset pack alt tag 4096 — layout A: [4096][attrs...]
    {
        const std::string path = openkal::platform::join_path(root, "alt_a.akarea");
        std::vector<std::uint8_t> bytes;
        const std::int32_t tag = 4096;
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(&tag),
                     reinterpret_cast<const std::uint8_t*>(&tag) + sizeof(tag));
        std::vector<std::uint32_t> attrs(256u * 256u, 0xffffu);
        attrs[0] = 0xabcd0001u;
        attrs[1] = 0x12348000u;
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(attrs.data()),
                     reinterpret_cast<const std::uint8_t*>(attrs.data()) +
                         attrs.size() * sizeof(std::uint32_t));
        expect(openkal::test::write_bytes(path, bytes), "write alt A");
        openkal::map::AreaCell g;
        expect(openkal::area_import::import_legacy_area(path, g), "import alt A");
        expect(g.version() == openkal::map::kVersion, "alt A logical version");
        expect(g.attrs_data() != nullptr && g.attr_at_flat(0) == 0xabcd0000u,
               "alt A tile flags preserved without legacy blocked bits");
        expect(g.blocked_at_subcell(0, 0), "alt A legacy blocked bit is preserved");
        expect(!g.blocked_at_subcell(1, 0), "alt A adjacent blocked subcell stays open");
        expect(!g.not_moveable_at_subcell(0, 0), "alt A tile without not-moveable remains open");
        expect(g.blocked_at_subcell(7, 3), "alt A final legacy blocked bit is preserved");
        expect(!g.not_moveable_at_subcell(4, 0) && !g.not_moveable_at_subcell(7, 3),
               "legacy bit 15 must not expand into a whole-tile movement restriction");
        expect(g.attr_at_flat(1) == 0x12340000u, "legacy bit 15 is not a zone flag");
    }

    // external asset pack alt tag 4096 — layout B: [4096][1][attrs...] (one tile short,
    // padded).
    {
        const std::string path = openkal::platform::join_path(root, "alt_b.akarea");
        std::vector<std::uint8_t> bytes;
        const std::int32_t tag = 4096;
        const std::int32_t ver = openkal::map::kVersion;
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(&tag),
                     reinterpret_cast<const std::uint8_t*>(&tag) + sizeof(tag));
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(&ver),
                     reinterpret_cast<const std::uint8_t*>(&ver) + sizeof(ver));
        std::vector<std::uint32_t> attrs(256u * 256u - 1u, 0xffffu);
        attrs[0] = 0x11112222u;
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(attrs.data()),
                     reinterpret_cast<const std::uint8_t*>(attrs.data()) +
                         attrs.size() * sizeof(std::uint32_t));
        expect(bytes.size() == 4u + 4u + (256u * 256u - 1u) * 4u, "alt B plain size");
        expect(openkal::test::write_bytes(path, bytes), "write alt B");
        openkal::map::AreaCell g;
        expect(openkal::area_import::import_legacy_area(path, g), "import alt B");
        expect(g.version() == openkal::map::kVersion, "alt B logical version");
        expect(g.attr_at_flat(0) == 0x11110000u, "alt B first tile flag is not shifted version");
        expect(g.attr_at_flat(g.attr_count() - 1) == 0u, "alt B padded last tile");
    }

    // Later client DEF_PATH_FINDING_LETSBE_090218: [version][attr DWORD +
    // object WORD] * 65536 = 393220 bytes. Object words must not leak into attrs.
    {
        const std::string path = openkal::platform::join_path(root, "interleaved.akarea");
        std::vector<std::uint8_t> bytes;
        const std::int32_t ver = openkal::map::kVersion;
        bytes.insert(bytes.end(),
                     reinterpret_cast<const std::uint8_t*>(&ver),
                     reinterpret_cast<const std::uint8_t*>(&ver) + sizeof(ver));
        const std::uint16_t object_poison = 0xfedcu;
        for (std::size_t i = 0; i < 256u * 256u; ++i) {
            const std::uint32_t attr = (i == 0) ? 0xabcdef01u : ((i == 1) ? 0x11111111u : 0xffffu);
            bytes.insert(bytes.end(),
                         reinterpret_cast<const std::uint8_t*>(&attr),
                         reinterpret_cast<const std::uint8_t*>(&attr) + sizeof(attr));
            bytes.insert(bytes.end(),
                         reinterpret_cast<const std::uint8_t*>(&object_poison),
                         reinterpret_cast<const std::uint8_t*>(&object_poison) +
                             sizeof(object_poison));
        }
        expect(bytes.size() == 393220u, "interleaved size");
        expect(openkal::test::write_bytes(path, bytes), "write interleaved");
        openkal::map::AreaCell g;
        expect(openkal::area_import::import_legacy_area(path, g), "import interleaved");
        expect(g.version() == openkal::map::kVersion, "interleaved logical version");
        expect(g.attr_at_flat(0) == 0xabcd0000u, "interleaved first tile flag");
        expect(g.attr_at_flat(1) == 0x11110000u,
               "interleaved second tile flag not mixed with WORD");
        expect(g.attr_at_flat(2) == 0u, "interleaved collision bits do not become zone flags");
    }

    // Historical server-resource maps may contain an opaque 12- or 13-byte
    // record layout instead of SVMAP attributes. Import must remain safe until
    // those cells receive explicit area data in the new editor.
    {
        const std::string path = openkal::platform::join_path(root, "opaque.akarea");
        std::vector<std::uint8_t> bytes(1078u + 12u * 256u * 256u, 0u);
        expect(openkal::test::write_bytes(path, bytes), "write opaque legacy layout");
        openkal::map::AreaCell g;
        expect(openkal::area_import::import_legacy_area(path, g), "import opaque legacy layout");
        expect(!g.walkable(0, 0), "opaque legacy tile is conservatively blocked");
        expect(g.blocked_at_subcell(0, 0), "opaque legacy subcell is conservatively blocked");
    }

    {
        openkal::map::WorldGrid world;
        grid.set_cell_x(31);
        grid.set_cell_y(31);
        world.set_cell(31, 31, grid);
        expect(world.cell_count() == 1, "world one cell");
        const int world_tx = (31 << 8) + 10;
        const int world_ty = (31 << 8) + 20;
        expect(world.walkable_world_tile(world_tx, world_ty) == grid.walkable(10, 20),
               "world walkable maps to local");
    }

    {
        const std::string path = openkal::map::MapStream::cell_path(root, 31, 31);
        grid.set_cell_x(31);
        grid.set_cell_y(31);
        expect(openkal::map::AreaMapCodec::save(path, grid), "write area map fixture");
        expect(path.find("n_031_031.akarea") != std::string::npos, "MapStream::cell_path name");
        expect(openkal::map::MapStream::cell_exists(root, 31, 31),
               "MapStream::cell_exists for present area map");
        expect(!openkal::map::MapStream::cell_exists(root, 30, 31),
               "MapStream::cell_exists rejects an absent area map");
        openkal::map::WorldGrid streamed;
        expect(openkal::map::MapStream::ensure_cell(streamed, root, 31, 31),
               "MapStream::ensure_cell");
        expect(streamed.cell_count() == 1, "streamed one cell");
        expect(openkal::map::MapStream::ensure_cell(streamed, root, 31, 31),
               "ensure_cell idempotent");
        expect(streamed.cell_count() == 1, "streamed still one cell");
        expect(!openkal::map::MapStream::ensure_cell(streamed, root, 30, 31),
               "missing area map stays unavailable");
        expect(streamed.cell_count() == 1, "missing area map does not add a cell");
        const int world_px = ((31 << 8) + 10) << 5;
        const int world_py = ((31 << 8) + 20) << 5;
        expect(openkal::map::MapStream::ensure_around(streamed, root, world_px, world_py, 0) == 1,
               "MapStream::ensure_around radius 0");
    }

    {
        const std::string catalog_path = openkal::platform::join_path(root, "mapinfo.json");
        {
            std::ofstream out(catalog_path);
            out << R"({"maps":[
  {"index":1,"kind":1,"xy":[31,31],"filename":"n_031_031.akarea"},
  {"index":2,"kind":0,"xy":[32,32]}
]})";
        }
        openkal::map::MapCatalog catalog;
        expect(catalog.load(catalog_path), "MapCatalog::load");
        expect(catalog.size() == 2, "MapCatalog size");
        expect(catalog.entries()[0].index() == 1 && catalog.entries()[0].kind() == 1,
               "entry0 fields");
        expect(catalog.entries()[0].x() == 31 && catalog.entries()[0].y() == 31, "entry0 xy");
        expect(catalog.entries()[0].filename() == "n_031_031.akarea", "entry0 filename");
        expect(catalog.entries()[1].kind() == 0 && catalog.entries()[1].filename().empty(),
               "entry1 indoor/no file");
    }

    {
        const std::string catalog_path = openkal::platform::join_path(root, "mapinfo.json");
        {
            std::ofstream out(catalog_path);
            out << R"({"maps":[
  {"index":2,"kind":1,"fogEnd":2000,"fogColor":[0.02,0.07,0.04],
   "xy":[6,4],"sizeX":2,"sizeY":2,"initPos":[51832,34603],
   "mapname":"Map\\tomb_01"}
]})";
        }
        openkal::map::MapCatalog catalog;
        expect(catalog.load(catalog_path), "map catalog with spaced fields");
        expect(catalog.size() == 1, "map catalog size");
        if (!catalog.entries().empty()) {
            expect(catalog.entries()[0].index() == 2 && catalog.entries()[0].x() == 6 &&
                       catalog.entries()[0].y() == 4,
                   "map catalog coordinates");
            expect(catalog.entries()[0].mapname() == "Map\\tomb_01", "map catalog name");
            expect(catalog.entries()[0].fog_end() == 2000, "map catalog fog end");
            expect(catalog.entries()[0].fog_r() > 0.019f && catalog.entries()[0].fog_r() < 0.021f,
                   "map catalog fog red component");
            expect(catalog.entries()[0].size_x() == 2 && catalog.entries()[0].size_y() == 2,
                   "map catalog dimensions");
            expect(catalog.entries()[0].init_pos_x() == 51832 &&
                       catalog.entries()[0].init_pos_y() == 34603,
                   "map catalog initial position");
            const auto* cell = catalog.find_cell(2, 6, 4);
            expect(cell != nullptr && cell->mapname() == "Map\\tomb_01", "find_cell");
            expect(catalog.find_cell(2, 0, 0) == nullptr, "find_cell miss");
            expect(catalog.find_index(2) == cell, "find_index");
            expect(catalog.find_named_cell(6, 4) == cell, "find_named_cell");
            expect(catalog.index_at_pixel(50204, 34102) == 2, "index_at_pixel d1");
            expect(catalog.resolve_teleport_map(50204, 34102, 0) == 2, "resolve named indoor map");
            expect(catalog.resolve_teleport_map(0, 0, 7) == 7, "resolve keeps fallback outdoors");
            expect(catalog.has_indoor_mapname(2), "dungeon has indoor mapname");
            expect(!catalog.has_indoor_mapname(0), "overworld has no indoor mapname");
            expect(!catalog.has_indoor_mapname(20), "missing index has no indoor mapname");
        }
    }

    {
        openkal::map::MapCatalog catalog;
        expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/mapinfo.json"),
               "load config/mapinfo.json");
        expect(catalog.size() == 518, "mapinfo.json cell count");
        const openkal::map::MapInfo* tomb = catalog.find_named_cell(6, 4);
        expect(tomb != nullptr && tomb->index() == 2 && tomb->mapname() == "Map\\tomb_01",
               "mapinfo.json tomb_01 cell");
        if (tomb != nullptr) {
            expect(tomb->fog_end() == 2000, "mapinfo.json tomb_01 fogEnd");
            expect(tomb->size_x() == 2 && tomb->size_y() == 2, "mapinfo.json tomb_01 size");
            expect(tomb->init_pos_x() == 51832 && tomb->init_pos_y() == 34603,
                   "mapinfo.json tomb_01 initPos");
        }
        const openkal::map::MapInfo* outdoor = catalog.find_cell(0, 47, 24);
        expect(outdoor != nullptr && outdoor->mapname().empty() &&
                   outdoor->filename() == "data\\MAPS\\n_047_024.akarea",
               "mapinfo.json outdoor area cell");
        expect(catalog.load_json_text(R"({"maps":[{"index":9,"kind":1,"x":3,"y":4,
            "fogEnd":4000,"fogColor":[0.2,0.1,0.1],"sizeX":2,"sizeY":2,
            "initPos":[1,2],"mapname":"Map\\tomb"}]})"),
               "load_json_text");
        expect(catalog.size() == 1 && catalog.entries()[0].index() == 9 &&
                   catalog.entries()[0].fog_end() == 4000 && catalog.entries()[0].init_pos_x() == 1,
               "load_json_text fields");
    }

    {
        openkal::map::MapCatalog catalog;
        expect(catalog.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/mapinfo.json"),
               "load config/mapinfo.json");
        expect(catalog.size() == 362, "mapinfo.json cell count");
        const openkal::map::MapInfo* cell = catalog.find_cell(1, 0, 0);
        expect(cell != nullptr && cell->kind() == 1 && cell->filename() == "n_000_000.akarea",
               "mapinfo.json first area cell");
        const openkal::map::MapInfo* d6 = catalog.find_cell(31, 8, 2);
        expect(d6 != nullptr && d6->filename() == "n_008_002.akarea", "mapinfo.json D6 floor cell");
    }

    {
        const std::string catalog_path =
            openkal::platform::join_path(root, "emok_map_catalog.json");
        {
            std::ofstream out(catalog_path);
            out << R"({"maps":[
  {"index":0,"kind":1,"xy":[41,31],"filename":"n_041_031.akarea"},
  {"index":20,"kind":1,"xy":[41,31],"filename":"n_041_031.akarea"},
  {"index":2,"kind":1,"xy":[6,4],"mapname":"Map\\tomb_01"}
]})";
        }
        openkal::map::MapCatalog catalog;
        expect(catalog.load(catalog_path), "emok map definition load");
        expect(!catalog.has_indoor_mapname(20), "emok map 20 reuses outdoor KCM");
        expect(catalog.has_indoor_mapname(2), "named dungeon stays indoor");
        const int emok_px = openkal::coords::cell_origin_pixels(41) + 100;
        const int emok_py = openkal::coords::cell_origin_pixels(31) + 100;
        expect(catalog.index_at_pixel(emok_px, emok_py) == 0, "emok cell stays outdoor index");
        expect(catalog.resolve_teleport_map(emok_px, emok_py, 20) == 20,
               "emok teleport keeps outdoor map");
    }

    {
        expect(openkal::coords::even_cell(7) == 6, "even_cell");
        expect(openkal::coords::cell_origin_pixels(6) == 6 * 256 * 32, "cell_origin_pixels");
        const int origin = 32 * 256 * 32;
        const int shift = openkal::coords::cell_origin_pixels(6) - origin;
        const float obj_x = 1828.0f + static_cast<float>(shift);
        const float player_x = openkal::coords::to_client(50204);
        expect(std::fabs(obj_x - player_x) < 2500.0f, "d1 cell origin lines up with player");
    }

    {
        const std::string map_path = openkal::platform::join_path(root, "tiny.map");
        std::vector<std::uint8_t> bytes(60 + 18 + 52, 0);
        bytes[0] = 2;  // version
        bytes[1] = 0;  // region_count
        const char strings[] = "tiny.map\0lib/a.gb";
        const std::uint32_t strsz = 18;
        std::memcpy(bytes.data() + 12, &strsz, 4);
        const std::uint16_t objects = 1;
        std::memcpy(bytes.data() + 46, &objects, 2);
        std::memcpy(bytes.data() + 60, strings, 18);
        const std::uint32_t path_off = 9;
        std::memcpy(bytes.data() + 78, &path_off, 4);
        const float ident[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 100, 200, 300};
        std::memcpy(bytes.data() + 82, ident, sizeof(ident));
        expect(openkal::platform::write_file(map_path, bytes.data(), bytes.size()),
               "write tiny.map");
        openkal::map::IndoorMap indoor;
        expect(indoor.load(map_path), "IndoorMap::load tiny");
        expect(indoor.stem() == "tiny", "indoor stem");
        expect(indoor.regions().empty(), "tiny has no regions");
        expect(indoor.objects().size() == 1, "tiny one object");
        expect(indoor.objects()[0].path() == "lib/a.gb", "tiny object path");
        expect(indoor.objects()[0].matrix()[9] == 100.0f &&
                   indoor.objects()[0].matrix()[11] == 300.0f,
               "tiny object translation");
        expect(openkal::map::IndoorMap::stem_from_mapname("Map\\tomb") == "tomb",
               "stem_from_mapname");
    }

    {
        const std::string map_path = openkal::platform::join_path(root, "parts.map");
        std::vector<std::uint8_t> bytes;
        auto append = [&bytes](const auto& value) {
            const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), first, first + sizeof(value));
        };
        auto append_floats = [&bytes](const float* values, std::size_t count) {
            const auto* first = reinterpret_cast<const std::uint8_t*>(values);
            bytes.insert(bytes.end(), first, first + count * sizeof(float));
        };

        append(std::uint8_t{2});
        append(std::uint8_t{1});
        append(std::uint16_t{1});
        append(std::uint32_t{0});
        append(std::uint16_t{1});
        append(std::uint16_t{2});
        append(std::uint32_t{30});
        const float map_maximum[3] = {20.0f, 20.0f, 20.0f};
        const float map_minimum[3] = {-20.0f, -20.0f, -20.0f};
        append_floats(map_maximum, 3);
        append_floats(map_minimum, 3);
        append(std::uint32_t{0});
        append(std::uint16_t{0});
        append(std::uint16_t{0});
        const std::uint32_t reserved = 0;
        append(reserved);
        append(reserved);
        append(reserved);

        const char names[] = "parts.map\0region\0part0\0part1\0";
        bytes.insert(bytes.end(), names, names + sizeof(names) - 1);
        append(std::uint8_t{0x03});  // region present, part 0 visible, part 1 hidden

        append(std::uint32_t{29});
        const float plane[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        append_floats(plane, 4);
        append(std::uint16_t{0});
        append(std::uint16_t{0xffff});
        append(std::uint16_t{0xffff});

        append(std::uint32_t{10});
        append(std::uint32_t{2});
        append_floats(map_maximum, 3);
        append_floats(map_minimum, 3);
        append(std::uint32_t{29});
        append(reserved);
        append(reserved);

        const float part0_maximum[3] = {10.0f, 10.0f, 10.0f};
        const float part0_minimum[3] = {0.0f, 0.0f, 0.0f};
        append(std::uint32_t{17});
        append_floats(part0_maximum, 3);
        append_floats(part0_minimum, 3);
        append(reserved);
        append(reserved);
        const float part1_maximum[3] = {20.0f, 20.0f, 20.0f};
        const float part1_minimum[3] = {10.0f, 10.0f, 10.0f};
        append(std::uint32_t{23});
        append_floats(part1_maximum, 3);
        append_floats(part1_minimum, 3);
        append(reserved);
        append(reserved);

        expect(openkal::platform::write_file(map_path, bytes.data(), bytes.size()),
               "write parts.map");
        openkal::map::IndoorMap indoor;
        expect(indoor.load(map_path), "IndoorMap::load parts");
        expect(indoor.regions().size() == 1 && indoor.regions()[0].parts().size() == 2,
               "indoor parses region parts");
        expect(indoor.regions()[0].parts()[0].name() == "part0" &&
                   indoor.regions()[0].parts()[1].minimum()[0] == 10.0f,
               "indoor keeps part order and bounds");
        expect(indoor.leaves().size() == 1 && indoor.leaves()[0].regions().size() == 1,
               "indoor links region to leaf");
        std::vector<std::uint8_t> visibility;
        if (!indoor.leaves().empty() && !indoor.leaves()[0].regions().empty()) {
            expect(
                indoor.collect_region_part_visibility(indoor.leaves()[0].regions()[0], visibility),
                "indoor reads complete part PVS");
        }
        expect(visibility.size() == 2 && visibility[0] == 1 && visibility[1] == 0,
               "indoor preserves per-part PVS bits");
        const openkal::map::IndoorBspLeaf::RegionLink truncated_link(0, 239);
        expect(!indoor.collect_region_part_visibility(truncated_link, visibility),
               "truncated part PVS requests conservative fallback");
    }

    {
        const float cx = openkal::coords::to_client(openkal::coords::kNarootuhServerX);
        const float back = static_cast<float>(openkal::coords::to_server(cx));
        expect(std::abs(back - openkal::coords::kNarootuhServerX) < 1.0f, "to_client/to_server");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
