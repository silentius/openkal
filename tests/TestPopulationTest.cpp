/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/GuildService.h"
#include "openkal/server/TestControl.h"
#include "test_util.h"

#include <iostream>

using namespace openkal;
using J = platform::Json;
namespace {
    class MemoryGuilds final : public server::IGuildStore {
      public:
        J state = J::parse(R"({"revision":0,"next_id":1,"guilds":[]})");
        bool fail = false;
        bool load_guilds(J& value) override {
            value = state;
            return true;
        }
        bool commit_guilds(std::uint64_t revision,
                           const J& value,
                           const std::vector<server::CharacterRecord>&) override {
            if (fail || static_cast<std::uint64_t>(state.at("revision").number()) != revision) {
                return false;
            }
            state = value;
            return true;
        }
    };
    J command(const char* op, int id, int guild = 0, int rank = 5) {
        auto result = J::parse("{}");
        result.object()["op"].set_type(J::Type::String);
        result.object()["op"].string() = op;
        result.object()["id"].set_number(id);
        result.object()["guild"].set_number(guild);
        result.object()["rank"].set_number(rank);
        return result;
    }
}  // namespace
int main() {
    using test::expect;
    expect(server::test_identity("test", 1) == "test0001" &&
               server::test_identity("TestPassword", 50) == "TestPassword0050",
           "deterministic test credentials");
    server::CharacterRecord original;
    original.id = 8;
    original.account_id = 3;
    original.name = "TestPlayer0008";
    original.map_x = -12;
    original.money = 123456789;
    original.level = 50;
    const auto decoded = server::read_test_character(server::test_character_snapshot(original));
    expect(decoded && decoded->id == 8 && decoded->map_x == -12 && decoded->money == original.money,
           "complete signed character snapshot round trip");
    expect(!server::read_test_character("gg") && !server::read_test_character("0") &&
               !server::read_test_character(server::test_character_snapshot(original) + "00"),
           "reject malformed or trailing snapshot bytes");
    server::SqliteStore db;
    expect(db.open(":memory:") && db.ensure_default_account("owned", "owned"), "open owned store");
    const auto account = db.find_account("owned");
    if (!account) {
        return 1;
    }
    for (int i = 1; i <= 50; ++i) {
        auto character = original;
        character.id = i;
        character.account_id = account->id;
        character.name = server::test_identity("TestPlayer", i);
        expect(db.save_character(character), "seed test character");
    }
    MemoryGuilds storage;
    server::GuildRules rules;
    std::string error;
    expect(rules.parse(J::parse("{}"), error), "load independent default guild rules");
    server::GuildService guilds(rules, storage, db);
    expect(guilds.restore(), "restore empty guild domain");
    auto ensured = guilds.test_control(J::parse(R"({"op":"guilds","count":8})"), 1000);
    expect(ensured.at("guilds").array().size() == 8, "create eight managed guilds");
    const auto revision = guilds.revision();
    (void) guilds.test_control(J::parse(R"({"op":"guilds","count":8})"), 1000);
    expect(guilds.revision() == revision, "repeated population preparation is idempotent");
    for (int i = 1; i <= 50; ++i) {
        expect(guilds.test_control(command("membership", i, (i - 1) % 8 + 1), 1000)
                   .at("error")
                   .is_null(),
               "distribute fifty members without founding fees");
    }
    const auto checkLeaders = [&] {
        for (const auto& group : storage.state.at("guilds").array()) {
            int leaders = 0;
            for (const auto& member : group.at("members").array()) {
                if (member.at("rank").as_int() == 1) {
                    ++leaders;
                }
            }
            if (!group.at("members").array().empty() && leaders != 1) {
                return false;
            }
        }
        return true;
    };
    expect(checkLeaders(), "each populated guild has exactly one leader");
    expect(guilds.test_control(command("membership", 1, 8, 1), 1001).at("error").is_null() &&
               guilds.membership(1) == 8 && checkLeaders(),
           "move leader and elect source successor");
    expect(guilds.test_control(command("membership", 1, 1, 1), 1002).at("error").is_null() &&
               guilds.membership(1) == 1 && checkLeaders(),
           "move in the reverse direction and transfer leadership");
    expect(guilds.test_control(command("membership", 1, 1, 5), 1003).at("error").is_null() &&
               checkLeaders(),
           "demoting a leader elects another member");
    const auto before = J::stringify(storage.state, 0);
    storage.fail = true;
    expect(!guilds.test_control(command("membership", 2, 1, 2), 1004).at("error").is_null(),
           "surface guild transaction failure");
    storage.fail = false;
    expect(J::stringify(storage.state, 0) == before && guilds.membership(2) == 2,
           "failed reassignment preserves durable and in-memory membership");
    expect(!guilds.test_control(command("membership", 2, 1, 6), 1005).at("error").is_null(),
           "reject invalid role");
    expect(!guilds.test_control(command("membership", 999, 1, 5), 1005).at("error").is_null(),
           "reject missing character");
    (void) guilds.test_control(J::parse(R"({"op":"guilds","count":10})"), 1006);
    expect(guilds.membership(1) == 1 && storage.state.at("guilds").array().size() == 10,
           "growing guild count preserves manually assigned members");
    (void) guilds.test_control(command("membership", 1, 0), 1007);
    expect(guilds.membership(1) == 0 && checkLeaders(),
           "remove membership and preserve leadership invariant");
    server::GuildService restored(rules, storage, db);
    expect(restored.restore() && restored.membership(2) == 2 &&
               restored.test_control(J::parse(R"({"op":"guilds","count":10})"), 1008)
                       .at("guilds")
                       .array()
                       .size() == 10,
           "managed guild tags survive a server restart");
    const auto archivedId = restored.test_control(J::parse(R"({"op":"guilds"})"), 1009)
                                .at("guilds")
                                .array()
                                .back()
                                .at("id")
                                .as_int();
    auto shrunk = restored.test_control(
        J::parse(R"({"op":"guilds","count":3,"active_ids":[2, 3, 4]})"), 1010);
    expect(shrunk.at("guilds").array().size() == 3 && restored.membership(50) == 0,
           "shrinking archives extra guilds and removes inactive members from live guilds");
    auto expanded = restored.test_control(J::parse(R"({"op":"guilds","count":10})"), 1011);
    expect(expanded.at("guilds").array().size() == 10 &&
               expanded.at("guilds").array().back().at("id").as_int() == archivedId,
           "reactivating an archived guild preserves its identity");
    (void) restored.test_control(J::parse(R"({"op":"guilds","count":0})"), 1012);
    expect(restored.test_control(J::parse(R"({"op":"guilds"})"), 1013).at("guilds").array().empty(),
           "zero active guilds is supported without deleting archived data");
    return test::g_failures ? 1 : 0;
}
