/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Path.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/ServerEvent.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <limits>

int main() {
    using openkal::protocol::Quest;
    using openkal::server::ExpEventDonationResult;
    using openkal::server::ServerEventManager;
    using openkal::server::SqliteStore;
    using openkal::test::expect;

    SqliteStore store;
    expect(store.open(":memory:"), "open in-memory store");
    const auto config =
        openkal::platform::join_path(OPENKAL_REPO_ROOT, "config/server_events.json");
    ServerEventManager event;
    expect(event.load_file(config) && event.restore(store, 1000), "load and initialize EXP event");
    const auto all = Quest::make_key(9501, 1);
    const auto hundred = Quest::make_key(9502, 1);
    expect(event.donate_quest_key() == all && event.donate_hundred_quest_key() == hundred,
           "both dialog links map to donations");
    expect(event.donation_amount(hundred, 499) == 100 &&
               event.donation_amount(hundred, 100) == 100 &&
               event.donation_amount(hundred, 99) == 0 && event.donation_amount(all, 499) == 499 &&
               event.donation_amount(all, 0) == 0 && event.donation_amount(0, 500) == 0,
           "fixed donation requires exactly 100 while donate-all uses the whole bag");
    const auto initial = event.snapshot(1000);
    expect(initial.enabled && initial.contributed == 0 && initial.target == 500 &&
               initial.manager_npc_index == 5037 && initial.donation_item_index == 23016 &&
               initial.next_bonus_percent == 10 && initial.remaining_seconds == 0,
           "noticeboard exposes the configured initial goal");
    ExpEventDonationResult result;
    expect(event.contribute(store, 499, 1000, result) && !result.activated &&
               event.progress_items() == 499 && event.active_bonus_percent(1000) == 0,
           "499 coins from an empty round do not activate an event");
    expect(event.contribute(store, 1, 1001, result) && result.activated && !result.extended &&
               event.active_bonus_percent(1001) == 10 && event.remaining_seconds(1001) == 3600 &&
               event.progress_items() == 0,
           "first 500 coins give ten percent for sixty minutes");
    for (int i = 0; i < 4; ++i) {
        expect(event.contribute(store, 100, 1100, result) && !result.activated && !result.extended,
               "100-coin donations accumulate while the event is active");
    }
    expect(event.progress_items() == 400 && event.remaining_seconds(1100) == 3501 &&
               event.snapshot(1100).next_bonus_percent == 20,
           "partial active donations preserve the timer and advertise the next bonus");
    expect(event.contribute(store, 100, 1100, result) && result.extended && !result.activated &&
               event.active_bonus_percent(1100) == 20 && event.remaining_seconds(1100) == 4401 &&
               result.active_until == 5501 && event.progress_items() == 0,
           "second 500 add ten percentage points and fifteen minutes to the existing expiry");
    expect(event.contribute(store, 1500, 1200, result) && result.extended &&
               event.active_bonus_percent(1200) == 50 && result.active_until == 8201,
           "bulk donations apply all three remaining bonus steps");
    expect(event.contribute(store, 1000, 1300, result) && result.extended &&
               event.active_bonus_percent(1300) == 50 && result.active_until == 10001 &&
               event.snapshot(1300).next_bonus_percent == 50,
           "donations at fifty percent continue adding fifteen minutes per 500");

    ServerEventManager restored;
    expect(restored.load_file(config) && restored.restore(store, 1400) &&
               restored.active_bonus_percent(1400) == 50 &&
               restored.remaining_seconds(1400) == 8601 && restored.progress_items() == 0,
           "restart preserves increased bonus and extended expiry");
    expect(restored.advance(store, 10001) && restored.active_bonus_percent(10001) == 0 &&
               restored.snapshot(10001).next_bonus_percent == 10,
           "expired event resets its next bonus to ten percent");
    expect(restored.contribute(store, 3000, 20000, result) && result.activated &&
               restored.active_bonus_percent(20000) == 50 &&
               restored.remaining_seconds(20000) == 8100 && restored.progress_items() == 0,
           "3000 coins start sixty minutes plus five fifteen-minute extensions, capped at fifty");
    expect(store.save_server_event({"exp_event", 500, 0, 0}) && restored.restore(store, 30000) &&
               restored.active_bonus_percent(30000) == 10 &&
               restored.remaining_seconds(30000) == 3600 && restored.progress_items() == 0,
           "legacy funded idle credit activates without another donation");
    expect(store.save_server_event({"exp_event", 537, 10, 40000}) &&
               restored.restore(store, 39000) && restored.active_bonus_percent(39000) == 20 &&
               restored.remaining_seconds(39000) == 1900 && restored.progress_items() == 37,
           "legacy funded active credit immediately upgrades and extends the event");
    expect(restored.advance(store, 39000) && restored.remaining_seconds(39000) == 1900 &&
               restored.progress_items() == 37,
           "repeated ticks never apply an extension twice");
    expect(store.save_server_event({"exp_event", 499, 50, 50000}) &&
               restored.restore(store, 49999) && restored.advance(store, 50000) &&
               restored.active_bonus_percent(50000) == 0 && restored.progress_items() == 499 &&
               restored.contribute(store, 1, 50000, result) && result.activated &&
               restored.active_bonus_percent(50000) == 10 &&
               restored.remaining_seconds(50000) == 3600,
           "donation at exact expiry starts a new base event using partial carry-over");
    expect(restored.contribute(store, 100, 50001, result), "prepare nonzero pending credit");
    expect(!restored.contribute(store, std::numeric_limits<std::int64_t>::max(), 50001, result) &&
               restored.progress_items() == 100,
           "overflowing donation leaves contributions unchanged");
    store.close();
    expect(!restored.contribute(store, 400, 50001, result) && !result.accepted &&
               restored.progress_items() == 100 && restored.active_bonus_percent(50001) == 10 &&
               restored.remaining_seconds(50001) == 3599,
           "failed persistence never consumes credit or extends the in-memory event");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
