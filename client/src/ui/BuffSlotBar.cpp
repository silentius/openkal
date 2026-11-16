/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BuffSlotBar.h"

#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/TalentTree.h"

#include <algorithm>
#include <cstdio>

namespace openkal::client {
    namespace {

        constexpr float kSlot = 25.0f;
        constexpr float kBorder = 3.0f;
        constexpr int kColBreakA = 6;
        constexpr int kColBreakB = 13;

        [[nodiscard]] bool entry_active(const BuffSlotEntry& e, std::uint64_t now_ms) {
            if (e.icon_file.empty()) {
                return false;
            }
            if (e.is_count) {
                return e.charges > 0;
            }
            if (e.expire_ms == 0) {
                return true;
            }
            return e.expire_ms > now_ms;
        }

        void layout_for_index(int display, int& col, int& row) {
            col = 0;
            row = display;
            if (display > kColBreakB) {
                col = 2;
                row = display - (kColBreakB + 1);
            } else if (display > kColBreakA) {
                col = 1;
                row = display - (kColBreakA + 1);
            }
        }

    }  // namespace

    void sync_talent_beacon_buff(std::vector<BuffSlotEntry>& slots,
                                 const protocol::TalentState& state,
                                 std::uint64_t now_ms) {
        auto current = std::find_if(slots.begin(), slots.end(), [](const BuffSlotEntry& slot) {
            return slot.is_talent_beacon;
        });
        if (state.beacon_active_ms == 0 || !protocol::talent_bonuses(state.mask).beacon) {
            if (current != slots.end()) {
                slots.erase(current);
            }
            return;
        }
        BuffSlotEntry entry;
        entry.is_talent_beacon = true;
        entry.icon_file = "talents/beacon.bmp";
        entry.effect_name = "Beacon\nAttracts idle monsters within 20 tiles.";
        entry.expire_ms = now_ms + state.beacon_active_ms;
        if (current != slots.end()) {
            *current = std::move(entry);
        } else {
            slots.push_back(std::move(entry));
        }
    }

    bool BuffSlotBar::should_blink(const BuffSlotEntry& entry, std::uint64_t now_ms) {
        if (!entry_active(entry, now_ms)) {
            return false;
        }
        if (entry.is_count) {
            return entry.charges <= protocol::kPayBuffBlinkCharges;
        }
        if (entry.always_blink) {
            return true;
        }
        if (entry.expire_ms == 0) {
            return false;
        }
        const std::uint64_t left = entry.expire_ms - now_ms;
        // Blink during the final minute of the buff lifetime.
        return left <= static_cast<std::uint64_t>(protocol::kPayBuffRefreshWindowSec) * 1000ull;
    }

    std::string BuffSlotBar::remain_text(const BuffSlotEntry& entry, std::uint64_t now_ms) {
        if (entry.is_count) {
            if (entry.charges <= 0) {
                return {};
            }
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Remaining Time: %d", entry.charges);
            return buf;
        }
        if (entry.expire_ms <= now_ms) {
            return {};
        }
        const std::uint64_t left_ms = entry.expire_ms - now_ms;
        const int total_sec = static_cast<int>(left_ms / 1000ull);
        const int days = total_sec / 86400;
        const int hours = (total_sec % 86400) / 3600;
        const int mins = (total_sec % 3600) / 60;
        const int secs = total_sec % 60;
        char buf[96];
        if (days > 0) {
            std::snprintf(buf,
                          sizeof(buf),
                          "Remaining Time: %dDay(s) %dHour(s) %dMinute(s)",
                          days,
                          hours,
                          mins);
        } else if (hours > 0) {
            std::snprintf(buf, sizeof(buf), "Remaining Time: %dHour(s) %dMinute(s)", hours, mins);
        } else if (mins > 0) {
            std::snprintf(buf, sizeof(buf), "Remaining Time: %dMinute(s)", mins);
        } else {
            std::snprintf(buf, sizeof(buf), "Remaining Time: %dSecond(s)", secs);
        }
        return buf;
    }

    std::string BuffSlotBar::tooltip_text(const BuffSlotEntry& entry, std::uint64_t now_ms) {
        if (!entry_active(entry, now_ms)) {
            return {};
        }
        std::string tip;
        if (!entry.effect_name.empty()) {
            tip = entry.effect_name;
        }
        const std::string remain = remain_text(entry, now_ms);
        if (!remain.empty()) {
            if (!tip.empty()) {
                tip.push_back('\n');
            }
            tip += remain;
        }
        return tip;
    }

    int BuffSlotBar::hit_test(float origin_x,
                              float origin_y,
                              const std::vector<BuffSlotEntry>& slots,
                              std::uint64_t now_ms,
                              float scale,
                              float mx,
                              float my) {
        const float s = std::max(0.25f, scale);
        const float slot = kSlot * s;
        int display = 0;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const BuffSlotEntry& e = slots[i];
            if (!entry_active(e, now_ms)) {
                continue;
            }
            int col = 0;
            int row = 0;
            layout_for_index(display, col, row);
            const float x = origin_x + kBorder * s + static_cast<float>(col) * slot;
            const float y = origin_y + kBorder * s + static_cast<float>(row) * slot;
            if (mx >= x && mx < x + slot && my >= y && my < y + slot) {
                return static_cast<int>(i);
            }
            ++display;
        }
        return -1;
    }

    void BuffSlotBar::draw(WidgetTools& ui,
                           float origin_x,
                           float origin_y,
                           const std::vector<BuffSlotEntry>& slots,
                           std::uint64_t now_ms,
                           float scale) {
        const float s = std::max(0.25f, scale);
        const float slot = kSlot * s;
        int display = 0;
        for (const BuffSlotEntry& e : slots) {
            if (!entry_active(e, now_ms)) {
                continue;
            }
            int col = 0;
            int row = 0;
            layout_for_index(display, col, row);
            const float x = origin_x + kBorder * s + static_cast<float>(col) * slot;
            const float y = origin_y + kBorder * s + static_cast<float>(row) * slot;
            float alpha = 1.0f;
            if (should_blink(e, now_ms)) {
                // 0.3s toggle like compatibility behavior.
                const std::uint64_t phase = (now_ms / 300ull) % 2ull;
                alpha = phase == 0 ? 1.0f : 0.25f;
            }
            // compatibility behavior skillbuff blit uses ITEMCOLORKEY RGB(15,15,15) plus item fill
            // RGB(4,4,4).
            ui.draw_item_tex(e.icon_file, x, y, slot, slot, alpha);
            ++display;
        }
    }

}  // namespace openkal::client
