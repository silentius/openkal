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

namespace openkal::server {
    struct ItemRecord;

    /** Checks instance properties and count capacity; callers verify catalog stackability.
     * @param destination Existing unequipped stack.
     * @param source Incoming unequipped stack belonging to the same character.
     * @return Whether all source units can be merged without losing properties or overflowing.
     */
    [[nodiscard]] bool can_stack_items(const ItemRecord& destination, const ItemRecord& source);
}  // namespace openkal::server
