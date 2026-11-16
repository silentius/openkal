/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ItemStacking.h"

#include "openkal/server/store/DataStore.h"

#include <limits>
#include <tuple>

namespace openkal::server {
    namespace {
        auto properties(const ItemRecord& item) {
            return std::tie(item.character_id,
                            item.account_id,
                            item.index,
                            item.prefix,
                            item.cur_end,
                            item.max_end,
                            item.x_attack,
                            item.x_magic,
                            item.x_hit,
                            item.x_defense,
                            item.x_dodge,
                            item.upgrade_level,
                            item.upgrade_rate,
                            item.mix_type,
                            item.bof,
                            item.qigong_perforated,
                            item.qigong_effect,
                            item.dragon_effect,
                            item.random_stat_type,
                            item.random_stat_value,
                            item.transform_level,
                            item.transform_exp,
                            item.pet_expire_at,
                            item.ride_expire_at,
                            item.account_bound,
                            item.character_bound,
                            item.enchant_protected);
        }
    }

    bool can_stack_items(const ItemRecord& destination, const ItemRecord& source) {
        return destination.index > 0 && destination.slot == 0 && source.slot == 0 &&
               destination.count > 0 && source.count > 0 &&
               destination.count <= std::numeric_limits<int>::max() - source.count &&
               properties(destination) == properties(source);
    }
}  // namespace openkal::server
