/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerAgent.h"

#include "TestPlayerSelectorIndex.h"
#include "openkal/client/DialogCatalog.h"
#include "openkal/client/Player.h"
#include "openkal/client/Skills.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/map/MapStream.h"
#include "openkal/map/Portal.h"
#include "openkal/map/Teleport.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"
#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/MState.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/ThiefSkill.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/Combat.h"
#include "openkal/server/ShopCatalog.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/NpcRegistry.h"
#include "openkal/server/world/QuestEngine.h"
#include "openkal/terrain/HeightField.h"
#include "openkal/terrain/IndoorCollision.h"
#include "openkal/terrain/OplLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace {
    using namespace openkal;
    using Position = TestPlayerPosition;
    using Send = std::function<bool(const std::vector<std::uint8_t>&)>;
    double distance(Position a, Position b) {
        return std::hypot(static_cast<double>(a.x) - b.x, static_cast<double>(a.y) - b.y);
    }
    struct Npc {
        int index = 0, map = 0, html = 0;
        std::uint32_t quest = 0;
        Position position;
    };
    struct Item {
        int index = 0, prefix = 0, count = 0;
        std::uint32_t info = 0;
        protocol::InsertCharmFields charm;
    };
    struct Entity {
        int index = 0, hp = 0, race = 0;
        Position position;
        bool kneeling = false;
    };
    std::vector<platform::Json> documents(const std::string& dir) {
        std::vector<platform::Json> result;
        for (const auto& name : platform::list_directory(dir)) {
            if (std::filesystem::path(name).extension() != ".json") {
                continue;
            }
            const auto bytes = platform::read_file(platform::join_path(dir, name));
            platform::Json value;
            if (bytes && platform::Json::parse(std::string(bytes->begin(), bytes->end()), value) &&
                value.is_object()) {
                result.push_back(std::move(value));
            }
        }
        return result;
    }
    bool combat_gear(const server::ItemCombatDef& def) {
        return def.wearable && (def.is_weapon || def.is_defense || def.is_ornament) &&
               def.skin_slot == protocol::SkinSlot::None;
    }
    bool comparable_gear(const Item& item) {
        const auto& c = item.charm;
        return !c.upgrade_level && !c.mix_type && !c.bof && !c.qigong_effect && !c.dragon_effect &&
               !c.random_stat_type;
    }
}  // namespace

/** Catalogs are immutable after loading and shared across player threads. */
class TestPlayerCatalogs {
    friend class TestPlayerAgent;
    friend class TestPlayerData;
    server::GameWorld m_catalog;
    server::QuestDb m_quests;
    server::ShopCatalog m_shops;
    client::DialogCatalog m_dialogs;
    client::TaskCatalog m_tasks;
    client::Catalog m_skills;
    std::vector<std::uint16_t> m_taskIds;
    std::set<int> m_questRequiredItems;
    std::map<int, Npc> m_npcs;
    map::MapCatalog m_maps;
    map::PortalCatalog m_portals;
    struct PortalEdge {
        const map::PortalRecord* portal;
        std::string key;
    };
    // Catalog storage is immutable after loading; edge order preserves authored route ties.
    std::map<int, std::vector<PortalEdge>> m_portalEdges;
    /** Static spawn/drop joins populated before immutable catalog publication. */
    std::optional<TestPlayerSelectorIndex> m_selectorIndex;
    std::string m_areas, m_visuals, m_assets;
};

/** Each agent owns its lazy navigation caches; no pathfinding holds a shared lock. */
class TestPlayerData {
    friend class TestPlayerAgent;
    std::shared_ptr<const TestPlayerCatalogs> m_catalogs;
    struct QuestDiagnostics {
        std::mutex mutex;
        std::set<std::pair<std::uint32_t, std::string>> reported;
    };
    std::shared_ptr<QuestDiagnostics> m_questDiagnostics = std::make_shared<QuestDiagnostics>();
    map::WorldGrid m_grid;
    terrain::FloorField m_outdoor;
    std::set<std::pair<int, int>> m_loadedProps, m_loadedPropNeighborhoods;
    std::map<std::string, terrain::IndoorCollision> m_indoor;
    std::set<std::pair<int, int>> m_missingAreas, m_missingHeights;
    struct StepKey {
        int map = 0, x = 0, y = 0, z = 0, nextX = 0, nextY = 0;
        bool operator==(const StepKey& other) const {
            return map == other.map && x == other.x && y == other.y && z == other.z &&
                   nextX == other.nextX && nextY == other.nextY;
        }
    };
    struct StepHash {
        std::size_t operator()(const StepKey& key) const {
            std::size_t result = 0;
            for (const int value : {key.map, key.x, key.y, key.z, key.nextX, key.nextY}) {
                result ^= std::hash<int>{}(value) + 0x9e3779b9u + (result << 6) + (result >> 2);
            }
            return result;
        }
    };
    // Incoming height and direction remain significant for cached movement edges.
    std::unordered_map<StepKey, std::optional<int>, StepHash> m_pixelSteps;
    std::unordered_map<StepKey, std::optional<int>, StepHash> m_segments;
    std::uint64_t m_geometryRevision = 0;
    std::optional<StepKey> m_standingKey;
    std::optional<int> m_standingHeight;

    void invalidate_geometry() {
        ++m_geometryRevision;
        m_pixelSteps.clear();
        m_segments.clear();
        m_standingKey.reset();
    }

    bool outdoor_map(int mapIndex, int x, int y) const {
        const auto* info =
            m_catalogs->m_maps.find_cell(mapIndex, x / map::kWorldSize, y / map::kWorldSize);
        if (!info) {
            info = m_catalogs->m_maps.find_index(mapIndex);
        }
        return mapIndex == 0 || (info && info->kind() == 1 && info->mapname().empty());
    }

    std::optional<int> step(int mapIndex, Position from, int x, int y) {
        if (x < 0 || y < 0 || x >= 524288 || y >= 524288) {
            return {};
        }
        const StepKey key{mapIndex, from.x, from.y, from.z, x, y};
        if (const auto found = m_segments.find(key); found != m_segments.end()) {
            return found->second;
        }
        const auto revision = m_geometryRevision;
        const auto height = resolve_step(mapIndex, from, x, y);
        // Loading neighboring geometry can change segments checked earlier in this query.
        if (revision == m_geometryRevision) {
            if (m_segments.size() >= 4096) {
                m_segments.clear();
            }
            m_segments.emplace(key, height);
        }
        return height;
    }

    std::optional<int> resolve_step(int mapIndex, Position from, int x, int y) {
        // Load standing surfaces before interpreting heightless area masks.
        if (!surface(mapIndex, from, x, y, false)) {
            return {};
        }
        // Match the ordinary client's standing-point mask and body-height wall ray.
        const int samples = std::max(1, static_cast<int>(std::ceil(distance(from, {x, y, 0}))));
        Position previous = from;
        for (int i = 0; i <= samples; ++i) {
            const int px = from.x + (x - from.x) * i / samples;
            const int py = from.y + (y - from.y) * i / samples;
            if (i > 0 && px == previous.x && py == previous.y) {
                continue;
            }
            const auto height = pixel_step(mapIndex, previous, px, py);
            if (!height) {
                return {};
            }
            previous = {px, py, *height};
        }
        return previous.z;
    }

    std::optional<int> pixel_step(int mapIndex, Position from, int x, int y) {
        if (x < 0 || y < 0 || x >= 524288 || y >= 524288) {
            return {};
        }
        const StepKey key{mapIndex, from.x, from.y, from.z, x, y};
        if (const auto found = m_pixelSteps.find(key); found != m_pixelSteps.end()) {
            return found->second;
        }
        const auto resolve = [&]() -> std::optional<int> {
            const auto height = x == from.x && y == from.y ? std::optional<int>{from.z}
                                                           : surface(mapIndex, from, x, y, true);
            if (!height || std::abs(*height - from.z) > 127) {
                return {};
            }
            const auto cell = std::make_pair(x / map::kWorldSize, y / map::kWorldSize);
            if (!m_grid.cell(cell.first, cell.second)) {
                if (m_missingAreas.count(cell) ||
                    !map::MapStream::ensure_cell(
                        m_grid, m_catalogs->m_areas, cell.first, cell.second)) {
                    m_missingAreas.insert(cell);
                    return {};
                }
            }
            if (m_grid.not_moveable_world_pixel(x, y)) {
                const float wx = coords::to_client(x), wz = coords::to_client(y);
                // The normal client allows a reachable bridge above blocked terrain.
                if (!outdoor_map(mapIndex, x, y) || !m_outdoor.height_field().covers(wx, wz) ||
                    m_outdoor.sample(wx, wz, coords::to_client_y(from.z)) <=
                        m_outdoor.sample_terrain(wx, wz) +
                            terrain::FloorField::kStandableDeckEpsilon) {
                    return {};
                }
            }
            return height;
        };
        const auto height = resolve();
        if (m_pixelSteps.size() >= 262144) {
            m_pixelSteps.clear();
        }
        m_pixelSteps.emplace(key, height);
        return height;
    }

    std::optional<int> standing_height(int mapIndex, Position position) {
        const StepKey key{mapIndex, position.x, position.y, position.z, position.x, position.y};
        if (!m_standingKey || !(*m_standingKey == key)) {
            m_standingHeight = surface(mapIndex, position, position.x, position.y, false);
            m_standingKey = key;
        }
        return m_standingHeight;
    }

    void load_outdoor_props(std::pair<int, int> cell) {
        if (m_loadedProps.insert(cell).second) {
            invalidate_geometry();
            std::array<char, 40> name{};
            std::snprintf(name.data(), name.size(), "n_%03d_%03d.opl", cell.first, cell.second);
            auto path = platform::join_path(m_catalogs->m_visuals, name.data());
            if (!platform::file_exists(path)) {
                path.replace(path.size() - 4, 4, ".OPL");
            }
            terrain::OplFile objects;
            if (platform::file_exists(path) && terrain::load_opl(path, objects)) {
                for (const auto& instance : objects.instances) {
                    const auto modelPath =
                        platform::resolve_client_asset(m_catalogs->m_assets, instance.name, ".gb");
                    if (modelPath.empty()) {
                        continue;
                    }
                    float wx = 0, wy = 0, wz = 0;
                    terrain::opl_world_translation(instance, cell.first, cell.second, wx, wy, wz);
                    const auto transform = engine::Mat4::multiply(
                        engine::Mat4::multiply(
                            engine::Mat4::translation(wx, wy, wz),
                            engine::Mat4::from_quaternion(
                                instance.qx, instance.qy, instance.qz, instance.qw)),
                        engine::Mat4::scaling(instance.sx, instance.sy, instance.sz));
                    m_outdoor.add_prop(modelPath, transform, cell.first, cell.second);
                    m_outdoor.add_obstacle(modelPath, transform, cell.first, cell.second);
                }
            }
        }
    }

    std::optional<int> surface(int mapIndex, Position from, int x, int y, bool moving) {
        if (x < 0 || y < 0 || x >= 524288 || y >= 524288) {
            return {};
        }
        const auto* info =
            m_catalogs->m_maps.find_cell(mapIndex, x / map::kWorldSize, y / map::kWorldSize);
        if (!info) {
            info = m_catalogs->m_maps.find_index(mapIndex);
        }
        if (mapIndex != 0 && (!info || info->kind() != 1 || !info->mapname().empty())) {
            if (!info || info->mapname().empty()) {
                return {};
            }
            const int cx = coords::even_cell(info->x()), cy = coords::even_cell(info->y());
            if (x < cx * map::kWorldSize || y < cy * map::kWorldSize ||
                x >= (cx + 2) * map::kWorldSize || y >= (cy + 2) * map::kWorldSize) {
                return {};
            }
            const std::string key =
                std::to_string(mapIndex) + ":" + std::to_string(cx) + ":" + std::to_string(cy);
            auto [it, inserted] = m_indoor.try_emplace(key);
            if (inserted) {
                const auto file =
                    map::IndoorMap::resolve_map_file(m_catalogs->m_visuals, info->mapname());
                if (!file.empty()) {
                    (void) it->second.load(m_catalogs->m_visuals, file, cx, cy);
                }
            }
            const auto* floor = it->second.floor();
            float height = 0;
            const engine::Vec3 a{static_cast<float>(from.x),
                                 coords::to_client_y(from.z),
                                 static_cast<float>(from.y)};
            engine::Vec3 b{static_cast<float>(x), a.y, static_cast<float>(y)};
            if (!floor || !terrain::IndoorCollision::resolve_standing(*floor, b, height)) {
                return {};
            }
            b.y = height;
            if (moving && !terrain::IndoorCollision::can_move(*floor, a, b)) {
                return {};
            }
            return coords::to_server_z(height);
        }
        const auto cell = std::make_pair(x / map::kWorldSize, y / map::kWorldSize);
        if (!m_outdoor.height_field().find_patch(cell.first, cell.second)) {
            if (m_missingHeights.count(cell)) {
                return {};
            }
            std::array<char, 40> name{};
            std::snprintf(name.data(), name.size(), "n_%03d_%03d.kcm", cell.first, cell.second);
            terrain::HeightPatch patch;
            if (!terrain::load_kcm(platform::join_path(m_catalogs->m_visuals, name.data()),
                                   patch)) {
                m_missingHeights.insert(cell);
                return {};
            }
            m_outdoor.add_patch(std::move(patch));
            invalidate_geometry();
        }
        // A bridge or wall can extend into this cell from a neighboring OPL placement.
        // Load that geometry before resolving standing heights or caching movement edges.
        if (m_loadedPropNeighborhoods.insert(cell).second) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (cell.first + dx >= 0 && cell.second + dy >= 0 && cell.first + dx < 64 &&
                        cell.second + dy < 64) {
                        load_outdoor_props({cell.first + dx, cell.second + dy});
                    }
                }
            }
        }
        const float wx = coords::to_client(x), wy = coords::to_client(y);
        if (!m_outdoor.height_field().covers(wx, wy)) {
            return {};
        }
        float standing = coords::to_client_y(from.z);
        if (!m_outdoor.resolve_standing(wx, wy, standing, standing)) {
            return {};
        }
        const int height = coords::to_server_z(standing);
        if (moving) {
            const float ax = coords::to_client(from.x), az = coords::to_client(from.y);
            const float dx = wx - ax, dz = wy - az;
            const float length = std::hypot(dx, dz);
            if (length > 0 && m_outdoor.has_blockers()) {
                float hit = length + client::kBodyRadius;
                if (m_outdoor.raycast(ax,
                                      coords::to_client_y(from.z) + client::kBodyHeight,
                                      az,
                                      dx / length,
                                      0,
                                      dz / length,
                                      hit,
                                      hit,
                                      terrain::RayHitFilter::Walls) &&
                    hit < length + client::kBodyRadius) {
                    return {};
                }
            }
        }
        return height;
    }
};

