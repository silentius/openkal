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
#include "openkal/server/store/PostgresStore.h"
#include "test_util.h"

#include <cstdlib>
#include <future>
#include <libpq-fe.h>
#include <memory>
#include <unistd.h>
using namespace openkal;
using test::expect;

namespace {
    void honor_store_test(server::PostgresStore& db, const std::string& dsn) {
        const auto week = server::honor_week_start(1'800'000'000);
        platform::Json state;
        expect(db.load_castle_war(state), "load Castle War state for Honor migration");
        state.object()["revision"].set_number(3);
        state.object()["honor"] = platform::Json::parse("[[1, 12000], [77, 923], [2, 4000]]");
        expect(db.commit_castle_war(2, state) &&
                   db.execute("UPDATE openkal_schema_version SET version=7"),
               "prepare two legacy Honor balances and one purged character");
        server::PostgresStore reopened;
        expect(reopened.open(dsn), "migrate the old Castle Honor state");
        const auto legacy = reopened.load_honor(1, week);
        expect(legacy && legacy->honor_points == 12000 && legacy->weekly_honor == 0 &&
                   legacy->reward_points == 0,
               "legacy Honor lands in lifetime points only");

        const server::HonorGrant castle{
            "castle_war", "round-4", "settlement", 1, week + 50, 8000, 0};
        const server::HonorGrant festival{
            "festival", "autumn-1", "completion", 1, week + 51, 0, 1000};
        auto inconsistent = festival;
        inconsistent.reward_points = 2000;
        state.object()["revision"].set_number(4);
        expect(reopened.grant_honor({festival}) && reopened.grant_honor({festival}) &&
                   !reopened.commit_castle_war(3, state, {castle, inconsistent}),
               "an identical retry succeeds and a conflicting replay rolls back the entire round");
        const auto failed = reopened.load_honor(1, week);
        expect(failed && failed->honor_points == 12000 && failed->reward_points == 1000,
               "rollback preserves the independent event award");
        expect(reopened.commit_castle_war(3, state, {castle}) &&
                   !reopened.commit_castle_war(3, state, {castle}),
               "the round and Honor commit together once");
        expect(reopened.execute("DELETE FROM honor_reward_wallet; "
                                "UPDATE openkal_schema_version SET version=8") &&
                   db.open(dsn) && db.close_honor_week(week),
               "migrate the previous Reward Point ledger balance into the new wallet");
        const auto earned = db.load_honor(1, week);
        expect(earned && earned->honor_points == 20000 && earned->weekly_honor == 8000 &&
                   earned->reward_points == 1000 && earned->rank == protocol::HonorRank::Specialist,
               "the first weekly rank uses Honor earned before its cutoff");

        const server::HonorGrant first{"test", "1", "win", 1, week + 52, 120000, 0};
        const server::HonorGrant second{"test", "1", "win", 2, week + 52, 135000, 0};
        const auto next = week + server::kHonorWeekSeconds;
        expect(db.grant_honor({first, second}) && db.close_honor_week(next) &&
                   db.close_honor_week(next),
               "publish the next week once after both characters pass General's threshold");
        const auto general = db.load_honor(1, next);
        const auto colonel = db.load_honor(2, next);
        expect(general && colonel && general->rank == protocol::HonorRank::General &&
                   colonel->rank == protocol::HonorRank::Colonel && general->weekly_honor == 0 &&
                   general->reward_points == 1000,
               "the sole General place goes to the highest score and wallets stay independent");

        using ExchangeResult = server::HonorExchangeResult;
        server::ItemRecord coupon;
        coupon.iid = 1001;
        coupon.index = 23032;
        coupon.character_id = 1;
        coupon.count = 2;
        expect(db.insert_item(coupon), "seed a PostgreSQL coupon stack");
        auto remainder = coupon;
        --remainder.count;
        const server::HonorRewardExchange redeem{1, next, 500};
        expect(
            db.exchange_honor_rewards(redeem, {coupon}, {remainder}) == ExchangeResult::Committed &&
                db.exchange_honor_rewards(redeem, {coupon}, {remainder}) == ExchangeResult::Failed,
            "coupon credit and stack removal are atomic even with a stale inventory update");
        auto reward = remainder;
        reward.iid = 1002;
        reward.index = 9002;
        const server::HonorRewardExchange buy{1, next, -900, protocol::HonorRank::General};
        expect(reopened.open(dsn), "open a second connection to the migrated wallet");
        auto wrong_week = buy;
        wrong_week.week_start = week;
        expect(db.exchange_honor_rewards(wrong_week, {}, {reward}) == ExchangeResult::RankTooLow,
               "a lower or stale rank cannot purchase a General offer");
        auto alternate = reward;
        alternate.iid = 1003;
        auto first_buy = std::async(std::launch::async,
                                    [&] { return db.exchange_honor_rewards(buy, {}, {reward}); });
        auto second_buy = std::async(std::launch::async, [&] {
            return reopened.exchange_honor_rewards(buy, {}, {alternate});
        });
        const auto a = first_buy.get();
        const auto b = second_buy.get();
        expect((a == ExchangeResult::Committed && b == ExchangeResult::InsufficientPoints) ||
                   (b == ExchangeResult::Committed && a == ExchangeResult::InsufficientPoints),
               "parallel purchases serialize against one Reward Points balance");
        const auto after_exchange = db.load_honor(1, next);
        const auto inventory = db.load_items(1);
        expect(after_exchange && after_exchange->honor_points == 140000 &&
                   after_exchange->reward_points == 600 && inventory.size() == 2,
               "one reward and one remaining coupon persist after one debit");
    }

    void castle_war_store_test(server::PostgresStore& db,
                               server::PostgresStore& second,
                               const std::string& dsn) {
        platform::Json state;
        expect(db.castle_war_store() != nullptr && db.load_castle_war(state) &&
                   state.at("revision").is_number() && state.at("revision").as_int() == 0,
               "new multiplayer worlds start with empty Castle War state");
        const auto left = platform::Json::parse(R"({"revision":1,"owner":17,"awards":{"3":7000}})");
        const auto right =
            platform::Json::parse(R"({"revision":1,"owner":21,"awards":{"4":7000}})");
        expect(!db.commit_castle_war(1, left),
               "Castle War commits require an exactly matching successor revision");
        auto a = std::async(std::launch::async, [&] { return db.commit_castle_war(0, left); });
        auto b = std::async(std::launch::async, [&] { return second.commit_castle_war(0, right); });
        const bool left_won = a.get();
        const bool right_won = b.get();
        expect(left_won != right_won, "one Castle War writer can claim the first revision");
        const int owner = left_won ? 17 : 21;
        const char* recipient = left_won ? "3" : "4";
        server::PostgresStore reopened;
        expect(reopened.open(dsn) && reopened.load_castle_war(state) &&
                   state.at("revision").as_int() == 1 && state.at("owner").as_int() == owner &&
                   state.at("awards").object().size() == 1 &&
                   state.at("awards").at(recipient).as_int() == 7000,
               "the winner and its awards persist together across reconnects");
        auto next = state;
        next.object()["revision"].set_number(2);
        next.object()["treasury"].set_number(73);
        expect(!db.commit_castle_war(0, left) && db.commit_castle_war(1, next) &&
                   !second.commit_castle_war(1, next) && reopened.load_castle_war(state) &&
                   state.at("revision").as_int() == 2 && state.at("treasury").as_int() == 73 &&
                   state.at("owner").as_int() == owner,
               "stale Castle War writes cannot replay an already committed revision");
    }
}  // namespace

int main() {
    const char* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
    if (!base) {
        return 77;
    }
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Connection admin(PQconnectdb(base), PQfinish);
    if (!admin || PQstatus(admin.get()) != CONNECTION_OK) {
        return 1;
    }
    PQsetNoticeProcessor(admin.get(), [](void*, const char*) {}, nullptr);
    const auto sql = [&](const std::string& text) {
        Result r(PQexec(admin.get(), text.c_str()), PQclear);
        return r && PQresultStatus(r.get()) == PGRES_COMMAND_OK;
    };
    const auto schema = "openkal_guild_test_" + std::to_string(getpid());
    if (!sql("CREATE SCHEMA " + schema)) {
        return 1;
    }
    const auto dsn = std::string(base) + " options='-csearch_path=" + schema + "'";
    {
        server::PostgresStore db, second;
        expect(db.open(dsn) && second.open(dsn), "create and reopen independent guild schema");
        expect(db.execute("DROP TABLE guild_applications, guild_members, guild_character_state, "
                          "guild_operations, guild_ranking, guilds, guild_meta, castle_war_state, "
                          "trading_post_offers, trading_post_deliveries; "
                          "ALTER TABLE items DROP COLUMN location; "
                          "ALTER TABLE items ALTER COLUMN account_id DROP NOT NULL; "
                          "ALTER TABLE items ADD CHECK ((character_id IS NOT NULL AND "
                          "account_id IS NULL) OR (character_id IS NULL AND account_id IS NOT "
                          "NULL)), ADD CHECK (account_id IS NULL OR slot BETWEEN 0 AND 215); "
                          "ALTER TABLE characters DROP COLUMN role; UPDATE "
                          "openkal_schema_version SET version=1"),
               "prepare the legacy item layout and an old schema without guild tables");
        const bool migrated = second.open(dsn);
        expect(migrated, "atomically migrate PostgreSQL schema 1 to the current schema");
        if (!migrated) {
            expect(sql("DROP SCHEMA " + schema + " CASCADE"), "remove failed migration fixture");
            return 1;
        }
        castle_war_store_test(db, second, dsn);
        server::AccountRecord account;
        account.id = 1;
        account.name = "OwnedGuildAccount";
        account.password = "owned";
        expect(db.upsert_account(account), "seed isolated account");
        server::CharacterRecord first;
        first.id = 1;
        first.account_id = 1;
        first.level = 30;
        first.money = 1000;
        first.name = "FounderOne";
        auto other = first;
        other.id = 2;
        other.name = "FounderTwo";
        expect(db.save_character(first) && db.save_character(other),
               "seed isolated guild founders");
        honor_store_test(db, dsn);
        server::GuildRules rules;
        std::string error;
        expect(rules.parse(platform::Json::parse(R"({"creation":{"fee_zamogeon":100}})"), error),
               "configure actual transaction fee");
        server::GuildService service(rules, db, db);
        expect(service.restore(), "load empty guild storage");
        server::GuildContext context;
        context.actor = first;
        protocol::GuildRequest create;
        create.command = protocol::GuildCommand::Create;
        create.request_id = 1;
        create.text = "FirstGuild";
        create.expected_fee = 100;
        auto outcome = service.execute(create, context, 1000);
        expect(outcome.error.empty() && db.load_character(1)->money == 900,
               "commit guild, leader and debit together");
        context.actor = other;
        create.request_id = 2;
        create.text = "firstguild";
        outcome = service.execute(create, context, 1000);
        expect(!outcome.error.empty() && db.load_character(2)->money == 1000,
               "case-insensitive unique name never charges loser");
        platform::Json state;
        expect(db.load_guilds(state), "read normalized guild records");
        const auto revision = static_cast<std::uint64_t>(state.at("revision").number());
        auto invalid = state;
        auto duplicate = state.at("guilds").array().front();
        duplicate.object()["id"].set_number(999);
        duplicate.object()["key"].string() = "collision";
        duplicate.object()["name"].string() = "Collision";
        invalid.object()["guilds"].array().push_back(duplicate);
        invalid.object()["revision"].set_number(revision + 1);
        auto charged = *db.load_character(1);
        charged.money = 123;
        expect(!db.commit_guilds(revision, invalid, {charged}),
               "membership unique constraint rejects a duplicate guild inside a transaction");
        expect(db.load_character(1)->money == 900,
               "constraint failure rolls back prior wallet update");
        platform::Json after;
        expect(db.load_guilds(after) && after.at("guilds").array().size() == 1 &&
                   static_cast<std::uint64_t>(after.at("revision").number()) == revision,
               "constraint failure leaves no partial guild or revision");
        auto next = state;
        next.object()["revision"].set_number(revision + 1);
        auto a =
            std::async(std::launch::async, [&] { return db.commit_guilds(revision, next, {}); });
        auto b = std::async(std::launch::async,
                            [&] { return second.commit_guilds(revision, next, {}); });
        expect(a.get() != b.get(),
               "two PostgreSQL writers with the same revision have exactly one winner");
        expect(!db.schedule_character_deletion(1, 1, 1001),
               "database-backed deletion guard protects the leader");
        server::GuildService restored(rules, second, second);
        expect(restored.restore() && restored.membership(1) != 0,
               "new coordinator restores persistent membership after writer race");
        expect(!restored.execute(create, context, 1001).error.empty(),
               "restored state still reserves the canonical guild name");
    }
    expect(sql("DROP SCHEMA " + schema + " CASCADE"), "remove only the isolated test schema");
    return test::g_failures ? 1 : 0;
}
