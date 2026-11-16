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
#include <cstdint>
namespace openkal::protocol {
    /** Reserved OpenKal entity definition for the owned aloe plant. */
    inline constexpr int kAloePlantIndex = 65000;
    /** Stackable aloe crafting ingredient. */
    inline constexpr int kAloeItemIndex = 23027;
    /** Reserved OpenKal entity definition for the owned blue lotus plant. */
    inline constexpr int kBlueLotusPlantIndex = 65001;
    /** Stackable blue lotus leaf crafting ingredient. */
    inline constexpr int kBlueLotusItemIndex = 23028;
    /** Reserved OpenKal entity definition for the owned ginseng plant. */
    inline constexpr int kGinsengPlantIndex = 65002;
    /** Stackable ginseng root crafting ingredient. */
    inline constexpr int kGinsengItemIndex = 23029;
    /** One short gathering action, independent of treasure-box timing. */
    inline constexpr float kHarvestSeconds = 2.0f;
    /** OpenKal player-state extension: one-shot gathering rather than looping box opening. */
    inline constexpr std::uint32_t kCgsHarvestPlant = 0x02000000u;
}  // namespace openkal::protocol
