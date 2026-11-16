/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MinimapWidget.h"
#include "openkal/map/Coords.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/map/MapStream.h"
#include "openkal/map/MiniMapSize.h"
#include "openkal/map/Portal.h"
#include "test_util.h"

#include <cmath>
#include <string>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::client::MinimapWidget;
    using openkal::map::kMapSize;
    using openkal::map::kPatchGrid;
    using openkal::map::kTerrainCellSize;
    using openkal::map::kWorldSize;
    using openkal::map::MiniMapSize;
    using openkal::map::MiniMapSizeCatalog;
    using openkal::test::expect;

    const float center_cells = static_cast<float>(kMapSize * kPatchGrid) * 0.5f;
    expect(std::fabs(center_cells - 8192.0f) < 0.01f, "map centre in cells is 8192");

    expect(std::fabs(MinimapWidget::atlas_x(0.0f) - 4096.0f) < 0.01f,
           "world x=0 is atlas centre x");
    expect(std::fabs(MinimapWidget::atlas_y(0.0f) - 4096.0f) < 0.01f,
           "world z=0 is atlas centre y");

    const float one_patch = static_cast<float>(kMapSize * kTerrainCellSize);
    expect(std::fabs(MinimapWidget::atlas_x(one_patch) - 4224.0f) < 0.01f,
           "one patch east is +128 atlas px");
    expect(std::fabs(MinimapWidget::atlas_y(one_patch) - 3968.0f) < 0.01f,
           "one patch north is -128 atlas px");

    float left = 0, top = 0, right = 0, bottom = 0;
    MinimapWidget::tile_rect(32, 32, left, top, right, bottom);
    expect(std::fabs(left - 4096.0f) < 0.01f, "tile 32,32 left at atlas centre");
    expect(std::fabs(right - 4352.0f) < 0.01f, "tile 32,32 is 256 px wide");
    expect(std::fabs(top - 3840.0f) < 0.01f, "tile 32,32 top");
    expect(std::fabs(bottom - 4096.0f) < 0.01f, "tile 32,32 bottom at atlas centre");

    MinimapWidget::tile_rect(0, 0, left, top, right, bottom);
    expect(std::fabs(left) < 0.01f, "tile 0,0 left");
    expect(std::fabs(top - (8192.0f - 256.0f)) < 0.01f, "tile 0,0 top is atlas south");

    expect(MinimapWidget::sanitize_zoom(0) == MinimapWidget::kZoomNormal, "sanitize 0 is 1x");
    expect(MinimapWidget::sanitize_zoom(1) == MinimapWidget::kZoomNormal, "sanitize 1 is 1x");
    expect(MinimapWidget::sanitize_zoom(3) == MinimapWidget::kZoomNormal, "sanitize 3 is 1x");
    expect(MinimapWidget::sanitize_zoom(5) == MinimapWidget::kZoomA, "sanitize 5 is 4x");
    expect(MinimapWidget::sanitize_zoom(8) == MinimapWidget::kZoomB, "sanitize 8 is 8x");
    expect(MinimapWidget::step_zoom_in(1) == MinimapWidget::kZoomA, "extend 1x to 4x");
    expect(MinimapWidget::step_zoom_in(4) == MinimapWidget::kZoomB, "extend 4x to 8x");
    expect(MinimapWidget::step_zoom_in(8) == MinimapWidget::kZoomB, "extend 8x stays 8x");
    expect(MinimapWidget::step_zoom_out(8) == MinimapWidget::kZoomA, "reduce 8x to 4x");
    expect(MinimapWidget::step_zoom_out(4) == MinimapWidget::kZoomNormal, "reduce 4x to 1x");
    expect(MinimapWidget::step_zoom_out(1) == MinimapWidget::kZoomNormal, "reduce 1x stays 1x");

    const std::string repo_root = OPENKAL_TEST_RESOURCE_ROOT;
    openkal::map::WorldGrid areas;
    expect(openkal::map::MapStream::ensure_cell(areas, repo_root + "/RuntimeData/areas", 31, 31),
           "load shipped Narootuh area");
    auto area_at_tile = [&](int x, int y, int origin = 32) {
        return MinimapWidget::area_name(openkal::coords::to_client(x * 32, origin),
                                        openkal::coords::to_client(y * 32, origin),
                                        origin,
                                        areas);
    };
    expect(area_at_tile(8040, 8085) == "Village of Narootuh",
           "Narootuh spawn resolves to its minimap area");
    expect(!areas.has_attr_world_pixel(8100 * 32, 8085 * 32, openkal::map::kAttrVillage) &&
               areas.has_attr_world_pixel(8100 * 32, 8085 * 32, openkal::map::kAttrSafety),
           "shipped tile inside the old city rectangle is safe but outside the village");
    expect(area_at_tile(8100, 8085) == "Naraeha",
           "leaving the actual village clears the city label even inside its bounding box");
    expect(area_at_tile(8150, 8085) == "Naraeha",
           "outside Narootuh's city zone does not retain the city label");
    expect(area_at_tile(8040, 8085) == "Village of Narootuh",
           "returning to the village restores the city label");
    expect(area_at_tile(8040, 8085, 0) == "Village of Narootuh" &&
               area_at_tile(8100, 8085, 0) == "Naraeha",
           "village boundary lookup respects a non-default coordinate origin");
    expect(area_at_tile(8750, 7250) == "Forest of Statues",
           "non-settlement regions do not require a village flag");
    expect(area_at_tile(0, 0) == "Naraeha", "unnamed overworld falls back to Naraeha");
    openkal::map::WorldGrid missing_areas;
    expect(MinimapWidget::area_name(openkal::coords::to_client(257285),
                                    openkal::coords::to_client(258744),
                                    32,
                                    missing_areas) == "Naraeha",
           "missing area data cannot invent a city label from the bounding box");

    struct Settlement {
        int x;
        int y;
        const char* name;
    };
    for (const auto& settlement : {Settlement{8040, 8085, "Village of Narootuh"},
                                   Settlement{8280, 8180, "Jook-Suh Cargo Station"},
                                   Settlement{8310, 8900, "Geum-Oh Mine"},
                                   Settlement{8010, 9020, "The Pub of the Giant Bird"},
                                   Settlement{8380, 7580, "Temporary Fort"},
                                   Settlement{7280, 9160, "The City of Priest"}}) {
        expect(openkal::map::MapStream::ensure_cell(
                   areas, repo_root + "/RuntimeData/areas", settlement.x / 256, settlement.y / 256),
               "load shipped settlement area");
        expect(area_at_tile(settlement.x, settlement.y) == settlement.name,
               "shipped village tile uses the matching settlement name");
        auto* area = areas.cell(settlement.x / 256, settlement.y / 256);
        if (area != nullptr) {
            const std::size_t offset =
                static_cast<std::size_t>(settlement.y % 256) * 256 + settlement.x % 256;
            area->attrs_data()[offset] &= ~openkal::map::kAttrVillage;
            area->attrs_data()[offset] |= openkal::map::kAttrSafety;
            expect(area_at_tile(settlement.x, settlement.y) != settlement.name,
                   "safety without Village does not retain any settlement name");
        }
    }

    openkal::map::PortalCatalog portals;
    expect(portals.load_json(repo_root + "/config/portals.json"), "load dungeon portal IDs");
    for (int dungeon = 1; dungeon <= 6; ++dungeon) {
        const auto* portal = portals.entrance_for_dungeon(dungeon);
        expect(portal != nullptr, "dungeon entrance has a map ID");
        if (portal != nullptr) {
            expect(MinimapWidget::dungeon_area_name(portal->to_map()) ==
                       "D" + std::to_string(dungeon),
                   "dungeon label follows the portal destination, not its asset stem");
        }
    }
    expect(MinimapWidget::dungeon_area_name(0) == "Dungeon" &&
               MinimapWidget::dungeon_area_name(999) == "Dungeon",
           "unknown indoor map IDs use Dungeon instead of leaking an outdoor name");
    openkal::map::MapCatalog map_catalog;
    expect(map_catalog.load(repo_root + "/config/mapinfo.json"), "load dungeon floor map IDs");
    for (const auto& entry : map_catalog.entries()) {
        const auto& name = entry.mapname();
        if (name.rfind("Map\\jsa", 0) == 0) {
            expect(MinimapWidget::dungeon_area_name(entry.index()) == "D5",
                   "all Tower of Priest floor IDs display D5");
        } else if (name == "Map\\d_tower_01") {
            expect(MinimapWidget::dungeon_area_name(entry.index()) == "D6",
                   "all Tower of Devah floor IDs display D6");
        } else if (name.rfind("Map\\d03_tomb_", 0) == 0) {
            expect(MinimapWidget::dungeon_area_name(entry.index()) == "D4",
                   "Giant Bird cave floors display D4");
        } else if (name.rfind("Map\\tomb", 0) == 0) {
            const std::string_view expected = name == "Map\\tomb_01" ? "D1"
                                              : name == "Map\\tomb"  ? "D2"
                                                                     : "D3";
            expect(MinimapWidget::dungeon_area_name(entry.index()) == expected,
                   "tomb assets in distinct dungeons retain distinct labels");
        }
    }

    expect(std::fabs(MinimapWidget::dungeon_atlas_x(0.0f, 32, 32, 0, 0)) < 0.01f,
           "dungeon origin is atlas x=0");
    expect(std::fabs(MinimapWidget::dungeon_atlas_y(0.0f, 32, 32, 0, 0, 512) - 512.0f) < 0.01f,
           "dungeon origin is atlas south");
    expect(std::fabs(
               MinimapWidget::dungeon_atlas_x(static_cast<float>(kTerrainCellSize), 32, 32, 0, 0) -
               1.0f) < 0.01f,
           "one dungeon cell east is +1 atlas px");
    expect(std::fabs(
               MinimapWidget::dungeon_atlas_y(static_cast<float>(kWorldSize), 32, 32, 0, 0, 512) -
               256.0f) < 0.01f,
           "one dungeon patch north is atlas y=256");

    const float d1_origin_wx = openkal::coords::to_client(6 * kWorldSize, 32);
    const float d1_origin_wz = openkal::coords::to_client(4 * kWorldSize, 32);
    expect(std::fabs(MinimapWidget::dungeon_atlas_x(d1_origin_wx, 32, 6, 0, 0)) < 0.01f,
           "D1 orgPatch 6,4 maps to atlas x=0");
    expect(std::fabs(MinimapWidget::dungeon_atlas_y(d1_origin_wz, 32, 4, 0, 0, 512) - 512.0f) <
               0.01f,
           "D1 orgPatch 6,4 maps to atlas south");

    MinimapWidget::dungeon_tile_rect(0, 256, 256, 512, left, top, right, bottom);
    expect(std::fabs(left) < 0.01f && std::fabs(top) < 0.01f, "dungeon tile 0 at 0,0");
    expect(std::fabs(right - 256.0f) < 0.01f && std::fabs(bottom - 256.0f) < 0.01f,
           "dungeon tile 0 is 256 px");
    MinimapWidget::dungeon_tile_rect(1, 256, 256, 512, left, top, right, bottom);
    expect(std::fabs(left - 256.0f) < 0.01f && std::fabs(top) < 0.01f, "dungeon tile 1 is NE");
    MinimapWidget::dungeon_tile_rect(2, 256, 256, 512, left, top, right, bottom);
    expect(std::fabs(left) < 0.01f && std::fabs(top - 256.0f) < 0.01f, "dungeon tile 2 is SW row");

    MiniMapSizeCatalog catalog;
    const std::string json_path =
        std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/ui/dungeon_minimaps.json";
    expect(catalog.load_json(json_path), "load dungeon_minimaps.json");
    const MiniMapSize* tomb01 = catalog.find("tomb_01");
    expect(tomb01 != nullptr && tomb01->size_x() == 512 && tomb01->tile_width() == 256,
           "tomb_01 size");
    expect(catalog.find("Map\\tomb_01") == nullptr, "lookup is by stem not Map\\\\ path");
    expect(catalog.find("") == nullptr, "empty mapname is outdoor");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
