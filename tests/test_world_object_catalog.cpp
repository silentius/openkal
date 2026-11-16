/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WorldObjectCatalog.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    using openkal::client::WorldObjectCatalog;
    using openkal::test::expect;

    const char* tmp = std::getenv("TMPDIR");
    if (tmp == nullptr || tmp[0] == '\0') {
        tmp = "/tmp";
    }
    const std::string dir =
        std::string(tmp) + "/openkal_world_objects_" + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0755);
    const std::string path = openkal::platform::join_path(dir, "world_objects.json");
    {
        std::ofstream out(path);
        out << R"({
  "models": [
    {
      "path": "MAP\\lib\\[ae]dg_fire",
      "particles": [
        { "name": "firepot_flame03", "effectBone": -1, "offset": [-11.117, 13.0, -0.78] }
      ]
    },
    {
      "path": "MAP\\lib\\effect_light",
      "particles": [
        { "name": "Wongori_16", "effectBone": -1, "offset": [0, 0, 0] }
      ]
    },
    {
      "path": "Map\\lib\\d03_obj_27",
      "particles": [
        ["Fire_25", -1, 0, 0, 0]
      ]
    }
  ]
}
)";
    }

    WorldObjectCatalog cat;
    expect(cat.load_json(path), "load world object JSON");
    expect(cat.size() == 3, "three models");

    const auto* fire = cat.find(R"(MAP\lib\[ae]dg_fire)");
    expect(fire != nullptr && fire->size() == 1, "dg_fire hit");
    expect(fire != nullptr && fire->at(0).name() == "firepot_flame03", "dg_fire particle");
    expect(fire != nullptr && fire->at(0).ox() == -11.117f && fire->at(0).oy() == 13.0f,
           "dg_fire offset");

    expect(cat.find("lib/[ae]dg_fire") != nullptr, "indoor relative without MAP");
    expect(cat.find(R"(MAP\lib\[ae]dg_fire.gb)") != nullptr, "trailing .gb ignored");
    expect(cat.find("map/lib/effect_light") != nullptr, "effect_light slash fold");
    expect(cat.find("MAP\\lib\\d03_obj_27") != nullptr, "d03 torch case fold");
    expect(cat.find(R"(MAP\lib_00_lamp\[ae]d02_0f_lamp)") == nullptr, "D5 lamp absent in fixture");

    const auto* light = cat.find(R"(MAP\lib\effect_light)");
    expect(light != nullptr && light->at(0).name() == "Wongori_16", "effect_light particle");

    expect(WorldObjectCatalog::indoor_lookup_key(R"(lib\[ae]dg_fire)") == R"(MAP\lib\[ae]dg_fire)",
           "indoor MAP prefix");
    expect(WorldObjectCatalog::indoor_lookup_key(R"(MAP\lib\effect_light)") ==
               R"(MAP\lib\effect_light)",
           "already prefixed");

    std::uint32_t serial = 1;
    const std::uint32_t a = WorldObjectCatalog::take_fx_entity(serial);
    const std::uint32_t b = WorldObjectCatalog::take_fx_entity(serial);
    expect((a & WorldObjectCatalog::kFxBase) == WorldObjectCatalog::kFxBase, "fx base bits");
    expect(a != b, "serial advances");

#ifdef OPENKAL_REPO_ROOT
    WorldObjectCatalog shipped;
    const std::string shipped_path =
        openkal::platform::join_path(std::string(OPENKAL_REPO_ROOT), "config/world_objects.json");
    expect(shipped.load_json(shipped_path), "load shipped world_objects.json");
    const auto* dg = shipped.find(R"(MAP\lib\[ae]dg_fire)");
    expect(dg != nullptr && dg->at(0).name() == "firepot_flame03", "shipped dg_fire");
    const auto* d5 = shipped.find(R"(MAP\lib_00_lamp\[ae]d02_0f_lamp)");
    expect(d5 != nullptr && d5->at(0).name() == "firepot_flame03", "D5 wall lamp fallback");
    expect(shipped.find(R"(MAP\lib\effect_light)") != nullptr, "shipped effect_light");
    const auto* d4_gate = shipped.find(R"(DATA\OBJECTS\b\8\structure\8-door2)");
    expect(d4_gate != nullptr && d4_gate->size() == 1 && d4_gate->at(0).name() == "worpgate_01",
           "D4 gate uses the warp-gate particle");
#endif
    return openkal::test::g_failures == 0 ? 0 : 1;
}
