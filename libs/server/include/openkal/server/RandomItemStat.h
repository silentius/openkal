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

#include "openkal/protocol/InsertItem.h"

#include <cstdint>

namespace openkal::server {

    struct ItemCombatDef;

    /** One randomly rolled, persistent equipment modifier. */
    struct RandomItemStatRoll {
        protocol::RandomItemStatType type = protocol::RandomItemStatType::None;
        std::uint8_t value = 0;
    };

    /**
     * Rolls the optional random modifier for a newly created weapon or defense item.
     * @param item Item definition used to select the weapon or armor table.
     * @return No modifier for the 90% miss, otherwise one persisted modifier.
     */
    [[nodiscard]] RandomItemStatRoll roll_random_item_stat(const ItemCombatDef& item);

}  // namespace openkal::server
