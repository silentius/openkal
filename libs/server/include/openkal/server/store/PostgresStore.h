/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/server/CastleWarStore.h"
#include "openkal/server/GuildStore.h"
#include "openkal/server/store/DataStore.h"

namespace openkal::server {

    /** PostgreSQL persistence for one dedicated multiplayer world. */
    class PostgresStore final : public IDataStore, public IGuildStore, public ICastleWarStore {
      public:
        /** @copydoc IDataStore::grant_starter_items */
        [[nodiscard]] bool grant_starter_items(const CharacterRecord& character,
                                               const std::vector<ItemRecord>& items) override;

        /** Constructs a closed PostgreSQL store. */
        PostgresStore();

        /** @return This multiplayer transaction boundary. */
        IGuildStore* guild_store() override {
            return this;
        }

        /** @copydoc IGuildStore::load_guilds */
        [[nodiscard]] bool load_guilds(platform::Json& state) override;

        /** @copydoc IGuildStore::commit_guilds */
        [[nodiscard]] bool commit_guilds(std::uint64_t previous,
                                         const platform::Json& state,
                                         const std::vector<CharacterRecord>& characters) override;

        /** @copydoc IDataStore::castle_war_store */
        ICastleWarStore* castle_war_store() override {
            return this;
        }

        /** @copydoc ICastleWarStore::load_castle_war */
        [[nodiscard]] bool load_castle_war(platform::Json& state) override;

        /** @copydoc ICastleWarStore::commit_castle_war */
        [[nodiscard]] bool commit_castle_war(std::uint64_t expected_revision,
                                             const platform::Json& state,
                                             const std::vector<HonorGrant>& awards = {}) override;

        /** @copydoc IDataStore::load_honor */
        [[nodiscard]] std::optional<protocol::HonorStatus>
        load_honor(std::int32_t character_id, std::int64_t week_start) override;

        /** @copydoc IDataStore::exchange_honor_rewards */
        [[nodiscard]] HonorExchangeResult
        exchange_honor_rewards(const HonorRewardExchange& exchange,
                               const std::vector<ItemRecord>& expected,
                               const std::vector<ItemRecord>& replacements) override;

        /** @copydoc IDataStore::grant_honor */
        [[nodiscard]] bool grant_honor(const std::vector<HonorGrant>& awards) override;

        /** @copydoc IDataStore::close_honor_week */
        [[nodiscard]] bool close_honor_week(std::int64_t week_start) override;

        /** @copydoc IDataStore::honor_revision */
        [[nodiscard]] std::uint64_t honor_revision() const override {
            return m_honor_revision;
        }

        /** Closes the database. */
        ~PostgresStore() override;

        PostgresStore(const PostgresStore&) = delete;
        PostgresStore& operator=(const PostgresStore&) = delete;

        /** Opens a PostgreSQL database.
         * @param path_or_dsn Server-only libpq connection string.
         * @return True when connection and schema validation succeed.
         */
        [[nodiscard]] bool open(const std::string& path_or_dsn) override;

        /** Closes the database.
         */
        void close() override;

        /** Finds an account by name.
         * @param name Account username.
         * @return Matching record, or nullopt when absent or the query fails.
         */
        [[nodiscard]] std::optional<AccountRecord> find_account(const std::string& name) override;

        /** Inserts or updates an account.
         * @param account Account record.
         * @return True on success.
         */
        [[nodiscard]] bool upsert_account(const AccountRecord& account) override;

        [[nodiscard]] std::optional<AccountDiscipline>
        load_account_discipline(std::int32_t account_id) override;

        [[nodiscard]] std::optional<AccountDiscipline>
        update_account_discipline(std::int32_t account_id, AccountDisciplineChange change) override;

        /** Lists active characters owned by an account.
         * @param account_id Owning account identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<CharacterRecord>
        list_characters(std::int32_t account_id) override;

        /** Lists recoverable deleted characters owned by an account.
         * @param account_id Owning account identifier.
         * @param now_seconds Current Unix time in seconds.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<CharacterRecord>
        list_deleted_characters(std::int32_t account_id, std::int64_t now_seconds) override;

        /** Loads a character by identifier.
         * @param id Character identifier.
         * @return Matching record, or nullopt when absent or the query fails.
         */
        [[nodiscard]] std::optional<CharacterRecord> load_character(std::int32_t id) override;

        /** Saves a character.
         * @param character Character snapshot.
         * @return True on success.
         */
        [[nodiscard]] bool save_character(const CharacterRecord& character) override;

        /** Marks a character for deletion after the recovery window.
         * @param id Character identifier.
         * @param account_id Owning account identifier.
         * @param now_seconds Current Unix time in seconds.
         * @return True on success.
         */
        [[nodiscard]] bool schedule_character_deletion(std::int32_t id,
                                                       std::int32_t account_id,
                                                       std::int64_t now_seconds) override;

        /** Restores a recoverable character.
         * @param id Character identifier.
         * @param account_id Owning account identifier.
         * @param now_seconds Current Unix time in seconds.
         * @return True on success.
         */
        [[nodiscard]] bool restore_character(std::int32_t id,
                                             std::int32_t account_id,
                                             std::int64_t now_seconds) override;

