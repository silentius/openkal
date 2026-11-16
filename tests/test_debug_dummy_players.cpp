/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/server/Combat.h"
#include "openkal/server/SkillBuff.h"
#include "openkal/server/debug/DebugDummyPlayers.h"
#include "test_util.h"

#include <cstdint>
#include <string>
#include <vector>

int main() {
    using openkal::server::DebugDummyPlayers;
    using openkal::server::DummyPlayer;
    using openkal::server::SkillBuffId;
    using openkal::server::combat::kPcArcher;
    using openkal::server::combat::kPcKnight;
    using openkal::server::combat::kPcMage;
    using openkal::server::combat::kPcThief;
    using openkal::test::expect;

    DebugDummyPlayers dummies;
    std::uint32_t next_id = 1000;
    expect(dummies.spawn(next_id), "spawn succeeds");
    expect(dummies.spawned(), "spawned flag");
    expect(dummies.all().size() == 4, "four class dummies");
    expect(next_id == 1004, "consumed four entity ids");
    expect(dummies.spawn(next_id), "spawn is idempotent");
    expect(next_id == 1004, "idempotent spawn does not consume ids");

    const int classes[4] = {kPcKnight, kPcMage, kPcArcher, kPcThief};
    const char* names[4] = {"DummyKnight", "DummyMage", "DummyArcher", "DummyThief"};
    for (std::size_t i = 0; i < dummies.all().size(); ++i) {
        const DummyPlayer& dummy = dummies.all()[i];
        expect(dummy.player_id() == 1000 + static_cast<std::uint32_t>(i), "dummy id sequence");
        expect(dummy.clazz() == classes[i], "dummy class");
        expect(dummy.name() == names[i], "dummy name");
        expect(dummy.cur_hp() == dummy.max_hp() && dummy.max_hp() > 0, "full hp");
        expect(dummy.cur_mp() == dummy.max_mp() && dummy.max_mp() > 0, "full mp");
        expect((dummy.create_player_frame().size() > 3), "create frame");
    }

    {
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        const auto framed = dummies.all()[0].create_player_frame();
        expect(
            openkal::protocol::try_pop_frame(framed.data(), framed.size(), consumed, type, payload),
            "dummy create frame pop");
        openkal::protocol::CreatePlayerFields fields;
        expect(openkal::protocol::CreatePlayer::parse(payload.data(), payload.size(), fields),
               "dummy create parse");
        expect((fields.class_byte & 0x80) == 0, "other-pc create has no hero bit");
        expect(fields.name == "DummyKnight", "dummy create name");
    }

    DummyPlayer* knight = dummies.find(1000);
    expect(knight != nullptr, "find knight");
    knight->set_cur_hp(12);
    knight->set_cur_mp(3);
    std::int32_t remain = 0;
    std::uint64_t bstate = 0;
    expect(knight->skill_buffs().apply(SkillBuffId::Haste, 1, 26, 60, 15, 0, 1000, remain, bstate),
           "apply haste on dummy");
    expect(!knight->skill_buffs().entries().empty(), "buff present before reset");

    dummies.reset();
    expect(knight->cur_hp() == knight->max_hp(), "reset fills hp");
    expect(knight->cur_mp() == knight->max_mp(), "reset fills mp");
    expect(knight->skill_buffs().entries().empty(), "reset clears buffs");
    expect(dummies.spawned(), "reset does not despawn");

    const std::vector<std::uint32_t> ids = dummies.despawn();
    expect(ids.size() == 4, "despawn returns four ids");
    expect(!dummies.spawned() && dummies.all().empty(), "despawn empties list");
    expect(dummies.find(1000) == nullptr, "find after despawn");

    return openkal::test::g_failures ? 1 : 0;
}
