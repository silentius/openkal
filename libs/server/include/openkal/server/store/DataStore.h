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

#include "openkal/protocol/Pvp.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/TradingPost.h"
#include "openkal/server/BattleRiding.h"
#include "openkal/server/Config.h"
#include "openkal/server/InventoryUnlock.h"
#include "openkal/server/PayBuff.h"
#include "openkal/server/UserRoles.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openkal::server {

    /** Persisted equipment slot for the active Assassin Mask. */
    inline constexpr int kAssassinMaskSlot = 7;
    class ICastleWarStore;
    class IGuildStore;

    /** Reserved non-login system account that owns active trading post escrow. */
    inline constexpr std::int32_t kTradingPostAccountId = -1;

    /** Persistent enforcement state for one account. */
    struct AccountDiscipline {
        int points = 0;
        int kicks = 0;
        bool banned = false;
    };

    /** A single change to an account's protocol violation points. */
    enum class AccountDisciplineChange { Violation, Decay };

    /** Persistent account record. */
    struct AccountRecord {
        std::int32_t id = 0;
        std::string name;
        std::string password;  // plaintext for local slice only; SP uses
                               // kDefaultAccountPassword
    };

    /** Sole local single-player account name. */
    inline constexpr const char* kDefaultAccountName = "DefaultAccount";

    /** Sole local single-player protocol password. */
    inline constexpr const char* kDefaultAccountPassword = "DefaultAccount";

    /** Default SQLite file name under a save directory. */
    inline constexpr const char* kSqliteFileName = "openkal.sqlite";

    /** Maximum number of active characters owned by one account. */
    inline constexpr std::size_t kMaxCharactersPerAccount = 8;

    /** Maximum number of recoverable deleted characters, matching the list. */
    inline constexpr std::size_t kMaxDeletedCharactersPerAccount = 5;

    /** Character recovery window in seconds (30 days). */
    inline constexpr std::int64_t kCharacterDeletionGraceSeconds = 30LL * 24LL * 60LL * 60LL;

    /** Persistent character-deletion lifecycle. */
    enum class CharacterDeletionState : std::uint8_t {
        Active = 0,
        Recoverable = 1,
        Purging = 2,
    };

    /** Result of one bounded expired-character cleanup step. */
    enum class CharacterPurgeResult : std::uint8_t {
        Idle = 0,
        WorkRemaining,
        CharacterPurged,
        Error,
    };

    /** Persistent character record (SaveAllProperty subset plus OpenKal extras). */
    struct CharacterRecord {
        std::int32_t id = 0;
        std::int32_t account_id = 0;
        std::string name;
        /** Server-owned role; ordinary saves preserve the existing database value. */
        UserRole role = UserRole::Player;

        std::int32_t clazz = 0;  // 0 knight / 1 mage / 2 archer / 3 thief
        /** Natural appearance, as one-based clothes mesh indices. */
        std::int32_t face = 1;
        std::int32_t hair = 1;
        /** Logical map (0 = overworld, matches compatibility behavior). */
        int map = 0;
        int map_x = 257285;
        int map_y = 258744;
        int map_z = 16100;
        /** Saved Black Crow statue point (0 = Narootuh). */
        std::int32_t revival_point = 0;
        /** Percentage of level EXP owed on village revival after an Assassin kill. */
        int pvp_death_exp_percent = 0;
        std::int32_t level = 1;
        std::int64_t exp = 0;
        std::int32_t hp = 100;
        std::int32_t max_hp = 100;
        std::int32_t mp = 50;
        std::int32_t max_mp = 50;
        std::int32_t pu_point = 0;
        std::int32_t su_point = 0;
        /** Contribution earned from quests (P_CONTRIBUTE). */
        std::int32_t contribution = 0;
        /** Persisted transform rage (P_RAGE). */
        std::int32_t rage = 0;
        std::int32_t str = 0;
        std::int32_t hth = 0;
        std::int32_t intl = 0;
        std::int32_t wis = 0;
        std::int32_t dex = 0;
        std::int64_t money = 0;
        std::uint32_t next_iid = 1;
        int weapon_index = 0;
        int armor_index = 0;
        int transform_index = 0;
        int transform_level = 1;
        int transform_exp = 0;
        int transform_put_on_ticks = 0;

        /** Committed talent-node bits (project-owned stable order). */
        std::uint64_t talent_mask = 0;
        /** Unix milliseconds until Beacon is ready; survives reconnects. */
        std::int64_t beacon_ready_at = 0;

        /** Job specialty bitmask (bit 0 = wandering). */
        std::int32_t specialty = 1;

        /** Soft-delete lifecycle flag. */
        CharacterDeletionState deletion_state = CharacterDeletionState::Active;

        /** Unix timestamp after which restore is no longer allowed. */
        std::int64_t delete_after = 0;
    };

    /** One persisted inventory row (S2D_INSERTITEM / UPDATEITEMNUM / PUTON/PUTOFF). */
    struct ItemRecord {
        std::int32_t iid = 0;
        /** Character owner, or 0 in account storage or trading post custody. */
        std::int32_t character_id = 0;

        /** Owning account in inventory and storage; inferred from character_id when writing. */
        std::int32_t account_id = 0;
        int index = 0;
        int prefix = 0;
        int count = 0;
        /** 0 bag, 1 weapon, 2 armor/ornament, 3 transform, 4 pet, 5 ride, 6 skin. */
        int slot = 0;

        /** Current endurance, or 0 when the item has none. */
        int cur_end = 0;

        /** Max endurance, or 0 when the item has none. */
        int max_end = 0;

        /** Talisman of Attack plus. */
        int x_attack = 0;

        /** Talisman of Magic plus. */
        int x_magic = 0;

        /** Talisman of Accuracy plus. */
        int x_hit = 0;

        /** Talisman of Defense plus. */
        int x_defense = 0;

        /** Talisman of Evasion plus. */
        int x_dodge = 0;

        /** Explosive Blow level. */
        int upgrade_level = 0;

        /** Stone of Revision applications at the current EB level. */
        int upgrade_rate = 0;

        /** Mixing effect type encoded as int32 mix_type (low 8 bits transported). */
        int mix_type = 0;

        /** 1 when Bead of Fire is applied to this armor instance. */
        int bof = 0;

        /** 1 after a successful Perforation Shot. */
        int qigong_perforated = 0;

        /** Qigong random effect id in [1, 80], or zero. */
        int qigong_effect = 0;

        /** Dragon Fusing random effect id in [1, 240], or zero. */
        int dragon_effect = 0;

        /** Random equipment modifier type encoded as protocol::RandomItemStatType. */
        int random_stat_type = 0;

        /** Random equipment modifier magnitude (1 through 5), or zero. */
        int random_stat_value = 0;

        /** Animal level on a transform egg (compatibility behavior). */
        int transform_level = 1;

        /** Animal EXP on a transform egg (compatibility behavior). */
        int transform_exp = 0;

        /** Unix expiry time for a temporary pet instance; zero means not started/permanent. */
        std::int64_t pet_expire_at = 0;

        /** Unix expiry time for a temporary riding item; zero means not started/permanent. */
        std::int64_t ride_expire_at = 0;

        /** True when the item cannot leave its account; account storage remains allowed. */
        bool account_bound = false;

        /** One remaining enchantment attempt protected by a Polishing Stone. */
        bool enchant_protected = false;

        /** True when the item cannot leave its character, including account storage. */
        bool character_bound = false;
    };

    /** One atomic full-instance transfer or split of an existing inventory stack. */
    struct TradeTransfer {
        /** Expected underlying inventory row, including its complete count. */
        ItemRecord source;
        /** Units transferred to the other character. */
        int count = 0;
        /** Fresh destination IID for a partial transfer; zero for a complete transfer. */
        std::int32_t split_iid = 0;
    };

    /** Durable state shared by every player for one configured server event. */
    struct ServerEventRecord {
        /** Stable event id from config/server_events.json. */
        std::string event_id;
        /** Coins accumulated towards the next activation. */
        std::int64_t contribution_count = 0;
        /** Bonus percentage while the event is active; zero when inactive. */
        std::int32_t active_bonus_percent = 0;
        /** Unix timestamp at which the event stops; zero when inactive. */
        std::int64_t active_until = 0;
    };

    /** Allocates a process-wide item iid (compatibility behavior / InterlockedIncrement).
     * @param next_iid Last issued iid; incremented and the new value is returned.
     * @return New iid (never 0).
     */
    [[nodiscard]] inline std::uint32_t allocate_item_iid(std::atomic<std::uint32_t>& next_iid) {
        std::uint32_t iid = next_iid.fetch_add(1, std::memory_order_relaxed) + 1;
        if (iid == 0) {
            iid = next_iid.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        return iid;
    }

    /** Raises the iid seed when a persisted or world item is already at `iid`.
     * @param next_iid Last issued iid.
     * @param iid Observed iid; ignored when 0.
     */
    inline void observe_used_iid(std::atomic<std::uint32_t>& next_iid, std::uint32_t iid) {
        if (iid == 0) {
            return;
        }
        std::uint32_t cur = next_iid.load(std::memory_order_relaxed);
        while (iid > cur && !next_iid.compare_exchange_weak(
                                cur, iid, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }

    /** One persisted learned skill (S2D_INSERTSKILL / SKILLUP). */
    struct SkillRecord {
        std::int32_t character_id = 0;
        int skill_index = 0;
        int level = 1;
    };

    /** Number of persisted hotbar slots (two rows of ten). */
    inline constexpr std::size_t kShortcutSlots = protocol::kShortcutSlots;

    /** Persisted skill bar (C2S_SHORTCUT / S2C_SHORTCUT). */
    struct ShortcutRecord {
        std::int32_t character_id = 0;
        int active_index = -1;
        std::array<int, kShortcutSlots> slots{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    };

    /** One persisted quest flag (S2D_INSERTQUESTFLAG / UPDATEQUESTFLAG). */
    struct QuestFlagRecord {
        std::int32_t character_id = 0;
        std::uint16_t quest = 0;
        std::uint8_t flag = 0;
        std::uint8_t clear = 0;
    };

    /** One persistent system-mail item attachment. */
    struct SystemMailRecord {
        /** Durable mail identifier. */
        std::int64_t id = 0;
        /** Owning character. */
        std::int32_t character_id = 0;
        /** Idempotency key, unique for this character. */
        std::string source_key;
        /** Localized-display fallback subject. */
        std::string subject;
        /** Localized-display fallback message body. */
        std::string body;
        /** Attached item definition index, or zero for a message without an attachment. */
        int item_index = 0;
        /** Attached stack quantity; zero exactly when item_index is zero. */
        int item_count = 0;
        /** Unix timestamp when the server created the mail. */
        std::int64_t created_at = 0;
        /** Unix timestamp when the attachment was claimed; zero while available. */
        std::int64_t claimed_at = 0;
        /** Unix timestamp of the first read; zero while unread. */
        std::int64_t read_at = 0;
    };

    /** Result of an attempt to move one mail attachment into the character inventory. */
    enum class SystemMailClaimResult : std::uint8_t {
        Claimed,
        NotFound,
        AlreadyClaimed,
        Error,
    };

    /** Persistence boundary used by server game code. */
    /** Character selection boundary shared by local stores and the remote lobby. */
    class ICharacterRoster {
      public:
        /** Releases the character source. */
        virtual ~ICharacterRoster() = default;

        /** Lists active characters. @param account_id Owner. @return Owned characters. */
        [[nodiscard]] virtual std::vector<CharacterRecord>
        list_characters(std::int32_t account_id) = 0;

        /** Lists recoverable characters. @param account_id Owner. @param now_seconds Unix time.
         * @return Owned recoverable characters. */
        [[nodiscard]] virtual std::vector<CharacterRecord>
        list_deleted_characters(std::int32_t account_id, std::int64_t now_seconds) = 0;

        /** Loads a character. @param id Character id. @return Character, if accessible. */
        [[nodiscard]] virtual std::optional<CharacterRecord> load_character(std::int32_t id) = 0;

        /** Creates or saves a character, preserving an existing server-owned role.
         * @param character Character draft or local record; role applies only on creation.
         * @return True on success. */
        [[nodiscard]] virtual bool save_character(const CharacterRecord& character) = 0;

        /** Loads equipment previews. @param character_id Owner. @return Owned item rows. */
        [[nodiscard]] virtual std::vector<ItemRecord> load_items(std::int32_t character_id) = 0;

        /** Schedules deletion. @param id Character. @param account_id Owner. @param now_seconds
         * Unix time. @return True on success. */
        [[nodiscard]] virtual bool schedule_character_deletion(std::int32_t id,
                                                               std::int32_t account_id,
                                                               std::int64_t now_seconds) = 0;

        /** Restores a character. @param id Character. @param account_id Owner. @param now_seconds
         * Unix time. @return True on success. */
        [[nodiscard]] virtual bool
        restore_character(std::int32_t id, std::int32_t account_id, std::int64_t now_seconds) = 0;
    };

    /** Server-owned catalog metadata stored beside an anonymous offer. */
    struct TradingPostCategory {
        int clazz = -1, grade = 0;
        protocol::TradingPostType type = protocol::TradingPostType::Other;
    };

    /** Persistence result; quantities and charges describe only committed work. */
    struct TradingPostResult {
        bool committed = false;
        int quantity = 0;
        std::int64_t charged = 0;
        std::string status = "The trading post is unavailable. Please try again.";
        /** Seller of a committed purchase, used only for private server notifications. */
        int seller = 0;
    };

    /** One atomic collection pickup; live state changes only after commit. */
    struct TradingPostCollectionResult {
        bool committed = false;
        /** Final inventory stack with its surviving IID, absent for sale proceeds. */
        std::optional<protocol::TradeItem> item;
        /** Units received, excluding any pre-existing destination stack. */
        int received_count = 0;
        /** Geons added to the wallet by this pickup. */
        std::int64_t geons = 0;
        std::string status = "Collection is unavailable. Please try again.";
    };

    class IDataStore : public ICharacterRoster {
      public:
        /** Destroys the persistence implementation. */
        ~IDataStore() override = default;

        /** Atomically grants a character's starter kit once, including money and spawn.
         * @param character Final character state after applying the kit.
         * @param items New character-bound inventory rows with unused instance IDs.
         * @return True when the complete kit was committed; false if already granted or failed.
         */
        [[nodiscard]] virtual bool grant_starter_items(const CharacterRecord& character,
                                                       const std::vector<ItemRecord>& items) = 0;

        /** Returns the optional multiplayer guild transaction boundary.
         * @return Guild storage, or null for unsupported/local backends. */
        virtual IGuildStore* guild_store() {
            return nullptr;
        }

        /** Returns the optional Castle War transaction boundary.
         * @return Castle War storage, or null for unsupported/local backends.
         */
        virtual ICastleWarStore* castle_war_store() {
            return nullptr;
        }

        /** Creates a persistence implementation.
         * @param backend Persistence backend.
         * @return Store instance, or nullptr when unsupported.
         */
        [[nodiscard]] static std::unique_ptr<IDataStore> create(StoreBackend backend);

        /** Opens the persistence backend.
         * @param path_or_dsn Storage path or data-source name.
         * @return True when opening succeeds.
         */
        [[nodiscard]] virtual bool open(const std::string& path_or_dsn) = 0;

        /** Closes the backend. */
        virtual void close() = 0;

        /** Finds an account by name. */
        [[nodiscard]] virtual std::optional<AccountRecord>
        find_account(const std::string& name) = 0;

        /** Inserts or updates an account. */
        [[nodiscard]] virtual bool upsert_account(const AccountRecord& account) = 0;

        /** Loads enforcement state.
         * @param account_id Authenticated account.
         * @return Stored state, or empty on failure or missing account.
         */
        [[nodiscard]] virtual std::optional<AccountDiscipline>
        load_account_discipline(std::int32_t account_id) = 0;

        /** Atomically applies a violation or decay. A violation returning zero points triggered
         * a kick and may have set the ban.
         * @param account_id Authenticated account.
         * @param change Violation or decay of one point.
         * @return Committed state, or empty on failure or ineligible account.
         */
        [[nodiscard]] virtual std::optional<AccountDiscipline>
        update_account_discipline(std::int32_t account_id, AccountDisciplineChange change) = 0;

        /** Lists active characters owned by an account. */
        [[nodiscard]] virtual std::vector<CharacterRecord>
        list_characters(std::int32_t account_id) override = 0;

        /**
         * Lists recoverable deleted characters owned by an account.
         * @param account_id Owning account.
         * @param now_seconds Current Unix timestamp.
         * @return Recoverable rows ordered by expiry.
         */
        [[nodiscard]] virtual std::vector<CharacterRecord>
        list_deleted_characters(std::int32_t account_id, std::int64_t now_seconds) override = 0;

        /** Loads one character by identifier. */
        [[nodiscard]] virtual std::optional<CharacterRecord>
        load_character(std::int32_t id) override = 0;

        /** Saves gameplay state while preserving an existing server-owned role.
         * @param character Character draft or local record.
         * @return True on success. */
        [[nodiscard]] virtual bool save_character(const CharacterRecord& character) override = 0;

        /**
         * Marks an active character for deletion after the recovery window.
         * @param id Character identifier.
         * @param account_id Owning account.
         * @param now_seconds Current Unix timestamp.
         * @return True when the character entered the recoverable state.
         */
        [[nodiscard]] virtual bool schedule_character_deletion(
            std::int32_t id, std::int32_t account_id, std::int64_t now_seconds) override = 0;

        /**
         * Restores a recoverable character when the account has a free slot.
         * @param id Character identifier.
         * @param account_id Owning account.
         * @param now_seconds Current Unix timestamp.
         * @return True when the character was restored.
         */
        [[nodiscard]] virtual bool restore_character(std::int32_t id,
                                                     std::int32_t account_id,
                                                     std::int64_t now_seconds) override = 0;

        /**
         * Performs one bounded cleanup step for an expired character.
         * @param now_seconds Current Unix timestamp.
         * @param row_budget Maximum dependent rows removed by this step.
         * @return Cleanup progress.
         */
        [[nodiscard]] virtual CharacterPurgeResult
        process_character_deletions(std::int64_t now_seconds, std::size_t row_budget) = 0;

        /** Loads inventory rows for a character. */
        [[nodiscard]] virtual std::vector<ItemRecord>
        load_items(std::int32_t character_id) override = 0;

        /** Loads all account-storage rows ordered by storage slot.
         * @param account_id Owning account.
         * @return Stored item rows.
         */
        [[nodiscard]] virtual std::vector<ItemRecord>
        load_storage_items(std::int32_t account_id) = 0;

        /** Loads the saved Geon balance for an account.
         * @param account_id Owning account.
         * @return Balance, zero for a new account, or empty on failure.
         */
        [[nodiscard]] virtual std::optional<std::int64_t>
        load_storage_money(std::int32_t account_id) = 0;

        /** Commits the account balance change and character wallet as a single transaction.
         * @param account_id Owning account.
         * @param character_id Active character.
         * @param wallet_after Server-authoritative wallet after the transfer.
         * @param change Signed change to the account balance.
         * @return Committed account balance, or empty if either change was rejected.
         */
        [[nodiscard]] virtual std::optional<std::int64_t>
        transfer_storage_money(std::int32_t account_id,
                               std::int32_t character_id,
                               std::int64_t wallet_after,
                               std::int64_t change) = 0;

        /** Inserts one inventory row. */
        [[nodiscard]] virtual bool insert_item(const ItemRecord& item) = 0;

        /** Updates count/prefix/slot for one inventory row. */
        [[nodiscard]] virtual bool update_item(const ItemRecord& item) = 0;

        /** Atomically saves an equipment transfer and up to eight displaced skins.
         * @param items Existing instances belonging to one character, with their final wear slots.
         * @return True when every row was updated; false leaves all rows unchanged.
         */
        [[nodiscard]] virtual bool update_equipment_items(const std::vector<ItemRecord>& items) = 0;

        /** Atomically consumes one box and inserts its newly allocated rewards.
         * @param box Expected current inventory box; count and owner must still match.
         * @param rewards New inventory instances belonging to the same character.
         * @return True only when the entire exchange was committed.
         */
        [[nodiscard]] virtual bool
        consume_item_with_rewards(const ItemRecord& box,
                                  const std::vector<ItemRecord>& rewards) = 0;

        /** Atomically inserts or replaces the affected inventory rows of an item grant or exchange.
         * @param expected Current unequipped rows; empty for a new grant. Owner, index and count must still match.
         * @param replacements Final rows, including new output instances and remaining inputs.
         * @return True on commit; false leaves persistence unchanged, including unsupported stores.
         */
        [[nodiscard]] virtual bool
        exchange_inventory_items(const std::vector<ItemRecord>& expected,
                                 const std::vector<ItemRecord>& replacements) {
            (void) expected;
            (void) replacements;
            return false;
        }

        /** Saves a defeat, its forfeited items and any Town Board entry in one transaction.
         * @param character Final character state with zero HP.
         * @param items Expected worn mask and any dropped inventory instance.
         * @param last_hitter Real character name of the last hitter, empty for a non-PvP death.
         * @return True when the defeat was committed.
         */
        [[nodiscard]] virtual bool commit_assassin_death(const CharacterRecord& character,
                                                         const std::vector<ItemRecord>& items,
                                                         const std::string& last_hitter) {
            (void) character;
            (void) items;
            (void) last_hitter;
            return false;
        }

        /** Loads the most recent Town Board entries, newest first.
         * @return A board snapshot or nullopt on a store error.
         */
        [[nodiscard]] virtual std::optional<std::vector<protocol::AssassinListEntry>>
        load_assassin_list() = 0;

        /** Atomically saves two balances and transfers full instances or partial stacks.
         * The server owns both locked sessions; item ownership/count is checked in the transaction.
         * @param first Final first character. @param second Final second character.
         * @param items Expected sources, quantities and fresh split IDs; destination is the other
         * character.
         * @return True on commit, false without any persisted change. */
        [[nodiscard]] virtual bool commit_trade(const CharacterRecord& first,
                                                const CharacterRecord& second,
                                                const std::vector<TradeTransfer>& items) {
            (void) first;
            (void) second;
            (void) items;
            return false;
        }

        /** Reads one bounded market or collection page.
         * @param character Owning character for private views.
         * @param request View, filters and page.
         * @return Anonymous entries, or no value after a database failure. */
        [[nodiscard]] virtual std::optional<protocol::TradingPostState>
        trading_post_list(int character, const protocol::TradingPostRequest& request) = 0;

        /** Atomically moves one owned receipt into inventory or the wallet.
         * @param character Current persisted character and expected wallet balance.
         * @param receipt Collection receipt ID.
         * @param merge_target Expected compatible stack, or null to preserve the receipt IID.
         * @return Committed pickup, or an unchanged receipt on failure. */
        [[nodiscard]] virtual TradingPostCollectionResult
        trading_post_collect(const CharacterRecord& character,
                             std::uint64_t receipt,
                             const ItemRecord* merge_target = nullptr) = 0;

        /** Atomically escrows an inventory quantity and creates a fixed-price offer.
         * @param item Expected current source instance.
         * @param quantity Offered portion.
         * @param unit_price Geons per item.
         * @param category Server-authoritative catalog classification.
         * @param split_iid Unused IID for a partial stack; zero when listing the whole instance.
         * @return Committed outcome. */
        [[nodiscard]] virtual TradingPostResult
        trading_post_sell(const ItemRecord& item,
                          int quantity,
                          std::int64_t unit_price,
                          const TradingPostCategory& category,
                          std::int32_t split_iid = 0) = 0;

        /** Atomically buys up to the remaining quantity and records both entitlements.
         * @param buyer Current persisted character snapshot.
         * @param listing Durable offer identifier.
         * @param quantity Desired quantity.
         * @param unit_price Expected displayed price per item.
         * @param split_iid Unused IID if a partial fill splits the offered stack.
         * @return Committed quantity and charge, including a partial-fill notice. */
        [[nodiscard]] virtual TradingPostResult trading_post_buy(const CharacterRecord& buyer,
                                                                 std::uint64_t listing,
                                                                 int quantity,
                                                                 std::int64_t unit_price,
                                                                 std::int32_t split_iid = 0) = 0;

        /** Moves an owned offer's remaining items into its owner's collection.
         * @param character Seller character.
         * @param listing Durable offer identifier.
         * @return Committed outcome. */
        [[nodiscard]] virtual TradingPostResult trading_post_cancel(int character,
                                                                    std::uint64_t listing) = 0;

        /** Deletes one inventory row. */
        [[nodiscard]] virtual bool delete_item(std::int32_t iid) = 0;

        /** Loads one global server-event state row. */
        [[nodiscard]] virtual std::optional<ServerEventRecord>
        load_server_event(const std::string& event_id) = 0;

        /** Inserts or updates one global server-event state row. */
        [[nodiscard]] virtual bool save_server_event(const ServerEventRecord& event) = 0;

        /** Loads persistent Battle Riding state for one mount IID. */
        [[nodiscard]] virtual std::optional<BattleMountRecord>
        load_battle_mount(std::int32_t mount_iid) = 0;

        /** Atomically inserts or replaces persistent Battle Riding state. */
        [[nodiscard]] virtual bool upsert_battle_mount(const BattleMountRecord& mount) = 0;

        /** Deletes persistent Battle Riding state for one mount IID. */
        [[nodiscard]] virtual bool delete_battle_mount(std::int32_t mount_iid) = 0;

        /** Highest persisted item iid (DBSvr SELECT MAX(IID) at MainSvr connect).
         * @return MAX(iid), or 0 when the table is empty.
         */
        [[nodiscard]] virtual std::uint32_t max_item_iid() = 0;

        /** Loads learned skills for a character. */
        [[nodiscard]] virtual std::vector<SkillRecord> load_skills(std::int32_t character_id) = 0;

        /** Inserts or updates one learned skill. Level ≤ 0 deletes the row. */
        [[nodiscard]] virtual bool upsert_skill(const SkillRecord& skill) = 0;

        /** Loads the saved skill bar for a character.
         * @param character_id Character identifier.
         * @return Saved bar, or nullopt when none has been stored.
         */
        [[nodiscard]] virtual std::optional<ShortcutRecord>
        load_shortcuts(std::int32_t character_id) = 0;

        /** Inserts or updates the skill bar for a character.
         * @param shortcuts Active skill and hotbar slots.
         * @return True when the write succeeds.
         */
        [[nodiscard]] virtual bool upsert_shortcuts(const ShortcutRecord& shortcuts) = 0;

        /** Loads quest flags for a character. */
        [[nodiscard]] virtual std::vector<QuestFlagRecord>
        load_quest_flags(std::int32_t character_id) = 0;

        /** Inserts or updates one quest flag. */
        [[nodiscard]] virtual bool upsert_quest_flag(const QuestFlagRecord& flag) = 0;

        /** Marks owned, visible system mail read or deletes a claimed message.
         * @param character_id Owning character.
         * @param mail_id Durable mail identifier.
         * @param remove True to hide claimed mail, false to mark it read.
         * @param now Current Unix timestamp.
         * @return True when the owned message was updated.
         */
        [[nodiscard]] virtual bool update_system_mail(std::int32_t character_id,
                                                      std::int64_t mail_id,
                                                      bool remove,
                                                      std::int64_t now) = 0;

        /** Stores a system mail once for its character/source key pair.
         * @param mail Complete system mail and item attachment.
         * @return True when the mail is stored or already exists.
         */
        [[nodiscard]] virtual bool ensure_system_mail(const SystemMailRecord& mail) = 0;

        /** Lists all system mails, including attachments already claimed.
         * @param character_id Character id.
         * @return Mails ordered from newest to oldest.
         */
        [[nodiscard]] virtual std::vector<SystemMailRecord>
        list_system_mail(std::int32_t character_id) = 0;

        /** Atomically persists an attached item and marks its mail as claimed.
         * @param character_id Mail and item owner.
         * @param mail_id Durable mail identifier.
         * @param item Item instance to persist; the stored mail controls its index and count.
         * @param claimed_at Unix timestamp for the successful claim.
         * @return Claim outcome.
         */
        [[nodiscard]] virtual SystemMailClaimResult claim_system_mail(std::int32_t character_id,
                                                                      std::int64_t mail_id,
                                                                      const ItemRecord& item,
                                                                      std::int64_t claimed_at) = 0;

        /**
         * Loads persisted pay-buff remains for a character.
         * @param character_id Character id.
         * @return Buff remain rows.
         */
        [[nodiscard]] virtual std::vector<BuffRemainRecord>
        load_buff_remains(std::int32_t character_id) = 0;

        /**
         * Replaces all pay-buff remains for a character.
         * @param character_id Character id.
         * @param rows Active remains to store.
         * @return True on success.
         */
        [[nodiscard]] virtual bool
        replace_buff_remains(std::int32_t character_id,
                             const std::vector<BuffRemainRecord>& rows) = 0;

        /**
         * Loads active inventory-register unlocks for a character.
         * @param character_id Character id.
         * @return Unlock rows in stored register order.
         */
        [[nodiscard]] virtual std::vector<InventoryUnlockRecord>
        load_inventory_unlocks(std::int32_t character_id) = 0;

        /**
         * Replaces all active inventory-register unlocks for a character.
         * @param character_id Character id.
         * @param rows Active unlock rows.
         * @return True when the replacement succeeds.
         */
        [[nodiscard]] virtual bool
        replace_inventory_unlocks(std::int32_t character_id,
                                  const std::vector<InventoryUnlockRecord>& rows) = 0;

        /** Ensures the sole local account exists. */
        [[nodiscard]] virtual bool ensure_default_account(const std::string& account_name,
                                                          const std::string& password) = 0;

        /** Ensures a local account and fallback character exist. */
        [[nodiscard]] virtual bool ensure_default_player(const std::string& account_name,
                                                         const std::string& password,
                                                         const CharacterRecord& defaults) = 0;
    };

    /** Compatibility wrapper for IDataStore::create.
     * @param backend Persistence backend.
     * @return Store instance, or nullptr when unsupported.
     */
    [[nodiscard]] std::unique_ptr<IDataStore> create_data_store(StoreBackend backend);

    /** SQLite implementation of the server persistence boundary. */
    class SqliteStore final : public IDataStore {
      public:
        /** @copydoc IDataStore::grant_starter_items */
        [[nodiscard]] bool grant_starter_items(const CharacterRecord& character,
                                               const std::vector<ItemRecord>& items) override;

        /** Constructs a closed SQLite store. */
        SqliteStore();

        /** Closes the database. */
        ~SqliteStore() override;

        SqliteStore(const SqliteStore&) = delete;
        SqliteStore& operator=(const SqliteStore&) = delete;

        /** Opens a SQLite file or save directory. */
        [[nodiscard]] bool open(const std::string& path_or_dsn) override;

        /** Closes the database. */
        void close() override;

        /** Finds an account by name. */
        [[nodiscard]] std::optional<AccountRecord> find_account(const std::string& name) override;

        /** Inserts or updates an account. */
        [[nodiscard]] bool upsert_account(const AccountRecord& account) override;

        [[nodiscard]] std::optional<AccountDiscipline>
        load_account_discipline(std::int32_t account_id) override;

        [[nodiscard]] std::optional<AccountDiscipline>
        update_account_discipline(std::int32_t account_id, AccountDisciplineChange change) override;

        /** Lists active characters owned by an account. */
        [[nodiscard]] std::vector<CharacterRecord>
        list_characters(std::int32_t account_id) override;

        /** Lists recoverable deleted characters owned by an account. */
        [[nodiscard]] std::vector<CharacterRecord>
        list_deleted_characters(std::int32_t account_id, std::int64_t now_seconds) override;

        /** Loads a character by identifier. */
        [[nodiscard]] std::optional<CharacterRecord> load_character(std::int32_t id) override;

        /** Saves a character. */
        [[nodiscard]] bool save_character(const CharacterRecord& character) override;

        /** Marks a character for deletion after the recovery window. */
        [[nodiscard]] bool schedule_character_deletion(std::int32_t id,
                                                       std::int32_t account_id,
                                                       std::int64_t now_seconds) override;

        /** Restores a recoverable character. */
        [[nodiscard]] bool restore_character(std::int32_t id,
                                             std::int32_t account_id,
                                             std::int64_t now_seconds) override;

        /** Performs one bounded expired-character cleanup step. */
        [[nodiscard]] CharacterPurgeResult
        process_character_deletions(std::int64_t now_seconds, std::size_t row_budget) override;

        /** Loads inventory rows. */
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

        /** Inserts one inventory row. */
        [[nodiscard]] bool insert_item(const ItemRecord& item) override;

        /** Updates one inventory row. */
        [[nodiscard]] bool update_item(const ItemRecord& item) override;

        /** @copydoc IDataStore::update_equipment_items */
        [[nodiscard]] bool update_equipment_items(const std::vector<ItemRecord>& items) override;

        /** @copydoc IDataStore::consume_item_with_rewards */
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

        /** Loads one Battle Riding state row. */
        [[nodiscard]] std::optional<BattleMountRecord>
        load_battle_mount(std::int32_t mount_iid) override;

        /** Upserts one Battle Riding state row. */
        [[nodiscard]] bool upsert_battle_mount(const BattleMountRecord& mount) override;

        /** Deletes one Battle Riding state row. */
        [[nodiscard]] bool delete_battle_mount(std::int32_t mount_iid) override;

        /** Highest persisted item iid (DBSvr SELECT MAX(IID) at MainSvr connect).
         * @return MAX(iid), or 0 when the table is empty.
         */
        [[nodiscard]] std::uint32_t max_item_iid() override;

        /** Loads learned skills. */
        [[nodiscard]] std::vector<SkillRecord> load_skills(std::int32_t character_id) override;

        /** Upserts one skill. Level ≤ 0 deletes the row. */
        [[nodiscard]] bool upsert_skill(const SkillRecord& skill) override;

        /** Loads the saved skill bar. */
        [[nodiscard]] std::optional<ShortcutRecord>
        load_shortcuts(std::int32_t character_id) override;

        /** Upserts the skill bar. */
        [[nodiscard]] bool upsert_shortcuts(const ShortcutRecord& shortcuts) override;

        /** Loads quest flags. */
        [[nodiscard]] std::vector<QuestFlagRecord>
        load_quest_flags(std::int32_t character_id) override;

        /** Upserts one quest flag. */
        [[nodiscard]] bool upsert_quest_flag(const QuestFlagRecord& flag) override;

        /** @copydoc IDataStore::update_system_mail */
        [[nodiscard]] bool update_system_mail(std::int32_t character_id,
                                              std::int64_t mail_id,
                                              bool remove,
                                              std::int64_t now) override;

        /** @copydoc IDataStore::ensure_system_mail */
        [[nodiscard]] bool ensure_system_mail(const SystemMailRecord& mail) override;

        /** Lists all system mails for a character. */
        [[nodiscard]] std::vector<SystemMailRecord>
        list_system_mail(std::int32_t character_id) override;

        /** Atomically claims one system-mail attachment. */
        [[nodiscard]] SystemMailClaimResult claim_system_mail(std::int32_t character_id,
                                                              std::int64_t mail_id,
                                                              const ItemRecord& item,
                                                              std::int64_t claimed_at) override;

        /** Loads pay-buff remains. */
        [[nodiscard]] std::vector<BuffRemainRecord>
        load_buff_remains(std::int32_t character_id) override;

        /** Replaces pay-buff remains. */
        [[nodiscard]] bool replace_buff_remains(std::int32_t character_id,
                                                const std::vector<BuffRemainRecord>& rows) override;

        /** Loads active inventory-register unlocks. */
        [[nodiscard]] std::vector<InventoryUnlockRecord>
        load_inventory_unlocks(std::int32_t character_id) override;

        /** Replaces active inventory-register unlocks. */
        [[nodiscard]] bool
        replace_inventory_unlocks(std::int32_t character_id,
                                  const std::vector<InventoryUnlockRecord>& rows) override;

        /** Ensures the local account exists. */
        [[nodiscard]] bool ensure_default_account(const std::string& account_name,
                                                  const std::string& password) override;

        /** Ensures the local account and fallback character exist. */
        [[nodiscard]] bool ensure_default_player(const std::string& account_name,
                                                 const std::string& password,
                                                 const CharacterRecord& defaults) override;

      private:
        struct Impl;

        /** SQLite connection and path. */
        std::unique_ptr<Impl> m_impl;
    };

}  // namespace openkal::server
