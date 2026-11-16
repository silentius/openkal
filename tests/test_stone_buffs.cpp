/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/BuffState.h"
#include "openkal/server/FishingCatalog.h"
#include "openkal/server/PayBuff.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "test_util.h"

#include <cstdint>
#include <limits>

int main() {
    using openkal::protocol::BuffType;
    using openkal::server::apply_wealth_stone_bonus;
    using openkal::server::DropOutcome;
    using openkal::server::DropRoller;
    using openkal::server::DropSource;
    using openkal::server::fishing_cycle_interval_ms;
    using openkal::server::FishingCatalog;
    using openkal::server::GroupDef;
    using openkal::server::GroupEntry;
    using openkal::server::ItemGroupDb;
    using openkal::server::ItemGroupDef;
    using openkal::server::ItemGroupEntry;
    using openkal::server::PayBuffSet;
    using openkal::test::expect;

    constexpr std::uint64_t kOtherBits = openkal::protocol::kCbsStoneExp |
                                         openkal::protocol::kCbsExpEvent |
                                         openkal::protocol::kCbsSkillResistLightning;
    expect((openkal::protocol::kCbsStoneWealth &
            (kOtherBits | openkal::protocol::kCbsStoneFishery)) == 0,
           "Wealth Stone has an independent state bit");
    expect((openkal::protocol::kCbsStoneFishery & kOtherBits) == 0,
           "Fishery Stone has an independent state bit");
    expect(static_cast<int>(BuffType::StoneWealth) == 102 &&
               static_cast<int>(BuffType::StoneFishery) == 104,
           "existing stone item buff IDs stay stable");
    expect(openkal::server::pay_buff_bstate(102) == openkal::protocol::kCbsStoneWealth &&
               openkal::server::pay_buff_bstate(104) == openkal::protocol::kCbsStoneFishery,
           "both stone types can be applied as timed item buffs");

    PayBuffSet wealth;
    std::int32_t remain = 0;
    std::uint64_t bstate = 0;
    expect(wealth.apply(102, 60, 1000, remain, bstate, 473),
           "apply existing Wealth Stone item type");
    expect(remain == 60 && bstate == openkal::protocol::kCbsStoneWealth,
           "Wealth Stone returns its own timed state");
    const auto saved = wealth.snapshot(1, 1000);
    PayBuffSet restored;
    restored.load(saved, 1001);
    expect(restored.has(openkal::protocol::kCbsStoneWealth) &&
               restored.entries().front().source_item_index == 473,
           "Wealth Stone survives persistence with its source item");
    const bool wealth_active = restored.has(openkal::protocol::kCbsStoneWealth);

    ItemGroupDb db;
    GroupDef group;
    group.index = 9001;
    GroupEntry money_entry;
    money_entry.kind = GroupEntry::Kind::Money;
    money_entry.chance = 100.0f;
    money_entry.amount = 42;
    group.entries.push_back(money_entry);
    db.upsert_group(group);
    ItemGroupDef itemgroup;
    itemgroup.index = 9002;
    ItemGroupEntry row;
    row.chance = 100.0f;
    row.group_index = group.index;
    itemgroup.entries.push_back(row);
    db.upsert_itemgroup(itemgroup);
    DropRoller roller(db);
    const auto rolled = roller.roll_itemgroup(itemgroup.index);
    expect(rolled.has_value() && rolled->kind == DropOutcome::Kind::Money && rolled->count == 42,
           "synthetic drop table rolls a money outcome");
    if (!rolled) {
        return 1;
    }
    DropOutcome money = *rolled;
    expect(apply_wealth_stone_bonus(money, DropSource::MonsterKill, true, wealth_active).count ==
               50,
           "solo monster money gains twenty percent with integer rounding");
    expect(apply_wealth_stone_bonus(money, DropSource::MonsterKill, true, false).count == 42,
           "monster money stays unchanged without the stone");
    expect(apply_wealth_stone_bonus(money, DropSource::MonsterKill, false, true).count == 42,
           "party money stays unchanged");
    expect(apply_wealth_stone_bonus(money, DropSource::Chest, true, true).count == 42,
           "chest money stays unchanged");
    DropOutcome item = money;
    item.kind = DropOutcome::Kind::Item;
    expect(apply_wealth_stone_bonus(item, DropSource::MonsterKill, true, true).count == 42,
           "item stacks stay unchanged");
    money.count = std::numeric_limits<int>::max();
    expect(apply_wealth_stone_bonus(money, DropSource::MonsterKill, true, true).count ==
               std::numeric_limits<int>::max(),
           "money bonus saturates without overflow");

    FishingCatalog fishing;
    expect(
        fishing.load_json_text(
            R"({"schema":"openkal.fishing/v1","baseIntervalSeconds":600,"acceleratedIntervalSeconds":300,"rewards":[{"itemIndex":237,"chancePerThousand":1000}]})"),
        "load project-owned fishing timing fixture");
    PayBuffSet fishery;
    expect(fishing_cycle_interval_ms(fishing, fishery) == 600000,
           "ordinary fishing uses the configured base interval");
    expect(fishery.apply(104, 60, 1000, remain, bstate, 477),
           "apply existing Fishery Stone item type");
    expect(fishing_cycle_interval_ms(fishing, fishery) == 300000,
           "Fishery Stone uses the configured accelerated interval");
    expect((fishery.tick(1060) & openkal::protocol::kCbsStoneFishery) != 0,
           "Fishery Stone expires at its deadline");
    expect(fishing_cycle_interval_ms(fishing, fishery) == 600000,
           "the next cycle returns to base timing after expiration");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
