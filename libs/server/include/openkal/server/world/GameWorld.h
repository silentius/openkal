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

#include "openkal/map/MapCatalog.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/protocol/Riding.h"
#include "openkal/protocol/Skin.h"
#include "openkal/server/BofCatalog.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/SkillBuff.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace openkal::server {

    class WorldOccupancyIndex;
    struct ItemRecord;

    /** Quest-gated kill drop slot from monster definition / JSON stats.quests. */
    struct MonsterQuestDrop {
        /** Quest index (high 16 bits of CheckQuestFlag key). */
        std::uint16_t quest_index = 0;

        /** Required quest flag (low 16 bits; compared equal, clear ignored). */
        std::uint8_t flag = 0;

        /** Outer itemgroup index to roll. */
        int itemgroup = 0;

        /** Number of roll attempts. */
        int drop_num = 1;
    };

    /** One skill configured on a monster definition. */
    struct MonsterSkillSlot {
        /** Index in config/skills/monster. */
        int index = 0;
        /** Configured skill rank. */
        int level = 1;
    };

    /** Immutable monster definition loaded from server configuration. */
    struct MonsterDef {
        int index = 1;
        std::string name = "Mob";
        /** monster definition race byte; see protocol::MonsterRace. */
        int race = 0;
        std::int32_t level = 1;
        /** Base kill EXP from monster definition (exp N) / JSON stats.exp. */
        std::int64_t exp = 0;
        /** Revised primary strength used by monster skills. */
        std::int32_t strength = 0;
        /** Revised primary intelligence used by monster skills. */
        std::int32_t intelligence = 0;
        float speed = 20.0f;  // server pixels / second
        std::int32_t max_hp = 100;
        std::int32_t min_attack = 1;
        std::int32_t max_attack = 1;
        /** Minimum revised magical damage. */
        std::int32_t min_magic = 0;
        /** Maximum revised magical damage. */
        std::int32_t max_magic = 0;
        std::int32_t defense_melee = 0;
        std::int32_t defense_range = 0;
        std::int32_t hit = 0;
        std::int32_t dodge = 0;
        std::int32_t absorb = 0;
        /** monster definition resist[RT_FIRE..RT_PALSY] after `* 10` and ReviseProperty. */
        std::int32_t res_fire = 0;
        std::int32_t res_ice = 0;
        std::int32_t res_lightning = 0;
        std::int32_t res_curse = 0;
        std::int32_t res_palsy = 0;
        int attack_range = 20;  // monster definition range, in server pixels
        /** monster definition (attack type min max) — 0 melee, 1 ranged. */
        int attack_type = 0;
        /** monster definition sight[0], proximity detect radius in server pixels. */
        int detect_range = 160;
        int sight_range = 240;  // monster definition sight[1], chase leash, in server pixels
        /** monster definition / JSON stats.ai (AIS_* / subclass selector). */
        int ai = 1;
        /** Derived from ai + race (compatibility behavior runtime state). */
        bool aggressive = false;
        int attack_speed_ms = 2000;
        int roam_speed_ms = 1600;
        int chase_speed_ms = 800;
        /** Skills loaded from stats.skill, in the authored order. */
        std::vector<MonsterSkillSlot> skills;
        /** Drop table slots: (itemgroup_index, attempt_count). */
        std::vector<std::pair<int, int>> item_groups;
        /** Quest item drop slots (rolled when CheckQuestFlag matches). */
        std::vector<MonsterQuestDrop> quest_drops;
    };

    /** Combat-relevant item definition loaded by the server. */
    struct ItemCombatDef {
        int index = 0;
        int range = 0;
        std::int32_t min_attack = 0;
        std::int32_t max_attack = 0;
        std::int32_t hit = 0;
        int attack_speed_ms = 0;
        std::int32_t defense_melee = 0;
        std::int32_t defense_range = 0;
        std::int32_t absorb = 0;
        std::int32_t dodge = 0;
        std::int32_t min_magic = 0;
        std::int32_t max_magic = 0;
        std::int32_t fatal_rate = 0;
        std::int32_t fatal_pt = 0;
        std::int32_t str = 0;
        std::int32_t hth = 0;
        std::int32_t intl = 0;
        std::int32_t wis = 0;
        std::int32_t dex = 0;
        std::int32_t hp = 0;
        std::int32_t mp = 0;
        std::int32_t res_fire = 0;
        std::int32_t res_ice = 0;
        std::int32_t res_lightning = 0;
        std::int32_t res_curse = 0;
        std::int32_t res_palsy = 0;
        int level_limit = 1;
        int class_limit = -1;  // -1 = any; 0/1/2/3 = knight/mage/archer/thief
        bool wearable = false;

        /** Whether equipping the item places the character in Assassin mode. */
        bool assassin_mask = false;
        /** Cosmetic slot; cosmetic definitions never contribute combat stats. */
        protocol::SkinSlot skin_slot = protocol::SkinSlot::None;
        /** Whole-body appearance replacing individual body skins. */
        bool full_body_skin = false;
        /** Required combat weapon category; None leaves a weapon skin unusable. */
        protocol::WeaponSkinType skin_weapon_type = protocol::WeaponSkinType::None;
        bool is_weapon = false;
        bool is_defense = false;
        bool is_transform = false;
        bool is_ornament = false;
        /** True for `pet` and `limitedpet` item classes. */
        bool is_pet = false;
        /** True for common `ride` item classes. */
        bool is_ride = false;

        /** item definition extra.race for transform eggs (0 tiger / 4 panther / 6 bird / 8 gale).
         */
        int transform_race = 0;

        /** Definition name used to recognize named weapon families. */
        std::string name;

        /** First item definition class token (weapon, quest, ...). */
        std::string class_big;

        /** Derived category for special quest items granted only by quest scripts. */
        bool is_task = false;

        /** Second item definition class token (helmet, ring, trinket, ...). */
        std::string class_small;

        /** True when JSON stackable is 1; wearable items still cannot merge. */
        bool stackable = false;

        /** item definition pay / cash-shop flag; trash is refused when true. */
        bool pay = false;

        bool has_changeprefix = false;
        std::string changeprefix_target;
        int changeprefix_rate0 = 0;
        int changeprefix_prefix0 = 0;
        int changeprefix_rate1 = 0;
        int changeprefix_prefix1 = 0;

        /** item definition endurance; forced to 0 for ornaments. */
        int endurance = 0;
        int grade = 0;
        bool has_repair = false;
        int repair_mode = 0;
        int repair_amount = 0;
        /** item definition NPC purchase price. */
        int buy = 0;
        /** item definition NPC sale price. */
        int sell = 0;

        /** specialty.charming target, or None. */
        CharmKind charming = CharmKind::None;

        /** True when specialty.bof is set (Bead of Fire applicator). */
        bool bof = false;

        /** True when item definition (use 1). */
        bool usable = false;
        /** Reward Points granted by using one Honor Coupon. */
        int honor_reward_points = 0;

        /** HP restored when specialty.refresh stat is hp. */
        int refresh_hp = 0;

        /** True when specialty.refresh.auto_hp requires automatic use. */
        bool auto_hp = false;

        /** MP restored when specialty.refresh stat is mp. */
        int refresh_mp = 0;

        /** City teleport table index from specialty.teleport, or -1. */
        int teleport_loc = -1;

        /** Shared refresh-item cooldown in milliseconds (item definition cooltime). */
        int cooltime_ms = 0;

        /** S2C_EFFECT id played on successful use. */
        int effect = 0;

        /**
         * specialty.buff[0] BUFF_TYPE, or -1 when absent.
         * Pay scrolls use 51–59.
         */
        int buff_type = -1;

        /**
         * specialty.buff[1]: seconds for timed buffs, or charge count for Perfect Def /
         * Drain.
         */
        int buff_remain = 0;

        /** specialty.buff[2] amount (sit HP/MP per-second pulse; unused for pay scrolls). */
        int buff_amount = 0;

        /**
         * specialty.revival extra-cut percent (100 = no EXP loss, 80 = 20% loss).
         * -1 when not a revival scroll.
         */
        int revival_extra_cut = -1;

        /**
         * Adds specialty primary-stat bonuses before apply_stat_revise.
         * @param str Strength accumulator.
         * @param hth Health accumulator.
         * @param intl Intelligence accumulator.
         * @param wis Wisdom accumulator.
         * @param dex Dexterity accumulator.
         */
        void apply_primaries(int& str, int& hth, int& intl, int& wis, int& dex) const;

        /**
         * Adds specialty combat bonuses after revise (compatibility behavior).
         * @param hit Hit accumulator.
         * @param dodge Dodge accumulator.
         * @param min_attack Minimum physical attack.
         * @param max_attack Maximum physical attack.
         * @param min_magic Minimum magical attack.
         * @param max_magic Maximum magical attack.
         * @param defense Melee defense accumulator (GetDefense(0)).
         * @param defense_range Ranged defense accumulator (GetDefense(1)).
         * @param absorb Absorb accumulator.
         * @param fatal_rate Fatal rate accumulator.
         * @param fatal_pt Fatal point accumulator.
         * @param res_fire Fire resist.
         * @param res_ice Ice resist.
         * @param res_lightning Lightning resist.
         * @param res_curse Curse resist.
         * @param res_palsy Palsy resist.
         */
        void apply_gear(int& hit,
                        int& dodge,
                        int& min_attack,
                        int& max_attack,
                        int& min_magic,
                        int& max_magic,
                        int& defense,
                        int& defense_range,
                        int& absorb,
                        int& fatal_rate,
                        int& fatal_pt,
                        int& res_fire,
                        int& res_ice,
                        int& res_lightning,
                        int& res_curse,
                        int& res_palsy) const;
    };

    /** Server-authoritative gameplay definition for one pet item. */
    struct PetDef {
        int item_index = 0;
        /** `null` in JSON means permanent. Zero is also permanent at runtime. */
        std::int64_t duration_seconds = 0;
        int heal_per_two_seconds = 0;
        bool auto_pickup = false;
    };

    /** Server-authoritative gameplay definition for one riding animal. */
    struct RidingDef {
        int item_index = 0;
        /** `null` in JSON means permanent. Zero is also permanent at runtime. */
        std::int64_t duration_seconds = 0;
        int movement_speed_percent = 0;
        protocol::RidingKind kind = protocol::RidingKind::Common;
        protocol::BattleRidingGrade initial_grade = protocol::BattleRidingGrade::Normal;
    };

    /** Exclusive paperdoll slot for a wearable item. */
    enum class WearKind : int {
        None = 0,
        Weapon = 1,
        Shield = 2,
        Helmet = 3,
        Upper = 4,
        Pants = 5,
        Boots = 6,
        Gauntlet = 7,
        Face = 8,
        Ring = 9,
        Necklace = 10,
        Trinket = 11,
        Transform = 12,
        Pet = 13,
        Ride = 14,
        Collection = 15,
        SkinWeapon = 16,
        SkinShield,
        SkinHelmet,
        SkinUpper,
        SkinPants,
        SkinBoots,
        SkinGauntlet,
        SkinFace,
        AssassinMask,
    };

    /**
     * Resolves the exclusive wear slot for an item definition.
     * @param def Combat item definition.
     * @return Wear slot, or WearKind::None when the item is not worn.
     */
    [[nodiscard]] WearKind wear_kind_of(const ItemCombatDef& def);

    /**
     * Maps a wear kind onto ItemRecord.slot.
     * @param kind Exclusive wear slot.
     * @return 0 bag, 1 weapon, 2 other wearable, 3 transform, 4 pet, 5 ride, 6 skin, or 7 Assassin Mask.
     */
    [[nodiscard]] int persist_slot_of(WearKind kind);

    /** GenMonster-style axis-aligned spawn rectangle in server pixels. */
    struct MonsterSpawn {
        int def_index = 1;
        int count = 1;
        int area = 0;
        int map_index = 0;
        int cycle_sec = 1;

        /** Seconds until the next refill (`runtime state`). Zero means due. */
        float next_gen_left = 0.0f;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
    };

    /** Mutable authoritative monster state. */
    struct MonsterEntity {
        std::uint32_t id = 0;
        int def_index = 1;
        /** CreateMonster race byte; see protocol::MonsterRace. */
        int race = 0;
        std::string name;
        int x = 0;
        int y = 0;
        int z = 0;
        /** Logical map (0 = overworld, 1–4 = D1–D4). */
        int map = 0;
        int dir = 0;
        std::int32_t hp = 100;
        std::int32_t max_hp = 100;
        // macro-e mspeed[0]/[1] → px/s via 32000/tick (roam walk vs aggro run).
        float roam_speed = 20.0f;
        float chase_speed = 40.0f;
        float speed = 20.0f;  // alias of roam_speed (compat)
        int roam_x0 = 0;
        int roam_y0 = 0;
        int roam_x1 = 0;
        int roam_y1 = 0;
        int target_x = 0;
        int target_y = 0;
        // Kal MoveMonster: server snaps to dest immediately; busy until client-scale
        // travel time.
        bool moving = false;
        bool move_is_run = false;
        float move_busy_left = 0.0f;
        float idle_left = 0.0f;  // seconds until next roam (after stop)
        bool dirty_create = true;
        bool dirty_move = false;
        // Emit MoveMonsterEnd so the client aborts an in-flight roam/chase waypoint.
        bool dirty_stop = false;
        // KalOnline compatibility behavior — Roam / Chase / Attack.
        enum class Ai : std::uint8_t { Roam = 0, Chase = 1, Attack = 2 };
        Ai ai = Ai::Roam;
        // Combat / aggro (mirrors SetTarget + hostility).
        std::uint32_t aggro_player_id = 0;
        bool aggro = false;
        /** Current target has an active Beacon in range; reset by each AI tick. */
        bool beacon_pursuit = false;

        /** Hostility assigned to the current aggro target. */
        std::int32_t aggro_hostility = 0;

        /** Accumulated reward hostility per player, matching compatibility behavior. */
        std::unordered_map<std::uint32_t, std::int64_t> hostility_by_player;

        /** Actual HP damage credited during the current hostility session. */
        std::unordered_map<std::uint32_t, std::int64_t> reward_damage_by_player;

        /** True when idle ScanSight may engage nearby players. */
        bool aggressive = false;
        int attack_range = 20;
        /** monster definition sight[0] proximity detect radius. */
        int detect_range = 160;
        int sight_range = 240;  // monster definition sight[1] chase leash
        std::int32_t min_attack = 1;
        std::int32_t max_attack = 1;
        float attack_cooldown = 0.0f;

        /** Seconds until a periodic monster skill may be selected. */
        float skill_cooldown = 0.0f;
        /** Remaining server-side precast time for the selected monster skill. */
        float skill_precast_left = 0.0f;
        /** Selected skill slot while skill_precast_left is positive; -1 when idle. */
        int pending_skill_slot = -1;
        /** Round-robin cursor into MonsterDef::skills. */
        std::size_t next_skill_slot = 0;

        /** Seconds for which the monster ignores incoming player damage after creation. */
        float spawn_protection_left = 0.0f;

        /** Castle War flag slot, or -1 for ordinary monsters. */
        int castle_flag = -1;

        /** Remaining Doggebi Moon Skin absorb-buff duration. */
        float doggebi_absorb_left = 0.0f;

        /** Remaining Doggebi Protection of Moon resistance-buff duration. */
        float doggebi_resist_all_left = 0.0f;

        /** Damage retained for the next compatibility behavior check. */
        std::int32_t doggebi_pending_reaction_damage = 0;

        /** True when the pending Doggebi reaction was caused by physical damage. */
        bool doggebi_pending_reaction_physical = true;

        /** True while a melee swing is committed (no chase until it finishes). */
        bool attack_swinging = false;
        /** Seconds until idle ScanSight may re-engage after dropping combat. */
        float detect_cooldown = 0.0f;
        /** Seconds remaining for a skill movement/action lock. */
        float control_left = 0.0f;

        /** True when positive incoming damage clears control_left. */
        bool control_break_on_damage = false;

        /** Player currently opening this chest (race Item); 0 = none. */
        std::uint32_t open_player_id = 0;
        /** Seconds remaining until the chest finishes opening. */
        float open_left = 0.0f;
        /** After open success: seconds until RemoveMonster (die clip plays). */
        float open_remove_left = 0.0f;

        /** CreateMonster / Maguni gstate (compatibility behavior while beheadable). */
        std::uint32_t gstate = 0;

        /** Maguni kneel remaining seconds; 0 when not kneeling. */
        float knee_left = 0.0f;

        /** After AT_DIE: seconds until RemoveMonster so the die clip can play. */
        float die_remove_left = 0.0f;

        /** Flame-plant delay before compatibility behavior starts. */
        float fire_flower_explode_left = 0.0f;

        /** Flame-plant compatibility behavior precast delay. */
        float fire_flower_skill_fire_left = 0.0f;

        /** Flame-plant lifetime after beginning its explosion. */
        float fire_flower_remove_left = 0.0f;

        /** True once the flame plant has entered its explosion sequence. */
        bool fire_flower_exploded = false;

        /** True after the flame plant emitted its terminal death event. */
        bool fire_flower_removed = false;

        /** Remaining Ice mastery slow seconds. */
        float slow_left = 0.0f;

        /** Ice mastery move-speed percent (`-20 * PtIce`). */
        int slow_percent = 0;

        /** True when P_MSPEED / compatibility behavior must be rebroadcast. */
        bool dirty_slow = false;

        /** Timed Archer / shared skill states (Mesh of Speed, curses, Cloud Sight). */
        SkillBuffSet skill_buffs;

        /** Remaining compatibility behavior waypoints (server pixels) while detouring around
         * bodies. */
        std::vector<std::pair<int, int>> path_waypoints;
    };

    /** Ground-loot access selected from credited damage versus maximum HP. */
    enum class MonsterLootAccess : std::uint8_t {
        /** Less than 21 percent credited damage: no normal loot. */
        None = 0,
        /** 21 through 49 percent: loot exists but nobody may pick it for 120 seconds. */
        ServerLocked = 1,
        /** At least 50 percent: highest-hostility player or party owns it temporarily. */
        HostilityOwner = 2,
    };

    /** Reward pool derived from actual damage in the current hostility session. */
    struct MonsterRewardCoverage {
        /** Damage counted toward rewards, capped at the monster's maximum HP. */
        std::int64_t credited_damage = 0;

        /** Base EXP available before level and pay-buff revisions. */
        std::int64_t exp_pool = 0;

        /** Normal-loot access tier. */
        MonsterLootAccess loot_access = MonsterLootAccess::None;
    };

    /** Calculates proportional EXP and loot access from credited actual damage.
     * @param monster Monster at its death transition.
     * @param base_exp Monster's unrevised full-kill EXP.
     * @return Capped damage, proportional EXP pool, and loot-access tier.
     */
    [[nodiscard]] MonsterRewardCoverage
    calculate_monster_reward_coverage(const MonsterEntity& monster, std::int64_t base_exp);

    /** Minimal authoritative player position used by monster AI. */
    struct PlayerPose {
        std::uint32_t id = 0;
        int x = 0;
        int y = 0;
        /** Logical map (0 = overworld, 1–4 = D1–D4). */
        int map = 0;

        /** True while login spawn protection or hiding (compatibility behavior / IsNormal)
         * hides this player from ScanSight and drops current aggro. */
        bool aggro_immune = false;
        /** Attract idle hostile monsters while Beacon is active. */
        bool beacon_active = false;
    };

    /** Monster movement emitted by a world tick. */
    struct MonsterMoveEvent {
        std::uint32_t id = 0;
        int x = 0;
        int y = 0;
        std::int8_t dx = 0;
        std::int8_t dy = 0;
        bool ended = false;
        bool run = false;
    };

    /** Monster combat event emitted by a world tick. */
    struct MonsterAttackEvent {
        /** Network/combat phase of this event. */
        enum class Kind : std::uint8_t {
            /** A regular physical monster attack. */
            Basic = 0,
            /** Skill telegraph before its configured precast delay. */
            SkillPrepare = 1,
            /** Skill impact after its configured precast delay. */
            SkillFire = 2,
        };

        Kind kind = Kind::Basic;
        std::uint32_t attacker_id = 0;
        std::uint32_t target_player_id = 0;
        /** Monster skill index; zero for basic attacks. */
        std::uint8_t skill_index = 0;
        /** Monster skill rank; one for basic attacks. */
        std::uint8_t skill_level = 1;
        std::int32_t damage = 0;
    };

    /** Treasure-box open lifecycle event emitted by GameWorld::tick. */
    struct ChestOpenEvent {
        /** Event kind. */
        enum class Kind : std::uint8_t {
            /** Opening finished; drops and die action should be sent. */
            Completed = 0,
            /** Post-die delay finished; RemoveMonster should be sent. */
            Remove = 1,
        };

        /** Event kind. */
        Kind kind = Kind::Completed;

        /** Chest entity id. */
        std::uint32_t monster_id = 0;

        /** Opener player id. */
        std::uint32_t player_id = 0;

        /** Def index for drop tables. */
        int def_index = 0;

        /** Corpse X. */
        int x = 0;

        /** Corpse Y. */
        int y = 0;
    };

    /** Combat-corpse lifecycle event emitted by GameWorld::tick. */
    struct MonsterCorpseEvent {
        /** Event kind. */
        enum class Kind : std::uint8_t {
            /** Maguni kneel timed out; AT_DIE then RemoveMonster. */
            KneeTimeout = 0,

            /** Die-clip delay finished; RemoveMonster. */
            Remove = 1,

            /** Timed special monster died; emit AT_DIE and RemoveMonster. */
            Die = 2,
        };

        /** Event kind. */
        Kind kind = Kind::Remove;

        /** Corpse entity id. */
        std::uint32_t monster_id = 0;
    };

    /** Ground loot entity spawned from monster drops or player drops. */
    struct GroundItem {
        /** Persistent item iid (compatibility behavior); also the CreateItem world id. */
        std::uint32_t id = 0;

        /** item definition index (money uses kMoneyItemIndex). */
        int item_index = 0;

        /** Prefix id. */
        int prefix = 0;

        /** Random equipment modifier type encoded as protocol::RandomItemStatType. */
        int random_stat_type = 0;

        /** Random equipment modifier magnitude (1 through 5), or zero. */
        int random_stat_value = 0;

        /** Stack count or money amount. */
        int count = 1;

        /** Server pixel X. */
        int x = 0;

        /** Server pixel Y. */
        int y = 0;

        /** Logical map of the drop. */
        int map = 0;

        /** Full item attributes for an inventory instance dropped on Assassin defeat. */
        std::shared_ptr<const ItemRecord> inventory_instance;

        /** Killer / dropper who may pick first; 0 = free for all. */
        std::uint32_t owner_player_id = 0;

        /** Owning party whose members may pick first; 0 = no party ownership. */
        std::uint32_t owner_party_id = 0;

        /** Random party recipient retained across pickup retries; zero means unassigned. */
        std::uint32_t party_recipient_id = 0;

        /** True while the server blocks every player from picking this item. */
        bool server_owned = false;

        /** Seconds until ownership expires (legacy ~120s). */
        float ownership_left = 0.0f;

        /** Seconds until despawn (legacy ~180s). */
        float despawn_left = 180.0f;
    };

    /** Combat-relevant skill definition loaded by the server. */
    struct SkillDef {
        int clazz = 0;
        int index = 0;
        int max_level = 1;
        int mp = 0;
        /** Rage cost (skill definition rage or jobsystem compare id 5). */
        int rage = 0;
        /** Converted jobsystem damageType. */
        int damage_type = 0;

        /** Converted jobsystem lastTime. */
        int last_time = 0;

        /** Converted jobsystem value1. */
        int value1 = 0;

        /** Converted jobsystem value2. */
        int value2 = 0;

        /** Raw jobsystem property tuple. */
        std::vector<int> property;

        /** Raw extra.targetkind values. */
        std::vector<int> target_kinds;

        /** Compare id 2 cast ranges indexed by learned rank minus one. */
        std::vector<int> cast_range_by_level;

        int level_limit = 0;   // limit[0]
        int delay_pre_ms = 0;  // delay[0] — precast wind-up
        int delay_ms = 0;      // delay[1] — cooldown portion (ReviseTick uses pre+mid)
        /** Jobsystem compare[3] literal cooldown in milliseconds. */
        int compare_cooldown_ms = 0;
        /** Compare id 4 cooldowns indexed by learned rank minus one. */
        std::vector<int> cooldown_by_level;
        /** True when compare[3] includes jobsystem `weapon_speed`. */
        bool compare_cooldown_weapon_speed = false;
        /** True when jobsystem type includes `use`. */
        bool usable = false;

        /** True when jobsystem type includes `once`. */
        bool once = false;

        /** True when ranks are bought with SU instead of auto-learned. */
        bool manual_learn = false;

        /** Jobsystem grade mask: 1 Common, 3 1st, 7 2nd, …; 0 treated as common. */
        int grade_mask = 0;
        /** Required prior skill index from skill definition limit[2]; 0 = none. */
        int require_skill_index = 0;
        /** Required prior skill level from skill definition limit[3]. */
        int require_skill_level = 0;
        /** Character levels that grant successive auto-learn ranks. Empty uses
         * `player_level - level_limit + 1`. */
        std::vector<int> auto_learn_levels;
    };

    /** Owns authoritative map, definition, spawn, and monster state. */
    class GameWorld {
      public:
        /** Loads area cells around a spawn point.
         * @param maps_dir area-map root directory.
         * @param spawn_x Spawn X coordinate.
         * @param spawn_y Spawn Y coordinate.
         * @param radius_cells Cell radius.
         * @return True when at least one cell is loaded.
         */
        [[nodiscard]] bool
        load_maps_around(const std::string& maps_dir, int spawn_x, int spawn_y, int radius_cells);

        /** Loads map cells covering configured spawn rectangles.
         * @param maps_dir area-map root directory.
         * @return True when map data is available.
         */
        [[nodiscard]] bool ensure_maps_for_spawns(const std::string& maps_dir);

        /** Loads map definition cells within a spawn-centered radius.
         * @return True when map data is available.
         */
        [[nodiscard]] bool load_map_catalog(const std::string& map_catalog_path,
                                            const std::string& maps_dir,
                                            int spawn_x,
                                            int spawn_y,
                                            int radius_cells);

        /** Loads combat item definitions from JSON files. */
        [[nodiscard]] bool load_items_json_dir(const std::string& dir);

        /** Loads strict pet gameplay definitions from config/pets/catalog.json. */
        [[nodiscard]] bool load_pets_json_dir(const std::string& dir);

        /** Loads strict common riding definitions from config/riding/catalog.json. */
        [[nodiscard]] bool load_riding_json_dir(const std::string& dir);

        /**
         * Loads prefix combat bonuses from JSON files.
         * @param dir Prefix JSON directory.
         * @return True when at least one prefix was loaded.
         */
        [[nodiscard]] bool load_prefixes_json_dir(const std::string& dir);

        /**
         * Loads Beads of Fire stats from a JSON file.
         * @param path Path to config/bof.json.
         * @return True when the catalog loaded.
         */
        [[nodiscard]] bool load_bof_json(const std::string& path);

        /** Loads percentage drop tables (groups then itemgroups).
         * @param groups_dir Inner group JSON directory.
         * @param itemgroups_dir Outer itemgroup JSON directory.
         * @return True when both directories load at least one table.
         */
        [[nodiscard]] bool load_drop_tables(const std::string& groups_dir,
                                            const std::string& itemgroups_dir);

        /** Rolls drops for a killed monster using its definition's item_groups,
         *  plus the reserved global itemgroup once for normal combat races.
         * @param def_index Monster definition index.
         * @param corpse_x Death X in server pixels.
         * @param corpse_y Death Y in server pixels.
         * @return Scattered drop outcomes (empty when no tables / no hits).
         */
        [[nodiscard]] std::vector<DropOutcome>
        roll_monster_drops(int def_index, int corpse_x, int corpse_y) const;

        /** Rolls quest-gated kill drops for matching active quest flags.
         * @param def_index Monster definition index.
         * @param corpse_x Death X in server pixels.
         * @param corpse_y Death Y in server pixels.
         * @param quest_index Quest index that matched CheckQuestFlag.
         * @param quest_flag Quest flag that matched.
         * @return Scattered drop outcomes for one matching quest drop slot set.
         */
        [[nodiscard]] std::vector<DropOutcome>
        roll_monster_quest_drops(int def_index,
                                 int corpse_x,
                                 int corpse_y,
                                 std::uint16_t quest_index,
                                 std::uint8_t quest_flag) const;

        /** Returns loaded drop tables. */
        [[nodiscard]] const ItemGroupDb& drop_tables() const {
            return m_drop_tables;
        }

        /** Finds a monster definition by index.
         * @param index Monster definition index.
         * @return Definition, or nullptr.
         */
        [[nodiscard]] const MonsterDef* find_def(int index) const;

        /**
         * Creates one configured monster at an exact world position.
         * @param id_gen Next entity identifier; advanced only on success.
         * @param def_index Monster definition index.
         * @param map Logical destination map.
         * @param x Server X coordinate.
         * @param y Server Y coordinate.
         * @param z Server Z coordinate.
         * @return New entity id, or std::nullopt when the definition or tile is invalid.
         */
        [[nodiscard]] std::optional<std::uint32_t>
        spawn_monster_at(std::uint32_t& id_gen, int def_index, int map, int x, int y, int z);

        /** Finds the first GenMonster spawn with the given area id.
         * @param area GenMonster / JSON `area` value.
         * @return Spawn rectangle, or nullptr.
         */
        [[nodiscard]] const MonsterSpawn* find_spawn_by_area(int area) const;

        /** Loads skill definitions from JSON files. */
        [[nodiscard]] bool load_skills_json_dir(const std::string& dir);

        /** Loads monster definitions and spawns, applying startup stat modifiers.
         * @param dir Monster JSON directory.
         * @param spawn_x Spawn center X in pixels.
         * @param spawn_y Spawn center Y in pixels.
         * @param max_dist_px Spawn rectangle filter radius, or zero for all spawns.
         * @param strength_path Optional monster strength JSON path; empty uses authored stats.
         * @return True when modifiers are valid and at least one monster definition loaded.
         */
        [[nodiscard]] bool load_monsters_json_dir(const std::string& dir,
                                                  int spawn_x,
                                                  int spawn_y,
                                                  int max_dist_px,
                                                  const std::string& strength_path = {});

        /**
         * Aligns spawn map indices with named indoor map definition cells (mapinfo.json).
         * Tests and conversion use this. Host does not remap at load: spawn.map
         * in JSON must already match the dungeon (D6 = 25/32/…, not 31).
         * @param maps Loaded mapinfo.json / map definition catalog.
         */
        void apply_named_spawn_maps(const map::MapCatalog& maps);

        /**
         * Moves field gens onto overworld only when their physical map definition cell is
         * explicitly index 0. Later-pack GenMonster uses map 6 / 100 for such
         * outdoor cells. Authored instance maps such as Emok (20) stay intact.
         * @param maps Complete map definition catalog.
         */
        void bind_field_spawns_to_overworld(const map::MapCatalog& maps);

        /** Rebuilds live monsters from configured spawn rectangles. */
        void spawn_from_config(std::uint32_t& id_gen,
                               int max_monsters = 64,
                               int prefer_x = 0,
                               int prefer_y = 0);

        /** Returns the loaded map grid. */
        [[nodiscard]] const map::WorldGrid& grid() const {
            return m_grid;
        }

        /** Returns the mutable map grid. */
        [[nodiscard]] map::WorldGrid& grid() {
            return m_grid;
        }

        /** Returns mutable live monsters. */
        [[nodiscard]] std::vector<MonsterEntity>& monsters() {
            return m_monsters;
        }

        /** Returns live monsters. */
        [[nodiscard]] const std::vector<MonsterEntity>& monsters() const {
            return m_monsters;
        }

        /** Returns mutable ground items. */
        [[nodiscard]] std::vector<GroundItem>& ground_items() {
            return m_ground_items;
        }

        /** Returns ground items. */
        [[nodiscard]] const std::vector<GroundItem>& ground_items() const {
            return m_ground_items;
        }

        /** Spawns a ground item from a drop roll.
         * @param iid Persistent item iid used as the world id.
         * @param drop Rolled drop outcome.
         * @param owner_player_id Temporary loot owner (0 = free).
         * @param ownership_sec Ownership lock duration in seconds.
         * @param despawn_sec Despawn timer in seconds.
         * @param map Logical map of the drop.
         * @param owner_party_id Temporary owning party (0 = no party ownership).
         * @param server_owned True to block every player until ownership_sec expires.
         * @return Reference to the spawned ground item.
         */
        GroundItem& spawn_ground_item(std::uint32_t iid,
                                      const DropOutcome& drop,
                                      std::uint32_t owner_player_id = 0,
                                      float ownership_sec = 120.0f,
                                      float despawn_sec = 180.0f,
                                      int map = 0,
                                      std::uint32_t owner_party_id = 0,
                                      bool server_owned = false);

        /** Removes a ground item by id.
         * @param id World entity id.
         * @return True when removed.
         */
        [[nodiscard]] bool remove_ground_item(std::uint32_t id);

        /** Finds a mutable ground item by id.
         * @param id World entity id.
         * @return Item, or nullptr.
         */
        [[nodiscard]] GroundItem* find_ground_item(std::uint32_t id);

        /** Finds a ground item by id.
         * @param id World entity id.
         * @return Item, or nullptr.
         */
        [[nodiscard]] const GroundItem* find_ground_item(std::uint32_t id) const;

        /** Advances ground-item ownership and despawn timers.
         * @param dt Elapsed seconds.
         * @param removed_ids Receives ids that despawned this tick.
         */
        void tick_ground_items(float dt, std::vector<std::uint32_t>& removed_ids);

        /** Returns loaded monster definitions. */
        [[nodiscard]] const std::vector<MonsterDef>& defs() const {
            return m_defs;
        }

        /** Applies quest-only special item categories after all acquisition catalogs load.
         * @param indices Quest-granted indices with no configured loot source.
         */
        void classify_task_items(const std::unordered_set<int>& indices);

        /** @return Loaded item definitions. */
        [[nodiscard]] const std::unordered_map<int, ItemCombatDef>& item_defs() const {
            return m_item_defs;
        }

        /** Returns pet definitions keyed by item definition index. */
        [[nodiscard]] const std::unordered_map<int, PetDef>& pet_defs() const {
            return m_pet_defs;
        }

        /** Returns common riding definitions keyed by item definition index. */
        [[nodiscard]] const std::unordered_map<int, RidingDef>& riding_defs() const {
            return m_riding_defs;
        }

        /** Returns loaded prefix combat definitions. */
        [[nodiscard]] const std::unordered_map<int, PrefixCombatDef>& prefix_defs() const {
            return m_prefix_defs;
        }

        /** @return Beads of Fire catalog. */
        [[nodiscard]] const BofCatalog& bof() const {
            return m_bof;
        }

        /** Returns loaded skill definitions. */
        [[nodiscard]] const std::vector<SkillDef>& skill_defs() const {
            return m_skill_defs;
        }

        /** Returns configured monster spawn rectangles. */
        [[nodiscard]] const std::vector<MonsterSpawn>& spawns() const {
            return m_spawns;
        }

        /** Advances roaming without player targets. */
        void tick(float dt, std::vector<MonsterMoveEvent>& moves_out);

        /** Advances roaming and chase behavior. */
        void tick(float dt,
                  const std::vector<PlayerPose>& players,
                  std::vector<MonsterMoveEvent>& moves_out);

        /** Advances all monster AI and emits movement and attacks. */
        void tick(float dt,
                  const std::vector<PlayerPose>& players,
                  std::vector<MonsterMoveEvent>& moves_out,
                  std::vector<MonsterAttackEvent>& attacks_out);

        /** Advances monster AI and also emits chest open completions. */
        void tick(float dt,
                  const std::vector<PlayerPose>& players,
                  std::vector<MonsterMoveEvent>& moves_out,
                  std::vector<MonsterAttackEvent>& attacks_out,
                  std::vector<ChestOpenEvent>& chests_out);

        /** Advances monster AI, chests, and combat-corpse timers. */
        void tick(float dt,
                  const std::vector<PlayerPose>& players,
                  std::vector<MonsterMoveEvent>& moves_out,
                  std::vector<MonsterAttackEvent>& attacks_out,
                  std::vector<ChestOpenEvent>& chests_out,
                  std::vector<MonsterCorpseEvent>& corpses_out);

        /** Registers plant definitions for population accounting and harvest completion.
         * @param indices Monster definition indices configured as gathering plants.
         */
        void set_harvest_plant_definitions(const std::vector<int>& indices);

        /**
         * Starts opening a treasure-box monster (compatibility behavior attack=0).
         * @param player_id Opener.
         * @param monster_id Chest entity.
         * @param open_sec Opening duration (legacy 6, or 2 with luck).
         * @return 0 ok, 1 missing/invalid, 2 already used by another player.
         */
        [[nodiscard]] int
        begin_open_chest(std::uint32_t player_id, std::uint32_t monster_id, float open_sec = 6.0f);

        /**
         * Cancels chest opening for a player (C2S_CANCELITEMMONSTER / move).
         * @param player_id Opener.
         * @param monster_id Optional specific chest; 0 cancels any.
         * @return True when an open was canceled.
         */
        [[nodiscard]] bool cancel_open_chest(std::uint32_t player_id, std::uint32_t monster_id = 0);

        /** Tests whether a world pixel is walkable. */
        [[nodiscard]] bool walkable_pixel(int px, int py) const;

        /** Tests whether a world pixel permits monster spawning. */
        [[nodiscard]] bool monster_spawnable_pixel(int px, int py) const;

        /** Loads the area cell covering a world pixel when needed. */
        [[nodiscard]] bool ensure_cell_for_pixel(int px, int py);

        /** Tops up configured monster populations around a point.
         * Nearby gens are filled nearest-first so a local hunting ground is not
         * starved by farther GenMonster records that happen to load earlier.
         * @param px Player server X.
         * @param py Player server Y.
         * @param radius_px Include gens whose rect is within this distance.
         * @param id_gen Next monster entity id.
         * @param max_monsters Resident population budget per separated viewer neighborhood.
         * @param map Logical map filter, or -1 for every map.
         * @param viewers Active world viewers; their visible monsters cannot be evicted.
         * Empty retains the singleplayer population cap.
         * @return Number of newly created monsters.
         */
        [[nodiscard]] int ensure_monsters_around(int px,
                                                 int py,
                                                 int radius_px,
                                                 std::uint32_t& id_gen,
                                                 int max_monsters,
                                                 int map = -1,
                                                 const std::vector<PlayerPose>& viewers = {});

        /**
         * Routes Castle War flag damage to the event's authority.
         * @param handler Callback returning damage actually applied by the event.
         */
        void set_castle_flag_damage(
            std::function<std::int32_t(std::uint32_t, MonsterEntity&, std::int32_t)> handler);

        /** Applies player damage and aggro to a monster.
         * @param player_id Attacking player entity id.
         * @param monster_id Damaged monster entity id.
         * @param damage Authoritative damage after combat resolution.
         * @param px Attacking player X coordinate.
         * @param py Attacking player Y coordinate.
         * @param physical True for physical damage and false for magical damage.
         * @return Damage applied, or zero for an invalid target.
         */
        [[nodiscard]] std::int32_t apply_player_attack(std::uint32_t player_id,
                                                       std::uint32_t monster_id,
                                                       std::int32_t damage,
                                                       int px = 0,
                                                       int py = 0,
                                                       bool physical = true);

        /**
         * Applies explicit skill hostility without dealing damage.
         * @param player_id Player receiving aggro.
         * @param monster_id Target monster.
         * @param hostility Positive hostility amount.
         * @param px Player X used to orient the monster.
         * @param py Player Y used to orient the monster.
         * @return True when applied to a living combat monster.
         */
        [[nodiscard]] bool apply_monster_hostility(std::uint32_t player_id,
                                                   std::uint32_t monster_id,
                                                   std::int32_t hostility,
                                                   int px,
                                                   int py);

        /** Replaces one player's accumulated hostility, or removes it at zero.
         * @param player_id Player whose hostility changes.
         * @param monster_id Target monster.
         * @param hostility New non-negative hostility value.
         * @param px Player X used for safe-zone/range validation when positive.
         * @param py Player Y used for safe-zone/range validation when positive.
         * @return True when the hostility entry was updated.
         */
        [[nodiscard]] bool set_monster_hostility(std::uint32_t player_id,
                                                 std::uint32_t monster_id,
                                                 std::int32_t hostility,
                                                 int px,
                                                 int py);

        /** Clears hostility toward one player. */
        void clear_aggro_on_player(std::uint32_t player_id,
                                   std::vector<MonsterMoveEvent>* moves_out = nullptr);

        /**
         * Clears all current hostility from one monster.
         * @param monster_id Monster identifier.
         * @param moves_out Optional immediate movement-stop event destination.
         * @return True when a living combat monster was found.
         */
        [[nodiscard]] bool clear_monster_aggro(std::uint32_t monster_id,
                                               std::vector<MonsterMoveEvent>* moves_out = nullptr);

        /** Finds a mutable monster by identifier. */
        [[nodiscard]] MonsterEntity* find_monster(std::uint32_t id);

        /** Finds a monster by identifier. */
        [[nodiscard]] const MonsterEntity* find_monster(std::uint32_t id) const;

        /**
         * Selects living combat monsters inside a circular area.
         * @param center_x Center X in server pixels.
         * @param center_y Center Y in server pixels.
         * @param map Logical map.
         * @param radius_pixels Inclusive radius in server pixels.
         * @param max_targets Maximum returned ids; zero means unlimited.
         * @return Monster ids in stable world order.
         */
        [[nodiscard]] std::vector<std::uint32_t> monsters_in_radius(
            int center_x, int center_y, int map, int radius_pixels, int max_targets = 0) const;

        /**
         * Selects living combat monsters inside a Chebyshev tile square.
         * Matches compatibility behavior / GetPlayerMonsterListInRect (center ± radius tiles).
         * @param center_x Center X in server pixels.
         * @param center_y Center Y in server pixels.
         * @param map Logical map.
         * @param radius_tiles Inclusive tile radius.
         * @param max_targets Maximum returned ids; zero means unlimited.
         * @return Monster ids in stable world order.
         */
        [[nodiscard]] std::vector<std::uint32_t> monsters_in_tile_rect(
            int center_x, int center_y, int map, int radius_tiles, int max_targets = 0) const;

        /**
         * Applies a timed movement/action lock to a monster.
         * @param monster_id Monster identifier.
         * @param duration_sec Positive duration in seconds.
         * @param break_on_damage Whether positive player damage clears the lock.
         * @return True when applied to a living combat monster.
         */
        [[nodiscard]] bool
        apply_monster_control(std::uint32_t monster_id, float duration_sec, bool break_on_damage);

        /**
         * Applies Ice Magic Mastery slow (`BUFF_MOVESPEED`).
         * @param monster_id Monster identifier.
         * @param duration_sec Positive duration in seconds (`PtIce + 2`).
         * @param percent Negative move-speed percent (`-20 * PtIce`).
         * @return True when applied to a living combat monster.
         */
        [[nodiscard]] bool
        apply_monster_slow(std::uint32_t monster_id, float duration_sec, int percent);

      private:
        /** Activates a tick-local occupancy snapshot and restores any previous query context. */
        class TickOccupancyScope;

        // Allows predicate and combat-input characterization without changing runtime behavior.
        friend class GameWorldCoveragePeer;

        /** Picks a 32px roam step that is free of terrain and other bodies.
         * @param m Roaming monster.
         * @param players Player poses that occupy cells.
         */
        void pick_roam_target(MonsterEntity& m, const std::vector<PlayerPose>& players);

        /** Picks a chase step, pathing around occupied 8×8 cells when blocked.
         * @param m Chasing monster.
         * @param px Prey pixel X.
         * @param py Prey pixel Y.
         * @param players Player poses that occupy cells.
         * @return True when a chase step or attack transition was selected.
         */
        [[nodiscard]] bool
        pick_chase_target(MonsterEntity& m, int px, int py, const std::vector<PlayerPose>& players);

        /** Occupied pixel of a live monster (destination while a move is queued/in flight).
         * @param m Monster.
         * @param ox Receives occupy X.
         * @param oy Receives occupy Y.
         */
        static void occupy_pixel(const MonsterEntity& m, int& ox, int& oy);

        /** True when another blocking body already owns this 8×8 occupancy cell.
         * @param self_id Monster to ignore (0 = none).
         * @param px Pixel X.
         * @param py Pixel Y.
         * @param map Logical map.
         * @param players Optional player poses that also occupy cells.
         * @return True when the cell is taken.
         */
        [[nodiscard]] bool occupancy_blocked(std::uint32_t self_id,
                                             int px,
                                             int py,
                                             int map,
                                             const std::vector<PlayerPose>* players) const;

        /** compatibility behavior — 8px samples along a step, terrain + occupancy.
         * @param self_id Moving monster.
         * @param from_x Start pixel X.
         * @param from_y Start pixel Y.
         * @param dx Step X.
         * @param dy Step Y.
         * @param map Logical map.
         * @param players Player poses that occupy cells.
         * @return True when the step is free.
         */
        [[nodiscard]] bool is_move_tile(std::uint32_t self_id,
                                        int from_x,
                                        int from_y,
                                        int dx,
                                        int dy,
                                        int map,
                                        const std::vector<PlayerPose>& players) const;

        [[nodiscard]] bool load_one_cell(const std::string& maps_dir, int cell_x, int cell_y);
        /**
         * Places a monster on a walkable, non-village pixel inside the gen rect.
         * @param g Spawn rectangle.
         * @param def Monster definition.
         * @param id New entity id.
         * @param out Filled entity when placement succeeds.
         * @param check_occupancy Whether existing monsters exclude the spawn pixel.
         * @return False when no valid tile exists (matches compatibility behavior).
         */
        [[nodiscard]] bool try_place_monster(const MonsterSpawn& g,
                                             const MonsterDef& def,
                                             std::uint32_t id,
                                             MonsterEntity& out,
                                             bool check_occupancy = true);
        /** Selects the farthest live, damageable monster outside the protected radius.
         * @param px Destination pixel X.
         * @param py Destination pixel Y.
         * @param keep_radius_px Radius whose monsters remain resident.
         * @param protected_monsters Entity ids visible to active world viewers.
         * @return Selected vector index, or no value when no monster can be replaced.
         */
        [[nodiscard]] std::optional<std::size_t>
        find_farthest_monster(int px,
                              int py,
                              int keep_radius_px,
                              const std::unordered_set<std::uint32_t>& protected_monsters) const;

        /** Loaded area cells. */
        map::WorldGrid m_grid;

        /** Loaded monster definitions. */
        std::vector<MonsterDef> m_defs;

        /** First loaded position in m_defs for each monster definition ID. */
        std::unordered_map<int, std::size_t> m_def_positions;

        /** Loaded item definitions. */
        std::unordered_map<int, ItemCombatDef> m_item_defs;

        /** Loaded pet gameplay definitions. */
        std::unordered_map<int, PetDef> m_pet_defs;

        /** Loaded common riding gameplay definitions. */
        std::unordered_map<int, RidingDef> m_riding_defs;

        /** Loaded prefix combat definitions. */
        std::unordered_map<int, PrefixCombatDef> m_prefix_defs;

        /** Beads of Fire catalog. */
        BofCatalog m_bof;

        /** Percentage drop tables. */
        ItemGroupDb m_drop_tables;

        /** Loaded skill definitions. */
        std::vector<SkillDef> m_skill_defs;

        /** Configured spawn rectangles. */
        std::vector<MonsterSpawn> m_spawns;

        /** Rebuilds the ordered map buckets after the spawn catalog or its map assignments change. */
        void rebuild_spawn_map_index();

        /** Spawn vector positions in catalog order, grouped by logical map. */
        std::unordered_map<int, std::vector<std::size_t>> m_spawn_indices_by_map;

        /** Live monster entities. */
        std::vector<MonsterEntity> m_monsters;

        /** Objective damage never reaches the normal corpse and reward lifecycle. */
        std::function<std::int32_t(std::uint32_t, MonsterEntity&, std::int32_t)>
            m_castle_flag_damage;

        /** Occupancy snapshot borrowed only while the current tick processes monsters. */
        WorldOccupancyIndex* m_tick_occupancy = nullptr;

        /** Current tick scope, used to build reservations on its first occupancy query. */
        TickOccupancyScope* m_tick_occupancy_scope = nullptr;

        /** Player poses represented by the active tick snapshot. */
        const std::vector<PlayerPose>* m_tick_players = nullptr;

        /** Allows the coverage peer to compare ticks against the scan oracle. */
        bool m_use_occupancy_index = true;

        /** Configured gathering definitions, independent of ordinary monster caps. */
        std::unordered_set<int> m_harvest_plant_indices;

        /** Live ground loot entities. */
        std::vector<GroundItem> m_ground_items;

        /** area-map root used for on-demand loading. */
        std::string m_maps_dir;
    };

}  // namespace openkal::server
