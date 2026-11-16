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

namespace openkal::platform {
    /** Loads NPC definitions and resolves optional relative placement in memory.
     * Relative definitions inherit the anchor's appearance, map and facing.
     * An explicit xy triple overrides the relative position, including height.
     * Missing anchors omit the relative NPC; absolute source files remain unchanged.
     * @param dir NPC configuration directory.
     * @return Resolved NPC objects ordered by definition index.
     */
    [[nodiscard]] std::vector<Json> load_npc_definitions(const std::string& dir);
}  // namespace openkal::platform