class TestPlayerAgent::Impl {
    friend class TestPlayerAgent;

  public:
    explicit Impl(const server::CharacterRecord& initial)
        : m_character(initial), m_random(static_cast<std::mt19937::result_type>(initial.id)) {
        m_pose = {initial.map_x, initial.map_y, initial.map_z};
    }

  private:
    server::CharacterRecord m_character;
    std::mt19937 m_random;
    Position m_pose;
    std::shared_ptr<TestPlayerData> m_data;
    bool m_enabled = false, m_ready = false, m_moving = false, m_resting = false;
    bool m_recovering = false, m_selling = false;
    bool m_saleCityRequested = false;
    std::string m_state = "Paused";
    std::uint32_t m_self = 0, m_target = 0;
    std::uint32_t m_gstate = 0;
    std::uint64_t m_mstate = 0;
    std::int16_t m_moveSpeedPercent = 0;
    bool m_boosting = false;
    bool m_boostPending = false;
    std::uint64_t m_boostDeadline = 0, m_boostRetry = 0;
    struct MovementBuff {
        std::uint64_t expires = 0;
        int item = 0;
        // Initial snapshots arrive before tick supplies the controller's monotonic clock.
        bool awaitingFirstTick = false;
    };
    std::map<std::uint64_t, MovementBuff> m_movementBuffs;
    std::uint64_t m_lastWalk = 0;
    double m_movementCredit = 0;
    std::uint32_t m_equipCandidate = 0, m_equipPrevious = 0;
    enum class EquipmentStage { Removing, PuttingOn, Restoring };
    EquipmentStage m_equipmentStage = EquipmentStage::Removing;
    std::uint64_t m_equipmentDeadline = 0, m_targetSince = 0;
    int m_targetHp = 0;
    std::map<std::uint32_t, Item> m_items;
    std::map<std::uint32_t, Entity> m_monsters, m_drops;
    std::map<std::uint32_t, std::uint64_t> m_attackers;
    std::map<std::uint32_t, protocol::CreateNpcInfo> m_npcs;
    server::PlayerQuestMap m_quests;
    mutable std::unordered_map<std::uint32_t, bool> m_questPreviews;
    int m_questNpcIndex = 0;
    std::map<int, int> m_skills;
    std::map<int, std::uint64_t> m_skillReady;
    std::set<std::uint32_t> m_beheadAttempts;
    unsigned m_primarySnapshot = 0;
    bool m_pointsSeen = false;
    struct PendingStat {
        int property, previous;
        std::uint64_t deadline;
        bool primarySeen = false, pointsSeen = false;
    };
    std::optional<PendingStat> m_pendingStat;
    std::map<std::string, std::uint64_t> m_avoid;
    std::deque<Position> m_route;
    std::unique_ptr<TestPlayerRouteSearch> m_search;
    Position m_destination;
    std::string m_routeKey;
    struct FailedRoute {
        int map = 0;
        Position start, goal;
    };
    std::map<std::string, FailedRoute> m_failedRoutes;
    mutable bool m_portalRoutesReady = false;
    mutable std::map<int, const map::PortalRecord*> m_portalRoutes;
    enum class ProbeAvailability { Unchecked, Standable, Unavailable };
    struct SpawnSurvey {
        std::vector<Position> goals;
        std::size_t next = 0;
        std::vector<ProbeAvailability> availability;
    };
    std::map<std::string, SpawnSurvey> m_spawnSurveys;
    std::string m_lastRouteFailure;
    std::uint64_t m_searchStarted = 0;
    int m_searchSpacing = 32;
    std::size_t m_searchExpanded = 0;
    std::chrono::milliseconds m_planningBudget{2};
    std::function<bool(std::size_t)> m_cityTravel;
    std::set<std::string> m_npcCityFallbacks;
    std::function<bool(const TestPlayerAreaTravel&)> m_areaTravel;
    std::optional<std::size_t> m_pendingCity;
    std::optional<TestPlayerAreaTravel> m_pendingArea;
    std::uint64_t m_areaArrivalUntil = 0;
    std::uint64_t m_travelRequestedAt = 0;
    bool m_travelTeleportSeen = false;
    int m_destinationReach = 0, m_html = 0;
    std::uint32_t m_dialogNpc = 0;
    int m_dialogReach = protocol::kNpcInteractionRange;
    std::set<int> m_npcReplyApproaches;
    std::set<int> m_shopReplyApproaches;
    struct PendingNpcReply {
        std::uint32_t entity;
        int index;
        std::uint64_t deadline;
    };
    std::optional<PendingNpcReply> m_pendingNpcReply;
    std::uint64_t m_nextAction = 0, m_potionReady = 0, m_nextMove = 0, m_lastDamage = 0;
    std::uint64_t m_lastProgress = 0, m_now = 0, m_castAt = 0, m_pendingSince = 0;
    int m_pendingSkill = -1, m_pendingCooldown = 0;
    std::uint32_t m_pendingTarget = 0;
    std::optional<Position> m_pendingMove;
    int m_walkFailures = 0, m_heightFailures = 0;
    bool m_adjustingHeight = false;
    protocol::ShopItemList m_shop;
    std::uint32_t m_shopRequestedNpc = 0;
    std::uint64_t m_shopReplyDeadline = 0;

    void expire_dialog() {
        const auto npc = m_npcs.find(m_dialogNpc);
        if (npc == m_npcs.end() ||
            distance(m_pose, {npc->second.x, npc->second.y, npc->second.z}) > m_dialogReach) {
            m_html = 0;
            m_dialogNpc = 0;
            m_pendingNpcReply.reset();
        }
    }

