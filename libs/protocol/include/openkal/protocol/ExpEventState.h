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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Server-authoritative snapshot for the community event noticeboard. */
    struct ExpEventStateFields {
        bool enabled = false;
        std::int32_t manager_npc_index = 0;
        std::int32_t donation_item_index = 0;
        /** Unspent contributions towards the next activation or extension. */
        std::uint64_t contributed = 0;
        std::uint64_t target = 0;
        std::int32_t active_bonus_percent = 0;
        std::uint32_t remaining_seconds = 0;
        std::int32_t next_bonus_percent = 0;
    };

    /** Encodes the OpenKal EXP event snapshot extension. */
    class ExpEventState final {
      public:
        ExpEventState() = delete;

        /** Frames an authoritative event snapshot.
         * @param fields Current event state.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame(const ExpEventStateFields& fields);

        /** Parses a complete snapshot without changing the output on failure.
         * @param data Payload bytes.
         * @param size Payload byte count.
         * @param fields Receives validated event data.
         * @return True for a complete, valid snapshot.
         */
        [[nodiscard]] static bool
        parse(const std::uint8_t* data, std::size_t size, ExpEventStateFields& fields);
    };
}  // namespace openkal::protocol
