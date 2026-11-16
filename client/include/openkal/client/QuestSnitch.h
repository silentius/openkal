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

#include "openkal/client/NpcCatalog.h"
#include "openkal/client/PowerbookCatalog.h"
#include "openkal/client/TaskCatalog.h"

namespace openkal::client {
    /** One named destination for the current quest objective. */
    struct QuestLocation {
        std::string name;     /**< NPC or monster name. */
        PowerbookSpawn spawn; /**< NPC point or monster spawn rectangle. */
        bool npc = false;     /**< True for an NPC destination. */
    };

    /** Resolves the current objective from owned runtime catalogs.
     * @param quest Quest ID.
     * @param stage Current task flag.
     * @param have Required item count currently held.
     * @param monsters Powerbook monster catalog.
     * @param npcs NPC spawn catalog.
     * @return Named destinations; empty when no matching locations are configured. */
    [[nodiscard]] std::vector<QuestLocation>
    quest_locations(int quest,
                    const TaskFlagDef& stage,
                    int have,
                    const std::vector<PowerbookMonster>& monsters,
                    const std::vector<NpcSpawn>& npcs);
}  // namespace openkal::client