    std::optional<std::size_t> city_for(Position goal, int destinationMap) const {
        if (!m_cityTravel || destinationMap != 0) {
            return {};
        }
        const double direct = m_character.map == destinationMap
                                  ? distance(m_pose, goal)
                                  : std::numeric_limits<double>::infinity();
        double best = direct - 1024;
        std::optional<std::size_t> chosen;
        for (std::size_t index = 0; index < map::kTeleportLocCount; ++index) {
            const auto& city = map::kTeleportLocs[index];
            if (city.map != destinationMap) {
                continue;
            }
            const double remaining = distance({city.x, city.y, city.z}, goal);
            if (remaining < best) {
                best = remaining;
                chosen = index;
            }
        }
        return chosen;
    }
    bool request_city(std::size_t city, const Send& send) {
        if (m_pendingMove) {
            return true;
        }
        stop(send);
        m_pendingCity = city;
        m_travelRequestedAt = m_now;
        m_travelTeleportSeen = false;
        m_state = "Teleporting to " + std::string(map::kTeleportLocs[city].name);
        if (!m_cityTravel(city)) {
            m_pendingCity.reset();
            m_enabled = false;
            m_state = "Paused: city teleport request unavailable";
        }
        return true;
    }
    bool travel_to_city(Position goal, int destinationMap, const Send& send) {
        const auto city = city_for(goal, destinationMap);
        return city && request_city(*city, send);
    }
    void return_to_city(Position goal, const std::string& key, const Send& send) {
        if (!m_cityTravel || m_character.map != 0 || m_pendingMove ||
            m_npcCityFallbacks.count(key)) {
            return;
        }
        std::optional<std::size_t> chosen;
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < map::kTeleportLocCount; ++index) {
            const auto& city = map::kTeleportLocs[index];
            if (city.map != m_character.map) {
                continue;
            }
            const double remaining = distance({city.x, city.y, city.z}, goal);
            if (remaining < best) {
                chosen = index;
                best = remaining;
            }
        }
        if (!chosen) {
            return;
        }
        const auto& city = map::kTeleportLocs[*chosen];
        if (distance(m_pose, {city.x, city.y, city.z}) <= 32) {
            return;
        }
        m_npcCityFallbacks.insert(key);
        (void) request_city(*chosen, send);
    }

    void travel_to_area(const server::MonsterSpawn& spawn, Position goal, const Send& send) {
        if (!m_areaTravel || m_pendingMove) {
            return;
        }
        const auto height = m_data->standing_height(spawn.map_index, goal);
        if (!height) {
            return;
        }
        goal.z = *height;
        if (!m_data->step(spawn.map_index, goal, goal.x, goal.y)) {
            return;
        }
        stop(send);
        m_pendingArea = TestPlayerAreaTravel{spawn.map_index, spawn.area, goal};
        m_travelRequestedAt = m_now;
        m_travelTeleportSeen = false;
        m_state = "Teleporting to spawn area " + std::to_string(spawn.area);
        if (!m_areaTravel(*m_pendingArea)) {
            m_pendingArea.reset();
            m_enabled = false;
            m_state = "Paused: area teleport request unavailable";
        }
    }
    const map::PortalRecord* portal_to(int destinationMap) const {
        if (destinationMap == m_character.map) {
            return nullptr;
        }
        if (!m_portalRoutesReady) {
            m_portalRoutes.clear();
            std::queue<int> maps;
            maps.push(m_character.map);
            m_portalRoutes[m_character.map] = nullptr;
            while (!maps.empty()) {
                const int source = maps.front();
                maps.pop();
                const auto edges = m_data->m_catalogs->m_portalEdges.find(source);
                if (edges == m_data->m_catalogs->m_portalEdges.end()) {
                    continue;
                }
                for (const auto& edge : edges->second) {
                    const auto& portal = *edge.portal;
                    if (m_portalRoutes.count(portal.to_map())) {
                        continue;
                    }
                    const auto skipped = m_avoid.find(edge.key);
                    const auto failed = m_failedRoutes.find(edge.key);
                    if ((failed != m_failedRoutes.end() &&
                         route_failed(edge.key, failed->second.goal)) ||
                        (skipped != m_avoid.end() && skipped->second > m_now)) {
                        continue;
                    }
                    m_portalRoutes[portal.to_map()] =
                        source == m_character.map ? &portal : m_portalRoutes.at(source);
                    maps.push(portal.to_map());
                }
            }
            m_portalRoutesReady = true;
        }
        const auto found = m_portalRoutes.find(destinationMap);
        return found == m_portalRoutes.end() ? nullptr : found->second;
    }

    bool travel_to_map(int destinationMap, const Send& send) {
        const auto* portal = portal_to(destinationMap);
        if (!portal) {
            return false;
        }
        const auto key = "portal:" + portal->id();
        const int width = portal->right() - portal->left();
        const int height = portal->bottom() - portal->top();
        if (width <= 0 || height <= 0) {
            return false;
        }
        Position gate{(portal->left() + width / 2) * 32 + 16,
                      (portal->top() + height / 2) * 32 + 16,
                      m_pose.z};
        // Large lookup rectangles require an actual TILE_PORTAL trigger tile.
        if (static_cast<std::int64_t>(width) * height > map::kMaxUngatedPortalArea) {
            bool found = false;
            int budget = 4096;
            for (int y = portal->top(); y < portal->bottom() && budget > 0 && !found; ++y) {
                for (int x = portal->left(); x < portal->right() && budget-- > 0; ++x) {
                    const int px = x * 32 + 16, py = y * 32 + 16;
                    (void) map::MapStream::ensure_cell(m_data->m_grid,
                                                       m_data->m_catalogs->m_areas,
                                                       px / map::kWorldSize,
                                                       py / map::kWorldSize);
                    std::uint32_t attributes = 0;
                    if (m_data->m_grid.attr_at_world_tile(x, y, attributes) &&
                        map::has_attr(attributes, map::kAttrPortal)) {
                        gate = {px, py, m_pose.z};
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                m_avoid[key] = m_now + 30000;
                m_portalRoutesReady = false;
                return false;
            }
        }
        m_state = "Travelling through portal " + portal->id();
        if (walk(gate, 0, key, send)) {
            (void) send_action(protocol::Move::frame_client_end(0, 0, 0), send, 3000);
            m_avoid[key] = m_now + 30000;
            m_portalRoutesReady = false;
        }
        return true;
    }

    const server::ItemCombatDef* definition(const Item& item) const {
        const auto it = m_data->m_catalogs->m_catalog.item_defs().find(item.index);
        return it == m_data->m_catalogs->m_catalog.item_defs().end() ? nullptr : &it->second;
    }
    int count(int index) const {
        int result = 0;
        for (const auto& [id, item] : m_items) {
            (void) id;
            if (item.index == index) {
                result += item.count;
            }
        }
        return result;
    }
    bool allowed(const server::ItemCombatDef& def) const {
        return combat_gear(def) && def.level_limit <= m_character.level &&
               (def.class_limit < 0 || def.class_limit == m_character.clazz);
    }
    double score(const Item& item) const {
        const auto* def = definition(item);
        if (!def) {
            return -1;
        }
        // Heuristic utility, never an authority for whether the server permits wearing.
        const bool mage = m_character.clazz == 1;
        double value = 4.0 * (def->defense_melee + def->defense_range + def->absorb) +
                       0.2 * def->hp + def->hth * 2 + def->dodge +
                       (mage ? def->intl + def->wis : def->str + def->dex);
        if (def->is_weapon) {
            const double damage = mage
                                      ? def->min_magic + def->max_magic + 2 * item.charm.x_magic
                                      : def->min_attack + def->max_attack + 2 * item.charm.x_attack;
            value += damage * 1000.0 / std::max(300, def->attack_speed_ms);
        }
        value += 4 * item.charm.x_defense + item.charm.x_dodge + item.charm.x_hit;
        if (const auto it = m_data->m_catalogs->m_catalog.prefix_defs().find(item.prefix);
            it != m_data->m_catalogs->m_catalog.prefix_defs().end()) {
            const auto& p = it->second;
            value += 0.2 * p.hp + p.hth * 2 + (mage ? p.intl + p.wis : p.str + p.dex);
            value += 4.0 * (p.defense + p.absorb) + p.hit +
                     (mage ? p.min_magic + p.max_magic : p.min_attack + p.max_attack);
        }
        return value;
    }
    bool protected_item(const Item& item) const {
        const auto* def = definition(item);
        if (!def || protocol::insert_put_on(item.info) ||
            protocol::insert_account_bound(item.info) ||
            protocol::insert_character_bound(item.info) || def->pay || !combat_gear(*def) ||
            def->sell <= 0 || item.prefix != 0 || item.charm.upgrade_level || item.charm.bof ||
            item.charm.qigong_effect || item.charm.dragon_effect || item.charm.random_stat_type ||
            item.charm.x_attack || item.charm.x_magic || item.charm.x_defense || item.charm.x_hit ||
            item.charm.x_dodge) {
            return true;
        }
        if (m_data->m_catalogs->m_questRequiredItems.count(item.index)) {
            return true;
        }
        for (const auto id : m_data->m_catalogs->m_taskIds) {
            const auto* task = m_data->m_catalogs->m_tasks.find(id);
            if (task) {
                for (const auto& flag : task->flags) {
                    if (flag.item_index == item.index) {
                        return true;
                    }
                }
            }
        }
        // Retain future upgrades as well as usable upgrades.
        const auto kind = server::wear_kind_of(*def);
        double worn = -1;
        for (const auto& [id, other] : m_items) {
            (void) id;
            const auto* otherDef = definition(other);
            if (otherDef && protocol::insert_put_on(other.info) &&
                server::wear_kind_of(*otherDef) == kind) {
                worn = std::max(worn, score(other));
            }
        }
        return (def->class_limit < 0 || def->class_limit == m_character.clazz) &&
               score(item) > worn;
    }
    bool send_action(const std::vector<std::uint8_t>& packet,
                     const Send& send,
                     std::uint64_t delay = 1000) {
        if (!send(packet)) {
            m_state = "Transport unavailable";
            m_enabled = false;
            return false;
        }
        m_nextAction = m_now + delay;
        return true;
    }
    void reset_movement_clock() {
        m_lastWalk = 0;
        m_nextMove = 0;
        m_movementCredit = 0;
    }
    void clear_boost() {
        m_boosting = false;
        m_boostPending = false;
        reset_movement_clock();
    }
    void request_boost(const Send& send) {
        if (m_boostPending && m_now >= m_boostDeadline) {
            m_boostPending = false;
            m_boostRetry = m_now + 30000;
        }
        if (m_boosting || m_boostPending || m_resting || m_now < m_boostRetry ||
            (m_mstate & protocol::kCmsMoveStop) != 0) {
            return;
        }
        const auto haste = m_movementBuffs.find(protocol::kCbsSkillHaste);
        if (haste != m_movementBuffs.end() &&
            (haste->second.expires == 0 || haste->second.expires > m_now)) {
            return;
        }
        const auto learned = m_skills.find(0);
        const auto* skill = m_data->m_catalogs->m_skills.find(m_character.clazz, 0);
        if (learned == m_skills.end() || learned->second <= 0) {
            return;
        }
        // The server accepts learned run mode without a catalog definition, at six MP.
        const int cost = skill ? client::Catalog::skill_spend_mp(
                                     m_character.clazz, skill, learned->second, m_character.level)
                               : 6;
        if (m_character.mp < cost) {
            return;
        }
        if (send(protocol::Skill::frame_client_boost(1))) {
            m_boostPending = true;
            m_boostDeadline = m_now + 3000;
        } else {
            m_enabled = false;
            m_state = "Transport unavailable";
        }
    }
    double walking_speed() const {
        if ((m_mstate & protocol::kCmsMoveStop) != 0) {
            return 0;
        }
        int percent = m_moveSpeedPercent;
        for (const auto& [bit, buff] : m_movementBuffs) {
            if (buff.expires != 0 && buff.expires <= m_now) {
                continue;
            }
            if (bit == protocol::kCbsSkillHaste) {
                if (buff.item > 0) {
                    const auto it = m_data->m_catalogs->m_catalog.item_defs().find(buff.item);
                    if (it != m_data->m_catalogs->m_catalog.item_defs().end() &&
                        it->second.buff_type == static_cast<int>(protocol::BuffType::Haste)) {
                        percent += it->second.buff_amount;
                    }
                } else if (const auto skill = m_skills.find(26); skill != m_skills.end()) {
                    percent += 15 * skill->second;
                }
            } else if (bit == protocol::kCbsSkillEscape) {
                percent += protocol::kArcherEscapeMoveSpeedPercent;
            } else if (bit == protocol::kCbsSkillArcherSharpness) {
                percent += protocol::kArcherSharpnessMoveSpeedPercent;
            } else if (bit == protocol::kCbsSkillHiding) {
                percent += protocol::kThiefHidingMoveSpeedPercent;
            }
        }
        return (m_boosting ? client::kRunSpeed : client::kWalkSpeed) *
               protocol::move_speed_factor(
                   static_cast<std::int16_t>(std::clamp(percent, -99, 300)));
    }
    bool route_failed(const std::string& key, Position goal) const {
        const auto it = m_failedRoutes.find(key);
        return it != m_failedRoutes.end() && it->second.map == m_character.map &&
               distance(it->second.start, m_pose) <= 32 &&
               std::abs(it->second.start.z - m_pose.z) <= 70 &&
               distance(it->second.goal, goal) <= 24;
    }
    void fail_route(const std::string& key, Position goal, const std::string& reason) {
        m_failedRoutes[key] = {m_character.map, m_pose, goal};
        m_portalRoutesReady = false;
        m_lastRouteFailure = reason + " (" + key + ")";
        m_state = m_lastRouteFailure;
        m_search.reset();
        m_route.clear();
    }
    bool align_height(const Send& send) {
        if (m_pendingMove) {
            if (!m_adjustingHeight) {
                return false;
            }
            m_state = "Waiting for standing-height confirmation";
            if (m_now - m_pendingSince > 2500) {
                (void) send(protocol::Move::frame_client_end(0, 0, 0));
                m_pendingSince = m_now;
            }
            return true;
        }
        const auto height = m_data->standing_height(m_character.map, m_pose);
        // Resolve against loaded decks as well as terrain before correcting either direction.
        if (!height || *height == m_pose.z) {
            return false;
        }
        if (m_heightFailures >= 3) {
            m_enabled = false;
            m_state = "Paused: standing-height movement repeatedly rejected";
            return true;
        }
        m_state = "Aligning standing height";
        if ((m_mstate & protocol::kCmsMoveStop) != 0 || m_now < m_nextMove) {
            return true;
        }
        m_route.clear();
        m_search.reset();
        m_lastWalk = 0;
        m_movementCredit = 0;
        const auto dz = static_cast<std::int8_t>(std::clamp(*height - m_pose.z, -127, 127));
        if (send(protocol::Move::frame_client_end(0, 0, dz))) {
            m_pendingMove = Position{m_pose.x, m_pose.y, m_pose.z + dz};
            m_pendingSince = m_now;
            m_nextMove = m_now + 50;
            m_adjustingHeight = true;
            m_moving = false;
        }
        return true;
    }
    void stop(const Send& send) {
        reset_movement_clock();
        m_route.clear();
        m_search.reset();
        if (m_moving && !m_pendingMove) {
            (void) send(protocol::Move::frame_client_end(0, 0, 0));
            m_moving = false;
        }
    }
    void begin_search(int spacing) {
        m_searchSpacing = spacing;
        m_search = std::make_unique<TestPlayerRouteSearch>(
            m_pose,
            m_destination,
            m_destinationReach,
            [this](Position from, int x, int y) {
                return m_data->step(m_character.map, from, x, y);
            },
            spacing == 32 && distance(m_pose, m_destination) <= 4096 ? 8192 : 65536,
            spacing);
    }
    bool walk(Position goal,
              int reach,
              const std::string& key,
              const Send& send,
              std::uint64_t planningLimit = 90000) {
        if (travel_to_city(goal, m_character.map, send)) {
            return false;
        }
        if (m_avoid[key] > m_now || route_failed(key, goal)) {
            return false;
        }
        if (align_height(send)) {
            return false;
        }
        if (distance(m_pose, goal) <= reach) {
            stop(send);
            return true;
        }
        request_boost(send);
        if (!m_enabled) {
            return false;
        }
        const double speed = walking_speed();
        if (speed <= 0) {
            stop(send);
            m_state = "Waiting: movement stopped by server state";
            return false;
        }
        // Keep fractional distance across packets, but never bank movement while idle or stalled.
        const auto elapsed = m_now >= m_lastWalk ? m_now - m_lastWalk : 0;
        if (m_lastWalk == 0 || elapsed > 100) {
            m_movementCredit = speed * 0.05;
        } else {
            m_movementCredit += speed * elapsed / 1000.0;
        }
        m_lastWalk = m_now;
        if (m_pendingMove) {
            m_movementCredit = std::min(m_movementCredit, speed * 0.05 + std::sqrt(2.0));
            if (m_now - m_pendingSince > 2500) {
                m_state = "Waiting for movement confirmation";
                // A zero-distance stop allows recovery after a normal stun without
                // guessing whether an earlier position delta was accepted.
                (void) send(protocol::Move::frame_client_end(0, 0, 0));
                m_pendingSince = m_now;
            }
            return false;
        }
        if (m_routeKey != key ||
            (!m_search && (distance(m_destination, goal) > 24 || m_destinationReach != reach))) {
            m_routeKey = key;
            m_route.clear();
            m_search.reset();
        }
        if (m_route.empty()) {
            if (!m_search) {
                m_destination = goal;
                m_destinationReach = reach;
                m_searchStarted = m_now;
                m_searchExpanded = 0;
                begin_search(32);
            }
            // A nearby destination may still require a long bridge or riverbank detour.
            if (m_now - m_searchStarted > planningLimit) {
                fail_route(key, m_destination, "Route planning timed out; choosing another target");
                return false;
            }
            auto result = m_search->advance(32, m_planningBudget);
            if (m_searchSpacing == 32 && result == TestPlayerRouteState::Unavailable) {
                m_searchExpanded += m_search->expanded_nodes();
                begin_search(8);
                result = TestPlayerRouteState::Searching;
            }
            if (result == TestPlayerRouteState::Searching) {
                m_movementCredit = std::min(m_movementCredit, speed * 0.05);
                m_state = "Planning route: " +
                          std::to_string(m_searchExpanded + m_search->expanded_nodes()) +
                          " nodes (" + key + ", " + std::to_string(m_searchSpacing) + "px grid)";
                return false;
            }
            const auto& found = m_search->route();
            m_route.assign(found.begin(), found.end());
            m_search.reset();
            m_lastProgress = m_now;
            if (result == TestPlayerRouteState::Unavailable || m_route.empty()) {
                fail_route(key, m_destination, "No route or missing navigation data");
                return false;
            }
        }
        if (m_now < m_nextMove) {
            return false;
        }
        const auto waypoint = m_route.front();
        const double remaining = distance(m_pose, waypoint);
        const double fraction = remaining > 0 ? std::min(1.0, m_movementCredit / remaining) : 1.0;
        int dx = static_cast<int>((waypoint.x - m_pose.x) * fraction);
        int dy = static_cast<int>((waypoint.y - m_pose.y) * fraction);
        if (dx == 0 && dy == 0) {
            return false;
        }
        Position next{m_pose.x + dx, m_pose.y + dy, waypoint.z};
        auto height = m_data->step(m_character.map, m_pose, next.x, next.y);
        if (!height || std::abs(*height - m_pose.z) > 127) {
            const int sx = (waypoint.x > m_pose.x) - (waypoint.x < m_pose.x);
            const int sy = (waypoint.y > m_pose.y) - (waypoint.y < m_pose.y);
            double best = remaining;
            for (const auto& delta : {std::pair<int, int>{sx, sy}, {sx, 0}, {0, sy}}) {
                if ((delta.first == 0 && delta.second == 0) ||
                    std::hypot(delta.first, delta.second) > m_movementCredit) {
                    continue;
                }
                const Position candidate{m_pose.x + delta.first, m_pose.y + delta.second, m_pose.z};
                const auto standing =
                    m_data->step(m_character.map, m_pose, candidate.x, candidate.y);
                const double left = distance(candidate, waypoint);
                if (standing && std::abs(*standing - m_pose.z) <= 127 && left < best) {
                    next = candidate;
                    dx = delta.first;
                    dy = delta.second;
                    height = standing;
                    best = left;
                }
            }
        }
        if (!height || std::abs(*height - m_pose.z) > 127) {
            m_route.clear();
            if (m_searchSpacing == 32) {
                m_searchStarted = m_now;
                m_searchExpanded = 0;
                begin_search(8);
                m_state = "Refining blocked route (" + key + ")";
            } else {
                fail_route(key, goal, "Route blocked; selecting another target");
            }
            return false;
        }
        if (m_now - m_lastProgress > 3000 || m_walkFailures >= 3) {
            m_route.clear();
            m_walkFailures = 0;
            fail_route(key, goal, "Movement rejected; avoiding target");
            return false;
        }
        next.z = *height;
        const bool end = m_route.size() == 1 && next.x == waypoint.x && next.y == waypoint.y;
        const auto packetX = static_cast<std::int8_t>(dx);
        const auto packetY = static_cast<std::int8_t>(dy);
        const auto packetZ = static_cast<std::int8_t>(next.z - m_pose.z);
        if (send(end ? protocol::Move::frame_client_end(packetX, packetY, packetZ)
                     : protocol::Move::frame_client_on(packetX, packetY, packetZ))) {
            m_pendingMove = next;
            m_pendingSince = m_now;
            m_moving = !end;
            m_movementCredit -= distance(m_pose, next);
            m_nextMove = m_now + 50;
        }
        return false;
    }
    bool preview(std::uint32_t key, std::set<std::uint32_t>& visited, int depth = 0) const {
        if (depth > 12 || !visited.insert(key).second) {
            return false;
        }
        auto progress = m_quests;
        bool productive = false;
        int page = 0;
        server::QuestActionSink sink;
        sink.report_diagnostic = [this](std::uint32_t step, std::string_view reason) {
            bool first = false;
            {
                std::lock_guard<std::mutex> lock(m_data->m_questDiagnostics->mutex);
                first =
                    m_data->m_questDiagnostics->reported.emplace(step, std::string(reason)).second;
            }
            if (first) {
                log::warn("Autonomous quest preview: quest=" +
                          std::to_string(protocol::Quest::key_quest(step)) +
                          " flag=" + std::to_string(protocol::Quest::key_flag(step)) + ": " +
                          std::string(reason) + "; step unavailable");
            }
        };
        sink.clazz = m_character.clazz;
        sink.level = m_character.level;
        sink.specialty = m_character.specialty;
        sink.gstate = m_gstate;
        sink.count_item = [&](int index) { return count(index); };
        sink.send_html = [&](int id) { page = id; };
        sink.send_save_quest = [&](const auto&) { productive = true; };
        sink.exchange_items = [&](const auto&) { return true; };
        sink.add_exp = [&](auto) { productive = true; };
        sink.add_skill_points = [&](int) { productive = true; };
        sink.add_contribution = [&](int) { productive = true; };
        sink.add_specialty = [&](int) { productive = true; };
        // Service toggles and travel alone do not advance a quest journal.
        sink.set_gstate = [](int, bool) {};
        sink.teleport = [](int, int, int) {};
        sink.show_guide = [](int) {};
        sink.redistribute = [] {};
        const server::QuestEngine engine(m_data->m_catalogs->m_quests);
        if (!engine.call_process(key, progress, sink)) {
            return false;
        }
        if (productive) {
            return true;
        }
        if (const auto* dialog = m_data->m_catalogs->m_dialogs.find(page)) {
            for (const auto& link : dialog->links) {
                if (link.type == "quest" &&
                    preview(protocol::Quest::make_key(link.quest_index, link.quest_flag),
                            visited,
                            depth + 1)) {
                    return true;
                }
            }
        }
        return false;
    }
    bool useful_quest(std::uint32_t key) const {
        if (const auto cached = m_questPreviews.find(key); cached != m_questPreviews.end()) {
            return cached->second;
        }
        std::set<std::uint32_t> visited;
        const bool useful = preview(key, visited);
        m_questPreviews.emplace(key, useful);
        return useful;
    }
    int desired_monster() const {
        const auto& selector = *m_data->m_catalogs->m_selectorIndex;
        const auto& definitions = m_data->m_catalogs->m_catalog.defs();
        for (const auto& [id, entry] : m_quests) {
            if (entry.clear) {
                continue;
            }
            const auto* task = m_data->m_catalogs->m_tasks.find(id);
            if (!task) {
                continue;
            }
            for (const auto& flag : task->flags) {
                if (flag.key != entry.flag || flag.level > m_character.level ||
                    flag.item_index <= 0 || count(flag.item_index) >= flag.item_count) {
                    continue;
                }
                for (std::size_t ordinal = 0; ordinal < definitions.size(); ++ordinal) {
                    const auto& def = definitions[ordinal];
                    if (def.level > m_character.level + 1 ||
                        !protocol::MonsterRaceInfo::is_normal_combat(def.race)) {
                        continue;
                    }
                    const auto& maps = selector.spawn_maps(def.index);
                    const bool reachableMap = std::any_of(maps.begin(), maps.end(), [&](int map) {
                        return map == m_character.map || portal_to(map);
                    });
                    if (!reachableMap) {
                        continue;
                    }
                    if (selector.drops_item(ordinal, id, entry.flag, flag.item_index)) {
                        return def.index;
                    }
                }
            }
        }
        return 0;
    }
    bool quest(const Send& send) {
        expire_dialog();
        if (m_pendingNpcReply) {
            const auto& pending = *m_pendingNpcReply;
            m_state = "Quest: waiting for NPC " + std::to_string(pending.index) + " reply";
            if (m_now >= pending.deadline) {
                m_enabled = false;
                m_state = "Paused: NPC " + std::to_string(pending.index) +
                          " did not reply (entity " + std::to_string(pending.entity) + ")";
            }
            return true;
        }
        if (m_html != 0 && m_dialogNpc != 0) {
            const int page = m_html;
            m_html = 0;
            if (const auto* dialog = m_data->m_catalogs->m_dialogs.find(page)) {
                for (const auto& link : dialog->links) {
                    const auto key = protocol::Quest::make_key(link.quest_index, link.quest_flag);
                    if (link.type == "quest" && useful_quest(key) &&
                        m_avoid["quest:" + std::to_string(key)] <= m_now) {
                        stop(send);
                        m_state = "Advancing quest " + std::to_string(link.quest_index);
                        m_avoid["quest:" + std::to_string(key)] = m_now + 10000;
                        if (send_action(protocol::Quest::frame_quest(key), send)) {
                            m_pendingNpcReply = PendingNpcReply{
                                m_dialogNpc, m_npcs.at(m_dialogNpc).index, m_now + 5000};
                        }
                        return true;
                    }
                }
            }
        }
        // Keep a useful quest destination until completion or invalidation.
        const auto eligible = [&](const Npc& npc) {
            const auto key = "npc:" + std::to_string(npc.index);
            const bool city = city_for(npc.position, npc.map).has_value();
            return (npc.map == m_character.map || city || portal_to(npc.map)) &&
                   m_avoid[key] <= m_now && (!route_failed(key, npc.position) || city);
        };
        const Npc* chosen = nullptr;
        const auto objective = [&](int npcIndex) {
            for (const auto& [id, progress] : m_quests) {
                const auto* task = m_data->m_catalogs->m_tasks.find(id);
                if (!task || progress.clear) {
                    continue;
                }
                for (const auto& flag : task->flags) {
                    if (flag.key == progress.flag && flag.npc_index == npcIndex &&
                        flag.level <= m_character.level &&
                        (flag.item_index <= 0 || count(flag.item_index) >= flag.item_count)) {
                        return true;
                    }
                }
            }
            return false;
        };
        if (const auto previous = m_data->m_catalogs->m_npcs.find(m_questNpcIndex);
            previous != m_data->m_catalogs->m_npcs.end() && eligible(previous->second)) {
            chosen = &previous->second;
        }
        if (!chosen) {
            double best = std::numeric_limits<double>::infinity();
            bool bestObjective = false;
            for (const auto& [index, npc] : m_data->m_catalogs->m_npcs) {
                (void) index;
                if (!eligible(npc)) {
                    continue;
                }
                bool available = npc.quest && useful_quest(npc.quest);
                if (const auto* dialog = m_data->m_catalogs->m_dialogs.find(npc.html)) {
                    for (const auto& link : dialog->links) {
                        available |= link.type == "quest" &&
                                     useful_quest(protocol::Quest::make_key(link.quest_index,
                                                                            link.quest_flag));
                    }
                }
                if (!available) {
                    continue;
                }
                const double d =
                    distance(m_pose, npc.position) + (npc.map != m_character.map ? 10000000 : 0);
                const bool activeObjective = objective(npc.index);
                if ((activeObjective && !bestObjective) ||
                    (activeObjective == bestObjective && d < best)) {
                    best = d;
                    bestObjective = activeObjective;
                    chosen = &npc;
                }
            }
        }
        m_questNpcIndex = chosen ? chosen->index : 0;
        if (!chosen) {
            return false;
        }
        if (travel_to_city(chosen->position, chosen->map, send)) {
            return true;
        }
        if (chosen->map != m_character.map) {
            return travel_to_map(chosen->map, send);
        }
        const auto key = "npc:" + std::to_string(chosen->index);
        m_state = "Quest: travelling to NPC " + std::to_string(chosen->index);
        Position goal = chosen->position;
        std::uint32_t runtime = 0;
        for (const auto& [id, npc] : m_npcs) {
            if (npc.index == chosen->index) {
                goal = {npc.x, npc.y, npc.z};
                runtime = id;
                break;
            }
        }
        // Discover the server NPC before searching its potentially obstructed standing point.
        constexpr int sightReach = coords::kSightTileRadius * map::kTileSize / 2;
        const bool discovering = !runtime && distance(m_pose, goal) > sightReach;
        const int reach = discovering ? sightReach
                          : m_npcReplyApproaches.count(chosen->index)
                              ? protocol::kNpcReplyRange
                              : protocol::kNpcInteractionRange;
        if (!discovering && m_routeKey == key && m_search &&
            m_destinationReach > protocol::kNpcReplyRange) {
            m_search.reset();
        }
        if (walk(goal, reach, key, send, discovering ? 90000 : 5000) && runtime) {
            m_dialogNpc = runtime;
            m_dialogReach = reach;
            m_avoid[key] = m_now + 10000;
            if (send_action(protocol::Quest::frame_ask_npc(runtime), send)) {
                m_pendingNpcReply = PendingNpcReply{runtime, chosen->index, m_now + 5000};
                m_state = "Quest: asking NPC " + std::to_string(chosen->index) + " (" +
                          std::to_string(static_cast<int>(std::ceil(distance(m_pose, goal)))) +
                          "px)";
            }
        } else if (route_failed(key, goal)) {
            if (runtime && m_npcReplyApproaches.insert(chosen->index).second) {
                // A shore or collision mask can block close approach while talk remains legal.
                m_failedRoutes.erase(key);
                m_state = "Quest: trying NPC " + std::to_string(chosen->index) +
                          " within server talk range";
            } else {
                return_to_city(goal, key, send);
            }
        } else if (!runtime && distance(m_pose, goal) <= reach) {
            m_state = "Waiting: NPC " + std::to_string(chosen->index) + " is not visible";
        }
        return true;
    }
    bool equip(const Send& send) {
        if (m_equipCandidate) {
            const auto worn = [&](std::uint32_t id) {
                const auto it = m_items.find(id);
                return it != m_items.end() && protocol::insert_put_on(it->second.info);
            };
            if (worn(m_equipCandidate) ||
                (m_equipmentStage == EquipmentStage::Restoring && worn(m_equipPrevious))) {
                m_equipCandidate = m_equipPrevious = 0;
                return false;
            }
            if (m_equipmentStage == EquipmentStage::Removing && !worn(m_equipPrevious)) {
                m_equipmentStage = EquipmentStage::PuttingOn;
                m_equipmentDeadline = m_now + 3000;
                m_state = "Equipping an upgrade";
                (void) send_action(protocol::ItemWear::frame_client_put_on(m_equipCandidate), send);
                return true;
            }
            if (m_now < m_equipmentDeadline) {
                m_state = "Waiting for equipment confirmation";
                return true;
            }
            m_avoid["equip:" + std::to_string(m_equipCandidate)] = m_now + 30000;
            if (m_equipmentStage == EquipmentStage::Removing || m_equipPrevious == 0) {
                m_equipCandidate = m_equipPrevious = 0;
                return false;
            }
            if (m_equipmentStage == EquipmentStage::PuttingOn) {
                m_equipmentStage = EquipmentStage::Restoring;
                m_equipmentDeadline = m_now + 3000;
                m_state = "Upgrade rejected; restoring previous equipment";
                (void) send_action(protocol::ItemWear::frame_client_put_on(m_equipPrevious), send);
                return true;
            }
            m_enabled = false;
            m_state = "Paused: previous equipment could not be restored";
            return true;
        }
        std::uint32_t candidate = 0, wornId = 0;
        double bestGain = 0;
        for (const auto& [id, item] : m_items) {
            const auto* def = definition(item);
            if (!def || !allowed(*def) || !comparable_gear(item) ||
                protocol::insert_put_on(item.info) || item.count != 1 ||
                (protocol::insert_max_end(item.info) > 0 &&
                 protocol::insert_cur_end(item.info) <= 0) ||
                m_avoid["equip:" + std::to_string(id)] > m_now) {
                continue;
            }
            const auto kind = server::wear_kind_of(*def);
            if (kind == server::WearKind::None || kind >= server::WearKind::Transform) {
                continue;
            }
            double oldScore = 0;
            std::uint32_t oldId = 0;
            bool conflict = false;
            for (const auto& [otherId, other] : m_items) {
                const auto* otherDef = definition(other);
                if (!otherDef || !protocol::insert_put_on(other.info)) {
                    continue;
                }
                if ((def->class_small == "sword2h" &&
                     server::wear_kind_of(*otherDef) == server::WearKind::Shield) ||
                    (kind == server::WearKind::Shield && otherDef->class_small == "sword2h")) {
                    conflict = true;
                }
                if (server::wear_kind_of(*otherDef) == kind) {
                    if (!comparable_gear(other)) {
                        conflict = true;
                    }
                    oldScore = score(other);
                    oldId = otherId;
                }
            }
            const double gain = score(item) - oldScore;
            if (!conflict && gain > bestGain + 0.01) {
                candidate = id;
                wornId = oldId;
                bestGain = gain;
            }
        }
        if (!candidate) {
            return false;
        }
        stop(send);
        m_state = "Equipping an upgrade";
        m_equipCandidate = candidate;
        m_equipPrevious = wornId;
        m_equipmentStage = wornId ? EquipmentStage::Removing : EquipmentStage::PuttingOn;
        m_equipmentDeadline = m_now + 3000;
        if (wornId) {
            m_avoid["equip:" + std::to_string(wornId)] = m_now + 10000;
            (void) send_action(protocol::ItemWear::frame_client_put_off(wornId), send);
        } else {
            m_avoid["equip:" + std::to_string(candidate)] = m_now + 10000;
            (void) send_action(protocol::ItemWear::frame_client_put_on(candidate), send);
        }
        return true;
    }
    bool sell(const Send& send) {
        std::uint32_t excess = 0;
        std::size_t generalCount = 0;
        for (const auto& [id, item] : m_items) {
            const auto* def = definition(item);
            if (!def || (def->class_big != "quest" && !def->is_task)) {
                ++generalCount;
            }
            if (!protected_item(item) && m_avoid["sell:" + std::to_string(id)] <= m_now) {
                excess = id;
            }
        }
        if (generalCount >= 60 && !m_selling) {
            m_selling = true;
            m_saleCityRequested = false;
        }
        if (!excess) {
            m_selling = false;
            return false;
        }
        if (!m_selling) {
            return false;
        }
        if (m_cityTravel && !m_saleCityRequested) {
            if (m_pendingMove) {
                m_state = "Waiting to teleport to Narootuh for selling";
                return true;
            }
            m_saleCityRequested = true;
            return request_city(0, send);
        }
        const Npc* chosen = nullptr;
        double best = std::numeric_limits<double>::infinity();
        for (const auto& [index, npc] : m_data->m_catalogs->m_npcs) {
            const auto* shopDef = m_data->m_catalogs->m_shops.find(index);
            if (!shopDef || shopDef->currency_item != 0 || npc.map != m_character.map ||
                (m_cityTravel && (npc.map != map::kTeleportLocs[0].map ||
                                  distance(npc.position,
                                           {map::kTeleportLocs[0].x,
                                            map::kTeleportLocs[0].y,
                                            map::kTeleportLocs[0].z}) > 4096)) ||
                m_avoid["shop:" + std::to_string(index)] > m_now ||
                route_failed("shop:" + std::to_string(index), npc.position)) {
                continue;
            }
            if (distance(m_pose, npc.position) < best) {
                best = distance(m_pose, npc.position);
                chosen = &npc;
            }
        }
        if (!chosen) {
            if (m_cityTravel) {
                m_enabled = false;
                m_state = "Paused: no reachable configured ordinary merchant in Narootuh";
                return true;
            }
            m_state = "Inventory filling; no reachable known merchant";
            return generalCount >= 70;
        }
        m_state = "Selling excess equipment";
        const auto key = "shop:" + std::to_string(chosen->index);
        std::uint32_t runtime = 0;
        Position goal = chosen->position;
        for (const auto& [id, npc] : m_npcs) {
            if (npc.index == chosen->index) {
                runtime = id;
                goal = {npc.x, npc.y, npc.z};
                break;
            }
        }
        constexpr int sightReach = coords::kSightTileRadius * map::kTileSize / 2;
        const bool discovering = !runtime && distance(m_pose, goal) > sightReach;
        const int reach = discovering                                  ? sightReach
                          : m_shopReplyApproaches.count(chosen->index) ? protocol::kNpcReplyRange
                                                                       : 40;
        if (!discovering && m_routeKey == key && m_search &&
            m_destinationReach > protocol::kNpcReplyRange) {
            m_search.reset();
        }
        if (walk(goal, reach, key, send, discovering ? 90000 : 5000) && runtime) {
            if (m_shop.npc_id != runtime) {
                if (m_shopRequestedNpc == runtime) {
                    m_state = "Waiting for Narootuh merchant reply";
                    if (m_now >= m_shopReplyDeadline) {
                        m_enabled = false;
                        m_state =
                            "Paused: merchant " + std::to_string(chosen->index) + " did not reply";
                    }
                } else if (send_action(protocol::Shop::frame_item_list_c2s(runtime), send)) {
                    m_shopRequestedNpc = runtime;
                    m_shopReplyDeadline = m_now + 5000;
                }
            } else if (m_shop.currency_index == 0) {
                m_avoid["sell:" + std::to_string(excess)] = m_now + 10000;
                (void) send_action(protocol::Shop::frame_sell_c2s(runtime, excess, 1), send);
            }
        } else if (route_failed(key, goal)) {
            if (runtime && m_shopReplyApproaches.insert(chosen->index).second) {
                m_failedRoutes.erase(key);
                m_state = "Trying merchant within server talk range";
            } else {
                return_to_city(goal, key, send);
            }
        }
        return true;
    }
    bool potion(bool health, const Send& send) {
        if (m_now < m_potionReady) {
            return false;
        }
        std::uint32_t chosen = 0;
        int amount = 0, cooldown = 1000;
        bool automatic = false;
        for (const auto& [id, item] : m_items) {
            const auto* def = definition(item);
            if (!def || !def->usable || item.count <= 0 || def->level_limit > m_character.level ||
                (def->class_limit >= 0 && def->class_limit != m_character.clazz)) {
                continue;
            }
            const int restore = health ? def->refresh_hp : def->refresh_mp;
            if (restore > amount) {
                amount = restore;
                chosen = id;
                cooldown = std::max(1000, def->cooltime_ms);
                automatic = health && def->auto_hp;
            }
        }
        if (!chosen) {
            return false;
        }
        if (send(protocol::UseItem::frame_c2s(chosen, automatic ? 75 : 0))) {
            m_potionReady = m_now + static_cast<std::uint64_t>(cooldown);
            m_state = health ? "Using health potion" : "Using mana potion";
            // Inventory and HP remain server-owned; no optimistic consumption.
            return true;
        }
        return false;
    }
    bool allocate_stats(const Send& send) {
        if (m_pendingStat) {
            if (m_pendingStat->primarySeen && m_pendingStat->pointsSeen) {
                m_pendingStat.reset();
            } else {
                m_state = "Waiting for stat-point confirmation";
                if (m_now >= m_pendingStat->deadline) {
                    m_enabled = false;
                    m_state = "Paused: stat-point allocation was not confirmed";
                }
                return true;
            }
        }
        if (m_primarySnapshot != 31 || !m_pointsSeen || m_character.pu_point <= 0 ||
            m_pendingMove || m_character.clazz < 0 || m_character.clazz > 3) {
            return false;
        }
        // Profiles divide spent PU, rather than equipment bonuses or innate class stats.
        constexpr std::array<std::array<int, 5>, 4> weights{{
            {{50, 40, 0, 0, 10}},
            {{0, 25, 25, 0, 50}},
            {{25, 25, 0, 0, 50}},
            {{25, 50, 0, 0, 25}},
        }};
        const auto& profile = weights[m_character.clazz];
        const auto base = server::combat::class_base_plus_bonus(m_character.clazz);
        const std::array<int, 5> baseline{base.str, base.hth, base.intl, base.wis, base.dex};
        const std::array<int, 5> current{
            m_character.str, m_character.hth, m_character.intl, m_character.wis, m_character.dex};
        int chosen = -1, cost = 0;
        std::int64_t finish = 0;
        for (int property = 0; property < 5; ++property) {
            if (profile[property] == 0 || current[property] < baseline[property]) {
                continue;
            }
            const int need =
                server::combat::get_need_point(m_character.clazz, property, current[property]);
            if (need <= 0) {
                continue;
            }
            const auto spent =
                server::combat::pending_need_points(m_character.clazz,
                                                    property,
                                                    baseline[property],
                                                    current[property] - baseline[property]);
            const std::int64_t next = static_cast<std::int64_t>(spent) + need;
            if (chosen < 0 || next * profile[chosen] < finish * profile[property]) {
                chosen = property;
                cost = need;
                finish = next;
            }
        }
        // Keep an unaffordable remainder for the next intended allocation.
        if (chosen < 0 || cost > m_character.pu_point) {
            return false;
        }
        stop(send);
        constexpr std::array<const char*, 5> names{"STR", "HTH", "INT", "WIS", "DEX"};
        m_state = "Allocating stat points: " + std::string(names[chosen]);
        m_pendingStat = PendingStat{chosen, current[chosen], m_now + 5000};
        if (!send_action(protocol::Property::frame_c2s_update(static_cast<std::uint8_t>(chosen)),
                         send,
                         100)) {
            m_pendingStat.reset();
        }
        return true;
    }
    bool threatened() const {
        const auto target = m_monsters.find(m_target);
        return (target != m_monsters.end() && target->second.hp > 0) ||
               std::any_of(m_attackers.begin(), m_attackers.end(), [&](const auto& entry) {
                   const auto attacker = m_monsters.find(entry.first);
                   return attacker == m_monsters.end() || attacker->second.hp > 0;
               });
    }
    bool behead(const Send& send) {
        const auto learned = m_skills.find(1);
        const auto* skill = m_data->m_catalogs->m_skills.find(m_character.clazz, 1);
        if (!skill || learned == m_skills.end() || learned->second <= 0 ||
            m_skillReady[1] > m_now ||
            client::Catalog::skill_spend_mp(
                m_character.clazz, skill, learned->second, m_character.level) > m_character.mp) {
            return false;
        }
        int range = 0, speed = 1500;
        for (const auto& [id, item] : m_items) {
            (void) id;
            const auto* def = definition(item);
            if (def && def->is_weapon && protocol::insert_put_on(item.info)) {
                range = protocol::kBeheadRange;
                speed = std::max(300, def->attack_speed_ms);
            }
        }
        if (range == 0) {
            return false;
        }
        std::uint32_t chosen = 0;
        double best = 500;
        for (const auto& [id, monster] : m_monsters) {
            const auto key = "behead:" + std::to_string(id);
            if (monster.kneeling && protocol::MonsterRaceInfo::is_normal_combat(monster.race) &&
                !m_beheadAttempts.count(id) && m_avoid[key] <= m_now &&
                !route_failed(key, monster.position) && distance(m_pose, monster.position) < best) {
                chosen = id;
                best = distance(m_pose, monster.position);
            }
        }
        if (!chosen) {
            return false;
        }
        const auto& corpse = m_monsters.at(chosen);
        m_state = "Beheading monster " + std::to_string(chosen);
        if (walk(corpse.position, range, "behead:" + std::to_string(chosen), send, 5000)) {
            m_pendingSkill = 1;
            m_pendingTarget = chosen;
            m_pendingCooldown =
                std::max({500,
                          client::Catalog::ui_cooldown_ms(*skill, speed, learned->second),
                          skill->delay_ms()});
            m_castAt = m_now +
                       static_cast<std::uint64_t>(std::max(
                           0, client::Catalog::preskill_ms(*skill, learned->second, m_skills[14])));
            (void) send_action(protocol::Skill::frame_preskill(1, chosen), send, 0);
        }
        return true;
    }
    void hunt(const Send& send) {
        const bool armed = std::any_of(m_items.begin(), m_items.end(), [&](const auto& entry) {
            const auto* def = definition(entry.second);
            return def && def->is_weapon && protocol::insert_put_on(entry.second.info);
        });
        if (!armed) {
            m_target = 0;
            stop(send);
            m_state = "Waiting: no weapon equipped";
            return;
        }
        const int wanted = desired_monster();
        int minimumLevel = std::max(1, m_character.level - 3);
        int fallbackLevel = 0;
        bool hasLevelRange = false;
        std::map<int, bool> reachableMaps{{m_character.map, true}};
        for (const auto& spawn : m_data->m_catalogs->m_catalog.spawns()) {
            const auto* def = m_data->m_catalogs->m_catalog.find_def(spawn.def_index);
            if (!def || spawn.count <= 0 || def->level > m_character.level + 1 ||
                !protocol::MonsterRaceInfo::is_normal_combat(def->race)) {
                continue;
            }
            auto [it, inserted] = reachableMaps.try_emplace(spawn.map_index, false);
            if (inserted) {
                it->second = portal_to(spawn.map_index) != nullptr;
            }
            if (it->second) {
                hasLevelRange |= def->level >= minimumLevel;
                fallbackLevel = std::max(fallbackLevel, def->level);
            }
        }
        for (const auto& [id, monster] : m_monsters) {
            (void) id;
            const auto* def = m_data->m_catalogs->m_catalog.find_def(monster.index);
            if (def && monster.hp > 0 && def->level <= m_character.level + 1 &&
                protocol::MonsterRaceInfo::is_normal_combat(monster.race)) {
                hasLevelRange |= def->level >= minimumLevel;
                fallbackLevel = std::max(fallbackLevel, def->level);
            }
        }
        if (!hasLevelRange && fallbackLevel > 0) {
            minimumLevel = fallbackLevel;
        }
        const auto attacker = [&](std::uint32_t id) { return m_attackers.count(id) != 0; };
        const auto current = m_monsters.find(m_target);
        const auto* targetDef = current == m_monsters.end()
                                    ? nullptr
                                    : m_data->m_catalogs->m_catalog.find_def(current->second.index);
        const bool obsolete = targetDef && m_targetSince == 0 && !attacker(m_target) &&
                              current->second.index != wanted && targetDef->level < minimumLevel;
        if (m_target && (!m_monsters.count(m_target) || m_monsters.at(m_target).hp <= 0 ||
                         obsolete || m_avoid["monster:" + std::to_string(m_target)] > m_now ||
                         route_failed("monster:" + std::to_string(m_target),
                                      m_monsters.at(m_target).position))) {
            m_target = 0;
        }
        if (!m_target) {
            using Priority = std::tuple<int, int, int, double>;
            Priority best{2, 2, std::numeric_limits<int>::max(), 0};
            std::vector<std::uint32_t> candidates;
            for (const auto& [id, monster] : m_monsters) {
                const auto* def = m_data->m_catalogs->m_catalog.find_def(monster.index);
                if (!def || monster.hp <= 0 ||
                    !protocol::MonsterRaceInfo::is_normal_combat(monster.race) ||
                    (!attacker(id) && (def->level > m_character.level + 1 ||
                                       (monster.index != wanted && def->level < minimumLevel))) ||
                    m_avoid["monster:" + std::to_string(id)] > m_now ||
                    route_failed("monster:" + std::to_string(id), monster.position) ||
                    (wanted && monster.index != wanted && m_now - m_lastDamage >= 3000)) {
                    continue;
                }
                const Priority priority{attacker(id) ? 0 : 1,
                                        wanted && wanted != monster.index ? 1 : 0,
                                        std::abs(def->level - m_character.level),
                                        attacker(id) ? distance(m_pose, monster.position) : 0};
                if (priority < best) {
                    best = priority;
                    candidates.clear();
                }
                if (priority == best) {
                    candidates.push_back(id);
                }
            }
            if (!candidates.empty()) {
                // Each character owns its draw; keep the selected entity until invalidated.
                const auto index =
                    std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(m_random);
                m_target = candidates[index];
                m_targetSince = 0;
                m_targetHp = m_monsters.at(m_target).hp;
            }
        }
        if (m_target) {
            const auto monster = m_monsters.at(m_target);
            const auto key = "monster:" + std::to_string(m_target);
            int range = 16, speed = 1500;
            for (const auto& [id, item] : m_items) {
                (void) id;
                const auto* def = definition(item);
                if (def && def->is_weapon && protocol::insert_put_on(item.info)) {
                    range = std::max(8, def->range);
                    speed = std::max(300, def->attack_speed_ms);
                }
            }
            const client::SkillDef* cast = nullptr;
            int rank = 0;
            for (const auto& [index, learnedRank] : m_skills) {
                const auto* skill = m_data->m_catalogs->m_skills.find(m_character.clazz, index);
                if (!skill || learnedRank <= 0 || skill->rage() > 0 ||
                    client::Catalog::cast_kind_for(m_character.clazz, index) !=
                        client::CastKind::Combat ||
                    client::Catalog::is_taunt_skill(m_character.clazz, index) ||
                    client::Catalog::skill_spend_mp(
                        m_character.clazz, skill, learnedRank, m_character.level) >
                        m_character.mp ||
                    m_skillReady[index] > m_now) {
                    continue;
                }
                cast = skill;
                rank = learnedRank;
                range = std::max(
                    8, client::Catalog::skill_cast_range(m_character.clazz, skill, rank, range));
                break;
            }
            m_state = wanted == monster.index ? "Collecting quest objectives"
                                              : "Hunting for experience and equipment";
            const int attackReach = std::max(4, range - 4);
            // Distant approaches use the route validator. Check the final attack segment only
            // once a hit could be attempted.
            const bool clearAttackLine =
                distance(m_pose, monster.position) > attackReach ||
                m_data->step(m_character.map, m_pose, monster.position.x, monster.position.y)
                    .has_value();
            if (!walk(monster.position, clearAttackLine ? attackReach : 4, key, send)) {
                return;
            }
            if (m_targetSince == 0 || monster.hp < m_targetHp) {
                m_targetSince = m_now;
                m_targetHp = monster.hp;
            } else if (m_now - m_targetSince > 15000) {
                m_avoid[key] = m_now + 30000;
                m_target = 0;
                m_state = "Target not taking damage; choosing another";
                return;
            }
            if (cast) {
                m_pendingSkill = cast->index();
                m_pendingTarget = m_target;
                m_pendingCooldown = std::max(
                    {500, client::Catalog::ui_cooldown_ms(*cast, speed, rank), cast->delay_ms()});
                m_castAt = m_now + static_cast<std::uint64_t>(std::max(
                                       0, client::Catalog::preskill_ms(*cast, rank, m_skills[14])));
                (void) send_action(protocol::Skill::frame_preskill(
                                       static_cast<std::uint8_t>(cast->index()), m_target),
                                   send,
                                   0);
            } else {
                (void) send_action(protocol::Attack::frame_client(protocol::kCkMonster, m_target),
                                   send,
                                   static_cast<std::uint64_t>(speed));
            }
            return;
        }
        // When no suitable target is visible, travel toward an authored spawn on this map.
        const server::MonsterSpawn* chosen = nullptr;
        double best = std::numeric_limits<double>::infinity();
        for (const auto& spawn : m_data->m_catalogs->m_catalog.spawns()) {
            const auto* def = m_data->m_catalogs->m_catalog.find_def(spawn.def_index);
            const auto key =
                "spawn:" + std::to_string(spawn.area) + ":" + std::to_string(spawn.def_index);
            if (!def || (spawn.map_index != m_character.map && !portal_to(spawn.map_index)) ||
                spawn.count <= 0 || def->level > m_character.level + 1 ||
                (def->index != wanted && def->level < minimumLevel) ||
                !protocol::MonsterRaceInfo::is_normal_combat(def->race) || m_avoid[key] > m_now ||
                (wanted && def->index != wanted)) {
                continue;
            }
            const Position entry{std::clamp(m_pose.x, spawn.x0, spawn.x1),
                                 std::clamp(m_pose.y, spawn.y0, spawn.y1),
                                 m_pose.z};
            auto& survey = m_spawnSurveys[key];
            if (survey.goals.empty()) {
                survey.goals.push_back(entry);
                // Cover large regions from the nearest edge instead of assuming their center
                // is accessible or populated. Keep these goals stable while walking.
                const int nx = std::clamp((spawn.x1 - spawn.x0) / 512 + 1, 1, 64);
                const int ny = std::clamp((spawn.y1 - spawn.y0) / 512 + 1, 1, 64);
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        survey.goals.push_back(
                            {spawn.x0 + static_cast<int>((x + .5) * (spawn.x1 - spawn.x0) / nx),
                             spawn.y0 + static_cast<int>((y + .5) * (spawn.y1 - spawn.y0) / ny),
                             m_pose.z});
                    }
                }
                std::stable_sort(
                    survey.goals.begin() + 1, survey.goals.end(), [&](Position a, Position b) {
                        return distance(entry, a) < distance(entry, b);
                    });
            }
            if (survey.availability.empty()) {
                survey.availability.resize(survey.goals.size(), ProbeAvailability::Unchecked);
            }
            while (survey.next < survey.goals.size()) {
                auto& goal = survey.goals[survey.next];
                if (survey.availability[survey.next] == ProbeAvailability::Unchecked &&
                    spawn.map_index == m_character.map) {
                    const auto standing = m_data->standing_height(spawn.map_index, goal);
                    if (standing) {
                        goal.z = *standing;
                    }
                    survey.availability[survey.next] =
                        standing && m_data->step(spawn.map_index, goal, goal.x, goal.y)
                            ? ProbeAvailability::Standable
                            : ProbeAvailability::Unavailable;
                }
                const auto probeKey = key + ":" + std::to_string(survey.next);
                if (survey.availability[survey.next] != ProbeAvailability::Unavailable &&
                    !route_failed(probeKey, goal)) {
                    break;
                }
                ++survey.next;
            }
            if (survey.next == survey.goals.size()) {
                m_avoid[key] = m_now + 15000;
                survey.next = 0;
                continue;
            }
            const double priority = distance(m_pose, survey.goals[survey.next]) +
                                    std::abs(def->level - m_character.level) * 100000.0 +
                                    (wanted && wanted != def->index ? 1000000 : 0) +
                                    (spawn.map_index != m_character.map ? 10000000 : 0);
            if (priority < best) {
                best = priority;
                chosen = &spawn;
            }
        }
        if (chosen) {
            if (chosen->map_index != m_character.map) {
                (void) travel_to_map(chosen->map_index, send);
                return;
            }
            m_state = wanted ? "Travelling to quest monsters" : "Travelling to hunting ground";
            const auto key =
                "spawn:" + std::to_string(chosen->area) + ":" + std::to_string(chosen->def_index);
            auto& survey = m_spawnSurveys.at(key);
            // The ordinary server sight window reveals monsters before reaching the probe.
            constexpr int sightReach = coords::kSightTileRadius * map::kTileSize / 2;
            const auto probeKey = key + ":" + std::to_string(survey.next);
            if (walk(survey.goals[survey.next], sightReach, probeKey, send, 5000)) {
                ++survey.next;
                if (survey.next == survey.goals.size()) {
                    survey.next = 0;
                    m_avoid[key] = m_now + 15000;
                }
            } else if (route_failed(probeKey, survey.goals[survey.next])) {
                travel_to_area(*chosen, survey.goals[survey.next], send);
            }
        } else {
            stop(send);
            m_state = m_lastRouteFailure.empty()
                          ? "Waiting: no suitable reachable quest or hunting target"
                          : "Waiting after " + m_lastRouteFailure;
        }
    }
};

