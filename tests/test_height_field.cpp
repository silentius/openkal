/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/Constants.h"
#include "openkal/terrain/HeightField.h"
#include "test_util.h"

#include <cstdint>

static openkal::terrain::HeightPatch make_flat_patch(int px, int py, std::uint16_t h) {
    openkal::terrain::HeightPatch p;
    p.patch_x = px;
    p.patch_y = py;
    p.heights.assign(
        static_cast<std::size_t>(openkal::map::kHeightSamples) * openkal::map::kHeightSamples, h);
    return p;
}

int main() {
    using openkal::test::expect;
    openkal::terrain::HeightField hf;
    hf.patches.push_back(make_flat_patch(32, 32, 1500));
    const float y = hf.sample(16.0f, 16.0f, -1.0f);
    expect(y > 1499.0f && y < 1501.0f, "flat height sample near origin");
    const float miss = hf.sample(1.0e8f, 1.0e8f, 42.0f);
    expect(miss == 42.0f, "fallback when patch missing");
    expect(hf.covers(16.0f, 16.0f), "loaded patch covers the origin cell");
    expect(!hf.covers(1.0e8f, 1.0e8f), "far XZ has no KCM patch");
    openkal::terrain::HeightField empty;
    expect(!empty.covers(16.0f, 16.0f), "empty height field covers nothing");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
