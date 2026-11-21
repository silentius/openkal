/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "AccountDisciplineSql.h"
#include "TradingPostStore.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/server/store/DataStore.h"
#if defined(OPENKAL_WITH_POSTGRES)
#include "openkal/server/store/PostgresStore.h"
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace openkal::server {
    namespace {

        [[nodiscard]] bool looks_like_sqlite_file(const std::string& path) {
            if (path == ":memory:") {
                return true;
            }
            auto ends_with = [&](const char* ext) {
                const std::size_t n = std::strlen(ext);
                return path.size() >= n && path.compare(path.size() - n, n, ext) == 0;
            };
            return ends_with(".sqlite") || ends_with(".db") || ends_with(".sqlite3");
        }

        [[nodiscard]] CharacterRecord character_from_stmt(sqlite3_stmt* stmt) {
            CharacterRecord c;
            c.id = sqlite3_column_int(stmt, 0);
            c.account_id = sqlite3_column_int(stmt, 1);
            const unsigned char* name = sqlite3_column_text(stmt, 2);
            c.name = name ? reinterpret_cast<const char*>(name) : "";
            c.role = static_cast<UserRole>(sqlite3_column_int(stmt, 3));
            c.clazz = sqlite3_column_int(stmt, 4);
            c.map = sqlite3_column_int(stmt, 5);
            c.map_x = sqlite3_column_int(stmt, 6);
            c.map_y = sqlite3_column_int(stmt, 7);
            c.map_z = sqlite3_column_int(stmt, 8);
            c.revival_point = sqlite3_column_int(stmt, 9);
            c.level = sqlite3_column_int(stmt, 10);
            c.exp = sqlite3_column_int64(stmt, 11);
            c.hp = sqlite3_column_int(stmt, 12);
            c.max_hp = sqlite3_column_int(stmt, 13);
            c.mp = sqlite3_column_int(stmt, 14);
            c.max_mp = sqlite3_column_int(stmt, 15);
            c.pu_point = sqlite3_column_int(stmt, 16);
            c.str = sqlite3_column_int(stmt, 17);
            c.hth = sqlite3_column_int(stmt, 18);
            c.intl = sqlite3_column_int(stmt, 19);
            c.wis = sqlite3_column_int(stmt, 20);
            c.dex = sqlite3_column_int(stmt, 21);
            c.money = sqlite3_column_int64(stmt, 22);
            c.next_iid = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 23));
            c.weapon_index = sqlite3_column_int(stmt, 24);
            c.armor_index = sqlite3_column_int(stmt, 25);
            c.transform_index = sqlite3_column_int(stmt, 26);
            c.transform_level = sqlite3_column_int(stmt, 27);
            c.transform_exp = sqlite3_column_int(stmt, 28);
            c.transform_put_on_ticks = sqlite3_column_int(stmt, 29);
            c.specialty = sqlite3_column_int(stmt, 30);
            if (c.specialty <= 0) {
                c.specialty = 1;
            }
            c.su_point = sqlite3_column_int(stmt, 31);
            c.rage = sqlite3_column_int(stmt, 32);
            c.contribution = sqlite3_column_int(stmt, 33);
            c.face = sqlite3_column_int(stmt, 34);
            c.hair = sqlite3_column_int(stmt, 35);
            c.deletion_state = static_cast<CharacterDeletionState>(sqlite3_column_int(stmt, 36));
            c.delete_after = sqlite3_column_int64(stmt, 37);
            c.talent_mask = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 38));
            c.beacon_ready_at = sqlite3_column_int64(stmt, 39);
            c.pvp_death_exp_percent = sqlite3_column_int(stmt, 40);
            return c;
        }

        [[nodiscard]] ItemRecord item_from_stmt(sqlite3_stmt* stmt) {
            ItemRecord item;
            item.iid = sqlite3_column_int(stmt, 0);
            item.character_id = sqlite3_column_int(stmt, 1);
            item.account_id = sqlite3_column_int(stmt, 2);
            item.index = sqlite3_column_int(stmt, 3);
            item.prefix = sqlite3_column_int(stmt, 4);
            item.count = sqlite3_column_int(stmt, 5);
            item.slot = sqlite3_column_int(stmt, 6);
            item.cur_end = sqlite3_column_int(stmt, 7);
            item.max_end = sqlite3_column_int(stmt, 8);
            item.x_attack = sqlite3_column_int(stmt, 9);
            item.x_magic = sqlite3_column_int(stmt, 10);
            item.x_hit = sqlite3_column_int(stmt, 11);
            item.x_defense = sqlite3_column_int(stmt, 12);
            item.x_dodge = sqlite3_column_int(stmt, 13);
            item.upgrade_level = sqlite3_column_int(stmt, 14);
            item.upgrade_rate = sqlite3_column_int(stmt, 15);
            item.mix_type = sqlite3_column_int(stmt, 16);
            item.bof = sqlite3_column_int(stmt, 17);
            item.qigong_perforated = sqlite3_column_int(stmt, 18);
            item.qigong_effect = sqlite3_column_int(stmt, 19);
            item.dragon_effect = sqlite3_column_int(stmt, 20);
            item.random_stat_type = sqlite3_column_int(stmt, 21);
            item.random_stat_value = sqlite3_column_int(stmt, 22);
            item.transform_level = sqlite3_column_int(stmt, 23);
            item.transform_exp = sqlite3_column_int(stmt, 24);
            item.pet_expire_at = sqlite3_column_int64(stmt, 25);
            item.ride_expire_at = sqlite3_column_int64(stmt, 26);
            item.account_bound = sqlite3_column_int(stmt, 27) != 0;
            item.enchant_protected = sqlite3_column_int(stmt, 28) != 0;
            item.character_bound = sqlite3_column_int(stmt, 29) != 0;
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
            "beacon_ready_at, pvp_death_exp_percent "
            "FROM characters";

        constexpr const char* kSchemaSql = R"SQL(
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS schema_version (
  version INTEGER PRIMARY KEY
);
CREATE TABLE IF NOT EXISTS accounts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE,
  password TEXT NOT NULL,
  protocol_points INTEGER NOT NULL DEFAULT 0 CHECK(protocol_points BETWEEN 0 AND 9),
  protocol_kicks INTEGER NOT NULL DEFAULT 0 CHECK(protocol_kicks BETWEEN 0 AND 3),
  banned INTEGER NOT NULL DEFAULT 0 CHECK(banned IN (0,1))
);
CREATE TABLE IF NOT EXISTS account_storage_money (
  account_id INTEGER PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,
  balance INTEGER NOT NULL CHECK(balance >= 0)
);
CREATE TABLE IF NOT EXISTS characters (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  account_id INTEGER NOT NULL,
  name TEXT NOT NULL,
  role INTEGER NOT NULL DEFAULT 0 CHECK(role BETWEEN 0 AND 3),
  clazz INTEGER NOT NULL DEFAULT 0,
  map INTEGER NOT NULL DEFAULT 0,
  x INTEGER NOT NULL DEFAULT 257285,
  y INTEGER NOT NULL DEFAULT 258744,
  z INTEGER NOT NULL DEFAULT 16100,
  revival_point INTEGER NOT NULL DEFAULT 0,
  pvp_death_exp_percent INTEGER NOT NULL DEFAULT 0,
  level INTEGER NOT NULL DEFAULT 1,
  exp INTEGER NOT NULL DEFAULT 0,
  hp INTEGER NOT NULL DEFAULT 100,
  max_hp INTEGER NOT NULL DEFAULT 100,
  mp INTEGER NOT NULL DEFAULT 50,
  max_mp INTEGER NOT NULL DEFAULT 50,
  pu_point INTEGER NOT NULL DEFAULT 0,
  su_point INTEGER NOT NULL DEFAULT 0,
  contribution INTEGER NOT NULL DEFAULT 0,
  rage INTEGER NOT NULL DEFAULT 0,
  str INTEGER NOT NULL DEFAULT 0,
  hth INTEGER NOT NULL DEFAULT 0,
  intl INTEGER NOT NULL DEFAULT 0,
  wis INTEGER NOT NULL DEFAULT 0,
  dex INTEGER NOT NULL DEFAULT 0,
  money INTEGER NOT NULL DEFAULT 0,
  next_iid INTEGER NOT NULL DEFAULT 1,
  weapon_index INTEGER NOT NULL DEFAULT 0,
  armor_index INTEGER NOT NULL DEFAULT 0,
  transform_index INTEGER NOT NULL DEFAULT 0,
  transform_level INTEGER NOT NULL DEFAULT 1,
  transform_exp INTEGER NOT NULL DEFAULT 0,
  transform_put_on_ticks INTEGER NOT NULL DEFAULT 0,
  specialty INTEGER NOT NULL DEFAULT 1,
  face INTEGER NOT NULL DEFAULT 1,
  hair INTEGER NOT NULL DEFAULT 1,
  deletion_state INTEGER NOT NULL DEFAULT 0,
  delete_after INTEGER NOT NULL DEFAULT 0,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS items (
  iid INTEGER PRIMARY KEY,
  character_id INTEGER,
  account_id INTEGER,
  item_index INTEGER NOT NULL,
  prefix INTEGER NOT NULL DEFAULT 0,
  count INTEGER NOT NULL DEFAULT 1,
  slot INTEGER NOT NULL DEFAULT 0,
  cur_end INTEGER NOT NULL DEFAULT 0,
  max_end INTEGER NOT NULL DEFAULT 0,
  x_attack INTEGER NOT NULL DEFAULT 0,
  x_magic INTEGER NOT NULL DEFAULT 0,
  x_hit INTEGER NOT NULL DEFAULT 0,
  x_defense INTEGER NOT NULL DEFAULT 0,
  x_dodge INTEGER NOT NULL DEFAULT 0,
  upgrade_level INTEGER NOT NULL DEFAULT 0,
  upgrade_rate INTEGER NOT NULL DEFAULT 0,
  mix_type INTEGER NOT NULL DEFAULT 0,
  bof INTEGER NOT NULL DEFAULT 0,
  qigong_perforated INTEGER NOT NULL DEFAULT 0,
  qigong_effect INTEGER NOT NULL DEFAULT 0,
  dragon_effect INTEGER NOT NULL DEFAULT 0,
  random_stat_type INTEGER NOT NULL DEFAULT 0,
  random_stat_value INTEGER NOT NULL DEFAULT 0,
  transform_level INTEGER NOT NULL DEFAULT 1,
  transform_exp INTEGER NOT NULL DEFAULT 0,
  pet_expire_at INTEGER NOT NULL DEFAULT 0,
  ride_expire_at INTEGER NOT NULL DEFAULT 0,
  account_bound INTEGER NOT NULL DEFAULT 0 CHECK(account_bound IN (0, 1)),
  CHECK ((character_id IS NOT NULL AND account_id IS NULL) OR
         (character_id IS NULL AND account_id IS NOT NULL)),
  CHECK (account_id IS NULL OR slot BETWEEN 0 AND 215),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS trading_post_lock(id BIGINT PRIMARY KEY CHECK(id=1));
INSERT INTO trading_post_lock VALUES(1) ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS trading_post_offers(id INTEGER PRIMARY KEY AUTOINCREMENT, seller BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>=0),unit_price BIGINT NOT NULL CHECK(unit_price>0),attributes TEXT NOT NULL,clazz BIGINT NOT NULL,type BIGINT NOT NULL,grade BIGINT NOT NULL,prefix BIGINT NOT NULL,enhancement BIGINT NOT NULL,mount TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS trading_post_search ON trading_post_offers(type,grade,unit_price,id);
CREATE INDEX IF NOT EXISTS trading_post_seller ON trading_post_offers(seller,id);
CREATE TABLE IF NOT EXISTS trading_post_deliveries(id INTEGER PRIMARY KEY AUTOINCREMENT,character_id BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,item_index BIGINT NOT NULL,count BIGINT NOT NULL CHECK(count>0),unit_price BIGINT NOT NULL CHECK(unit_price>0),kind BIGINT NOT NULL,attributes TEXT NOT NULL,mount TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS trading_post_collection ON trading_post_deliveries(character_id,id);

CREATE TABLE IF NOT EXISTS skills (
  character_id INTEGER NOT NULL,
  skill_index INTEGER NOT NULL,
  level INTEGER NOT NULL DEFAULT 1,
  PRIMARY KEY (character_id, skill_index),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS shortcuts (
  character_id INTEGER PRIMARY KEY,
  active_index INTEGER NOT NULL DEFAULT -1,
  slot0 INTEGER NOT NULL DEFAULT -1,
  slot1 INTEGER NOT NULL DEFAULT -1,
  slot2 INTEGER NOT NULL DEFAULT -1,
  slot3 INTEGER NOT NULL DEFAULT -1,
  slot4 INTEGER NOT NULL DEFAULT -1,
  slot5 INTEGER NOT NULL DEFAULT -1,
  slot6 INTEGER NOT NULL DEFAULT -1,
  slot7 INTEGER NOT NULL DEFAULT -1,
  slot8 INTEGER NOT NULL DEFAULT -1,
  slot9 INTEGER NOT NULL DEFAULT -1,
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS quest_flags (
  character_id INTEGER NOT NULL,
  quest INTEGER NOT NULL,
  flag INTEGER NOT NULL DEFAULT 0,
  clear_flag INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, quest),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS buff_remain (
  character_id INTEGER NOT NULL,
  buff_type INTEGER NOT NULL,
  remain INTEGER NOT NULL DEFAULT 0,
  is_count INTEGER NOT NULL DEFAULT 0,
  stack_pack INTEGER NOT NULL DEFAULT 0,
  source_item_index INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, buff_type),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS inventory_unlock (
  character_id INTEGER NOT NULL,
  register_index INTEGER NOT NULL,
  remain INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, register_index),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS server_events (
  event_id TEXT PRIMARY KEY,
  contribution_count INTEGER NOT NULL DEFAULT 0,
  active_bonus_percent INTEGER NOT NULL DEFAULT 0,
  active_until INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS assassin_list (
  id INTEGER PRIMARY KEY,
  assassin TEXT NOT NULL,
  last_hitter TEXT NOT NULL
);
)SQL";

        inline constexpr int kSchemaVersion = 23;

        /** Reads the highest applied schema_version row. */
        [[nodiscard]] int max_schema_version(sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(
                    db, "SELECT MAX(version) FROM schema_version", -1, &stmt, nullptr) !=
                SQLITE_OK) {
                return 0;
            }
            int version = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                version = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
            return version;
        }

        /** Removes duplicate rows before unique indexes are applied. */
        [[nodiscard]] bool dedupe_unique_violations(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore dedupe: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            static constexpr const char* kStatements[] = {
                "DELETE FROM items WHERE rowid NOT IN (SELECT MIN(rowid) FROM items GROUP BY iid)",
                "DELETE FROM accounts WHERE rowid NOT IN (SELECT MIN(rowid) FROM accounts GROUP BY "
                "id)",
                "DELETE FROM characters WHERE rowid NOT IN "
                "(SELECT MIN(rowid) FROM characters GROUP BY account_id, name)",
            };
            for (const char* sql : kStatements) {
                if (!exec(sql)) {
                    return false;
                }
            }
            return true;
        }

        /** Applies schema version 2: explicit unique indexes and upsert safety. */
        [[nodiscard]] bool apply_schema_v2(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v2: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            if (!dedupe_unique_violations(db)) {
                return false;
            }
            static constexpr const char* kIndexes[] = {
                "CREATE UNIQUE INDEX IF NOT EXISTS ux_items_iid ON items(iid)",
                "CREATE UNIQUE INDEX IF NOT EXISTS ux_accounts_id ON accounts(id)",
                "CREATE UNIQUE INDEX IF NOT EXISTS ux_accounts_name ON accounts(name)",
                "CREATE UNIQUE INDEX IF NOT EXISTS ux_characters_account_name "
                "ON characters(account_id, name)",
            };
            for (const char* sql : kIndexes) {
                if (!exec(sql)) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(2)");
        }

        /** Applies schema version 3: pay-scroll buff remain table. */
        [[nodiscard]] bool apply_schema_v3(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v3: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            if (!exec("CREATE TABLE IF NOT EXISTS buff_remain ("
                      "character_id INTEGER NOT NULL,"
                      "buff_type INTEGER NOT NULL,"
                      "remain INTEGER NOT NULL DEFAULT 0,"
                      "is_count INTEGER NOT NULL DEFAULT 0,"
                      "PRIMARY KEY (character_id, buff_type),"
                      "FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE)")) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(3)");
        }

        /** Applies schema version 4: buff_remain.stack_pack for count-tier isolation. */
        [[nodiscard]] bool apply_schema_v4(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v4: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            // Existing DBs created at v3 lack stack_pack — add if missing.
            bool has_stack_pack = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(buff_remain)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "stack_pack") == 0) {
                        has_stack_pack = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_stack_pack) {
                if (!exec("ALTER TABLE buff_remain ADD COLUMN stack_pack INTEGER NOT NULL "
                          "DEFAULT 0")) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(4)");
        }

        /** Applies schema version 5: characters.specialty (runtime state). */
        [[nodiscard]] bool apply_schema_v5(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v5: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_specialty = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "specialty") == 0) {
                        has_specialty = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_specialty) {
                if (!exec("ALTER TABLE characters ADD COLUMN specialty INTEGER NOT NULL "
                          "DEFAULT 1")) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(5)");
        }

        /** Applies schema version 6: characters.su_point (runtime state). */
        [[nodiscard]] bool apply_schema_v6(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v6: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_su = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "su_point") == 0) {
                        has_su = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_su) {
                if (!exec("ALTER TABLE characters ADD COLUMN su_point INTEGER NOT NULL "
                          "DEFAULT 0")) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(6)");
        }

        /** Applies schema version 14: characters.contribution (runtime state). */
        [[nodiscard]] bool apply_schema_v14(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v14: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_contribution = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "contribution") == 0) {
                        has_contribution = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_contribution) {
                if (!exec("ALTER TABLE characters ADD COLUMN contribution INTEGER NOT NULL "
                          "DEFAULT 0")) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(14)");
        }

        /** Applies schema version 7: characters.rage (runtime state). */
        [[nodiscard]] bool apply_schema_v7(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v7: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_rage = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "rage") == 0) {
                        has_rage = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_rage) {
                if (!exec("ALTER TABLE characters ADD COLUMN rage INTEGER NOT NULL DEFAULT 0")) {
                    return false;
                }
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(7)");
        }

        /** Applies schema version 8: characters.revival_point. */
        [[nodiscard]] bool apply_schema_v8(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v8: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_revival_point = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "revival_point") == 0) {
                        has_revival_point = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_revival_point &&
                !exec(
                    "ALTER TABLE characters ADD COLUMN revival_point INTEGER NOT NULL DEFAULT 0")) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(8)");
        }

        /** Applies schema version 9: per-instance Qigong perforation and effect. */
        [[nodiscard]] bool apply_schema_v9(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v9: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_perforated = false;
            bool has_effect = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (!name) {
                        continue;
                    }
                    has_perforated = has_perforated || std::strcmp(name, "qigong_perforated") == 0;
                    has_effect = has_effect || std::strcmp(name, "qigong_effect") == 0;
                }
                sqlite3_finalize(info);
            }
            if ((!has_perforated && !exec("ALTER TABLE items ADD COLUMN qigong_perforated INTEGER "
                                          "NOT NULL DEFAULT 0")) ||
                (!has_effect &&
                 !exec("ALTER TABLE items ADD COLUMN qigong_effect INTEGER NOT NULL DEFAULT 0"))) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(9)");
        }

        /** Applies schema version 10: per-instance Dragon Fusing effect. */
        [[nodiscard]] bool apply_schema_v10(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v10: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_effect = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    has_effect = has_effect || (name && std::strcmp(name, "dragon_effect") == 0);
                }
                sqlite3_finalize(info);
            }
            if (!has_effect &&
                !exec("ALTER TABLE items ADD COLUMN dragon_effect INTEGER NOT NULL DEFAULT 0")) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(10)");
        }

        /** Applies schema version 11: timed inventory register unlocks. */
        [[nodiscard]] bool apply_schema_v11(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v11: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            if (!exec("CREATE TABLE IF NOT EXISTS inventory_unlock ("
                      "character_id INTEGER NOT NULL,"
                      "register_index INTEGER NOT NULL,"
                      "remain INTEGER NOT NULL DEFAULT 0,"
                      "PRIMARY KEY (character_id, register_index),"
                      "FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE)")) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(11)");
        }

        /** Applies schema version 12: source item for pay-buff HUD entries. */
        [[nodiscard]] bool apply_schema_v12(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v12: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_source_item_index = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(buff_remain)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (name && std::strcmp(name, "source_item_index") == 0) {
                        has_source_item_index = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_source_item_index &&
                !exec("ALTER TABLE buff_remain ADD COLUMN source_item_index INTEGER NOT NULL "
                      "DEFAULT 0")) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(12)");
        }

        /** Applies schema version 13: persistent random equipment modifiers. */
        [[nodiscard]] bool apply_schema_v13(sqlite3* db) {
            auto exec = [&](const char* sql) {
                char* err = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
                if (rc != SQLITE_OK) {
                    log::error(std::string("SqliteStore migrate v13: ") +
                               (err ? err : sqlite3_errmsg(db)));
                    sqlite3_free(err);
                    return false;
                }
                return true;
            };
            bool has_type = false;
            bool has_value = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) ==
                SQLITE_OK) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    if (!name) {
                        continue;
                    }
                    has_type = has_type || std::strcmp(name, "random_stat_type") == 0;
                    has_value = has_value || std::strcmp(name, "random_stat_value") == 0;
                }
                sqlite3_finalize(info);
            }
            if ((!has_type && !exec("ALTER TABLE items ADD COLUMN random_stat_type INTEGER NOT "
                                    "NULL DEFAULT 0")) ||
                (!has_value && !exec("ALTER TABLE items ADD COLUMN random_stat_value INTEGER NOT "
                                     "NULL DEFAULT 0"))) {
                return false;
            }
            return exec("INSERT OR IGNORE INTO schema_version(version) VALUES(13)");
        }

        /** Adds persistent natural appearance to existing character rows. */
        [[nodiscard]] bool apply_schema_v15(sqlite3* db) {
            bool has_face = false, has_hair = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                if (name) {
                    has_face = has_face || std::strcmp(name, "face") == 0;
                    has_hair = has_hair || std::strcmp(name, "hair") == 0;
                }
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v15: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_face ||
                    exec("ALTER TABLE characters ADD COLUMN face INTEGER NOT NULL DEFAULT 1")) &&
                   (has_hair ||
                    exec("ALTER TABLE characters ADD COLUMN hair INTEGER NOT NULL DEFAULT 1")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(15)");
        }

        /** Adds the recoverable character-deletion lifecycle. */
        [[nodiscard]] bool apply_schema_v16(sqlite3* db) {
            bool has_deletion_state = false;
            bool has_delete_after = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                if (!name) {
                    continue;
                }
                has_deletion_state = has_deletion_state || std::strcmp(name, "deletion_state") == 0;
                has_delete_after = has_delete_after || std::strcmp(name, "delete_after") == 0;
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v16: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_deletion_state ||
                    exec("ALTER TABLE characters ADD COLUMN deletion_state INTEGER NOT NULL "
                         "DEFAULT 0")) &&
                   (has_delete_after ||
                    exec("ALTER TABLE characters ADD COLUMN delete_after INTEGER NOT NULL "
                         "DEFAULT 0")) &&
                   exec("CREATE INDEX IF NOT EXISTS ix_characters_deletion_due "
                        "ON characters(deletion_state, delete_after)") &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(16)");
        }

        /** Adds mutually exclusive character/account item ownership for account storage. */
        [[nodiscard]] bool apply_schema_v17(sqlite3* db) {
            bool has_account_id = false;
            bool character_id_not_null = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                if (!name) {
                    continue;
                }
                if (std::strcmp(name, "account_id") == 0) {
                    has_account_id = true;
                } else if (std::strcmp(name, "character_id") == 0) {
                    character_id_not_null = sqlite3_column_int(info, 3) != 0;
                }
            }
            sqlite3_finalize(info);

            auto exec = [&](const char* sql) {
                char* error = nullptr;
                const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
                if (rc == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v17: ") +
                           (error ? error : sqlite3_errmsg(db)));
                sqlite3_free(error);
                return false;
            };
            if (has_account_id && !character_id_not_null) {
                return exec("CREATE UNIQUE INDEX IF NOT EXISTS ux_items_account_slot "
                            "ON items(account_id, slot) WHERE account_id IS NOT NULL") &&
                       exec("INSERT OR IGNORE INTO schema_version(version) VALUES(17)");
            }

            if (!exec("BEGIN IMMEDIATE")) {
                return false;
            }
            static constexpr const char* kRebuildSql = R"SQL(