        /** Performs one bounded expired-character cleanup step.
         * @param now_seconds Current Unix time in seconds.
         * @param row_budget Maximum dependent rows processed in one step.
         * @return Outcome of the operation.
         */
        [[nodiscard]] CharacterPurgeResult
        process_character_deletions(std::int64_t now_seconds, std::size_t row_budget) override;

        /** Loads inventory rows.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<ItemRecord> load_items(std::int32_t character_id) override;
        [[nodiscard]] std::vector<ItemRecord> load_storage_items(std::int32_t account_id) override;

        /** @copydoc IDataStore::load_storage_money */
        [[nodiscard]] std::optional<std::int64_t>
        load_storage_money(std::int32_t account_id) override;

        /** @copydoc IDataStore::transfer_storage_money */
        [[nodiscard]] std::optional<std::int64_t>
        transfer_storage_money(std::int32_t account_id,
                               std::int32_t character_id,
                               std::int64_t wallet_after,
                               std::int64_t change) override;

        /** Inserts one inventory row.
         * @param item Inventory item state.
         * @return True on success.
         */
        [[nodiscard]] bool insert_item(const ItemRecord& item) override;

        /** Updates one inventory row.
         * @param item Inventory item state.
         * @return True on success.
         */
        [[nodiscard]] bool update_item(const ItemRecord& item) override;

        /** @copydoc IDataStore::update_equipment_items */
        [[nodiscard]] bool update_equipment_items(const std::vector<ItemRecord>& items) override;

        /** Implements consume item with rewards.
         * @param box Expected consumable item state.
         * @param rewards Reward items inserted atomically.
         * @return True on success.
         */
        [[nodiscard]] bool
        consume_item_with_rewards(const ItemRecord& box,
                                  const std::vector<ItemRecord>& rewards) override;

        /** @copydoc IDataStore::commit_assassin_death */
        [[nodiscard]] bool commit_assassin_death(const CharacterRecord& character,
                                                 const std::vector<ItemRecord>& items,
                                                 const std::string& last_hitter) override;

        /** @copydoc IDataStore::load_assassin_list */
        [[nodiscard]] std::optional<std::vector<protocol::AssassinListEntry>>
        load_assassin_list() override;

        /** @copydoc IDataStore::commit_trade */
        [[nodiscard]] bool commit_trade(const CharacterRecord& first,
                                        const CharacterRecord& second,
                                        const std::vector<TradeTransfer>& items) override;

        /** @copydoc IDataStore::exchange_inventory_items */
        [[nodiscard]] bool
        exchange_inventory_items(const std::vector<ItemRecord>& expected,
                                 const std::vector<ItemRecord>& replacements) override;

        /** Deletes one inventory row. @param iid Instance ID. @return Whether deletion succeeded.
         */

        [[nodiscard]] std::optional<protocol::TradingPostState>
        trading_post_list(int character, const protocol::TradingPostRequest& request) override;
        [[nodiscard]] TradingPostCollectionResult
        trading_post_collect(const CharacterRecord& character,
                             std::uint64_t receipt,
                             const ItemRecord* merge_target = nullptr) override;
        [[nodiscard]] TradingPostResult trading_post_sell(const ItemRecord& item,
                                                          int quantity,
                                                          std::int64_t unit_price,
                                                          const TradingPostCategory& category,
                                                          std::int32_t split_iid = 0) override;
        [[nodiscard]] TradingPostResult trading_post_buy(const CharacterRecord& buyer,
                                                         std::uint64_t listing,
                                                         int quantity,
                                                         std::int64_t unit_price,
                                                         std::int32_t split_iid = 0) override;
        [[nodiscard]] TradingPostResult trading_post_cancel(int character,
                                                            std::uint64_t listing) override;

        [[nodiscard]] bool delete_item(std::int32_t iid) override;

        [[nodiscard]] std::optional<ServerEventRecord>
        load_server_event(const std::string& event_id) override;

        [[nodiscard]] bool save_server_event(const ServerEventRecord& event) override;

        /** Loads one Battle Riding state row.
         * @param mount_iid Mount item instance identifier.
         * @return Matching record, or nullopt when absent or the query fails.
         */
        [[nodiscard]] std::optional<BattleMountRecord>
        load_battle_mount(std::int32_t mount_iid) override;

        /** Upserts one Battle Riding state row.
         * @param mount Persistent mount state.
         * @return True on success.
         */
        [[nodiscard]] bool upsert_battle_mount(const BattleMountRecord& mount) override;

        /** Deletes one Battle Riding state row.
         * @param mount_iid Mount item instance identifier.
         * @return True on success.
         */
        [[nodiscard]] bool delete_battle_mount(std::int32_t mount_iid) override;

        /** Highest persisted item iid (DBSvr SELECT MAX(IID) at MainSvr connect).
         * @return Highest stored item instance id, or zero when empty.
         */
        [[nodiscard]] std::uint32_t max_item_iid() override;

