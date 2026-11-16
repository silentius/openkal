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

#include "openkal/protocol/Cooking.h"

namespace openkal::server {

    inline constexpr int kDemonBloodItemIndex = 443;
    inline constexpr int kSeaBreamItemIndex = 444;
    inline constexpr int kBambooKegItemIndex = 445;
    inline constexpr int kBloodFishItemIndex = 446;
    inline constexpr int kSpeedMedicineItemIndex = 447;
    inline constexpr int kSpeedMedicineDialogId = 192;
    inline constexpr int kSpeedMedicineMissingMaterialsDialogId = 193;
    inline constexpr int kSpeedMedicineDurationMs = 10000;

    /** Material, fee, and output totals for one chemist order. */
    struct SpeedMedicineCost {
        int demon_blood = 0;
        int sea_bream = 0;
        int bamboo_keg = 0;
        int blood_fish = 0;
        int geon = 0;
        int output_blood_fish = 0;
        int output_medicine = 0;
    };

    /** Tests whether at least one recipe was requested.
     * @param order Requested recipe quantities.
     * @return True for a non-empty order.
     */
    [[nodiscard]] bool valid_speed_medicine_order(const protocol::CookingOrder& order);

    /** Calculates the POTIONS recipe totals.
     * @param order Requested recipe quantities.
     * @return Required inputs, fee, and outputs.
     */
    [[nodiscard]] SpeedMedicineCost speed_medicine_cost(const protocol::CookingOrder& order);

}  // namespace openkal::server
