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

#include "openkal/client/WidgetTools.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::protocol {
    struct TalentState;
}

namespace openkal::client {

    /** One active buff presented in the HUD or Character Info window. */
    struct BuffSlotEntry {
        /** compatibility behavior bitmask. */
        std::uint64_t bstate = 0;

        /** HyperText icon filename (e.g. shopbuff_004.bmp). */
        std::string icon_file;

        /** Interface label id for the effect title. */
        int sys_name_id = 0;

        /** Resolved effect title; filled by the HUD before draw/tooltip. */
        std::string effect_name;

        /** Source item index; 0 for buffs not activated by an item. */
        std::int32_t source_item_index = 0;

        /** True when remain is a charge count. */
        bool is_count = false;

        /** Remaining charges when is_count. */
        std::int32_t charges = 0;

        /** Steady-clock expire ms when timed; 0 means unused (counts) or permanent. */
        std::uint64_t expire_ms = 0;

        /** True when remain is -1 (ChkDrawSkillBuff always blinks). */
        bool always_blink = false;

        /** True when this HUD-only entry opens the mail inbox when clicked. */
        bool is_mail_notification = false;

        /** Beacon uses its own authoritative talent state, without a legacy bstate bit. */
        bool is_talent_beacon = false;
    };

    /** Synchronizes Beacon's timed buff slot from an authoritative talent snapshot.
     * @param slots Current HUD buff entries; unrelated entries are preserved.
     * @param state Server selection and remaining Beacon duration.
     * @param now_ms Current steady-clock milliseconds.
     */
    void sync_talent_beacon_buff(std::vector<BuffSlotEntry>& slots,
                                 const protocol::TalentState& state,
                                 std::uint64_t now_ms);

    /**
     * Left-screen buff icon column (compatibility behavior at y=90, 25×25 slots).
     */
    class BuffSlotBar {
      public:
        /**
         * Draws active buff icons under the status panel (no on-icon text).
         * @param ui Widget drawing helpers.
         * @param origin_x Left origin in pixels (default 0).
         * @param origin_y Top origin in pixels (default 90).
         * @param slots Active buff entries.
         * @param now_ms Steady-clock milliseconds.
         * @param scale HUD scale factor.
         */
        static void draw(WidgetTools& ui,
                         float origin_x,
                         float origin_y,
                         const std::vector<BuffSlotEntry>& slots,
                         std::uint64_t now_ms,
                         float scale = 1.0f);

        /**
         * Builds hover tooltip text (effect name + remaining time/charges).
         * @param entry Buff slot.
         * @param now_ms Steady-clock milliseconds.
         * @return Multi-line tooltip, or empty when inactive.
         */
        [[nodiscard]] static std::string tooltip_text(const BuffSlotEntry& entry,
                                                      std::uint64_t now_ms);

        /**
         * Hit-tests a screen point against drawn buff icons.
         * @param origin_x Bar origin X.
         * @param origin_y Bar origin Y.
         * @param slots Active buff entries.
         * @param now_ms Steady-clock milliseconds.
         * @param scale HUD scale.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Slot index into slots, or -1.
         */
        [[nodiscard]] static int hit_test(float origin_x,
                                          float origin_y,
                                          const std::vector<BuffSlotEntry>& slots,
                                          std::uint64_t now_ms,
                                          float scale,
                                          float mx,
                                          float my);

        /**
         * True when the icon should blink (last 10s timed / ≤10 charges).
         * @param entry Buff slot.
         * @param now_ms Steady-clock milliseconds.
         * @return True when blinking.
         */
        [[nodiscard]] static bool should_blink(const BuffSlotEntry& entry, std::uint64_t now_ms);

        /**
         * Formats remaining time for tooltips (compatibility behavior time lines).
         * @param entry Buff slot.
         * @param now_ms Steady-clock milliseconds.
         * @return Overlay remain label, or empty.
         */
        [[nodiscard]] static std::string remain_text(const BuffSlotEntry& entry,
                                                     std::uint64_t now_ms);
    };

}  // namespace openkal::client