std::shared_ptr<TestPlayerData> TestPlayerAgent::load_data(const std::string& resource_root,
                                                           const std::string& profile_override) {
    std::string error;
    const auto runtime =
        platform::RuntimeProfile::load_for_tool(resource_root, profile_override, &error);
    if (!runtime) {
        throw std::runtime_error(error);
    }
    // Use the packaged overlay even when a development profile names a loose config tree.
    const auto package = platform::ConfigArchive::select(resource_root, {}, &error);
    if (!package) {
        throw std::runtime_error("Autonomous play requires config.pak: " + error);
    }
    auto catalogs = std::make_shared<TestPlayerCatalogs>();
    const auto packagedAreas = platform::join_path(resource_root, "RuntimeData/areas");
    catalogs->m_areas =
        platform::is_directory(packagedAreas) ? packagedAreas : runtime->profile.server_map_dir();
    catalogs->m_visuals = runtime->profile.maps_dir();
    catalogs->m_assets = runtime->profile.data_root();
    const auto& config = package->configuration_root;
    const auto path = [&](const char* name) { return platform::join_path(config, name); };
    if (!catalogs->m_catalog.load_items_json_dir(path("items"))) {
        throw std::runtime_error("Autonomous play requires the matching item catalog.");
    }
    (void) catalogs->m_catalog.load_prefixes_json_dir(path("prefixes"));
    (void) catalogs->m_skills.load(path("skills"));
    (void) catalogs->m_catalog.load_monsters_json_dir(path("monsters"), 0, 0, 0);
    (void) catalogs->m_catalog.load_drop_tables(path("groups"), path("itemgroups"));
    (void) catalogs->m_maps.load(path("mapinfo.json"));
    (void) catalogs->m_portals.load_json(path("portals.json"));
    for (const auto& portal : catalogs->m_portals.portals()) {
        if (portal.field_trigger_enabled()) {
            catalogs->m_portalEdges[portal.from_map()].push_back(
                {&portal, "portal:" + portal.id()});
        }
    }
    (void) catalogs->m_quests.load_dir(path("quests"));
    (void) catalogs->m_quests.load_task_levels_dir(path("tasks"));
    (void) catalogs->m_tasks.load_dir(path("tasks"));
    (void) catalogs->m_dialogs.load_dir(path("dialogs"));
    (void) catalogs->m_shops.load_dir(path("server/shops"), catalogs->m_catalog.item_defs());
    for (const auto& doc : documents(path("quests"))) {
        const auto* step =
            catalogs->m_quests.find(static_cast<std::uint16_t>(doc.at("index").as_int()),
                                    static_cast<std::uint8_t>(doc.at("flag").as_int()));
        if (!step) {
            continue;
        }
        for (const auto& branch : step->cases) {
            for (const auto& condition : branch.conditions) {
                for (const auto& item : condition.items) {
                    catalogs->m_questRequiredItems.insert(item.first);
                }
            }
            for (const auto& action : branch.actions) {
                for (const auto& item : action.item_out) {
                    if (!item.empty()) {
                        catalogs->m_questRequiredItems.insert(item.front());
                    }
                }
            }
        }
    }
    for (const auto& doc : documents(path("tasks"))) {
        if (const int id = doc.at("key").as_int(); id > 0 && id <= 65535) {
            catalogs->m_taskIds.push_back(static_cast<std::uint16_t>(id));
        }
    }
    std::sort(catalogs->m_taskIds.begin(), catalogs->m_taskIds.end());
    server::NpcRegistry registry;
    (void) registry.load_dir(path("npcs"));
    std::uint32_t catalogId = 1;
    registry.spawn_for_map(catalogId, -1);
    for (const auto& source : registry.npcs()) {
        Npc npc;
        npc.index = source.index;
        npc.map = source.map;
        npc.html = source.html;
        npc.quest = source.quest_key;
        npc.position = {source.x, source.y, source.z};
        catalogs->m_npcs[npc.index] = npc;
    }
    catalogs->m_selectorIndex.emplace(catalogs->m_catalog.defs(),
                                      catalogs->m_catalog.spawns(),
                                      catalogs->m_catalog.drop_tables());
    auto data = std::make_shared<TestPlayerData>();
    data->m_catalogs = std::move(catalogs);
    return data;
}

