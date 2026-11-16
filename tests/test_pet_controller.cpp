/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PetController.h"
#include "test_util.h"

#include <cmath>
#include <vector>

namespace {
    [[nodiscard]] bool near(float a, float b, float tolerance = 0.01f) {
        return std::fabs(a - b) <= tolerance;
    }
}  // namespace

int main() {
    using openkal::client::PetController;
    using openkal::client::PetOwnerState;
    using openkal::client::PetPickupTarget;
    using openkal::test::expect;

    PetOwnerState owner;
    PetController pet;
    pet.reset(owner);
    expect(near(pet.x(), 0.0f), "reset aligns pet with owner on x");
    expect(near(pet.z(), 46.0f), "reset places pet behind owner on z");

    // Turning in place must not swing the pet around an owner-relative anchor.
    owner.yaw = 1.57079632679f;
    const auto turn_only = pet.update(0.1f, owner, false, {});
    expect(!turn_only.moving, "owner turning in place leaves pet idle");
    expect(near(pet.x(), 0.0f), "owner turning in place preserves pet x");
    expect(near(pet.z(), 46.0f), "owner turning in place preserves pet z");

    // A sharp reversal is handled as a turn before translation, so the actor
    // cannot skate sideways while its facing catches up.
    PetOwnerState reversed_owner = owner;
    reversed_owner.x = 0.0f;
    reversed_owner.z = 150.0f;
    const auto sharp_turn = pet.update(0.1f, reversed_owner, false, {});
    expect(!sharp_turn.moving, "sharp reversal turns pet before moving");
    expect(near(pet.x(), 0.0f), "sharp reversal preserves pet x while turning");
    expect(near(pet.z(), 46.0f), "sharp reversal preserves pet z while turning");

    owner = {};
    pet.reset(owner);

    // Opening a gap makes the pet chase the owner's world position under its
    // own speed rather than copying the owner's displacement.
    owner.x = 100.0f;
    const float before_x = pet.x();
    const float before_z = pet.z();
    const auto following = pet.update(0.1f, owner, false, {});
    expect(following.moving, "pet follows owner across a gap");
    expect(pet.x() > before_x, "following advances pet x toward owner");
    expect(pet.z() < before_z, "following advances pet z toward owner");
    expect(pet.x() < owner.x, "following uses pet speed instead of copying owner position");

    // Once the follow radius is reached, subsequent frames must remain fully
    // idle rather than reporting tiny floating-point correction steps.
    for (int i = 0; i < 50; ++i) {
        (void) pet.update(0.1f, owner, false, {});
    }
    const float stopped_x = pet.x();
    const float stopped_z = pet.z();
    for (int i = 0; i < 10; ++i) {
        const auto stopped = pet.update(0.1f, owner, false, {});
        expect(!stopped.moving, "pet remains idle within follow radius");
        expect(near(pet.x(), stopped_x), "idle pet preserves x");
        expect(near(pet.z(), stopped_z), "idle pet preserves z");
    }

    // An auto-pickup pet reaches the pouch before requesting the authoritative
    // pickup transaction, then observes a retry cooldown while the packet is handled.
    owner = {};
    pet.reset(owner);
    const std::vector<PetPickupTarget> drops{{42, 20.0f, 0.0f}};
    std::uint32_t reached = 0;
    for (int i = 0; i < 20 && reached == 0; ++i) {
        reached = pet.update(0.1f, owner, true, drops).pickup_item_id;
    }
    expect(reached == 42, "pet requests pickup for reached drop");
    expect(std::hypot(pet.x() - drops[0].x, pet.z() - drops[0].z) <= 5.01f,
           "pet reaches drop before requesting pickup");
    expect(pet.update(0.1f, owner, true, drops).pickup_item_id == 0,
           "pickup retry observes cooldown");
    expect(pet.pickup_target_id() == 0, "pickup request clears target");

    // The movement stop tolerance and pickup reach must agree. This distance
    // lands in the tolerance band instead of exactly on the nominal radius.
    pet.reset(owner);
    const std::vector<PetPickupTarget> tolerance_drop{{43, 0.0f, 1.8f}};
    reached = 0;
    for (int i = 0; i < 20 && reached == 0; ++i) {
        reached = pet.update(0.1f, owner, true, tolerance_drop).pickup_item_id;
    }
    expect(reached == 43, "pickup reach includes movement stop tolerance");

    // Drops outside the server pickup radius are never chased.
    pet.reset(owner);
    const std::vector<PetPickupTarget> distant_drop{{77, 65.0f, 0.0f}};
    expect(!pet.update(0.1f, owner, true, distant_drop).moving,
           "pet does not chase drops outside server pickup radius");
    expect(pet.pickup_target_id() == 0, "distant drop does not become pickup target");

    // Large owner discontinuities are teleports, so the companion is rehomed
    // instead of running across the map.
    owner.x = 1000.0f;
    owner.z = 1000.0f;
    owner.yaw = 0.0f;
    const auto teleported = pet.update(0.1f, owner, false, {});
    expect(!teleported.moving, "owner teleport rehomes pet without running");
    expect(near(pet.x(), 1000.0f), "teleport rehomes pet x near owner");
    expect(near(pet.z(), 1046.0f), "teleport rehomes pet z behind owner");

    return openkal::test::g_failures ? 1 : 0;
}
