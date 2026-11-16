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

#include <cstdint>
#include <vector>
namespace openkal::server {
    struct CharacterRecord;
    /** Durable atomic guild operations; unavailable in local SQLite games. */
    class IGuildStore {
      public:
        virtual ~IGuildStore() = default;
        /** @param state Destination world records. @return Successful read, including an empty
         * world. */
        [[nodiscard]] virtual bool load_guilds(platform::Json& state) = 0;
        /** Commits against a locked world revision together with authoritative character changes.
         * @param previous Expected committed revision. @param state New guild records.
         * @param characters Updated authoritative characters. @return Successful atomic commit. */
        [[nodiscard]] virtual bool
        commit_guilds(std::uint64_t previous,
                      const platform::Json& state,
                      const std::vector<CharacterRecord>& characters) = 0;
    };
}  // namespace openkal::server