ALTER TABLE items RENAME TO items_v16;
CREATE TABLE items (
  iid INTEGER PRIMARY KEY,
  character_id INTEGER,
  account_id INTEGER,
  item_index INTEGER NOT NULL,
  prefix INTEGER NOT NULL DEFAULT 0,
  count INTEGER NOT NULL DEFAULT 1,
  slot INTEGER NOT NULL DEFAULT 0,
  cur_end INTEGER NOT NULL DEFAULT 0,
  max_end INTEGER NOT NULL DEFAULT 0,
  x_attack INTEGER NOT NULL DEFAULT 0,
  x_magic INTEGER NOT NULL DEFAULT 0,
  x_hit INTEGER NOT NULL DEFAULT 0,
  x_defense INTEGER NOT NULL DEFAULT 0,
  x_dodge INTEGER NOT NULL DEFAULT 0,
  upgrade_level INTEGER NOT NULL DEFAULT 0,
  upgrade_rate INTEGER NOT NULL DEFAULT 0,
  mix_type INTEGER NOT NULL DEFAULT 0,
  bof INTEGER NOT NULL DEFAULT 0,
  qigong_perforated INTEGER NOT NULL DEFAULT 0,
  qigong_effect INTEGER NOT NULL DEFAULT 0,
  dragon_effect INTEGER NOT NULL DEFAULT 0,
  random_stat_type INTEGER NOT NULL DEFAULT 0,
  random_stat_value INTEGER NOT NULL DEFAULT 0,
  transform_level INTEGER NOT NULL DEFAULT 1,
  transform_exp INTEGER NOT NULL DEFAULT 0,
  CHECK ((character_id IS NOT NULL AND account_id IS NULL) OR
         (character_id IS NULL AND account_id IS NOT NULL)),
  CHECK (account_id IS NULL OR slot BETWEEN 0 AND 215),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);
INSERT INTO items(iid, character_id, account_id, item_index, prefix, count, slot,
                  cur_end, max_end, x_attack, x_magic, x_hit, x_defense, x_dodge,
                  upgrade_level, upgrade_rate, mix_type, bof, qigong_perforated,
                  qigong_effect, dragon_effect, random_stat_type, random_stat_value,
                  transform_level, transform_exp)
