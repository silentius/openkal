/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/LobbyService.h"

#include "openkal/protocol/CharacterName.h"
#include "openkal/server/AccountPassword.h"
#include "openkal/server/Combat.h"
#include "openkal/server/Packets.h"
#if defined(OPENKAL_WITH_POSTGRES)
#include "openkal/server/store/PostgresStore.h"
#endif
#include <algorithm>
#include <chrono>
#include <future>

namespace openkal::server {
    struct PendingLobbyCredentials {
        LobbyOperation operation = LobbyOperation::Login;
        std::string name;
        std::string email;
        std::optional<AccountRecord> account;
        std::future<std::string> result;
    };

    namespace {
        std::int64_t now_seconds() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }
    }  // namespace
    LobbyService::LobbyService(IDataStore& store,
                               const ServerConfig& config,
                               const std::unordered_map<int, ItemCombatDef>& items,
                               std::atomic<std::uint32_t>& next)
        : m_store(store), m_config(config), m_items(items), m_next_iid(next) {
        if (config.multiplayer) {
            (void) m_starters.load_dir(config.starters_dir);
            m_dummy_hash = hash_account_password("OpenKal dummy password, never an account");
        }
    }
    bool LobbyService::create_character(const CharacterRecord& draft, std::int32_t account) {
#if defined(OPENKAL_WITH_POSTGRES)
        auto* pg = dynamic_cast<PostgresStore*>(&m_store);
        constexpr int bases[4][5] = {
            {18, 16, 8, 8, 10}, {8, 10, 18, 16, 8}, {14, 10, 8, 10, 18}, {14, 10, 8, 10, 18}};
        if (!pg || draft.id != 0 || !protocol::character_name_valid(draft.name) ||
            draft.clazz < 0 || draft.clazz > 3 || draft.face < 1 || draft.face > 255 ||
            draft.hair < 1 || draft.hair > 255) {
            return false;
        }
        const int stats[] = {draft.str, draft.hth, draft.intl, draft.wis, draft.dex};
        int spent = 0;
        for (int i = 0; i < 5; ++i) {
            const auto delta = static_cast<std::int64_t>(stats[i]) - bases[draft.clazz][i];
            if (delta < 0 || delta > 5) {
                return false;
            }
            spent += static_cast<int>(delta);
        }
        if (spent != 5) {
            return false;
        }
        if (!pg->execute("BEGIN; LOCK TABLE characters IN SHARE ROW EXCLUSIVE MODE")) {
            return false;
        }
        struct Rollback {
            PostgresStore& store;
            bool committed = false;
            ~Rollback() {
                if (!committed) {
                    (void) store.execute("ROLLBACK");
                }
            }
        } tx{*pg};
        if (m_store.list_characters(account).size() >= kMaxCharactersPerAccount) {
            return false;
        }
        CharacterRecord c;
        c.account_id = account;
        c.name = draft.name;
        c.clazz = draft.clazz;
        c.face = draft.face;
        c.hair = draft.hair;
        c.str = draft.str;
        c.hth = draft.hth;
        c.intl = draft.intl;
        c.wis = draft.wis;
        c.dex = draft.dex;
        c.hp = c.max_hp = combat::max_hp_from_stats(c.hth, 1, c.clazz);
        c.mp = c.max_mp = combat::max_mp_from_stats(c.wis, 1, c.clazz);
        m_starters.apply_spawn(c);
        if (!m_store.save_character(c)) {
            return false;
        }
        for (const auto& row : m_store.list_characters(account)) {
            if (row.name == c.name) {
                c.id = row.id;
                break;
            }
        }
        if (c.id <= 0) {
            return false;
        }
        const auto& starter_rows = m_starters.items_for_class(c.clazz);
        for (const auto& row : starter_rows) {
            if (row.item_id != kMoneyItemIndex && m_items.find(row.item_id) == m_items.end()) {
                return false;
            }
        }
        if ((!starter_rows.empty() || m_starters.has_spawn()) &&
            !m_starters.grant_if_empty(m_store, c, m_items, &m_next_iid)) {
            return false;
        }
        if (!pg->execute("COMMIT")) {
            return false;
        }
        tx.committed = true;
        return true;
#else
        (void) draft;
        (void) account;
        return false;
#endif
    }
    std::vector<std::uint8_t> LobbyService::handle(const std::vector<std::uint8_t>& payload,
                                                   LobbyIdentity& identity,
                                                   bool& malformed) {
        protocol::PacketReader r(payload);
        std::uint8_t raw = 255;
        malformed = !r.read_u8(raw) || raw > static_cast<std::uint8_t>(LobbyOperation::Items);
        const auto op = static_cast<LobbyOperation>(raw);
        auto reply = [&](bool ok,
                         std::string_view message,
                         const protocol::PacketWriter& data = protocol::PacketWriter{}) {
            protocol::PacketWriter w;
            w.write_u8(raw);
            w.write_u8(ok ? 1 : 0);
            w.write_string(message);
            w.write_bytes(data.payload().data(), data.payload().size());
            return w.frame(kLobbyResponse);
        };
        const auto now = now_seconds();
        if (now - identity.window_started >= 60) {
            identity.requests = 0;
            identity.window_started = now;
            identity.credential_attempts = 0;
        }
        if (!m_config.multiplayer || ++identity.requests > 120 || malformed) {
            return reply(false, "Lobby request rejected.");
        }
        if (op == LobbyOperation::Capabilities) {
            if (r.remaining()) {
                malformed = true;
                return reply(false, "Invalid request.");
            }
            protocol::PacketWriter data;
            data.write_u8(1);
            data.write_u8(m_config.allow_client_registration ? 1 : 0);
            return reply(true, "", data);
        }
        if (op == LobbyOperation::Register || op == LobbyOperation::Login) {
            if (identity.account_id) {
                malformed = true;
                return reply(false, "Already signed in.");
            }
            if (now - m_window_started >= 60) {
                m_window_started = now;
                m_credentials = 0;
            }
            if (++identity.credential_attempts > 6 || ++m_credentials > 30) {
                return reply(false, "Too many attempts. Try again in a minute.");
            }
            if (op == LobbyOperation::Register && !m_config.allow_client_registration) {
                return reply(false, "Client registration is disabled on this server.");
            }
            std::string name, email, password;
            if (!r.read_string(name) || name.size() > 32) {
                return reply(false, "Invalid account details.");
            }
            if (op == LobbyOperation::Register && (!r.read_string(email) || email.size() > 254)) {
                return reply(false, "Invalid email address.");
            }
            if (!r.read_string(password) || r.remaining() || !valid_account_password(password)) {
                return reply(false,
                             "Use a password with 12 to 128 bytes and no control characters.");
            }
            name = normalize_account_name(name);
            if (name.empty()) {
                return reply(false, "Use 3 to 32 letters, digits or underscores for the username.");
            }
            if (op == LobbyOperation::Register) {
                email = normalize_account_email(email);
                if (email.empty()) {
                    return reply(false, "Enter a valid email address.");
                }
            }
            m_jobs.erase(std::remove_if(m_jobs.begin(),
                                        m_jobs.end(),
                                        [](const auto& job) {
                                            return job->result.valid() &&
                                                   job->result.wait_for(std::chrono::seconds(0)) ==
                                                       std::future_status::ready &&
                                                   job.use_count() == 1;
                                        }),
                         m_jobs.end());
            if (m_jobs.size() >= 2) {
                return reply(false, "Account service is busy. Try again shortly.");
            }
            auto pending = std::make_shared<PendingLobbyCredentials>();
            pending->operation = op;
            pending->name = name;
            pending->email = email;
            if (op == LobbyOperation::Login) {
                pending->account = m_store.find_account(name);
            }
            const auto stored = pending->account ? pending->account->password : m_dummy_hash;
            pending->result =
                std::async(std::launch::async, [op, password = std::move(password), stored] {
                    if (op == LobbyOperation::Register) {
                        return hash_account_password(password);
                    }
                    return verify_account_password(password, stored) ? std::string("verified")
                                                                     : std::string{};
                });
            identity.pending = pending;
            m_jobs.push_back(std::move(pending));
            return {};
        }
        if (!identity.account_id) {
            return reply(false, "Sign in first.");
        }
        if (op == LobbyOperation::Characters || op == LobbyOperation::Deleted) {
            if (r.remaining()) {
                malformed = true;
                return reply(false, "Invalid request.");
            }
            const auto rows = op == LobbyOperation::Characters
                                  ? m_store.list_characters(identity.account_id)
                                  : m_store.list_deleted_characters(identity.account_id, now);
            if (rows.size() > kMaxCharactersPerAccount) {
                return reply(false, "Invalid character count.");
            }
            protocol::PacketWriter data;
            data.write_u8(static_cast<std::uint8_t>(rows.size()));
            for (const auto& row : rows) {
                write_lobby_character(data, row);
            }
            return reply(true, "", data);
        }
        if (op == LobbyOperation::Create) {
            CharacterRecord draft;
            malformed = !read_lobby_character(r, draft) || r.remaining() != 0;
            if (malformed || !create_character(draft, identity.account_id)) {
                return reply(false,
                             "Character could not be created. Check its name, attributes and "
                             "available slots.");
            }
            return reply(true, "");
        }
        std::int32_t id = 0;
        if (!r.read_i32(id) || r.remaining() || id <= 0) {
            malformed = true;
            return reply(false, "Invalid character.");
        }
        if (op == LobbyOperation::Restore) {
            if (!m_store.restore_character(id, identity.account_id, now)) {
                return reply(false, "Character cannot be restored.");
            }
            return reply(true, "");
        }
        const auto character = m_store.load_character(id);
        if (!character || character->account_id != identity.account_id) {
            return reply(false, "Character is unavailable.");
        }
        if (op == LobbyOperation::Delete) {
            if (!m_store.schedule_character_deletion(id, identity.account_id, now)) {
                return reply(false, "Character cannot be deleted.");
            }
            identity.character_id = 0;
            return reply(true, "");
        }
        if (op == LobbyOperation::Items) {
            auto items = m_store.load_items(id);
            items.erase(std::remove_if(items.begin(),
                                       items.end(),
                                       [](const ItemRecord& item) { return item.slot <= 0; }),
                        items.end());
            if (items.size() > 20) {
                return reply(false, "Invalid preview equipment.");
            }
            protocol::PacketWriter data;
            data.write_u8(static_cast<std::uint8_t>(items.size()));
            for (const auto& item : items) {
                write_lobby_item(data, item);
            }
            return reply(true, "", data);
        }
        if (op == LobbyOperation::Select) {
            identity.character_id = id;
            protocol::PacketWriter data;
            write_lobby_character(data, *character);
            return reply(true, "", data);
        }
        return reply(false, "Unknown lobby operation.");
    }
    std::vector<std::uint8_t> LobbyService::poll(LobbyIdentity& identity,
                                                 const std::function<bool(std::int32_t)>& busy) {
        m_jobs.erase(std::remove_if(m_jobs.begin(),
                                    m_jobs.end(),
                                    [](const auto& job) {
                                        return job.use_count() == 1 && job->result.valid() &&
                                               job->result.wait_for(std::chrono::seconds(0)) ==
                                                   std::future_status::ready;
                                    }),
                     m_jobs.end());
        if (!identity.pending || identity.pending->result.wait_for(std::chrono::seconds(0)) !=
                                     std::future_status::ready) {
            return {};
        }
        auto pending = std::move(identity.pending);
        auto reply = [&](bool ok,
                         std::string_view message,
                         const protocol::PacketWriter& data = protocol::PacketWriter{}) {
            protocol::PacketWriter writer;
            writer.write_u8(static_cast<std::uint8_t>(pending->operation));
            writer.write_u8(ok ? 1 : 0);
            writer.write_string(message);
            writer.write_bytes(data.payload().data(), data.payload().size());
            return writer.frame(kLobbyResponse);
        };
        m_jobs.erase(std::remove(m_jobs.begin(), m_jobs.end(), pending), m_jobs.end());
        std::string result;
        try {
            result = pending->result.get();
        } catch (...) {
            return reply(false, "Account service failed. Try again.");
        }
        if (pending->operation == LobbyOperation::Register) {
            if (!m_config.allow_client_registration) {
                return reply(false, "Client registration is disabled on this server.");
            }
#if defined(OPENKAL_WITH_POSTGRES)
            auto* postgres = dynamic_cast<PostgresStore*>(&m_store);
            if (!postgres || result.empty() ||
                !postgres->register_account(pending->name, pending->email, result)) {
                return reply(
                    false,
                    "Account could not be created. Username or email may already be in use.");
            }
            return reply(true, "Account created. You can now sign in.");
#else
            return reply(false, "Online accounts require PostgreSQL support.");
#endif
        }
        const auto current = m_store.find_account(pending->name);
        if (result.empty() || !pending->account || !current ||
            current->id != pending->account->id ||
            current->password != pending->account->password) {
            return reply(false, "Incorrect username or password.");
        }
        const auto discipline = m_store.load_account_discipline(current->id);
        if (!discipline) {
            return reply(false, "Account service failed. Try again.");
        }
        if (discipline->banned) {
            return reply(false, "This account is banned.");
        }
        if (busy(current->id)) {
            return reply(false, "This account is already signed in.");
        }
        identity.account_id = current->id;
        protocol::PacketWriter data;
        data.write_i32(current->id);
        return reply(true, "", data);
    }
}  // namespace openkal::server
