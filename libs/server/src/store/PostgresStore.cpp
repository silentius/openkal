/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/store/PostgresStore.h"

#include "AccountDisciplineSql.h"
#include "TradingPostStore.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <libpq-fe.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openkal::server {
    namespace {
        using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
        using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
        constexpr int kOk = 0, kRow = 1, kDone = 2, kNull = 3, kError = 4;
        // Json::stringify preserves every integer below this threshold.
        constexpr std::uint64_t kCastleWarRevisionLimit = 1'000'000'000'000'000ULL;

        bool run(PGconn* db, const char* sql) {
            Result result(PQexec(db, sql), PQclear);
            if (!result || PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
                log::error("PostgresStore: command failed");
                return false;
            }
            return true;
        }

        // Result and parameter storage remain alive until the statement is released.
        struct Statement {
            PGconn* db;
            std::string sql;
            std::vector<std::optional<std::string>> values;
            Result result{nullptr, PQclear};
            int row = -1;
            int& changes;
            Statement(PGconn* connection, std::string_view query, int& changed)
                : db(connection), changes(changed) {
                for (char ch : query) {
                    if (ch == '?') {
                        values.emplace_back();
                        sql += "$" + std::to_string(values.size());
                    } else {
                        sql += ch;
                    }
                }
            }
        };
        using StatementPtr = std::unique_ptr<Statement>;
        int bind_int64(const StatementPtr& s, int i, std::int64_t v) {
            if (i <= 0 || static_cast<std::size_t>(i) > s->values.size()) {
                return kError;
            }
            s->values[i - 1] = std::to_string(v);
            return kOk;
        }
        int bind_text(const StatementPtr& s, int i, const char* v, int, int) {
            if (i <= 0 || static_cast<std::size_t>(i) > s->values.size()) {
                return kError;
            }
            s->values[i - 1] = v;
            return kOk;
        }
        int bind_null(const StatementPtr& s, int i) {
            if (i <= 0 || static_cast<std::size_t>(i) > s->values.size()) {
                return kError;
            }
            s->values[i - 1].reset();
            return kOk;
        }
        int step(const StatementPtr& s) {
            if (!s->result) {
                std::vector<const char*> params;
                for (const auto& v : s->values) {
                    params.push_back(v ? v->c_str() : nullptr);
                }
                s->result.reset(PQexecParams(s->db,
                                             s->sql.c_str(),
                                             static_cast<int>(params.size()),
                                             nullptr,
                                             params.data(),
                                             nullptr,
                                             nullptr,
                                             0));
                if (!s->result) {
                    return kError;
                }
                const auto status = PQresultStatus(s->result.get());
                if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
                    const char* code = PQresultErrorField(s->result.get(), PG_DIAG_SQLSTATE);
                    log::error(std::string("PostgresStore: statement failed SQLSTATE=") +
                               (code ? code : "unknown"));
                    return kError;
                }
                s->changes = 0;
                const char* changed = PQcmdTuples(s->result.get());
                if (changed && *changed) {
                    std::from_chars(changed, changed + std::strlen(changed), s->changes);
                }
            }
            return ++s->row < PQntuples(s->result.get()) ? kRow : kDone;
        }
        std::int64_t column_int64(const StatementPtr& s, int col) {
            std::int64_t value = 0;
            const char* text = PQgetvalue(s->result.get(), s->row, col);
            std::from_chars(text, text + std::strlen(text), value);
            return value;
        }
        int column_int(const StatementPtr& s, int col) {
            return static_cast<int>(column_int64(s, col));
        }
        const unsigned char* column_text(const StatementPtr& s, int col) {
            return reinterpret_cast<const unsigned char*>(PQgetvalue(s->result.get(), s->row, col));
        }
        int column_type(const StatementPtr& s, int col) {
            return PQgetisnull(s->result.get(), s->row, col) ? kNull : kOk;
        }

        constexpr const char* kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS accounts (
  id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  password TEXT NOT NULL,
  email TEXT UNIQUE,
  email_verified BIGINT NOT NULL DEFAULT 0,
  protocol_points BIGINT NOT NULL DEFAULT 0 CHECK(protocol_points BETWEEN 0 AND 9),
  protocol_kicks BIGINT NOT NULL DEFAULT 0 CHECK(protocol_kicks BETWEEN 0 AND 3),
  banned BIGINT NOT NULL DEFAULT 0 CHECK(banned IN (0,1))
);
CREATE TABLE IF NOT EXISTS account_storage_money (
  account_id BIGINT PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,
  balance BIGINT NOT NULL CHECK(balance >= 0)
);
CREATE TABLE IF NOT EXISTS characters (
  id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
  account_id BIGINT NOT NULL,
  name TEXT NOT NULL,
  role BIGINT NOT NULL DEFAULT 0 CHECK(role BETWEEN 0 AND 3),
  clazz BIGINT NOT NULL DEFAULT 0,
  map BIGINT NOT NULL DEFAULT 0,
  x BIGINT NOT NULL DEFAULT 257285,
  y BIGINT NOT NULL DEFAULT 258744,
  z BIGINT NOT NULL DEFAULT 16100,
  revival_point BIGINT NOT NULL DEFAULT 0,
  level BIGINT NOT NULL DEFAULT 1,
  exp BIGINT NOT NULL DEFAULT 0,
  hp BIGINT NOT NULL DEFAULT 100,
  max_hp BIGINT NOT NULL DEFAULT 100,
  mp BIGINT NOT NULL DEFAULT 50,
  max_mp BIGINT NOT NULL DEFAULT 50,
  pu_point BIGINT NOT NULL DEFAULT 0,
  su_point BIGINT NOT NULL DEFAULT 0,
  contribution BIGINT NOT NULL DEFAULT 0,
  rage BIGINT NOT NULL DEFAULT 0,
  str BIGINT NOT NULL DEFAULT 0,
  hth BIGINT NOT NULL DEFAULT 0,
  intl BIGINT NOT NULL DEFAULT 0,
  wis BIGINT NOT NULL DEFAULT 0,
  dex BIGINT NOT NULL DEFAULT 0,
  money BIGINT NOT NULL DEFAULT 0,
  next_iid BIGINT NOT NULL DEFAULT 1,
  weapon_index BIGINT NOT NULL DEFAULT 0,
  armor_index BIGINT NOT NULL DEFAULT 0,
  transform_index BIGINT NOT NULL DEFAULT 0,
  transform_level BIGINT NOT NULL DEFAULT 1,
  transform_exp BIGINT NOT NULL DEFAULT 0,
  transform_put_on_ticks BIGINT NOT NULL DEFAULT 0,
  specialty BIGINT NOT NULL DEFAULT 1,
  face BIGINT NOT NULL DEFAULT 1,
  hair BIGINT NOT NULL DEFAULT 1,
  deletion_state BIGINT NOT NULL DEFAULT 0,
  delete_after BIGINT NOT NULL DEFAULT 0, talent_mask BIGINT NOT NULL DEFAULT 0, beacon_ready_at BIGINT NOT NULL DEFAULT 0,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS items (
  iid BIGINT PRIMARY KEY,
  character_id BIGINT,
  account_id BIGINT,
  item_index BIGINT NOT NULL,
  prefix BIGINT NOT NULL DEFAULT 0,
  count BIGINT NOT NULL DEFAULT 1,
  slot BIGINT NOT NULL DEFAULT 0,
  cur_end BIGINT NOT NULL DEFAULT 0,
  max_end BIGINT NOT NULL DEFAULT 0,
  x_attack BIGINT NOT NULL DEFAULT 0,
  x_magic BIGINT NOT NULL DEFAULT 0,
  x_hit BIGINT NOT NULL DEFAULT 0,
  x_defense BIGINT NOT NULL DEFAULT 0,
  x_dodge BIGINT NOT NULL DEFAULT 0,
  upgrade_level BIGINT NOT NULL DEFAULT 0,
  upgrade_rate BIGINT NOT NULL DEFAULT 0,
  mix_type BIGINT NOT NULL DEFAULT 0,
  bof BIGINT NOT NULL DEFAULT 0,
  qigong_perforated BIGINT NOT NULL DEFAULT 0,
  qigong_effect BIGINT NOT NULL DEFAULT 0,
  dragon_effect BIGINT NOT NULL DEFAULT 0,
  random_stat_type BIGINT NOT NULL DEFAULT 0,
  random_stat_value BIGINT NOT NULL DEFAULT 0,
  transform_level BIGINT NOT NULL DEFAULT 1,
  transform_exp BIGINT NOT NULL DEFAULT 0,
  pet_expire_at BIGINT NOT NULL DEFAULT 0,
  ride_expire_at BIGINT NOT NULL DEFAULT 0,
  account_bound BIGINT NOT NULL DEFAULT 0 CHECK(account_bound IN (0, 1)), enchant_protected BIGINT NOT NULL DEFAULT 0 CHECK(enchant_protected IN (0, 1)),
  CHECK ((character_id IS NOT NULL AND account_id IS NULL) OR
         (character_id IS NULL AND account_id IS NOT NULL)),
  CHECK (account_id IS NULL OR slot BETWEEN 0 AND 215),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS trading_post_lock(id BIGINT PRIMARY KEY CHECK(id=1));
INSERT INTO trading_post_lock VALUES(1) ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS trading_post_offers(id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY, seller BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>=0),unit_price BIGINT NOT NULL CHECK(unit_price>0),attributes TEXT NOT NULL,clazz BIGINT NOT NULL,type BIGINT NOT NULL,grade BIGINT NOT NULL,prefix BIGINT NOT NULL,enhancement BIGINT NOT NULL,mount TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS trading_post_search ON trading_post_offers(type,grade,unit_price,id);
CREATE INDEX IF NOT EXISTS trading_post_seller ON trading_post_offers(seller,id);
CREATE TABLE IF NOT EXISTS trading_post_deliveries(id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,character_id BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>0),unit_price BIGINT NOT NULL CHECK(unit_price>0),kind BIGINT NOT NULL,attributes TEXT NOT NULL,mount TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS trading_post_collection ON trading_post_deliveries(character_id,id);

CREATE TABLE IF NOT EXISTS skills (
  character_id BIGINT NOT NULL,
  skill_index BIGINT NOT NULL,
  level BIGINT NOT NULL DEFAULT 1,
  PRIMARY KEY (character_id, skill_index),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS shortcuts (
  character_id BIGINT PRIMARY KEY,
  active_index BIGINT NOT NULL DEFAULT -1,
  slot0 BIGINT NOT NULL DEFAULT -1,
  slot1 BIGINT NOT NULL DEFAULT -1,
  slot2 BIGINT NOT NULL DEFAULT -1,
  slot3 BIGINT NOT NULL DEFAULT -1,
  slot4 BIGINT NOT NULL DEFAULT -1,
  slot5 BIGINT NOT NULL DEFAULT -1,
  slot6 BIGINT NOT NULL DEFAULT -1,
  slot7 BIGINT NOT NULL DEFAULT -1,
  slot8 BIGINT NOT NULL DEFAULT -1,
  slot9 BIGINT NOT NULL DEFAULT -1, slot10 BIGINT NOT NULL DEFAULT -1, slot11 BIGINT NOT NULL DEFAULT -1, slot12 BIGINT NOT NULL DEFAULT -1, slot13 BIGINT NOT NULL DEFAULT -1, slot14 BIGINT NOT NULL DEFAULT -1, slot15 BIGINT NOT NULL DEFAULT -1, slot16 BIGINT NOT NULL DEFAULT -1, slot17 BIGINT NOT NULL DEFAULT -1, slot18 BIGINT NOT NULL DEFAULT -1, slot19 BIGINT NOT NULL DEFAULT -1,
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS quest_flags (
  character_id BIGINT NOT NULL,
  quest BIGINT NOT NULL,
  flag BIGINT NOT NULL DEFAULT 0,
  clear_flag BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, quest),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS buff_remain (
  character_id BIGINT NOT NULL,
  buff_type BIGINT NOT NULL,
  remain BIGINT NOT NULL DEFAULT 0,
  is_count BIGINT NOT NULL DEFAULT 0,
  stack_pack BIGINT NOT NULL DEFAULT 0,
  source_item_index BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, buff_type),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS inventory_unlock (
  character_id BIGINT NOT NULL,
  register_index BIGINT NOT NULL,
  remain BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, register_index),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS server_events (
  event_id TEXT PRIMARY KEY,
  contribution_count BIGINT NOT NULL DEFAULT 0,
  active_bonus_percent BIGINT NOT NULL DEFAULT 0,
  active_until BIGINT NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_items_iid ON items(iid);
CREATE UNIQUE INDEX IF NOT EXISTS ux_accounts_id ON accounts(id);
CREATE UNIQUE INDEX IF NOT EXISTS ux_accounts_name ON accounts(name);
CREATE UNIQUE INDEX IF NOT EXISTS ux_characters_account_name ON characters(account_id, name);
CREATE INDEX IF NOT EXISTS ix_characters_deletion_due ON characters(deletion_state, delete_after);
CREATE UNIQUE INDEX IF NOT EXISTS ux_items_account_slot ON items(account_id, slot) WHERE account_id IS NOT NULL;
CREATE TABLE IF NOT EXISTS system_mail (id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,character_id BIGINT NOT NULL,source_key TEXT NOT NULL,subject TEXT NOT NULL,body TEXT NOT NULL,item_index BIGINT NOT NULL,item_count BIGINT NOT NULL,created_at BIGINT NOT NULL,claimed_at BIGINT NOT NULL DEFAULT 0, read_at BIGINT NOT NULL DEFAULT 0, deleted_at BIGINT NOT NULL DEFAULT 0,UNIQUE(character_id, source_key),FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS ix_system_mail_character ON system_mail(character_id, claimed_at, created_at DESC);
CREATE TABLE IF NOT EXISTS battle_mounts (mount_iid BIGINT PRIMARY KEY,grade BIGINT NOT NULL DEFAULT 0 CHECK(grade BETWEEN 0 AND 3),satiety BIGINT NOT NULL DEFAULT 100,max_satiety BIGINT NOT NULL DEFAULT 100,revision BIGINT NOT NULL DEFAULT 0,ability0 BIGINT NOT NULL DEFAULT 0,ability1 BIGINT NOT NULL DEFAULT 0,ability2 BIGINT NOT NULL DEFAULT 0,ability3 BIGINT NOT NULL DEFAULT 0,ability4 BIGINT NOT NULL DEFAULT 0,ability5 BIGINT NOT NULL DEFAULT 0,equipment0 BIGINT NOT NULL DEFAULT 0,equipment1 BIGINT NOT NULL DEFAULT 0,equipment2 BIGINT NOT NULL DEFAULT 0,equipment3 BIGINT NOT NULL DEFAULT 0,equipment4 BIGINT NOT NULL DEFAULT 0,FOREIGN KEY(mount_iid) REFERENCES items(iid) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS battle_mount_collection (account_id BIGINT NOT NULL,mount_item_index BIGINT NOT NULL,PRIMARY KEY(account_id, mount_item_index),FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE);
CREATE UNIQUE INDEX IF NOT EXISTS ux_online_character_name ON characters(lower(name));
CREATE TABLE IF NOT EXISTS guild_meta (id BIGINT PRIMARY KEY CHECK(id=1), document TEXT NOT NULL);
INSERT INTO guild_meta VALUES(1, '{"revision":0,"next_id":1}') ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS guilds (id BIGINT PRIMARY KEY, name_key TEXT UNIQUE NOT NULL, document TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS guild_members (character_id BIGINT PRIMARY KEY REFERENCES characters(id), guild_id BIGINT NOT NULL REFERENCES guilds(id) ON DELETE CASCADE, rank BIGINT NOT NULL CHECK(rank BETWEEN 0 AND 5));
CREATE UNIQUE INDEX IF NOT EXISTS guild_one_leader ON guild_members(guild_id) WHERE rank=1;
CREATE UNIQUE INDEX IF NOT EXISTS guild_one_subleader ON guild_members(guild_id) WHERE rank=2;
CREATE TABLE IF NOT EXISTS guild_applications (guild_id BIGINT NOT NULL REFERENCES guilds(id) ON DELETE CASCADE, character_id BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE, document TEXT NOT NULL, PRIMARY KEY(guild_id,character_id));
CREATE TABLE IF NOT EXISTS guild_character_state (character_id BIGINT PRIMARY KEY REFERENCES characters(id) ON DELETE CASCADE, document TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS guild_operations (actor BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE, request_id TEXT NOT NULL, document TEXT NOT NULL, PRIMARY KEY(actor,request_id));
CREATE TABLE IF NOT EXISTS guild_ranking (id BIGINT PRIMARY KEY CHECK(id=1), document TEXT NOT NULL);
INSERT INTO guild_ranking VALUES(1,'[]') ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS castle_war_state (
    id BIGINT PRIMARY KEY CHECK(id=1),
    revision BIGINT NOT NULL CHECK(revision>0),
    document TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS openkal_schema_version(version BIGINT PRIMARY KEY);
INSERT INTO openkal_schema_version VALUES(1) ON CONFLICT DO NOTHING;
)SQL";
        [[nodiscard]] CharacterRecord character_from_stmt(const StatementPtr& stmt) {
            CharacterRecord c;
            c.id = column_int(stmt, 0);
            c.account_id = column_int(stmt, 1);
            const unsigned char* name = column_text(stmt, 2);
            c.name = name ? reinterpret_cast<const char*>(name) : "";
            c.role = static_cast<UserRole>(column_int(stmt, 3));
            c.clazz = column_int(stmt, 4);
            c.map = column_int(stmt, 5);
            c.map_x = column_int(stmt, 6);
            c.map_y = column_int(stmt, 7);
            c.map_z = column_int(stmt, 8);
            c.revival_point = column_int(stmt, 9);
            c.level = column_int(stmt, 10);
            c.exp = column_int64(stmt, 11);
            c.hp = column_int(stmt, 12);
            c.max_hp = column_int(stmt, 13);
            c.mp = column_int(stmt, 14);
            c.max_mp = column_int(stmt, 15);
            c.pu_point = column_int(stmt, 16);
            c.str = column_int(stmt, 17);
            c.hth = column_int(stmt, 18);
            c.intl = column_int(stmt, 19);
            c.wis = column_int(stmt, 20);
            c.dex = column_int(stmt, 21);
            c.money = column_int64(stmt, 22);
            c.next_iid = static_cast<std::uint32_t>(column_int64(stmt, 23));
            c.weapon_index = column_int(stmt, 24);
            c.armor_index = column_int(stmt, 25);
            c.transform_index = column_int(stmt, 26);
            c.transform_level = column_int(stmt, 27);
            c.transform_exp = column_int(stmt, 28);
            c.transform_put_on_ticks = column_int(stmt, 29);
            c.specialty = column_int(stmt, 30);
            if (c.specialty <= 0) {
                c.specialty = 1;
            }
            c.su_point = column_int(stmt, 31);
            c.rage = column_int(stmt, 32);
            c.contribution = column_int(stmt, 33);
            c.face = column_int(stmt, 34);
            c.hair = column_int(stmt, 35);
            c.deletion_state = static_cast<CharacterDeletionState>(column_int(stmt, 36));
            c.delete_after = column_int64(stmt, 37);
            c.talent_mask = static_cast<std::uint64_t>(column_int64(stmt, 38));
            c.beacon_ready_at = column_int64(stmt, 39);
            return c;
        }

        [[nodiscard]] ItemRecord item_from_stmt(const StatementPtr& stmt) {
            ItemRecord item;
            item.iid = column_int(stmt, 0);
            item.character_id = column_int(stmt, 1);
            item.account_id = column_int(stmt, 2);
            item.index = column_int(stmt, 3);
            item.prefix = column_int(stmt, 4);
            item.count = column_int(stmt, 5);
            item.slot = column_int(stmt, 6);
            item.cur_end = column_int(stmt, 7);
            item.max_end = column_int(stmt, 8);
            item.x_attack = column_int(stmt, 9);
            item.x_magic = column_int(stmt, 10);
            item.x_hit = column_int(stmt, 11);
            item.x_defense = column_int(stmt, 12);
            item.x_dodge = column_int(stmt, 13);
            item.upgrade_level = column_int(stmt, 14);
            item.upgrade_rate = column_int(stmt, 15);
            item.mix_type = column_int(stmt, 16);
            item.bof = column_int(stmt, 17);
            item.qigong_perforated = column_int(stmt, 18);
            item.qigong_effect = column_int(stmt, 19);
            item.dragon_effect = column_int(stmt, 20);
            item.random_stat_type = column_int(stmt, 21);
            item.random_stat_value = column_int(stmt, 22);
            item.transform_level = column_int(stmt, 23);
            item.transform_exp = column_int(stmt, 24);
            item.pet_expire_at = column_int64(stmt, 25);
            item.ride_expire_at = column_int64(stmt, 26);
            item.account_bound = column_int(stmt, 27) != 0;
            item.enchant_protected = column_int(stmt, 28) != 0;
            item.character_bound = column_int(stmt, 29) != 0;
            return item;
        }

        constexpr const char* kItemSelect =
            "SELECT iid, character_id, account_id, item_index, prefix, count, slot, "
            "cur_end, max_end, x_attack, x_magic, x_hit, x_defense, x_dodge, "
            "upgrade_level, upgrade_rate, mix_type, bof, qigong_perforated, qigong_effect, "
            "dragon_effect, random_stat_type, random_stat_value, transform_level, transform_exp, "
            "pet_expire_at, ride_expire_at, account_bound, enchant_protected, character_bound "
            "FROM items";

        constexpr const char* kCharacterSelect =
            "SELECT id, account_id, name, role, clazz, map, x, y, z, revival_point, level, exp, "
            "hp, "
            "max_hp, mp, max_mp, "
            "pu_point, str, hth, intl, wis, dex, money, next_iid, weapon_index, armor_index, "
            "transform_index, transform_level, transform_exp, transform_put_on_ticks, specialty, "
            "su_point, rage, contribution, face, hair, deletion_state, delete_after, talent_mask, "
            "beacon_ready_at "
            "FROM characters";

    }  // namespace

    struct PostgresStore::Impl {
        Connection db{nullptr, PQfinish};
        int changes = 0;
        [[nodiscard]] bool exec(const char* sql) const {
            return run(db.get(), sql);
        }
        [[nodiscard]] StatementPtr prepare(const char* sql) {
            return std::make_unique<Statement>(db.get(), sql, changes);
        }

        std::optional<trading_post::Rows> market_query(const std::string& sql,
                                                       const std::vector<std::string>& args) {
            auto stmt = prepare(sql.c_str());
            if (!stmt) {
                return std::nullopt;
            }
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (bind_text(stmt, static_cast<int>(i + 1), args[i].c_str(), -1, 0) != kOk) {
                    return std::nullopt;
                }
            }
            trading_post::Rows rows;
            int result = kOk;
            while ((result = step(stmt)) == kRow) {
                std::vector<std::string> row;
                for (int i = 0; i < PQnfields(stmt->result.get()); ++i) {
                    row.emplace_back(reinterpret_cast<const char*>(column_text(stmt, i)));
                }
                rows.push_back(std::move(row));
            }
            if (result != kDone) {
                return std::nullopt;
            }
            return rows;
        }

        [[nodiscard]] bool
        bind_character(const StatementPtr& stmt, const CharacterRecord& c, int id_bind) const {
            if (id_bind > 0) {
                if (bind_int64(stmt, id_bind, c.id) != kOk) {
                    return false;
                }
            }
            const int base = id_bind > 0 ? 2 : 1;
            auto text = [&](int idx, const std::string& s) {
                return bind_text(stmt, idx, s.c_str(), -1, 0) == kOk;
            };
            return bind_int64(stmt, base + 0, c.account_id) == kOk && text(base + 1, c.name) &&
                   bind_int64(stmt, base + 2, static_cast<std::int32_t>(c.role)) == kOk &&
                   bind_int64(stmt, base + 3, c.clazz) == kOk &&
                   bind_int64(stmt, base + 4, c.map) == kOk &&
                   bind_int64(stmt, base + 5, c.map_x) == kOk &&
                   bind_int64(stmt, base + 6, c.map_y) == kOk &&
                   bind_int64(stmt, base + 7, c.map_z) == kOk &&
                   bind_int64(stmt, base + 8, c.revival_point) == kOk &&
                   bind_int64(stmt, base + 9, c.level) == kOk &&
                   bind_int64(stmt, base + 10, c.exp) == kOk &&
                   bind_int64(stmt, base + 11, c.hp) == kOk &&
                   bind_int64(stmt, base + 12, c.max_hp) == kOk &&
                   bind_int64(stmt, base + 13, c.mp) == kOk &&
                   bind_int64(stmt, base + 14, c.max_mp) == kOk &&
                   bind_int64(stmt, base + 15, c.pu_point) == kOk &&
                   bind_int64(stmt, base + 16, c.str) == kOk &&
                   bind_int64(stmt, base + 17, c.hth) == kOk &&
                   bind_int64(stmt, base + 18, c.intl) == kOk &&
                   bind_int64(stmt, base + 19, c.wis) == kOk &&
                   bind_int64(stmt, base + 20, c.dex) == kOk &&
                   bind_int64(stmt, base + 21, c.money) == kOk &&
                   bind_int64(stmt, base + 22, static_cast<std::int64_t>(c.next_iid)) == kOk &&
                   bind_int64(stmt, base + 23, c.weapon_index) == kOk &&
                   bind_int64(stmt, base + 24, c.armor_index) == kOk &&
                   bind_int64(stmt, base + 25, c.transform_index) == kOk &&
                   bind_int64(stmt, base + 26, c.transform_level) == kOk &&
                   bind_int64(stmt, base + 27, c.transform_exp) == kOk &&
                   bind_int64(stmt, base + 28, c.transform_put_on_ticks) == kOk &&
                   bind_int64(stmt, base + 29, c.specialty) == kOk &&
                   bind_int64(stmt, base + 30, c.su_point) == kOk &&
                   bind_int64(stmt, base + 31, c.rage) == kOk &&
                   bind_int64(stmt, base + 32, c.contribution) == kOk &&
                   bind_int64(stmt, base + 33, c.face) == kOk &&
                   bind_int64(stmt, base + 34, c.hair) == kOk &&
                   bind_int64(stmt, base + 35, static_cast<std::int64_t>(c.talent_mask)) == kOk &&
                   bind_int64(stmt, base + 36, c.beacon_ready_at) == kOk;
        }
    };

    PostgresStore::PostgresStore() : m_impl(std::make_unique<Impl>()) {}
    PostgresStore::~PostgresStore() = default;
    bool PostgresStore::open(const std::string& dsn) {
        close();
        m_impl->db.reset(PQconnectdb(dsn.c_str()));
        if (!m_impl->db || PQstatus(m_impl->db.get()) != CONNECTION_OK) {
            log::error("PostgresStore: connection failed");
            close();
            return false;
        }
        PQsetNoticeProcessor(m_impl->db.get(), [](void*, const char*) {}, nullptr);
        auto exists = m_impl->prepare("SELECT to_regclass('openkal_schema_version') IS NOT NULL");
        if (step(exists) != kRow) {
            close();
            return false;
        }
        const bool present = column_text(exists, 0)[0] == 't';
        int current_version = 0;
        if (present) {
            auto version = m_impl->prepare("SELECT MAX(version) FROM openkal_schema_version");
            if (step(version) != kRow || column_int(version, 0) < 1 || column_int(version, 0) > 7) {
                log::error("PostgresStore: unsupported schema version");
                close();
                return false;
            }
            current_version = column_int(version, 0);
        }
        if (!m_impl->exec("SET statement_timeout = '5s'; SET lock_timeout = '3s'") ||
            !m_impl->exec("BEGIN") || !m_impl->exec(kSchemaSql) ||
            !m_impl->exec("ALTER TABLE characters ADD COLUMN IF NOT EXISTS starter_granted "
                          "BIGINT NOT NULL DEFAULT 1 CHECK(starter_granted IN (0,1)); "
                          "ALTER TABLE characters ALTER COLUMN starter_granted SET DEFAULT 0; "
                          "ALTER TABLE items ADD COLUMN IF NOT EXISTS character_bound "
                          "BIGINT NOT NULL DEFAULT 0 CHECK(character_bound IN (0,1))")) {
            (void) m_impl->exec("ROLLBACK");
            close();
            return false;
        }
        if (current_version < 4) {
            static constexpr const char* kOwnership = R"SQL(
INSERT INTO accounts(id,name,password) VALUES(-1,'!openkal-trading-post','!') ON CONFLICT(id) DO NOTHING;
UPDATE trading_post_lock SET id=id WHERE id=1;
DO $$ DECLARE constraint_name text; BEGIN
 FOR constraint_name IN SELECT conname FROM pg_constraint WHERE conrelid='items'::regclass
 AND contype='c' AND pg_get_constraintdef(oid) LIKE '%account_id IS NULL%'
 LOOP EXECUTE format('ALTER TABLE items DROP CONSTRAINT %I',constraint_name); END LOOP;
END $$;
DROP INDEX IF EXISTS ux_items_account_slot;
ALTER TABLE items ADD COLUMN location BIGINT NOT NULL DEFAULT 0 CHECK(location BETWEEN 0 AND 2);
UPDATE items SET location=1 WHERE character_id IS NULL;
UPDATE items SET account_id=characters.account_id FROM characters WHERE items.character_id=characters.id;
ALTER TABLE items ALTER COLUMN account_id SET NOT NULL;
ALTER TABLE items ADD CONSTRAINT items_storage_slot CHECK(location<>1 OR slot BETWEEN 0 AND 215);
ALTER TABLE items ADD CONSTRAINT items_location CHECK((location=0 AND character_id IS NOT NULL) OR (location IN (1,2) AND character_id IS NULL));
CREATE UNIQUE INDEX ux_items_account_slot ON items(account_id,slot) WHERE location=1;
ALTER TABLE trading_post_offers ADD COLUMN item_iid BIGINT REFERENCES items(iid);
ALTER TABLE trading_post_deliveries ADD COLUMN item_iid BIGINT REFERENCES items(iid);
)SQL";
            static constexpr const char* kReferences = R"SQL(

CREATE TABLE trading_post_offers_new(id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
 seller BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
 item_iid BIGINT NOT NULL UNIQUE REFERENCES items(iid) ON DELETE CASCADE,
 unit_price BIGINT NOT NULL CHECK(unit_price>0),clazz BIGINT NOT NULL,type BIGINT NOT NULL,grade BIGINT NOT NULL);
INSERT INTO trading_post_offers_new(id,seller,item_iid,unit_price,clazz,type,grade)
 SELECT id,seller,item_iid,unit_price,clazz,type,grade FROM trading_post_offers WHERE count>0;
CREATE TABLE trading_post_deliveries_new(id BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
 character_id BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
 item_iid BIGINT UNIQUE REFERENCES items(iid) ON DELETE CASCADE,
 item_index BIGINT NOT NULL DEFAULT 0,count BIGINT NOT NULL DEFAULT 0,
 unit_price BIGINT NOT NULL CHECK(unit_price>0),kind BIGINT NOT NULL CHECK(kind BETWEEN 0 AND 2),
 CHECK((kind=1 AND item_iid IS NULL AND item_index>0 AND count>0) OR
       (kind<>1 AND item_iid IS NOT NULL AND item_index=0 AND count=0)));
INSERT INTO trading_post_deliveries_new(id,character_id,item_iid,item_index,count,unit_price,kind)
 SELECT id,character_id,item_iid,CASE WHEN kind=1 THEN item_index ELSE 0 END,
 CASE WHEN kind=1 THEN count ELSE 0 END,unit_price,kind FROM trading_post_deliveries;
DROP TABLE trading_post_offers;
DROP TABLE trading_post_deliveries;
ALTER TABLE trading_post_offers_new RENAME TO trading_post_offers;
ALTER TABLE trading_post_deliveries_new RENAME TO trading_post_deliveries;
CREATE INDEX trading_post_search ON trading_post_offers(type,grade,unit_price,id);
CREATE INDEX trading_post_seller ON trading_post_offers(seller,id);
CREATE INDEX trading_post_collection ON trading_post_deliveries(character_id,id);
SELECT setval(pg_get_serial_sequence('trading_post_offers','id'),COALESCE(MAX(id),1),MAX(id) IS NOT NULL) FROM trading_post_offers;
SELECT setval(pg_get_serial_sequence('trading_post_deliveries','id'),COALESCE(MAX(id),1),MAX(id) IS NOT NULL) FROM trading_post_deliveries;
UPDATE trading_post_lock SET id=id WHERE id=1;
)SQL";
            if (!m_impl->exec(kOwnership) ||
                !trading_post::migrate_escrow([&](const auto& sql, const auto& args) {
                    return m_impl->market_query(sql, args);
                }) ||
                !m_impl->exec(kReferences)) {
                (void) m_impl->exec("ROLLBACK");
                close();
                return false;
            }
        }
        if (!m_impl->exec("ALTER TABLE characters ADD COLUMN IF NOT EXISTS role BIGINT NOT NULL "
                          "DEFAULT 0 CHECK(role BETWEEN 0 AND 3); "
                          "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS protocol_points "
                          "BIGINT NOT NULL DEFAULT 0 CHECK(protocol_points BETWEEN 0 AND 9); "
                          "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS protocol_kicks "
                          "BIGINT NOT NULL DEFAULT 0 CHECK(protocol_kicks BETWEEN 0 AND 3); "
                          "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS banned "
                          "BIGINT NOT NULL DEFAULT 0 CHECK(banned IN (0,1))")) {
            (void) m_impl->exec("ROLLBACK");
            close();
            return false;
        }
        if (!m_impl->exec("DELETE FROM openkal_schema_version; INSERT INTO openkal_schema_version "
                          "VALUES(7); COMMIT")) {
            (void) m_impl->exec("ROLLBACK");
            close();
            return false;
        }
        return true;
    }

    bool PostgresStore::load_castle_war(platform::Json& state) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        auto query = m_impl->prepare("SELECT revision,document FROM castle_war_state WHERE id=1");
        const int result = step(query);
        platform::Json loaded;
        if (result == kDone) {
            loaded.set_type(platform::Json::Type::Object);
            loaded.object()["revision"].set_number(0);
        } else {
            if (result != kRow ||
                !platform::Json::parse(reinterpret_cast<const char*>(column_text(query, 1)),
                                       loaded) ||
                !loaded.is_object()) {
                return false;
            }
            const auto revision = column_int64(query, 0);
            const auto& value = loaded.at("revision");
            if (revision < 1 || static_cast<std::uint64_t>(revision) >= kCastleWarRevisionLimit ||
                !value.is_number() || value.number() != static_cast<double>(revision)) {
                return false;
            }
        }
        state = std::move(loaded);
        return true;
    }

    bool PostgresStore::commit_castle_war(std::uint64_t expected_revision,
                                          const platform::Json& state) {
        if (!m_impl || !m_impl->db || !state.is_object() ||
            expected_revision >= kCastleWarRevisionLimit - 1) {
            return false;
        }
        const auto revision = static_cast<std::int64_t>(expected_revision + 1);
        const auto& value = state.at("revision");
        if (!value.is_number() || value.number() != static_cast<double>(revision)) {
            return false;
        }
        auto command = m_impl->prepare(
            expected_revision == 0
                ? "INSERT INTO castle_war_state(id,revision,document) VALUES(1,?,?) "
                  "ON CONFLICT(id) DO NOTHING"
                : "UPDATE castle_war_state SET revision=?,document=? WHERE id=1 AND revision=?");
        const auto document = platform::Json::stringify(state, 0);
        if (bind_int64(command, 1, revision) != kOk ||
            bind_text(command, 2, document.c_str(), -1, 0) != kOk ||
            (expected_revision != 0 &&
             bind_int64(command, 3, static_cast<std::int64_t>(expected_revision)) != kOk)) {
            return false;
        }
        return step(command) == kDone && m_impl->changes == 1;
    }

    bool PostgresStore::load_guilds(platform::Json& state) {
        using J = platform::Json;
        if (!m_impl || !m_impl->db) {
            return false;
        }
        auto meta = m_impl->prepare("SELECT document FROM guild_meta WHERE id=1");
        if (step(meta) != kRow ||
            !J::parse(reinterpret_cast<const char*>(column_text(meta, 0)), state) ||
            !state.is_object()) {
            return false;
        }
        for (const auto& entry : std::vector<std::pair<const char*, const char*>>{
                 {"guilds", "SELECT document FROM guilds ORDER BY id"},
                 {"characters", "SELECT document FROM guild_character_state ORDER BY character_id"},
                 {"applications",
                  "SELECT document FROM guild_applications ORDER BY guild_id,character_id"},
                 {"receipts", "SELECT document FROM guild_operations ORDER BY actor,request_id"}}) {
            J rows;
            rows.set_type(J::Type::Array);
            auto query = m_impl->prepare(entry.second);
            int rc = 0;
            while ((rc = step(query)) == kRow) {
                J row;
                if (!J::parse(reinterpret_cast<const char*>(column_text(query, 0)), row) ||
                    !row.is_object()) {
                    return false;
                }
                rows.array().push_back(std::move(row));
            }
            if (rc != kDone) {
                return false;
            }
            state.object()[entry.first] = std::move(rows);
        }
        auto ranking = m_impl->prepare("SELECT document FROM guild_ranking WHERE id=1");
        J rows;
        if (step(ranking) != kRow ||
            !J::parse(reinterpret_cast<const char*>(column_text(ranking, 0)), rows) ||
            !rows.is_array()) {
            return false;
        }
        state.object()["ranking"] = std::move(rows);
        return true;
    }
    bool PostgresStore::commit_guilds(std::uint64_t previous,
                                      const platform::Json& state,
                                      const std::vector<CharacterRecord>& characters) {
        using J = platform::Json;
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN")) {
            return false;
        }
        const auto fail = [&]() {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        auto lock = m_impl->prepare("SELECT document FROM guild_meta WHERE id=1 FOR UPDATE");
        J old;
        if (step(lock) != kRow ||
            !J::parse(reinterpret_cast<const char*>(column_text(lock, 0)), old) ||
            static_cast<std::uint64_t>(old.at("revision").number()) != previous) {
            return fail();
        }
        for (const auto& c : characters) {
            if (!save_character(c)) {
                return fail();
            }
        }
        auto all = m_impl->prepare("SELECT id FROM guilds");
        std::vector<std::int64_t> removed;
        int rc = 0;
        while ((rc = step(all)) == kRow) {
            const auto id = column_int64(all, 0);
            bool found = false;
            for (const auto& g : state.at("guilds").array()) {
                if (static_cast<std::int64_t>(g.at("id").number()) == id) {
                    found = true;
                }
            }
            if (!found) {
                removed.push_back(id);
            }
        }
        if (rc != kDone) {
            return fail();
        }
        for (auto id : removed) {
            auto erase = m_impl->prepare("DELETE FROM guilds WHERE id=?");
            bind_int64(erase, 1, id);
            if (step(erase) != kDone) {
                return fail();
            }
        }
        std::vector<const J*> changed_guilds;
        for (const auto& g : state.at("guilds").array()) {
            const auto id = static_cast<std::int64_t>(g.at("id").number());
            const auto document = J::stringify(g, 0);
            auto upsert = m_impl->prepare(
                "INSERT INTO guilds(id,name_key,document) VALUES(?,?,?) ON CONFLICT(id) DO UPDATE "
                "SET name_key=excluded.name_key,document=excluded.document WHERE "
                "guilds.document<>excluded.document");
            bind_int64(upsert, 1, id);
            bind_text(upsert, 2, g.at("key").string().c_str(), -1, 0);
            bind_text(upsert, 3, document.c_str(), -1, 0);
            if (step(upsert) != kDone) {
                return fail();
            }
            if (m_impl->changes) {
                auto erase = m_impl->prepare("DELETE FROM guild_members WHERE guild_id=?");
                bind_int64(erase, 1, id);
                if (step(erase) != kDone) {
                    return fail();
                }
                changed_guilds.push_back(&g);
            }
        }
        // Clear every changed roster before inserting cross-guild transfers.
        for (const auto* group : changed_guilds) {
            const auto& g = *group;
            const auto id = static_cast<std::int64_t>(g.at("id").number());
            for (const auto& member : g.at("members").array()) {
                auto add = m_impl->prepare(
                    "INSERT INTO guild_members(character_id,guild_id,rank) VALUES(?,?,?)");
                bind_int64(add, 1, static_cast<std::int64_t>(member.at("id").number()));
                bind_int64(add, 2, id);
                bind_int64(add, 3, member.at("rank").as_int());
                if (step(add) != kDone) {
                    return fail();
                }
            }
        }
        if (!m_impl->exec("DELETE FROM guild_applications; DELETE FROM guild_operations")) {
            return fail();
        }
        for (const auto& a : state.at("applications").array()) {
            auto add = m_impl->prepare(
                "INSERT INTO guild_applications(guild_id,character_id,document) VALUES(?,?,?)");
            const auto doc = J::stringify(a, 0);
            bind_int64(add, 1, static_cast<std::int64_t>(a.at("guild").number()));
            bind_int64(add, 2, static_cast<std::int64_t>(a.at("character").number()));
            bind_text(add, 3, doc.c_str(), -1, 0);
            if (step(add) != kDone) {
                return fail();
            }
        }
        for (const auto& a : state.at("characters").array()) {
            auto add = m_impl->prepare(
                "INSERT INTO guild_character_state(character_id,document) VALUES(?,?) ON "
                "CONFLICT(character_id) DO UPDATE SET document=excluded.document WHERE "
                "guild_character_state.document<>excluded.document");
            const auto doc = J::stringify(a, 0);
            bind_int64(add, 1, static_cast<std::int64_t>(a.at("id").number()));
            bind_text(add, 2, doc.c_str(), -1, 0);
            if (step(add) != kDone) {
                return fail();
            }
        }
        for (const auto& a : state.at("receipts").array()) {
            auto add = m_impl->prepare(
                "INSERT INTO guild_operations(actor,request_id,document) VALUES(?,?,?)");
            const auto doc = J::stringify(a, 0);
            bind_int64(add, 1, static_cast<std::int64_t>(a.at("actor").number()));
            bind_text(add, 2, a.at("request").string().c_str(), -1, 0);
            bind_text(add, 3, doc.c_str(), -1, 0);
            if (step(add) != kDone) {
                return fail();
            }
        }
        const auto ranks = J::stringify(state.at("ranking"), 0);
        auto ranking = m_impl->prepare("UPDATE guild_ranking SET document=? WHERE id=1");
        bind_text(ranking, 1, ranks.c_str(), -1, 0);
        if (step(ranking) != kDone) {
            return fail();
        }
        J metadata = state;
        for (const auto* key : {"guilds", "characters", "applications", "receipts", "ranking"}) {
            metadata.object().erase(key);
        }
        const auto document = J::stringify(metadata, 0);
        auto meta = m_impl->prepare("UPDATE guild_meta SET document=? WHERE id=1");
        bind_text(meta, 1, document.c_str(), -1, 0);
        if (step(meta) != kDone || !m_impl->exec("COMMIT")) {
            return fail();
        }
        return true;
    }

    void PostgresStore::close() {
        m_impl->db.reset();
    }
    bool PostgresStore::execute(std::string_view sql) {
        return m_impl->db && m_impl->exec(std::string(sql).c_str());
    }
    bool PostgresStore::ping() {
        if (!m_impl->db || PQstatus(m_impl->db.get()) != CONNECTION_OK) {
            return false;
        }
        auto statement = m_impl->prepare("SELECT 1");
        return step(statement) == kRow && column_int(statement, 0) == 1;
    }
    bool PostgresStore::acquire_server_lease() {
        if (!m_impl->db) {
            return false;
        }
        auto lock = m_impl->prepare("SELECT pg_try_advisory_lock(hashtext(current_database()), "
                                    "hashtext(current_schema()))");
        if (step(lock) != kRow || column_text(lock, 0)[0] != 't') {
            return false;
        }
        auto label = m_impl->prepare(
            "SELECT set_config('application_name', 'openkal-world:' || current_schema(), false)");
        return step(label) == kRow;
    }
    bool PostgresStore::register_account(const std::string& name,
                                         const std::string& email,
                                         const std::string& hash) {
        if (!m_impl->db) {
            return false;
        }
        auto s = m_impl->prepare(
            "INSERT INTO accounts(name,email,password) VALUES(?,?,?) ON CONFLICT DO NOTHING");
        bind_text(s, 1, name.c_str(), -1, 0);
        bind_text(s, 2, email.c_str(), -1, 0);
        bind_text(s, 3, hash.c_str(), -1, 0);
        return step(s) == kDone && m_impl->changes == 1;
    }

    std::optional<AccountRecord> PostgresStore::find_account(const std::string& name) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        StatementPtr stmt =
            m_impl->prepare("SELECT id, name, password FROM accounts WHERE name = ? AND id > 0");
        if (!stmt) {
            return std::nullopt;
        }
        bind_text(stmt, 1, name.c_str(), -1, 0);
        std::optional<AccountRecord> out;
        if (step(stmt) == kRow) {
            AccountRecord a;
            a.id = column_int(stmt, 0);
            const unsigned char* n = column_text(stmt, 1);
            const unsigned char* p = column_text(stmt, 2);
            a.name = n ? reinterpret_cast<const char*>(n) : "";
            a.password = p ? reinterpret_cast<const char*>(p) : "";
            out = a;
        }
        stmt.reset();
        return out;
    }

    std::optional<AccountDiscipline>
    PostgresStore::load_account_discipline(std::int32_t account_id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        const auto stmt = m_impl->prepare(account_discipline::kLoad);
        bind_int64(stmt, 1, account_id);
        if (step(stmt) != kRow) {
            return std::nullopt;
        }
        return AccountDiscipline{
            column_int(stmt, 0), column_int(stmt, 1), column_int(stmt, 2) != 0};
    }

    std::optional<AccountDiscipline>
    PostgresStore::update_account_discipline(std::int32_t account_id,
                                             AccountDisciplineChange change) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        const auto stmt = m_impl->prepare(change == AccountDisciplineChange::Violation
                                              ? account_discipline::kViolation
                                              : account_discipline::kDecay);
        bind_int64(stmt, 1, account_id);
        if (step(stmt) != kRow) {
            return std::nullopt;
        }
        return AccountDiscipline{
            column_int(stmt, 0), column_int(stmt, 1), column_int(stmt, 2) != 0};
    }

    bool PostgresStore::upsert_account(const AccountRecord& account) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (account.id > 0) {
            StatementPtr stmt =
                m_impl->prepare("INSERT INTO accounts(id, name, password) VALUES(?, ?, ?) "
                                "ON CONFLICT(id) DO UPDATE SET name=excluded.name, "
                                "password=excluded.password");
            if (!stmt) {
                return false;
            }
            bind_int64(stmt, 1, account.id);
            bind_text(stmt, 2, account.name.c_str(), -1, 0);
            bind_text(stmt, 3, account.password.c_str(), -1, 0);
            const bool ok = step(stmt) == kDone;
            if (!ok) {
                log::error(std::string("PostgresStore upsert_account: ") +
                           "database statement failed");
            }
            stmt.reset();
            return ok;
        }
        StatementPtr stmt =
            m_impl->prepare("INSERT INTO accounts(name, password) VALUES(?, ?) "
                            "ON CONFLICT(name) DO UPDATE SET password=excluded.password");
        if (!stmt) {
            return false;
        }
        bind_text(stmt, 1, account.name.c_str(), -1, 0);
        bind_text(stmt, 2, account.password.c_str(), -1, 0);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore upsert_account: ") + "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    std::vector<CharacterRecord> PostgresStore::list_characters(std::int32_t account_id) {
        std::vector<CharacterRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kCharacterSelect) +
                                " WHERE account_id = ? AND deletion_state = 0 ORDER BY id";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, account_id);
        while (step(stmt) == kRow) {
            out.push_back(character_from_stmt(stmt));
        }
        stmt.reset();
        return out;
    }

    std::vector<CharacterRecord> PostgresStore::list_deleted_characters(std::int32_t account_id,
                                                                        std::int64_t now_seconds) {
        std::vector<CharacterRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kCharacterSelect) +
                                " WHERE account_id = ? AND deletion_state = 1 AND delete_after > ? "
                                "ORDER BY delete_after, id";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, account_id);
        bind_int64(stmt, 2, now_seconds);
        while (step(stmt) == kRow) {
            out.push_back(character_from_stmt(stmt));
        }
        stmt.reset();
        return out;
    }

    std::optional<CharacterRecord> PostgresStore::load_character(std::int32_t id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        const std::string sql =
            std::string(kCharacterSelect) + " WHERE id = ? AND deletion_state = 0";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return std::nullopt;
        }
        bind_int64(stmt, 1, id);
        std::optional<CharacterRecord> out;
        if (step(stmt) == kRow) {
            out = character_from_stmt(stmt);
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::save_character(const CharacterRecord& character) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        constexpr const char* kCols =
            "account_id, name, role, clazz, map, x, y, z, revival_point, level, exp, hp, max_hp, "
            "mp, "
            "max_mp, "
            "pu_point, str, hth, intl, wis, dex, money, next_iid, weapon_index, armor_index, "
            "transform_index, transform_level, transform_exp, transform_put_on_ticks, specialty, "
            "su_point, rage, contribution, face, hair, talent_mask, beacon_ready_at";
        if (character.id > 0) {
            const std::string sql =
                std::string("INSERT INTO characters(id, ") + kCols +
                ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "?, "
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
                "account_id=excluded.account_id, "
                "name=excluded.name, clazz=excluded.clazz, map=excluded.map, x=excluded.x, "
                "y=excluded.y, "
                "z=excluded.z, revival_point=excluded.revival_point, level=excluded.level, "
                "exp=excluded.exp, hp=excluded.hp, "
                "max_hp=excluded.max_hp, mp=excluded.mp, max_mp=excluded.max_mp, "
                "pu_point=excluded.pu_point, str=excluded.str, hth=excluded.hth, "
                "intl=excluded.intl, wis=excluded.wis, dex=excluded.dex, money=excluded.money, "
                "next_iid=excluded.next_iid, weapon_index=excluded.weapon_index, "
                "armor_index=excluded.armor_index, transform_index=excluded.transform_index, "
                "transform_level=excluded.transform_level, transform_exp=excluded.transform_exp, "
                "transform_put_on_ticks=excluded.transform_put_on_ticks, "
                "specialty=excluded.specialty, su_point=excluded.su_point, rage=excluded.rage, "
                "contribution=excluded.contribution, face=excluded.face, hair=excluded.hair, "
                "talent_mask=excluded.talent_mask, beacon_ready_at=excluded.beacon_ready_at";
            StatementPtr stmt = m_impl->prepare(sql.c_str());
            if (!stmt) {
                return false;
            }
            if (!m_impl->bind_character(stmt, character, 1)) {
                stmt.reset();
                return false;
            }
            const bool ok = step(stmt) == kDone;
            if (!ok) {
                log::error(std::string("PostgresStore save_character: ") +
                           "database statement failed");
            }
            stmt.reset();
            return ok;
        }
        const std::string sql =
            std::string("INSERT INTO characters(") + kCols +
            ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return false;
        }
        if (!m_impl->bind_character(stmt, character, 0)) {
            stmt.reset();
            return false;
        }
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore save_character: ") + "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    bool PostgresStore::schedule_character_deletion(std::int32_t id,
                                                    std::int32_t account_id,
                                                    std::int64_t now_seconds) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "UPDATE characters SET deletion_state = 1, delete_after = ? "
            "WHERE id = ? AND account_id = ? AND deletion_state = 0 "
            "AND NOT EXISTS (SELECT 1 FROM guild_members WHERE character_id = characters.id) "
            "AND (SELECT COUNT(*) FROM characters WHERE account_id = ? "
            "AND deletion_state = 1) < ?");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, now_seconds + kCharacterDeletionGraceSeconds);
        bind_int64(stmt, 2, id);
        bind_int64(stmt, 3, account_id);
        bind_int64(stmt, 4, account_id);
        bind_int64(stmt, 5, static_cast<std::int64_t>(kMaxDeletedCharactersPerAccount));
        const bool ok = step(stmt) == kDone && m_impl->changes > 0;
        stmt.reset();
        return ok;
    }

    bool PostgresStore::restore_character(std::int32_t id,
                                          std::int32_t account_id,
                                          std::int64_t now_seconds) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "UPDATE characters SET deletion_state = 0, delete_after = 0 "
            "WHERE id = ? AND account_id = ? AND deletion_state = 1 AND delete_after > ? "
            "AND (SELECT COUNT(*) FROM characters WHERE account_id = ? AND deletion_state = 0) "
            "< ?");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, id);
        bind_int64(stmt, 2, account_id);
        bind_int64(stmt, 3, now_seconds);
        bind_int64(stmt, 4, account_id);
        bind_int64(stmt, 5, static_cast<std::int64_t>(kMaxCharactersPerAccount));
        const bool ok = step(stmt) == kDone && m_impl->changes > 0;
        stmt.reset();
        return ok;
    }

    CharacterPurgeResult PostgresStore::process_character_deletions(std::int64_t now_seconds,
                                                                    std::size_t row_budget) {
        if (!m_impl || !m_impl->db || row_budget == 0) {
            return CharacterPurgeResult::Error;
        }
        constexpr std::size_t kMaxPurgeBatch = 256;
        const std::int64_t bounded_budget =
            static_cast<std::int64_t>(std::min(row_budget, kMaxPurgeBatch));

        StatementPtr due = m_impl->prepare(
            "SELECT 1 FROM characters "
            "WHERE deletion_state = 2 OR (deletion_state = 1 AND delete_after <= ?) LIMIT 1");
        if (!due) {
            return CharacterPurgeResult::Error;
        }
        bind_int64(due, 1, now_seconds);
        const bool has_due_character = step(due) == kRow;
        due.reset();
        if (!has_due_character) {
            return CharacterPurgeResult::Idle;
        }

        if (!m_impl->exec("BEGIN")) {
            return CharacterPurgeResult::Error;
        }
        auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return CharacterPurgeResult::Error;
        };

        StatementPtr select = m_impl->prepare(
            "SELECT id FROM characters "
            "WHERE deletion_state = 2 OR (deletion_state = 1 AND delete_after <= ?) "
            "ORDER BY deletion_state DESC, delete_after, id LIMIT 1");
        if (!select) {
            return rollback();
        }
        bind_int64(select, 1, now_seconds);
        if (step(select) != kRow) {
            select.reset();
            if (!m_impl->exec("COMMIT")) {
                return CharacterPurgeResult::Error;
            }
            return CharacterPurgeResult::Idle;
        }
        const std::int32_t character_id = column_int(select, 0);
        select.reset();

        StatementPtr mark = m_impl->prepare(
            "UPDATE characters SET deletion_state = 2 WHERE id = ? AND deletion_state = 1");
        if (!mark) {
            return rollback();
        }
        bind_int64(mark, 1, character_id);
        const bool marked = step(mark) == kDone;
        mark.reset();
        if (!marked) {
            return rollback();
        }

        if (!m_impl->exec("UPDATE trading_post_lock SET id=id WHERE id=1")) {
            return rollback();
        }

        static constexpr const char* kDependentTables[] = {
            "items", "skills", "shortcuts", "quest_flags", "buff_remain", "inventory_unlock"};
        for (const char* table : kDependentTables) {
            const bool item_table = std::string_view(table) == "items";
            const std::string owner_filter = item_table
                                                 ? "character_id=? OR iid IN (SELECT item_iid FROM "
                                                   "trading_post_offers WHERE seller=?) "
                                                   "OR iid IN (SELECT item_iid FROM "
                                                   "trading_post_deliveries WHERE character_id=?)"
                                                 : "character_id=?";
            const std::string sql = std::string("DELETE FROM ") + table +
                                    " WHERE ctid IN (SELECT ctid FROM " + table + " WHERE " +
                                    owner_filter + " LIMIT ?)";
            StatementPtr remove = m_impl->prepare(sql.c_str());
            if (!remove) {
                return rollback();
            }
            bind_int64(remove, 1, character_id);
            if (item_table) {
                bind_int64(remove, 2, character_id);
                bind_int64(remove, 3, character_id);
            }
            bind_int64(remove, item_table ? 4 : 2, bounded_budget);
            const bool removed = step(remove) == kDone;
            const int changed = m_impl->changes;
            remove.reset();
            if (!removed) {
                return rollback();
            }
            if (changed > 0) {
                if (!m_impl->exec("COMMIT")) {
                    return CharacterPurgeResult::Error;
                }
                return CharacterPurgeResult::WorkRemaining;
            }
        }

        StatementPtr remove_character =
            m_impl->prepare("DELETE FROM characters WHERE id = ? AND deletion_state = 2");
        if (!remove_character) {
            return rollback();
        }
        bind_int64(remove_character, 1, character_id);
        const bool removed = step(remove_character) == kDone;
        const int changed = m_impl->changes;
        remove_character.reset();
        if (!removed || changed == 0) {
            return rollback();
        }
        if (!m_impl->exec("COMMIT")) {
            return CharacterPurgeResult::Error;
        }
        return CharacterPurgeResult::CharacterPurged;
    }

    std::vector<ItemRecord> PostgresStore::load_items(std::int32_t character_id) {
        std::vector<ItemRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kItemSelect) + " WHERE character_id = ? ORDER BY iid";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            out.push_back(item_from_stmt(stmt));
        }
        stmt.reset();
        return out;
    }

    std::vector<ItemRecord> PostgresStore::load_storage_items(std::int32_t account_id) {
        std::vector<ItemRecord> out;
        if (!m_impl || !m_impl->db || account_id <= 0) {
            return out;
        }
        const std::string sql = std::string(kItemSelect) +
                                " WHERE account_id = ? AND location=1 AND character_id IS NULL "
                                "ORDER BY slot, iid";
        StatementPtr stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, account_id);
        while (step(stmt) == kRow) {
            out.push_back(item_from_stmt(stmt));
        }
        stmt.reset();
        return out;
    }

    std::optional<std::int64_t> PostgresStore::load_storage_money(std::int32_t account_id) {
        if (!m_impl || !m_impl->db || account_id <= 0) {
            return std::nullopt;
        }
        auto stmt = m_impl->prepare(
            "SELECT COALESCE(s.balance,0) FROM accounts a LEFT JOIN account_storage_money s "
            "ON s.account_id=a.id WHERE a.id=?");
        if (!stmt) {
            return std::nullopt;
        }
        bind_int64(stmt, 1, account_id);
        if (step(stmt) == kRow) {
            return column_int64(stmt, 0);
        }
        return std::nullopt;
    }

    std::optional<std::int64_t> PostgresStore::transfer_storage_money(std::int32_t account_id,
                                                                      std::int32_t character_id,
                                                                      std::int64_t wallet_after,
                                                                      std::int64_t change) {
        if (!m_impl || !m_impl->db || account_id <= 0 || character_id <= 0 || wallet_after < 0 ||
            change == 0 || change == std::numeric_limits<std::int64_t>::min() ||
            !m_impl->exec("BEGIN")) {
            return std::nullopt;
        }
        const auto rollback = [&]() -> std::optional<std::int64_t> {
            (void) m_impl->exec("ROLLBACK");
            return std::nullopt;
        };
        auto stmt = m_impl->prepare(
            "UPDATE characters SET money=? WHERE id=? AND account_id=? AND deletion_state=0");
        if (!stmt) {
            return rollback();
        }
        bind_int64(stmt, 1, wallet_after);
        bind_int64(stmt, 2, character_id);
        bind_int64(stmt, 3, account_id);
        if (step(stmt) != kDone || m_impl->changes != 1) {
            return rollback();
        }
        stmt = m_impl->prepare("INSERT INTO account_storage_money(account_id,balance) VALUES(?,0) "
                               "ON CONFLICT(account_id) DO NOTHING");
        if (!stmt) {
            return rollback();
        }
        bind_int64(stmt, 1, account_id);
        if (step(stmt) != kDone) {
            return rollback();
        }
        stmt = m_impl->prepare("UPDATE account_storage_money SET balance=balance+? "
                               "WHERE account_id=? AND balance BETWEEN ? AND ? RETURNING balance");
        if (!stmt) {
            return rollback();
        }
        bind_int64(stmt, 1, change);
        bind_int64(stmt, 2, account_id);
        bind_int64(stmt, 3, std::max<std::int64_t>(0, -change));
        bind_int64(
            stmt, 4, std::numeric_limits<std::int64_t>::max() - std::max<std::int64_t>(0, change));
        if (step(stmt) != kRow) {
            return rollback();
        }
        const auto balance = column_int64(stmt, 0);
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return balance;
    }

    bool PostgresStore::consume_item_with_rewards(const ItemRecord& box,
                                                  const std::vector<ItemRecord>& rewards) {
        if (!m_impl || !m_impl->db || box.iid <= 0 || box.character_id <= 0 || box.count <= 0 ||
            box.slot != 0 || rewards.empty()) {
            return false;
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        StatementPtr consume = m_impl->prepare(
            box.count == 1
                ? "DELETE FROM items WHERE iid=? AND character_id=? AND item_index=? AND count=? "
                  "AND slot=0"
                : "UPDATE items SET count=count-1 WHERE iid=? AND character_id=? AND item_index=? "
                  "AND count=? AND slot=0");
        if (!consume) {
            return rollback();
        }
        bind_int64(consume, 1, box.iid);
        bind_int64(consume, 2, box.character_id);
        bind_int64(consume, 3, box.index);
        bind_int64(consume, 4, box.count);
        const bool consumed = step(consume) == kDone && m_impl->changes == 1;
        consume.reset();
        if (!consumed) {
            return rollback();
        }
        for (const auto& reward : rewards) {
            if (reward.iid <= 0 || reward.iid == box.iid || reward.index <= 0 ||
                reward.count <= 0 || reward.character_id != box.character_id || reward.slot != 0) {
                return rollback();
            }
            // insert_item also supports upserts, so reject existing ids before calling it.
            StatementPtr existing = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
            if (!existing) {
                return rollback();
            }
            bind_int64(existing, 1, reward.iid);
            const int result = step(existing);
            existing.reset();
            if (result != kDone || !insert_item(reward)) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool PostgresStore::grant_starter_items(const CharacterRecord& character,
                                            const std::vector<ItemRecord>& items) {
        if (!m_impl || !m_impl->db || character.id <= 0) {
            return false;
        }
        const bool owns_transaction = PQtransactionStatus(m_impl->db.get()) == PQTRANS_IDLE;
        if (!m_impl->exec(owns_transaction ? "BEGIN" : "SAVEPOINT starter_kit")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec(owns_transaction ? "ROLLBACK"
                                                 : "ROLLBACK TO starter_kit; RELEASE starter_kit");
            return false;
        };
        auto claim = m_impl->prepare(
            "UPDATE characters SET starter_granted=1 WHERE id=? AND starter_granted=0 "
            "AND deletion_state=0 AND NOT EXISTS (SELECT 1 FROM items WHERE character_id=?)");
        if (!claim) {
            return rollback();
        }
        bind_int64(claim, 1, character.id);
        bind_int64(claim, 2, character.id);
        const bool claimed = step(claim) == kDone && m_impl->changes == 1;
        if (!claimed) {
            return rollback();
        }
        for (const auto& item : items) {
            if (item.iid <= 0 || item.count <= 0 || item.character_id != character.id ||
                !item.character_bound) {
                return rollback();
            }
            auto existing = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
            if (!existing) {
                return rollback();
            }
            bind_int64(existing, 1, item.iid);
            if (step(existing) != kDone || !insert_item(item)) {
                return rollback();
            }
        }
        if (!save_character(character) ||
            !m_impl->exec(owns_transaction ? "COMMIT" : "RELEASE starter_kit")) {
            return rollback();
        }
        return true;
    }

    bool PostgresStore::insert_item(const ItemRecord& item) {
        if (!m_impl || !m_impl->db || (item.character_bound && item.character_id <= 0)) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO items(iid, character_id, account_id, item_index, prefix, count, slot, "
            "cur_end, max_end, x_attack, x_magic, x_hit, x_defense, x_dodge, "
            "upgrade_level, upgrade_rate, mix_type, bof, qigong_perforated, qigong_effect, "
            "dragon_effect, random_stat_type, random_stat_value, "
            "transform_level, transform_exp, pet_expire_at, ride_expire_at, account_bound, "
            "enchant_protected, character_bound, location) "
            "VALUES(?, ?, COALESCE((SELECT account_id FROM characters WHERE id=?), ?), ?, ?, ?, ?, "
            "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "?, ?, ?, ?, ?) "
            "ON CONFLICT(iid) DO UPDATE SET character_id=excluded.character_id, "
            "account_id=excluded.account_id, "
            "item_index=excluded.item_index, prefix=excluded.prefix, "
            "count=excluded.count, slot=excluded.slot, cur_end=excluded.cur_end, "
            "max_end=excluded.max_end, x_attack=excluded.x_attack, "
            "x_magic=excluded.x_magic, x_hit=excluded.x_hit, "
            "x_defense=excluded.x_defense, x_dodge=excluded.x_dodge, "
            "upgrade_level=excluded.upgrade_level, "
            "upgrade_rate=excluded.upgrade_rate, mix_type=excluded.mix_type, "
            "bof=excluded.bof, qigong_perforated=excluded.qigong_perforated, "
            "qigong_effect=excluded.qigong_effect, dragon_effect=excluded.dragon_effect, "
            "random_stat_type=excluded.random_stat_type, "
            "random_stat_value=excluded.random_stat_value, "
            "transform_level=excluded.transform_level, "
            "transform_exp=excluded.transform_exp, pet_expire_at=excluded.pet_expire_at, "
            "ride_expire_at=excluded.ride_expire_at, account_bound=excluded.account_bound, "
            "enchant_protected=excluded.enchant_protected, "
            "character_bound=excluded.character_bound, location=excluded.location");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, item.iid);
        if (item.character_id > 0) {
            bind_int64(stmt, 2, item.character_id);
        } else {
            bind_null(stmt, 2);
        }
        bind_int64(stmt, 3, item.character_id);
        if (item.account_id > 0) {
            bind_int64(stmt, 4, item.account_id);
        } else {
            bind_null(stmt, 4);
        }
        bind_int64(stmt, 5, item.index);
        bind_int64(stmt, 6, item.prefix);
        bind_int64(stmt, 7, item.count);
        bind_int64(stmt, 8, item.slot);
        bind_int64(stmt, 9, item.cur_end);
        bind_int64(stmt, 10, item.max_end);
        bind_int64(stmt, 11, item.x_attack);
        bind_int64(stmt, 12, item.x_magic);
        bind_int64(stmt, 13, item.x_hit);
        bind_int64(stmt, 14, item.x_defense);
        bind_int64(stmt, 15, item.x_dodge);
        bind_int64(stmt, 16, item.upgrade_level);
        bind_int64(stmt, 17, item.upgrade_rate);
        bind_int64(stmt, 18, item.mix_type);
        bind_int64(stmt, 19, item.bof);
        bind_int64(stmt, 20, item.qigong_perforated);
        bind_int64(stmt, 21, item.qigong_effect);
        bind_int64(stmt, 22, item.dragon_effect);
        bind_int64(stmt, 23, item.random_stat_type);
        bind_int64(stmt, 24, item.random_stat_value);
        bind_int64(stmt, 25, item.transform_level);
        bind_int64(stmt, 26, item.transform_exp);
        bind_int64(stmt, 27, item.pet_expire_at);
        bind_int64(stmt, 28, item.ride_expire_at);
        bind_int64(stmt, 29, item.account_bound ? 1 : 0);
        bind_int64(stmt, 30, item.enchant_protected ? 1 : 0);
        bind_int64(stmt, 31, item.character_bound ? 1 : 0);
        bind_int64(stmt, 32, item.character_id > 0 ? 0 : 1);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore insert_item: ") + "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    bool PostgresStore::update_item(const ItemRecord& item) {
        if (!m_impl || !m_impl->db || (item.character_bound && item.character_id <= 0)) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "UPDATE items SET item_index = ?, prefix = ?, count = ?, slot = ?, "
            "character_id = ?, account_id = COALESCE((SELECT account_id FROM characters WHERE "
            "id=?), ?), cur_end = ?, max_end = ?, x_attack = ?, x_magic = "
            "?, "
            "x_hit = ?, x_defense = ?, x_dodge = ?, upgrade_level = ?, upgrade_rate = ?, "
            "mix_type = ?, bof = ?, qigong_perforated = ?, qigong_effect = ?, dragon_effect = ?, "
            "random_stat_type = ?, random_stat_value = ?, transform_level = ?, transform_exp = ?, "
            "pet_expire_at = ?, ride_expire_at = ?, account_bound = ?, enchant_protected = ?, "
            "character_bound = ?, location = ? "
            "WHERE iid = ?");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, item.index);
        bind_int64(stmt, 2, item.prefix);
        bind_int64(stmt, 3, item.count);
        bind_int64(stmt, 4, item.slot);
        if (item.character_id > 0) {
            bind_int64(stmt, 5, item.character_id);
        } else {
            bind_null(stmt, 5);
        }
        bind_int64(stmt, 6, item.character_id);
        if (item.account_id > 0) {
            bind_int64(stmt, 7, item.account_id);
        } else {
            bind_null(stmt, 7);
        }
        bind_int64(stmt, 8, item.cur_end);
        bind_int64(stmt, 9, item.max_end);
        bind_int64(stmt, 10, item.x_attack);
        bind_int64(stmt, 11, item.x_magic);
        bind_int64(stmt, 12, item.x_hit);
        bind_int64(stmt, 13, item.x_defense);
        bind_int64(stmt, 14, item.x_dodge);
        bind_int64(stmt, 15, item.upgrade_level);
        bind_int64(stmt, 16, item.upgrade_rate);
        bind_int64(stmt, 17, item.mix_type);
        bind_int64(stmt, 18, item.bof);
        bind_int64(stmt, 19, item.qigong_perforated);
        bind_int64(stmt, 20, item.qigong_effect);
        bind_int64(stmt, 21, item.dragon_effect);
        bind_int64(stmt, 22, item.random_stat_type);
        bind_int64(stmt, 23, item.random_stat_value);
        bind_int64(stmt, 24, item.transform_level);
        bind_int64(stmt, 25, item.transform_exp);
        bind_int64(stmt, 26, item.pet_expire_at);
        bind_int64(stmt, 27, item.ride_expire_at);
        bind_int64(stmt, 28, item.account_bound ? 1 : 0);
        bind_int64(stmt, 29, item.enchant_protected ? 1 : 0);
        bind_int64(stmt, 30, item.character_bound ? 1 : 0);
        bind_int64(stmt, 31, item.character_id > 0 ? 0 : 1);
        bind_int64(stmt, 32, item.iid);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore update_item: ") + "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    bool PostgresStore::update_equipment_items(const std::vector<ItemRecord>& items) {
        if (!m_impl || !m_impl->db || items.empty() || items.size() > 9) {
            return false;
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                if (items[i].iid == items[j].iid) {
                    return false;
                }
            }
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        for (const auto& item : items) {
            if (item.iid <= 0 || item.character_id <= 0 || item.count != 1 ||
                item.character_id != items.front().character_id || item.slot < 0 || item.slot > 6) {
                return rollback();
            }
            auto statement = m_impl->prepare("SELECT 1 FROM items WHERE iid=? AND character_id=? "
                                             "AND count=1");
            if (!statement) {
                return rollback();
            }
            bind_int64(statement, 1, item.iid);
            bind_int64(statement, 2, item.character_id);
            const bool owned = step(statement) == kRow;
            statement.reset();
            if (!owned || !update_item(item) || m_impl->changes != 1) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool PostgresStore::exchange_inventory_items(const std::vector<ItemRecord>& expected,
                                                 const std::vector<ItemRecord>& replacements) {
        if (!m_impl || !m_impl->db || replacements.empty() || !m_impl->exec("BEGIN")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        const auto owner = replacements.front().character_id;
        if (owner <= 0) {
            return rollback();
        }
        for (const auto& item : expected) {
            if (item.iid <= 0 || owner <= 0 || item.character_id != owner || item.slot != 0 ||
                item.count <= 0) {
                return rollback();
            }
            auto statement =
                m_impl->prepare("DELETE FROM items WHERE iid=? AND character_id=? AND item_index=? "
                                "AND count=? AND slot=0");
            if (!statement) {
                return rollback();
            }
            bind_int64(statement, 1, item.iid);
            bind_int64(statement, 2, owner);
            bind_int64(statement, 3, item.index);
            bind_int64(statement, 4, item.count);
            const bool removed = step(statement) == kDone && m_impl->changes == 1;
            statement.reset();
            if (!removed) {
                return rollback();
            }
        }
        for (const auto& item : replacements) {
            if (item.iid <= 0 || item.character_id != owner || item.slot != 0 || item.index <= 0 ||
                item.count <= 0) {
                return rollback();
            }
            // insert_item supports upserts; never overwrite an unrelated existing instance.
            auto statement = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
            if (!statement) {
                return rollback();
            }
            bind_int64(statement, 1, item.iid);
            const int result = step(statement);
            statement.reset();
            if (result != kDone || !insert_item(item)) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool PostgresStore::commit_trade(const CharacterRecord& first,
                                     const CharacterRecord& second,
                                     const std::vector<TradeTransfer>& items) {
        if (!m_impl || !m_impl->db || first.id <= 0 || second.id <= 0 || first.id == second.id ||
            first.money < 0 || second.money < 0 || !m_impl->exec("BEGIN")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        if (!save_character(first) || !save_character(second)) {
            return rollback();
        }
        std::vector<int> seen;
        for (const auto& transfer : items) {
            const auto& item = transfer.source;
            const bool partial = transfer.count < item.count;
            if (item.iid <= 0 || item.count <= 0 || transfer.count <= 0 ||
                transfer.count > item.count || item.slot || item.account_bound ||
                item.character_bound ||
                (item.character_id != first.id && item.character_id != second.id) ||
                std::find(seen.begin(), seen.end(), item.iid) != seen.end() ||
                (partial ? transfer.split_iid <= 0 : transfer.split_iid != 0)) {
                return rollback();
            }
            seen.push_back(item.iid);
            if (partial) {
                if (std::find(seen.begin(), seen.end(), transfer.split_iid) != seen.end()) {
                    return rollback();
                }
                seen.push_back(transfer.split_iid);
                auto statement = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
                if (!statement) {
                    return rollback();
                }
                bind_int64(statement, 1, transfer.split_iid);
                const bool unused = step(statement) == kDone;
                statement.reset();
                if (!unused) {
                    return rollback();
                }
            }
            const int recipient = item.character_id == first.id ? second.id : first.id;
            auto statement = m_impl->prepare(
                "UPDATE items SET account_id=(SELECT account_id FROM characters WHERE id=?), "
                "character_id=?, count=? WHERE iid=? AND character_id=? "
                "AND item_index=? AND count=? AND slot=0 AND "
                "account_bound=0 AND character_bound=0");
            if (!statement) {
                return rollback();
            }
            bind_int64(statement, 1, partial ? item.character_id : recipient);
            bind_int64(statement, 2, partial ? item.character_id : recipient);
            bind_int64(statement, 3, partial ? item.count - transfer.count : transfer.count);
            bind_int64(statement, 4, item.iid);
            bind_int64(statement, 5, item.character_id);
            bind_int64(statement, 6, item.index);
            bind_int64(statement, 7, item.count);
            const bool ok = step(statement) == kDone && m_impl->changes == 1;
            statement.reset();
            if (!ok) {
                return rollback();
            }
            if (partial) {
                auto received = item;
                received.iid = transfer.split_iid;
                received.character_id = recipient;
                received.count = transfer.count;
                if (!insert_item(received)) {
                    return rollback();
                }
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    std::optional<protocol::TradingPostState>
    PostgresStore::trading_post_list(int character, const protocol::TradingPostRequest& request) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        return trading_post::list(
            [&](const auto& sql, const auto& args) { return m_impl->market_query(sql, args); },
            character,
            request);
    }

    TradingPostResult PostgresStore::trading_post_sell(const ItemRecord& item,
                                                       int quantity,
                                                       std::int64_t price,
                                                       const TradingPostCategory& category,
                                                       std::int32_t split_iid) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN")) {
            return {};
        }
        struct Transaction {
            Impl& impl;
            bool committed = false;
            ~Transaction() {
                if (!committed) {
                    (void) impl.exec("ROLLBACK");
                }
            }
        } transaction{*m_impl};
        if (!m_impl->exec("UPDATE trading_post_lock SET id=id WHERE id=1")) {
            return {};
        }
        auto result = trading_post::sell(
            [&](const auto& sql, const auto& params) { return m_impl->market_query(sql, params); },
            item,
            quantity,
            price,
            category,
            split_iid);
        if (!result.committed) {
            return result;
        }
        if (!m_impl->exec("COMMIT")) {
            return {};
        }
        transaction.committed = true;
        return result;
    }

    TradingPostResult PostgresStore::trading_post_buy(const CharacterRecord& buyer,
                                                      std::uint64_t listing,
                                                      int quantity,
                                                      std::int64_t price,
                                                      std::int32_t split_iid) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN")) {
            return {};
        }
        struct Transaction {
            Impl& impl;
            bool committed = false;
            ~Transaction() {
                if (!committed) {
                    (void) impl.exec("ROLLBACK");
                }
            }
        } transaction{*m_impl};
        if (!m_impl->exec("UPDATE trading_post_lock SET id=id WHERE id=1")) {
            return {};
        }
        auto result = trading_post::buy(
            [&](const auto& sql, const auto& params) { return m_impl->market_query(sql, params); },
            buyer,
            listing,
            quantity,
            price,
            split_iid);
        if (!result.committed) {
            return result;
        }
        if (!m_impl->exec("COMMIT")) {
            return {};
        }
        transaction.committed = true;
        return result;
    }

    TradingPostResult PostgresStore::trading_post_cancel(int character, std::uint64_t listing) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN")) {
            return {};
        }
        struct Transaction {
            Impl& impl;
            bool committed = false;
            ~Transaction() {
                if (!committed) {
                    (void) impl.exec("ROLLBACK");
                }
            }
        } transaction{*m_impl};
        if (!m_impl->exec("UPDATE trading_post_lock SET id=id WHERE id=1")) {
            return {};
        }
        auto result = trading_post::cancel(
            [&](const auto& sql, const auto& params) { return m_impl->market_query(sql, params); },
            character,
            listing);
        if (!result.committed) {
            return result;
        }
        if (!m_impl->exec("COMMIT")) {
            return {};
        }
        transaction.committed = true;
        return result;
    }
    TradingPostCollectionResult PostgresStore::trading_post_collect(
        const CharacterRecord& character, std::uint64_t listing, const ItemRecord* merge_target) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN")) {
            return {};
        }
        struct Transaction {
            Impl& impl;
            bool committed = false;
            ~Transaction() {
                if (!committed) {
                    (void) impl.exec("ROLLBACK");
                }
            }
        } transaction{*m_impl};
        if (!m_impl->exec("UPDATE trading_post_lock SET id=id WHERE id=1")) {
            return {};
        }
        auto result = trading_post::collect(
            [&](const auto& sql, const auto& params) { return m_impl->market_query(sql, params); },
            character,
            listing,
            merge_target);
        if (!result.committed) {
            return result;
        }
        if (!m_impl->exec("COMMIT")) {
            return {};
        }
        transaction.committed = true;
        return result;
    }

    bool PostgresStore::delete_item(std::int32_t iid) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare("DELETE FROM items WHERE iid = ?");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, iid);
        const bool ok = step(stmt) == kDone;
        stmt.reset();
        return ok;
    }

    std::optional<ServerEventRecord> PostgresStore::load_server_event(const std::string& event_id) {
        if (!m_impl || !m_impl->db || event_id.empty()) {
            return std::nullopt;
        }
        StatementPtr stmt = m_impl->prepare(
            "SELECT event_id, contribution_count, active_bonus_percent, active_until "
            "FROM server_events WHERE event_id = ?");
        if (!stmt) {
            return std::nullopt;
        }
        bind_text(stmt, 1, event_id.c_str(), -1, 0);
        std::optional<ServerEventRecord> out;
        if (step(stmt) == kRow) {
            ServerEventRecord event;
            const unsigned char* id = column_text(stmt, 0);
            event.event_id = id ? reinterpret_cast<const char*>(id) : event_id;
            event.contribution_count = column_int64(stmt, 1);
            event.active_bonus_percent = column_int(stmt, 2);
            event.active_until = column_int64(stmt, 3);
            out = event;
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::save_server_event(const ServerEventRecord& event) {
        if (!m_impl || !m_impl->db || event.event_id.empty()) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO server_events(event_id, contribution_count, active_bonus_percent, "
            "active_until) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(event_id) DO UPDATE SET "
            "contribution_count=excluded.contribution_count, "
            "active_bonus_percent=excluded.active_bonus_percent, "
            "active_until=excluded.active_until");
        if (!stmt) {
            return false;
        }
        bind_text(stmt, 1, event.event_id.c_str(), -1, 0);
        bind_int64(stmt, 2, event.contribution_count);
        bind_int64(stmt, 3, event.active_bonus_percent);
        bind_int64(stmt, 4, event.active_until);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore save_server_event: ") +
                       "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    std::optional<BattleMountRecord> PostgresStore::load_battle_mount(std::int32_t mount_iid) {
        if (!m_impl || !m_impl->db || mount_iid <= 0) {
            return std::nullopt;
        }
        StatementPtr stmt =
            m_impl->prepare("SELECT mount_iid, grade, satiety, max_satiety, revision, "
                            "ability0, ability1, ability2, ability3, ability4, ability5, "
                            "equipment0, equipment1, equipment2, equipment3, equipment4 "
                            "FROM battle_mounts WHERE mount_iid = ?");
        if (!stmt) {
            return std::nullopt;
        }
        bind_int64(stmt, 1, mount_iid);
        std::optional<BattleMountRecord> result;
        if (step(stmt) == kRow) {
            BattleMountRecord mount;
            mount.mount_iid = column_int(stmt, 0);
            mount.grade =
                static_cast<protocol::BattleRidingGrade>(std::clamp(column_int(stmt, 1), 0, 3));
            mount.max_satiety = std::max(1, column_int(stmt, 3));
            mount.satiety = std::clamp(column_int(stmt, 2), 0, mount.max_satiety);
            mount.revision = static_cast<std::uint32_t>(std::max(0, column_int(stmt, 4)));
            for (std::size_t i = 0; i < mount.abilities.size(); ++i) {
                mount.abilities[i] = column_int(stmt, 5 + static_cast<int>(i));
            }
            for (std::size_t i = 0; i < mount.equipment_iids.size(); ++i) {
                mount.equipment_iids[i] = column_int(stmt, 11 + static_cast<int>(i));
            }
            result = mount;
        }
        stmt.reset();
        return result;
    }

    bool PostgresStore::upsert_battle_mount(const BattleMountRecord& mount) {
        if (!m_impl || !m_impl->db || mount.mount_iid <= 0 || mount.max_satiety <= 0 ||
            mount.satiety < 0 || mount.satiety > mount.max_satiety) {
            return false;
        }
        StatementPtr stmt =
            m_impl->prepare("INSERT INTO battle_mounts("
                            "mount_iid, grade, satiety, max_satiety, revision, "
                            "ability0, ability1, ability2, ability3, ability4, ability5, "
                            "equipment0, equipment1, equipment2, equipment3, equipment4) "
                            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                            "ON CONFLICT(mount_iid) DO UPDATE SET "
                            "grade=excluded.grade, satiety=excluded.satiety, "
                            "max_satiety=excluded.max_satiety, revision=excluded.revision, "
                            "ability0=excluded.ability0, ability1=excluded.ability1, "
                            "ability2=excluded.ability2, ability3=excluded.ability3, "
                            "ability4=excluded.ability4, ability5=excluded.ability5, "
                            "equipment0=excluded.equipment0, equipment1=excluded.equipment1, "
                            "equipment2=excluded.equipment2, equipment3=excluded.equipment3, "
                            "equipment4=excluded.equipment4");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, mount.mount_iid);
        bind_int64(stmt, 2, static_cast<int>(mount.grade));
        bind_int64(stmt, 3, mount.satiety);
        bind_int64(stmt, 4, mount.max_satiety);
        bind_int64(stmt, 5, static_cast<std::int64_t>(mount.revision));
        for (std::size_t i = 0; i < mount.abilities.size(); ++i) {
            bind_int64(stmt, 6 + static_cast<int>(i), mount.abilities[i]);
        }
        for (std::size_t i = 0; i < mount.equipment_iids.size(); ++i) {
            bind_int64(stmt, 12 + static_cast<int>(i), mount.equipment_iids[i]);
        }
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore upsert_battle_mount: ") +
                       "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    bool PostgresStore::delete_battle_mount(std::int32_t mount_iid) {
        if (!m_impl || !m_impl->db || mount_iid <= 0) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare("DELETE FROM battle_mounts WHERE mount_iid = ?");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, mount_iid);
        const bool ok = step(stmt) == kDone;
        stmt.reset();
        return ok;
    }

    std::uint32_t PostgresStore::max_item_iid() {
        if (!m_impl || !m_impl->db) {
            return 0;
        }
        StatementPtr stmt = m_impl->prepare("SELECT COALESCE(MAX(iid), 0) FROM items");
        if (!stmt) {
            return 0;
        }
        std::uint32_t max_iid = 0;
        if (step(stmt) == kRow) {
            const std::int64_t value = column_int64(stmt, 0);
            if (value > 0) {
                max_iid = static_cast<std::uint32_t>(value);
            }
        }
        stmt.reset();
        return max_iid;
    }

    std::vector<SkillRecord> PostgresStore::load_skills(std::int32_t character_id) {
        std::vector<SkillRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        StatementPtr stmt = m_impl->prepare(
            "SELECT character_id, skill_index, level FROM skills WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            SkillRecord s;
            s.character_id = column_int(stmt, 0);
            s.skill_index = column_int(stmt, 1);
            s.level = column_int(stmt, 2);
            out.push_back(s);
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::upsert_skill(const SkillRecord& skill) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (skill.level <= 0) {
            StatementPtr stmt =
                m_impl->prepare("DELETE FROM skills WHERE character_id = ? AND skill_index = ?");
            if (!stmt) {
                return false;
            }
            bind_int64(stmt, 1, skill.character_id);
            bind_int64(stmt, 2, skill.skill_index);
            const bool ok = step(stmt) == kDone;
            if (!ok) {
                log::error(std::string("PostgresStore upsert_skill delete: ") +
                           "database statement failed");
            }
            stmt.reset();
            return ok;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO skills(character_id, skill_index, level) VALUES(?, ?, ?) "
            "ON CONFLICT(character_id, skill_index) DO UPDATE SET level=excluded.level");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, skill.character_id);
        bind_int64(stmt, 2, skill.skill_index);
        bind_int64(stmt, 3, skill.level);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore upsert_skill: ") + "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    std::optional<ShortcutRecord> PostgresStore::load_shortcuts(std::int32_t character_id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        StatementPtr stmt = m_impl->prepare(
            "SELECT character_id, active_index, slot0, slot1, slot2, slot3, slot4, "
            "slot5, slot6, slot7, slot8, slot9, slot10, slot11, slot12, slot13, slot14, "
            "slot15, slot16, slot17, slot18, slot19 FROM shortcuts WHERE character_id = ?");
        if (!stmt) {
            return std::nullopt;
        }
        bind_int64(stmt, 1, character_id);
        std::optional<ShortcutRecord> out;
        if (step(stmt) == kRow) {
            ShortcutRecord rec;
            rec.character_id = column_int(stmt, 0);
            rec.active_index = column_int(stmt, 1);
            for (std::size_t i = 0; i < kShortcutSlots; ++i) {
                rec.slots[i] = column_int(stmt, static_cast<int>(2 + i));
            }
            out = rec;
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::upsert_shortcuts(const ShortcutRecord& shortcuts) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO shortcuts(character_id, active_index, slot0, slot1, slot2, slot3, "
            "slot4, slot5, slot6, slot7, slot8, slot9, slot10, slot11, slot12, slot13, slot14, "
            "slot15, slot16, slot17, slot18, slot19) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id) DO UPDATE SET "
            "active_index=excluded.active_index, slot0=excluded.slot0, slot1=excluded.slot1, "
            "slot2=excluded.slot2, slot3=excluded.slot3, slot4=excluded.slot4, "
            "slot5=excluded.slot5, slot6=excluded.slot6, slot7=excluded.slot7, "
            "slot8=excluded.slot8, slot9=excluded.slot9, slot10=excluded.slot10, "
            "slot11=excluded.slot11, slot12=excluded.slot12, slot13=excluded.slot13, "
            "slot14=excluded.slot14, slot15=excluded.slot15, slot16=excluded.slot16, "
            "slot17=excluded.slot17, slot18=excluded.slot18, slot19=excluded.slot19");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, shortcuts.character_id);
        bind_int64(stmt, 2, shortcuts.active_index);
        for (std::size_t i = 0; i < kShortcutSlots; ++i) {
            bind_int64(stmt, static_cast<int>(3 + i), shortcuts.slots[i]);
        }
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore upsert_shortcuts: ") +
                       "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    std::vector<QuestFlagRecord> PostgresStore::load_quest_flags(std::int32_t character_id) {
        std::vector<QuestFlagRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        StatementPtr stmt =
            m_impl->prepare("SELECT character_id, quest, flag, clear_flag FROM quest_flags "
                            "WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            QuestFlagRecord q;
            q.character_id = column_int(stmt, 0);
            q.quest = static_cast<std::uint16_t>(column_int(stmt, 1));
            q.flag = static_cast<std::uint8_t>(column_int(stmt, 2));
            q.clear = static_cast<std::uint8_t>(column_int(stmt, 3));
            out.push_back(q);
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::upsert_quest_flag(const QuestFlagRecord& flag) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO quest_flags(character_id, quest, flag, clear_flag) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(character_id, quest) DO UPDATE SET flag=excluded.flag, "
            "clear_flag=excluded.clear_flag");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, flag.character_id);
        bind_int64(stmt, 2, static_cast<int>(flag.quest));
        bind_int64(stmt, 3, static_cast<int>(flag.flag));
        bind_int64(stmt, 4, static_cast<int>(flag.clear));
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore upsert_quest_flag: ") +
                       "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    bool PostgresStore::update_system_mail(std::int32_t character_id,
                                           std::int64_t mail_id,
                                           bool remove,
                                           std::int64_t now) {
        if (!m_impl || !m_impl->db || character_id <= 0 || mail_id <= 0 || now <= 0) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            remove ? "UPDATE system_mail SET deleted_at = ? WHERE id = ? AND character_id = ? AND "
                     "deleted_at = 0 AND (claimed_at != 0 OR item_index = 0)"
                   : "UPDATE system_mail SET read_at = CASE WHEN read_at = 0 THEN ? ELSE read_at "
                     "END WHERE id = ? AND character_id = ? AND deleted_at = 0");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, now);
        bind_int64(stmt, 2, mail_id);
        bind_int64(stmt, 3, character_id);
        const bool ok = step(stmt) == kDone && m_impl->changes == 1;
        stmt.reset();
        return ok;
    }

    bool PostgresStore::ensure_system_mail(const SystemMailRecord& mail) {
        if (!m_impl || !m_impl->db || mail.character_id <= 0 || mail.source_key.empty() ||
            mail.item_index < 0 || mail.item_count < 0 ||
            ((mail.item_index == 0) != (mail.item_count == 0))) {
            return false;
        }
        StatementPtr stmt = m_impl->prepare(
            "INSERT INTO system_mail(character_id, source_key, subject, body, item_index, "
            "item_count, created_at) VALUES(?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, source_key) DO NOTHING");
        if (!stmt) {
            return false;
        }
        bind_int64(stmt, 1, mail.character_id);
        bind_text(stmt, 2, mail.source_key.c_str(), -1, 0);
        bind_text(stmt, 3, mail.subject.c_str(), -1, 0);
        bind_text(stmt, 4, mail.body.c_str(), -1, 0);
        bind_int64(stmt, 5, mail.item_index);
        bind_int64(stmt, 6, mail.item_count);
        bind_int64(stmt, 7, mail.created_at);
        const bool ok = step(stmt) == kDone;
        if (!ok) {
            log::error(std::string("PostgresStore ensure_system_mail: ") +
                       "database statement failed");
        }
        stmt.reset();
        return ok;
    }

    std::vector<SystemMailRecord> PostgresStore::list_system_mail(std::int32_t character_id) {
        std::vector<SystemMailRecord> out;
        if (!m_impl || !m_impl->db || character_id <= 0) {
            return out;
        }
        StatementPtr stmt = m_impl->prepare(
            "SELECT id, character_id, source_key, subject, body, item_index, item_count, "
            "created_at, claimed_at, read_at FROM system_mail WHERE character_id = ? AND "
            "deleted_at = 0 "
            "ORDER BY created_at DESC, id DESC");
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            SystemMailRecord mail;
            mail.id = column_int64(stmt, 0);
            mail.character_id = column_int(stmt, 1);
            const auto* source_key = column_text(stmt, 2);
            const auto* subject = column_text(stmt, 3);
            const auto* body = column_text(stmt, 4);
            mail.source_key = source_key ? reinterpret_cast<const char*>(source_key) : "";
            mail.subject = subject ? reinterpret_cast<const char*>(subject) : "";
            mail.body = body ? reinterpret_cast<const char*>(body) : "";
            mail.item_index = column_int(stmt, 5);
            mail.item_count = column_int(stmt, 6);
            mail.created_at = column_int64(stmt, 7);
            mail.claimed_at = column_int64(stmt, 8);
            mail.read_at = column_int64(stmt, 9);
            out.push_back(std::move(mail));
        }
        stmt.reset();
        return out;
    }

    SystemMailClaimResult PostgresStore::claim_system_mail(std::int32_t character_id,
                                                           std::int64_t mail_id,
                                                           const ItemRecord& item,
                                                           std::int64_t claimed_at) {
        if (!m_impl || !m_impl->db || character_id <= 0 || mail_id <= 0 || item.iid <= 0 ||
            claimed_at <= 0) {
            return SystemMailClaimResult::Error;
        }
        if (!m_impl->exec("BEGIN")) {
            return SystemMailClaimResult::Error;
        }
        auto rollback = [&] { (void) m_impl->exec("ROLLBACK"); };

        StatementPtr select =
            m_impl->prepare("SELECT item_index, item_count, claimed_at FROM system_mail "
                            "WHERE id = ? AND character_id = ? AND deleted_at = 0 FOR UPDATE");
        if (!select) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        bind_int64(select, 1, mail_id);
        bind_int64(select, 2, character_id);
        const int select_rc = step(select);
        if (select_rc != kRow) {
            select.reset();
            rollback();
            return select_rc == kDone ? SystemMailClaimResult::NotFound
                                      : SystemMailClaimResult::Error;
        }
        const int item_index = column_int(select, 0);
        const int item_count = column_int(select, 1);
        const std::int64_t existing_claimed_at = column_int64(select, 2);
        select.reset();
        if (existing_claimed_at != 0 || item_index <= 0 || item_count <= 0) {
            rollback();
            return SystemMailClaimResult::AlreadyClaimed;
        }

        ItemRecord claimed_item = item;
        claimed_item.character_id = character_id;
        claimed_item.account_id = 0;
        claimed_item.index = item_index;
        claimed_item.count = item_count;
        claimed_item.slot = 0;
        if (!insert_item(claimed_item)) {
            rollback();
            return SystemMailClaimResult::Error;
        }

        StatementPtr update =
            m_impl->prepare("UPDATE system_mail SET claimed_at = ? "
                            "WHERE id = ? AND character_id = ? AND claimed_at = 0");
        if (!update) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        bind_int64(update, 1, claimed_at);
        bind_int64(update, 2, mail_id);
        bind_int64(update, 3, character_id);
        const bool updated = step(update) == kDone && m_impl->changes == 1;
        update.reset();
        if (!updated || !m_impl->exec("COMMIT")) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        return SystemMailClaimResult::Claimed;
    }

    std::vector<BuffRemainRecord> PostgresStore::load_buff_remains(std::int32_t character_id) {
        std::vector<BuffRemainRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        StatementPtr stmt = m_impl->prepare(
            "SELECT character_id, buff_type, remain, is_count, stack_pack, source_item_index "
            "FROM buff_remain "
            "WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            BuffRemainRecord row;
            row.character_id = column_int(stmt, 0);
            row.buff_type = column_int(stmt, 1);
            row.remain = column_int(stmt, 2);
            row.is_count = column_int(stmt, 3);
            row.stack_pack = column_int(stmt, 4);
            row.source_item_index = column_int(stmt, 5);
            out.push_back(row);
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::replace_buff_remains(std::int32_t character_id,
                                             const std::vector<BuffRemainRecord>& rows) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        StatementPtr del = m_impl->prepare("DELETE FROM buff_remain WHERE character_id = ?");
        if (!del) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        bind_int64(del, 1, character_id);
        const bool deleted = step(del) == kDone;
        del.reset();
        if (!deleted) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        for (const auto& row : rows) {
            StatementPtr ins = m_impl->prepare(
                "INSERT INTO buff_remain(character_id, buff_type, remain, is_count, stack_pack, "
                "source_item_index) VALUES(?, ?, ?, ?, ?, ?)");
            if (!ins) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
            bind_int64(ins, 1, character_id);
            bind_int64(ins, 2, row.buff_type);
            bind_int64(ins, 3, row.remain);
            bind_int64(ins, 4, row.is_count);
            bind_int64(ins, 5, row.stack_pack);
            bind_int64(ins, 6, row.source_item_index);
            const bool ok = step(ins) == kDone;
            ins.reset();
            if (!ok) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
        }
        return m_impl->exec("COMMIT");
    }

    std::vector<InventoryUnlockRecord>
    PostgresStore::load_inventory_unlocks(std::int32_t character_id) {
        std::vector<InventoryUnlockRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        StatementPtr stmt =
            m_impl->prepare("SELECT character_id, register_index, remain FROM inventory_unlock "
                            "WHERE character_id = ? ORDER BY register_index ASC");
        if (!stmt) {
            return out;
        }
        bind_int64(stmt, 1, character_id);
        while (step(stmt) == kRow) {
            InventoryUnlockRecord row;
            row.character_id = column_int(stmt, 0);
            row.register_index = column_int(stmt, 1);
            row.remain = column_int(stmt, 2);
            out.push_back(row);
        }
        stmt.reset();
        return out;
    }

    bool PostgresStore::replace_inventory_unlocks(std::int32_t character_id,
                                                  const std::vector<InventoryUnlockRecord>& rows) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        StatementPtr del = m_impl->prepare("DELETE FROM inventory_unlock WHERE character_id = ?");
        if (!del) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        bind_int64(del, 1, character_id);
        const bool deleted = step(del) == kDone;
        del.reset();
        if (!deleted) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        for (const auto& row : rows) {
            StatementPtr ins = m_impl->prepare(
                "INSERT INTO inventory_unlock(character_id, register_index, remain) "
                "VALUES(?, ?, ?)");
            if (!ins) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
            bind_int64(ins, 1, character_id);
            bind_int64(ins, 2, row.register_index);
            bind_int64(ins, 3, row.remain);
            const bool ok = step(ins) == kDone;
            ins.reset();
            if (!ok) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
        }
        return m_impl->exec("COMMIT");
    }

    bool PostgresStore::ensure_default_account(const std::string& account_name,
                                               const std::string& password) {
        auto acc = find_account(account_name);
        if (acc) {
            return true;
        }
        AccountRecord a;
        a.name = account_name;
        a.password = password;
        return upsert_account(a);
    }

    bool PostgresStore::ensure_default_player(const std::string& account_name,
                                              const std::string& password,
                                              const CharacterRecord& defaults) {
        if (!ensure_default_account(account_name, password)) {
            return false;
        }
        auto acc = find_account(account_name);
        if (!acc) {
            return false;
        }
        auto chars = list_characters(acc->id);
        if (!chars.empty()) {
            return true;
        }
        CharacterRecord c = defaults;
        c.id = 0;
        c.account_id = acc->id;
        if (c.name.empty()) {
            c.name = "Hero";
        }
        return save_character(c);
    }

}  // namespace openkal::server
