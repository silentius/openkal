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
#include "openkal/client/UiDocument.h"
#include "openkal/protocol/Riding.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace openkal::client {
    /** Tests whether an inventory instance occupies a riding equipment slot.
     * @param equipment_iids Authoritative equipment instance IDs.
     * @param iid Inventory instance ID; zero never denotes equipment.
     * @return True only for the exact slotted instance.
     */
    [[nodiscard]] inline bool
    riding_equipment_contains_iid(const std::array<std::uint32_t, 5>& equipment_iids,
                                  std::uint32_t iid) {
        return iid != 0 &&
               std::find(equipment_iids.begin(), equipment_iids.end(), iid) != equipment_iids.end();
    }

    /** Resolves an equipment binding to its protocol slot.
     * @param bind Icon binding from the layout.
     * @return Slot index, or -1 for an unrelated binding.
     */
    [[nodiscard]] int riding_equipment_slot(std::string_view bind);

    /** Checks a drop against the layout and authoritative mount grade.
     * @param widget Target equipment slot.
     * @param state Latest server riding snapshot.
     * @param item_index Dragged item definition index.
     * @return True for compatible Battle Riding equipment.
     */
    [[nodiscard]] bool riding_equipment_accepts(const UiWidget& widget,
                                                const protocol::RidingState& state,
                                                int item_index);
}  // namespace openkal::client