SELECT iid, character_id, NULL, item_index, prefix, count, slot,
       cur_end, max_end, x_attack, x_magic, x_hit, x_defense, x_dodge,
       upgrade_level, upgrade_rate, mix_type, bof, qigong_perforated,
       qigong_effect, dragon_effect, random_stat_type, random_stat_value,
       transform_level, transform_exp
FROM items_v16;
DROP TABLE items_v16;
CREATE UNIQUE INDEX IF NOT EXISTS ux_items_iid ON items(iid);
CREATE UNIQUE INDEX IF NOT EXISTS ux_items_account_slot
ON items(account_id, slot) WHERE account_id IS NOT NULL;
INSERT OR IGNORE INTO schema_version(version) VALUES(17);
)SQL";
            if (!exec(kRebuildSql) || !exec("COMMIT")) {
                (void) exec("ROLLBACK");
                return false;
            }
            return true;
        }

        /** Adds per-instance pet expiry timestamps. */
        [[nodiscard]] bool apply_schema_v18(sqlite3* db) {
            bool has_pet_expire_at = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_pet_expire_at = has_pet_expire_at ||
                                    (name != nullptr && std::strcmp(name, "pet_expire_at") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v18: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_pet_expire_at ||
                    exec(
                        "ALTER TABLE items ADD COLUMN pet_expire_at INTEGER NOT NULL DEFAULT 0")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(18)");
        }

        /** Adds durable, idempotent system-mail attachments. */
        [[nodiscard]] bool apply_schema_v19(sqlite3* db) {
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v19: ") + sqlite3_errmsg(db));
                return false;
            };
            return exec("CREATE TABLE IF NOT EXISTS system_mail ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "character_id INTEGER NOT NULL,"
                        "source_key TEXT NOT NULL,"
                        "subject TEXT NOT NULL,"
                        "body TEXT NOT NULL,"
                        "item_index INTEGER NOT NULL,"
                        "item_count INTEGER NOT NULL,"
                        "created_at INTEGER NOT NULL,"
                        "claimed_at INTEGER NOT NULL DEFAULT 0,"
                        "UNIQUE(character_id, source_key),"
                        "FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE)") &&
                   exec("CREATE INDEX IF NOT EXISTS ix_system_mail_character "
                        "ON system_mail(character_id, claimed_at, created_at DESC)") &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(19)");
        }

        /** Adds per-instance common riding expiry timestamps. */
        [[nodiscard]] bool apply_schema_v20(sqlite3* db) {
            bool has_ride_expire_at = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_ride_expire_at = has_ride_expire_at ||
                                     (name != nullptr && std::strcmp(name, "ride_expire_at") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v20: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_ride_expire_at || exec("ALTER TABLE items ADD COLUMN ride_expire_at "
                                               "INTEGER NOT NULL DEFAULT 0")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(20)");
        }

        /** Adds persistent per-IID Battle Riding progression. */
        [[nodiscard]] bool apply_schema_v21(sqlite3* db) {
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v21: ") + sqlite3_errmsg(db));
                return false;
            };
            return exec("CREATE TABLE IF NOT EXISTS battle_mounts ("
                        "mount_iid INTEGER PRIMARY KEY,"
                        "grade INTEGER NOT NULL DEFAULT 0 CHECK(grade BETWEEN 0 AND 3),"
                        "satiety INTEGER NOT NULL DEFAULT 100,"
                        "max_satiety INTEGER NOT NULL DEFAULT 100,"
                        "revision INTEGER NOT NULL DEFAULT 0,"
                        "ability0 INTEGER NOT NULL DEFAULT 0,"
                        "ability1 INTEGER NOT NULL DEFAULT 0,"
                        "ability2 INTEGER NOT NULL DEFAULT 0,"
                        "ability3 INTEGER NOT NULL DEFAULT 0,"
                        "ability4 INTEGER NOT NULL DEFAULT 0,"
                        "ability5 INTEGER NOT NULL DEFAULT 0,"
                        "equipment0 INTEGER NOT NULL DEFAULT 0,"
                        "equipment1 INTEGER NOT NULL DEFAULT 0,"
                        "equipment2 INTEGER NOT NULL DEFAULT 0,"
                        "equipment3 INTEGER NOT NULL DEFAULT 0,"
                        "equipment4 INTEGER NOT NULL DEFAULT 0,"
                        "FOREIGN KEY(mount_iid) REFERENCES items(iid) ON DELETE CASCADE)") &&
                   exec("CREATE TABLE IF NOT EXISTS battle_mount_collection ("
                        "account_id INTEGER NOT NULL,"
                        "mount_item_index INTEGER NOT NULL,"
                        "PRIMARY KEY(account_id, mount_item_index),"
                        "FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE)") &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(21)");
        }

        /** Adds the persistent account-bound marker for items that cannot leave their account. */
        [[nodiscard]] bool apply_schema_v22(sqlite3* db) {
            bool has_account_bound = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_account_bound = has_account_bound ||
                                    (name != nullptr && std::strcmp(name, "account_bound") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v22: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_account_bound ||
                    exec("ALTER TABLE items ADD COLUMN account_bound INTEGER NOT NULL DEFAULT 0 "
                         "CHECK(account_bound IN (0, 1))")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(22)");
        }

        /** Adds globally shared, persistent server-event state. */
        [[nodiscard]] bool apply_schema_v23(sqlite3* db) {
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v23: ") + sqlite3_errmsg(db));
                return false;
            };
            return exec("CREATE TABLE IF NOT EXISTS server_events ("
                        "event_id TEXT PRIMARY KEY, "
                        "contribution_count INTEGER NOT NULL DEFAULT 0, "
                        "active_bonus_percent INTEGER NOT NULL DEFAULT 0, "
                        "active_until INTEGER NOT NULL DEFAULT 0)") &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(23)");
        }

        /** Adds durable read state and soft deletion without losing reward idempotency. */
        [[nodiscard]] bool apply_schema_v24(sqlite3* db) {
            bool has_read = false, has_deleted = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(system_mail)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_read = has_read || (name && std::strcmp(name, "read_at") == 0);
                has_deleted = has_deleted || (name && std::strcmp(name, "deleted_at") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v24: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_read ||
                    exec(
                        "ALTER TABLE system_mail ADD COLUMN read_at INTEGER NOT NULL DEFAULT 0")) &&
                   (has_deleted || exec("ALTER TABLE system_mail ADD COLUMN deleted_at INTEGER NOT "
                                        "NULL DEFAULT 0")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(24)");
        }

        /** Adds the persistent single-attempt enchantment protection marker. */
        [[nodiscard]] bool apply_schema_v25(sqlite3* db) {
            bool has_enchant_protected = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(items)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_enchant_protected =
                    has_enchant_protected ||
                    (name != nullptr && std::strcmp(name, "enchant_protected") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [&](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v25: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_enchant_protected ||
                    exec(
                        "ALTER TABLE items ADD COLUMN enchant_protected INTEGER NOT NULL DEFAULT 0 "
                        "CHECK(enchant_protected IN (0, 1))")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(25)");
        }

        /** Adds durable talent selection and the Beacon cooldown deadline. */
        [[nodiscard]] bool apply_schema_v26(sqlite3* db) {
            bool has_mask = false, has_ready = false;
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_mask = has_mask || (name && std::strcmp(name, "talent_mask") == 0);
                has_ready = has_ready || (name && std::strcmp(name, "beacon_ready_at") == 0);
            }
            sqlite3_finalize(info);
            auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v26: ") + sqlite3_errmsg(db));
                return false;
            };
            return (has_mask || exec("ALTER TABLE characters ADD COLUMN talent_mask INTEGER NOT "
                                     "NULL DEFAULT 0")) &&
                   (has_ready || exec("ALTER TABLE characters ADD COLUMN beacon_ready_at INTEGER "
                                      "NOT NULL DEFAULT 0")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(26)");
        }

        [[nodiscard]] bool apply_schema_v27(sqlite3* db) {
            const auto has_column = [db](const char* table, const char* column) {
                sqlite3_stmt* info = nullptr;
                const std::string sql = std::string("PRAGMA table_info(") + table + ")";
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &info, nullptr) != SQLITE_OK) {
                    return false;
                }
                bool found = false;
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                    found = found || (name && std::strcmp(name, column) == 0);
                }
                sqlite3_finalize(info);
                return found;
            };
            const auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v27: ") + sqlite3_errmsg(db));
                return false;
            };
            if (!exec("SAVEPOINT starter_schema")) {
                return false;
            }
            // Existing characters are already served, even if their inventory is now empty.
            const bool ok =
                (has_column("characters", "starter_granted") ||
                 (exec("ALTER TABLE characters ADD COLUMN starter_granted INTEGER NOT NULL DEFAULT "
                       "0 "
                       "CHECK(starter_granted IN (0, 1))") &&
                  exec("UPDATE characters SET starter_granted=1"))) &&
                (has_column("items", "character_bound") ||
                 exec("ALTER TABLE items ADD COLUMN character_bound INTEGER NOT NULL DEFAULT 0 "
                      "CHECK(character_bound IN (0, 1))")) &&
                exec("INSERT OR IGNORE INTO schema_version(version) VALUES(27)");
            if (!ok) {
                (void) exec("ROLLBACK TO starter_schema");
                (void) exec("RELEASE starter_schema");
                return false;
            }
            return exec("RELEASE starter_schema");
        }

        std::optional<trading_post::Rows>
        query_market(sqlite3* db, const std::string& sql, const std::vector<std::string>& args) {
            sqlite3_stmt* prepared = nullptr;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &prepared, nullptr) != SQLITE_OK) {
                return std::nullopt;
            }
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(prepared,
                                                                            sqlite3_finalize);
            if (!stmt) {
                return std::nullopt;
            }
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (sqlite3_bind_text(stmt.get(),
                                      static_cast<int>(i + 1),
                                      args[i].c_str(),
                                      -1,
                                      SQLITE_TRANSIENT) != SQLITE_OK) {
                    return std::nullopt;
                }
            }
            trading_post::Rows rows;
            int result = SQLITE_OK;
            while ((result = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                std::vector<std::string> row;
                for (int i = 0; i < sqlite3_column_count(stmt.get()); ++i) {
                    const auto* value = sqlite3_column_text(stmt.get(), i);
                    row.emplace_back(value ? reinterpret_cast<const char*>(value) : "");
                }
                rows.push_back(std::move(row));
            }
            if (result != SQLITE_DONE) {
                return std::nullopt;
            }
            return rows;
        }

        /** Persists the owning account independently of inventory/storage location.
         * @param db Open database outside a transaction.
         * @return Whether migration and foreign-key validation succeeded. */
        [[nodiscard]] bool apply_schema_v28(sqlite3* db) {
            if (max_schema_version(db) >= 28) {
                return true;
            }
            const auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v28: ") + sqlite3_errmsg(db));
                return false;
            };
            // Rebuild without deleting mount state through the existing items foreign key.
            if (!exec("PRAGMA foreign_keys=OFF")) {
                return false;
            }
            if (!exec("BEGIN IMMEDIATE")) {
                (void) exec("PRAGMA foreign_keys=ON");
                return false;
            }
            if (max_schema_version(db) >= 28) {
                return exec("COMMIT") && exec("PRAGMA foreign_keys=ON");
            }
            static constexpr const char* kMigration = R"SQL(
INSERT INTO accounts(id,name,password) VALUES(-1,'!openkal-trading-post','!') ON CONFLICT(id) DO NOTHING;
CREATE TABLE items_v28 (
  iid INTEGER PRIMARY KEY,
  character_id INTEGER,
  account_id INTEGER NOT NULL,
  location INTEGER NOT NULL DEFAULT 0 CHECK(location BETWEEN 0 AND 2),
  item_index INTEGER NOT NULL,
  prefix INTEGER NOT NULL DEFAULT 0,
  count INTEGER NOT NULL DEFAULT 1,
  slot INTEGER NOT NULL DEFAULT 0,
  cur_end INTEGER NOT NULL DEFAULT 0,
  max_end INTEGER NOT NULL DEFAULT 0,
  x_attack INTEGER NOT NULL DEFAULT 0,
  x_magic INTEGER NOT NULL DEFAULT 0,
  x_hit INTEGER NOT NULL DEFAULT 0,
  x_defense INTEGER NOT NULL DEFAULT 0,
  x_dodge INTEGER NOT NULL DEFAULT 0,
  upgrade_level INTEGER NOT NULL DEFAULT 0,
  upgrade_rate INTEGER NOT NULL DEFAULT 0,
  mix_type INTEGER NOT NULL DEFAULT 0,
  bof INTEGER NOT NULL DEFAULT 0,
  qigong_perforated INTEGER NOT NULL DEFAULT 0,
  qigong_effect INTEGER NOT NULL DEFAULT 0,
  dragon_effect INTEGER NOT NULL DEFAULT 0,
  random_stat_type INTEGER NOT NULL DEFAULT 0,
  random_stat_value INTEGER NOT NULL DEFAULT 0,
  transform_level INTEGER NOT NULL DEFAULT 1,
  transform_exp INTEGER NOT NULL DEFAULT 0,
  pet_expire_at INTEGER NOT NULL DEFAULT 0,
  ride_expire_at INTEGER NOT NULL DEFAULT 0,
  account_bound INTEGER NOT NULL DEFAULT 0 CHECK(account_bound IN (0, 1)),
  enchant_protected INTEGER NOT NULL DEFAULT 0 CHECK(enchant_protected IN (0, 1)),
  character_bound INTEGER NOT NULL DEFAULT 0 CHECK(character_bound IN (0, 1)),
  CHECK (location<>1 OR slot BETWEEN 0 AND 215),
  CHECK ((location=0 AND character_id IS NOT NULL) OR (location IN (1,2) AND character_id IS NULL)),
  FOREIGN KEY(character_id) REFERENCES characters(id) ON DELETE CASCADE,
  FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE
);
INSERT INTO items_v28(location,iid,character_id,account_id,item_index,prefix,count,slot,cur_end,max_end,x_attack,x_magic,x_hit,x_defense,x_dodge,upgrade_level,upgrade_rate,mix_type,bof,qigong_perforated,qigong_effect,dragon_effect,random_stat_type,random_stat_value,transform_level,transform_exp,pet_expire_at,ride_expire_at,account_bound,enchant_protected,character_bound)
SELECT CASE WHEN character_id IS NULL THEN 1 ELSE 0 END,iid,character_id,COALESCE((SELECT account_id FROM characters WHERE id=items.character_id), account_id),item_index,prefix,count,slot,cur_end,max_end,x_attack,x_magic,x_hit,x_defense,x_dodge,upgrade_level,upgrade_rate,mix_type,bof,qigong_perforated,qigong_effect,dragon_effect,random_stat_type,random_stat_value,transform_level,transform_exp,pet_expire_at,ride_expire_at,account_bound,enchant_protected,character_bound FROM items;
DROP TABLE items;
ALTER TABLE items_v28 RENAME TO items;
CREATE UNIQUE INDEX ux_items_iid ON items(iid);
CREATE UNIQUE INDEX ux_items_account_slot ON items(account_id,slot) WHERE location=1;
ALTER TABLE trading_post_offers ADD COLUMN item_iid BIGINT REFERENCES items(iid);
ALTER TABLE trading_post_deliveries ADD COLUMN item_iid BIGINT REFERENCES items(iid);
)SQL";
            static constexpr const char* kReferences = R"SQL(

