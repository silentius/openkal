/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RemotePlayerMotion.h"

#include "test_util.h"

#include <cmath>

namespace {
    bool near(float a, float b) {
        return std::fabs(a - b) < 0.001f;
    }
}  // namespace

int main() {
    using openkal::client::RemotePlayerMotion;
    using openkal::test::expect;
    RemotePlayerMotion motion;
    motion.reset({0, 10, 0});
    motion.set_target({6, 13, 3}, true);
    expect(near(motion.position().x, 0), "network packet does not snap the displayed player");
    motion.update(0.025f);
    expect(near(motion.position().x, 2) && near(motion.position().y, 11) &&
               near(motion.position().z, 1),
           "interpolate the complete displayed feet pose");
    motion.set_target({6, 13, 3}, true);
    motion.update(0.05f);
    expect(near(motion.position().x, 6),
           "repeated per-frame snapshot does not restart interpolation");
    expect(motion.moving(), "reaching a MoveOn endpoint does not select idle");
    motion.update(0.2f);
    expect(motion.moving(), "packet gaps do not reset the locomotion clip");

    motion.reset({0, 0, 0});
    bool continuous = true;
    bool forward_only = true;
    bool locomotion = true;
    constexpr float dt = 1.0f / 120.0f;
    for (int frame = 0; frame < 120; ++frame) {
        if (frame % 6 == 0) {
            motion.set_target({static_cast<float>(frame / 6 + 1) * 6.0f, 0, 0}, true);
        }
        const float previous = motion.position().x;
        motion.update(dt);
        continuous = continuous && motion.position().x > previous;
        forward_only = forward_only && motion.position().x <= 120.0f;
        locomotion = locomotion && motion.moving();
    }
    expect(continuous && forward_only,
           "20 Hz movement advances on every 120 Hz render frame without overshoot");
    expect(locomotion, "continuous input never falls into idle between packets");
    motion.set_target({120, 0, 0}, false);
    expect(motion.moving(), "MoveEnd retains the walk clip while approaching the stop point");
    motion.update(0.01f);
    expect(motion.moving(), "final interpolated segment remains animated");
    motion.update(0.1f);
    expect(near(motion.position().x, 120) && !motion.moving(),
           "idle starts exactly at the final stop position");

    motion.reset({0, 0, 0});
    motion.set_target({4, 0, 3}, true);
    motion.update(0.05f);
    const float previous_yaw = motion.yaw();
    motion.set_target({7, 0, 7}, true);
    expect(near(motion.yaw(), previous_yaw),
           "integer coordinate rounding cannot snap facing on packet arrival");
    motion.update(dt);
    expect(std::fabs(motion.yaw() - previous_yaw) < 0.05f,
           "smooth diagonal heading jitter per render frame");

    motion.reset({0, 0, 0});
    motion.set_target({-1, 0, 10}, true);
    motion.update(0.1f);
    const float near_pi = motion.yaw();
    motion.set_target({0, 0, 20}, true);
    motion.update(dt);
    const float turn = std::remainder(motion.yaw() - near_pi, 6.28318530718f);
    expect(std::fabs(turn) < 0.05f, "turn across the yaw wrap uses the shortest arc");

    motion.set_target({1000, 50, 1000}, false);
    expect(near(motion.position().x, 1000) && near(motion.position().y, 50) && !motion.moving(),
           "large corrections snap instead of gliding across the map");
    motion.reset({12, 34, 56});
    motion.update(1.0f);
    expect(near(motion.position().x, 12) && near(motion.position().z, 56) && !motion.moving(),
           "recreated player has no stale interpolation or movement");
    motion.reset({12, 34, 56});
    motion.face_target(22, 56);
    expect(near(motion.yaw(), -1.57079632679f), "stationary attacker faces its target");
    motion.set_target({12, 34, 56}, false);
    motion.update(0.1f);
    expect(near(motion.yaw(), -1.57079632679f) && near(motion.position().x, 12),
           "unchanged snapshots preserve combat facing without moving the actor");
    motion.face_target(12, 56);
    expect(near(motion.yaw(), -1.57079632679f), "self-target support casts preserve facing");
    return openkal::test::g_failures ? 1 : 0;
}
