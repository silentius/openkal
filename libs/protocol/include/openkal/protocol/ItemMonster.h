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

#include <algorithm>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Monster GState bit for Maguni kneel. */
    inline constexpr std::uint32_t kCgsKnee = 0x00000001u;

    /** Target cannot be attacked. */
    inline constexpr std::uint32_t kCgsInvincible = 0x00002000u;

    /** Player GState bit for treasure-box opening. */
    inline constexpr std::uint32_t kCgsOpenBox = 0x00008000u;

    /** Completeness bits 16–17: 0 none, 1 half, 2 full. */
    inline constexpr std::uint32_t kCgsBofCompletenessShift = 16;

    /** Mask for BOF completeness. */
    inline constexpr std::uint32_t kCgsBofCompletenessMask = 0x3u << 16;

    /** FX level bits 18–24 (0–127). */
    inline constexpr std::uint32_t kCgsBofFxLevelShift = 18;

    /** Mask for BOF FX level. */
    inline constexpr std::uint32_t kCgsBofFxLevelMask = 0x7Fu << 18;

    /** Combined BOF GState bits. */
    inline constexpr std::uint32_t kCgsBofMask = kCgsBofCompletenessMask | kCgsBofFxLevelMask;

    /**
     * Packs BOF set completeness and FX level into a GState dword.
     * @param gstate Existing GState bits.
     * @param completeness 0 none, 1 half, 2 full.
     * @param fx_level Snapped FX level (46–65), or 0.
     * @return GState with BOF bits replaced.
     */
    [[nodiscard]] inline std::uint32_t
    pack_bof_gstate(std::uint32_t gstate, int completeness, int fx_level) {
        gstate &= ~kCgsBofMask;
        const std::uint32_t complete = static_cast<std::uint32_t>(std::clamp(completeness, 0, 2))
                                       << kCgsBofCompletenessShift;
        const std::uint32_t level = static_cast<std::uint32_t>(std::clamp(fx_level, 0, 127))
                                    << kCgsBofFxLevelShift;
        return gstate | complete | level;
    }

    /**
     * Reads BOF completeness from GState.
     * @param gstate Packed GState.
     * @return 0 none, 1 half, 2 full.
     */
    [[nodiscard]] inline int bof_gstate_completeness(std::uint32_t gstate) {
        return static_cast<int>((gstate & kCgsBofCompletenessMask) >> kCgsBofCompletenessShift);
    }

    /**
     * Reads BOF FX level from GState.
     * @param gstate Packed GState.
     * @return FX level, or 0.
     */
    [[nodiscard]] inline int bof_gstate_fx_level(std::uint32_t gstate) {
        return static_cast<int>((gstate & kCgsBofFxLevelMask) >> kCgsBofFxLevelShift);
    }

    /**
     * Client/server open range for GETITEMMONSTER (integer squared distance).
     * SendAskMonster uses METER*2.4 (24) to the race-4 (Item) centre —
     * the same melee-like distance as a basic attack, so wooden boxes and other
     * chests only open after walking up close. Client must gate with the same
     * integer check; float world distance can sit just under range while server
     * pixels are slightly over.
     */
    inline constexpr int kItemMonsterOpenRange = 24;

    /** MESSAGE_TYPE ordinals for item-monster (treasure box) feedback. */
    enum class ItemMonsterMsg : std::uint8_t {
        Used = 86,
        Open = 87,
        Opened = 88,
        Canceled = 89,
    };

    /** Packet helpers for C2S_GETITEMMONSTER / compatibility behavior and related S2C feedback. */
    class ItemMonster final {
      public:
        ItemMonster() = delete;

        /** Builds C2S_GETITEMMONSTER "d".
         * @param monster_id Target chest entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_get(std::uint32_t monster_id);

        /** Builds C2S_CANCELITEMMONSTER "d".
         * @param monster_id Target chest entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_cancel(std::uint32_t monster_id);

        /** Parses a single-u32 C2S item-monster payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param monster_id Out entity id.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_id(const std::uint8_t* data, std::size_t size, std::uint32_t& monster_id);

        /** Builds S2C_MESSAGEV "bb" (msg + open seconds).
         * @param msg Message ordinal.
         * @param open_sec Opening duration in seconds.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_message_v(std::uint8_t msg,
                                                                       std::uint8_t open_sec);

        /** Builds S2C_MESSAGE "b".
         * @param msg Message ordinal.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_message(std::uint8_t msg);

        /** Builds S2C_GSTATE "dd".
         * @param player_id Player entity id.
         * @param gstate Full GState bitmask.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_gstate(std::uint32_t player_id,
                                                                    std::uint32_t gstate);
    };

}  // namespace openkal::protocol