CREATE TABLE trading_post_offers_new(id INTEGER PRIMARY KEY AUTOINCREMENT,
 seller BIGINT NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
 item_iid BIGINT NOT NULL UNIQUE REFERENCES items(iid) ON DELETE CASCADE,
 unit_price BIGINT NOT NULL CHECK(unit_price>0),clazz BIGINT NOT NULL,type BIGINT NOT NULL,grade BIGINT NOT NULL);
INSERT INTO trading_post_offers_new(id,seller,item_iid,unit_price,clazz,type,grade)
 SELECT id,seller,item_iid,unit_price,clazz,type,grade FROM trading_post_offers WHERE count>0;
CREATE TABLE trading_post_deliveries_new(id INTEGER PRIMARY KEY AUTOINCREMENT,
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
INSERT INTO schema_version(version) VALUES(28);
)SQL";
            bool ok = exec(kMigration) &&
                      trading_post::migrate_escrow([db](const auto& sql, const auto& args) {
                          return query_market(db, sql, args);
                      }) &&
                      exec(kReferences);
            sqlite3_stmt* check = nullptr;
            if (ok) {
                ok = sqlite3_prepare_v2(db, "PRAGMA foreign_key_check", -1, &check, nullptr) ==
                         SQLITE_OK &&
                     sqlite3_step(check) == SQLITE_DONE;
                sqlite3_finalize(check);
            }
            if (ok) {
                ok = exec("COMMIT");
            }
            if (!ok) {
                (void) exec("ROLLBACK");
            }
            return exec("PRAGMA foreign_keys=ON") && ok;
        }

        /** Adds server-owned character roles without changing existing gameplay data.
         * @param db Open persistence database.
         * @return Whether the role column and migration marker are present. */
        [[nodiscard]] bool apply_schema_v29(sqlite3* db) {
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(characters)", -1, &info, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            bool present = false;
            while (sqlite3_step(info) == SQLITE_ROW) {
                const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                present = present || (name && std::strcmp(name, "role") == 0);
            }
            sqlite3_finalize(info);
            const auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v29: ") + sqlite3_errmsg(db));
                return false;
            };
            return (present || exec("ALTER TABLE characters ADD COLUMN role INTEGER NOT NULL "
                                    "DEFAULT 0 CHECK(role BETWEEN 0 AND 3)")) &&
                   exec("INSERT OR IGNORE INTO schema_version(version) VALUES(29)");
        }

        /** Adds persistent protocol discipline to existing accounts. */
        [[nodiscard]] bool apply_schema_v30(sqlite3* db) {
            const auto exec = [db](const char* sql) {
                if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
                    return true;
                }
                log::error(std::string("SqliteStore migrate v30: ") + sqlite3_errmsg(db));
                return false;
            };
            if (!exec("BEGIN IMMEDIATE")) {
                return false;
            }
            sqlite3_stmt* info = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA table_info(accounts)", -1, &info, nullptr) !=
                SQLITE_OK) {
                (void) exec("ROLLBACK");
                return false;
            }
            bool points = false, kicks = false, banned = false;
            int result;
            while ((result = sqlite3_step(info)) == SQLITE_ROW) {
                const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                points = points || std::strcmp(name, "protocol_points") == 0;
                kicks = kicks || std::strcmp(name, "protocol_kicks") == 0;
                banned = banned || std::strcmp(name, "banned") == 0;
            }
            sqlite3_finalize(info);
            if (result == SQLITE_DONE &&
                (points || exec("ALTER TABLE accounts ADD COLUMN protocol_points INTEGER NOT NULL "
                                "DEFAULT 0 CHECK(protocol_points BETWEEN 0 AND 9)")) &&
                (kicks || exec("ALTER TABLE accounts ADD COLUMN protocol_kicks INTEGER NOT NULL "
                               "DEFAULT 0 CHECK(protocol_kicks BETWEEN 0 AND 3)")) &&
                (banned || exec("ALTER TABLE accounts ADD COLUMN banned INTEGER NOT NULL "
                                "DEFAULT 0 CHECK(banned IN (0,1))")) &&
                exec("INSERT OR IGNORE INTO schema_version(version) VALUES(30)") &&
                exec("COMMIT")) {
                return true;
            }
            (void) exec("ROLLBACK");
            return false;
        }

    }  // namespace

    struct SqliteStore::Impl {
        sqlite3* db = nullptr;
        std::string path;

        [[nodiscard]] bool exec(const char* sql) const {
            char* err = nullptr;
            const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
            if (rc != SQLITE_OK) {
                log::error(std::string("SqliteStore: ") + (err ? err : sqlite3_errmsg(db)));
                sqlite3_free(err);
                return false;
            }
            return true;
        }

        [[nodiscard]] sqlite3_stmt* prepare(const char* sql) const {
            sqlite3_stmt* stmt = nullptr;
            const int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                log::error(std::string("SqliteStore prepare: ") + sqlite3_errmsg(db));
                return nullptr;
            }
            return stmt;
        }

        [[nodiscard]] std::optional<AccountDiscipline>
        account_discipline_row(const char* sql, std::int32_t account_id) const {
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(prepare(sql),
                                                                            sqlite3_finalize);
            if (!stmt || sqlite3_bind_int(stmt.get(), 1, account_id) != SQLITE_OK ||
                sqlite3_step(stmt.get()) != SQLITE_ROW) {
                return std::nullopt;
            }
            const AccountDiscipline state{sqlite3_column_int(stmt.get(), 0),
                                          sqlite3_column_int(stmt.get(), 1),
                                          sqlite3_column_int(stmt.get(), 2) != 0};
            return sqlite3_step(stmt.get()) == SQLITE_DONE ? std::optional{state} : std::nullopt;
        }

        std::optional<trading_post::Rows> market_query(const std::string& sql,
                                                       const std::vector<std::string>& args) {
            return query_market(db, sql, args);
        }

        [[nodiscard]] bool
        bind_character(sqlite3_stmt* stmt, const CharacterRecord& c, int id_bind) const {
            if (id_bind > 0) {
                if (sqlite3_bind_int(stmt, id_bind, c.id) != SQLITE_OK) {
                    return false;
                }
            }
            const int base = id_bind > 0 ? 2 : 1;
            auto text = [&](int idx, const std::string& s) {
                return sqlite3_bind_text(stmt, idx, s.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
            };
            return sqlite3_bind_int(stmt, base + 0, c.account_id) == SQLITE_OK &&
                   text(base + 1, c.name) &&
                   sqlite3_bind_int(stmt, base + 2, static_cast<std::int32_t>(c.role)) ==
                       SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 3, c.clazz) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 4, c.map) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 5, c.map_x) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 6, c.map_y) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 7, c.map_z) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 8, c.revival_point) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 9, c.level) == SQLITE_OK &&
                   sqlite3_bind_int64(stmt, base + 10, c.exp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 11, c.hp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 12, c.max_hp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 13, c.mp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 14, c.max_mp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 15, c.pu_point) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 16, c.str) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 17, c.hth) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 18, c.intl) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 19, c.wis) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 20, c.dex) == SQLITE_OK &&
                   sqlite3_bind_int64(stmt, base + 21, c.money) == SQLITE_OK &&
                   sqlite3_bind_int64(stmt, base + 22, static_cast<sqlite3_int64>(c.next_iid)) ==
                       SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 23, c.weapon_index) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 24, c.armor_index) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 25, c.transform_index) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 26, c.transform_level) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 27, c.transform_exp) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 28, c.transform_put_on_ticks) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 29, c.specialty) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 30, c.su_point) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 31, c.rage) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 32, c.contribution) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 33, c.face) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 34, c.hair) == SQLITE_OK &&
                   sqlite3_bind_int64(stmt, base + 35, static_cast<sqlite3_int64>(c.talent_mask)) ==
                       SQLITE_OK &&
                   sqlite3_bind_int64(stmt, base + 36, c.beacon_ready_at) == SQLITE_OK &&
                   sqlite3_bind_int(stmt, base + 37, c.pvp_death_exp_percent) == SQLITE_OK;
        }
    };

    SqliteStore::SqliteStore() : m_impl(std::make_unique<Impl>()) {}

    SqliteStore::~SqliteStore() {
        close();
    }

    bool SqliteStore::open(const std::string& path_or_dsn) {
        close();
        std::string file = path_or_dsn;
        if (path_or_dsn != ":memory:" && !looks_like_sqlite_file(path_or_dsn)) {
            (void) platform::ensure_directory(path_or_dsn);
            file = platform::join_path(path_or_dsn, kSqliteFileName);
        } else if (path_or_dsn != ":memory:") {
            const auto slash = path_or_dsn.find_last_of('/');
            const std::string dir =
                slash == std::string::npos ? std::string(".") : path_or_dsn.substr(0, slash);
            (void) platform::ensure_directory(dir);
        }

        sqlite3* db = nullptr;
        const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
        const int rc = sqlite3_open_v2(file.c_str(), &db, flags, nullptr);
        if (rc != SQLITE_OK) {
            log::error(std::string("SqliteStore: open failed: ") +
                       (db ? sqlite3_errmsg(db) : sqlite3_errstr(rc)));
            if (db) {
                sqlite3_close(db);
            }
            return false;
        }
        m_impl->db = db;
        m_impl->path = file;
        sqlite3_busy_timeout(db, 5000);
        if (!m_impl->exec("PRAGMA journal_mode=WAL") ||
            !m_impl->exec("PRAGMA synchronous=NORMAL") || !m_impl->exec("PRAGMA foreign_keys=ON") ||
            !m_impl->exec(kSchemaSql)) {
            close();
            return false;
        }
        {
            auto* info = m_impl->prepare("PRAGMA table_info(characters)");
            if (!info) {
                close();
                return false;
            }
            bool has_pvp_penalty = false;
            while (sqlite3_step(info) == SQLITE_ROW) {
                const auto* column = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                has_pvp_penalty |= column && std::strcmp(column, "pvp_death_exp_percent") == 0;
            }
            sqlite3_finalize(info);
            if (!has_pvp_penalty &&
                !m_impl->exec("ALTER TABLE characters ADD COLUMN pvp_death_exp_percent "
                              "INTEGER NOT NULL DEFAULT 0")) {
                close();
                return false;
            }
        }
        if (!m_impl->exec("INSERT OR IGNORE INTO schema_version(version) VALUES(1)")) {
            close();
            return false;
        }
        {
            std::array<bool, kShortcutSlots> present{};
            sqlite3_stmt* info = m_impl->prepare("PRAGMA table_info(shortcuts)");
            if (!info) {
                close();
                return false;
            }
            while (sqlite3_step(info) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
                for (std::size_t i = 0; name && i < present.size(); ++i) {
                    if (std::string(name) == "slot" + std::to_string(i)) {
                        present[i] = true;
                    }
                }
            }
            sqlite3_finalize(info);
            for (std::size_t i = 10; i < present.size(); ++i) {
                const std::string sql = "ALTER TABLE shortcuts ADD COLUMN slot" +
                                        std::to_string(i) + " INTEGER NOT NULL DEFAULT -1";
                if (!present[i] && !m_impl->exec(sql.c_str())) {
                    close();
                    return false;
                }
            }
        }
        {
            bool has_map = false;
            sqlite3_stmt* info = m_impl->prepare("PRAGMA table_info(characters)");
            if (info) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const unsigned char* name = sqlite3_column_text(info, 1);
                    if (name && std::strcmp(reinterpret_cast<const char*>(name), "map") == 0) {
                        has_map = true;
                        break;
                    }
                }
                sqlite3_finalize(info);
            }
            if (!has_map) {
                if (!m_impl->exec(
                        "ALTER TABLE characters ADD COLUMN map INTEGER NOT NULL DEFAULT 0")) {
                    close();
                    return false;
                }
            }
        }
        {
            bool has_cur_end = false;
            bool has_max_end = false;
            bool has_x_attack = false;
            bool has_x_magic = false;
            bool has_x_hit = false;
            bool has_x_defense = false;
            bool has_x_dodge = false;
            bool has_upgrade_level = false;
            bool has_upgrade_rate = false;
            bool has_mix_type = false;
            bool has_bof = false;
            bool has_transform_level = false;
            bool has_transform_exp = false;
            sqlite3_stmt* info = m_impl->prepare("PRAGMA table_info(items)");
            if (info) {
                while (sqlite3_step(info) == SQLITE_ROW) {
                    const unsigned char* name = sqlite3_column_text(info, 1);
                    if (!name) {
                        continue;
                    }
                    const char* col = reinterpret_cast<const char*>(name);
                    if (std::strcmp(col, "cur_end") == 0) {
                        has_cur_end = true;
                    } else if (std::strcmp(col, "max_end") == 0) {
                        has_max_end = true;
                    } else if (std::strcmp(col, "x_attack") == 0) {
                        has_x_attack = true;
                    } else if (std::strcmp(col, "x_magic") == 0) {
                        has_x_magic = true;
                    } else if (std::strcmp(col, "x_hit") == 0) {
                        has_x_hit = true;
                    } else if (std::strcmp(col, "x_defense") == 0) {
                        has_x_defense = true;
                    } else if (std::strcmp(col, "x_dodge") == 0) {
                        has_x_dodge = true;
                    } else if (std::strcmp(col, "upgrade_level") == 0) {
                        has_upgrade_level = true;
                    } else if (std::strcmp(col, "upgrade_rate") == 0) {
                        has_upgrade_rate = true;
                    } else if (std::strcmp(col, "mix_type") == 0) {
                        has_mix_type = true;
                    } else if (std::strcmp(col, "bof") == 0) {
                        has_bof = true;
                    } else if (std::strcmp(col, "transform_level") == 0) {
                        has_transform_level = true;
                    } else if (std::strcmp(col, "transform_exp") == 0) {
                        has_transform_exp = true;
                    }
                }
                sqlite3_finalize(info);
            }
            auto add_col = [this](bool present, const char* sql) -> bool {
                if (present) {
                    return true;
                }
                if (!m_impl->exec(sql)) {
                    close();
                    return false;
                }
                return true;
            };
            if (!add_col(has_cur_end,
                         "ALTER TABLE items ADD COLUMN cur_end INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_max_end,
                         "ALTER TABLE items ADD COLUMN max_end INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_x_attack,
                         "ALTER TABLE items ADD COLUMN x_attack INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_x_magic,
                         "ALTER TABLE items ADD COLUMN x_magic INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_x_hit,
                         "ALTER TABLE items ADD COLUMN x_hit INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_x_defense,
                         "ALTER TABLE items ADD COLUMN x_defense INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_x_dodge,
                         "ALTER TABLE items ADD COLUMN x_dodge INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_upgrade_level,
                         "ALTER TABLE items ADD COLUMN upgrade_level INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_upgrade_rate,
                         "ALTER TABLE items ADD COLUMN upgrade_rate INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_mix_type,
                         "ALTER TABLE items ADD COLUMN mix_type INTEGER NOT NULL DEFAULT 0") ||
                !add_col(has_bof, "ALTER TABLE items ADD COLUMN bof INTEGER NOT NULL DEFAULT 0") ||
                !add_col(
                    has_transform_level,
                    "ALTER TABLE items ADD COLUMN transform_level INTEGER NOT NULL DEFAULT 1") ||
                !add_col(has_transform_exp,
                         "ALTER TABLE items ADD COLUMN transform_exp INTEGER NOT NULL DEFAULT 0")) {
                return false;
            }
        }
        if (max_schema_version(db) < 2) {
            if (!apply_schema_v2(db)) {
                close();
                return false;
            }
        }
        if (max_schema_version(db) < 3) {
            if (!apply_schema_v3(db)) {
                close();
                return false;
            }
        }
        if (max_schema_version(db) < 4) {
            if (!apply_schema_v4(db)) {
                close();
                return false;
            }
        }
        if (max_schema_version(db) < 5) {
            if (!apply_schema_v5(db)) {
                close();
                return false;
            }
        }
        if (max_schema_version(db) < 6) {
            if (!apply_schema_v6(db)) {
                close();
                return false;
            }
        }
        // Re-run these idempotent migrations so databases restored from an
        // older backup cannot retain a newer schema marker without its columns.
        if (!apply_schema_v7(db) || !apply_schema_v8(db)) {
            close();
            return false;
        }
        if (!apply_schema_v9(db) || !apply_schema_v10(db) || !apply_schema_v11(db) ||
            !apply_schema_v12(db) || !apply_schema_v13(db) || !apply_schema_v14(db) ||
            !apply_schema_v15(db) || !apply_schema_v16(db) || !apply_schema_v17(db) ||
            !apply_schema_v18(db) || !apply_schema_v19(db) || !apply_schema_v20(db) ||
            !apply_schema_v21(db) || !apply_schema_v22(db) || !apply_schema_v23(db) ||
            !apply_schema_v24(db) || !apply_schema_v25(db) || !apply_schema_v26(db) ||
            !apply_schema_v27(db) || !apply_schema_v28(db) || !apply_schema_v29(db) ||
            !apply_schema_v30(db)) {
            close();
            return false;
        }
        return true;
    }

    void SqliteStore::close() {
        if (!m_impl || !m_impl->db) {
            return;
        }
        (void) m_impl->exec("PRAGMA wal_checkpoint(TRUNCATE)");
        sqlite3_close(m_impl->db);
        m_impl->db = nullptr;
        m_impl->path.clear();
    }

    std::optional<AccountRecord> SqliteStore::find_account(const std::string& name) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("SELECT id, name, password FROM accounts WHERE name = ? AND id > 0");
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<AccountRecord> out;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            AccountRecord a;
            a.id = sqlite3_column_int(stmt, 0);
            const unsigned char* n = sqlite3_column_text(stmt, 1);
            const unsigned char* p = sqlite3_column_text(stmt, 2);
            a.name = n ? reinterpret_cast<const char*>(n) : "";
            a.password = p ? reinterpret_cast<const char*>(p) : "";
            out = a;
        }
        sqlite3_finalize(stmt);
        return out;
    }

    std::optional<AccountDiscipline> SqliteStore::load_account_discipline(std::int32_t account_id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        return m_impl->account_discipline_row(account_discipline::kLoad, account_id);
    }

    std::optional<AccountDiscipline>
    SqliteStore::update_account_discipline(std::int32_t account_id,
                                           AccountDisciplineChange change) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        return m_impl->account_discipline_row(change == AccountDisciplineChange::Violation
                                                  ? account_discipline::kViolation
                                                  : account_discipline::kDecay,
                                              account_id);
    }

    bool SqliteStore::upsert_account(const AccountRecord& account) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (account.id > 0) {
            sqlite3_stmt* stmt =
                m_impl->prepare("INSERT INTO accounts(id, name, password) VALUES(?, ?, ?) "
                                "ON CONFLICT(id) DO UPDATE SET name=excluded.name, "
                                "password=excluded.password");
            if (!stmt) {
                return false;
            }
            sqlite3_bind_int(stmt, 1, account.id);
            sqlite3_bind_text(stmt, 2, account.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, account.password.c_str(), -1, SQLITE_TRANSIENT);
            const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            if (!ok) {
                log::error(std::string("SqliteStore upsert_account: ") +
                           sqlite3_errmsg(m_impl->db));
            }
            sqlite3_finalize(stmt);
            return ok;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("INSERT INTO accounts(name, password) VALUES(?, ?) "
                            "ON CONFLICT(name) DO UPDATE SET password=excluded.password");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, account.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, account.password.c_str(), -1, SQLITE_TRANSIENT);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore upsert_account: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::vector<CharacterRecord> SqliteStore::list_characters(std::int32_t account_id) {
        std::vector<CharacterRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kCharacterSelect) +
                                " WHERE account_id = ? AND deletion_state = 0 ORDER BY id";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, account_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            out.push_back(character_from_stmt(stmt));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    std::vector<CharacterRecord> SqliteStore::list_deleted_characters(std::int32_t account_id,
                                                                      std::int64_t now_seconds) {
        std::vector<CharacterRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kCharacterSelect) +
                                " WHERE account_id = ? AND deletion_state = 1 AND delete_after > ? "
                                "ORDER BY delete_after, id";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, account_id);
        sqlite3_bind_int64(stmt, 2, now_seconds);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            out.push_back(character_from_stmt(stmt));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    std::optional<CharacterRecord> SqliteStore::load_character(std::int32_t id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        const std::string sql =
            std::string(kCharacterSelect) + " WHERE id = ? AND deletion_state = 0";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, id);
        std::optional<CharacterRecord> out;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out = character_from_stmt(stmt);
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::save_character(const CharacterRecord& character) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        constexpr const char* kCols =
            "account_id, name, role, clazz, map, x, y, z, revival_point, level, exp, hp, max_hp, "
            "mp, "
            "max_mp, "
            "pu_point, str, hth, intl, wis, dex, money, next_iid, weapon_index, armor_index, "
            "transform_index, transform_level, transform_exp, transform_put_on_ticks, specialty, "
            "su_point, rage, contribution, face, hair, talent_mask, beacon_ready_at, pvp_death_exp_percent";
        if (character.id > 0) {
            const std::string sql =
                std::string("INSERT INTO characters(id, ") + kCols +
                ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "?, "
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
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
                "talent_mask=excluded.talent_mask, beacon_ready_at=excluded.beacon_ready_at, "
                "pvp_death_exp_percent=excluded.pvp_death_exp_percent";
            sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
            if (!stmt) {
                return false;
            }
            if (!m_impl->bind_character(stmt, character, 1)) {
                sqlite3_finalize(stmt);
                return false;
            }
            const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            if (!ok) {
                log::error(std::string("SqliteStore save_character: ") +
                           sqlite3_errmsg(m_impl->db));
            }
            sqlite3_finalize(stmt);
            return ok;
        }
        const std::string sql =
            std::string("INSERT INTO characters(") + kCols +
            ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return false;
        }
        if (!m_impl->bind_character(stmt, character, 0)) {
            sqlite3_finalize(stmt);
            return false;
        }
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore save_character: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::schedule_character_deletion(std::int32_t id,
                                                  std::int32_t account_id,
                                                  std::int64_t now_seconds) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("UPDATE characters SET deletion_state = 1, delete_after = ? "
                            "WHERE id = ? AND account_id = ? AND deletion_state = 0 "
                            "AND (SELECT COUNT(*) FROM characters WHERE account_id = ? "
                            "AND deletion_state = 1) < ?");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int64(stmt, 1, now_seconds + kCharacterDeletionGraceSeconds);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_bind_int(stmt, 3, account_id);
        sqlite3_bind_int(stmt, 4, account_id);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(kMaxDeletedCharactersPerAccount));
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_impl->db) > 0;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::restore_character(std::int32_t id,
                                        std::int32_t account_id,
                                        std::int64_t now_seconds) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "UPDATE characters SET deletion_state = 0, delete_after = 0 "
            "WHERE id = ? AND account_id = ? AND deletion_state = 1 AND delete_after > ? "
            "AND (SELECT COUNT(*) FROM characters WHERE account_id = ? AND deletion_state = 0) "
            "< ?");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, account_id);
        sqlite3_bind_int64(stmt, 3, now_seconds);
        sqlite3_bind_int(stmt, 4, account_id);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(kMaxCharactersPerAccount));
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_impl->db) > 0;
        sqlite3_finalize(stmt);
        return ok;
    }

    CharacterPurgeResult SqliteStore::process_character_deletions(std::int64_t now_seconds,
                                                                  std::size_t row_budget) {
        if (!m_impl || !m_impl->db || row_budget == 0) {
            return CharacterPurgeResult::Error;
        }
        constexpr std::size_t kMaxPurgeBatch = 256;
        const sqlite3_int64 bounded_budget =
            static_cast<sqlite3_int64>(std::min(row_budget, kMaxPurgeBatch));

        sqlite3_stmt* due = m_impl->prepare(
            "SELECT 1 FROM characters "
            "WHERE deletion_state = 2 OR (deletion_state = 1 AND delete_after <= ?) LIMIT 1");
        if (!due) {
            return CharacterPurgeResult::Error;
        }
        sqlite3_bind_int64(due, 1, now_seconds);
        const bool has_due_character = sqlite3_step(due) == SQLITE_ROW;
        sqlite3_finalize(due);
        if (!has_due_character) {
            return CharacterPurgeResult::Idle;
        }

        if (!m_impl->exec("BEGIN IMMEDIATE")) {
            return CharacterPurgeResult::Error;
        }
        auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return CharacterPurgeResult::Error;
        };

        sqlite3_stmt* select = m_impl->prepare(
            "SELECT id FROM characters "
            "WHERE deletion_state = 2 OR (deletion_state = 1 AND delete_after <= ?) "
            "ORDER BY deletion_state DESC, delete_after, id LIMIT 1");
        if (!select) {
            return rollback();
        }
        sqlite3_bind_int64(select, 1, now_seconds);
        if (sqlite3_step(select) != SQLITE_ROW) {
            sqlite3_finalize(select);
            if (!m_impl->exec("COMMIT")) {
                return CharacterPurgeResult::Error;
            }
            return CharacterPurgeResult::Idle;
        }
        const std::int32_t character_id = sqlite3_column_int(select, 0);
        sqlite3_finalize(select);

        sqlite3_stmt* mark = m_impl->prepare(
            "UPDATE characters SET deletion_state = 2 WHERE id = ? AND deletion_state = 1");
        if (!mark) {
            return rollback();
        }
        sqlite3_bind_int(mark, 1, character_id);
        const bool marked = sqlite3_step(mark) == SQLITE_DONE;
        sqlite3_finalize(mark);
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
                                    " WHERE rowid IN (SELECT rowid FROM " + table + " WHERE " +
                                    owner_filter + " LIMIT ?)";
            sqlite3_stmt* remove = m_impl->prepare(sql.c_str());
            if (!remove) {
                return rollback();
            }
            sqlite3_bind_int(remove, 1, character_id);
            if (item_table) {
                sqlite3_bind_int(remove, 2, character_id);
                sqlite3_bind_int(remove, 3, character_id);
            }
            sqlite3_bind_int64(remove, item_table ? 4 : 2, bounded_budget);
            const bool removed = sqlite3_step(remove) == SQLITE_DONE;
            const int changed = sqlite3_changes(m_impl->db);
            sqlite3_finalize(remove);
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

        sqlite3_stmt* remove_character =
            m_impl->prepare("DELETE FROM characters WHERE id = ? AND deletion_state = 2");
        if (!remove_character) {
            return rollback();
        }
        sqlite3_bind_int(remove_character, 1, character_id);
        const bool removed = sqlite3_step(remove_character) == SQLITE_DONE;
        const int changed = sqlite3_changes(m_impl->db);
        sqlite3_finalize(remove_character);
        if (!removed || changed == 0) {
            return rollback();
        }
        if (!m_impl->exec("COMMIT")) {
            return CharacterPurgeResult::Error;
        }
        return CharacterPurgeResult::CharacterPurged;
    }

    std::vector<ItemRecord> SqliteStore::load_items(std::int32_t character_id) {
        std::vector<ItemRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        const std::string sql = std::string(kItemSelect) + " WHERE character_id = ? ORDER BY iid";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            out.push_back(item_from_stmt(stmt));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    std::vector<ItemRecord> SqliteStore::load_storage_items(std::int32_t account_id) {
        std::vector<ItemRecord> out;
        if (!m_impl || !m_impl->db || account_id <= 0) {
            return out;
        }
        const std::string sql = std::string(kItemSelect) +
                                " WHERE account_id = ? AND location=1 AND character_id IS NULL "
                                "ORDER BY slot, iid";
        sqlite3_stmt* stmt = m_impl->prepare(sql.c_str());
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, account_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            out.push_back(item_from_stmt(stmt));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    std::optional<std::int64_t> SqliteStore::load_storage_money(std::int32_t account_id) {
        if (!m_impl || !m_impl->db || account_id <= 0) {
            return std::nullopt;
        }
        auto* stmt = m_impl->prepare(
            "SELECT COALESCE(s.balance,0) FROM accounts a LEFT JOIN account_storage_money s "
            "ON s.account_id=a.id WHERE a.id=?");
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, account_id);
        std::optional<std::int64_t> balance;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            balance = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return balance;
    }

    std::optional<std::int64_t> SqliteStore::transfer_storage_money(std::int32_t account_id,
                                                                    std::int32_t character_id,
                                                                    std::int64_t wallet_after,
                                                                    std::int64_t change) {
        if (!m_impl || !m_impl->db || account_id <= 0 || character_id <= 0 || wallet_after < 0 ||
            change == 0 || change == std::numeric_limits<std::int64_t>::min() ||
            !m_impl->exec("BEGIN IMMEDIATE")) {
            return std::nullopt;
        }
        const auto rollback = [&]() -> std::optional<std::int64_t> {
            (void) m_impl->exec("ROLLBACK");
            return std::nullopt;
        };
        auto* stmt = m_impl->prepare(
            "UPDATE characters SET money=? WHERE id=? AND account_id=? AND deletion_state=0");
        if (!stmt) {
            return rollback();
        }
        sqlite3_bind_int64(stmt, 1, wallet_after);
        sqlite3_bind_int(stmt, 2, character_id);
        sqlite3_bind_int(stmt, 3, account_id);
        const bool wallet_updated =
            sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(stmt);
        if (!wallet_updated) {
            return rollback();
        }
        stmt = m_impl->prepare("INSERT INTO account_storage_money(account_id,balance) VALUES(?,0) "
                               "ON CONFLICT(account_id) DO NOTHING");
        if (!stmt) {
            return rollback();
        }
        sqlite3_bind_int(stmt, 1, account_id);
        const bool initialized = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!initialized) {
            return rollback();
        }
        stmt = m_impl->prepare("UPDATE account_storage_money SET balance=balance+? "
                               "WHERE account_id=? AND balance BETWEEN ? AND ?");
        if (!stmt) {
            return rollback();
        }
        sqlite3_bind_int64(stmt, 1, change);
        sqlite3_bind_int(stmt, 2, account_id);
        sqlite3_bind_int64(stmt, 3, std::max<std::int64_t>(0, -change));
        sqlite3_bind_int64(
            stmt, 4, std::numeric_limits<std::int64_t>::max() - std::max<std::int64_t>(0, change));
        const bool balance_updated =
            sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(stmt);
        const auto balance = balance_updated ? load_storage_money(account_id) : std::nullopt;
        if (!balance || !m_impl->exec("COMMIT")) {
            return rollback();
        }
        return balance;
    }

    bool SqliteStore::consume_item_with_rewards(const ItemRecord& box,
                                                const std::vector<ItemRecord>& rewards) {
        if (!m_impl || !m_impl->db || box.iid <= 0 || box.character_id <= 0 || box.count <= 0 ||
            box.slot != 0 || rewards.empty()) {
            return false;
        }
        if (!m_impl->exec("BEGIN IMMEDIATE TRANSACTION")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        sqlite3_stmt* consume = m_impl->prepare(
            box.count == 1
                ? "DELETE FROM items WHERE iid=? AND character_id=? AND item_index=? AND count=? "
                  "AND slot=0"
                : "UPDATE items SET count=count-1 WHERE iid=? AND character_id=? AND item_index=? "
                  "AND count=? AND slot=0");
        if (!consume) {
            return rollback();
        }
        sqlite3_bind_int(consume, 1, box.iid);
        sqlite3_bind_int(consume, 2, box.character_id);
        sqlite3_bind_int(consume, 3, box.index);
        sqlite3_bind_int(consume, 4, box.count);
        const bool consumed =
            sqlite3_step(consume) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(consume);
        if (!consumed) {
            return rollback();
        }
        for (const auto& reward : rewards) {
            if (reward.iid <= 0 || reward.iid == box.iid || reward.index <= 0 ||
                reward.count <= 0 || reward.character_id != box.character_id || reward.slot != 0) {
                return rollback();
            }
            // insert_item also supports upserts, so reject existing ids before calling it.
            sqlite3_stmt* existing = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
            if (!existing) {
                return rollback();
            }
            sqlite3_bind_int(existing, 1, reward.iid);
            const int result = sqlite3_step(existing);
            sqlite3_finalize(existing);
            if (result != SQLITE_DONE || !insert_item(reward)) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool SqliteStore::grant_starter_items(const CharacterRecord& character,
                                          const std::vector<ItemRecord>& items) {
        if (!m_impl || !m_impl->db || character.id <= 0 || !m_impl->exec("SAVEPOINT starter_kit")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK TO starter_kit");
            (void) m_impl->exec("RELEASE starter_kit");
            return false;
        };
        sqlite3_stmt* claim = m_impl->prepare(
            "UPDATE characters SET starter_granted=1 WHERE id=? AND starter_granted=0 "
            "AND deletion_state=0 AND NOT EXISTS (SELECT 1 FROM items WHERE character_id=?)");
        if (!claim) {
            return rollback();
        }
        sqlite3_bind_int(claim, 1, character.id);
        sqlite3_bind_int(claim, 2, character.id);
        const bool claimed = sqlite3_step(claim) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(claim);
        if (!claimed) {
            return rollback();
        }
        for (const auto& item : items) {
            if (item.iid <= 0 || item.count <= 0 || item.character_id != character.id ||
                !item.character_bound) {
                return rollback();
            }
            sqlite3_stmt* existing = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
            if (!existing) {
                return rollback();
            }
            sqlite3_bind_int(existing, 1, item.iid);
            const int result = sqlite3_step(existing);
            sqlite3_finalize(existing);
            if (result != SQLITE_DONE || !insert_item(item)) {
                return rollback();
            }
        }
        if (!save_character(character) || !m_impl->exec("RELEASE starter_kit")) {
            return rollback();
        }
        return true;
    }

    bool SqliteStore::insert_item(const ItemRecord& item) {
        if (!m_impl || !m_impl->db || (item.character_bound && item.character_id <= 0)) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
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
        sqlite3_bind_int(stmt, 1, item.iid);
        if (item.character_id > 0) {
            sqlite3_bind_int(stmt, 2, item.character_id);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_int(stmt, 3, item.character_id);
        if (item.account_id > 0) {
            sqlite3_bind_int(stmt, 4, item.account_id);
        } else {
            sqlite3_bind_null(stmt, 4);
        }
        sqlite3_bind_int(stmt, 5, item.index);
        sqlite3_bind_int(stmt, 6, item.prefix);
        sqlite3_bind_int(stmt, 7, item.count);
        sqlite3_bind_int(stmt, 8, item.slot);
        sqlite3_bind_int(stmt, 9, item.cur_end);
        sqlite3_bind_int(stmt, 10, item.max_end);
        sqlite3_bind_int(stmt, 11, item.x_attack);
        sqlite3_bind_int(stmt, 12, item.x_magic);
        sqlite3_bind_int(stmt, 13, item.x_hit);
        sqlite3_bind_int(stmt, 14, item.x_defense);
        sqlite3_bind_int(stmt, 15, item.x_dodge);
        sqlite3_bind_int(stmt, 16, item.upgrade_level);
        sqlite3_bind_int(stmt, 17, item.upgrade_rate);
        sqlite3_bind_int(stmt, 18, item.mix_type);
        sqlite3_bind_int(stmt, 19, item.bof);
        sqlite3_bind_int(stmt, 20, item.qigong_perforated);
        sqlite3_bind_int(stmt, 21, item.qigong_effect);
        sqlite3_bind_int(stmt, 22, item.dragon_effect);
        sqlite3_bind_int(stmt, 23, item.random_stat_type);
        sqlite3_bind_int(stmt, 24, item.random_stat_value);
        sqlite3_bind_int(stmt, 25, item.transform_level);
        sqlite3_bind_int(stmt, 26, item.transform_exp);
        sqlite3_bind_int64(stmt, 27, item.pet_expire_at);
        sqlite3_bind_int64(stmt, 28, item.ride_expire_at);
        sqlite3_bind_int(stmt, 29, item.account_bound ? 1 : 0);
        sqlite3_bind_int(stmt, 30, item.enchant_protected ? 1 : 0);
        sqlite3_bind_int(stmt, 31, item.character_bound ? 1 : 0);
        sqlite3_bind_int(stmt, 32, item.character_id > 0 ? 0 : 1);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore insert_item: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::update_item(const ItemRecord& item) {
        if (!m_impl || !m_impl->db || (item.character_bound && item.character_id <= 0)) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
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
        sqlite3_bind_int(stmt, 1, item.index);
        sqlite3_bind_int(stmt, 2, item.prefix);
        sqlite3_bind_int(stmt, 3, item.count);
        sqlite3_bind_int(stmt, 4, item.slot);
        if (item.character_id > 0) {
            sqlite3_bind_int(stmt, 5, item.character_id);
        } else {
            sqlite3_bind_null(stmt, 5);
        }
        sqlite3_bind_int(stmt, 6, item.character_id);
        if (item.account_id > 0) {
            sqlite3_bind_int(stmt, 7, item.account_id);
        } else {
            sqlite3_bind_null(stmt, 7);
        }
        sqlite3_bind_int(stmt, 8, item.cur_end);
        sqlite3_bind_int(stmt, 9, item.max_end);
        sqlite3_bind_int(stmt, 10, item.x_attack);
        sqlite3_bind_int(stmt, 11, item.x_magic);
        sqlite3_bind_int(stmt, 12, item.x_hit);
        sqlite3_bind_int(stmt, 13, item.x_defense);
        sqlite3_bind_int(stmt, 14, item.x_dodge);
        sqlite3_bind_int(stmt, 15, item.upgrade_level);
        sqlite3_bind_int(stmt, 16, item.upgrade_rate);
        sqlite3_bind_int(stmt, 17, item.mix_type);
        sqlite3_bind_int(stmt, 18, item.bof);
        sqlite3_bind_int(stmt, 19, item.qigong_perforated);
        sqlite3_bind_int(stmt, 20, item.qigong_effect);
        sqlite3_bind_int(stmt, 21, item.dragon_effect);
        sqlite3_bind_int(stmt, 22, item.random_stat_type);
        sqlite3_bind_int(stmt, 23, item.random_stat_value);
        sqlite3_bind_int(stmt, 24, item.transform_level);
        sqlite3_bind_int(stmt, 25, item.transform_exp);
        sqlite3_bind_int64(stmt, 26, item.pet_expire_at);
        sqlite3_bind_int64(stmt, 27, item.ride_expire_at);
        sqlite3_bind_int(stmt, 28, item.account_bound ? 1 : 0);
        sqlite3_bind_int(stmt, 29, item.enchant_protected ? 1 : 0);
        sqlite3_bind_int(stmt, 30, item.character_bound ? 1 : 0);
        sqlite3_bind_int(stmt, 31, item.character_id > 0 ? 0 : 1);
        sqlite3_bind_int(stmt, 32, item.iid);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore update_item: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::update_equipment_items(const std::vector<ItemRecord>& items) {
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
        if (!m_impl->exec("BEGIN IMMEDIATE")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        for (const auto& item : items) {
            if (item.iid <= 0 || item.character_id <= 0 || item.count != 1 ||
                item.character_id != items.front().character_id || item.slot < 0 ||
                item.slot > kAssassinMaskSlot) {
                return rollback();
            }
            auto statement = m_impl->prepare("SELECT 1 FROM items WHERE iid=? AND character_id=? "
                                             "AND count=1");
            if (!statement) {
                return rollback();
            }
            sqlite3_bind_int(statement, 1, item.iid);
            sqlite3_bind_int(statement, 2, item.character_id);
            const bool owned = sqlite3_step(statement) == SQLITE_ROW;
            sqlite3_finalize(statement);
            if (!owned || !update_item(item) || sqlite3_changes(m_impl->db) != 1) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool SqliteStore::exchange_inventory_items(const std::vector<ItemRecord>& expected,
                                               const std::vector<ItemRecord>& replacements) {
        if (!m_impl || !m_impl->db || replacements.empty() || !m_impl->exec("BEGIN IMMEDIATE")) {
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
            sqlite3_bind_int(statement, 1, item.iid);
            sqlite3_bind_int(statement, 2, owner);
            sqlite3_bind_int(statement, 3, item.index);
            sqlite3_bind_int(statement, 4, item.count);
            const bool removed =
                sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
            sqlite3_finalize(statement);
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
            sqlite3_bind_int(statement, 1, item.iid);
            const int result = sqlite3_step(statement);
            sqlite3_finalize(statement);
            if (result != SQLITE_DONE || !insert_item(item)) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    bool SqliteStore::commit_assassin_death(const CharacterRecord& character,
                                            const std::vector<ItemRecord>& items,
                                            const std::string& last_hitter) {
        if (!m_impl || !m_impl->db || character.id <= 0 || character.hp != 0 || items.empty() ||
            !m_impl->exec("BEGIN IMMEDIATE")) {
            return false;
        }
        const auto rollback = [&] {
            (void) m_impl->exec("ROLLBACK");
            return false;
        };
        for (const auto& item : items) {
            if (item.iid <= 0 || item.character_id != character.id || item.count <= 0) {
                return rollback();
            }
            auto* statement =
                m_impl->prepare("DELETE FROM items WHERE iid=? AND character_id=? AND item_index=? "
                                "AND count=? AND slot=? AND location=0");
            if (!statement) {
                return rollback();
            }
            const bool deleted = sqlite3_bind_int(statement, 1, item.iid) == SQLITE_OK &&
                                 sqlite3_bind_int(statement, 2, item.character_id) == SQLITE_OK &&
                                 sqlite3_bind_int(statement, 3, item.index) == SQLITE_OK &&
                                 sqlite3_bind_int(statement, 4, item.count) == SQLITE_OK &&
                                 sqlite3_bind_int(statement, 5, item.slot) == SQLITE_OK &&
                                 sqlite3_step(statement) == SQLITE_DONE &&
                                 sqlite3_changes(m_impl->db) == 1;
            sqlite3_finalize(statement);
            if (!deleted) {
                return rollback();
            }
        }
        if (!save_character(character)) {
            return rollback();
        }
        if (!last_hitter.empty()) {
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> add(
                m_impl->prepare("INSERT INTO assassin_list(assassin,last_hitter) VALUES(?,?)"),
                sqlite3_finalize);
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> prune(
                m_impl->prepare("DELETE FROM assassin_list WHERE id < "
                                "(SELECT id FROM assassin_list ORDER BY id DESC LIMIT 1 OFFSET ?)"),
                sqlite3_finalize);
            if (!add || !prune ||
                sqlite3_bind_text(add.get(), 1, character.name.c_str(), -1, SQLITE_TRANSIENT) !=
                    SQLITE_OK ||
                sqlite3_bind_text(add.get(), 2, last_hitter.c_str(), -1, SQLITE_TRANSIENT) !=
                    SQLITE_OK ||
                sqlite3_step(add.get()) != SQLITE_DONE ||
                sqlite3_bind_int(prune.get(), 1, protocol::kAssassinListLimit - 1) != SQLITE_OK ||
                sqlite3_step(prune.get()) != SQLITE_DONE) {
                return rollback();
            }
        }
        if (!m_impl->exec("COMMIT")) {
            return rollback();
        }
        return true;
    }

    std::optional<std::vector<protocol::AssassinListEntry>> SqliteStore::load_assassin_list() {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> query(
            m_impl->prepare(
                "SELECT assassin,last_hitter FROM assassin_list ORDER BY id DESC LIMIT ?"),
            sqlite3_finalize);
        if (!query || sqlite3_bind_int(query.get(), 1, protocol::kAssassinListLimit) != SQLITE_OK) {
            return std::nullopt;
        }
        std::vector<protocol::AssassinListEntry> entries;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(query.get())) == SQLITE_ROW) {
            entries.push_back({reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0)),
                               reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1))});
        }
        if (result != SQLITE_DONE) {
            return std::nullopt;
        }
        return entries;
    }

    bool SqliteStore::commit_trade(const CharacterRecord& first,
                                   const CharacterRecord& second,
                                   const std::vector<TradeTransfer>& items) {
        if (!m_impl || !m_impl->db || first.id <= 0 || second.id <= 0 || first.id == second.id ||
            first.money < 0 || second.money < 0 || !m_impl->exec("BEGIN IMMEDIATE")) {
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
                auto* statement = m_impl->prepare("SELECT 1 FROM items WHERE iid=?");
                if (!statement) {
                    return rollback();
                }
                sqlite3_bind_int(statement, 1, transfer.split_iid);
                const bool unused = sqlite3_step(statement) == SQLITE_DONE;
                sqlite3_finalize(statement);
                if (!unused) {
                    return rollback();
                }
            }
            const int recipient = item.character_id == first.id ? second.id : first.id;
            auto* statement = m_impl->prepare(
                "UPDATE items SET account_id=(SELECT account_id FROM characters WHERE id=?), "
                "character_id=?, count=? WHERE iid=? AND character_id=? "
                "AND item_index=? AND count=? AND slot=0 AND "
                "account_bound=0 AND character_bound=0");
            if (!statement) {
                return rollback();
            }
            sqlite3_bind_int(statement, 1, partial ? item.character_id : recipient);
            sqlite3_bind_int(statement, 2, partial ? item.character_id : recipient);
            sqlite3_bind_int(statement, 3, partial ? item.count - transfer.count : transfer.count);
            sqlite3_bind_int(statement, 4, item.iid);
            sqlite3_bind_int(statement, 5, item.character_id);
            sqlite3_bind_int(statement, 6, item.index);
            sqlite3_bind_int(statement, 7, item.count);
            const bool ok =
                sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
            sqlite3_finalize(statement);
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
    SqliteStore::trading_post_list(int character, const protocol::TradingPostRequest& request) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        return trading_post::list(
            [&](const auto& sql, const auto& args) { return m_impl->market_query(sql, args); },
            character,
            request);
    }

    TradingPostResult SqliteStore::trading_post_sell(const ItemRecord& item,
                                                     int quantity,
                                                     std::int64_t price,
                                                     const TradingPostCategory& category,
                                                     std::int32_t split_iid) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN IMMEDIATE")) {
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

    TradingPostResult SqliteStore::trading_post_buy(const CharacterRecord& buyer,
                                                    std::uint64_t listing,
                                                    int quantity,
                                                    std::int64_t price,
                                                    std::int32_t split_iid) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN IMMEDIATE")) {
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

    TradingPostResult SqliteStore::trading_post_cancel(int character, std::uint64_t listing) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN IMMEDIATE")) {
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
    TradingPostCollectionResult SqliteStore::trading_post_collect(const CharacterRecord& character,
                                                                  std::uint64_t listing,
                                                                  const ItemRecord* merge_target) {
        if (!m_impl || !m_impl->db || !m_impl->exec("BEGIN IMMEDIATE")) {
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

    bool SqliteStore::delete_item(std::int32_t iid) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare("DELETE FROM items WHERE iid = ?");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, iid);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    std::optional<ServerEventRecord> SqliteStore::load_server_event(const std::string& event_id) {
        if (!m_impl || !m_impl->db || event_id.empty()) {
            return std::nullopt;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "SELECT event_id, contribution_count, active_bonus_percent, active_until "
            "FROM server_events WHERE event_id = ?");
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_text(stmt, 1, event_id.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<ServerEventRecord> out;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            ServerEventRecord event;
            const unsigned char* id = sqlite3_column_text(stmt, 0);
            event.event_id = id ? reinterpret_cast<const char*>(id) : event_id;
            event.contribution_count = sqlite3_column_int64(stmt, 1);
            event.active_bonus_percent = sqlite3_column_int(stmt, 2);
            event.active_until = sqlite3_column_int64(stmt, 3);
            out = event;
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::save_server_event(const ServerEventRecord& event) {
        if (!m_impl || !m_impl->db || event.event_id.empty()) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "INSERT INTO server_events(event_id, contribution_count, active_bonus_percent, "
            "active_until) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(event_id) DO UPDATE SET "
            "contribution_count=excluded.contribution_count, "
            "active_bonus_percent=excluded.active_bonus_percent, "
            "active_until=excluded.active_until");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, event.event_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, event.contribution_count);
        sqlite3_bind_int(stmt, 3, event.active_bonus_percent);
        sqlite3_bind_int64(stmt, 4, event.active_until);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore save_server_event: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::optional<BattleMountRecord> SqliteStore::load_battle_mount(std::int32_t mount_iid) {
        if (!m_impl || !m_impl->db || mount_iid <= 0) {
            return std::nullopt;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("SELECT mount_iid, grade, satiety, max_satiety, revision, "
                            "ability0, ability1, ability2, ability3, ability4, ability5, "
                            "equipment0, equipment1, equipment2, equipment3, equipment4 "
                            "FROM battle_mounts WHERE mount_iid = ?");
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, mount_iid);
        std::optional<BattleMountRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            BattleMountRecord mount;
            mount.mount_iid = sqlite3_column_int(stmt, 0);
            mount.grade = static_cast<protocol::BattleRidingGrade>(
                std::clamp(sqlite3_column_int(stmt, 1), 0, 3));
            mount.max_satiety = std::max(1, sqlite3_column_int(stmt, 3));
            mount.satiety = std::clamp(sqlite3_column_int(stmt, 2), 0, mount.max_satiety);
            mount.revision = static_cast<std::uint32_t>(std::max(0, sqlite3_column_int(stmt, 4)));
            for (std::size_t i = 0; i < mount.abilities.size(); ++i) {
                mount.abilities[i] = sqlite3_column_int(stmt, 5 + static_cast<int>(i));
            }
            for (std::size_t i = 0; i < mount.equipment_iids.size(); ++i) {
                mount.equipment_iids[i] = sqlite3_column_int(stmt, 11 + static_cast<int>(i));
            }
            result = mount;
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool SqliteStore::upsert_battle_mount(const BattleMountRecord& mount) {
        if (!m_impl || !m_impl->db || mount.mount_iid <= 0 || mount.max_satiety <= 0 ||
            mount.satiety < 0 || mount.satiety > mount.max_satiety) {
            return false;
        }
        sqlite3_stmt* stmt =
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
        sqlite3_bind_int(stmt, 1, mount.mount_iid);
        sqlite3_bind_int(stmt, 2, static_cast<int>(mount.grade));
        sqlite3_bind_int(stmt, 3, mount.satiety);
        sqlite3_bind_int(stmt, 4, mount.max_satiety);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(mount.revision));
        for (std::size_t i = 0; i < mount.abilities.size(); ++i) {
            sqlite3_bind_int(stmt, 6 + static_cast<int>(i), mount.abilities[i]);
        }
        for (std::size_t i = 0; i < mount.equipment_iids.size(); ++i) {
            sqlite3_bind_int(stmt, 12 + static_cast<int>(i), mount.equipment_iids[i]);
        }
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore upsert_battle_mount: ") +
                       sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::delete_battle_mount(std::int32_t mount_iid) {
        if (!m_impl || !m_impl->db || mount_iid <= 0) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare("DELETE FROM battle_mounts WHERE mount_iid = ?");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, mount_iid);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    std::uint32_t SqliteStore::max_item_iid() {
        if (!m_impl || !m_impl->db) {
            return 0;
        }
        sqlite3_stmt* stmt = m_impl->prepare("SELECT COALESCE(MAX(iid), 0) FROM items");
        if (!stmt) {
            return 0;
        }
        std::uint32_t max_iid = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const sqlite3_int64 value = sqlite3_column_int64(stmt, 0);
            if (value > 0) {
                max_iid = static_cast<std::uint32_t>(value);
            }
        }
        sqlite3_finalize(stmt);
        return max_iid;
    }

    std::vector<SkillRecord> SqliteStore::load_skills(std::int32_t character_id) {
        std::vector<SkillRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "SELECT character_id, skill_index, level FROM skills WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SkillRecord s;
            s.character_id = sqlite3_column_int(stmt, 0);
            s.skill_index = sqlite3_column_int(stmt, 1);
            s.level = sqlite3_column_int(stmt, 2);
            out.push_back(s);
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::upsert_skill(const SkillRecord& skill) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (skill.level <= 0) {
            sqlite3_stmt* stmt =
                m_impl->prepare("DELETE FROM skills WHERE character_id = ? AND skill_index = ?");
            if (!stmt) {
                return false;
            }
            sqlite3_bind_int(stmt, 1, skill.character_id);
            sqlite3_bind_int(stmt, 2, skill.skill_index);
            const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            if (!ok) {
                log::error(std::string("SqliteStore upsert_skill delete: ") +
                           sqlite3_errmsg(m_impl->db));
            }
            sqlite3_finalize(stmt);
            return ok;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "INSERT INTO skills(character_id, skill_index, level) VALUES(?, ?, ?) "
            "ON CONFLICT(character_id, skill_index) DO UPDATE SET level=excluded.level");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, skill.character_id);
        sqlite3_bind_int(stmt, 2, skill.skill_index);
        sqlite3_bind_int(stmt, 3, skill.level);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore upsert_skill: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::optional<ShortcutRecord> SqliteStore::load_shortcuts(std::int32_t character_id) {
        if (!m_impl || !m_impl->db) {
            return std::nullopt;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "SELECT character_id, active_index, slot0, slot1, slot2, slot3, slot4, "
            "slot5, slot6, slot7, slot8, slot9, slot10, slot11, slot12, slot13, slot14, "
            "slot15, slot16, slot17, slot18, slot19 FROM shortcuts WHERE character_id = ?");
        if (!stmt) {
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        std::optional<ShortcutRecord> out;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            ShortcutRecord rec;
            rec.character_id = sqlite3_column_int(stmt, 0);
            rec.active_index = sqlite3_column_int(stmt, 1);
            for (std::size_t i = 0; i < kShortcutSlots; ++i) {
                rec.slots[i] = sqlite3_column_int(stmt, static_cast<int>(2 + i));
            }
            out = rec;
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::upsert_shortcuts(const ShortcutRecord& shortcuts) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
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
        sqlite3_bind_int(stmt, 1, shortcuts.character_id);
        sqlite3_bind_int(stmt, 2, shortcuts.active_index);
        for (std::size_t i = 0; i < kShortcutSlots; ++i) {
            sqlite3_bind_int(stmt, static_cast<int>(3 + i), shortcuts.slots[i]);
        }
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore upsert_shortcuts: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::vector<QuestFlagRecord> SqliteStore::load_quest_flags(std::int32_t character_id) {
        std::vector<QuestFlagRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("SELECT character_id, quest, flag, clear_flag FROM quest_flags "
                            "WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            QuestFlagRecord q;
            q.character_id = sqlite3_column_int(stmt, 0);
            q.quest = static_cast<std::uint16_t>(sqlite3_column_int(stmt, 1));
            q.flag = static_cast<std::uint8_t>(sqlite3_column_int(stmt, 2));
            q.clear = static_cast<std::uint8_t>(sqlite3_column_int(stmt, 3));
            out.push_back(q);
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::upsert_quest_flag(const QuestFlagRecord& flag) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "INSERT INTO quest_flags(character_id, quest, flag, clear_flag) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(character_id, quest) DO UPDATE SET flag=excluded.flag, "
            "clear_flag=excluded.clear_flag");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, flag.character_id);
        sqlite3_bind_int(stmt, 2, static_cast<int>(flag.quest));
        sqlite3_bind_int(stmt, 3, static_cast<int>(flag.flag));
        sqlite3_bind_int(stmt, 4, static_cast<int>(flag.clear));
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore upsert_quest_flag: ") + sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::update_system_mail(std::int32_t character_id,
                                         std::int64_t mail_id,
                                         bool remove,
                                         std::int64_t now) {
        if (!m_impl || !m_impl->db || character_id <= 0 || mail_id <= 0 || now <= 0) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            remove ? "UPDATE system_mail SET deleted_at = ? WHERE id = ? AND character_id = ? AND "
                     "deleted_at = 0 AND (claimed_at != 0 OR item_index = 0)"
                   : "UPDATE system_mail SET read_at = CASE WHEN read_at = 0 THEN ? ELSE read_at "
                     "END WHERE id = ? AND character_id = ? AND deleted_at = 0");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_int64(stmt, 2, mail_id);
        sqlite3_bind_int(stmt, 3, character_id);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool SqliteStore::ensure_system_mail(const SystemMailRecord& mail) {
        if (!m_impl || !m_impl->db || mail.character_id <= 0 || mail.source_key.empty() ||
            mail.item_index < 0 || mail.item_count < 0 ||
            ((mail.item_index == 0) != (mail.item_count == 0))) {
            return false;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "INSERT INTO system_mail(character_id, source_key, subject, body, item_index, "
            "item_count, created_at) VALUES(?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, source_key) DO NOTHING");
        if (!stmt) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, mail.character_id);
        sqlite3_bind_text(stmt, 2, mail.source_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, mail.subject.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, mail.body.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, mail.item_index);
        sqlite3_bind_int(stmt, 6, mail.item_count);
        sqlite3_bind_int64(stmt, 7, mail.created_at);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            log::error(std::string("SqliteStore ensure_system_mail: ") +
                       sqlite3_errmsg(m_impl->db));
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::vector<SystemMailRecord> SqliteStore::list_system_mail(std::int32_t character_id) {
        std::vector<SystemMailRecord> out;
        if (!m_impl || !m_impl->db || character_id <= 0) {
            return out;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "SELECT id, character_id, source_key, subject, body, item_index, item_count, "
            "created_at, claimed_at, read_at FROM system_mail WHERE character_id = ? AND "
            "deleted_at = 0 "
            "ORDER BY created_at DESC, id DESC");
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SystemMailRecord mail;
            mail.id = sqlite3_column_int64(stmt, 0);
            mail.character_id = sqlite3_column_int(stmt, 1);
            const auto* source_key = sqlite3_column_text(stmt, 2);
            const auto* subject = sqlite3_column_text(stmt, 3);
            const auto* body = sqlite3_column_text(stmt, 4);
            mail.source_key = source_key ? reinterpret_cast<const char*>(source_key) : "";
            mail.subject = subject ? reinterpret_cast<const char*>(subject) : "";
            mail.body = body ? reinterpret_cast<const char*>(body) : "";
            mail.item_index = sqlite3_column_int(stmt, 5);
            mail.item_count = sqlite3_column_int(stmt, 6);
            mail.created_at = sqlite3_column_int64(stmt, 7);
            mail.claimed_at = sqlite3_column_int64(stmt, 8);
            mail.read_at = sqlite3_column_int64(stmt, 9);
            out.push_back(std::move(mail));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    SystemMailClaimResult SqliteStore::claim_system_mail(std::int32_t character_id,
                                                         std::int64_t mail_id,
                                                         const ItemRecord& item,
                                                         std::int64_t claimed_at) {
        if (!m_impl || !m_impl->db || character_id <= 0 || mail_id <= 0 || item.iid <= 0 ||
            claimed_at <= 0) {
            return SystemMailClaimResult::Error;
        }
        if (!m_impl->exec("BEGIN IMMEDIATE TRANSACTION")) {
            return SystemMailClaimResult::Error;
        }
        auto rollback = [&] { (void) m_impl->exec("ROLLBACK"); };

        sqlite3_stmt* select =
            m_impl->prepare("SELECT item_index, item_count, claimed_at FROM system_mail "
                            "WHERE id = ? AND character_id = ? AND deleted_at = 0");
        if (!select) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        sqlite3_bind_int64(select, 1, mail_id);
        sqlite3_bind_int(select, 2, character_id);
        const int select_rc = sqlite3_step(select);
        if (select_rc != SQLITE_ROW) {
            sqlite3_finalize(select);
            rollback();
            return select_rc == SQLITE_DONE ? SystemMailClaimResult::NotFound
                                            : SystemMailClaimResult::Error;
        }
        const int item_index = sqlite3_column_int(select, 0);
        const int item_count = sqlite3_column_int(select, 1);
        const std::int64_t existing_claimed_at = sqlite3_column_int64(select, 2);
        sqlite3_finalize(select);
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

        sqlite3_stmt* update =
            m_impl->prepare("UPDATE system_mail SET claimed_at = ? "
                            "WHERE id = ? AND character_id = ? AND claimed_at = 0");
        if (!update) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        sqlite3_bind_int64(update, 1, claimed_at);
        sqlite3_bind_int64(update, 2, mail_id);
        sqlite3_bind_int(update, 3, character_id);
        const bool updated =
            sqlite3_step(update) == SQLITE_DONE && sqlite3_changes(m_impl->db) == 1;
        sqlite3_finalize(update);
        if (!updated || !m_impl->exec("COMMIT")) {
            rollback();
            return SystemMailClaimResult::Error;
        }
        return SystemMailClaimResult::Claimed;
    }

    std::vector<BuffRemainRecord> SqliteStore::load_buff_remains(std::int32_t character_id) {
        std::vector<BuffRemainRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        sqlite3_stmt* stmt = m_impl->prepare(
            "SELECT character_id, buff_type, remain, is_count, stack_pack, source_item_index "
            "FROM buff_remain "
            "WHERE character_id = ?");
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BuffRemainRecord row;
            row.character_id = sqlite3_column_int(stmt, 0);
            row.buff_type = sqlite3_column_int(stmt, 1);
            row.remain = sqlite3_column_int(stmt, 2);
            row.is_count = sqlite3_column_int(stmt, 3);
            row.stack_pack = sqlite3_column_int(stmt, 4);
            row.source_item_index = sqlite3_column_int(stmt, 5);
            out.push_back(row);
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::replace_buff_remains(std::int32_t character_id,
                                           const std::vector<BuffRemainRecord>& rows) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        sqlite3_stmt* del = m_impl->prepare("DELETE FROM buff_remain WHERE character_id = ?");
        if (!del) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        sqlite3_bind_int(del, 1, character_id);
        const bool deleted = sqlite3_step(del) == SQLITE_DONE;
        sqlite3_finalize(del);
        if (!deleted) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        for (const auto& row : rows) {
            sqlite3_stmt* ins = m_impl->prepare(
                "INSERT INTO buff_remain(character_id, buff_type, remain, is_count, stack_pack, "
                "source_item_index) VALUES(?, ?, ?, ?, ?, ?)");
            if (!ins) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
            sqlite3_bind_int(ins, 1, character_id);
            sqlite3_bind_int(ins, 2, row.buff_type);
            sqlite3_bind_int(ins, 3, row.remain);
            sqlite3_bind_int(ins, 4, row.is_count);
            sqlite3_bind_int(ins, 5, row.stack_pack);
            sqlite3_bind_int(ins, 6, row.source_item_index);
            const bool ok = sqlite3_step(ins) == SQLITE_DONE;
            sqlite3_finalize(ins);
            if (!ok) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
        }
        return m_impl->exec("COMMIT");
    }

    std::vector<InventoryUnlockRecord>
    SqliteStore::load_inventory_unlocks(std::int32_t character_id) {
        std::vector<InventoryUnlockRecord> out;
        if (!m_impl || !m_impl->db) {
            return out;
        }
        sqlite3_stmt* stmt =
            m_impl->prepare("SELECT character_id, register_index, remain FROM inventory_unlock "
                            "WHERE character_id = ? ORDER BY register_index ASC");
        if (!stmt) {
            return out;
        }
        sqlite3_bind_int(stmt, 1, character_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            InventoryUnlockRecord row;
            row.character_id = sqlite3_column_int(stmt, 0);
            row.register_index = sqlite3_column_int(stmt, 1);
            row.remain = sqlite3_column_int(stmt, 2);
            out.push_back(row);
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool SqliteStore::replace_inventory_unlocks(std::int32_t character_id,
                                                const std::vector<InventoryUnlockRecord>& rows) {
        if (!m_impl || !m_impl->db) {
            return false;
        }
        if (!m_impl->exec("BEGIN")) {
            return false;
        }
        sqlite3_stmt* del = m_impl->prepare("DELETE FROM inventory_unlock WHERE character_id = ?");
        if (!del) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        sqlite3_bind_int(del, 1, character_id);
        const bool deleted = sqlite3_step(del) == SQLITE_DONE;
        sqlite3_finalize(del);
        if (!deleted) {
            (void) m_impl->exec("ROLLBACK");
            return false;
        }
        for (const auto& row : rows) {
            sqlite3_stmt* ins = m_impl->prepare(
                "INSERT INTO inventory_unlock(character_id, register_index, remain) "
                "VALUES(?, ?, ?)");
            if (!ins) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
            sqlite3_bind_int(ins, 1, character_id);
            sqlite3_bind_int(ins, 2, row.register_index);
            sqlite3_bind_int(ins, 3, row.remain);
            const bool ok = sqlite3_step(ins) == SQLITE_DONE;
            sqlite3_finalize(ins);
            if (!ok) {
                (void) m_impl->exec("ROLLBACK");
                return false;
            }
        }
        return m_impl->exec("COMMIT");
    }

    bool SqliteStore::ensure_default_account(const std::string& account_name,
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

    bool SqliteStore::ensure_default_player(const std::string& account_name,
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

    std::unique_ptr<IDataStore> IDataStore::create(StoreBackend backend) {
        switch (backend) {
        case StoreBackend::Postgres:
#if defined(OPENKAL_WITH_POSTGRES)
            return std::make_unique<PostgresStore>();
#else
            log::error("PostgreSQL support was not enabled at build time");
            return nullptr;
#endif
        case StoreBackend::Sqlite:
            return std::make_unique<SqliteStore>();
        case StoreBackend::Mysql:
            log::error("MySQL data store is not implemented yet");
            return nullptr;
        }
        return nullptr;
    }

    std::unique_ptr<IDataStore> create_data_store(StoreBackend backend) {
        return IDataStore::create(backend);
    }

}  // namespace openkal::server
