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
#include "openkal/map/Portal.h"
#include "openkal/server/CastleWarService.h"
#include "openkal/server/ClockSchedule.h"
#include "openkal/server/Config.h"
#include "openkal/server/FishingCatalog.h"
#include "openkal/server/GuildService.h"
#include "openkal/server/HostRuntimeMetrics.h"
#include "openkal/server/LevelMailRewards.h"
#include "openkal/server/ManagementApi.h"
#include "openkal/server/MixCatalog.h"
#include "openkal/server/RidingCacheCatalog.h"
#include "openkal/server/RidingProgressionCatalog.h"
#include "openkal/server/ServerEvent.h"
#include "openkal/server/ShopCatalog.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/DoggebiRitual.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/NpcRegistry.h"
#include "openkal/server/world/QuestDb.h"
#include "openkal/server/world/QuestEngine.h"
#include "openkal/server/world/StarterKit.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace openkal::server {

    /** Embeddable MainSvr used by standalone and single-player execution. */
    class ServerHost {
      public:
        /** Constructs a stopped server host. */
        ServerHost();

        /** Stops the server and releases owned resources. */
        ~ServerHost();

        ServerHost(const ServerHost&) = delete;
        ServerHost& operator=(const ServerHost&) = delete;

        /** Starts the server worker.
         * @param config Runtime server configuration.
         * @return True when startup succeeds.
         */
        [[nodiscard]] bool start(const ServerConfig& config);

        /** Stops the worker and closes persistence. */
        void stop();

        /** Configures optional measurements before starting the worker.
         * Enabling resets measurements; they remain readable after stop.
         * @param enabled Whether to collect bounded runtime measurements.
         * @return False while a worker is running or still needs joining.
         */
        [[nodiscard]] bool set_runtime_metrics_enabled(bool enabled);

        /** Copies runtime measurements without acquiring the world mutex.
         * @return Measurements, or a disabled empty snapshot when collection is off.
         */
        [[nodiscard]] HostRuntimeMetricsSnapshot runtime_metrics_snapshot() const;

        /** Reports whether the server worker is running.
         * @return True while running.
         */
        [[nodiscard]] bool running() const {
            return m_running.load();
        }

        /** Copies the current monster collection under the world lock.
         * @return Thread-safe monster snapshot.
         */
        [[nodiscard]] std::vector<MonsterEntity> monster_snapshot() const;

        /** Returns the effective server configuration.
         * @return Server configuration.
         */
        [[nodiscard]] const ServerConfig& config() const {
            return m_config;
        }

        /** Returns the active listen endpoint.
         * @return Unix path or TCP host and port.
         */
        [[nodiscard]] std::string listen_endpoint() const;

      private:
        /** Runs socket processing and world ticks. */
        void thread_main();

        /** Advances the world by one tick.
         * @param dt Elapsed time in seconds.
         */
        void tick_once(float dt);

        /** Effective runtime configuration. */
        ServerConfig m_config;

        /** Server-owned notices sent with the hourly clock snapshot. */
        ClockSchedule m_clock_schedule;

        /** Persistence implementation. */
        std::unique_ptr<IDataStore> m_store;
        /** Multiplayer-only guild coordinator, owned before the world worker starts. */
        std::unique_ptr<GuildService> m_guilds;

        /** Castle War, initialized after the guild roster and before gameplay. */
        std::unique_ptr<CastleWarService> m_castle_war;

        /** Authoritative game world. */
        GameWorld m_world;

        /** Quest definitions. */
        QuestDb m_quest_db;

        /** Quest evaluator (bound to runtime state after load). */
        std::unique_ptr<QuestEngine> m_quest_engine;

        /** NPC definitions and live entities. */
        NpcRegistry m_npcs;

        /** Shared Dungeon-3 Doggebi Lord ritual, when its monument layout loaded. */
        std::optional<DoggebiRitual> m_doggebi_ritual;

        /** D1–D6, Emok Island, and Valley of Devah portal rects and destinations. */
        map::PortalCatalog m_portals;

        /** Complete map definition cells used to align outdoor and instance-map spawns. */
        map::MapCatalog m_map_catalog;

        /** Per-class starter equipment tables. */
        StarterKit m_starters;

        /** Mix Master recipes and stones. */
        MixCatalog m_mix;

        /** Server-authoritative per-NPC shop inventories. */
        ShopCatalog m_shops;

        /** Explicit privileged command grants loaded from the server policy. */
        UserRoles m_user_roles;

        /** Separate authenticated local management endpoint. */
        ManagementApi m_management;

        /** Fishing reward and timing configuration. */
        FishingCatalog m_fishing;

        /** Level-up system-mail item attachments. */
        LevelMailRewardCatalog m_level_mail_rewards;

        /** Shared, persistent EXP-event state and tier configuration. */
        ServerEventManager m_exp_event;

        /** Server-authoritative Riding Cache rewards. */
        RidingCacheCatalog m_riding_cache;

        /** Server-authoritative Stablemaster progression. */
        RidingProgressionCatalog m_riding_progression;

        /** Synchronizes world access. */
        mutable std::mutex m_world_mutex;

        /** Protects recorder configuration and its lifetime during snapshot copies. */
        mutable std::mutex m_runtime_metrics_mutex;

        /** Optional recorder, stable throughout the worker lifetime and shutdown. */
        std::unique_ptr<HostRuntimeMetrics> m_runtime_metrics;

        /** Worker running state. */
        std::atomic<bool> m_running{false};

        /** Worker shutdown request. */
        std::atomic<bool> m_stop_requested{false};

        /** Server worker thread. */
        std::thread m_thread;

        /** Next world entity identifier (players, monsters, NPCs). */
        std::uint32_t m_next_entity_id = 1;

        /** Last issued item iid (compatibility behavior); loaded once from MAX(iid) at start. */
        std::atomic<std::uint32_t> m_next_iid{0};

        /** Bound socket endpoint. */
        std::string m_listen_endpoint;
    };

}  // namespace openkal::server
