/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/SpeedMedicine.h"

namespace openkal::server {

    bool valid_speed_medicine_order(const protocol::CookingOrder& order) {
        return order.blood_fish != 0 || order.medicine_from_blood_fish != 0 ||
               order.medicine_direct != 0;
    }

    SpeedMedicineCost speed_medicine_cost(const protocol::CookingOrder& order) {
        const int blood_fish = static_cast<int>(order.blood_fish);
        const int from_blood_fish = static_cast<int>(order.medicine_from_blood_fish);
        const int direct = static_cast<int>(order.medicine_direct);

        SpeedMedicineCost cost;
        cost.demon_blood = 4 * (blood_fish + direct);
        cost.sea_bream = blood_fish + direct;
        cost.bamboo_keg = blood_fish + direct;
        cost.blood_fish = from_blood_fish;
        cost.geon = 500 * (from_blood_fish + direct);
        cost.output_blood_fish = blood_fish;
        cost.output_medicine = from_blood_fish + direct;
        return cost;
    }

}  // namespace openkal::server
