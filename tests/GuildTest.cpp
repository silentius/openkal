/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/server/GuildService.h"
#include "test_util.h"

#include <iostream>
#include <limits>

using namespace openkal;
using test::expect;
using J = platform::Json;
using C = protocol::GuildCommand;
namespace {
    J json(std::string_view text) {
        return J::parse(text);
    }
    class Memory final : public server::IGuildStore {
      public:
        J state = json(
            R"({"revision":0,"next_id":1,"guilds":[],"applications":[],"characters":[],"receipts":[],"ranking":[]})");
        bool fail = false;
        std::vector<server::CharacterRecord> written;
        bool load_guilds(J& out) override {
            out = state;
            return true;
        }
        bool commit_guilds(std::uint64_t revision,
                           const J& next,
                           const std::vector<server::CharacterRecord>& records) override {
            if (fail || static_cast<std::uint64_t>(state.at("revision").number()) != revision) {
                return false;
            }
            state = next;
            written = records;
            return true;
        }
    };
    void rules_test() {
        server::GuildRules rules;
        std::string error;
        expect(rules.load(std::string(OPENKAL_REPO_ROOT) + "/config/server/guilds.json", error),
               "load deployment rules");
        expect(rules.parse(json("{}"), error),
               "load built-in defaults independently of owner policy");
        expect(rules.grade(2) == 1 && rules.quota(4, 9) == 0 && rules.quota(4, 10) == 1,
               "default thresholds and quotas");
        expect(
            rules.load(std::string(OPENKAL_REPO_ROOT) + "/tests/guild/TwoMemberRules.json", error),
            "load separate two-member policy");
        expect(rules.grade(2) == 4 && rules.grade(26) == 5,
               "highest threshold wins including ties");
        expect(rules.name_key("TeamKal") == "teamkal" && rules.name_key("Team Kal").empty() &&
                   rules.name_key("A  B").empty() &&
                   rules.name_key("ABCDEFGHIJKLMN") == "abcdefghijklmn" &&
                   rules.name_key("ABCDEFGHIJKLMNO").empty(),
               "canonical names preserve structural validation");
        expect(rules.name_key("한글") == "한글" && rules.name_key("한A").empty(),
               "Hangul scalars and mixed-set rejection");
        for (
            const auto* bad :
            {R"({"unknown":2})",
             R"({"creation":{"fee_zamogeon":-1}})",
             R"({"creation":{"minimum_members":2}})",
             R"({"rank_quotas":{"chief":{"members_per_slot":0}}})",
             R"({"grades":{"minimum_members_by_grade":[1,2,1]}})",
             R"({"names":{"maximum_latin":15}})",
             R"({"names":{"maximum_hangul":15}})",
             R"({"ranks":{"titles":["","Leader","Sub-Leader","HeadChief","Chief","Official Member"]}})",
             R"({"ranks":{"titles":["","Leader","Sub-Leader","HeadChief","Chief","ABCDEFGHIJKLMNO"]}})",
             R"({"ranks":{"permissions":[0,0,0,0,0,0]}})"}) {
            expect(!rules.parse(json(bad), error) && !error.empty(),
                   "reject invalid policy with diagnostic");
        }
        expect(
            rules.parse(
                json(
                    R"({"creation":{"require_party":true,"require_full_party":true,"minimum_members":12,"fee_zamogeon":0}})"),
                error),
            "owner can require full party and free creation");
        expect(rules.parse(json(R"({"grades":{"on_member_loss":"downgrade"}})"), error),
               "owner selects downgrade policy");
    }
    void protocol_test() {
        protocol::GuildRequest request;
        request.command = C::Announcement;
        request.request_id = 123;
        request.guild = 7;
        request.revision = 17;
        request.text = "Owned UTF-8 한글\n\nMeeting at the village.";
        const auto bytes = protocol::frame_guild_request(request);
        std::size_t used = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        expect(protocol::FrameParser::pop(bytes.data(), bytes.size(), used, type, payload) ==
                   protocol::FrameParseStatus::Complete,
               "framed guild request");
        protocol::GuildRequest parsed;
        expect(protocol::parse_guild_request(payload.data(), payload.size(), parsed) &&
                   parsed.text == request.text && parsed.request_id == 123,
               "request roundtrip");
        for (std::size_t length = 0; length < payload.size(); ++length) {
            expect(!protocol::parse_guild_request(payload.data(), length, parsed),
                   "reject every truncated request");
        }
        auto extra = payload;
        extra.push_back(0);
        expect(!protocol::parse_guild_request(extra.data(), extra.size(), parsed),
               "reject trailing request bytes");
        extra = payload;
        extra[0] = 2;
        expect(!protocol::parse_guild_request(extra.data(), extra.size(), parsed),
               "reject unsupported version");
        extra = payload;
        extra[1] = 255;
        expect(!protocol::parse_guild_request(extra.data(), extra.size(), parsed),
               "reject unknown operation");
        expect(!protocol::guild_text_valid(std::string("\xC0\xAF", 2), 10) &&
                   !protocol::guild_text_valid(std::string("\xED\xA0\x80", 3), 10) &&
                   !protocol::guild_text_valid("a\nb", 10),
               "reject overlong UTF-8, surrogates and controls");
        expect(protocol::guild_text_valid("a\n\nb", 10, true) &&
                   !protocol::guild_text_valid("a\tb", 10, true),
               "announcements allow blank lines but continue to reject other controls");
        auto invalid_name = request;
        invalid_name.command = C::Create;
        expect(protocol::frame_guild_request(invalid_name).empty(),
               "multiline support does not permit line breaks in guild names");
        expect(protocol::guild_label_valid("ABCDEFGHIJKLMN") &&
                   protocol::guild_label_valid("한글한글한글한글한글한글한글") &&
                   !protocol::guild_label_valid("ABCDEFGHIJKLMNO"),
               "guild labels count Unicode scalars and stop at fourteen");
        for (const auto* label : {"", "Team Kal", "Team\tKal", "Team\xC2\xA0Kal", "Team　Kal"}) {
            expect(!protocol::guild_label_valid(label), "guild labels reject all whitespace");
        }
        for (const auto command : {C::Create, C::Title}) {
            auto named = request;
            named.command = command;
            named.text = "ABCDEFGHIJKLMN";
            auto framed = protocol::frame_guild_request(named);
            expect(protocol::FrameParser::pop(framed.data(), framed.size(), used, type, payload) ==
                           protocol::FrameParseStatus::Complete &&
                       protocol::parse_guild_request(payload.data(), payload.size(), parsed),
                   "fourteen-character names and rank titles survive the protocol");
            payload.back() = ' ';
            expect(!protocol::parse_guild_request(payload.data(), payload.size(), parsed),
                   "direct packets cannot bypass the whitespace restriction");
            named.text += 'O';
            expect(protocol::frame_guild_request(named).empty(),
                   "the encoder rejects names and rank titles exceeding fourteen characters");
        }
        protocol::GuildResponse view;
        view.data = json(R"({"revision":12,"rows":[]})");
        const auto frame = protocol::frame_guild_response(view);
        expect(protocol::FrameParser::pop(frame.data(), frame.size(), used, type, payload) ==
                   protocol::FrameParseStatus::Complete,
               "decode response frame");
        protocol::GuildResponse decoded;
        expect(protocol::parse_guild_response(payload.data(), payload.size(), decoded) &&
                   decoded.data.at("revision").as_int() == 12,
               "response roundtrip");
    }
    void service_test() {
        server::SqliteStore characters;
        expect(characters.open(":memory:"), "owned character lookup");
        server::AccountRecord a;
        a.id = 1;
        a.name = "owned";
        a.password = "test";
        expect(characters.upsert_account(a), "seed account");
        std::vector<server::CharacterRecord> heroes;
        for (int id = 1; id <= 5; ++id) {
            server::CharacterRecord c;
            c.id = id;
            c.account_id = 1;
            c.name = "Hero" + std::to_string(id);
            c.level = 30;
            c.money = 500000;
            expect(characters.save_character(c), "seed character");
            heroes.push_back(c);
        }
        server::GuildRules rules;
        std::string error;
        expect(
            rules.parse(
                json(
                    R"({"creation":{"require_party":true,"minimum_members":2,"fee_zamogeon":100},"grades":{"minimum_members_by_grade":[1,2,2,2,26,40,54,70,90,110]},"rank_quotas":{"chief":{"members_per_slot":2}},"membership":{"rejoin_lock_seconds":60},"daily_reward":{"minimum_membership_seconds":0},"buffs":{"primary_per_grade":2,"hp_per_grade":10,"defense_per_grade":3}})"),
                error),
            "test policy");
        Memory storage;
        server::GuildService service(rules, storage, characters);
        expect(service.restore(), "restore empty state");
        std::uint64_t token = 0;
        const auto context = [&](int id) {
            server::GuildContext c;
            c.actor = heroes[id - 1];
            c.party = {heroes[0], heroes[1]};
            c.party_leader = 1;
            return c;
        };
        const auto perform = [&](C command,
                                 int actor,
                                 std::uint32_t target = 0,
                                 std::uint32_t value = 0,
                                 std::string text = "",
                                 std::uint32_t guild = 0,
                                 std::int64_t at = 1000) {
            protocol::GuildRequest r;
            r.command = command;
            r.request_id = ++token;
            r.revision = service.revision();
            r.guild = guild;
            r.target = target;
            r.value = value;
            r.text = std::move(text);
            if (command == C::Create) {
                r.expected_fee = 100;
            }
            auto result = service.execute(r, context(actor), at);
            for (const auto& c : result.characters) {
                heroes[c.id - 1] = c;
            }
            return result;
        };
        heroes[0].money = 99;
        auto result = perform(C::Create, 1, 0, 0, "GuildOne");
        expect(result.error == "Guild creation requires 100 Zamogeon; you have 99." &&
                   result.characters.empty() && !result.changed && service.membership(1) == 0 &&
                   heroes[0].money == 99 && service.view({}, 2, 1000).at("formation").as_int() == 0,
               "insufficient funds explain required and owned money without debit or consent");
        heroes[0].money = 500000;
        for (const auto* name : {"Team Kal", "ABCDEFGHIJKLMNO"}) {
            expect(!perform(C::Create, 1, 0, 0, name).error.empty() && service.membership(1) == 0,
                   "direct creation requests enforce name length and whitespace rules");
        }
        result = perform(C::Create, 1, 0, 0, "GuildOne");
        expect(!result.error.empty() && service.membership(1) == 0,
               "multi-founder creation waits for consent without debit");
        result = perform(C::FormationReply, 2, 1, 1);
        expect(result.error.empty() && service.membership(1) == service.membership(2) &&
                   service.membership(1) != 0,
               "all founders enroll atomically");
        expect(heroes[0].money == 499900, "founder pays once");
        const auto gid = service.membership(1);
        expect(service.guild_id("gUiLdOnE") == gid && service.contains_guild(gid) &&
                   service.guild_name(gid) == "GuildOne" && service.is_leader(1) &&
                   !service.is_leader(2) && !service.is_leader(3),
               "Castle War identity reads canonical guild and current leader");
        protocol::GuildRequest ranking;
        ranking.command = C::Ranking;
        expect(service.view(ranking, heroes[4].id, 1000).at("rows").array().size() == 1 &&
                   service.view(ranking, heroes[4].id, 1000)
                           .at("rows")
                           .array()[0]
                           .at("name")
                           .as_string() == "GuildOne",
               "new guild is immediately visible in ranking to a guildless player");
        expect(service.application_error(heroes[2], gid, 1000).empty() &&
                   !service.application_error(heroes[0], gid, 1000).empty(),
               "context offers applications only to eligible outsiders");
        expect(service.display(1).at("grade").as_int() == 4 &&
                   service.display(1).at("visible").boolean(),
               "two members immediately receive grade 4 with name display");
        expect(service.bonuses(1) == std::array<int, 3>{8, 40, 12}, "configured grade buffs");
        expect(perform(C::Rank, 2, 1, 2).error.size() > 0, "ordinary member cannot escalate");
        expect(perform(C::Rank, 1, 2, 2).error.empty(), "appoint one sub-leader");
        protocol::GuildRequest roster;
        roster.command = C::Members;
        roster.page = 1;
        const auto offices = service.view(roster, 1, 1000).at("guild").at("rank_options");
        expect(
            offices.array().size() == 5 && offices.array()[1].at("count").as_int() == 1 &&
                offices.array()[1].at("limit").as_int() == 1 &&
                offices.array()[2].at("limit").as_int() == 0 &&
                offices.array()[4].at("unlimited").boolean(),
            "rank picker gets complete occupancy and policy limits independent of roster paging");
        expect(!service.view(roster, heroes[4].id, 1000).at("guild").get("rank_options"),
               "rank administration metadata is only supplied for guild members");
        expect(!perform(C::Expel, 1, 2).error.empty(), "protected sub-leader cannot be expelled");
        expect(perform(C::Transfer, 1, 2).error.empty(), "atomic leadership succession");
        expect(!service.is_leader(1) && service.is_leader(2),
               "Castle War authority follows the committed leadership transfer");
        for (const auto* title : {"Official Member", "ABCDEFGHIJKLMNO"}) {
            expect(!perform(C::Title, 2, 5, 0, title).error.empty(),
                   "direct title requests enforce fourteen characters without spaces");
        }
        expect(perform(C::Title, 2, 5, 0, "ABCDEFGHIJKLMN").error.empty(),
               "fourteen-character rank titles can be saved");
        const std::string announcement = "Guild meeting\n\nBring supplies.";
        expect(perform(C::Announcement, 2, 0, 0, announcement).error.empty(),
               "authorized officers can publish multiline announcements");
        protocol::GuildRequest announcement_view;
        announcement_view.command = C::Detail;
        announcement_view.value = 1;
        expect(
            service.view(announcement_view, 1, 1000).at("guild").at("announcement").as_string() ==
                announcement,
            "other guild members read published paragraphs unchanged");
        expect(!perform(C::Leave, 2).error.empty(), "leader with other members cannot leave");
        expect(perform(C::Apply, 3, 0, 0, "", gid).error.empty(), "guildless applicant applies");
        expect(!service.application_error(heroes[2], gid, 1000).empty(),
               "pending application removes the contextual join option");
        expect(perform(C::Accept, 2, 3).error.empty() && service.membership(3) == gid,
               "accept current application");
        expect(perform(C::Rank, 2, 3, 4).error.empty(), "configured proportional officer quota");
        storage.fail = true;
        const auto before = service.revision();
        result = perform(C::Expel, 2, 3);
        expect(!result.error.empty() && service.membership(3) == gid &&
                   service.revision() == before,
               "persistence failure rolls back expulsion and cooldown");
        storage.fail = false;
        expect(perform(C::Expel, 2, 3).error.empty() && service.membership(3) == 0,
               "expel eligible member");
        expect(!perform(C::Apply, 3, 0, 0, "", gid, 1059).error.empty(),
               "rejoin blocked before deadline");
        expect(perform(C::Apply, 3, 0, 0, "", gid, 1060).error.empty(),
               "rejoin allowed at deadline");
        protocol::GuildRequest reward;
        reward.command = C::DailyReward;
        expect(service.view({}, 2, 1100, heroes[1].money)
                   .at("own")
                   .at("daily_reward_available")
                   .boolean(),
               "an eligible member can claim the daily reward");
        expect(!service.view({}, 2, 1100, 2000000000)
                       .at("own")
                       .at("daily_reward_available")
                       .boolean() &&
                   !service.view({}, 5, 1100, 0).at("own").at("daily_reward_available").boolean(),
               "full live wallets and guildless characters cannot claim");
        for (const auto& setting : {std::pair<const char*, const char*>{"enabled", "false"},
                                    {"minimum_grade", "10"},
                                    {"minimum_membership_seconds", "86400"}}) {
            auto policy = rules.document();
            policy.object()["daily_reward"].object()[setting.first] = json(setting.second);
            server::GuildRules unavailable_rules;
            expect(unavailable_rules.parse(policy, error), "load reward eligibility policy");
            Memory preview;
            preview.state = storage.state;
            server::GuildService unavailable(unavailable_rules, preview, characters);
            expect(unavailable.restore() && !unavailable.view({}, 2, 1100, 0)
                                                 .at("own")
                                                 .at("daily_reward_available")
                                                 .boolean(),
                   "reward availability respects enabled, grade and membership duration rules");
        }
        Memory penalized_state;
        penalized_state.state = storage.state;
        penalized_state.state.object()["guilds"].array()[0].object()["grade"].set_number(10);
        server::GuildService penalized(rules, penalized_state, characters);
        expect(penalized.restore() &&
                   !penalized.view({}, 2, 1100, 0).at("own").at("daily_reward_available").boolean(),
               "a penalized guild cannot claim daily rewards");
        reward.request_id = ++token;
        reward.revision = service.revision();
        result = service.execute(reward, context(2), 1100);
        expect(result.error.empty() && result.characters.size() == 1,
               "claim daily monetary reward");
        expect(!service.view({}, 2, 1100, 0).at("own").at("daily_reward_available").boolean() &&
                   service.view({}, 2, 1100 + 86400, 0)
                       .at("own")
                       .at("daily_reward_available")
                       .boolean(),
               "claiming disables the reward until the next server day");
        const auto committed = service.revision();
        expect(service.execute(reward, context(2), 1100).error.empty() &&
                   service.revision() == committed,
               "replayed operation does not award twice");
        expect(!perform(C::DailyReward, 2).error.empty(),
               "different request cannot claim the same daily reward");
        auto member = heroes[0];
        member.level = 34;
        expect(service.level_up(member, 1200), "award owned level-up event");
        expect(service.level_up(member, 1200), "replayed level-up is idempotent");
        expect(service.advance({{1, 101, 34}, {2, 102, 30}}, 1300, 1000), "record online presence");
        expect(service.advance({{1, 101, 34}, {2, 102, 30}}, 4900, 3601000),
               "credit connected hour with injected monotonic time");
        expect(storage.state.at("guilds").array()[0].at("points").as_int() == 44,
               "level points are credited once and each connected hour contributes ten");
        server::GuildService restored(rules, storage, characters);
        expect(restored.restore() && restored.membership(1) == gid,
               "restart restores membership and grade");
        expect(perform(C::Leave, 1).error.empty() && service.display(2).at("grade").as_int() == 4 &&
                   !service.display(2).at("visible").boolean(),
               "member loss retains grade but applies penalty");
        expect(service.bonuses(2) == std::array<int, 3>{0, 0, 0}, "penalty removes grade buffs");
        expect(perform(C::Leave, 2).error.empty() && service.membership(2) == 0,
               "last member disbands");
        expect(!service.contains_guild(gid) && service.guild_name(gid).empty() &&
                   service.guild_id("GuildOne") == 0 && !service.is_leader(2),
               "Castle War identity no longer resolves a disbanded guild or its leader");
        expect(!perform(C::Apply, 2, 0, 0, "", gid).error.empty(),
               "disbanded guild is unavailable");
        // A second coordinator cannot overwrite changes committed since its read.
        expect(restored.execute(reward, context(2), 120000).error.empty(),
               "durable replay remains harmless after another coordinator changed state");
    }
}  // namespace
int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "all";
    if (which == "rules" || which == "all") {
        rules_test();
    }
    if (which == "protocol" || which == "all") {
        protocol_test();
    }
    if (which == "service" || which == "all") {
        service_test();
    }
    return test::g_failures ? 1 : 0;
}
