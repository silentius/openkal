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

namespace openkal::server::account_discipline {

    /** Reads account discipline by its authenticated ID. */
    inline constexpr char kLoad[] =
        "SELECT protocol_points, protocol_kicks, banned FROM accounts WHERE id = ? AND id > 0";

    /** Consumes ten points on a kick and sets the ban on the third kick. */
    inline constexpr char kViolation[] = R"SQL(
UPDATE accounts
SET protocol_points = CASE WHEN protocol_points = 9 THEN 0 ELSE protocol_points + 1 END,
    protocol_kicks = CASE WHEN protocol_points = 9 THEN protocol_kicks + 1 ELSE protocol_kicks END,
    banned = CASE WHEN protocol_points = 9 AND protocol_kicks = 2 THEN 1 ELSE 0 END
WHERE id = ? AND id > 0 AND banned = 0
RETURNING protocol_points, protocol_kicks, banned
)SQL";

    /** Removes a single point down to zero. */
    inline constexpr char kDecay[] = R"SQL(
UPDATE accounts
SET protocol_points = CASE WHEN protocol_points > 0 THEN protocol_points - 1 ELSE 0 END
WHERE id = ? AND id > 0 AND banned = 0
RETURNING protocol_points, protocol_kicks, banned
)SQL";

}  // namespace openkal::server::account_discipline
