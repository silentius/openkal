/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/OplLoader.h"
#include "test_util.h"

#include <cmath>
#include <string>
#include <vector>

int main() {
    using openkal::test::expect;

    const std::string root = openkal::test::temp_dir("opl");
    const std::string path = openkal::platform::join_path(root, "n_031_031.opl");
    std::vector<std::pair<std::string, std::array<float, 3>>> items = {
        {"DATA\\OBJECTS\\common\\stub.gb", {0.25f, 0.50f, 100.0f}},
        {"data/OBJECTS/b/1/land/rock.gb", {0.10f, 0.20f, 50.0f}},
    };
    expect(openkal::test::write_opl_v4(path, 31, 31, items), "write synthetic opl");
    expect(openkal::platform::file_exists(path), "opl fixture exists");

    openkal::terrain::OplFile opl;
    expect(openkal::terrain::load_opl(path, opl), "load_opl");
    expect(opl.patch_x == 31 && opl.patch_y == 31, "opl patch xy");
    expect(opl.version >= 4, "opl version>=4");
    expect(opl.instances.size() == 2, "opl instance count");

    const auto& first = opl.instances.front();
    expect(!first.name.empty(), "first name");
    expect(first.name.find("OBJECTS") != std::string::npos ||
               first.name.find("objects") != std::string::npos,
           "name mentions OBJECTS");

    float wx = 0;
    float wy = 0;
    float wz = 0;
    openkal::terrain::opl_world_translation(first, 31, 31, wx, wy, wz);
    expect(std::fabs(wy - first.tz) < 0.01f, "world Y is vTrans.z");
    expect(std::fabs(wx - (first.tx * 8192.0f - 8192.0f)) < 0.5f, "world X formula");
    expect(std::fabs(wz - (first.ty * 8192.0f - 8192.0f)) < 0.5f, "world Z formula");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
