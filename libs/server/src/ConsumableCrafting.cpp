/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ConsumableCrafting.h"

#include "openkal/protocol/Quest.h"

namespace openkal::server {
    const std::vector<ConsumableRecipe>& consumable_recipes(ConsumableFamily family) {
        static const std::vector<ConsumableRecipe> medicine{
            {1,
             "Medium Medicine",
             23001,
             1,
             {{23000, 2, "Small Medicine"}, {kAloeVeraItemIndex, 1, "Aloe Vera"}}},
            {2,
             "Large Medicine",
             23002,
             1,
             {{23001, 2, "Medium Medicine"}, {kAloeVeraItemIndex, 1, "Aloe Vera"}}},
            {3,
             "Huge Medicine",
             23003,
             1,
             {{23002, 2, "Large Medicine"}, {kAloeVeraItemIndex, 1, "Aloe Vera"}}}};
        static const std::vector<ConsumableRecipe> empty;
        static const std::vector<ConsumableRecipe> magic_potions{
            {4,
             "Medium Magic Potion",
             23005,
             1,
             {{23004, 2, "Small Magic Potion"}, {kBlueLotusLeafItemIndex, 1, "Blue Lotus Leaf"}}},
            {5,
             "Large Magic Potion",
             23006,
             1,
             {{23005, 2, "Medium Magic Potion"}, {kBlueLotusLeafItemIndex, 1, "Blue Lotus Leaf"}}},
            {6,
             "Huge Magic Potion",
             23007,
             1,
             {{23006, 2, "Large Magic Potion"}, {kBlueLotusLeafItemIndex, 1, "Blue Lotus Leaf"}}}};
        static const std::vector<ConsumableRecipe> auto_medicine{
            {7,
             "Small Auto-HP Medicine",
             23008,
             1,
             {{23000, 1, "Small Medicine"}, {kGinsengRootItemIndex, 1, "Ginseng Root"}}},
            {8,
             "Medium Auto-HP Medicine",
             23009,
             1,
             {{23001, 1, "Medium Medicine"}, {kGinsengRootItemIndex, 1, "Ginseng Root"}}},
            {9,
             "Large Auto-HP Medicine",
             23010,
             1,
             {{23002, 1, "Large Medicine"}, {kGinsengRootItemIndex, 1, "Ginseng Root"}}},
            {10,
             "Huge Auto-HP Medicine",
             23011,
             1,
             {{23003, 1, "Huge Medicine"}, {kGinsengRootItemIndex, 1, "Ginseng Root"}}}};
        switch (family) {
        case ConsumableFamily::Medicine:
            return medicine;
        case ConsumableFamily::MagicPotion:
            return magic_potions;
        case ConsumableFamily::AutoMedicine:
            return auto_medicine;
        }
        return empty;
    }

    std::string_view consumable_family_name(ConsumableFamily family) {
        switch (family) {
        case ConsumableFamily::Medicine:
            return "Medicine";
        case ConsumableFamily::MagicPotion:
            return "Magic Potions";
        case ConsumableFamily::AutoMedicine:
            return "Auto Medicine";
        }
        return "Medicine";
    }

    const ConsumableRecipe* find_consumable_recipe(std::uint32_t key) {
        for (const auto family : {ConsumableFamily::Medicine,
                                  ConsumableFamily::MagicPotion,
                                  ConsumableFamily::AutoMedicine}) {
            for (const auto& recipe : consumable_recipes(family)) {
                if (key == protocol::Quest::make_key(kConsumableCraftingQuestIndex, recipe.flag)) {
                    return &recipe;
                }
            }
        }
        return nullptr;
    }
}  // namespace openkal::server
