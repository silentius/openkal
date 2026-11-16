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

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Number of visible hotbar slots (two rows of ten). */
    inline constexpr std::size_t kShortcutSlots = 20;

    /** Wire sentinel for an empty skill slot. */
    inline constexpr std::uint16_t kShortcutEmpty = 0xFFFF;

    /**
     * High bit packed into a hotbar binding for item definition shortcuts.
     * Skills stay as their raw index (backward compatible with saved bars).
     */
    inline constexpr int kShortcutItemFlag = 0x8000;

    /**
     * Tests whether a decoded hotbar binding is a skill index.
     * @param binding Slot value from ShortcutBar / PlayerSkills.
     * @return True when the slot holds a skill.
     */
    [[nodiscard]] inline bool shortcut_is_skill(int binding) {
        return binding >= 0 && binding < kShortcutItemFlag;
    }

    /**
     * Tests whether a decoded hotbar binding is an item definition shortcut.
     * @param binding Slot value from ShortcutBar / PlayerSkills.
     * @return True when the slot holds an item index.
     */
    [[nodiscard]] inline bool shortcut_is_item(int binding) {
        return binding >= kShortcutItemFlag;
    }

    /**
     * Packs an item definition index into a hotbar binding.
     * @param item_index item definition index (must be in 1..32766).
     * @return Packed binding, or -1 when the index cannot be stored.
     */
    [[nodiscard]] inline int shortcut_pack_item(int item_index) {
        if (item_index <= 0 || item_index >= kShortcutItemFlag) {
            return -1;
        }
        return kShortcutItemFlag | item_index;
    }

    /**
     * Unpacks the item definition index from a hotbar binding.
     * @param binding Packed item binding.
     * @return item definition index, or -1 when the binding is not an item.
     */
    [[nodiscard]] inline int shortcut_item_index(int binding) {
        if (!shortcut_is_item(binding)) {
            return -1;
        }
        return binding & (kShortcutItemFlag - 1);
    }

    /** Active skill plus twenty hotbar bindings. */
    struct ShortcutBar {
        /** Top mouse-slot skill index, or -1. */
        int active_index = -1;

        /** Hotbar bindings; -1 marks an empty slot. */
        std::array<int, kShortcutSlots> slots{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    };

    /** Encodes and decodes C2S_SHORTCUT / S2C_SHORTCUT. */
    class Shortcut final {
      public:
        Shortcut() = delete;

        /**
         * Builds C2S_SHORTCUT to persist the bar (flag 1).
         * @param bar Active skill and hotbar slots.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_save(const ShortcutBar& bar);

        /**
         * Parses C2S_SHORTCUT. Flag 0 is ignored (load request).
         * @param data Payload bytes.
         * @param size Payload size.
         * @param bar Receives the bar when flag is 1.
         * @return True when a save payload was parsed.
         */
        [[nodiscard]] static bool
        parse_client_save(const std::uint8_t* data, std::size_t size, ShortcutBar& bar);

        /**
         * Builds S2C_SHORTCUT. Flag 0 means no saved bar (client defaults).
         * @param bar Saved bar, or nullptr for flag 0.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server(const ShortcutBar* bar);

        /**
         * Parses S2C_SHORTCUT.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param present Set true when a saved bar follows.
         * @param bar Receives the bar when present.
         * @return True when the payload is well-formed.
         */
        [[nodiscard]] static bool
        parse_server(const std::uint8_t* data, std::size_t size, bool& present, ShortcutBar& bar);
    };

}  // namespace openkal::protocol
