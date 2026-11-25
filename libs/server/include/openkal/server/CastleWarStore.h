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

#include "openkal/platform/Json.h"
#include "openkal/server/Honor.h"

#include <vector>

#include <cstdint>

namespace openkal::server {
    /** Atomic persistence for one dedicated world's Castle War state. */
    class ICastleWarStore {
      public:
        /** Destroys the persistence implementation. */
        virtual ~ICastleWarStore() = default;

        /** Loads the current state, including revision zero for a new world.
         * @param state Destination document, changed only on success.
         * @return Whether the state could be read.
         */
        [[nodiscard]] virtual bool load_castle_war(platform::Json& state) = 0;

        /** Replaces the state and advances the revision atomically.
         * @param expected_revision Current committed revision.
         * @param state State document with the next numerical revision.
         * @param awards Honor grants to commit with the round result.
         * @return Whether the document committed against the expected revision.
         */
        [[nodiscard]] virtual bool
        commit_castle_war(std::uint64_t expected_revision,
                          const platform::Json& state,
                          const std::vector<HonorGrant>& awards = {}) = 0;
    };
}  // namespace openkal::server
