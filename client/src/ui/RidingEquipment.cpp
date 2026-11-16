/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RidingEquipment.h"

#include <algorithm>
#include <array>

namespace openkal::client {
    int riding_equipment_slot(std::string_view bind) {
        constexpr std::array<std::string_view, 5> bindings{"riding.equipment.helmet",
                                                           "riding.equipment.armor",
                                                           "riding.equipment.saddle",
                                                           "riding.equipment.cloth",
                                                           "riding.equipment.magnet"};
        const auto found = std::find(bindings.begin(), bindings.end(), bind);
        return found == bindings.end() ? -1 : static_cast<int>(found - bindings.begin());
    }

    bool riding_equipment_accepts(const UiWidget& widget,
                                  const protocol::RidingState& state,
                                  int item_index) {
        const int slot = riding_equipment_slot(widget.bind());
        if (widget.type() != UiWidgetType::IconSlot || slot < 0 || state.iid == 0 ||
            state.item_index == 0 || state.kind != protocol::RidingKind::Battle ||
            item_index < 30200 || item_index > 30223) {
            return false;
        }
        const int offset = item_index - 30200;
        const int type = offset % 6;
        const int item_slot = type <= 1 ? 0 : type - 1;
        constexpr std::string_view prefix = "riding.equipment.";
        const std::string tag = "riding." + widget.bind().substr(prefix.size());
        return item_slot == slot && offset / 6 == static_cast<int>(state.grade) &&
               std::find(widget.accept().begin(), widget.accept().end(), tag) !=
                   widget.accept().end();
    }

}  // namespace openkal::client
