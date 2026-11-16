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

#include <cstdint>
#include <string_view>
#include <vector>

namespace openkal::server {
    /** Shared recipe families; unconfigured families have no recipes. */
    enum class ConsumableFamily { Medicine, MagicPotion, AutoMedicine };

    inline constexpr int kConsumableCraftingDialogId = 990300;
    inline constexpr int kAloeVeraItemIndex = 23027;
    inline constexpr int kBlueLotusLeafItemIndex = 23028;
    inline constexpr int kGinsengRootItemIndex = 23029;
    inline constexpr std::uint16_t kConsumableCraftingQuestIndex = 9600;

    /** One input requirement for a consumable recipe. */
    struct CraftIngredient {
        int index = 0;
        int count = 0;
        std::string_view name;
    };

    /** Shared authoritative recipe and client presentation data. */
    struct ConsumableRecipe {
        std::uint8_t flag = 0;
        std::string_view name;
        int output_index = 0;
        int output_count = 1;
        std::vector<CraftIngredient> ingredients;
    };

    /** Gets the recipes configured for a family.
     * @param family Recipe family.
     * @return Stable recipe descriptors for the selected family.
     */
    [[nodiscard]] const std::vector<ConsumableRecipe>& consumable_recipes(ConsumableFamily family);

    /** Gets the presentation title for a family.
     * @param family Recipe family.
     * @return Human-readable family name.
     */
    [[nodiscard]] std::string_view consumable_family_name(ConsumableFamily family);

    /** Looks up a craft request without accepting arbitrary item indices.
     * @param key Combined quest index and flag.
     * @return Configured recipe, or nullptr for unsupported requests.
     */
    [[nodiscard]] const ConsumableRecipe* find_consumable_recipe(std::uint32_t key);
}  // namespace openkal::server
