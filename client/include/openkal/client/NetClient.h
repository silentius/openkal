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

#include "openkal/client/BuffSlotBar.h"
#include "openkal/client/CombatFloatText.h"
#include "openkal/client/RemotePlayerMotion.h"
#include "openkal/client/ServerClock.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Cooking.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/ExpEventState.h"
#include "openkal/protocol/Honor.h"
#include "openkal/protocol/Guild.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemInfo.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/MapState.h"
#include "openkal/protocol/Party.h"
#include "openkal/protocol/PlayerOptions.h"
#include "openkal/protocol/Pvp.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Riding.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/Storage.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/protocol/Trade.h"
#include "openkal/protocol/TradingPost.h"
#include "openkal/protocol/UpgradeItem.h"
#include "openkal/server/net/Transport.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace openkal::client {

    class Catalog;
    class ItemDb;

    /** Latched monster→player S2C_ATTACK presentation for `(*EFFECT_DAMAGE)`. */
    struct PendingMonsterActionDamage {
        /** Packet damage shown as a combat float (0 on miss/block). */
        std::int32_t damage = 0;

        /** S2C_ATTACK flags (hit / fatal / block / explosive). */
        std::uint8_t flags = 0;

        /** Target entity id (local or remote player). */
        std::uint32_t target_id = 0;
    };

    /** Network-synced monster instance for client prediction and render. */
    class NetMonster {
      public:
        [[nodiscard]] std::uint32_t id() const {
            return m_id;
        }
        void set_id(std::uint32_t v) {
            m_id = v;
        }

        [[nodiscard]] int index() const {
            return m_index;
        }
        void set_index(int v) {
            m_index = v;
        }

        [[nodiscard]] int race() const {
            return m_race;
        }
        void set_race(int v) {
            m_race = v;
        }

        [[nodiscard]] int x() const {
            return m_x;
        }
        void set_x(int v) {
            m_x = v;
        }

        [[nodiscard]] int y() const {
            return m_y;
        }
        void set_y(int v) {
            m_y = v;
        }

        [[nodiscard]] int dir() const {
            return m_dir;
        }
        void set_dir(int v) {
            m_dir = v;
        }

        [[nodiscard]] std::int32_t hp() const {
            return m_hp;
        }
        void set_hp(std::int32_t v) {
            m_hp = v;
        }

        [[nodiscard]] std::int32_t max_hp() const {
            return m_max_hp;
        }
        void set_max_hp(std::int32_t v) {
            m_max_hp = v;
        }

        [[nodiscard]] const std::string& name() const {
            return m_name;
        }
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** Client-space X (1 unit == 1 server pixel; origin-shifted). */
        [[nodiscard]] float wx() const {
            return m_wx;
        }
        void set_wx(float v) {
            m_wx = v;
        }

        [[nodiscard]] float wy() const {
            return m_wy;
        }
        void set_wy(float v) {
            m_wy = v;
        }

        [[nodiscard]] float wz() const {
            return m_wz;
        }
        void set_wz(float v) {
            m_wz = v;
        }

        [[nodiscard]] bool moving() const {
            return m_moving;
        }
        void set_moving(bool v) {
            m_moving = v;
        }

        /** Stops interpolation immediately (Kal ModelChange away from walk/run → MoveEnd). */
        void halt_motion() {
            m_moving = false;
            m_has_abs_target = false;
            m_stop_on_arrive = false;
            m_attack_after_move = false;
            m_tx = m_wx;
            m_tz = m_wz;
        }

        [[nodiscard]] bool has_abs_target() const {
            return m_has_abs_target;
        }
        void set_has_abs_target(bool v) {
            m_has_abs_target = v;
        }

        /** MoveMonsterEnd: idle when reaching the absolute target. Chase MoveOn keeps the run clip.
         */
        [[nodiscard]] bool stop_on_arrive() const {
            return m_stop_on_arrive;
        }
        void set_stop_on_arrive(bool v) {
            m_stop_on_arrive = v;
        }

        [[nodiscard]] float tx() const {
            return m_tx;
        }
        void set_tx(float v) {
            m_tx = v;
        }

        [[nodiscard]] float tz() const {
            return m_tz;
        }
        void set_tz(float v) {
            m_tz = v;
        }

        /** macro-e mspeed walk/run px/s. */
        [[nodiscard]] float move_speed() const {
            return m_move_speed;
        }
        void set_move_speed(float v) {
            m_move_speed = v;
        }

        [[nodiscard]] float walk_speed() const {
            return m_walk_speed;
        }
        void set_walk_speed(float v) {
            m_walk_speed = v;
        }

        [[nodiscard]] float run_speed() const {
            return m_run_speed;
        }
        void set_run_speed(float v) {
            m_run_speed = v;
        }

        [[nodiscard]] bool running() const {
            return m_running;
        }
        void set_running(bool v) {
            m_running = v;
            refresh_move_speed();
        }

        [[nodiscard]] float yaw() const {
            return m_yaw;
        }
        void set_yaw(float v) {
            m_yaw = v;
        }

        [[nodiscard]] bool pending_attack() const {
            return m_pending_attack;
        }
        void set_pending_attack(bool v) {
            m_pending_attack = v;
        }

        /**
         * Latches ActionDamage until the attack clip is started.
         * @param hit Packet damage, flags, and target id.
         */
        void set_pending_action_damage(PendingMonsterActionDamage hit) {
            m_action_damage = std::move(hit);
            m_pending_action_damage = true;
        }

        /**
         * Consumes a latched ActionDamage payload.
         * @param out Receives the hit.
         * @return True if a payload was pending.
         */
        [[nodiscard]] bool take_pending_action_damage(PendingMonsterActionDamage& out) {
            if (!m_pending_action_damage) {
                return false;
            }
            m_pending_action_damage = false;
            out = m_action_damage;
            return true;
        }

        /** True when S2C_ATTACK arrived during an in-flight interpolation (Kal runtime state).
         * @return True when the swing is deferred until this step arrives.
         */
        [[nodiscard]] bool attack_after_move() const {
            return m_attack_after_move;
        }

        /** Queues the attack clip until the current MoveOn/End dest is reached.
         * @param v True to play the swing after this step finishes.
         */
        void set_attack_after_move(bool v) {
            m_attack_after_move = v;
        }

        [[nodiscard]] bool pending_die() const {
            return m_pending_die;
        }
        void set_pending_die(bool v) {
            m_pending_die = v;
        }

        [[nodiscard]] bool pending_knee() const {
            return m_pending_knee;
        }
        void set_pending_knee(bool v) {
            m_pending_knee = v;
        }

        [[nodiscard]] bool pending_behead() const {
            return m_pending_behead;
        }
        void set_pending_behead(bool v) {
            m_pending_behead = v;
        }

        [[nodiscard]] std::uint32_t gstate() const {
            return m_gstate;
        }
        void set_gstate(std::uint32_t v) {
            m_gstate = v;
        }

        [[nodiscard]] bool server_removed() const {
            return m_server_removed;
        }
        void set_server_removed(bool v) {
            m_server_removed = v;
        }

        [[nodiscard]] bool finishing_death() const {
            return m_finishing_death;
        }
        void set_finishing_death(bool v) {
            m_finishing_death = v;
        }

        /** True while the Maguni corpse is beheadable (compatibility behavior). */
        [[nodiscard]] bool kneeling() const {
            return (m_gstate & protocol::kCgsKnee) != 0;
        }

        /**
         * Advances wx/wz toward tx/tz using move_speed; clears moving when arrived.
         * @param dt Frame delta seconds.
         */
        void tick_motion(float dt);

        /**
         * Applies macro-e mspeed ticks → walk/run/move px/s.
         * @param walk_tick Walk speed tick value.
         * @param run_tick Run speed tick value.
         */
        void apply_mspeed_from_ticks(float walk_tick, float run_tick);

        /**
         * Applies P_MSPEED signed percent (`runtime state`).
         * @param speed_value Negative slows; 0 restores base walk/run.
         */
        void apply_speed_value(std::int16_t speed_value);

        /** @return Signed P_MSPEED percent. */
        [[nodiscard]] std::int16_t speed_value() const {
            return m_speed_value;
        }

        /**
         * Animation dt scale matching `counter * speedno / tick`.
         * @return Multiplier applied to clip time.
         */
        [[nodiscard]] float anim_speed_scale() const;

        /** @return True while compatibility behavior or compatibility behavior tints the mesh cyan.
         */
        [[nodiscard]] bool ice_slowed() const {
            return m_ice_slowed;
        }

        /**
         * Sets the ice-slow visual flag from S2C_MSTATE.
         * @param v True while compatibility behavior or compatibility behavior is set.
         */
        void set_ice_slowed(bool v) {
            m_ice_slowed = v;
        }

        /** @return True while compatibility behavior freeze is active. */
        [[nodiscard]] bool move_stopped() const {
            return m_move_stopped;
        }

        /**
         * Sets the freeze visual flag from S2C_MSTATE.
         * @param v True while compatibility behavior is set.
         */
        void set_move_stopped(bool v) {
            m_move_stopped = v;
        }

      private:
        /**
         * Recomputes runtime state from walk/run and P_MSPEED.
         */
        void refresh_move_speed();

        /**
         * Speed factor from `runtime state`.
         * @param speed_value Signed percent.
         * @return Move/anim multiplier.
         */
        [[nodiscard]] static float speed_factor(std::int16_t speed_value);

        /** World entity id. */
        std::uint32_t m_id = 0;

        /** monster definition / catalog index. */
        int m_index = 0;

        /** Race / species id from CreateMonster. */
        int m_race = 0;

        /** Last server tile/pixel X from spawn or move sync. */
        int m_x = 0;

        /** Last server tile/pixel Y from spawn or move sync. */
        int m_y = 0;

        /** Facing direction nibble from the server. */
        int m_dir = 0;

        /** Current HP. */
        std::int32_t m_hp = 0;

        /** Max HP. */
        std::int32_t m_max_hp = 0;

        /** Display name from CreateMonster. */
        std::string m_name;

        /** Client-space X (1 unit == 1 server pixel; origin-shifted). */
        float m_wx = 0;

        /** Client-space Y (height). */
        float m_wy = 0;

        /** Client-space Z. */
        float m_wz = 0;

        /** True while interpolating toward an absolute target. */
        bool m_moving = false;

        /** True when tx/tz are a valid absolute move target. */
        bool m_has_abs_target = false;

        /** Idle when arriving (MoveMonsterEnd). */
        bool m_stop_on_arrive = false;

        /** Absolute move target X in client space. */
        float m_tx = 0;

        /** Absolute move target Z in client space. */
        float m_tz = 0;

        /** Active move speed in px/s (walk or run). */
        float m_move_speed = 20.0f;

        /** Walk speed in px/s from macro-e mspeed. */
        float m_walk_speed = 20.0f;

        /** Run / chase speed in px/s from macro-e mspeed. */
        float m_run_speed = 40.0f;

        /** True when using run speed for the current move. */
        bool m_running = false;

        /** P_MSPEED signed percent (`runtime state`). */
        std::int16_t m_speed_value = 0;

        /** True while compatibility behavior is set on this entity. */
        bool m_ice_slowed = false;

        /** True while compatibility behavior is set on this entity. */
        bool m_move_stopped = false;

        /** Facing yaw in radians for render. */
        float m_yaw = 0.0f;

        /** Latched attack presentation for the next render tick. */
        bool m_pending_attack = false;

        /** Play the attack clip after the current interpolation finishes. */
        bool m_attack_after_move = false;

        /** True when ActionDamage is waiting for the attack clip to start. */
        bool m_pending_action_damage = false;

        /** Latched ActionDamage payload. */
        PendingMonsterActionDamage m_action_damage;

        /** Latched AT_DIE presentation (chest open / mob death). */
        bool m_pending_die = false;

        /** Latched AT_KNEE presentation (Maguni beheadable corpse). */
        bool m_pending_knee = false;

        /** Latched AT_BEHEADED presentation. */
        bool m_pending_behead = false;

        /** CreateMonster / action gstate (compatibility behavior while kneeling). */
        std::uint32_t m_gstate = 0;

        /** True after RemoveMonster while a local death clip is still playing. */
        bool m_server_removed = false;

        /** True after AT_DIE / AT_BEHEADED until the local clip is finished. */
        bool m_finishing_death = false;
    };

    /** Network-synced ground loot for render and pickup. */
    struct NetGroundItem {
        /** World entity id. */
        std::uint32_t id = 0;

        /** item definition index. */
        int index = 0;

        /** Stack count or money amount. */
        int count = 1;

        /** Client-space X. */
        float wx = 0.0f;

        /** Client-space Z. */
        float wz = 0.0f;

        /**
         * Seconds since CreateItem (for spawn bounce). Reset to 0 when the packet
         * arrives.
         */
        float age_sec = 0.0f;

        /** Client-space X where the pouch starts its fall (character or corpse). */
        float origin_wx = 0.0f;

        /** Client-space Z where the pouch starts its fall (character or corpse). */
        float origin_wz = 0.0f;
    };

    /** Network-synced NPC instance for pick / AskNpc. */
    class NetNpc {
      public:
        /** @return Entity id. */
        [[nodiscard]] std::uint32_t id() const {
            return m_id;
        }

        /** @param v Entity id. */
        void set_id(std::uint32_t v) {
            m_id = v;
        }

        /** @return GenNPC index. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /** @param v GenNPC index. */
        void set_index(int v) {
            m_index = v;
        }

        /** @return Shape id. */
        [[nodiscard]] int shape() const {
            return m_shape;
        }

        /** @param v Shape id. */
        void set_shape(int v) {
            m_shape = v;
        }

        /** @return Server X. */
        [[nodiscard]] int x() const {
            return m_x;
        }

        /** @param v Server X. */
        void set_x(int v) {
            m_x = v;
        }

        /** @return Server Y. */
        [[nodiscard]] int y() const {
            return m_y;
        }

        /** @param v Server Y. */
        void set_y(int v) {
            m_y = v;
        }

        /** @return Server Z. */
        [[nodiscard]] int z() const {
            return m_z;
        }

        /** @param v Server Z. */
        void set_z(int v) {
            m_z = v;
        }

        /** @return Client X. */
        [[nodiscard]] float wx() const {
            return m_wx;
        }

        /** @param v Client X. */
        void set_wx(float v) {
            m_wx = v;
        }

        /** @return Client Z. */
        [[nodiscard]] float wz() const {
            return m_wz;
        }

        /** @param v Client Z. */
        void set_wz(float v) {
            m_wz = v;
        }

        /** @return Client Y (spawn height, not GetFloor). */
        [[nodiscard]] float wy() const {
            return m_wy;
        }

        /** @param v Client Y. */
        void set_wy(float v) {
            m_wy = v;
        }

        /** @return Server-authoritative NPC scenario state. */
        [[nodiscard]] std::uint32_t gstate() const {
            return m_gstate;
        }

        /** @param v Server-authoritative NPC scenario state. */
        void set_gstate(std::uint32_t v) {
            m_gstate = v;
        }

      private:
        /** Runtime id. */
        std::uint32_t m_id = 0;

        /** GenNPC index. */
        int m_index = 0;

        /** Shape. */
        int m_shape = 0;

        /** Server pixels. */
        int m_x = 0;

        /** Server pixels. */
        int m_y = 0;

        /** Server pixels. */
        int m_z = 0;

        /** Client space. */
        float m_wx = 0.0f;

        /** Client space Y from server Z (not outdoor GetFloor). */
        float m_wy = 0.0f;

        /** Client space. */
        float m_wz = 0.0f;

        /** Last S2C_GSTATE value for this NPC. */
        std::uint32_t m_gstate = 0;
    };

    /** Pending S2C_INSERTITEM applied to the local bag. */
    struct PendingInsertItem {
        /** Inventory instance id. */
        std::uint32_t iid = 0;

        /** item definition index. */
        int index = 0;

        /** Prefix id. */
        int prefix = 0;

        /** Stack count or money amount. */
        int count = 1;

        /** Insert info flags; bit 0 = replace, bit 2 = worn (ITEM_PUTON). */
        std::uint32_t info = 0;

        /** Optional talisman / Explosive Blow bytes. */
        protocol::InsertCharmFields charm;

        /** Authoritative item expiry as unix seconds, or 0 for permanent. */
        std::uint64_t expire_at = 0;

        /** True when the INSERTITEM extension carried expire_at. */
        bool has_expire_at = false;
    };

    /** Pending S2C_PUTONITEM or S2C_PUTOFFITEM state change. */
    struct PendingItemWear {
        std::uint32_t player_id = 0;
        std::uint32_t iid = 0;
        std::uint16_t item_index = 0;
        /** Unix timestamp for a timed pet or riding item; zero means permanent. */
        std::uint64_t expire_at = 0;
        bool equipped = false;
    };

    /** Pending S2C_CHATTING line. */
    struct PendingChat {
        /** Speaker name. */
        std::string speaker;

        /** Wire text (channel prefix preserved). */
        std::string text;
    };

    /** Pending authoritative map-wide scenario update. */
    struct PendingMapState {
        /** Logical map index. */
        std::uint8_t map = 0;

        /** Scenario-specific phase ordinal. */
        std::uint8_t phase = 0;

        /** Scenario-specific active-object bit mask. */
        std::uint8_t active_mask = 0;

        /** Countdown category supplied by the map scenario. */
        protocol::MapStateTimer timer = protocol::MapStateTimer::None;

        /** Whole seconds remaining in the scenario countdown. */
        std::uint32_t timer_seconds = 0;
    };

    /** Pending battle / system info line for the chat log. */
    struct PendingInfoMessage {
        /** Display text. */
        std::string text;

        /** Battle kind; empty optional = untyped info. */
        std::optional<protocol::BattleMsgKind> kind;

        /** Optional RGB override for messages such as critical damage. */
        std::optional<std::array<float, 3>> color;
    };

    /** A confirmed normal attack to present, including misses. */
    struct PendingPlayerAttack {
        std::uint32_t attacker_id = 0;
        std::uint32_t target_id = 0;
        /** True when the confirmed swing targets another player. */
        bool player_target = false;
    };

    /** Pending monster hit feedback; normal Archer attacks also include misses for matching. */
    struct PendingHitFx {
        /** Attacker entity id (0 when unknown, e.g. some skill paths). */
        std::uint32_t attacker_id = 0;

        /** Target entity id. */
        std::uint32_t target_id = 0;

        /** Skill index for S2C_SKILL, or -1 for a normal attack. */
        int skill_index = -1;

        /** S2C_ATTACK flags (hit / fatal / explosive). */
        std::uint8_t flags = 0;
    };

    /** Pending rising combat number / Missed / Blocked / heal. */
    struct PendingCombatFloat {
        /** Color and copy kind. */
        CombatFloatKind kind = CombatFloatKind::DamageDealt;

        /** Display text. */
        std::string text;

        /** 0 = local player, else monster entity id. */
        std::uint32_t anchor_id = 0;

        /** True when related compatibility states should show the number at OnCrash. */
        bool wait_for_bolt = false;
    };

    /** Pending Fire Magic / Fire Blow dest-info HP drain (`OnDamage` bDelayDamage). */
    struct PendingDelayDamage {
        /** Monster whose dest-info bar should drain. */
        std::uint32_t target_id = 0;

        /** Full subtracted HP (`nCurHP + nCurHP_ADD`). */
        int damage = 0;
    };

    /** Pending S2C_ACTION AT_SKILL telegraph (prep presentation). */
    struct PendingSkillAction {
        /** Caster entity id. */
        std::uint32_t caster_id = 0;

        /** Skill index. */
        std::uint8_t skill = 0;

        /** Target entity id. */
        std::uint32_t target_id = 0;

        /** True when this is fire (S2C_SKILL); false for prep (AT_SKILL). */
        bool fire = false;
    };

    /** Pending S2C_ACTION AT_PRETRANSFORM / AT_TRANSFORM. */
    struct PendingTransformAction {
        /** Entity id. */
        std::uint32_t entity_id = 0;

        /** True for PRE, false for APPLY/REVERT. */
        bool pre = false;

        /** Stage (PRE) or TFlag (APPLY). */
        std::uint8_t arg = 0;
    };

    /** Latest server coordinates and movement state of a visible remote player. */
    struct NetPlayerPosition {
        int x = 0;            ///< Server pixel X.
        int y = 0;            ///< Server pixel Y.
        bool moving = false;  ///< True between MovePlayerOn and MovePlayerEnd.
        /** Authoritative server height, accumulated with horizontal movement. */
        int z = 0;
    };

    /** TCP/Unix game-session client: handshake, move sync, combat packets. */
    class NetClient {
      public:
        /** Selects observation without allowing gameplay requests.
         * @param value True for a read-only session view.
         */
        void set_read_only(bool value) {
            m_read_only = value;
        }

        /** Returns the observed local movement state.
         * @return True until the final confirmed movement segment has finished.
         */
        [[nodiscard]] bool observed_moving() const {
            return m_observed_motion.moving();
        }

        /** Returns whether the session has supplied an authoritative position.
         * @return True after a position snapshot, correction or teleport.
         */
        [[nodiscard]] bool observed_pose_ready() const {
            return m_have_move_baseline;
        }

        /** Returns the interpolated server-confirmed pose for a read-only view.
         * @return Observed local movement, including position and facing.
         */
        [[nodiscard]] const RemotePlayerMotion& observed_motion() const {
            return m_observed_motion;
        }

        /**
         * Connects over a local Unix domain socket.
         * @param path Socket path.
         * @return True on success.
         */
        [[nodiscard]] bool connect_local_unix(const std::string& path);

        /**
         * Connects over TCP.
         * @param host Hostname or IP.
         * @param port TCP port.
         * @return True on success.
         */
        [[nodiscard]] bool connect_tcp(const std::string& host, std::uint16_t port);

        /** Adopts an authenticated TLS lobby connection before world entry.
         * @param session Selected live connection.
         */
        void adopt_session(std::unique_ptr<server::ISession> session);

        /** Completes the KalOnline handshake sequence. @return True on success. */
        [[nodiscard]] bool handshake();

        /**
         * Pumps RX and advances local monster prediction.
         * @param dt Frame delta seconds.
         * @param origin_patch Map origin patch for coordinate conversion.
         */
        void update(float dt, int origin_patch);

        /** Returns visible remote player positions, accumulated in packet order.
         * @return Positions keyed by entity id.
         */
        [[nodiscard]] const std::unordered_map<std::uint32_t, NetPlayerPosition>&
        remote_player_positions() const {
            return m_remote_player_positions;
        }

        /** @return Live monster map keyed by entity id. */
        [[nodiscard]] const std::unordered_map<std::uint32_t, NetMonster>& monsters() const {
            return m_monsters;
        }

        /** @return Live ground items keyed by entity id. */
        [[nodiscard]] const std::unordered_map<std::uint32_t, NetGroundItem>& ground_items() const {
            return m_ground_items;
        }

        /**
         * Consumes a pending bag insert from S2C_INSERTITEM.
         * @param out Receives insert fields.
         * @return True if pending.
         */
        [[nodiscard]] bool take_insert_item(PendingInsertItem& out);

        /** Consumes an authoritative equipment state change. */
        [[nodiscard]] bool take_item_wear(PendingItemWear& out);

        /**
         * Consumes an authoritative common riding snapshot.
         * @param out Receives the state.
         * @return True if a snapshot was pending.
         */
        [[nodiscard]] bool take_riding_state(protocol::RidingState& out);

        /** Consumes one server-confirmed Riding Cache result.
         * @param out Opened-cache contents.
         * @return True when a result was pending.
         */
        [[nodiscard]] bool take_riding_cache_result(protocol::RidingCacheResult& out);

        /**
         * Consumes a pending S2C_UPDATEITEMINFO (egg level/EXP on a bag instance).
         * @param out Receives decoded fields.
         * @return True if pending.
         */
        [[nodiscard]] bool take_update_item_info(protocol::UpdateItemInfoFields& out);

        /**
         * Consumes a pending S2C_TRASHITEM instance id.
         * @param iid Receives the deleted inventory instance id.
         * @return True if pending.
         */
        [[nodiscard]] bool take_trash_item(std::uint32_t& iid);

        /**
         * Sends C2S_CHATTING.
         * @param text Chat text (truncated by protocol helper).
         * @return True if sent.
         */
        [[nodiscard]] bool send_chat(std::string_view text);

        /** @return Current recipient-relative trade state. */
        const protocol::TradeState& trade() const {
            return m_trade;
        }

        /** @return Monotonic sequence of received trade snapshots. */
        std::uint64_t trade_serial() const {
            return m_trade_serial;
        }

        /** @param request Trade-only command. @return Whether it was sent. */
        /** @return Latest anonymous trading-post page. */
        const protocol::TradingPostState& trading_post() const {
            return m_trading_post;
        }

        /** @return Received trading-post snapshot revision. */
        std::uint64_t trading_post_serial() const {
            return m_trading_post_serial;
        }

        /** @return Whether this character has items or Geons awaiting market collection. */
        bool trading_post_collection_pending() const {
            return m_trading_post_collection_pending;
        }

        /** Requests server-owned consolidation before sorting presentation.
         * @param quest True for Quest Items, false for General.
         * @return Whether the request was sent; only one may be outstanding.
         */
        [[nodiscard]] bool send_inventory_sort(bool quest);

        /** Takes a successful consolidation acknowledgement after applying item updates.
         * @param quest Receives the section to sort.
         * @return Whether a committed acknowledgement was available.
         */
        [[nodiscard]] bool take_inventory_sort(bool& quest);

        /** @param request Trading-post command. @return Whether it was sent. */
        [[nodiscard]] bool send_trading_post(const protocol::TradingPostRequest& request);

        [[nodiscard]] bool send_trade(const protocol::TradeRequest& request);

        /** Sends a server-validated party command. @param request Command. @return Whether sent. */
        [[nodiscard]] bool send_party(const protocol::PartyRequest& request);

        /** @param request Guild operation; a unique retry identity is assigned when absent.
         * @return Whether the connected transport accepted the packet. */
        [[nodiscard]] bool send_guild(protocol::GuildRequest request);

        /** @param out Next guild view in wire order. @return Whether a view was pending. */
        [[nodiscard]] bool take_guild_view(platform::Json& out);

        /** @param request Selected-player operation. @return Assigned request ID, or zero on
         * failure. */
        [[nodiscard]] std::uint64_t send_player_options(protocol::PlayerOptionsRequest request);

        /** @param out Next context response. @return Whether a response was pending. */
        [[nodiscard]] bool take_player_options(protocol::PlayerOptionsState& out);

        /** @return Latest private guild view. */
        const platform::Json& guild_view() const {
            return m_guild_view;
        }

        /** @return Changes to guild views, including reconnect resets. */
        std::uint64_t guild_serial() const {
            return m_guild_serial;
        }

        /** @param player Runtime player ID. @return Visible guild name, or empty. */
        std::string guild_name(std::uint32_t player) const;

        /** @param player Runtime player ID. @return Current title when the guild is visible. */
        std::string guild_title(std::uint32_t player) const;

        /** Reads the player's current Castle War faction.
         * @param player Runtime player ID.
         * @return Zero for no faction, one for attack, or two for defense.
         */
        [[nodiscard]] int castle_side(std::uint32_t player) const;

        /** Checks whether a player is on the opposite Castle War faction.
         * @param player Runtime player ID.
         * @return Whether both players are on opposing factions.
         */
        [[nodiscard]] bool castle_enemy(std::uint32_t player) const;

        /** Checks the last known Castle War death state.
         * @param player Runtime player ID.
         * @return Whether the player is a defeated Castle War participant.
         */
        [[nodiscard]] bool castle_defeated(std::uint32_t player) const;

        /** Displays PvP state for a visible player from the local player's perspective.
         * @param player Runtime player ID.
         * @return Current combat and nameplate state.
         */
        [[nodiscard]] protocol::PvpStatus pvp_status(std::uint32_t player) const;

        /** Consumes a duel defeat even if recovery arrived in the same network poll.
         * @param player Runtime player ID.
         * @return Whether a defeat was pending.
         */
        [[nodiscard]] bool take_duel_defeat(std::uint32_t player);

        /** Reports whether this player is a currently attackable opponent.
         * @param player Runtime player ID.
         * @return Whether the server permits attacking the player.
         */
        [[nodiscard]] bool pvp_can_attack(std::uint32_t player) const;

        /** Updates the subscription for the selected PvP opponent, if supported by the server.
         * @param player Selected remote player ID, or zero to clear.
         */
        void select_pvp_health_target(std::uint32_t player);

        /** @return Latest health snapshot for the active PvP target, if available. */
        [[nodiscard]] const std::optional<protocol::PvpTargetHealth>& pvp_target_health() const {
            return m_pvp_target_health;
        }

        /** @return Latest authoritative party snapshot. */
        const protocol::PartyState& party() const {
            return m_party;
        }

        /** @param out Next item awarded to another member. @return Whether one was pending. */
        [[nodiscard]] bool take_party_item(protocol::PartyItemNotice& out);

        /**
         * Consumes a pending S2C_CHATTING line.
         * @param out Receives speaker and text.
         * @return True if pending.
         */
        [[nodiscard]] bool take_chat(PendingChat& out);

        /**
         * Consumes a pending S2C_MAPSTATE update.
         * @param out Receives the map-wide scenario state.
         * @return True if pending.
         */
        [[nodiscard]] bool take_map_state(PendingMapState& out);

        /** @return Server clock for the current account connection. */
        const ServerClock& server_clock() const {
            return m_server_clock;
        }

        /** Gets the latest authoritative community event snapshot.
         * @return Snapshot, or no value before synchronization or after disconnect.
         */
        const std::optional<protocol::ExpEventStateFields>& exp_event_state() const {
            return m_exp_event_state;
        }

        /** @return Latest Honor snapshot for the connected character, if received. */
        [[nodiscard]] const std::optional<protocol::HonorStatus>& honor_status() const {
            return m_honor_status;
        }

        /**
         * Consumes a pending battle/system info line.
         * @param out Receives text and kind.
         * @return True if pending.
         */
        [[nodiscard]] bool take_info_message(PendingInfoMessage& out);

        /**
         * Sends C2S_PICKUPITEM for a ground entity.
         * @param item_id World entity id.
         * @param tile_x Server tile X (pixel >> 5).
         * @param tile_y Server tile Y (pixel >> 5).
         * @return True if sent.
         */
        [[nodiscard]] bool send_pickup_item(std::uint32_t item_id, int tile_x, int tile_y);

        /**
         * Sends C2S_DROPITEM to place an inventory stack on the ground.
         * @param iid Inventory instance id.
         * @param count Stack count to drop.
         * @return True if sent.
         */
        [[nodiscard]] bool send_drop_item(std::uint32_t iid, std::uint32_t count);

        /**
         * Sends C2S_TRASHITEM to permanently delete an inventory instance.
         * @param iid Inventory instance id.
         * @return True if sent.
         */
        [[nodiscard]] bool send_trash_item(std::uint32_t iid);

        /**
         * Sends C2S_USEITEM to consume a refresh item from the bag.
         * @param iid Inventory instance id.
         * @param auto_hp_percent Auto-HP threshold, or 0 for manual use.
         * @return True if sent.
         */
        [[nodiscard]] bool send_use_item(std::uint32_t iid, std::uint8_t auto_hp_percent = 0);

        /**
         * Sends C2S_ASKNPC.
         * @param npc_id Runtime NPC entity id.
         * @return True if sent.
         */
        [[nodiscard]] bool send_ask_npc(std::uint32_t npc_id);

        /** @return NPC id from the most recently sent C2S_ASKNPC packet. */
        [[nodiscard]] std::uint32_t last_asked_npc_id() const {
            return m_last_asked_npc_id;
        }

        /**
         * Sends C2S_QUEST.
         * @param quest_index Quest index.
         * @param quest_flag Step flag.
         * @return True if sent.
         */
        [[nodiscard]] bool send_quest(std::uint16_t quest_index, std::uint8_t quest_flag);

        /**
         * Consumes a pending S2C_SENDHTML id.
         * @param out Html id.
         * @param npc_id Optional output for the source NPC, with a legacy last-asked fallback.
         * @return True if pending.
         */
        [[nodiscard]] bool take_send_html(std::int32_t& out, std::uint32_t* npc_id = nullptr);

        /** Consumes the latest Town Board reply.
         * @return The board snapshot, if one was received.
         */
        [[nodiscard]] std::optional<protocol::AssassinList> take_assassin_list();

        /** Sends C2S_ITEMLIST for an NPC. @param npc_id Runtime NPC id. @return True if sent. */
        [[nodiscard]] bool send_shop_item_list(std::uint32_t npc_id);

        /** Sends one shop purchase. @param npc_id Runtime NPC id. @param index item definition
         * index.
         * @param quantity Count. @return True if sent. */
        [[nodiscard]] bool
        send_buy_item(std::uint32_t npc_id, std::uint16_t index, std::uint16_t quantity);

        /** Sends one shop sale. @param npc_id Runtime NPC id. @param iid Bag iid. @param quantity
         * Count. @return True if sent. */
        [[nodiscard]] bool
        send_sell_item(std::uint32_t npc_id, std::uint32_t iid, std::uint16_t quantity);

        /** Consumes one pending shop inventory. @param out Decoded list. @return True if pending.
         */
        [[nodiscard]] bool take_shop_item_list(protocol::ShopItemList& out);

        /** Sends C2S_STORAGEINFO for a storage keeper.
         * @param npc_id Runtime storage-keeper id.
         * @return True if sent.
         */
        [[nodiscard]] bool send_storage_info(std::uint32_t npc_id);

        /** Sends one full-stack account-storage deposit.
         * @param npc_id Runtime storage-keeper id.
         * @param iid Bag instance id.
         * @param count Stack count.
         * @return True if sent.
         */
        [[nodiscard]] bool
        send_storage_deposit(std::uint32_t npc_id, std::uint32_t iid, std::uint32_t count);

        /** Sends one full-stack account-storage withdrawal.
         * @param npc_id Runtime storage-keeper id.
         * @param iid Storage instance id.
         * @param count Stack count.
         * @return True if sent.
         */
        [[nodiscard]] bool
        send_storage_withdraw(std::uint32_t npc_id, std::uint32_t iid, std::uint32_t count);

        /** Consumes one pending account-storage snapshot.
         * @param out Decoded snapshot.
         * @return True if pending.
         */
        [[nodiscard]] bool take_storage_info(protocol::StorageInfo& out);

        /** Sends a Geon transfer at an open keeper.
         * @param transfer Keeper, direction and amount.
         * @return True if sent.
         */
        [[nodiscard]] bool send_storage_money(const protocol::StorageMoneyTransfer& transfer);

        /** Consumes one pending account balance.
         * @param out Decoded balance.
         * @return True if pending.
         */
        [[nodiscard]] bool take_storage_balance(protocol::StorageBalance& out);

        /** Sends a talent-tree request.
         * @param command Request operation.
         * @param expected Last authoritative mask.
         * @param desired New draft mask for Apply.
         * @return Whether the request was sent. */
        [[nodiscard]] bool send_talent_request(protocol::TalentCommand command,
                                               std::uint64_t expected = 0,
                                               std::uint64_t desired = 0);

        /** Consumes a new authoritative talent state.
         * @param out Received state.
         * @return Whether a state was available. */
        [[nodiscard]] bool take_talent_state(protocol::TalentState& out);

        /** Requests a fresh system-mail inbox snapshot. @return True if sent. */
        [[nodiscard]] bool send_mail_list();

        /** Requests an item attachment from one system mail.
         * @param id Durable mail id.
         * @return True if sent.
         */
        [[nodiscard]] bool send_mail_claim(std::uint32_t id);

        /** Sends a read-state or deletion request.
         * @param command Read or Delete operation.
         * @param id Durable mail identifier.
         * @return True if sent.
         */
        [[nodiscard]] bool send_mail_action(protocol::MailCommand command, std::uint32_t id);

        /** @return Latest server-authoritative system-mail inbox. */
        [[nodiscard]] const std::vector<protocol::MailEntry>& mail() const {
            return m_mail;
        }

        /** Consumes the inbox-refresh indication. @return True when mail changed. */
        [[nodiscard]] bool take_mail_dirty() {
            const bool dirty = m_mail_dirty;
            m_mail_dirty = false;
            return dirty;
        }

        /** Consumes one server response to a mail attachment claim.
         * @param out Claim response.
         * @return True if a response is pending.
         */
        [[nodiscard]] bool take_mail_claim_result(protocol::MailClaimResult& out);

        /** @return Live NPCs keyed by entity id. */
        [[nodiscard]] const std::unordered_map<std::uint32_t, NetNpc>& npcs() const {
            return m_npcs;
        }

        /** @return Current quest flag entries. */
        [[nodiscard]] const std::vector<protocol::QuestFlagEntry>& quest_flags() const {
            return m_quest_flags;
        }

        /** @return True when quest flags changed since last clear. */
        [[nodiscard]] bool take_quest_flags_dirty() {
            const bool dirty = m_quest_flags_dirty;
            m_quest_flags_dirty = false;
            return dirty;
        }

        /** @return True when the server confirmed a quest state change since last clear. */
        [[nodiscard]] bool take_quest_state_changed() {
            const bool changed = m_quest_state_changed;
            m_quest_state_changed = false;
            return changed;
        }

        /** @return True once when the server confirms a quest becoming active. */
        [[nodiscard]] bool take_quest_accepted() {
            const bool accepted = m_quest_accepted;
            m_quest_accepted = false;
            return accepted;
        }

        /**
         * Consumes a pending attack presentation for a monster.
         * @param id Monster entity id.
         * @return True if an attack was pending.
         */
        [[nodiscard]] bool take_monster_attack(std::uint32_t id);

        /**
         * Consumes a latched ActionDamage payload for a monster.
         * @param id Monster entity id.
         * @param out Receives damage, flags, and target id.
         * @return True if a payload was pending.
         */
        [[nodiscard]] bool take_monster_action_damage(std::uint32_t id,
                                                      PendingMonsterActionDamage& out);

        /**
         * Consumes a pending AT_DIE presentation for a monster (chest open / death).
         * @param id Monster entity id.
         * @return True if a die action was pending.
         */
        [[nodiscard]] bool take_monster_die(std::uint32_t id);

        /**
         * Consumes a pending AT_KNEE presentation for a Maguni corpse.
         * @param id Monster entity id.
         * @return True if a kneel action was pending.
         */
        [[nodiscard]] bool take_monster_knee(std::uint32_t id);

        /**
         * Consumes a pending AT_BEHEADED presentation.
         * @param id Monster entity id.
         * @return True if a behead action was pending.
         */
        [[nodiscard]] bool take_monster_behead(std::uint32_t id);

        /**
         * Drops a locally-finished corpse after the die/behead clip and sink.
         * @param id Monster entity id.
         */
        void forget_monster(std::uint32_t id);

        /** @return True if local player should play a skill/attack presentation this
         * frame. */
        [[nodiscard]] bool take_local_skill_cast();

        /**
         * Consumes a pending S2C_ACTION AT_SKILL telegraph.
         * @param out Receives caster, skill index, and target.
         * @return True if pending.
         */
        [[nodiscard]] bool take_skill_action(PendingSkillAction& out);

        /**
         * Consumes a pending transform PRE/APPLY action.
         * @param out Receives entity, pre flag, and arg.
         * @return True if pending.
         */
        [[nodiscard]] bool take_transform_action(PendingTransformAction& out);

        /**
         * Consumes pending hit feedback for a monster.
         * @param out Receives target id and S2C_ATTACK flags.
         * @return True if pending.
         */
        [[nodiscard]] bool take_hit_fx(PendingHitFx& out);

        /** Consumes a confirmed normal attack for actor presentation.
         * @param out Receives the attacker, target and target type.
         * @return True when an attack was available.
         */
        [[nodiscard]] bool take_player_attack(PendingPlayerAttack& out);

        /**
         * Consumes a pending local evade (monster attack missed CheckHit).
         * @return True if the local player should play the evade clip and chat line.
         */
        [[nodiscard]] bool take_local_evade();

        /**
         * Consumes a pending floating combat line.
         * @param out Receives kind, text, and anchor id.
         * @return True if pending.
         */
        [[nodiscard]] bool take_combat_float(PendingCombatFloat& out);

        /**
         * Consumes a pending Fire Magic / Fire Blow dest-info HP drain.
         * @param out Receives target id and subtracted HP.
         * @return True if pending.
         */
        [[nodiscard]] bool take_delay_damage(PendingDelayDamage& out);

        /**
         * Sends C2S_GETITEMMONSTER to begin opening a treasure box.
         * @param monster_id Chest entity id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_get_item_monster(std::uint32_t monster_id);

        /**
         * Sends C2S_CANCELITEMMONSTER to abort opening a treasure box.
         * @param monster_id Chest entity id (0 cancels any active open on server).
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_cancel_item_monster(std::uint32_t monster_id);

        /**
         * Consumes MSG_ITEMMONSTEROPEN (start kneel + open timer).
         * @param open_sec Out open duration in seconds.
         * @return True if pending.
         */
        [[nodiscard]] bool take_chest_open_start(std::uint8_t& open_sec);

        /**
         * Consumes MSG_ITEMMONSTEROPENED or MSG_ITEMMONSTERCANCELED (stop kneel).
         * @return True if pending.
         */
        [[nodiscard]] bool take_chest_open_stop();

        /**
         * Last item-monster MESSAGE / MESSAGEV ordinal (Used/Open/Opened/Canceled).
         * @return Message ordinal, or 0 when none received yet.
         */
        [[nodiscard]] std::uint8_t last_chest_feedback_msg() const {
            return m_last_chest_feedback_msg;
        }

        /**
         * Consumes monster id that just hit the local player.
         * @param attacker_id Out attacker entity id.
         * @return True if a hit was pending.
         */
        [[nodiscard]] bool take_hit_from(std::uint32_t& attacker_id);

        /** Consumes latest HP update. @param hp Out HP. @return True if pending. */
        [[nodiscard]] bool take_cur_hp(std::int32_t& hp);

        /** Consumes latest MP update. @param mp Out MP. @return True if pending. */
        [[nodiscard]] bool take_cur_mp(std::int32_t& mp);

        /**
         * Consumes a pending max-HP update (from P_HP vital).
         * @param max_hp Out max HP.
         * @return True if pending.
         */
        [[nodiscard]] bool take_max_hp(std::int32_t& max_hp);

        /**
         * Consumes a pending max-MP update (from P_MP vital).
         * @param max_mp Out max MP.
         * @return True if pending.
         */
        [[nodiscard]] bool take_max_mp(std::int32_t& max_mp);

        /**
         * Consumes a pending EXP bar snapshot.
         * @param current Out current EXP.
         * @param total Out NeedExp for current level.
         * @return True if pending.
         */
        [[nodiscard]] bool take_exp(std::int64_t& current, std::int64_t& total);

        /**
         * Consumes a pending level update.
         * @param level Out level.
         * @return True if pending.
         */
        [[nodiscard]] bool take_level(std::int32_t& level);

        /**
         * Consumes pending PU/SU updates.
         * @param pu Out status points.
         * @param su Out skill points.
         * @return True if either changed.
         */
        [[nodiscard]] bool take_points(std::int32_t& pu, std::int32_t& su);

        /**
         * Job specialty bitmask from MSG_SPECIALTYUP (bit 0 = wandering).
         * @return Current specialty mask; 1 until the server sends a snapshot.
         */
        [[nodiscard]] std::uint32_t specialty() const {
            return m_specialty;
        }

        /**
         * Consumes a pending level-up effect entity id.
         * @param entity_id Out entity to play FX on.
         * @return True if pending.
         */
        [[nodiscard]] bool take_level_up_effect(std::uint32_t& entity_id);

        /**
         * Consumes a pending oneshot S2C_EFFECT (E_HP, E_MP, or E_CHARM_1–8).
         * @param entity_id Out entity to attach FX to.
         * @param effect Out EffectType ordinal.
         * @return True if pending.
         */
        [[nodiscard]] bool take_charm_effect(std::uint32_t& entity_id, std::uint8_t& effect);

        /**
         * Consumes a pending E_DAMAGED player-hit effect.
         * @param entity_id Out player entity id.
         * @param damage Out applied damage.
         * @return True if pending.
         */
        [[nodiscard]] bool take_damaged_effect(std::uint32_t& entity_id, std::uint16_t& damage);

        /**
         * Active skill, item and talent buff slots for the left HUD.
         * @return Current buff entries (may include expired timed entries).
         */
        [[nodiscard]] const std::vector<BuffSlotEntry>& buff_slots() const {
            return m_buff_slots;
        }

        /**
         * Timed inventory register expiry timestamps from MSG_ADDBSTATE.
         * Entries 0 through 3 map to inventory registers 3 through 6.
         * @return Steady-clock expiry timestamps in milliseconds, or zero when locked.
         */
        [[nodiscard]] const std::array<std::uint64_t, 4>& inventory_register_expire_ms() const {
            return m_inventory_register_expire_ms;
        }

        /**
         * Uses the skill catalog for AddBState icon names (JSON `image`).
         * @param catalog Loaded catalog; may be null.
         */
        void set_skill_catalog(const Catalog* catalog) {
            m_skill_catalog = catalog;
        }

        /**
         * Uses the item catalog for item-sourced AddBState icons and titles.
         * @param catalog Loaded catalog; may be null.
         */
        void set_item_catalog(const ItemDb* catalog) {
            m_item_catalog = catalog;
        }

        /**
         * Consumes a pending S2C_SKILLINFO learned-skill snapshot.
         * @param learned Out index → level map.
         * @return True if pending.
         */
        [[nodiscard]] bool take_skill_info(std::unordered_map<int, int>& learned);

        /**
         * Consumes one pending S2C_SKILLUP (index, level).
         * @param index Out skill index.
         * @param level Out learned level.
         * @return True if pending.
         */
        [[nodiscard]] bool take_skill_up(int& index, int& level);

        /**
         * Consumes a pending S2C_SHORTCUT snapshot.
         * @param present Set true when a saved bar follows.
         * @param bar Receives the bar when present.
         * @return True if a shortcut packet is pending.
         */
        [[nodiscard]] bool take_shortcut(bool& present, protocol::ShortcutBar& bar);

        /**
         * Persists the skill bar (C2S_SHORTCUT).
         * @param bar Active skill and hotbar slots.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_shortcut(const protocol::ShortcutBar& bar);

        /** Requests village revival (C2S_REVIVAL). @return True if sent. */
        [[nodiscard]] bool send_revival();

        /**
         * Saves a Black Crow statue as the village revival point (C2S_SAVEREVIVALPT).
         * @param point Server revival-point id.
         * @param npc_id Statue entity id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_save_revival_point(std::uint8_t point, std::uint32_t npc_id);

        /**
         * Requests spending PU on one or more primary-stat steps (C2S_UPDATEPROPERTY).
         * @param property Property id (Str..Dex).
         * @param count Requested number of steps; the server decides how many are affordable.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_update_property(std::uint8_t property, std::uint8_t count = 1);

        /** Snapshot of combat / identity fields for the Character Info window. */
        struct CharInfoStats {
            /** Character name. */
            std::string name;

            /** Class index. */
            int clazz = 0;

            /** Job / class label (Knight, Magician, …). */
            std::string job;

            /** Guild name. */
            std::string guild;

            /** Guild rank / Pos. */
            std::string guild_pos;

            /** Government Position (G.P.) title; empty → resolve from specialty_index.
             */
            std::string gov_pos;

            /** Title index into the JSON files in config/jobs (0 = default / wandering). */
            int specialty_index = 0;

            /** Awaken label. */
            std::string awaken = "Awaken -";

            /** Level. */
            std::int32_t level = 1;

            /** Contribute / compatibility behavior. */
            int ctb = 0;

            /** Current HP. */
            std::int32_t hp = 0;

            /** Max HP. */
            std::int32_t max_hp = 0;

            /** Current MP. */
            std::int32_t mp = 0;

            /** Max MP. */
            std::int32_t max_mp = 0;

            /** STR. */
            std::int32_t str = 0;

            /** Health / HTH. */
            std::int32_t hth = 0;

            /** INT. */
            std::int32_t intl = 0;

            /** WIS. */
            std::int32_t wis = 0;

            /** DEX / AGI. */
            std::int32_t dex = 0;

            /** Remaining PU. */
            std::int32_t pu_point = 0;

            /** Min physical ATK. */
            std::int32_t min_attack = 0;

            /** Max physical ATK. */
            std::int32_t max_attack = 0;

            /** Min magic ATK. */
            std::int32_t min_magic = 0;

            /** Max magic ATK. */
            std::int32_t max_magic = 0;

            /** OTP / Hit. */
            std::int32_t hit = 0;

            /** Eva / Dodge. */
            std::int32_t dodge = 0;

            /** Defense. */
            std::int32_t defense = 0;

            /** Absorb percent. */
            std::int32_t absorb = 0;

            /** Fire resist. */
            std::int32_t res_fire = 0;

            /** Ice resist. */
            std::int32_t res_ice = 0;

            /** Lightning resist. */
            std::int32_t res_lightning = 0;

            /** Curse resist. */
            std::int32_t res_curse = 0;

            /** Non-elemental / palsy resist. */
            std::int32_t res_palsy = 0;
        };

        /**
         * Fills a Character Info snapshot from the latest UpdateProperty state.
         * @return Current stats copy.
         */
        [[nodiscard]] CharInfoStats char_info_stats() const;

        /**
         * Current transform rage (P_RAGE).
         * @return Rage points.
         */
        [[nodiscard]] std::int32_t rage() const {
            return m_rage;
        }

        /**
         * Current Knight Mortal charges (P_MORTAL).
         * @return Charges available to Brutal Attack.
         */
        [[nodiscard]] std::int32_t mortal() const {
            return m_mortal;
        }

        /**
         * Equipped / transformed TFlag (race<<4 | stage).
         * @return TFlag value.
         */
        [[nodiscard]] int transform_tflag() const {
            return m_transform_tflag;
        }

        /**
         * Local player's last P_MSPEED percent (transform, etc.).
         * @return Signed percent applied to walk/run.
         */
        [[nodiscard]] std::int16_t local_mspeed() const {
            return m_local_mspeed;
        }

        /** @return Worn egg item definition index from S2C_UPDATEITEMINFO, or 0. */
        [[nodiscard]] int transform_egg_index() const {
            return m_transform_egg_index;
        }

        /** @return Worn egg level from S2C_UPDATEITEMINFO. */
        [[nodiscard]] int transform_egg_level() const {
            return m_transform_egg_level;
        }

        /** @return Worn egg EXP toward the next level. */
        [[nodiscard]] int transform_egg_exp() const {
            return m_transform_egg_exp;
        }

        /**
         * Sets worn-egg level and EXP for offline / debug chat commands.
         * @param level Animal level (at least 1).
         * @param exp EXP toward the next level (at least 0).
         */
        void set_transform_egg_progress(int level, int exp) {
            m_transform_egg_level = level < 1 ? 1 : level;
            m_transform_egg_exp = exp < 0 ? 0 : exp;
        }

        /**
         * Seeds identity fields used by Character Info (name / class / job).
         * @param name Character name.
         * @param clazz Class index.
         * @param job Job label.
         */
        void set_char_identity(std::string name, int clazz, std::string job = {});

        /**
         * Seeds primary bases before the first S2C dump (offline / local).
         * @param str STR.
         * @param hth Health.
         * @param intl INT.
         * @param wis WIS.
         * @param dex DEX.
         * @param pu Remaining PU.
         */
        void seed_char_primaries(std::int32_t str,
                                 std::int32_t hth,
                                 std::int32_t intl,
                                 std::int32_t wis,
                                 std::int32_t dex,
                                 std::int32_t pu);

        /** @return True while the session socket is alive. */
        [[nodiscard]] bool connected() const {
            return m_session && m_session->alive();
        }

        /** @return True after a successful handshake. */
        [[nodiscard]] bool handshook() const {
            return m_handshook;
        }

        /** @return Local player entity id from CreatePlayer. */
        [[nodiscard]] std::uint32_t local_player_id() const {
            return m_local_player_id;
        }

        /**
         * Last S2C_GSTATE (or CreatePlayer gstate) for a player entity.
         * Local BOF FX stays bag-authoritative; remotes use these bits.
         * @param id Player entity id.
         * @return Packed GState, or 0 when unknown.
         */
        [[nodiscard]] std::uint32_t player_gstate(std::uint32_t id) const {
            const auto it = m_player_gstate.find(id);
            return it == m_player_gstate.end() ? 0u : it->second;
        }

        /** Consumes the next authoritative fishing-cycle start signal.
         * @return True when the local fishing progress bar must restart.
         */
        [[nodiscard]] bool take_fishing_progress_start();

        /** @return Last known server pixel X. */
        [[nodiscard]] int server_px() const {
            return m_server_px;
        }

        /** @return Last known server pixel Y. */
        [[nodiscard]] int server_py() const {
            return m_server_py;
        }

        /**
         * Consumes absolute pose snap from S2C_MOVEBEFORE.
         * @param sx Out server X.
         * @param sy Out server Y.
         * @param sz Out server height.
         * @return True if a snap was pending.
         */
        [[nodiscard]] bool take_move_before(int& sx, int& sy, int& sz);

        /**
         * Sends C2S attack against a monster.
         * @param monster_id Target entity id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_attack_monster(std::uint32_t monster_id);

        /** Sends a basic attack against an opposing Castle War player.
         * @param player Target runtime player ID.
         * @return Whether the frame was sent.
         */
        [[nodiscard]] bool send_attack_player(std::uint32_t player);

        /**
         * Sends a targeted skill against a monster.
         * @param skill Skill opcode/index.
         * @param monster_id Target entity id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_target_monster(std::uint8_t skill, std::uint32_t monster_id);

        /**
         * Sends a targeted monster skill carrying a required inventory instance.
         * @param skill Skill opcode/index.
         * @param monster_id Target entity id.
         * @param item_iid Required inventory instance id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_target_monster_item(std::uint8_t skill,
                                                          std::uint32_t monster_id,
                                                          std::uint32_t item_iid);

        /**
         * Sends a targeted skill carrying a required inventory instance.
         * @param skill Skill opcode/index.
         * @param kind Character kind (monster or player).
         * @param target_id Target entity id.
         * @param item_iid Required inventory instance id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_target_item(std::uint8_t skill,
                                                  std::uint8_t kind,
                                                  std::uint32_t target_id,
                                                  std::uint32_t item_iid);

        /**
         * Sends a targeted skill against a player.
         * @param skill Skill opcode/index.
         * @param player_id Target player entity id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_target_player(std::uint8_t skill, std::uint32_t player_id);

        /**
         * Sends a ground-coordinate skill.
         * @param skill Skill opcode/index.
         * @param x Server world X.
         * @param y Server world Y.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_ground(std::uint8_t skill, std::int32_t x, std::int32_t y);

        /**
         * Sends C2S_PRESKILL (cast telegraph before C2S_SKILL).
         * @param skill Skill opcode/index.
         * @param target Target entity id (monster or self).
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_preskill(std::uint8_t skill, std::uint32_t target);

        /**
         * Sends a self-targeted skill.
         * @param skill Skill opcode/index.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_self(std::uint8_t skill);

        /**
         * Sends C2S_TRANSFORM "bb".
         * @param flag TRANS_M_PRE / EXECUTE / FREE.
         * @param stage Stage 1–3.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_transform(std::uint8_t flag, std::uint8_t stage);

        /**
         * Sends C2S_TRANSFORMSKILL "b".
         * @param skill Animal skill index.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_transform_skill(std::uint8_t skill);

        /**
         * Enables or disables skill boost.
         * @param enable Desired boost state.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_boost(bool enable);

        /**
         * Sends C2S_LEARNSKILL for a skill index.
         * @param skill Skill index.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_learn_skill(std::uint8_t skill);

        /**
         * Sends C2S_SKILLUP for a skill index.
         * @param skill Skill index.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_skill_up(std::uint8_t skill);

        /**
         * Requests rest / stand.
         * @param want_rest True to rest.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_rest(bool want_rest);

        /** Sends a character emote request.
         * @param motion One-based action-10 motion.
         * @return True when a valid request was sent.
         */
        [[nodiscard]] bool send_emote(std::uint8_t motion);

        /** Consumes the next authoritative emote notification.
         * @param out Receives the player and motion.
         * @return True when an event was available.
         */
        [[nodiscard]] bool take_emote(protocol::PlayerEmote& out);

        /** Cancels the current quest job, including fishing.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_quest_job_end();

        /** Starts a Speed-up Medicine cooking order.
         * @param order Quantities for the three potion recipes.
         * @return True when the request was sent.
         */
        [[nodiscard]] bool send_cooking(const protocol::CookingOrder& order);

        /**
         * Consumes rest-state update from the server.
         * @param resting Out resting flag.
         * @return True if pending.
         */
        [[nodiscard]] bool take_rest_state(bool& resting);

        /**
         * Rest flag for any entity from S2C_ACTION AT_REST.
         * @param entity_id Target entity id.
         * @return True while sitting.
         */
        [[nodiscard]] bool entity_resting(std::uint32_t entity_id) const;

        /**
         * True while compatibility behavior or compatibility behavior is set for the entity
         * (S2C_MSTATE).
         * @param entity_id Target entity id.
         * @return True while sit-food consume should play.
         */
        [[nodiscard]] bool entity_consuming(std::uint32_t entity_id) const;

        /**
         * Equips an inventory item (SP: item definition index as iid).
         * @param iid Item instance / index id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_puton_item(std::uint32_t iid);

        /**
         * Unequips an inventory item.
         * @param iid Item instance / index id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_putoff_item(std::uint32_t iid);

        /** Requests mounting or dismounting the registered common riding animal. */
        [[nodiscard]] bool send_riding_toggle();

        /** Sends an idempotent Battle Riding mutation request. */
        [[nodiscard]] bool send_battle_riding_request(const protocol::BattleRidingRequest& request);

        /**
         * Sends C2S_ENCHANTITEM (talisman onto gear).
         * @param from_iid Talisman instance id (or item definition index fallback).
         * @param to_iid Destination gear instance id (or item definition index fallback).
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_enchant_item(std::uint32_t from_iid, std::uint32_t to_iid);

        /**
         * Sends C2S_UPGRADEITEM (Revise or Explosive Blow).
         * @param type Upgrade action.
         * @param iid Weapon instance id.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_upgrade_item(protocol::UpgradeItemType type, std::uint32_t iid);

        /**
         * Sends C2S_MIXING (weapon + materials).
         * @param mode Mixing kind (0 upgrade, 1 making).
         * @param weapon_iid Weapon instance iid.
         * @param extra_weapon_iid Extra weapon instance iid.
         * @param armor_iid Armor/defense instance iid.
         * @param stone1_iid Elemental stone #1 instance iid.
         * @param stone2_iid Second stone iid (0 unless making or G3 mix).
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_mixing(std::uint8_t mode,
                                       std::uint32_t weapon_iid,
                                       std::uint32_t extra_weapon_iid,
                                       std::uint32_t armor_iid,
                                       std::uint32_t stone1_iid,
                                       std::uint32_t stone2_iid);

        /**
         * Consumes a pending enchant-failed flag from S2C_MESSAGE MSG_ENCHANTFAILED.
         * @return True if a fail notice is pending.
         */
        [[nodiscard]] bool take_enchant_failed();

        /**
         * Requests city-scroll teleport to g_arTeleportLoc[index].
         * @param loc_index Teleport table index.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_teleport_request(std::uint8_t loc_index);

        /**
         * Consumes an S2C_TELEPORT payload.
         * @param map Out map id.
         * @param sx Out server X.
         * @param sy Out server Y.
         * @param sz Out server Z.
         * @return True if pending.
         */
        [[nodiscard]] bool take_teleport(int& map, int& sx, int& sy, int& sz);

        /**
         * Acknowledges a completed teleport at the given pose.
         * @param sx Server X.
         * @param sy Server Y.
         * @param sz Server Z.
         * @return True if the frame was sent.
         */
        [[nodiscard]] bool send_teleport_ack(int sx, int sy, int sz);

        /**
         * Offline / local snap: reset move sync without a packet.
         * @param sx Server X baseline.
         * @param sy Server Y baseline.
         * @param sz Server height baseline.
         */
        void seed_move_baseline(int sx, int sy, int sz);

        /**
         * Consumes a pending CreatePlayer payload.
         * @param out Out fields.
         * @return True if pending.
         */
        [[nodiscard]] bool take_create_player(protocol::CreatePlayerFields& out);

        /**
         * Consumes one pending other-PC CreatePlayer (no 0x80 hero bit).
         * @param out Out fields.
         * @return True if pending.
         */
        [[nodiscard]] bool take_create_other_player(protocol::CreatePlayerFields& out);

        /**
         * Consumes one pending S2C_REMOVEPLAYER id.
         * @param out_id Out entity id.
         * @return True if pending.
         */
        [[nodiscard]] bool take_remove_other_player(std::uint32_t& out_id);

        /**
         * Drains all pending MovePlayerOn/End until the server baseline matches the
         * local pose. Use before distance-sensitive C2S (treasure-box open) so pose
         * prediction lag cannot reject an in-range client attempt.
         * @param wx Client world X.
         * @param wy Client world height.
         * @param wz Client world Z.
         * @param origin_patch Map origin patch.
         */
        void flush_local_move(float wx, float wy, float wz, int origin_patch);

        /**
         * Predicts locally, then reports pixel deltas (C2S_MOVE_ON/END) at ~20 Hz.
         * @param wx Client world X.
         * @param wy Client world height.
         * @param wz Client world Z.
         * @param moving True while the player is moving.
         * @param dt Frame delta seconds.
         * @param origin_patch Map origin patch.
         * @param hold_position Suppress all movement packets during a stationary action.
         */
        void sync_local_move(float wx,
                             float wy,
                             float wz,
                             bool moving,
                             float dt,
                             int origin_patch,
                             bool hold_position = false);

        /**
         * Applies macro-e mspeed ticks → px/s (walk = [0], run/chase = [1]).
         * @param id Monster entity id.
         * @param walk_tick Walk speed tick value.
         * @param run_tick Run speed tick value.
         */
        void set_monster_mspeed(std::uint32_t id, float walk_tick, float run_tick);

        /** Persists the authoritative session through C2S_GAMEEXIT, then closes it. */
        void close();

      private:
        /** Sends a proportional XYZ delta within protocol and server limits.
         * @param sx Desired server X.
         * @param sy Desired server Y.
         * @param sz Desired server height.
         * @param end True to end movement when this packet reaches the destination.
         * @return True when the packet was sent.
         */
        [[nodiscard]] bool send_move_towards(int sx, int sy, int sz, bool end);

        /** Clears online views and advances their change serials once per invocation. */
        void clear_online_views();

        /** Reads available socket bytes into runtime state. */
        void pump_rx();

        /** Queues a green incoming-item battle message.
         * @param index Item definition index. @param count Received quantity. */
        void queue_item_received(int index, int count);

        /** Queues a white incoming-currency battle message.
         * @param geons Positive received amount; zero produces no message. */
        void queue_money_received(std::int64_t geons);

        /** Enforces the trade-only traffic gate on every outgoing frame.
         * @param packet One framed packet. @return Whether it was allowed and sent. */
        [[nodiscard]] bool send_packet(const std::vector<std::uint8_t>& packet);

        /** Handles online views and communication without owning frame or cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_online_packet(std::uint8_t type,
                                                const std::vector<std::uint8_t>& payload);

        /** Handles inventory, equipment and item-service replies without owning cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_inventory_packet(std::uint8_t type,
                                                   const std::vector<std::uint8_t>& payload);

        /** Handles conversation, quest and skill snapshots without owning cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_quest_packet(std::uint8_t type,
                                               const std::vector<std::uint8_t>& payload);

        /** Handles handshake, clock and scalar snapshots without owning frame or cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_snapshot_packet(std::uint8_t type,
                                                  const std::vector<std::uint8_t>& payload);

        /** Handles world snapshots and motion without owning frame or cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @param origin_patch Map origin patch for coordinate conversion.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_world_packet(std::uint8_t type,
                                               const std::vector<std::uint8_t>& payload,
                                               int origin_patch);

        /** Handles status snapshots, buffs and feedback without owning frame or cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_status_packet(std::uint8_t type,
                                                const std::vector<std::uint8_t>& payload);

        /** Handles combat presentation packets without owning frame or cleanup policy.
         * @param type Server opcode.
         * @param payload Borrowed complete payload, valid for this call only.
         * @param origin_patch Map origin patch for coordinate conversion.
         * @return True for a recognized opcode, including malformed payloads; false otherwise.
         */
        [[nodiscard]] bool handle_combat_packet(std::uint8_t type,
                                                const std::vector<std::uint8_t>& payload,
                                                int origin_patch);

        /** Deframes and dispatches complete packets from runtime state.
         * @param origin_patch Map origin patch for coordinate conversion. */
        void process_frames(int origin_patch);

        /** Read-only sessions follow self movement echoes instead of predicting input. */
        bool m_read_only = false;

        /** Smooth presentation of confirmed self movement in a read-only session. */
        RemotePlayerMotion m_observed_motion;

        /** Rotates a known monster toward the target of an attack or skill packet.
         * @param attacker Monster entity id.
         * @param target Target entity id.
         * @param origin_patch Map origin patch for remote coordinate conversion.
         */
        void face_monster_to_target(std::uint32_t attacker, std::uint32_t target, int origin_patch);

        /**
         * Queues a rising combat line for the HUD.
         * @param kind Color and copy kind.
         * @param text Display text.
         * @param anchor_id 0 = local player, else monster entity id.
         * @param wait_for_bolt True to hold until the fire/ice bolt crashes.
         */
        void enqueue_combat_float(CombatFloatKind kind,
                                  std::string text,
                                  std::uint32_t anchor_id,
                                  bool wait_for_bolt = false);

        /** Live transport session, or nullptr when disconnected. */
        std::unique_ptr<server::ISession> m_session;

        /** Raw RX byte buffer awaiting deframe. */
        std::vector<std::uint8_t> m_rx;

        /** Live monsters keyed by entity id. */
        std::unordered_map<std::uint32_t, NetMonster> m_monsters;

        /** Live ground loot keyed by entity id. */
        std::unordered_map<std::uint32_t, NetGroundItem> m_ground_items;

        /** Latest event snapshot; absent until the server sends one. */
        std::optional<protocol::ExpEventStateFields> m_exp_event_state;

        /** Personal Honor snapshot for this session. */
        std::optional<protocol::HonorStatus> m_honor_status;
        /** Authoritative time extrapolated for the current connection only. */
        ServerClock m_server_clock;

        /** Live NPCs keyed by entity id. */
        std::unordered_map<std::uint32_t, NetNpc> m_npcs;

        /** Quest journal state. */
        std::vector<protocol::QuestFlagEntry> m_quest_flags;

        /** Dirty bit for quest journal refresh. */
        bool m_quest_flags_dirty = false;

        /** Set by S2C_SAVEQUEST, excluding the initial S2C_QUESTINFO snapshot. */
        bool m_quest_state_changed = false;

        /** Latched when S2C_SAVEQUEST starts a previously inactive quest. */
        bool m_quest_accepted = false;

        /** Pending dialog ids paired with their source NPC entities. */
        std::deque<std::pair<std::int32_t, std::uint32_t>> m_pending_html;
        std::optional<protocol::AssassinList> m_pending_assassin_list;

        /** Pending server-authoritative NPC shop inventories. */
        std::deque<protocol::ShopItemList> m_pending_shop_lists;

        /** Pending server-authoritative account-storage snapshots. */
        std::deque<protocol::StorageInfo> m_pending_storage_info;
        std::deque<protocol::StorageBalance> m_pending_storage_balances;

        /** Latest server-authoritative system-mail inbox. */
        std::vector<protocol::MailEntry> m_mail;
        /** Latest trade state and snapshot sequence for modal draft reconciliation. */
        std::uint64_t m_trade_serial = 0;
        protocol::TradeState m_trade;
        int m_inventory_sort_pending = -1;
        int m_inventory_sort_ready = -1;
        protocol::TradingPostState m_trading_post;
        std::uint64_t m_trading_post_serial = 0;
        bool m_trading_post_collection_pending = false;
        protocol::PartyState m_party;
        platform::Json m_guild_view;
        std::deque<platform::Json> m_pending_guild_views;
        std::deque<protocol::PlayerOptionsState> m_pending_player_options;
        std::unordered_map<std::uint32_t, platform::Json> m_guild_displays;
        std::unordered_map<std::uint32_t, protocol::PvpStatus> m_pvp_status;
        /** Whether the server has advertised the PvP health subscription. */
        bool m_pvp_health_supported = false;
        protocol::PvpTargetHealthRequest m_pvp_health_request;
        std::optional<protocol::PvpTargetHealth> m_pvp_target_health;
        std::unordered_set<std::uint32_t> m_pending_duel_defeats;
        std::uint64_t m_guild_serial = 0, m_guild_request = 0;
        std::deque<protocol::PartyItemNotice> m_pending_party_items;
        protocol::TalentState m_talent_state;
        bool m_talent_dirty = false;

        /** True after a received inbox snapshot. */
        bool m_mail_dirty = false;

        /** Pending responses to attachment-claim requests. */
        std::deque<protocol::MailClaimResult> m_pending_mail_claims;

        /** NPC id associated with the latest C2S_ASKNPC request. */
        std::uint32_t m_last_asked_npc_id = 0;

        /** Pending S2C_INSERTITEM records for the local bag. */
        std::deque<PendingInsertItem> m_pending_inserts;

        /** Pending S2C_PUTONITEM and S2C_PUTOFFITEM records. */
        std::deque<PendingItemWear> m_pending_item_wear;

        /** Pending authoritative common riding snapshots. */
        std::deque<protocol::RidingState> m_pending_riding_states;

        /** Pending server-confirmed Riding Cache contents. */
        std::deque<protocol::RidingCacheResult> m_pending_riding_cache_results;

        /** Pending S2C_UPDATEITEMINFO records for bag eggs. */
        std::deque<protocol::UpdateItemInfoFields> m_pending_item_info;

        /** Pending S2C_TRASHITEM instance ids for the local bag. */
        std::deque<std::uint32_t> m_pending_trash;

        /** Pending S2C_CHATTING lines. */
        std::deque<PendingChat> m_pending_chats;

        /** Pending S2C_MAPSTATE scenario updates. */
        std::deque<PendingMapState> m_pending_map_states;

        /** Pending battle / system info lines. */
        std::deque<PendingInfoMessage> m_pending_infos;

        /** True when MSG_ENCHANTFAILED is waiting to be consumed. */
        bool m_enchant_failed = false;

        /** Pending hit feedback for monsters. */
        std::deque<PendingHitFx> m_pending_hit_fx;
        std::deque<PendingPlayerAttack> m_pending_player_attacks;

        /** Pending rising combat numbers. */
        std::deque<PendingCombatFloat> m_pending_combat_floats;

        /** Pending Fire Magic / Fire Blow dest-info HP drains. */
        std::deque<PendingDelayDamage> m_pending_delay_damage;

        /** Queued S2C_ACTION AT_SKILL telegraphs. */
        std::deque<PendingSkillAction> m_pending_skill_actions;

        /** Validated player-animation notifications awaiting presentation. */
        std::deque<protocol::PlayerEmote> m_pending_emotes;

        /** Queued S2C_ACTION AT_PRETRANSFORM / AT_TRANSFORM. */
        std::deque<PendingTransformAction> m_pending_transform_actions;

        /** Last known HP used to compute chat deltas (not consumed by take_cur_hp).
         */
        std::int32_t m_chat_prev_hp = -1;

        /** Last known MP used to compute chat deltas. */
        std::int32_t m_chat_prev_mp = -1;

        /** Handshake protocol-test value exchanged with the server. */
        std::uint32_t m_protocol_test = 0;

        /** True after S2C_CODE was received during handshake. */
        bool m_got_code = false;

        /** True after a successful handshake. */
        bool m_handshook = false;

        /** True after S2C_ANS_GAMEEXIT was received. */
        bool m_game_exit_answered = false;

        /** Persistence result carried by S2C_ANS_GAMEEXIT. */
        bool m_game_exit_saved = false;

        /** True once a move baseline (server_px/py) is known. */
        bool m_have_move_baseline = false;

        /** Previous-frame local moving flag for C2S move start/stop. */
        bool m_was_moving = false;

        /** Elapsed seconds since the last C2S move sync. */
        float m_move_sync_elapsed = 0.0f;

        /** Local player entity id from CreatePlayer. */
        std::uint32_t m_local_player_id = 0;

        /** Latest player GState bits keyed by entity id (BOF, open-box, fishing). */
        std::unordered_map<std::uint32_t, std::uint32_t> m_player_gstate;

        /** True after an S2C_GSTATE starts or restarts the local fishing cycle. */
        bool m_fishing_progress_start = false;

        /** Skill index of the last C2S targeted skill, for matching the S2C ack. */
        std::uint8_t m_expect_skill_index = 0;

        /** Target of the last C2S targeted skill. */
        std::uint32_t m_expect_skill_target = 0;

        /** Last known server pixel X for the local player. */
        int m_server_px = 0;

        /** Last known server pixel Y for the local player. */
        int m_server_py = 0;

        /** Predicted authoritative height baseline in server units. */
        int m_server_pz = 0;

        /** Cached CreatePlayer fields for the local character. */
        protocol::CreatePlayerFields m_create_player{};

        /** True when runtime state has been filled. */
        bool m_have_create_player = false;

        /** Queued CreatePlayer packets for other PCs (no 0x80). */
        std::vector<protocol::CreatePlayerFields> m_pending_other_players;

        /** Server position baselines for visible remote players. */
        std::unordered_map<std::uint32_t, NetPlayerPosition> m_remote_player_positions;

        /** Visible remote classes for class-specific packet interpretation. */
        std::unordered_map<std::uint32_t, std::uint8_t> m_remote_player_classes;

        /** Queued S2C_REMOVEPLAYER ids. */
        std::vector<std::uint32_t> m_pending_remove_others;

        /** True when a pre-teleport move snapshot is latched. */
        bool m_have_move_before = false;

        /** Server X captured before applying a pending teleport. */
        int m_move_before_sx = 0;

        /** Server Y captured before applying a pending teleport. */
        int m_move_before_sy = 0;

        /** Authoritative correction height in server units. */
        int m_move_before_sz = 0;

        /** True when an S2C_TELEPORT is waiting for the game to apply. */
        bool m_have_teleport = false;

        /** Pending teleport map index. */
        int m_teleport_map = 0;

        /** Pending teleport server X. */
        int m_teleport_sx = 0;

        /** Pending teleport server Y. */
        int m_teleport_sy = 0;

        /** Pending teleport Z. */
        int m_teleport_sz = 0;

        /** Latched local skill/attack presentation for the next frame. */
        bool m_pending_local_skill = false;

        /** Latched local evade (monster CheckHit miss). */
        bool m_pending_evade = false;

        /** True when runtime state has a pending take. */
        bool m_have_hp = false;

        /** True when runtime state has a pending take. */
        bool m_have_mp = false;

        /** True when runtime state has a pending take. */
        bool m_have_max_hp = false;

        /** True when runtime state has a pending take. */
        bool m_have_max_mp = false;

        /** Latest current HP from property / combat packets. */
        std::int32_t m_cur_hp = 0;

        /** Latest current MP from property packets. */
        std::int32_t m_cur_mp = 0;

        /** Latest max HP from P_HP vital. */
        std::int32_t m_max_hp_prop = 0;

        /** Latest max MP from P_MP vital. */
        std::int32_t m_max_mp_prop = 0;

        /** True when EXP bar values have a pending take. */
        bool m_have_exp = false;

        /** Current EXP toward the next level. */
        std::int64_t m_exp_current = 0;

        /** NeedExp for the current level. */
        std::int64_t m_exp_total = 1;

        /** True when runtime state has a pending take. */
        bool m_have_level = false;

        /** Character level. */
        std::int32_t m_level = 1;

        /** True when PU/SU points have a pending take. */
        bool m_have_points = false;

        /** Remaining property (PU) points. */
        std::int32_t m_pu_point = 0;

        /** Remaining skill (SU) points. */
        std::int32_t m_su_point = 0;

        /** True when a full S2C_SKILLINFO snapshot is pending. */
        bool m_have_skill_info = false;

        /** Learned ranks from the last S2C_SKILLINFO. */
        std::unordered_map<int, int> m_skill_info;

        /** Queued S2C_SKILLUP ranks (index, level). */
        std::deque<std::pair<int, int>> m_pending_skill_ups;

        /** True when an S2C_SHORTCUT snapshot is pending. */
        bool m_have_shortcut = false;

        /** True when the pending shortcut packet carried a saved bar. */
        bool m_shortcut_present = false;

        /** Saved bar from the last S2C_SHORTCUT (valid when runtime state). */
        protocol::ShortcutBar m_shortcut;

        /** Queued level-up effect recipients, in receive order. */
        std::deque<std::uint32_t> m_pending_level_up_fx;

        /** Queued charm / refresh oneshot effect attachments (entity, effect ordinal). */
        std::deque<std::pair<std::uint32_t, std::uint8_t>> m_pending_charm_fx;

        /** Queued E_DAMAGED effects (entity id, applied damage). */
        std::deque<std::pair<std::uint32_t, std::uint16_t>> m_pending_damaged_fx;

        /** Active pay-buff slots from MSG_ADDBSTATE / MSG_SUBBSTATE. */
        std::vector<BuffSlotEntry> m_buff_slots;

        /** Timed inventory-register expiries, ordered from register 3 to register 6. */
        std::array<std::uint64_t, 4> m_inventory_register_expire_ms{};

        /** Skill catalog for compatibility behavior → JSON `image` lookup; may be null. */
        const Catalog* m_skill_catalog = nullptr;

        /** Item catalog for item-sourced compatibility behavior HUD entries; may be null. */
        const ItemDb* m_item_catalog = nullptr;

        /** True when rest state has a pending take. */
        bool m_have_rest = false;

        /** Local player resting / sitting. */
        bool m_resting = false;

        /** Rest flags keyed by entity id (includes remotes). */
        std::unordered_map<std::uint32_t, bool> m_entity_resting;

        /** compatibility behavior bitmask keyed by entity id. */
        std::unordered_map<std::uint32_t, std::uint64_t> m_entity_mstate;

        /** True when MSG_ITEMMONSTEROPEN is pending. */
        bool m_have_chest_open_start = false;

        /** Open duration from MSG_ITEMMONSTEROPEN. */
        std::uint8_t m_chest_open_sec = 0;

        /** True when MSG_ITEMMONSTEROPENED / compatibility behavior is pending. */
        bool m_have_chest_open_stop = false;

        /** Last item-monster feedback message ordinal for DEBUG HUD / logs. */
        std::uint8_t m_last_chest_feedback_msg = 0;

        /** True when a hit-from attacker id is pending. */
        bool m_have_hit_from = false;

        /** Attacker entity id for the pending hit-from. */
        std::uint32_t m_hit_from_id = 0;

        /** Local player client-space X (for AOI / presentation). */
        float m_local_wx = 0.0f;

        /** Local player client-space Z (for AOI / presentation). */
        float m_local_wz = 0.0f;

        /** Character name for the Character Info window. */
        std::string m_char_name;

        /** Class index for the Character Info window. */
        int m_char_clazz = 0;

        /** Job / class label (Knight, Magician, …). */
        std::string m_char_job;

        /** Guild name. */
        std::string m_char_guild;

        /** Guild rank / Pos. */
        std::string m_char_guild_pos;

        /** Government Position (G.P.) title. */
        std::string m_char_gov_pos;

        /** Title index into the JSON files in config/jobs (0 = default). */
        int m_specialty_index = 0;

        /** Job specialty bitmask (1 = wandering). */
        std::uint32_t m_specialty = 1;

        /** Awaken label shown in Character Info. */
        std::string m_char_awaken = "Awaken -";

        /** Contribute / compatibility behavior. */
        int m_char_ctb = 0;

        /** STR. */
        std::int32_t m_str = 0;

        /** Health / HTH. */
        std::int32_t m_hth = 0;

        /** INT. */
        std::int32_t m_intl = 0;

        /** WIS. */
        std::int32_t m_wis = 0;

        /** DEX / AGI. */
        std::int32_t m_dex = 0;

        /** Min physical ATK. */
        std::int32_t m_min_attack = 0;

        /** Max physical ATK. */
        std::int32_t m_max_attack = 0;

        /** Min magic ATK. */
        std::int32_t m_min_magic = 0;

        /** Max magic ATK. */
        std::int32_t m_max_magic = 0;

        /** OTP / Hit. */
        std::int32_t m_hit = 0;

        /** Eva / Dodge. */
        std::int32_t m_dodge = 0;

        /** Defense. */
        std::int32_t m_defense = 0;

        /** Absorb percent. */
        std::int32_t m_absorb = 0;

        /** Fire resist. */
        std::int32_t m_res_fire = 0;

        /** Ice resist. */
        std::int32_t m_res_ice = 0;

        /** Lightning resist. */
        std::int32_t m_res_lightning = 0;

        /** Curse resist. */
        std::int32_t m_res_curse = 0;

        /** Non-elemental / palsy resist. */
        std::int32_t m_res_palsy = 0;

        /** Transform rage (P_RAGE). */
        std::int32_t m_rage = 0;

        /** Knight Brutal Attack charges (P_MORTAL). */
        std::int32_t m_mortal = 0;

        /** TFlag: (race << 4) | stage. */
        int m_transform_tflag = 0;

        /** Last P_MSPEED for the local player. */
        std::int16_t m_local_mspeed = 0;

        /** Worn egg item definition index from S2C_UPDATEITEMINFO. */
        int m_transform_egg_index = 0;

        /** Worn egg level from S2C_UPDATEITEMINFO. */
        int m_transform_egg_level = 1;

        /** Worn egg EXP toward the next level. */
        int m_transform_egg_exp = 0;
    };

}  // namespace openkal::client
