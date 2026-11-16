/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CharacterCreationDraft.h"
#include "openkal/protocol/CharacterName.h"
#include "test_util.h"

int main() {
    using openkal::test::expect;
    openkal::client::CharacterCreationDraft draft;
    expect(!draft.validation_error().empty(), "creation requires a selected class");
    expect(!draft.adjust_stat(0, 1), "class picker cannot allocate points");
    expect(!draft.select_class(4, {1}, {1}), "reject invalid class");
    expect(!draft.select_class(0, {}, {1}), "reject unavailable face assets");
    expect(draft.select_class(0, {1, 3, 6}, {1, 2}), "choose knight with available looks");
    expect(draft.stat(0) == 18 && draft.stat(1) == 16, "knight base stats");
    expect(!draft.adjust_stat(0, -1), "cannot refund base stats");
    for (int i = 0; i < 5; ++i) {
        expect(draft.adjust_stat(0, 1), "allocate a starting point");
    }
    expect(draft.stat(0) == 23 && draft.remaining_points() == 0, "all five points assigned");
    expect(!draft.adjust_stat(1, 1), "sixth point rejected");
    expect(!draft.adjust_stat(5, 1) && !draft.adjust_stat(0, 2),
           "invalid stat operations rejected");
    expect(draft.adjust_stat(0, -1) && draft.adjust_stat(4, 1), "reallocate a point");
    expect(draft.cycle_appearance(true, -1) && draft.face() == 6, "face wraps backward");
    expect(draft.cycle_appearance(true, 1) && draft.face() == 1, "face wraps forward");
    expect(draft.cycle_appearance(true, 1) && draft.face() == 3, "skip missing face variants");
    expect(draft.cycle_appearance(false, 1) && draft.hair() == 2, "hair changes independently");
    expect(!draft.validation_error().empty(), "name required even after spending points");
    draft.append_name("Knight1");
    expect(draft.validation_error().empty(), "completed draft can be submitted");
    draft.back();
    expect(draft.selected_class() == -1 && !draft.validation_error().empty(),
           "Back returns to class picker");
    expect(draft.select_class(1, {1, 2}, {1, 2}), "switch to magician");
    expect(draft.remaining_points() == 5 && draft.stat(2) == 18 && draft.stat(3) == 16,
           "class switch resets allocations to new bases");
    expect(draft.face() == 1 && draft.hair() == 1 && draft.name() == "Knight1",
           "class switch resets appearance but retains name");
    expect(!draft.validation_error().empty(), "new class must allocate its points");
    expect(draft.select_class(2, {1}, {1}) && draft.stat(4) == 18, "archer dexterity base");
    expect(draft.select_class(3, {1}, {1}) && draft.stat(0) == 14 && draft.stat(4) == 18,
           "four-class thief base stats");
    draft = openkal::client::CharacterCreationDraft{};
    draft.append_name("A _-!@#$%ä\t");
    expect(draft.name() == "A", "invalid name characters ignored");
    draft.append_name("bcdefghijklmnopqrst");
    expect(draft.name().size() == 14, "name length limited to 14");
    draft.erase_name();
    expect(draft.name().size() == 13, "backspace removes last character");
    expect(openkal::protocol::character_name_valid("Ab123456789012"),
           "the shared client/server policy accepts fourteen letters and digits");
    for (const auto* name :
         {"", "A", "Ab1234567890123", "Two Words", "Knight_1", "Knight-1", "Ärcher"}) {
        expect(!openkal::protocol::character_name_valid(name),
               "the server policy rejects invalid player names without input filtering");
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