TestPlayerAgent::TestPlayerAgent(const server::CharacterRecord& character)
    : m_impl(std::make_unique<Impl>(character)) {}
TestPlayerAgent::~TestPlayerAgent() = default;
void TestPlayerAgent::start(std::shared_ptr<TestPlayerData> data) {
    m_impl->m_portalRoutesReady = false;
    m_impl->m_data = data ? std::make_shared<TestPlayerData>() : nullptr;
    if (data) {
        m_impl->m_data->m_catalogs = data->m_catalogs;
        m_impl->m_data->m_questDiagnostics = data->m_questDiagnostics;
    }
    m_impl->m_questPreviews.clear();
    m_impl->m_questNpcIndex = 0;
    m_impl->m_pendingNpcReply.reset();
    m_impl->m_npcReplyApproaches.clear();
    m_impl->m_shopReplyApproaches.clear();
    m_impl->m_dialogReach = protocol::kNpcInteractionRange;
    m_impl->m_enabled = m_impl->m_data != nullptr;
    m_impl->m_state = m_impl->m_enabled ? "Starting autonomous play" : "Missing catalogs";
    m_impl->m_route.clear();
    m_impl->m_search.reset();
    m_impl->reset_movement_clock();
    m_impl->m_target = 0;
    m_impl->m_pendingCity.reset();
    m_impl->m_pendingArea.reset();
    m_impl->m_travelTeleportSeen = false;
    m_impl->m_areaArrivalUntil = 0;
    m_impl->m_failedRoutes.clear();
    m_impl->m_npcCityFallbacks.clear();
    m_impl->m_saleCityRequested = false;
    m_impl->m_shopRequestedNpc = 0;
    m_impl->m_spawnSurveys.clear();
    m_impl->m_lastRouteFailure.clear();
    m_impl->m_heightFailures = 0;
}
void TestPlayerAgent::pause() {
    m_impl->m_portalRoutesReady = false;
    m_impl->m_enabled = false;
    m_impl->m_route.clear();
    m_impl->m_search.reset();
    m_impl->reset_movement_clock();
    m_impl->m_pendingSkill = -1;
    m_impl->m_pendingNpcReply.reset();
    m_impl->m_pendingCity.reset();
    m_impl->m_pendingArea.reset();
    m_impl->m_areaArrivalUntil = 0;
    m_impl->m_travelTeleportSeen = false;
    m_impl->m_state = "Paused";
}
bool TestPlayerAgent::active() const {
    return m_impl->m_enabled;
}
std::optional<std::size_t> TestPlayerAgent::pending_city_travel() const {
    return m_impl->m_pendingCity;
}
std::optional<TestPlayerAreaTravel> TestPlayerAgent::pending_area_travel() const {
    return m_impl->m_pendingArea;
}
TestPlayerPosition TestPlayerAgent::position() const {
    return m_impl->m_pose;
}
const std::string& TestPlayerAgent::status() const {
    return m_impl->m_state;
}
int TestPlayerAgent::level() const {
    return m_impl->m_character.level;
}

