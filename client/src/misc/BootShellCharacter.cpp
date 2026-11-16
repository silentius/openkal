/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BootShell.h"

#include <algorithm>
#include <limits>

namespace openkal::client {

    protocol::PlayerInfoChar
    BootShell::char_from_record(const server::CharacterRecord& r,
                                const std::vector<server::ItemRecord>& items) {
        constexpr std::size_t max_wear = std::numeric_limits<std::uint8_t>::max();
        protocol::PlayerInfoChar c;
        c.pid = r.id;
        c.name = r.name;
        c.job = static_cast<std::uint8_t>(std::clamp(r.clazz, 0, 3));
        c.level = std::max(1, r.level);
        c.gid = 0;
        c.str = r.str > 0 ? r.str : 10;
        c.hth = r.hth > 0 ? r.hth : 10;
        c.intel = r.intl > 0 ? r.intl : 10;
        c.wis = r.wis > 0 ? r.wis : 10;
        c.dex = r.dex > 0 ? r.dex : 10;
        c.face = static_cast<std::uint8_t>(std::clamp(r.face, 1, 255));
        c.hair = static_cast<std::uint8_t>(std::clamp(r.hair, 1, 255));
        c.wear.reserve(std::min(items.size(), max_wear));
        for (const auto& item : items) {
            if (item.slot <= 0 || item.index <= 0 || item.count <= 0) {
                continue;
            }
            if (c.wear.size() >= max_wear) {
                break;
            }
            c.wear.push_back(static_cast<std::uint16_t>(item.index));
        }
        return c;
    }

}  // namespace openkal::client
