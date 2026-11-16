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

#include "openkal/client/RidingEquipment.h"
#include "openkal/client/UiView.h"
#include "openkal/protocol/Riding.h"

#include <string>
#include <string_view>

namespace openkal::client {
    class ItemDb;

    /** Server-backed icon and satiety bindings for the riding window. */
    class RidingUiHost final : public UiViewHost {
      public:
        /** @param catalog Item catalog used to resolve icon assets. */
        explicit RidingUiHost(const ItemDb& catalog);

        /** @param state Authoritative local-player riding snapshot. */
        void set_state(const protocol::RidingState& state);

        /** @return Latest server snapshot. */
        const protocol::RidingState& state() const;

        /** Sets the unregistered inventory mount selected for duplicate conversion.
         * @param item_index Selected item definition index, or zero to clear.
         */
        void set_duplicate_item_index(int item_index);

        /** Updates the button state from the window's current pointer hit.
         * @param hit Current UI hit.
         * @param pressed True while the left mouse button is held.
         */
        void set_pointer(const UiHit& hit, bool pressed);

        /** @param widget_id Button identifier.
         * @return True while this button is hovered.
         */
        bool button_hovered(std::string_view widget_id) const override;

        /** @param widget_id Button identifier.
         * @return True while this button is pressed.
         */
        bool button_pressed(std::string_view widget_id) const override;

        /** @param bind Icon binding.
         * @param index Cell index (zero for these individual slots).
         * @param out Resolved icon.
         * @return True for a riding icon binding.
         */
        bool resolve_slot(std::string_view bind, int index, UiSlotIcon& out) const override;

        /** @param bind Progress binding.
         * @return Satiety fraction, or zero for an unknown binding.
         */
        float progress_ratio(std::string_view bind) const override;

      private:
        const ItemDb& m_catalog;
        protocol::RidingState m_state;
        int m_duplicate_item_index = 0;
        std::string m_hovered_button;
        bool m_pointer_pressed = false;
    };
}  // namespace openkal::client
