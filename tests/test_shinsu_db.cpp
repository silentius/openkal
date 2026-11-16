/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/client/ShinsuDb.h"
#include "test_util.h"

#include <string>

#ifndef OPENKAL_REPO_ROOT
#error OPENKAL_REPO_ROOT required
#endif

int main() {
    using openkal::client::ShinsuDb;
    using openkal::test::expect;

    ShinsuDb db;
    const std::string root = OPENKAL_REPO_ROOT;
    expect(db.load_dir(root + "/config/shinsu"), "load config/shinsu");

    const auto* young = db.find(0, 2);
    expect(young != nullptr, "young tiger info");
    if (young != nullptr) {
        expect(young->bone() == 0, "young tiger uses shinsubone key 0");
        expect(young->particles().size() >= 3, "young tiger has head + both hands");
        expect(young->particles()[0].name() == "yungsang_head", "head fireball shader");
        expect(young->particles()[0].bone() == 2, "head fireball EFFECTBONE_HEAD");
        expect(young->particles()[1].bone() == 3, "first hand EFFECTBONE_EXTRA0");
        expect(young->particles()[2].bone() == 4, "second hand EFFECTBONE_EXTRA1");
    }

    // shinsubone key 0: "Bip01 Head" is skeleton 7 (EFFECTBONE 2).
    expect(db.skeleton_bone(0, 2) == 7, "young tiger head skeleton bone");
    expect(db.skeleton_bone(0, 3) == 27, "young tiger left hand skeleton bone");
    expect(db.skeleton_bone(0, 4) == 48, "young tiger right hand skeleton bone");
    expect(db.skeleton_bone(0, 2) != db.skeleton_bone(0, 3), "head is not a hand");
    expect(db.skeleton_bone(0, 2) != db.skeleton_bone(0, 4), "head is not the other hand");

    const auto* adult = db.find(0, 3);
    expect(adult != nullptr, "adult tiger info");
    if (adult != nullptr) {
        expect(adult->bone() == 30, "adult tiger uses shinsubone key 30");
        expect(db.skeleton_bone(adult->bone(), 2) == 6, "adult tiger head skeleton bone");
    }

    expect(db.skeleton_bone(99, 2) == -1, "unknown bone table");
    expect(db.skeleton_bone(0, -1) == -1, "invalid EFFECTBONE slot");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
