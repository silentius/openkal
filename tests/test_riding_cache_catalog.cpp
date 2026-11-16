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
#include "openkal/server/RidingCacheCatalog.h"
#include "openkal/server/RidingProgressionCatalog.h"
#include "test_util.h"

#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main() {
    using openkal::server::RidingCacheCatalog;
    using openkal::server::RidingProgressionCatalog;
    using openkal::test::expect;

    RidingCacheCatalog catalog;
    const std::string repo = OPENKAL_REPO_ROOT;
    expect(catalog.load_file(openkal::platform::join_path(repo, "config/riding/cache.json")),
           "load repository Riding Cache configuration");
    expect(catalog.ready(), "Riding Cache catalog reports ready");
    expect(catalog.cache_item_index() == 41321 && catalog.seal_item_index() == 41322 &&
               catalog.seal_count() == 1,
           "repository cache and guaranteed seal ids are stable");
    expect(catalog.reward_for_roll(1).has_value() &&
               catalog.reward_for_roll(1)->item_index == 30003,
           "first configured reward owns the first roll");
    expect(catalog.reward_for_roll(1000).has_value() &&
               catalog.reward_for_roll(1000)->item_index == 41323,
           "final configured reward owns the last roll");
    expect(!catalog.reward_for_roll(0).has_value() && !catalog.reward_for_roll(1001).has_value(),
           "out-of-range cache rolls are rejected");

    RidingCacheCatalog decimal_chances;
    expect(
        decimal_chances.load_json_text(
            R"({"schema":"openkal.riding-cache/v1","cacheItemIndex":1,"sealItemIndex":2,"sealCount":1,"rewards":[{"item":3,"count":1,"chance":0.1},{"item":4,"count":1,"chance":99.9}]})"),
        "accept decimal percentage chances");
    expect(decimal_chances.reward_for_roll(1)->item_index == 3 &&
               decimal_chances.reward_for_roll(2)->item_index == 4,
           "convert decimal percentages to internal cache roll units");

    RidingCacheCatalog invalid;
    expect(
        !invalid.load_json_text(
            R"({"schema":"openkal.riding-cache/v1","cacheItemIndex":1,"sealItemIndex":2,"sealCount":1,"rewards":[{"item":3,"count":1,"chance":99.9}]})"),
        "reject cache chances that do not total 100 percent");
    expect(!invalid.ready(), "invalid Riding Cache catalog remains unavailable");

    RidingProgressionCatalog progression;
    expect(
        progression.load_file(openkal::platform::join_path(repo, "config/riding/progression.json")),
        "load repository Stablemaster progression configuration");
    expect(progression.ready() && progression.seal_item_index() == 41322 &&
               progression.starter_item_index() == 30003 && progression.starter_seal_cost() == 2 &&
               progression.duplicate_seal_reward() == 2,
           "Stablemaster starter exchange values are stable");
    expect(progression.grade_costs()[0] == 5 && progression.grade_costs()[1] == 10 &&
               progression.grade_costs()[2] == 20 && progression.time_writ_extension_days() == 7 &&
               progression.time_writ_maximum_days() == 30,
           "grade and Time Writ values match the accepted design");
    const auto* horse = progression.promotion_for(30003);
    expect(horse && progression.promotion_result_for_roll(*horse, 1) == 30100 &&
               progression.promotion_result_for_roll(*horse, 333) == 30100 &&
               progression.promotion_result_for_roll(*horse, 334) == 30101 &&
               progression.promotion_result_for_roll(*horse, 666) == 30101 &&
               progression.promotion_result_for_roll(*horse, 667) == 30102 &&
               progression.promotion_result_for_roll(*horse, 1000) == 30102,
           "Horse promotion uses exact 33.3, 33.3, and 33.4 percent ranges");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
