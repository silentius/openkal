/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/DoggebiRitual.h"
#include "test_util.h"

#include <array>
#include <cstdint>
#include <vector>

int main() {
    using openkal::server::DoggebiRitual;
    using openkal::server::DoggebiRitualActivation;
    using openkal::server::DoggebiRitualConfig;
    using openkal::server::DoggebiRitualMonument;
    using openkal::server::DoggebiRitualPhase;
    using openkal::test::expect;

    constexpr int kMap = 3;
    constexpr std::uint64_t kNow = 1000;
    const std::array<int, 8> items = {645, 646, 647, 648, 641, 642, 643, 644};
    std::vector<DoggebiRitualMonument> monuments;
    for (int slot = 0; slot < 8; ++slot) {
        DoggebiRitualMonument monument;
        monument.npc_id = static_cast<std::uint32_t>(100 + slot);
        monument.map = kMap;
        monument.slot = slot;
        monument.required_item = items[static_cast<std::size_t>(slot)];
        monuments.push_back(monument);
    }

    expect(!DoggebiRitualConfig::create({}).has_value(), "empty ritual layout is rejected");
    auto duplicate = monuments;
    duplicate.back().slot = 0;
    expect(!DoggebiRitualConfig::create(duplicate).has_value(),
           "duplicate ritual slot is rejected");

    const auto config = DoggebiRitualConfig::create(monuments);
    expect(config.has_value(), "complete monument layout is accepted");
    if (!config.has_value()) {
        return 1;
    }

    DoggebiRitual ritual(*config);
    expect(ritual.map() == kMap, "ritual map comes from NPC data");
    expect(ritual.required_item(100) == 645, "slot zero keeps its configured item");
    expect(ritual.required_item(999) == 0, "unknown NPC has no required item");
    expect(ritual.can_activate(100), "first monument starts inactive");
    expect(ritual.activate(999, kNow) == DoggebiRitualActivation::UnknownMonument,
           "unknown NPC cannot activate the ritual");
    expect(ritual.activate(100, kNow) == DoggebiRitualActivation::Accepted,
           "first monument activates");
    expect(ritual.active_mask() == 0x01, "first monument sets bit zero");
    expect(ritual.activate(100, kNow) == DoggebiRitualActivation::AlreadyActive,
           "active monument cannot consume a second item");

    const auto before_expiry = ritual.tick(kNow + DoggebiRitual::kMonumentLifetimeMs - 1);
    expect(before_expiry.expired_mask == 0, "monument stays active before ten minutes");
    const auto at_expiry = ritual.tick(kNow + DoggebiRitual::kMonumentLifetimeMs);
    expect(at_expiry.expired_mask == 0x01 && ritual.active_mask() == 0,
           "monument expires after ten minutes");

    for (int slot = 0; slot < 8; ++slot) {
        expect(ritual.activate(static_cast<std::uint32_t>(100 + slot), kNow) ==
                   DoggebiRitualActivation::Accepted,
               "configured monument activates");
    }
    expect(ritual.phase() == DoggebiRitualPhase::QuakeOne,
           "eighth monument begins the first earthquake");
    expect(ritual.active_mask() == 0xFF, "all monuments remain active during the sequence");
    expect(ritual.remaining_to_spawn_ms(kNow) ==
               DoggebiRitual::kQuakeOneDurationMs + DoggebiRitual::kQuakeTwoDurationMs +
                   DoggebiRitual::kQuakeThreeDurationMs + DoggebiRitual::kQuakeFourDurationMs,
           "ritual countdown includes every remaining earthquake");
    expect(ritual.activate(100, kNow) == DoggebiRitualActivation::SequenceActive,
           "sequence rejects repeated monument use");

    auto advance = ritual.tick(kNow + DoggebiRitual::kQuakeOneDurationMs);
    expect(advance.phase_changed && ritual.phase() == DoggebiRitualPhase::QuakeTwo,
           "first earthquake advances after 100 seconds");
    expect(ritual.remaining_to_spawn_ms(kNow + DoggebiRitual::kQuakeOneDurationMs) ==
               DoggebiRitual::kQuakeTwoDurationMs + DoggebiRitual::kQuakeThreeDurationMs +
                   DoggebiRitual::kQuakeFourDurationMs,
           "ritual countdown decreases across phase changes");
    advance =
        ritual.tick(kNow + DoggebiRitual::kQuakeOneDurationMs + DoggebiRitual::kQuakeTwoDurationMs);
    expect(advance.phase_changed && ritual.phase() == DoggebiRitualPhase::QuakeThree,
           "second earthquake advances after 140 seconds");
    advance =
        ritual.tick(kNow + DoggebiRitual::kQuakeOneDurationMs + DoggebiRitual::kQuakeTwoDurationMs +
                    DoggebiRitual::kQuakeThreeDurationMs);
    expect(advance.phase_changed && ritual.phase() == DoggebiRitualPhase::QuakeFour,
           "third earthquake advances after 30 seconds");
    advance =
        ritual.tick(kNow + DoggebiRitual::kQuakeOneDurationMs + DoggebiRitual::kQuakeTwoDurationMs +
                    DoggebiRitual::kQuakeThreeDurationMs + DoggebiRitual::kQuakeFourDurationMs);
    expect(advance.phase_changed && advance.spawn_requested &&
               ritual.phase() == DoggebiRitualPhase::SpawnRequested,
           "fourth earthquake requests the boss spawn after 30 seconds");
    expect(!ritual.tick(kNow + 999999).spawn_requested, "spawn is requested only once");

    ritual.reset();
    expect(ritual.phase() == DoggebiRitualPhase::Idle && ritual.active_mask() == 0,
           "reset clears a completed or failed ritual");
    return 0;
}
