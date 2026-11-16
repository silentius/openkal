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
#include "test_util.h"

int main() {
    using openkal::test::expect;
    using namespace openkal;

    protocol::CookingOrder order;
    expect(!server::valid_speed_medicine_order(order), "empty order rejected");

    order.blood_fish = 2;
    order.medicine_from_blood_fish = 3;
    order.medicine_direct = 4;
    expect(server::valid_speed_medicine_order(order), "non-empty order accepted");
    const server::SpeedMedicineCost cost = server::speed_medicine_cost(order);
    expect(cost.demon_blood == 24, "demon blood formula");
    expect(cost.sea_bream == 6 && cost.bamboo_keg == 6, "fish and keg formula");
    expect(cost.blood_fish == 3, "Sea Bream with Blood input formula");
    expect(cost.geon == 3500, "chemist fee formula");
    expect(cost.output_blood_fish == 2 && cost.output_medicine == 7, "output formula");

    protocol::CookingOrder blood_fish_order;
    blood_fish_order.medicine_from_blood_fish = 1;
    const server::SpeedMedicineCost blood_fish_cost = server::speed_medicine_cost(blood_fish_order);
    expect(blood_fish_cost.demon_blood == 0 && blood_fish_cost.sea_bream == 0 &&
               blood_fish_cost.bamboo_keg == 0,
           "Sea Bream with Blood conversion needs no raw materials");
    expect(blood_fish_cost.blood_fish == 1 && blood_fish_cost.geon == 500,
           "Sea Bream with Blood conversion input formula");
    expect(blood_fish_cost.output_blood_fish == 0 && blood_fish_cost.output_medicine == 1,
           "Sea Bream with Blood conversion output formula");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
