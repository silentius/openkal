/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Path.h"
#include "openkal/server/FishingCatalog.h"
#include "test_util.h"

#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main() {
    using openkal::server::FishingCatalog;
    using openkal::test::expect;

    FishingCatalog catalog;
    const std::string repo = OPENKAL_REPO_ROOT;
    expect(catalog.load_file(openkal::platform::join_path(repo, "config/fishing.json")),
           "load repository fishing configuration");
    expect(catalog.ready(), "catalog reports ready after valid configuration");
    expect(catalog.base_interval_ms() == 600000, "preserve ten-minute interval");
    expect(catalog.accelerated_interval_ms() == 300000,
           "preserve Fishing Stone five-minute interval");
    expect(catalog.item_for_roll(1) == 241, "eel lower boundary");
    expect(catalog.item_for_roll(5) == 241, "eel upper boundary");
    expect(catalog.item_for_roll(6) == 444, "rare reward lower boundary");
    expect(catalog.item_for_roll(15) == 444, "rare reward upper boundary");
    expect(catalog.item_for_roll(16) == 240, "salmon lower boundary");
    expect(catalog.item_for_roll(40) == 240, "salmon upper boundary");
    expect(catalog.item_for_roll(41) == 239, "mackerel lower boundary");
    expect(catalog.item_for_roll(360) == 239, "mackerel upper boundary");
    expect(catalog.item_for_roll(361) == 237, "common fish lower boundary");
    expect(catalog.item_for_roll(1000) == 237, "common fish upper boundary");
    expect(!catalog.item_for_roll(0).has_value(), "zero is not a valid roll");
    expect(!catalog.item_for_roll(1001).has_value(), "out-of-range roll is rejected");

    FishingCatalog invalid;
    expect(
        !invalid.load_json_text(
            R"({"schema":"openkal.fishing/v1","baseIntervalSeconds":600,"acceleratedIntervalSeconds":300,"rewards":[{"itemIndex":237,"chancePerThousand":999}]})"),
        "reject chance tables that do not total 1000");
    expect(!invalid.ready(), "invalid catalog remains unavailable");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