void TestPlayerAgent::receive(std::uint8_t type, const std::vector<std::uint8_t>& payload) {
    auto& s = *m_impl;
    s.m_portalRoutesReady = false;
    using Op = protocol::S2C;
    const auto op = static_cast<Op>(type);
    if (op == Op::InsertItem || op == Op::TrashItem || op == Op::SaveQuest || op == Op::QuestInfo) {
        s.m_questPreviews.clear();
        s.m_questNpcIndex = 0;
    }
    const auto* bytes = payload.data();
    const auto size = payload.size();
    protocol::PacketReader reader(payload);
    if (op == Op::CreatePlayer) {
        protocol::CreatePlayerFields player;
        if (protocol::CreatePlayer::parse(bytes, size, player) && (player.class_byte & 0x80)) {
            if (!s.m_ready || s.m_self != player.id) {
                s.clear_boost();
            }
            s.m_questPreviews.clear();
            s.m_questNpcIndex = 0;
            s.m_self = player.id;
            s.m_gstate = player.gstate;
            s.m_mstate = 0;
            s.m_moveSpeedPercent = 0;
            s.m_movementBuffs.clear();
            s.m_primarySnapshot = 0;
            s.m_pointsSeen = false;
            s.m_character.clazz = player.class_byte & 0x7f;
            s.m_pose = {player.x, player.y, player.z};
            s.m_ready = true;
            s.m_adjustingHeight = false;
            s.m_heightFailures = 0;
            s.m_pendingMove.reset();
            s.reset_movement_clock();
            s.m_route.clear();
            s.m_search.reset();
            s.m_items.clear();
            s.m_monsters.clear();
            s.m_beheadAttempts.clear();
            s.m_attackers.clear();
            s.m_drops.clear();
            s.m_npcs.clear();
            s.m_html = 0;
            s.m_dialogNpc = 0;
            s.m_spawnSurveys.clear();
            s.m_shop = {};
            s.m_shopRequestedNpc = 0;
        }
    } else if (op == Op::MovePlayerOn || op == Op::MovePlayerEnd) {
        std::uint32_t id = 0;
        std::int8_t dx = 0, dy = 0, dz = 0;
        if (protocol::Move::parse_player(bytes, size, id, dx, dy, dz) && id == s.m_self) {
            s.m_pose = {s.m_pose.x + dx, s.m_pose.y + dy, s.m_pose.z + dz};
            s.m_moving = op == Op::MovePlayerOn;
            if (s.m_pendingMove && distance(s.m_pose, *s.m_pendingMove) < 1 &&
                s.m_pose.z == s.m_pendingMove->z) {
                if (!s.m_route.empty() && s.m_pose.x == s.m_route.front().x &&
                    s.m_pose.y == s.m_route.front().y && s.m_pose.z == s.m_route.front().z) {
                    s.m_route.pop_front();
                }
                s.m_walkFailures = 0;
                s.m_heightFailures = 0;
                s.m_lastProgress = s.m_now;
            } else if (s.m_pendingMove || dx != 0 || dy != 0 || dz != 0) {
                s.m_route.clear();
                s.m_search.reset();
                if (s.m_pendingMove) {
                    ++s.m_walkFailures;
                    if (s.m_adjustingHeight) {
                        ++s.m_heightFailures;
                    }
                }
            }
            s.m_adjustingHeight = false;
            s.m_pendingMove.reset();
        }
    } else if (op == Op::MoveBefore) {
        std::uint32_t id = 0;
        int x = 0, y = 0, z = 0;
        std::uint16_t dir = 0;
        if (protocol::Move::parse_before(bytes, size, id, x, y, z, dir) && id == s.m_self) {
            if (s.m_travelTeleportSeen) {
                if (s.m_pendingArea) {
                    const auto& goal = s.m_pendingArea->position;
                    if (s.m_pendingArea->map == s.m_character.map && goal.x == x && goal.y == y &&
                        goal.z == z) {
                        s.m_pendingArea.reset();
                        s.m_areaArrivalUntil = s.m_now + 1000;
                        s.m_travelTeleportSeen = false;
                    }
                } else {
                    s.m_pendingCity.reset();
                    s.m_travelTeleportSeen = false;
                }
            }
            if (!s.m_pendingMove && s.m_pose.x == x && s.m_pose.y == y && s.m_pose.z == z) {
                return;
            }
            s.m_pose = {x, y, z};
            if (s.m_pendingMove) {
                ++s.m_walkFailures;
                if (s.m_adjustingHeight) {
                    ++s.m_heightFailures;
                }
            }
            s.m_adjustingHeight = false;
            s.m_pendingMove.reset();
            s.reset_movement_clock();
            s.m_route.clear();
            s.m_search.reset();
            s.m_moving = false;
        }
    } else if (op == Op::Teleport) {
        std::uint8_t map = 0, cheat = 0;
        int x = 0, y = 0, z = 0;
        if (protocol::Teleport::parse_server(bytes, size, map, x, y, z, cheat)) {
            s.m_travelTeleportSeen =
                s.m_pendingCity.has_value() ||
                (s.m_pendingArea && s.m_pendingArea->map == map &&
                 s.m_pendingArea->position.x == x && s.m_pendingArea->position.y == y &&
                 s.m_pendingArea->position.z == z);
            s.clear_boost();
            s.m_character.map = map;
            s.m_adjustingHeight = false;
            s.m_heightFailures = 0;
            s.m_pose = {x, y, z};
            s.m_pendingMove.reset();
            s.reset_movement_clock();
            s.m_pendingSkill = -1;
            s.m_monsters.clear();
            s.m_beheadAttempts.clear();
            s.m_attackers.clear();
            s.m_npcs.clear();
            s.m_html = 0;
            s.m_dialogNpc = 0;
            s.m_spawnSurveys.clear();
            s.m_drops.clear();
            s.m_route.clear();
            s.m_search.reset();
            s.m_target = 0;
            s.m_moving = false;
            s.m_shop = {};
            s.m_shopRequestedNpc = 0;
        }
    } else if (op == Op::UpdateProperty) {
        protocol::UpdatePropertyFields value;
        if (!protocol::Property::parse_update_any(bytes, size, value)) {
            return;
        }
        const auto property = static_cast<protocol::PropertyType>(value.property);
        if (value.property <= static_cast<std::uint8_t>(protocol::PropertyType::Dex) &&
            value.kind == protocol::UpdatePropertyKind::Word) {
            const std::array<std::int32_t*, 5> primaries{&s.m_character.str,
                                                         &s.m_character.hth,
                                                         &s.m_character.intl,
                                                         &s.m_character.wis,
                                                         &s.m_character.dex};
            *primaries[value.property] = value.word_value;
            s.m_primarySnapshot |= 1u << value.property;
            if (s.m_pendingStat && s.m_pendingStat->property == value.property &&
                value.word_value == s.m_pendingStat->previous + 1) {
                s.m_pendingStat->primarySeen = true;
            }
        } else if (property == protocol::PropertyType::PuPoint) {
            s.m_character.pu_point = value.word_value;
            s.m_pointsSeen = true;
            if (s.m_pendingStat) {
                s.m_pendingStat->pointsSeen = true;
            }
        } else if (property == protocol::PropertyType::Hp ||
                   property == protocol::PropertyType::CurHp) {
            const int hp = value.kind == protocol::UpdatePropertyKind::Vital ? value.vital_cur
                                                                             : value.word_value;
            if (hp < s.m_character.hp) {
                s.m_lastDamage = s.m_now;
            }
            s.m_character.hp = hp;
            if (value.kind == protocol::UpdatePropertyKind::Vital) {
                s.m_character.max_hp = value.vital_max;
            }
        } else if (property == protocol::PropertyType::Mp ||
                   property == protocol::PropertyType::CurMp) {
            s.m_character.mp = value.kind == protocol::UpdatePropertyKind::Vital ? value.vital_cur
                                                                                 : value.word_value;
            if (value.kind == protocol::UpdatePropertyKind::Vital) {
                s.m_character.max_mp = value.vital_max;
            }
        } else if (value.kind == protocol::UpdatePropertyKind::Level) {
            s.m_character.level = value.level_value;
            s.m_questPreviews.clear();
            s.m_questNpcIndex = 0;
        }
    } else if (op == Op::UpdateMProperty) {
        std::uint32_t id = 0;
        std::uint8_t property = 0;
        std::int16_t value = 0;
        if (protocol::Property::parse_update_mproperty(bytes, size, id, property, value) &&
            id == s.m_self && property == protocol::kPropertyMSpeed) {
            s.m_moveSpeedPercent = value;
            s.m_movementCredit = 0;
            s.m_lastWalk = s.m_now;
        }
    } else if (op == Op::MState) {
        std::uint32_t id = 0;
        std::uint64_t state = 0;
        if (protocol::MState::parse(bytes, size, id, state) && id == s.m_self) {
            if (((s.m_mstate ^ state) & protocol::kCmsMoveStop) != 0) {
                s.reset_movement_clock();
            }
            s.m_mstate = state;
        }
    } else if (op == Op::MessageV || op == Op::Message) {
        if (size < 1) {
            return;
        }
        std::uint64_t state = 0;
        std::int32_t remaining = 0, skill = 0, item = 0;
        if (op == Op::MessageV && size >= 2 &&
            bytes[0] == static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp)) {
            s.m_character.specialty = bytes[1];
            s.m_questPreviews.clear();
            s.m_questNpcIndex = 0;
        } else if (bytes[0] == static_cast<std::uint8_t>(protocol::BStateMsg::AddBState) &&
                   protocol::BuffState::parse_add(
                       bytes + 1, size - 1, state, remaining, skill, item)) {
            if ((state & protocol::kCbsSkillHaste) != 0) {
                s.clear_boost();
            }
            const auto expires =
                remaining <= 0 ? 0 : s.m_now + static_cast<std::uint64_t>(remaining) * 1000;
            for (const auto bit : {protocol::kCbsSkillHaste,
                                   protocol::kCbsSkillEscape,
                                   protocol::kCbsSkillArcherSharpness,
                                   protocol::kCbsSkillHiding}) {
                if ((state & bit) != 0) {
                    s.m_movementBuffs[bit] = {expires, item, remaining > 0 && s.m_now == 0};
                }
            }
        } else if (bytes[0] == static_cast<std::uint8_t>(protocol::BStateMsg::SubBState) &&
                   protocol::BuffState::parse_sub(bytes + 1, size - 1, state)) {
            for (auto it = s.m_movementBuffs.begin(); it != s.m_movementBuffs.end();) {
                if ((it->first & state) != 0) {
                    it = s.m_movementBuffs.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else if (op == Op::InsertItem) {
        std::uint32_t id = 0, info = 0, num = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        if (reader.read_u32(id) && reader.read_u16(index) && reader.read_u8(prefix) &&
            reader.read_u32(info) && reader.read_u32(num) &&
            num <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
            auto& item = s.m_items[id];
            item.index = index;
            item.prefix = prefix;
            item.count = (info & protocol::kInsertReplace) ? static_cast<int>(num)
                                                           : item.count + static_cast<int>(num);
            item.info = info;
            std::array<std::uint8_t, protocol::kInsertCharmBytes> charm{};
            const auto count = std::min(reader.remaining(), charm.size());
            if (count >= 8 && reader.read_bytes(charm.data(), count)) {
                item.charm = protocol::unpack_insert_charm(charm.data(), count);
            }
            if (item.count <= 0) {
                s.m_items.erase(id);
            }
        }
    } else if (op == Op::PutOnItem || op == Op::PutOffItem) {
        std::uint32_t player = 0, id = 0;
        std::uint16_t index = 0;
        if (protocol::ItemWear::parse_server_item(bytes, size, player, id, index) &&
            player == s.m_self && s.m_items.count(id)) {
            auto& item = s.m_items.at(id);
            if (op == Op::PutOnItem) {
                item.info |= protocol::kInsertPutOn;
            } else {
                item.info &= ~protocol::kInsertPutOn;
            }
        }
    } else if (op == Op::TrashItem) {
        std::uint32_t id = 0;
        std::uint8_t reason = 0;
        if (protocol::TrashItem::parse_s2c(bytes, size, id, reason)) {
            s.m_items.erase(id);
        }
    } else if (op == Op::CreateNpc) {
        protocol::CreateNpcInfo npc;
        if (protocol::Quest::parse_create_npc(bytes, size, npc)) {
            s.m_npcs[npc.id] = npc;
        }
    } else if (op == Op::RemoveNpc) {
        std::uint32_t id = 0;
        if (protocol::Quest::parse_remove_npc(bytes, size, id)) {
            s.m_npcs.erase(id);
        }
    } else if (op == Op::SendHtml) {
        std::uint32_t npc = 0;
        if (protocol::Quest::parse_send_html(bytes, size, s.m_html, &npc)) {
            if (npc != 0) {
                s.m_dialogNpc = npc;
            }
            if (s.m_pendingNpcReply && s.m_pendingNpcReply->entity == s.m_dialogNpc) {
                s.m_pendingNpcReply.reset();
            }
        }
    } else if (op == Op::Chatting && s.m_pendingNpcReply) {
        std::string speaker, message;
        if (protocol::Chatting::parse_s2c(bytes, size, speaker, message) && speaker == "Server" &&
            (message.rfind("Quest reward unavailable: item ", 0) == 0 ||
             message == "Make room in your inventory before continuing this quest.")) {
            const int npc = s.m_pendingNpcReply->index;
            pause();
            s.m_state = "Paused: NPC " + std::to_string(npc) + ": " + message;
        }
    } else if (op == Op::SaveQuest) {
        protocol::QuestFlagEntry entry;
        if (protocol::Quest::parse_save_quest(bytes, size, entry)) {
            s.m_pendingNpcReply.reset();
            s.m_quests[entry.quest] = entry;
            s.m_avoid.clear();
            s.m_target = 0;
        }
    } else if (op == Op::QuestInfo) {
        std::vector<protocol::QuestFlagEntry> entries;
        if (protocol::Quest::parse_quest_info(bytes, size, entries)) {
            s.m_quests.clear();
            for (const auto& entry : entries) {
                s.m_quests[entry.quest] = entry;
            }
        }
    } else if (op == Op::ItemList) {
        if (protocol::Shop::parse_item_list_s2c(bytes, size, s.m_shop) &&
            s.m_shop.npc_id == s.m_shopRequestedNpc) {
            s.m_shopRequestedNpc = 0;
        }
    } else if (op == Op::SkillInfo) {
        std::vector<protocol::SkillRankEntry> ranks;
        if (protocol::Skill::parse_skill_info(bytes, size, ranks)) {
            s.m_skills.clear();
            for (const auto& rank : ranks) {
                s.m_skills[rank.index] = rank.level;
            }
        }
    } else if (op == Op::SkillUp) {
        std::uint8_t index = 0, rank = 0;
        if (protocol::Skill::parse_skill_up(bytes, size, index, rank)) {
            s.m_skills[index] = rank;
        }
    } else if (op == Op::CreateItem) {
        std::uint16_t index = 0;
        std::uint32_t id = 0, x = 0, y = 0, count = 0;
        if (reader.read_u16(index) && reader.read_u32(id) && reader.read_u32(x) &&
            reader.read_u32(y) && reader.read_u32(count)) {
            s.m_drops[id] = {index,
                             static_cast<int>(count),
                             0,
                             {static_cast<int>(x), static_cast<int>(y), s.m_pose.z}};
        }
    } else if (op == Op::RemoveItem || op == Op::RemoveMonster) {
        std::uint32_t id = 0;
        if (reader.read_u32(id)) {
            if (op == Op::RemoveItem) {
                s.m_drops.erase(id);
            } else {
                s.m_monsters.erase(id);
                s.m_beheadAttempts.erase(id);
                s.m_attackers.erase(id);
            }
        }
    } else if (op == Op::CreateMonster) {
        Entity monster;
        std::uint16_t index = 0, dir = 0;
        std::uint32_t id = 0, x = 0, y = 0, gstate = 0, mstate = 0, gid = 0;
        std::int32_t hp = 0, maxHp = 0;
        std::uint64_t guildId = 0;
        std::uint8_t race = 0;
        std::string guild, name;
        if (reader.read_u16(index) && reader.read_u32(id) && reader.read_u32(x) &&
            reader.read_u32(y) && reader.read_u16(dir) && reader.read_i32(hp) &&
            reader.read_i32(maxHp) && reader.read_u32(gstate) && reader.read_u32(mstate) &&
            reader.read_u64(guildId) && reader.read_string(guild) && reader.read_u8(race) &&
            reader.read_u32(gid) && reader.read_string(name)) {
            const bool kneeling = (gstate & protocol::kCgsKnee) != 0;
            monster = {index,
                       kneeling ? 0 : hp,
                       race,
                       {static_cast<int>(x), static_cast<int>(y), s.m_pose.z},
                       kneeling};
            s.m_monsters[id] = monster;
        }
    } else if (op == Op::MoveMonsterOn || op == Op::MoveMonsterEnd) {
        std::uint32_t id = 0;
        std::uint8_t dx = 0, dy = 0, run = 0;
        if (reader.read_u32(id) && reader.read_u8(dx) && reader.read_u8(dy) &&
            reader.read_u8(run) && s.m_monsters.count(id)) {
            auto& monster = s.m_monsters.at(id);
            monster.position.x += static_cast<std::int8_t>(dx);
            monster.position.y += static_cast<std::int8_t>(dy);
        }
    } else if (op == Op::Attack || op == Op::Skill) {
        std::uint32_t attacker = 0, target = 0;
        std::int32_t damage = 0, extra = 0;
        std::uint8_t flags = 0;
        if (op == Op::Attack) {
            if (!protocol::Attack::parse_server(
                    bytes, size, attacker, target, damage, extra, flags)) {
                return;
            }
        } else {
            protocol::S2CSkillFields skill;
            if (!protocol::Skill::parse_server(bytes, size, skill)) {
                return;
            }
            if (skill.skill == 0 && skill.caster == s.m_self && s.m_boostPending) {
                s.m_boosting = true;
                s.m_boostPending = false;
                s.reset_movement_clock();
            }
            if (!skill.has_damage) {
                return;
            }
            attacker = skill.caster;
            target = skill.target;
            damage = skill.damage;
        }
        if (target == s.m_self && damage > 0) {
            s.m_lastDamage = s.m_now;
            s.m_attackers[attacker] = s.m_now;
        }
        if (s.m_monsters.count(target) && damage > 0) {
            s.m_monsters.at(target).hp =
                std::max(0, s.m_monsters.at(target).hp - damage - std::max(0, extra));
        }
    } else if (op == Op::GState) {
        std::uint32_t id = 0, state = 0;
        if (reader.read_u32(id) && reader.read_u32(state)) {
            if (id == s.m_self) {
                if (s.m_gstate != state) {
                    s.m_questPreviews.clear();
                    s.m_questNpcIndex = 0;
                }
                s.m_gstate = state;
                if (state & protocol::kCgsKnee) {
                    s.m_character.hp = 0;
                }
            }
            if (s.m_monsters.count(id)) {
                auto& monster = s.m_monsters.at(id);
                monster.kneeling = (state & protocol::kCgsKnee) != 0;
                if (monster.kneeling) {
                    monster.hp = 0;
                }
            }
        }
    } else if (op == Op::Action) {
        std::uint32_t id = 0;
        std::uint8_t kind = 0, value = 0;
        if (protocol::Skill::parse_action(bytes, size, id, kind, value)) {
            if (id == s.m_self && kind == protocol::kAtRest) {
                s.m_resting = value != 0;
                if (s.m_resting) {
                    s.clear_boost();
                }
            }
            if (kind == protocol::kAtDie || kind == protocol::kAtKnee ||
                kind == protocol::kAtBeheaded) {
                if (id == s.m_self) {
                    s.m_character.hp = 0;
                }
                if (s.m_monsters.count(id)) {
                    auto& monster = s.m_monsters.at(id);
                    monster.hp = 0;
                    monster.kneeling = kind == protocol::kAtKnee;
                }
            }
        }
    }
    s.expire_dialog();
}

void TestPlayerAgent::tick(std::uint64_t now,
                           const Send& send,
                           std::chrono::milliseconds planning_budget,
                           const std::function<bool(std::size_t)>& city_travel,
                           const std::function<bool(const TestPlayerAreaTravel&)>& area_travel) {
    auto& s = *m_impl;
    s.m_now = now;
    s.m_portalRoutesReady = false;
    for (auto it = s.m_attackers.begin(); it != s.m_attackers.end();) {
        if (it->second == 0) {
            it->second = now;
            s.m_lastDamage = now;
        }
        if (now >= it->second && now - it->second >= 3000) {
            it = s.m_attackers.erase(it);
        } else {
            ++it;
        }
    }
    s.m_cityTravel = city_travel;
    s.m_areaTravel = area_travel;
    s.m_planningBudget =
        std::clamp(planning_budget, std::chrono::milliseconds{1}, std::chrono::milliseconds{8});
    for (auto& [bit, buff] : s.m_movementBuffs) {
        (void) bit;
        if (buff.awaitingFirstTick) {
            buff.expires += now;
            buff.awaitingFirstTick = false;
        }
    }
    for (auto it = s.m_failedRoutes.begin(); it != s.m_failedRoutes.end();) {
        if (!s.route_failed(it->first, it->second.goal)) {
            it = s.m_failedRoutes.erase(it);
        } else {
            ++it;
        }
    }
    if (s.m_failedRoutes.empty()) {
        s.m_lastRouteFailure.clear();
    }
    if (s.m_avoid.size() > 4096) {
        for (auto it = s.m_avoid.begin(); it != s.m_avoid.end();) {
            if (it->second <= now) {
                it = s.m_avoid.erase(it);
            } else {
                ++it;
            }
        }
    }
    if (!s.m_enabled || !s.m_data || !s.m_ready) {
        if (s.m_moving && !s.m_pendingMove) {
            s.stop(send);
        }
        return;
    }
    if (s.m_character.hp <= 0) {
        s.m_pendingCity.reset();
        s.m_pendingArea.reset();
        s.m_areaArrivalUntil = 0;
        s.m_travelTeleportSeen = false;
        s.clear_boost();
        s.m_pendingSkill = -1;
        s.stop(send);
        s.m_state = "Dead; requesting normal revival";
        if (now >= s.m_nextAction) {
            (void) s.send_action(protocol::Skill::frame_client_revival(), send, 5000);
        }
        return;
    }
    const int health =
        static_cast<int>(100LL * s.m_character.hp / std::max(1, s.m_character.max_hp));
    if (health < 60) {
        (void) s.potion(true, send);
    }
    if (health < 35) {
        s.m_recovering = true;
    }
    if (s.m_recovering && health >= 85) {
        s.m_recovering = false;
    }
    if (s.m_pendingCity || s.m_pendingArea) {
        const std::string kind = s.m_pendingArea ? "area" : "city";
        s.m_state = "Waiting for " + kind + " teleport confirmation";
        if (now - s.m_travelRequestedAt > 15000) {
            pause();
            s.m_state = "Paused: " + kind + " teleport confirmation timed out";
        }
        return;
    }
    if (now < s.m_areaArrivalUntil) {
        s.m_state = "Waiting for spawn-area sight";
        return;
    }
    if (s.m_recovering) {
        s.m_pendingSkill = -1;
        s.m_target = 0;
        s.m_state = "Survival: retreating and recovering";
        const Entity* nearest = nullptr;
        double nearestDistance = 180;
        for (const auto& [id, monster] : s.m_monsters) {
            (void) id;
            if (monster.hp > 0 && protocol::MonsterRaceInfo::is_normal_combat(monster.race) &&
                distance(s.m_pose, monster.position) < nearestDistance) {
                nearest = &monster;
                nearestDistance = distance(s.m_pose, monster.position);
            }
        }
        if (nearest) {
            Position safest = s.m_pose;
            double safety = nearestDistance;
            for (int dx = -64; dx <= 64; dx += 64) {
                for (int dy = -64; dy <= 64; dy += 64) {
                    Position candidate{s.m_pose.x + dx, s.m_pose.y + dy, s.m_pose.z};
                    double separation = std::numeric_limits<double>::infinity();
                    for (const auto& [id, monster] : s.m_monsters) {
                        (void) id;
                        if (monster.hp > 0 &&
                            protocol::MonsterRaceInfo::is_normal_combat(monster.race)) {
                            separation =
                                std::min(separation, distance(candidate, monster.position));
                        }
                    }
                    if (separation > safety) {
                        safety = separation;
                        safest = candidate;
                    }
                }
            }
            (void) s.walk(safest, 8, "retreat", send);
        } else {
            s.stop(send);
            if (!s.m_resting && now >= s.m_nextAction && now - s.m_lastDamage > 5000) {
                (void) s.send_action(protocol::Skill::frame_rest(1), send, 2000);
            }
        }
        return;
    }
    if (s.m_resting) {
        s.m_state = "Standing up";
        if (now >= s.m_nextAction) {
            (void) s.send_action(protocol::Skill::frame_rest(0), send);
        }
        return;
    }
    s.request_boost(send);
    if (!s.m_enabled) {
        return;
    }
    if (s.m_pendingSkill == 1 && s.threatened()) {
        s.m_pendingSkill = -1;
    }
    if (s.m_pendingSkill >= 0) {
        const auto target = s.m_monsters.find(s.m_pendingTarget);
        if (target == s.m_monsters.end() ||
            (s.m_pendingSkill == 1 ? !target->second.kneeling : target->second.hp <= 0)) {
            s.m_pendingSkill = -1;
            return;
        }
        if (now >= s.m_castAt) {
            const int skill = s.m_pendingSkill;
            s.m_pendingSkill = -1;
            if (s.send_action(protocol::Skill::frame_client_target(static_cast<std::uint8_t>(skill),
                                                                   protocol::kCkMonster,
                                                                   s.m_pendingTarget),
                              send,
                              static_cast<std::uint64_t>(s.m_pendingCooldown))) {
                s.m_skillReady[skill] = now + static_cast<std::uint64_t>(s.m_pendingCooldown);
                if (skill == 1) {
                    s.m_beheadAttempts.insert(s.m_pendingTarget);
                }
            }
        }
        return;
    }
    if (s.align_height(send)) {
        return;
    }
    if (now < s.m_nextAction) {
        return;
    }
    if (s.m_equipCandidate && s.equip(send)) {
        return;
    }
    if (s.m_character.max_mp > 0 && s.m_character.mp * 100LL < s.m_character.max_mp * 25LL) {
        (void) s.potion(false, send);
    }
    if (!s.threatened()) {
        if (s.m_pendingNpcReply && s.quest(send)) {
            return;
        }
        if (s.behead(send) || s.allocate_stats(send)) {
            return;
        }
        if (s.equip(send) || s.sell(send)) {
            return;
        }
        std::uint32_t chosen = 0;
        double best = 500;
        for (const auto& [id, drop] : s.m_drops) {
            if (s.m_avoid["drop:" + std::to_string(id)] <= now &&
                !s.route_failed("drop:" + std::to_string(id), drop.position) &&
                distance(s.m_pose, drop.position) < best) {
                best = distance(s.m_pose, drop.position);
                chosen = id;
            }
        }
        if (chosen) {
            const auto drop = s.m_drops.at(chosen);
            s.m_state = "Collecting loot";
            if (s.walk(drop.position, 16, "drop:" + std::to_string(chosen), send)) {
                protocol::PacketWriter packet;
                packet.write_u32(chosen);
                packet.write_u32(static_cast<std::uint32_t>(drop.position.x >> 5));
                packet.write_u32(static_cast<std::uint32_t>(drop.position.y >> 5));
                (void) s.send_action(
                    packet.frame(static_cast<std::uint8_t>(protocol::C2S::PickupItem)), send);
                s.m_avoid["drop:" + std::to_string(chosen)] = now + 10000;
            }
            return;
        }
        if (s.quest(send)) {
            return;
        }
    }
    s.hunt(send);
}
