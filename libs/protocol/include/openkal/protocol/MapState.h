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

    /** Countdown currently associated with a map-wide scenario state. */
    enum class MapStateTimer : std::uint8_t {
        /** No countdown is active. */
        None = 0,

        /** The ritual is progressing towards the Lord spawn. */
        Ritual,

        /** The spawned Lord is progressing towards despawn. */
        Lord,
    };

    /** Authoritative state of one map-wide scenario. */
    struct MapStateFields {
        /** Logical map index. */
        std::uint8_t map = 0;

        /** Scenario-specific phase ordinal. */
        std::uint8_t phase = 0;

        /** Scenario-specific active-object bit mask. */
        std::uint8_t active_mask = 0;

        /** Countdown category presented to players in this map. */
        MapStateTimer timer = MapStateTimer::None;

        /** Whole seconds remaining in the active countdown. */
        std::uint32_t timer_seconds = 0;
    };

    /** Encodes and decodes S2C_MAPSTATE scenario updates. */
    class MapState final {
      public:
        MapState() = delete;

        /**
         * Frames an S2C_MAPSTATE packet.
         * @param fields Map-wide scenario state.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame(const MapStateFields& fields);

        /**
         * Parses an S2C_MAPSTATE payload.
         * @param data Payload bytes.
         * @param size Payload size in bytes.
         * @param fields Receives the decoded scenario state.
         * @return True when the payload is complete and valid.
         */
        [[nodiscard]] static bool
        parse(const std::uint8_t* data, std::size_t size, MapStateFields& fields);
    };

}  // namespace openkal::protocol
