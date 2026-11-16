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

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::server {
    /** Validated owner policy; immutable during a server run. */
    class GuildRules final {
      public:
        /** @param path Physical JSON path. @param error Field-specific error. @return Loaded
         * policy. */
        [[nodiscard]] bool load(std::string_view path, std::string& error);
        /** @param document Policy JSON. @param error Field-specific error. @return Valid policy. */
        [[nodiscard]] bool parse(const platform::Json& document, std::string& error);
        /** @param group Group name. @param key Setting. @return Integral setting. */
        std::int64_t number(std::string_view group, std::string_view key) const;
        /** @param group Group name. @param key Setting. @return Boolean setting. */
        bool flag(std::string_view group, std::string_view key) const;
        /** @param count Counted members. @return Highest eligible grade. */
        int grade(std::size_t count) const;
        /** @param grade Attained grade. @param count Members. @return Maintenance penalty. */
        bool penalized(int grade, std::size_t count) const;
        /** @param rank Rank number. @param count Members. @return Allowed office count. */
        std::size_t quota(int rank, std::size_t count) const;
        /** @param name Input spelling. @return Unique canonical key, or empty for invalid names. */
        std::string name_key(std::string_view name) const;
        /** @return Effective policy suitable for authenticated clients. */
        const platform::Json& document() const {
            return m_document;
        }
        /** @return Whether guild gameplay is enabled. */
        bool enabled() const;

      private:
        platform::Json m_document;
    };
}  // namespace openkal::server