        /** Loads learned skills.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<SkillRecord> load_skills(std::int32_t character_id) override;

        /** Upserts one skill. Level ≤ 0 deletes the row.
         * @param skill Learned skill state.
         * @return True on success.
         */
        [[nodiscard]] bool upsert_skill(const SkillRecord& skill) override;

        /** Loads the saved skill bar.
         * @param character_id Owning character identifier.
         * @return Matching record, or nullopt when absent or the query fails.
         */
        [[nodiscard]] std::optional<ShortcutRecord>
        load_shortcuts(std::int32_t character_id) override;

        /** Upserts the skill bar.
         * @param shortcuts Saved skill bar.
         * @return True on success.
         */
        [[nodiscard]] bool upsert_shortcuts(const ShortcutRecord& shortcuts) override;

        /** Loads quest flags.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<QuestFlagRecord>
        load_quest_flags(std::int32_t character_id) override;

        /** Upserts one quest flag.
         * @param flag Persistent quest state.
         * @return True on success.
         */
        [[nodiscard]] bool upsert_quest_flag(const QuestFlagRecord& flag) override;

        /** Implements update system mail.
         * @param character_id Owning character identifier.
         * @param mail_id Durable message identifier.
         * @param remove True to hide a claimed message; false to mark it read.
         * @param now Current Unix time.
         * @return True on success.
         */
        [[nodiscard]] bool update_system_mail(std::int32_t character_id,
                                              std::int64_t mail_id,
                                              bool remove,
                                              std::int64_t now) override;

        /** Implements ensure system mail.
         * @param mail Idempotent system message.
         * @return True on success.
         */
        [[nodiscard]] bool ensure_system_mail(const SystemMailRecord& mail) override;

        /** Lists all system mails for a character.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<SystemMailRecord>
        list_system_mail(std::int32_t character_id) override;

        /** Atomically claims one system-mail attachment.
         * @param character_id Owning character identifier.
         * @param mail_id Durable message identifier.
         * @param item Inventory item state.
         * @param claimed_at Successful claim Unix time.
         * @return Outcome of the operation.
         */
        [[nodiscard]] SystemMailClaimResult claim_system_mail(std::int32_t character_id,
                                                              std::int64_t mail_id,
                                                              const ItemRecord& item,
                                                              std::int64_t claimed_at) override;

        /** Loads pay-buff remains.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<BuffRemainRecord>
        load_buff_remains(std::int32_t character_id) override;

        /** Replaces pay-buff remains.
         * @param character_id Owning character identifier.
         * @param rows Replacement state rows.
         * @return True on success.
         */
        [[nodiscard]] bool replace_buff_remains(std::int32_t character_id,
                                                const std::vector<BuffRemainRecord>& rows) override;

        /** Loads active inventory-register unlocks.
         * @param character_id Owning character identifier.
         * @return Matching records; empty when absent or the query fails.
         */
        [[nodiscard]] std::vector<InventoryUnlockRecord>
        load_inventory_unlocks(std::int32_t character_id) override;

        /** Replaces active inventory-register unlocks.
         * @param character_id Owning character identifier.
         * @param rows Replacement state rows.
         * @return True on success.
         */
        [[nodiscard]] bool
        replace_inventory_unlocks(std::int32_t character_id,
                                  const std::vector<InventoryUnlockRecord>& rows) override;

        /** Ensures the local account exists.
         * @param account_name Compatibility account username.
         * @param password Stored credential value; online accounts require an encoded hash.
         * @return True on success.
         */
        [[nodiscard]] bool ensure_default_account(const std::string& account_name,
                                                  const std::string& password) override;

        /** Ensures the local account and fallback character exist.
         * @param account_name Compatibility account username.
         * @param password Stored credential value; online accounts require an encoded hash.
         * @param defaults Default character state.
         * @return True on success.
         */
        [[nodiscard]] bool ensure_default_player(const std::string& account_name,
                                                 const std::string& password,
                                                 const CharacterRecord& defaults) override;

        /** Creates a unique online account.
         * @param name Normalized username.
         * @param email Normalized email address.
         * @param hash Encoded password hash.
         * @return True on insertion; false on conflict or database error.
         */
        [[nodiscard]] bool register_account(const std::string& name,
                                            const std::string& email,
                                            const std::string& hash);

        /** Executes a transaction boundary or trusted schema command.
         * @param sql Trusted SQL without user interpolation.
         * @return True on success.
         */
        [[nodiscard]] bool execute(std::string_view sql);

        /** Prevents two authoritative servers from writing the same world.
         * @return True on success.
         */
        [[nodiscard]] bool acquire_server_lease();

        /** Checks whether the authoritative database remains reachable.
         * @return True when a bounded health query succeeds.
         */
        [[nodiscard]] bool ping();

      private:
        struct Impl;

        /** PostgreSQL connection and statement state. */
        std::unique_ptr<Impl> m_impl;

        /** Writes an inventory delta inside the caller's transaction. */
        [[nodiscard]] bool replace_inventory_rows(std::int32_t owner,
                                                  const std::vector<ItemRecord>& expected,
                                                  const std::vector<ItemRecord>& replacements);
        std::uint64_t m_honor_revision = 0;
    };

}  // namespace openkal::server
