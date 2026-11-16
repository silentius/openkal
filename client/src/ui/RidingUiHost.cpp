/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RidingUiHost.h"

#include "openkal/client/ItemDb.h"

#include <algorithm>

namespace openkal::client {
    RidingUiHost::RidingUiHost(const ItemDb& catalog) : m_catalog(catalog) {}

    void RidingUiHost::set_state(const protocol::RidingState& state) {
        m_state = state;
    }

    const protocol::RidingState& RidingUiHost::state() const {
        return m_state;
    }

    void RidingUiHost::set_duplicate_item_index(int item_index) {
        m_duplicate_item_index = std::max(0, item_index);
    }

    void RidingUiHost::set_pointer(const UiHit& hit, bool pressed) {
        m_hovered_button = hit.kind() == UiHitKind::Button ? hit.widget_id() : std::string{};
        m_pointer_pressed = pressed;
    }

    bool RidingUiHost::button_hovered(std::string_view widget_id) const {
        return !m_hovered_button.empty() && m_hovered_button == widget_id;
    }

    bool RidingUiHost::button_pressed(std::string_view widget_id) const {
        return m_pointer_pressed && button_hovered(widget_id);
    }

    bool RidingUiHost::resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const {
        if (index != 0) {
            return false;
        }
        int item_index = 0;
        if (bind == "riding.mount") {
            item_index = m_state.item_index;
        } else if (bind == "stablemaster.duplicate_mount") {
            item_index = m_duplicate_item_index;
        } else {
            const int slot = riding_equipment_slot(bind);
            if (slot < 0) {
                return false;
            }
            if (m_state.kind == protocol::RidingKind::Battle && m_state.iid != 0 &&
                m_state.equipment_iids[static_cast<std::size_t>(slot)] != 0) {
                item_index = m_state.equipment_item_indices[static_cast<std::size_t>(slot)];
            }
        }
        out.set_empty(item_index == 0);
        if (item_index != 0) {
            out.set_file(m_catalog.icon_filename(item_index));
        }
        return true;
    }

    float RidingUiHost::progress_ratio(std::string_view bind) const {
        if (bind != "riding.satiety_ratio" || m_state.max_satiety == 0 ||
            m_state.kind != protocol::RidingKind::Battle) {
            return 0.0f;
        }
        return std::clamp(static_cast<float>(m_state.satiety) / m_state.max_satiety, 0.0f, 1.0f);
    }
}  // namespace openkal::client
