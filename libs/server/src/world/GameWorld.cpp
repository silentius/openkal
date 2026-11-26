/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/GameWorld.h"

#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"
#include "openkal/map/Coords.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/map/MapStream.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/HarvestPlant.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/Combat.h"
#include "openkal/server/MonsterStrength.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/FindPath.h"
#include "openkal/server/world/WorldOccupancyIndex.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <map>
#include <optional>
#include <string_view>

namespace openkal::server {

    using protocol::MonsterRaceInfo;

    MonsterRewardCoverage calculate_monster_reward_coverage(const MonsterEntity& monster,
                                                            std::int64_t base_exp) {
        MonsterRewardCoverage result;
        if (monster.max_hp <= 0) {
            return result;
        }

        const std::int64_t max_hp = monster.max_hp;
        for (const auto& entry : monster.reward_damage_by_player) {
            if (entry.second <= 0 || result.credited_damage >= max_hp) {
                continue;
            }
            result.credited_damage += std::min(entry.second, max_hp - result.credited_damage);
        }
        if (base_exp > 0) {
            result.exp_pool = static_cast<std::int64_t>(static_cast<long double>(base_exp) *
                                                        result.credited_damage / max_hp);
        }
        if (result.credited_damage * 100 >= max_hp * 50) {
            result.loot_access = MonsterLootAccess::HostilityOwner;
        } else if (result.credited_damage * 100 >= max_hp * 21) {
            result.loot_access = MonsterLootAccess::ServerLocked;
        }
        return result;
    }

    namespace {

        /** True when expr is the jobsystem `weapon_speed` symbol. */
        [[nodiscard]] bool expr_is_weapon_speed(const platform::Json& expr) {
            return expr.is_string() && expr.as_string() == "weapon_speed";
        }

        /**
         * Parses compare[3] as a literal, `weapon_speed`, or `plus(weapon_speed, N)`.
         * @param expr Compare expression.
         * @param ms Literal milliseconds, or the addend when weapon_speed is set.
         * @param weapon_speed True when cooldown includes equipped aspeed.
         */
        void parse_compare_cooldown_expr(const platform::Json& expr, int& ms, bool& weapon_speed) {
            if (expr.is_number()) {
                ms = expr.as_int(0);
                return;
            }
            if (expr_is_weapon_speed(expr)) {
                weapon_speed = true;
                return;
            }
            if (!expr.is_array() || expr.array().empty()) {
                return;
            }
            const auto& arr = expr.array();
            if (arr[0].is_number()) {
                ms = arr[0].as_int(0);
                return;
            }
            if (!arr[0].is_string() || arr[0].as_string() != "plus" || arr.size() != 3) {
                return;
            }
            if (expr_is_weapon_speed(arr[1]) && arr[2].is_number()) {
                weapon_speed = true;
                ms = arr[2].as_int(0);
                return;
            }
            if (expr_is_weapon_speed(arr[2]) && arr[1].is_number()) {
                weapon_speed = true;
                ms = arr[1].as_int(0);
            }
        }

        [[nodiscard]] std::optional<std::int64_t>
        eval_skill_level_expression(const platform::Json& expression, int skill_level) {
            if (expression.is_number()) {
                return static_cast<std::int64_t>(expression.number());
            }
            if (expression.is_string()) {
                if (expression.as_string() == "skill_level") {
                    return std::max(1, skill_level);
                }
                return std::nullopt;
            }
            if (expression.is_array() && !expression.array().empty() &&
                expression.array().front().is_number()) {
                const auto index = std::min<std::size_t>(std::max(1, skill_level) - 1,
                                                         expression.array().size() - 1);
                return static_cast<std::int64_t>(expression.array()[index].number());
            }
            if (!expression.is_array() || expression.array().empty() ||
                !expression.array()[0].is_string()) {
                return std::nullopt;
            }
            const auto& terms = expression.array();
            const std::string op = terms[0].as_string();
            if (op == "plus") {
                std::int64_t value = 0;
                for (std::size_t i = 1; i < terms.size(); ++i) {
                    const auto term = eval_skill_level_expression(terms[i], skill_level);
                    if (!term) {
                        return std::nullopt;
                    }
                    value += *term;
                }
                return value;
            }
            if (op == "mul") {
                std::int64_t value = 1;
                for (std::size_t i = 1; i < terms.size(); ++i) {
                    const auto term = eval_skill_level_expression(terms[i], skill_level);
                    if (!term) {
                        return std::nullopt;
                    }
                    value *= *term;
                }
                return value;
            }
            if (terms.size() != 3) {
                return std::nullopt;
            }
            const auto lhs = eval_skill_level_expression(terms[1], skill_level);
            const auto rhs = eval_skill_level_expression(terms[2], skill_level);
            if (!lhs || !rhs) {
                return std::nullopt;
            }
            if (op == "minus") {
                return *lhs - *rhs;
            }
            if (op == "div" && *rhs != 0) {
                return *lhs / *rhs;
            }
            return std::nullopt;
        }

        int irand(int lo, int hi) {
            if (hi <= lo) {
                return lo;
            }
            return lo + (std::rand() % (hi - lo + 1));
        }

        /** True when a live monster occupies this GenMonster rectangle. */
        [[nodiscard]] bool lives_in_gen(const MonsterEntity& m, const MonsterSpawn& g) {
            return m.hp > 0 && m.map == g.map_index && m.roam_x0 == g.x0 && m.roam_y0 == g.y0 &&
                   m.roam_x1 == g.x1 && m.roam_y1 == g.y1;
        }

        /** Counts live occupants of one gen rectangle. */
        [[nodiscard]] int count_live_in_gen(const std::vector<MonsterEntity>& monsters,
                                            const MonsterSpawn& g) {
            int live = 0;
            for (const auto& m : monsters) {
                if (lives_in_gen(m, g)) {
                    ++live;
                }
            }
            return live;
        }

        /**
         * True when compatibility behavior would refill this gen.
         * Compared the singleton timer's runtime state (== 1000 always), so
         * every gen respawned every second. Use the per-gen cycle instead.
         */
        [[nodiscard]] bool gen_cycle_due(const MonsterSpawn& g) {
            return g.cycle_sec <= 1 || g.next_gen_left <= 0.0f;
        }

        /** Arms runtime state = now + cycle after a refill attempt (cycle > 1 s). */
        void arm_gen_cycle(MonsterSpawn& g) {
            if (g.cycle_sec > 1) {
                g.next_gen_left = static_cast<float>(g.cycle_sec);
            }
        }

        // KalOnline direction: atan2(-dx,-dy) scaled to [0,255] with 128 = π (see
        // client yaw).
        [[nodiscard]] int dir_from_delta(int dx, int dy) {
            if (dx == 0 && dy == 0) {
                return 0;
            }
            return static_cast<int>(std::atan2(static_cast<float>(-dx), static_cast<float>(-dy)) *
                                    128.0f / 3.14159265f) &
                   255;
        }

        void emit_move_on(MonsterEntity& m, std::vector<MonsterMoveEvent>& moves_out) {
            // Kal compatibility behavior: apply delta to pixel immediately (SetXY), client
            // walks to abs dest.
            const int mdx = m.target_x - m.x;
            const int mdy = m.target_y - m.y;
            MonsterMoveEvent ev;
            ev.id = m.id;
            ev.x = m.x;
            ev.y = m.y;
            ev.dx = static_cast<std::int8_t>(std::clamp(mdx, -127, 127));
            ev.dy = static_cast<std::int8_t>(std::clamp(mdy, -127, 127));
            ev.ended = false;
            ev.run = m.move_is_run;
            moves_out.push_back(ev);

            const float dist = std::sqrt(static_cast<float>(mdx * mdx + mdy * mdy));
            float spd = std::max(1.0f, m.move_is_run ? m.chase_speed : m.roam_speed);
            int move_pct = 0;
            if (m.slow_left > 0.0f && m.slow_percent < 0) {
                move_pct = m.slow_percent;
            }
            const int dark = m.skill_buffs.archer_move_speed_percent();
            if (dark < 0) {
                move_pct = (move_pct == 0) ? dark : std::min(move_pct, dark);
            }
            if (move_pct < 0) {
                // initial: tick = (100 - speedValue) * speedno / 100 → speed' = 100 / (100 - sv).
                const int den = std::max(1, 100 - move_pct);
                spd = std::max(1.0f, spd * 100.0f / static_cast<float>(den));
            }
            m.x = m.target_x;
            m.y = m.target_y;
            m.move_busy_left = dist / spd;
            m.moving = true;
            m.dirty_move = false;
            if (m.move_is_run) {
                // compatibility behavior chase: runtime state = runtime state — no swing
                // until this step's travel time elapses.
                m.attack_cooldown = std::max(m.attack_cooldown, m.move_busy_left);
            }
            if (mdx != 0 || mdy != 0) {
                m.dir = dir_from_delta(mdx, mdy);
            }
        }

        void emit_move_end(MonsterEntity& m, std::vector<MonsterMoveEvent>& moves_out) {
            // Logical coords already match server (snapped on MoveOn). End(0,0) stops the
            // client walk.
            MonsterMoveEvent ev;
            ev.id = m.id;
            ev.x = m.x;
            ev.y = m.y;
            ev.dx = 0;
            ev.dy = 0;
            ev.ended = true;
            ev.run = m.move_is_run;
            moves_out.push_back(ev);
            m.target_x = m.x;
            m.target_y = m.y;
            m.move_busy_left = 0.0f;
            m.dirty_stop = false;
            m.dirty_move = false;
        }

        /** g_pt[n] = {32, n}: 32px Chebyshev octant step toward (dx, dy). */
        void octant_step(int dx, int dy, int& out_dx, int& out_dy) {
            const int absdx = std::abs(dx);
            const int absdy = std::abs(dy);
            if (absdx == 0 && absdy == 0) {
                out_dx = 0;
                out_dy = 0;
                return;
            }
            if (absdx < absdy) {
                out_dx = 32 * absdx / absdy;
                out_dy = 32;
            } else {
                out_dx = 32;
                out_dy = 32 * absdy / absdx;
            }
            if (dx < 0) {
                out_dx = -out_dx;
            }
            if (dy < 0) {
                out_dy = -out_dy;
            }
        }

        [[nodiscard]] bool same_occupancy_cell(int ax, int ay, int bx, int by) {
            return (ax >> 3) == (bx >> 3) && (ay >> 3) == (by >> 3);
        }

        void clear_combat(MonsterEntity& m) {
            m.aggro = false;
            m.beacon_pursuit = false;
            m.aggro_player_id = 0;
            m.aggro_hostility = 0;
            m.hostility_by_player.clear();
            m.reward_damage_by_player.clear();
            m.ai = MonsterEntity::Ai::Roam;
            m.attack_swinging = false;
            m.pending_skill_slot = -1;
            m.skill_precast_left = 0.0f;
            m.path_waypoints.clear();
            m.idle_left = 0.0f;
            // SetTarget(NULL) does not reset the runtime state. Preserve a
            // committed movement segment so the client reaches the server's already
            // snapped endpoint before another waypoint can replace it.
            m.detect_cooldown = MonsterRaceInfo::can_roam(m.race)
                                    ? 1.5f + static_cast<float>(irand(0, 20)) * 0.1f
                                    : 0.0f;
        }

        void refresh_aggro_target(MonsterEntity& monster) {
            std::uint32_t best_id = 0;
            std::int64_t best_hostility = 0;
            const auto current = monster.hostility_by_player.find(monster.aggro_player_id);
            if (current != monster.hostility_by_player.end() && current->second > 0) {
                best_id = current->first;
                best_hostility = current->second;
            }
            for (const auto& entry : monster.hostility_by_player) {
                if (entry.second > best_hostility) {
                    best_id = entry.first;
                    best_hostility = entry.second;
                }
            }
            monster.aggro_player_id = best_id;
            monster.aggro_hostility = combat::clamp_nonneg_i32(best_hostility);
            monster.aggro = best_id != 0;
            if (!monster.aggro) {
                monster.ai = MonsterEntity::Ai::Roam;
            }
        }

        [[nodiscard]] bool
        can_record_hostility(const GameWorld& world, const MonsterEntity& monster, int px, int py) {
            if (!MonsterRaceInfo::can_pursue(monster.race) ||
                world.grid().has_attr_world_pixel(px, py, map::kAttrVillage)) {
                return false;
            }
            const long long dx = static_cast<long long>(px) - monster.x;
            const long long dy = static_cast<long long>(py) - monster.y;
            const long long sight = monster.beacon_pursuit ? std::max(monster.sight_range,
                                                                      protocol::kBeaconRadiusPixels)
                                                           : std::max(0, monster.sight_range);
            return dx * dx + dy * dy <= sight * sight;
        }

        void
        add_hostility(MonsterEntity& monster, std::uint32_t player_id, std::int32_t hostility) {
            if (player_id == 0 || hostility <= 0) {
                return;
            }
            monster.hostility_by_player[player_id] += hostility;
            refresh_aggro_target(monster);
        }

        [[nodiscard]] float monster_attack_period_sec(const MonsterDef* def,
                                                      const MonsterEntity& m) {
            const int ms = (def && def->attack_speed_ms > 0) ? def->attack_speed_ms : 2000;
            const int revised =
                combat::revised_attack_delay_ms(ms, m.skill_buffs.archer_attack_speed_percent());
            return static_cast<float>(revised) / 1000.0f;
        }

        void begin_melee_swing(MonsterEntity& m,
                               const MonsterDef* def,
                               const PlayerPose& prey,
                               std::vector<MonsterAttackEvent>& attacks_out) {
            if (m.skill_buffs.archer_destroy_sight()) {
                return;
            }
            m.ai = MonsterEntity::Ai::Attack;
            m.dir = dir_from_delta(prey.x - m.x, prey.y - m.y);
            const float period = monster_attack_period_sec(def, m);
            m.attack_cooldown = period;
            m.attack_swinging = true;
            MonsterAttackEvent ev;
            ev.attacker_id = m.id;
            ev.target_player_id = prey.id;
            ev.damage = irand(m.min_attack, m.max_attack);
            attacks_out.push_back(ev);
        }

        /** Parses one JSON stats.skill value into the canonical slot sequence. */
        void parse_monster_skill_slots(const platform::Json& value,
                                       std::vector<MonsterSkillSlot>& slots) {
            slots.clear();
            if (!value.is_array() || value.array().empty()) {
                return;
            }
            const auto append = [&slots](const platform::Json& row) {
                if (!row.is_array() || row.array().empty()) {
                    return;
                }
                const int index = row.array()[0].as_int(0);
                // Compatibility behavior indexes its table from zero: skill 0 is
                // compatibility behavior and is used by several boss definitions.
                if (index < 0 || index > 255) {
                    return;
                }
                MonsterSkillSlot slot;
                slot.index = index;
                if (row.array().size() >= 2) {
                    slot.level = std::max(1, row.array()[1].as_int(1));
                }
                slots.push_back(slot);
            };
            if (value.array()[0].is_array()) {
                for (const auto& row : value.array()) {
                    append(row);
                }
            } else {
                append(value);
            }
        }

        enum class MonsterSkillAi : std::uint8_t { None, Only, Periodic };

        /** Maps the generic skill-bearing monster subclasses. */
        [[nodiscard]] MonsterSkillAi monster_skill_ai(const MonsterDef& def) {
            // The Doggebi Lord uses ServerHost's dedicated ritual and boss-skill
            // controller. Its configured rows must not also enter this generic path.
            if (def.index == 213 || def.skills.empty()) {
                return MonsterSkillAi::None;
            }
            const auto race = protocol::MonsterRaceInfo::from_int(def.race);
            if (race == protocol::MonsterRace::Maguni) {
                if (def.ai == 3) {
                    return MonsterSkillAi::Only;
                }
                if (def.ai == 4) {
                    return MonsterSkillAi::Periodic;
                }
            }
            if (race == protocol::MonsterRace::NotMaguni && def.ai == 4) {
                return MonsterSkillAi::Only;
            }
            // Later monster data assigns the remaining special subclasses (tower,
            // invader, black bug, and event races) a concrete skill list. They share
            // the same prepare/fire protocol and damage dispatch.
            return def.ai >= 2 ? MonsterSkillAi::Periodic : MonsterSkillAi::None;
        }

        /** True when this AI subclass replaces basic swings with skills. */
        [[nodiscard]] bool skills_replace_basic_attack(const MonsterDef& def) {
            const auto race = protocol::MonsterRaceInfo::from_int(def.race);
            return !def.skills.empty() &&
                   ((race == protocol::MonsterRace::Maguni && (def.ai == 3 || def.ai == 4)) ||
                    (race == protocol::MonsterRace::NotMaguni && def.ai == 4));
        }

        /** Maps monster definition ai + race to runtime state (compatibility behavior). */
        [[nodiscard]] bool derive_aggressive(int ai, int race) {
            if (ai <= 1) {
                return false;
            }
            if (ai == 2) {
                return true;
            }
            if (ai == 3) {
                return race == static_cast<int>(protocol::MonsterRace::Maguni);
            }
            return true;
        }

        /** Applies level-scaled monster combat properties. */
        void apply_monster_stat_revise(MonsterDef& def,
                                       const combat::BaseStats& stats,
                                       std::int32_t hp_base) {
            combat::revise_monster_properties(def.level,
                                              stats,
                                              hp_base,
                                              def.min_attack,
                                              def.max_attack,
                                              def.hit,
                                              def.dodge,
                                              def.min_magic,
                                              def.max_magic,
                                              def.res_fire,
                                              def.res_ice,
                                              def.res_lightning,
                                              def.res_curse,
                                              def.res_palsy,
                                              def.max_hp);
        }

        /** Fills derived AI/sight defaults after parsing a MonsterDef. */
        void finalize_monster_def(MonsterDef& def) {
            if (def.attack_range <= 0) {
                def.attack_range = 20;
            }
            if (def.detect_range <= 0) {
                def.detect_range = 160;
            }
            if (def.sight_range <= 0) {
                def.sight_range = 240;
            }
            // AI_NONE (0) is a valid monster definition value, notably for static objects.
            def.aggressive = derive_aggressive(def.ai, def.race);
            if (def.attack_speed_ms <= 0) {
                def.attack_speed_ms = 2000;
            }
            if (def.roam_speed_ms <= 0) {
                def.roam_speed_ms = 1600;
            }
            if (def.chase_speed_ms <= 0) {
                def.chase_speed_ms = 800;
            }
        }

    }  // namespace

    bool GameWorld::load_one_cell(const std::string& maps_dir, int cell_x, int cell_y) {
        return map::MapStream::ensure_cell(m_grid, maps_dir, cell_x, cell_y);
    }

    bool GameWorld::load_maps_around(const std::string& maps_dir,
                                     int spawn_x,
                                     int spawn_y,
                                     int radius_cells) {
        m_maps_dir = maps_dir;
        int loaded =
            map::MapStream::ensure_around(m_grid, maps_dir, spawn_x, spawn_y, radius_cells);
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(spawn_x, spawn_y, tile_x, tile_y);
        int cell_x = 0;
        int cell_y = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_tx, local_ty);
        log::info("GameWorld: loaded " + std::to_string(loaded) + " .akarea cells around (" +
                  std::to_string(cell_x) + "," + std::to_string(cell_y) + ")");
        if (loaded <= 0) {
            const int fallback = map::MapStream::ensure_around(
                m_grid, maps_dir, coords::kNarootuhServerX, coords::kNarootuhServerY, radius_cells);
            log::warn("GameWorld: no .akarea at spawn; Narootuh fallback loaded=" +
                      std::to_string(fallback));
            loaded = fallback;
        }
        return loaded > 0;
    }

    bool GameWorld::ensure_maps_for_spawns(const std::string& maps_dir) {
        m_maps_dir = maps_dir;
        int loaded = 0;
        for (const auto& g : m_spawns) {
            for (int py = g.y0; py <= g.y1; py += map::kWorldSize / 2) {
                for (int px = g.x0; px <= g.x1; px += map::kWorldSize / 2) {
                    int tx = 0;
                    int ty = 0;
                    coords::pixel_to_tile(px, py, tx, ty);
                    int cx = 0;
                    int cy = 0;
                    int ltx = 0;
                    int lty = 0;
                    coords::world_tile_to_cell(tx, ty, cx, cy, ltx, lty);
                    if (load_one_cell(maps_dir, cx, cy)) {
                        ++loaded;
                    }
                    // Also neighbors for roam bleed.
                    (void) load_one_cell(maps_dir, cx - 1, cy);
                    (void) load_one_cell(maps_dir, cx + 1, cy);
                    (void) load_one_cell(maps_dir, cx, cy - 1);
                    (void) load_one_cell(maps_dir, cx, cy + 1);
                }
            }
            // Corner cells of rect
            for (int py : {g.y0, g.y1}) {
                for (int px : {g.x0, g.x1}) {
                    int tx = 0;
                    int ty = 0;
                    coords::pixel_to_tile(px, py, tx, ty);
                    int cx = 0;
                    int cy = 0;
                    int ltx = 0;
                    int lty = 0;
                    coords::world_tile_to_cell(tx, ty, cx, cy, ltx, lty);
                    if (load_one_cell(maps_dir, cx, cy)) {
                        ++loaded;
                    }
                }
            }
        }
        log::info("GameWorld: spawn-cover map cells touched=" + std::to_string(loaded) +
                  " total_cells=" + std::to_string(m_grid.cell_count()));
        return m_grid.cell_count() > 0;
    }

    bool GameWorld::load_map_catalog(const std::string& map_catalog_path,
                                     const std::string& maps_dir,
                                     int spawn_x,
                                     int spawn_y,
                                     int radius_cells) {
        m_maps_dir = maps_dir;
        map::MapCatalog catalog;
        if (!catalog.load(map_catalog_path)) {
            return false;
        }
        const auto& entries = catalog.entries();

        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(spawn_x, spawn_y, tile_x, tile_y);
        int spawn_cx = 0;
        int spawn_cy = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tile_x, tile_y, spawn_cx, spawn_cy, local_tx, local_ty);

        int loaded = 0;
        int skipped = 0;
        int unavailable = 0;
        for (const auto& e : entries) {
            if (e.kind() != 1) {
                // Outdoor field maps use kind 1 in map definition.
                ++skipped;
                continue;
            }
            if (std::abs(e.x() - spawn_cx) > radius_cells ||
                std::abs(e.y() - spawn_cy) > radius_cells) {
                ++skipped;
                continue;
            }
            if (!map::MapStream::cell_exists(maps_dir, e.x(), e.y())) {
                ++unavailable;
                continue;
            }
            if (load_one_cell(maps_dir, e.x(), e.y())) {
                ++loaded;
            }
        }
        log::info("GameWorld: map definition loaded=" + std::to_string(loaded) + " skipped=" +
                  std::to_string(skipped) + " cells=" + std::to_string(m_grid.cell_count()));
        if (unavailable > 0) {
            log::warn("GameWorld: map definition has " + std::to_string(unavailable) +
                      " nearby outdoor cells without server area-map navigation; those cells are "
                      "blocked");
        }
        return loaded > 0 || m_grid.cell_count() > 0;
    }

    void GameWorld::classify_task_items(const std::unordered_set<int>& indices) {
        for (auto& entry : m_item_defs) {
            entry.second.is_task =
                entry.second.class_big == "quest" && indices.count(entry.first) != 0;
        }
    }

    bool GameWorld::load_items_json_dir(const std::string& dir) {
        m_item_defs.clear();
        if (!platform::is_directory(dir)) {
            log::warn("GameWorld: items dir missing " + dir);
            return false;
        }

        int loaded = 0;
        for (const auto& rel : platform::list_files_recursive(dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            // Basename check — skip manifest at any depth.
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("GameWorld: bad item json " + path + ": " + err);
                continue;
            }

            ItemCombatDef def;
            def.index = root.at("index").as_int(0);
            if (def.index <= 0) {
                continue;
            }
            def.name = root.at("name").as_string();
            def.range = root.at("range").as_int(0);
            def.grade = root.at("level").as_int(0);
            def.level_limit = root.at("level").as_int(1);
            def.endurance = root.at("endurance").as_int(0);
            def.buy = root.at("buy").as_int(0);
            def.sell = root.at("sell").as_int(0);
            def.wearable = root.at("wear").as_int(0) != 0;
            def.stackable = root.at("stackable").as_int(0) != 0;
            def.pay = root.at("pay").as_int(0) != 0;
            def.usable = root.at("use").as_int(0) != 0;
            def.effect = root.at("effect").as_int(0);

            const platform::Json& cooltime = root.at("cooltime");
            if (cooltime.is_array() && !cooltime.array().empty()) {
                def.cooltime_ms = cooltime.array()[0].as_int(0);
            } else if (cooltime.is_number()) {
                def.cooltime_ms = cooltime.as_int(0);
            }

            auto ascii_lower = [](std::string s) {
                for (char& ch : s) {
                    if (ch >= 'A' && ch <= 'Z') {
                        ch = static_cast<char>(ch - 'A' + 'a');
                    }
                }
                return s;
            };
            const platform::Json& clazz = root.at("class");
            if (clazz.is_array() && !clazz.array().empty()) {
                const std::string big = ascii_lower(clazz.array()[0].as_string());
                def.class_big = big;
                def.is_weapon = big == "weapon";
                def.is_defense = big == "defense";
                def.is_transform = big == "transform";
                def.is_ornament = big == "ornament";
                def.is_pet = big == "pet" || big == "limitedpet";
                def.is_ride = big == "ride";
                if (clazz.array().size() >= 2) {
                    def.class_small = ascii_lower(clazz.array()[1].as_string());
                }
            } else if (clazz.is_string()) {
                const std::string big = ascii_lower(clazz.as_string());
                const auto sp = big.find(' ');
                def.class_big = sp == std::string::npos ? big : big.substr(0, sp);
                def.is_weapon = big.find("weapon") == 0;
                def.is_defense = big.find("defense") == 0;
                def.is_transform = big.find("transform") == 0;
                def.is_ornament = big.find("ornament") == 0;
                def.is_pet = def.class_big == "pet" || def.class_big == "limitedpet";
                def.is_ride = def.class_big == "ride";
                if (sp != std::string::npos && sp + 1 < big.size()) {
                    def.class_small = big.substr(sp + 1);
                }
            }

            if (def.class_big == "skin") {
                def.skin_slot = protocol::skin_slot_for_part(def.class_small);
                def.full_body_skin = def.class_small == "body";
                if (def.skin_slot == protocol::SkinSlot::None) {
                    def.wearable = false;
                }
            }

            {
                const platform::Json* extra = root.get("extra");
                if (extra && extra->is_object()) {
                    def.transform_race = extra->at("race").as_int(0);
                }
            }

            def.assassin_mask = root.at("assassin_mask").boolean() && def.wearable &&
                                def.is_defense && def.class_small == "mask";
            const platform::Json& limit = root.at("limit");
            if (limit.is_object()) {
                const std::string lc = limit.at("class").as_string();
                if (lc == "knight") {
                    def.class_limit = 0;
                } else if (lc == "mage" || lc == "magician") {
                    def.class_limit = 1;
                } else if (lc == "archer") {
                    def.class_limit = 2;
                } else if (lc == "thief") {
                    def.class_limit = 3;
                }
                // Optional nested level on limit overrides top-level when present.
                if (limit.get("level")) {
                    def.level_limit = limit.at("level").as_int(def.level_limit);
                }
            }

            def.skin_weapon_type = protocol::resolve_weapon_skin_type(
                root.at("skin").at("weapon_type").as_string(), def.class_limit);

            const platform::Json& specialty = root.at("specialty");
            if (specialty.is_object()) {
                const auto& coupon = specialty.at("honor_reward_points");
                if (def.usable && def.class_big == "general" && def.class_small == "etc" &&
                    coupon.is_number() && coupon.number() >= 1 &&
                    coupon.number() <= std::numeric_limits<int>::max() &&
                    coupon.number() == coupon.as_int(0)) {
                    def.honor_reward_points = coupon.as_int(0);
                }
                def.hit = specialty.at("hit").as_int(0);
                def.attack_speed_ms = specialty.at("aspeed").as_int(0);
                def.absorb = specialty.at("absorb").as_int(0);
                def.dodge = specialty.at("dodge").as_int(0);
                def.str = specialty.at("str").as_int(0);
                def.hth = specialty.at("hth").as_int(0);
                def.intl = specialty.at("int").as_int(0);
                def.wis = specialty.at("wis").as_int(0);
                def.dex = specialty.at("dex").as_int(0);
                def.hp = specialty.at("hp").as_int(0);
                def.mp = specialty.at("mp").as_int(0);
                def.res_fire = specialty.at("resistfire").as_int(0);
                def.res_ice = specialty.at("resistice").as_int(0);
                def.res_lightning = specialty.at("resistlitning").as_int(0);
                def.res_curse = specialty.at("resistcurse").as_int(0);
                def.res_palsy = specialty.at("resistpalsy").as_int(0);
                def.fatal_rate = specialty.at("fatal").as_int(specialty.at("eb").as_int(0));
                def.fatal_pt = specialty.at("fatalpt").as_int(0);
                const platform::Json& magic = specialty.at("magic");
                if (magic.is_object()) {
                    def.min_magic = magic.at("min").as_int(0);
                    def.max_magic = magic.at("max").as_int(0);
                } else if (magic.is_array() && magic.array().size() >= 2) {
                    def.min_magic = magic.array()[0].as_int(0);
                    def.max_magic = magic.array()[1].as_int(0);
                }
                const platform::Json& attack = specialty.at("attack");
                if (attack.is_object()) {
                    def.min_attack = attack.at("min").as_int(0);
                    def.max_attack = attack.at("max").as_int(0);
                } else if (attack.is_array() && attack.array().size() >= 2) {
                    def.min_attack = attack.array()[0].as_int(0);
                    def.max_attack = attack.array()[1].as_int(0);
                }
                const platform::Json& defense = specialty.at("defense");
                if (defense.is_array() && defense.array().size() >= 2) {
                    def.defense_melee = defense.array()[0].as_int(0);
                    def.defense_range = defense.array()[1].as_int(0);
                } else if (defense.is_number()) {
                    // item definition specialty (defense N) is a single value — pair loader never
                    // matched it.
                    def.defense_melee = defense.as_int(0);
                    def.defense_range = 0;
                }
                const platform::Json& changeprefix = specialty.at("changeprefix");
                if (changeprefix.is_array() && changeprefix.array().size() >= 5) {
                    def.has_changeprefix = true;
                    def.changeprefix_target = changeprefix.array()[0].as_string();
                    def.changeprefix_rate0 = changeprefix.array()[1].as_int(0);
                    def.changeprefix_prefix0 = changeprefix.array()[2].as_int(0);
                    def.changeprefix_rate1 = changeprefix.array()[3].as_int(0);
                    def.changeprefix_prefix1 = changeprefix.array()[4].as_int(0);
                }
                const platform::Json& repair = specialty.at("repair");
                if (repair.is_array() && repair.array().size() >= 2) {
                    def.has_repair = true;
                    def.repair_mode = repair.array()[0].as_int(0);
                    def.repair_amount = repair.array()[1].as_int(0);
                }
                def.charming = parse_charm_kind(specialty.at("charming").as_string());
                const platform::Json& bof = specialty.at("bof");
                if (bof.type() == platform::Json::Type::Bool) {
                    def.bof = bof.boolean();
                } else if (bof.is_number()) {
                    def.bof = bof.as_int(0) != 0;
                }
                const platform::Json& teleport = specialty.at("teleport");
                if (teleport.is_object()) {
                    def.teleport_loc = teleport.at("loc").as_int(-1);
                }
                const platform::Json& refresh = specialty.at("refresh");
                if (refresh.is_object()) {
                    def.auto_hp = refresh.at("auto_hp").boolean();
                    const std::string stat = ascii_lower(refresh.at("stat").as_string());
                    const int amount = refresh.at("amount").as_int(0);
                    if (stat == "hp") {
                        def.refresh_hp = amount;
                    } else if (stat == "mp") {
                        def.refresh_mp = amount;
                    }
                }
                const platform::Json& buff = specialty.at("buff");
                if (buff.is_array() && buff.array().size() >= 2) {
                    def.buff_type = buff.array()[0].as_int(-1);
                    def.buff_remain = buff.array()[1].as_int(0);
                    if (buff.array().size() >= 3) {
                        def.buff_amount = buff.array()[2].as_int(0);
                    }
                }
                const platform::Json& revival = specialty.at("revival");
                if (revival.is_number()) {
                    def.revival_extra_cut = revival.as_int(-1);
                }
            }

            if (def.max_attack < def.min_attack) {
                std::swap(def.min_attack, def.max_attack);
            }
            if (def.is_ornament) {
                def.endurance = 0;
            }
            m_item_defs[def.index] = def;
            ++loaded;
        }

        log::info("GameWorld: items json loaded " + std::to_string(loaded) +
                  " combat definitions from " + dir);
        return loaded > 0;
    }

    bool GameWorld::load_pets_json_dir(const std::string& dir) {
        m_pet_defs.clear();
        const std::string path = platform::join_path(dir, "catalog.json");
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::error("GameWorld: pet catalog missing " + path);
            return false;
        }
        platform::Json root;
        std::string error;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
            !root.is_object() || root.at("schema").as_string() != "openkal.pets/v1" ||
            !root.at("pets").is_array()) {
            log::error("GameWorld: invalid pet catalog " + path + ": " + error);
            return false;
        }
        for (const platform::Json& entry : root.at("pets").array()) {
            const platform::Json* duration = entry.get("durationSeconds");
            const platform::Json* heal = entry.get("healPerTwoSeconds");
            const platform::Json* auto_pickup = entry.get("autoPickup");
            if (!entry.is_object() || !duration || !heal || !auto_pickup || !heal->is_number() ||
                auto_pickup->type() != platform::Json::Type::Bool ||
                (!duration->is_null() && !duration->is_number())) {
                log::error("GameWorld: invalid pet catalog record in " + path);
                m_pet_defs.clear();
                return false;
            }
            PetDef pet;
            pet.item_index = entry.at("itemIndex").as_int(0);
            pet.duration_seconds = duration->is_null() ? 0 : duration->as_int(-1);
            pet.heal_per_two_seconds = heal->as_int(-1);
            pet.auto_pickup = auto_pickup->boolean();
            if (pet.item_index <= 0 || pet.duration_seconds < 0 || pet.heal_per_two_seconds < 0 ||
                !m_pet_defs.emplace(pet.item_index, pet).second) {
                log::error("GameWorld: invalid or duplicate pet item in " + path);
                m_pet_defs.clear();
                return false;
            }
        }
        if (m_pet_defs.empty()) {
            log::error("GameWorld: pet catalog has no records " + path);
            return false;
        }
        log::info("GameWorld: pet catalog loaded " + std::to_string(m_pet_defs.size()) +
                  " records from " + path);
        return true;
    }

    bool GameWorld::load_riding_json_dir(const std::string& dir) {
        m_riding_defs.clear();
        const std::string path = platform::join_path(dir, "catalog.json");
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::error("GameWorld: riding catalog missing " + path);
            return false;
        }
        platform::Json root;
        std::string error;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
            !root.is_object() || root.at("schema").as_string() != "openkal.riding/v1" ||
            !root.at("mounts").is_array()) {
            log::error("GameWorld: invalid riding catalog " + path + ": " + error);
            return false;
        }
        for (const platform::Json& entry : root.at("mounts").array()) {
            const platform::Json* duration = entry.get("durationSeconds");
            const platform::Json* speed = entry.get("movementSpeedPercent");
            if (!entry.is_object() || !duration || !speed || !speed->is_number() ||
                (!duration->is_null() && !duration->is_number())) {
                log::error("GameWorld: invalid riding catalog record in " + path);
                m_riding_defs.clear();
                return false;
            }
            RidingDef riding;
            riding.item_index = entry.at("itemIndex").as_int(0);
            riding.duration_seconds = duration->is_null() ? 0 : duration->as_int(-1);
            riding.movement_speed_percent = speed->as_int(-1);
            if (const platform::Json* kind = entry.get("kind")) {
                if (!kind->is_string() ||
                    (kind->as_string() != "common" && kind->as_string() != "battle")) {
                    log::error("GameWorld: invalid riding kind in " + path);
                    m_riding_defs.clear();
                    return false;
                }
                riding.kind = kind->as_string() == "battle" ? protocol::RidingKind::Battle
                                                            : protocol::RidingKind::Common;
            }
            if (const platform::Json* grade = entry.get("initialGrade")) {
                const int value = grade->as_int(-1);
                if (!grade->is_number() || value < 0 || value > 3 ||
                    riding.kind != protocol::RidingKind::Battle) {
                    log::error("GameWorld: invalid initial Battle Riding grade in " + path);
                    m_riding_defs.clear();
                    return false;
                }
                riding.initial_grade = static_cast<protocol::BattleRidingGrade>(value);
            }
            if (riding.item_index <= 0 || riding.duration_seconds < 0 ||
                riding.movement_speed_percent < 0 ||
                !m_riding_defs.emplace(riding.item_index, riding).second) {
                log::error("GameWorld: invalid or duplicate riding item in " + path);
                m_riding_defs.clear();
                return false;
            }
        }
        if (m_riding_defs.empty()) {
            log::error("GameWorld: riding catalog has no records " + path);
            return false;
        }
        log::info("GameWorld: riding catalog loaded " + std::to_string(m_riding_defs.size()) +
                  " records from " + path);
        return true;
    }

    void ItemCombatDef::apply_primaries(
        int& o_str, int& o_hth, int& o_intl, int& o_wis, int& o_dex) const {
        o_str += str;
        o_hth += hth;
        o_intl += intl;
        o_wis += wis;
        o_dex += dex;
    }

    void ItemCombatDef::apply_gear(int& o_hit,
                                   int& o_dodge,
                                   int& o_min_attack,
                                   int& o_max_attack,
                                   int& o_min_magic,
                                   int& o_max_magic,
                                   int& o_defense,
                                   int& o_defense_range,
                                   int& o_absorb,
                                   int& o_fatal_rate,
                                   int& o_fatal_pt,
                                   int& o_res_fire,
                                   int& o_res_ice,
                                   int& o_res_lightning,
                                   int& o_res_curse,
                                   int& o_res_palsy) const {
        o_hit += hit;
        o_dodge += dodge;
        o_min_attack += min_attack;
        o_max_attack += max_attack;
        o_min_magic += min_magic;
        o_max_magic += max_magic;
        o_defense += defense_melee;
        o_defense_range += defense_range;
        o_absorb += absorb;
        o_fatal_rate += fatal_rate;
        o_fatal_pt += fatal_pt;
        o_res_fire += res_fire;
        o_res_ice += res_ice;
        o_res_lightning += res_lightning;
        o_res_curse += res_curse;
        o_res_palsy += res_palsy;
    }

    WearKind wear_kind_of(const ItemCombatDef& def) {
        if (def.assassin_mask) {
            return WearKind::AssassinMask;
        }
        if (def.skin_slot != protocol::SkinSlot::None) {
            return static_cast<WearKind>(static_cast<int>(WearKind::SkinWeapon) +
                                         static_cast<int>(def.skin_slot));
        }
        const auto collection = collection_def(def.index);
        if (collection.kind != CollectionKind::None && !collection.material) {
            return WearKind::Collection;
        }
        if (def.is_weapon) {
            return WearKind::Weapon;
        }
        if (def.is_transform) {
            return WearKind::Transform;
        }
        if (def.is_pet) {
            return WearKind::Pet;
        }
        if (def.is_ride) {
            return WearKind::Ride;
        }
        if (def.is_defense) {
            if (def.class_small == "shield") {
                return WearKind::Shield;
            }
            if (def.class_small == "helmet" || def.class_small == "hat") {
                return WearKind::Helmet;
            }
            if (def.class_small == "lowerarmor" || def.class_small == "pants") {
                return WearKind::Pants;
            }
            if (def.class_small == "boots") {
                return WearKind::Boots;
            }
            if (def.class_small == "gauntlet" || def.class_small == "gloves") {
                return WearKind::Gauntlet;
            }
            if (def.class_small == "face" || def.class_small == "mask") {
                return WearKind::Face;
            }
            return WearKind::Upper;
        }
        if (def.is_ornament) {
            if (def.class_small == "ring") {
                return WearKind::Ring;
            }
            if (def.class_small == "necklace" || def.class_small == "resistnecklace") {
                return WearKind::Necklace;
            }
            return WearKind::Trinket;
        }
        return WearKind::None;
    }

    int persist_slot_of(WearKind kind) {
        if (kind >= WearKind::SkinWeapon && kind <= WearKind::SkinFace) {
            return 6;
        }
        switch (kind) {
        case WearKind::Weapon:
            return 1;
        case WearKind::Transform:
            return 3;
        case WearKind::Pet:
            return 4;
        case WearKind::Ride:
            return 5;
        case WearKind::AssassinMask:
            return kAssassinMaskSlot;
        case WearKind::None:
            return 0;
        default:
            return 2;
        }
    }

    bool GameWorld::load_prefixes_json_dir(const std::string& dir) {
        m_prefix_defs.clear();
        if (!platform::is_directory(dir)) {
            log::warn("GameWorld: prefixes dir missing " + dir);
            return false;
        }

        auto read_minmax = [](const platform::Json& node, int& min_v, int& max_v) {
            if (node.is_object()) {
                min_v = node.at("min").as_int(0);
                max_v = node.at("max").as_int(0);
            } else if (node.is_array() && node.array().size() >= 2) {
                min_v = node.array()[0].as_int(0);
                max_v = node.array()[1].as_int(0);
            } else if (node.is_number()) {
                min_v = node.as_int(0);
                max_v = min_v;
            }
        };

        int loaded = 0;
        for (const auto& rel : platform::list_files_recursive(dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("GameWorld: bad prefix json " + path + ": " + err);
                continue;
            }
            PrefixCombatDef def;
            def.index = root.at("index").as_int(0);
            if (def.index <= 0) {
                continue;
            }
            def.str = root.at("str").as_int(0);
            def.hth = root.at("hth").as_int(0);
            def.intl = root.at("int").as_int(0);
            def.wis = root.at("wis").as_int(0);
            def.dex = root.at("dex").as_int(0);
            def.hp = root.at("hp").as_int(0);
            def.mp = root.at("mp").as_int(0);
            def.hit = root.at("hit").as_int(0);
            def.defense = root.at("defense").as_int(0);
            def.absorb = root.at("absorb").as_int(0);
            def.res_fire = root.at("resistfire").as_int(0);
            def.res_ice = root.at("resistice").as_int(0);
            def.res_lightning = root.at("resistlitning").as_int(0);
            def.res_curse = root.at("resistcurse").as_int(0);
            def.res_palsy = root.at("resistpalsy").as_int(0);
            read_minmax(root.at("attack"), def.min_attack, def.max_attack);
            read_minmax(root.at("magic"), def.min_magic, def.max_magic);
            m_prefix_defs[def.index] = def;
            ++loaded;
        }
        log::info("GameWorld: prefixes json loaded " + std::to_string(loaded) + " from " + dir);
        return loaded > 0;
    }

    bool GameWorld::load_bof_json(const std::string& path) {
        if (!m_bof.load_file(path)) {
            log::warn("GameWorld: bof json load failed: " + path);
            return false;
        }
        log::info("GameWorld: bof json loaded from " + path);
        return true;
    }

    bool GameWorld::load_drop_tables(const std::string& groups_dir,
                                     const std::string& itemgroups_dir) {
        m_drop_tables.clear();
        const bool groups_ok = m_drop_tables.load_groups_dir(groups_dir);
        const bool itemgroups_ok = m_drop_tables.load_itemgroups_dir(itemgroups_dir);
        if (!groups_ok || !itemgroups_ok) {
            log::warn("GameWorld: drop tables incomplete (groups=" + std::to_string(groups_ok) +
                      " itemgroups=" + std::to_string(itemgroups_ok) + ")");
            return false;
        }
        log::info(
            "GameWorld: drop tables ready (groups=" + std::to_string(m_drop_tables.group_count()) +
            " itemgroups=" + std::to_string(m_drop_tables.itemgroup_count()) + ")");
        return true;
    }

    std::vector<DropOutcome>
    GameWorld::roll_monster_drops(int def_index, int corpse_x, int corpse_y) const {
        const MonsterDef* def = find_def(def_index);
        if (!def) {
            return {};
        }
        const auto groups =
            DropRoller::effective_monster_kill_item_groups(def->item_groups, def->race);
        if (groups.empty()) {
            return {};
        }
        DropRoller roller(m_drop_tables);
        return roller.roll_monster_drops(groups, corpse_x, corpse_y);
    }

    std::vector<DropOutcome> GameWorld::roll_monster_quest_drops(int def_index,
                                                                 int corpse_x,
                                                                 int corpse_y,
                                                                 std::uint16_t quest_index,
                                                                 std::uint8_t quest_flag) const {
        const MonsterDef* def = find_def(def_index);
        if (!def || def->quest_drops.empty()) {
            return {};
        }
        std::vector<std::pair<int, int>> groups;
        for (const MonsterQuestDrop& qd : def->quest_drops) {
            if (qd.quest_index == quest_index && qd.flag == quest_flag && qd.itemgroup > 0 &&
                qd.drop_num > 0) {
                groups.emplace_back(qd.itemgroup, qd.drop_num);
            }
        }
        if (groups.empty()) {
            return {};
        }
        DropRoller roller(m_drop_tables);
        return roller.roll_monster_drops(groups, corpse_x, corpse_y);
    }

    bool GameWorld::load_skills_json_dir(const std::string& dir) {
        m_skill_defs.clear();
        if (!platform::is_directory(dir)) {
            log::warn("GameWorld: skills dir missing " + dir);
            return false;
        }

        auto role_to_class = [](const std::string& role) -> int {
            if (role == "knight") {
                return 0;
            }
            if (role == "mage" || role == "magician") {
                return 1;
            }
            if (role == "archer") {
                return 2;
            }
            if (role == "thief") {
                return 3;
            }
            if (role == "transform") {
                return -1;
            }
            if (role == "monster") {
                return -2;
            }
            if (role == "common") {
                return -3;
            }
            if (role == "yungsang") {
                return 10;
            }
            if (role == "tuguk") {
                return 14;
            }
            if (role == "gunchi") {
                return 16;
            }
            if (role == "gale") {
                return 18;
            }
            return -100;
        };

        int loaded = 0;
        for (const auto& rel : platform::list_files_recursive(dir)) {
            if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto slash = rel.find_last_of('/');
            const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
            if (base == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, rel);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("GameWorld: bad skill json " + path + ": " + err);
                continue;
            }

            SkillDef def;
            const platform::Json& class_j = root.at("class");
            if (class_j.is_number()) {
                def.clazz = class_j.as_int(0);
            } else if (class_j.is_string()) {
                def.clazz = role_to_class(class_j.as_string());
            }
            const std::string role = root.at("role").as_string();
            if (!role.empty() && !class_j.is_number()) {
                def.clazz = role_to_class(role);
            }
            def.index = root.at("index").as_int(-1);
            if (def.index < 0) {
                continue;
            }
            def.max_level = root.at("maxLevel").as_int(1);
            def.mp = root.at("mp").as_int(0);
            def.rage = root.at("rage").as_int(0);
            def.damage_type = root.at("damageType").as_int(0);
            def.last_time = root.at("lastTime").as_int(0);
            def.value1 = root.at("value1").as_int(0);
            def.value2 = root.at("value2").as_int(0);
            {
                const platform::Json& property = root.at("property");
                if (property.is_array()) {
                    for (const auto& value : property.array()) {
                        def.property.push_back(value.as_int(0));
                    }
                }
            }
            {
                const platform::Json* extra = root.get("extra");
                if (extra && extra->is_object()) {
                    const platform::Json* target_kind = extra->get("targetkind");
                    if (target_kind && target_kind->is_array()) {
                        for (const auto& value : target_kind->array()) {
                            def.target_kinds.push_back(value.as_int(0));
                        }
                    } else if (target_kind && target_kind->is_number()) {
                        def.target_kinds.push_back(target_kind->as_int(0));
                    }
                }
            }
            def.level_limit = root.at("levelLimit").as_int(0);
            if (def.level_limit == 0) {
                const platform::Json& limit = root.at("limit");
                if (limit.is_array() && !limit.array().empty()) {
                    def.level_limit = limit.array()[0].as_int(0);
                } else if (limit.is_number()) {
                    def.level_limit = limit.as_int(0);
                }
            }
            const platform::Json& delay = root.at("delay");
            if (delay.is_array()) {
                if (!delay.array().empty()) {
                    def.delay_pre_ms = delay.array()[0].as_int(0);
                }
                if (delay.array().size() >= 2) {
                    def.delay_ms = delay.array()[1].as_int(0);
                }
            }
            {
                const platform::Json& type = root.at("type");
                auto type_has = [&](std::string_view token) {
                    if (type.is_string()) {
                        return type.as_string() == token;
                    }
                    if (!type.is_array()) {
                        return false;
                    }
                    for (const auto& e : type.array()) {
                        if (e.as_string() == token) {
                            return true;
                        }
                    }
                    return false;
                };
                def.usable = type_has("use");
                def.once = type_has("once");
            }
            {
                const platform::Json& manual = root.at("manualLearn");
                def.manual_learn = manual.type() == platform::Json::Type::Bool && manual.boolean();
            }
            {
                const platform::Json& compares = root.at("compares");
                if (compares.is_array()) {
                    for (const auto& entry : compares.array()) {
                        if (!entry.is_object()) {
                            continue;
                        }
                        const int compare_id = entry.at("id").as_int(0);
                        const platform::Json& expr = entry.at("expr");
                        if (compare_id == 2) {
                            for (int level = 1; level <= std::max(1, def.max_level); ++level) {
                                const auto range = eval_skill_level_expression(expr, level);
                                if (!range) {
                                    def.cast_range_by_level.clear();
                                    break;
                                }
                                def.cast_range_by_level.push_back(
                                    static_cast<int>(std::clamp<std::int64_t>(
                                        *range, 0, std::numeric_limits<int>::max())));
                            }
                        } else if (compare_id == 4) {
                            for (int level = 1; level <= std::max(1, def.max_level); ++level) {
                                const auto value = eval_skill_level_expression(expr, level);
                                if (!value) {
                                    def.cooldown_by_level.clear();
                                    break;
                                }
                                def.cooldown_by_level.push_back(static_cast<int>(
                                    std::clamp<std::int64_t>(*value, 0, INT32_MAX)));
                            }
                            int ms = 0;
                            bool weapon_speed = false;
                            parse_compare_cooldown_expr(expr, ms, weapon_speed);
                            if (ms > 0) {
                                def.compare_cooldown_ms = ms;
                            }
                            if (weapon_speed) {
                                def.compare_cooldown_weapon_speed = true;
                            }
                        } else if (compare_id == 5) {
                            if (expr.is_number()) {
                                def.rage = expr.as_int(def.rage);
                            }
                        }
                    }
                }
            }
            const platform::Json& grade = root.at("grade");
            if (grade.is_array() && !grade.array().empty()) {
                def.grade_mask = grade.array()[0].as_int(0);
            } else if (grade.is_number()) {
                def.grade_mask = grade.as_int(0);
            }
            {
                const platform::Json& limit = root.at("limit");
                if (limit.is_array()) {
                    if (limit.array().size() >= 3) {
                        def.require_skill_index = limit.array()[2].as_int(0);
                    }
                    if (limit.array().size() >= 4) {
                        def.require_skill_level = limit.array()[3].as_int(0);
                    }
                }
            }
            {
                const platform::Json& auto_learn = root.at("autoLearnLevels");
                if (auto_learn.is_array()) {
                    for (const auto& value : auto_learn.array()) {
                        def.auto_learn_levels.push_back(value.as_int(0));
                    }
                }
            }
            if ((def.index >= protocol::kTalentSkillFirst &&
                 def.index <= protocol::kTalentSkillLast) ||
                def.index == protocol::kBeaconSkill) {
                continue;
            }
            m_skill_defs.push_back(def);
            ++loaded;
        }

        log::info("GameWorld: skills json loaded " + std::to_string(loaded) + " definitions from " +
                  dir);
        return loaded > 0;
    }

    bool GameWorld::load_monsters_json_dir(const std::string& dir,
                                           int spawn_x,
                                           int spawn_y,
                                           int max_dist_px,
                                           const std::string& strength_path) {
        MonsterStrength strength;
        if (!strength_path.empty() && !strength.load_file(strength_path)) {
            return false;
        }
        m_defs.clear();
        m_def_positions.clear();
        m_spawns.clear();
        m_spawn_indices_by_map.clear();
        if (!platform::is_directory(dir)) {
            log::warn("GameWorld: monsters dir missing " + dir);
            return false;
        }

        int loaded_defs = 0;
        int loaded_spawns = 0;
        int parsed_spawns = 0;
        const auto entries = platform::list_directory(dir);
        for (const auto& name : entries) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            if (name == "bones.json" || name == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            const std::string text(bytes->begin(), bytes->end());
            platform::Json root;
            std::string err;
            if (!platform::json_parse(text, root, &err) || !root.is_object()) {
                log::warn("GameWorld: bad monster json " + path + ": " + err);
                continue;
            }

            MonsterDef def;
            def.index = root.at("index").as_int(0);
            if (def.index <= 0) {
                continue;
            }
            def.name = root.at("name").as_string("Monster" + std::to_string(def.index));

            const platform::Json& stats = root.at("stats");
            if (stats.is_object()) {
                def.race = stats.at("race").as_int(0);
                def.level = stats.at("level").as_int(1);
                def.ai = stats.at("ai").as_int(1);
                const auto adjusted = [&strength, &stats, &def](std::string_view name,
                                                                int fallback = 0) {
                    return strength.adjust(stats.at(name).as_int(fallback), def.level, name);
                };
                def.exp = stats.at("exp").as_int(0);
                def.hit = adjusted("hit", 0);
                def.dodge = adjusted("dodge", 0);
                def.absorb = adjusted("absorb", 0);
                def.attack_range = stats.at("range").as_int(20);
                def.attack_speed_ms = stats.at("aspeed").as_int(2000);

                combat::BaseStats primaries;
                primaries.str = adjusted("str", 0);
                primaries.hth = adjusted("hth", 0);
                primaries.intl = adjusted("int", 0);
                primaries.wis = adjusted("wis", 0);
                primaries.dex = adjusted("dex", 0);
                const int hp_base = adjusted("hp", 0);
                def.strength = primaries.str;
                def.intelligence = primaries.intl;

                const platform::Json& attack = stats.at("attack");
                if (attack.is_object()) {
                    def.attack_type = attack.at("type").as_int(0);
                    def.min_attack = attack.at("min").as_int(1);
                    def.max_attack = attack.at("max").as_int(1);
                } else if (attack.is_array() && attack.array().size() >= 3) {
                    def.attack_type = attack.array()[0].as_int(0);
                    def.min_attack = attack.array()[1].as_int(1);
                    def.max_attack = attack.array()[2].as_int(1);
                }

                const platform::Json& magic = stats.at("magic");
                if (magic.is_array() && magic.array().size() >= 2) {
                    def.min_magic = magic.array()[0].as_int(0);
                    def.max_magic = magic.array()[1].as_int(0);
                }

                const platform::Json& defense = stats.at("defense");
                if (defense.is_array() && defense.array().size() >= 2) {
                    def.defense_melee = defense.array()[0].as_int(0);
                    def.defense_range = defense.array()[1].as_int(0);
                }

                const platform::Json& resist = stats.at("resist");
                if (resist.is_array() && resist.array().size() >= 5) {
                    def.res_fire = resist.array()[0].as_int(0);
                    def.res_ice = resist.array()[1].as_int(0);
                    def.res_lightning = resist.array()[2].as_int(0);
                    def.res_curse = resist.array()[3].as_int(0);
                    def.res_palsy = resist.array()[4].as_int(0);
                }

                const platform::Json& sight = stats.at("sight");
                if (sight.is_array() && sight.array().size() >= 2) {
                    def.detect_range = sight.array()[0].as_int(160);
                    def.sight_range = sight.array()[1].as_int(240);
                }

                const platform::Json& mspeed = stats.at("mspeed");
                if (mspeed.is_array() && mspeed.array().size() >= 2) {
                    def.roam_speed_ms = mspeed.array()[0].as_int(1600);
                    def.chase_speed_ms = mspeed.array()[1].as_int(800);
                }

                parse_monster_skill_slots(stats.at("skill"), def.skills);

                const platform::Json& item_groups = stats.at("itemGroups");
                if (item_groups.is_array()) {
                    def.item_groups.clear();
                    for (const auto& row : item_groups.array()) {
                        if (!row.is_array() || row.array().size() < 2) {
                            continue;
                        }
                        const int gid = row.array()[0].as_int(0);
                        const int attempts = row.array()[1].as_int(0);
                        if (gid > 0 && attempts > 0) {
                            def.item_groups.emplace_back(gid, attempts);
                        }
                    }
                }

                const platform::Json& quests = stats.at("quests");
                if (quests.is_array()) {
                    def.quest_drops.clear();
                    const auto add_quest_drop = [&def](const platform::Json& row) {
                        if (!row.is_array() || row.array().size() < 4) {
                            return;
                        }
                        MonsterQuestDrop qd;
                        qd.quest_index =
                            static_cast<std::uint16_t>(std::max(0, row.array()[0].as_int(0)));
                        qd.flag =
                            static_cast<std::uint8_t>(std::clamp(row.array()[1].as_int(0), 0, 255));
                        qd.itemgroup = row.array()[2].as_int(0);
                        qd.drop_num = row.array()[3].as_int(0);
                        if (qd.quest_index > 0 && qd.itemgroup > 0 && qd.drop_num > 0) {
                            def.quest_drops.push_back(qd);
                        }
                    };
                    if (!quests.array().empty() && !quests.array().front().is_array()) {
                        add_quest_drop(quests);
                    } else {
                        for (const auto& row : quests.array()) {
                            add_quest_drop(row);
                        }
                    }
                }

                apply_monster_stat_revise(def, primaries, hp_base);
            }

            if (def.max_attack < def.min_attack) {
                std::swap(def.min_attack, def.max_attack);
            }
            finalize_monster_def(def);
            def.speed = 32000.0f / static_cast<float>(def.roam_speed_ms);
            m_defs.push_back(def);
            m_def_positions.try_emplace(def.index, m_defs.size() - 1);
            ++loaded_defs;

            const platform::Json& spawns = root.at("spawns");
            if (!spawns.is_array()) {
                continue;
            }
            for (const auto& s : spawns.array()) {
                if (!s.is_object()) {
                    continue;
                }
                MonsterSpawn g;
                g.def_index = def.index;
                g.map_index = s.at("map").as_int(0);
                g.area = s.at("area").as_int(0);
                g.count = s.at("max").as_int(0);
                g.cycle_sec = s.at("cycle").as_int(1);
                const platform::Json& rect = s.at("rect");
                if (!rect.is_array() || rect.array().size() < 4 || g.count <= 0) {
                    continue;
                }
                ++parsed_spawns;
                int tx0 = rect.array()[0].as_int(0);
                int ty0 = rect.array()[1].as_int(0);
                int tx1 = rect.array()[2].as_int(0);
                int ty1 = rect.array()[3].as_int(0);
                if (tx1 < tx0) {
                    std::swap(tx0, tx1);
                }
                if (ty1 < ty0) {
                    std::swap(ty0, ty1);
                }
                g.x0 = tx0 * map::kTileSize;
                g.y0 = ty0 * map::kTileSize;
                g.x1 = tx1 * map::kTileSize;
                g.y1 = ty1 * map::kTileSize;
                if (max_dist_px > 0) {
                    const int cx = (g.x0 + g.x1) / 2;
                    const int cy = (g.y0 + g.y1) / 2;
                    const long long dx = static_cast<long long>(cx) - spawn_x;
                    const long long dy = static_cast<long long>(cy) - spawn_y;
                    if (dx * dx + dy * dy > static_cast<long long>(max_dist_px) * max_dist_px) {
                        continue;
                    }
                }
                m_spawns.push_back(g);
                ++loaded_spawns;
            }
        }

        rebuild_spawn_map_index();
        log::info("GameWorld: monsters json defs=" + std::to_string(loaded_defs) +
                  " spawns_parsed=" + std::to_string(parsed_spawns) +
                  " spawns_kept=" + std::to_string(loaded_spawns) + " from " + dir);
        return loaded_defs > 0;
    }

    void GameWorld::rebuild_spawn_map_index() {
        m_spawn_indices_by_map.clear();
        for (std::size_t position = 0; position < m_spawns.size(); ++position) {
            m_spawn_indices_by_map[m_spawns[position].map_index].push_back(position);
        }
    }

    void GameWorld::apply_named_spawn_maps(const map::MapCatalog& maps) {
        if (maps.empty()) {
            return;
        }
        for (MonsterSpawn& spawn : m_spawns) {
            const int x = (spawn.x0 + spawn.x1) / 2;
            const int y = (spawn.y0 + spawn.y1) / 2;
            spawn.map_index = maps.resolve_teleport_map(x, y, spawn.map_index);
        }
        rebuild_spawn_map_index();
    }

    void GameWorld::bind_field_spawns_to_overworld(const map::MapCatalog& maps) {
        if (maps.empty()) {
            return;
        }
        for (MonsterSpawn& spawn : m_spawns) {
            if (spawn.map_index == 0) {
                continue;
            }
            const int x = (spawn.x0 + spawn.x1) / 2;
            const int y = (spawn.y0 + spawn.y1) / 2;
            const std::optional<int> cell_index = maps.cell_index_at_pixel(x, y);
            if (!cell_index.has_value() || *cell_index != 0) {
                continue;
            }
            spawn.map_index = 0;
        }
        rebuild_spawn_map_index();
    }

    const MonsterDef* GameWorld::find_def(int index) const {
        const auto found = m_def_positions.find(index);
        return found == m_def_positions.end() ? nullptr : &m_defs[found->second];
    }

    std::optional<std::uint32_t> GameWorld::spawn_monster_at(
        std::uint32_t& id_gen, int def_index, int map, int x, int y, int z) {
        const MonsterDef* def = find_def(def_index);
        if (!def || !ensure_cell_for_pixel(x, y)) {
            return std::nullopt;
        }

        MonsterSpawn spawn;
        spawn.def_index = def_index;
        spawn.map_index = map;
        spawn.x0 = x;
        spawn.y0 = y;
        spawn.x1 = x;
        spawn.y1 = y;
        MonsterEntity monster;
        // An explicitly placed standard must appear at its configured coordinate
        // even when an ordinary monster occupies the same cell.
        const bool check_occupancy =
            def->race != static_cast<int>(protocol::MonsterRace::CastleStandard);
        if (!try_place_monster(spawn, *def, id_gen, monster, check_occupancy)) {
            return std::nullopt;
        }
        monster.z = z;
        m_monsters.push_back(std::move(monster));
        return id_gen++;
    }

    const MonsterSpawn* GameWorld::find_spawn_by_area(int area) const {
        if (area <= 0) {
            return nullptr;
        }
        for (const auto& spawn : m_spawns) {
            if (spawn.area == area) {
                return &spawn;
            }
        }
        return nullptr;
    }

    bool GameWorld::walkable_pixel(int px, int py) const {
        // compatibility behavior(..., SM_NOTMOVEABLE): 8×8 subcell bit in the low 16
        // bits.
        return m_grid.walkable_world_pixel(px, py);
    }

    bool GameWorld::monster_spawnable_pixel(int px, int py) const {
        // compatibility behavior: IsValidTile(..., SM_ATTR_VILLAGE) must be true (village
        // bit clear).
        return m_grid.walkable_world_pixel(px, py, map::kAttrVillage);
    }

    void GameWorld::occupy_pixel(const MonsterEntity& m, int& ox, int& oy) {
        if (m.dirty_move || m.moving) {
            ox = m.target_x;
            oy = m.target_y;
        } else {
            ox = m.x;
            oy = m.y;
        }
    }

    class GameWorld::TickOccupancyScope final {
      public:
        /** Activates lazy occupancy queries for a stable monster traversal.
         * @param world World being advanced.
         * @param players Player poses used by this tick.
         */
        TickOccupancyScope(GameWorld& world, const std::vector<PlayerPose>& players)
            : m_world(world), m_players(players), m_previous_scope(world.m_tick_occupancy_scope),
              m_previous_index(world.m_tick_occupancy), m_previous_players(world.m_tick_players),
              m_active(world.m_use_occupancy_index && world.m_monsters.size() > 1) {
            world.m_tick_occupancy_scope = m_active ? this : nullptr;
            world.m_tick_occupancy = nullptr;
            world.m_tick_players = m_active ? &players : nullptr;
        }

        /** Restores the previous query context without allocating or updating the index. */
        ~TickOccupancyScope() noexcept {
            m_world.m_tick_occupancy_scope = m_previous_scope;
            m_world.m_tick_occupancy = m_previous_index;
            m_world.m_tick_players = m_previous_players;
        }

        TickOccupancyScope(const TickOccupancyScope&) = delete;
        TickOccupancyScope& operator=(const TickOccupancyScope&) = delete;

        /** Builds reservations only when the tick first needs an occupancy query. */
        void ensure_built() {
            if (m_built) {
                return;
            }
            m_index.emplace();
            m_index->rebuild(m_world.m_monsters, m_players);
            // The current monster may already have changed before its first query.
            if (m_previous_monster) {
                m_previous_reservation = WorldOccupancyIndex::reservation(*m_previous_monster);
            }
            m_built = true;
            m_world.m_tick_occupancy = &*m_index;
        }

        /** Publishes the previous monster's final reservation before processing the next.
         * @param monster Next monster in the stable tick traversal.
         */
        void advance(MonsterEntity& monster) {
            if (!m_active) {
                return;
            }
            if (m_built) {
                // Tick never inserts or erases monsters, so the previous pointer stays valid.
                if (m_previous_monster) {
                    m_index->update(m_previous_reservation, *m_previous_monster);
                }
                m_previous_reservation = WorldOccupancyIndex::reservation(monster);
            }
            m_previous_monster = &monster;
        }

      private:
        GameWorld& m_world;
        const std::vector<PlayerPose>& m_players;
        TickOccupancyScope* m_previous_scope;
        WorldOccupancyIndex* m_previous_index;
        const std::vector<PlayerPose>* m_previous_players;
        std::optional<WorldOccupancyIndex> m_index;
        WorldOccupancyIndex::Reservation m_previous_reservation{};
        const MonsterEntity* m_previous_monster = nullptr;
        bool m_active;
        bool m_built = false;
    };

    bool GameWorld::occupancy_blocked(std::uint32_t self_id,
                                      int px,
                                      int py,
                                      int map,
                                      const std::vector<PlayerPose>* players) const {
        if (players == nullptr || players == m_tick_players) {
            if (m_tick_occupancy_scope) {
                m_tick_occupancy_scope->ensure_built();
            }
            if (m_tick_occupancy) {
                return m_tick_occupancy->blocked(self_id, px, py, map, players != nullptr);
            }
        }
        for (const auto& other : m_monsters) {
            if (other.id == self_id || other.hp <= 0 || other.map != map) {
                continue;
            }
            if (!MonsterRaceInfo::blocks_collision(other.race)) {
                continue;
            }
            int ox = 0;
            int oy = 0;
            occupy_pixel(other, ox, oy);
            if (same_occupancy_cell(px, py, ox, oy)) {
                return true;
            }
        }
        if (players != nullptr) {
            for (const auto& p : *players) {
                if (p.map != map) {
                    continue;
                }
                if (same_occupancy_cell(px, py, p.x, p.y)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool GameWorld::is_move_tile(std::uint32_t self_id,
                                 int from_x,
                                 int from_y,
                                 int dx,
                                 int dy,
                                 int map,
                                 const std::vector<PlayerPose>& players) const {
        const int absdx = std::abs(dx);
        const int absdy = std::abs(dy);
        const int xy = absdx < absdy ? absdy : absdx;
        if (xy <= 0) {
            return false;
        }
        bool more = true;
        for (int i = 8; more && i <= 32; i += 8) {
            more = i < xy;
            const int sx = more ? from_x + dx * i / xy : from_x + dx;
            const int sy = more ? from_y + dy * i / xy : from_y + dy;
            if (!monster_spawnable_pixel(sx, sy)) {
                return false;
            }
            if (occupancy_blocked(self_id, sx, sy, map, &players)) {
                return false;
            }
        }
        if (xy > 32) {
            const int ex = from_x + dx;
            const int ey = from_y + dy;
            if (!monster_spawnable_pixel(ex, ey) ||
                occupancy_blocked(self_id, ex, ey, map, &players)) {
                return false;
            }
        }
        return true;
    }

    bool GameWorld::ensure_cell_for_pixel(int px, int py) {
        int tx = 0;
        int ty = 0;
        coords::pixel_to_tile(px, py, tx, ty);
        int cx = 0;
        int cy = 0;
        int lx = 0;
        int ly = 0;
        coords::world_tile_to_cell(tx, ty, cx, cy, lx, ly);
        return load_one_cell(m_maps_dir, cx, cy);
    }

    bool GameWorld::try_place_monster(const MonsterSpawn& g,
                                      const MonsterDef& def,
                                      std::uint32_t id,
                                      MonsterEntity& out,
                                      bool check_occupancy) {
        out = MonsterEntity{};
        out.id = id;
        out.def_index = def.index;
        out.race = def.race;
        out.name = def.name;
        out.max_hp = def.max_hp;
        out.hp = def.max_hp;
        out.roam_speed = def.speed > 0.0f ? def.speed : 20.0f;
        const int chase_ms = def.chase_speed_ms > 0 ? def.chase_speed_ms : 800;
        out.chase_speed = 32000.0f / static_cast<float>(chase_ms);
        out.speed = out.roam_speed;
        out.attack_range = def.attack_range;
        out.detect_range = def.detect_range > 0 ? def.detect_range : 160;
        out.sight_range = def.sight_range > 0 ? def.sight_range : 240;
        out.aggressive = def.aggressive;
        out.min_attack = def.min_attack;
        out.max_attack = def.max_attack;
        out.roam_x0 = g.x0;
        out.roam_y0 = g.y0;
        out.roam_x1 = g.x1;
        out.roam_y1 = g.y1;
        bool placed = false;
        for (int attempt = 0; attempt < 48; ++attempt) {
            const int x = irand(g.x0, g.x1);
            const int y = irand(g.y0, g.y1);
            if (monster_spawnable_pixel(x, y) &&
                (!check_occupancy || !occupancy_blocked(id, x, y, g.map_index, nullptr))) {
                out.x = x;
                out.y = y;
                placed = true;
                break;
            }
        }
        if (!placed) {
            // compatibility behavior returns FALSE when IsValidTile fails; the gen
            // retries on the next pulse instead of standing on a blocked tile.
            return false;
        }
        out.map = g.map_index;
        out.target_x = out.x;
        out.target_y = out.y;
        if (out.race == static_cast<int>(protocol::MonsterRace::FireFlower)) {
            // compatibility behavior starts invincible, then triggers skill 34 after
            // twelve seconds and deletes itself seven seconds later.
            out.gstate |= protocol::kCgsInvincible;
            out.fire_flower_explode_left = 12.0f;
        }
        // Static races (chests, fire flowers, magic fields, …) never start a roam
        // timer.
        if (MonsterRaceInfo::can_roam(out.race)) {
            out.idle_left = 2.0f + static_cast<float>(irand(0, 40)) * 0.1f;
        } else {
            out.idle_left = 0.0f;
            out.ai = MonsterEntity::Ai::Roam;
        }
        out.dirty_create = true;
        return true;
    }

    std::optional<std::size_t> GameWorld::find_farthest_monster(
        int px,
        int py,
        int keep_radius_px,
        const std::unordered_set<std::uint32_t>& protected_monsters) const {
        const long long keep2 =
            static_cast<long long>(std::max(0, keep_radius_px)) * std::max(0, keep_radius_px);
        std::optional<std::size_t> best;
        long long best_d2 = -1;
        for (int i = 0; i < static_cast<int>(m_monsters.size()); ++i) {
            const auto& m = m_monsters[static_cast<std::size_t>(i)];
            if (m.castle_flag >= 0 || m.hp <= 0 || !MonsterRaceInfo::accepts_damage(m.race)) {
                continue;
            }
            const long long dx = static_cast<long long>(m.x) - px;
            const long long dy = static_cast<long long>(m.y) - py;
            const long long d2 = dx * dx + dy * dy;
            if (d2 <= keep2) {
                continue;
            }
            if (protected_monsters.count(m.id) != 0) {
                continue;
            }
            if (d2 > best_d2) {
                best_d2 = d2;
                best = static_cast<std::size_t>(i);
            }
        }
        return best;
    }

    int GameWorld::ensure_monsters_around(int px,
                                          int py,
                                          int radius_px,
                                          std::uint32_t& id_gen,
                                          int max_monsters,
                                          int map,
                                          const std::vector<PlayerPose>& viewers) {
        if (radius_px <= 0) {
            radius_px = 16384;
        }
        if (max_monsters <= 0) {
            max_monsters = 256;
        }

        // Co-located clients share a budget; distant hunting grounds need independent capacity.
        std::vector<const PlayerPose*> neighborhoods;
        for (const auto& viewer : viewers) {
            if (std::none_of(neighborhoods.begin(), neighborhoods.end(), [&](const auto* focus) {
                    return focus->map == viewer.map &&
                           coords::in_sight(focus->x, focus->y, viewer.x, viewer.y);
                })) {
                neighborhoods.push_back(&viewer);
            }
        }
        const auto population_limit = std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            static_cast<std::int64_t>(max_monsters) *
                static_cast<std::int64_t>(std::max<std::size_t>(1, neighborhoods.size())));

        // Warm walkable cells around the player so placement + AI work after a city
        // hop.
        {
            int tx = 0;
            int ty = 0;
            coords::pixel_to_tile(px, py, tx, ty);
            int cx = 0;
            int cy = 0;
            int lx = 0;
            int ly = 0;
            coords::world_tile_to_cell(tx, ty, cx, cy, lx, ly);
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    (void) load_one_cell(m_maps_dir, cx + dx, cy + dy);
                }
            }
        }

        const long long r2 = static_cast<long long>(radius_px) * radius_px;
        const int sight_keep = coords::kSightTileRadius * map::kTileSize;
        int spawned = 0;
        // Count once per refill rather than scanning every monster for every gen.
        std::map<std::array<int, 5>, int> live_by_rectangle;
        std::unordered_set<std::uint32_t> protected_monsters;
        const auto visible_to_viewer = [&](const MonsterEntity& monster) {
            return std::any_of(viewers.begin(), viewers.end(), [&](const PlayerPose& viewer) {
                return viewer.map == monster.map &&
                       coords::in_sight(viewer.x, viewer.y, monster.x, monster.y);
            });
        };
        int population = 0;
        for (const auto& monster : m_monsters) {
            if (monster.castle_flag >= 0) {
                continue;
            }
            if (visible_to_viewer(monster)) {
                protected_monsters.insert(monster.id);
            }
            if (m_harvest_plant_indices.count(monster.def_index) == 0) {
                ++population;
            }
            if (monster.hp > 0) {
                ++live_by_rectangle[{monster.map,
                                     monster.roam_x0,
                                     monster.roam_y0,
                                     monster.roam_x1,
                                     monster.roam_y1}];
            }
        }

        auto gen_dist2 = [&](const MonsterSpawn& g) -> long long {
            const int nearest_x = std::clamp(px, g.x0, g.x1);
            const int nearest_y = std::clamp(py, g.y0, g.y1);
            const long long ndx = static_cast<long long>(nearest_x) - px;
            const long long ndy = static_cast<long long>(nearest_y) - py;
            return ndx * ndx + ndy * ndy;
        };

        std::vector<std::pair<long long, std::size_t>> nearby;
        const auto consider_spawn = [&](std::size_t position) {
            const auto distance = gen_dist2(m_spawns[position]);
            if (distance <= r2) {
                nearby.emplace_back(distance, position);
            }
        };
        if (map < 0) {
            for (std::size_t position = 0; position < m_spawns.size(); ++position) {
                consider_spawn(position);
            }
        } else if (const auto found = m_spawn_indices_by_map.find(map);
                   found != m_spawn_indices_by_map.end()) {
            for (const auto position : found->second) {
                consider_spawn(position);
            }
        }
        std::sort(nearby.begin(), nearby.end());

        for (const auto& [distance, gi] : nearby) {
            const MonsterSpawn& g = m_spawns[gi];
            const MonsterDef* def = find_def(g.def_index);
            if (!def) {
                continue;
            }
            MonsterSpawn& gen = m_spawns[gi];
            if (!gen_cycle_due(gen)) {
                continue;
            }
            const std::array<int, 5> rectangle{g.map_index, g.x0, g.y0, g.x1, g.y1};
            const auto found = live_by_rectangle.find(rectangle);
            const int live = found == live_by_rectangle.end() ? 0 : found->second;
            const int want = gen.count;
            if (live >= want) {
                arm_gen_cycle(gen);
                continue;
            }
            // Only placement needs to warm the gen's cells again.
            for (int py_px : {g.y0, (g.y0 + g.y1) / 2, g.y1}) {
                for (int px_px : {g.x0, (g.x0 + g.x1) / 2, g.x1}) {
                    (void) ensure_cell_for_pixel(px_px, py_px);
                }
            }
            const int gen_keep = static_cast<int>(std::sqrt(static_cast<double>(distance)));
            const int keep_radius = std::max(gen_keep, sight_keep);
            for (int i = live; i < want; ++i) {
                while (population >= population_limit) {
                    const auto replaced =
                        find_farthest_monster(px, py, keep_radius, protected_monsters);
                    if (!replaced) {
                        break;
                    }
                    const auto& old = m_monsters[*replaced];
                    if (m_harvest_plant_indices.count(old.def_index) == 0) {
                        --population;
                    }
                    const auto count = live_by_rectangle.find(
                        {old.map, old.roam_x0, old.roam_y0, old.roam_x1, old.roam_y1});
                    if (count != live_by_rectangle.end()) {
                        --count->second;
                    }
                    m_monsters.erase(m_monsters.begin() + static_cast<std::ptrdiff_t>(*replaced));
                }
                if (population >= population_limit) {
                    break;
                }
                MonsterEntity m;
                if (!try_place_monster(g, *def, id_gen, m)) {
                    continue;
                }
                ++id_gen;
                if (m_harvest_plant_indices.count(m.def_index) == 0) {
                    ++population;
                }
                if (m.hp > 0) {
                    ++live_by_rectangle[rectangle];
                }
                if (visible_to_viewer(m)) {
                    protected_monsters.insert(m.id);
                }
                m_monsters.push_back(std::move(m));
                ++spawned;
            }
            arm_gen_cycle(gen);
        }

        /* Debug monster spawn loading
        if (spawned > 0) {
            log::info("GameWorld: ensure_monsters_around spawned=" + std::to_string(spawned) +
                      " near (" + std::to_string(px) + "," + std::to_string(py) +
                      ") live=" + std::to_string(m_monsters.size()));
        }*/
        return spawned;
    }

    void GameWorld::pick_roam_target(MonsterEntity& m, const std::vector<PlayerPose>& players) {
        // GetRandomPt: one 32px octant step that IsMoveTile accepts.
        for (int attempt = 0; attempt < 32; ++attempt) {
            const int ox = irand(0, 64) - 32;
            const int oy = irand(0, 64) - 32;
            if (ox == 0 && oy == 0) {
                continue;
            }
            int dx = 0;
            int dy = 0;
            octant_step(ox, oy, dx, dy);
            const int nx = std::clamp(m.x + dx, m.roam_x0, m.roam_x1);
            const int ny = std::clamp(m.y + dy, m.roam_y0, m.roam_y1);
            dx = nx - m.x;
            dy = ny - m.y;
            if (dx == 0 && dy == 0) {
                continue;
            }
            if (is_move_tile(m.id, m.x, m.y, dx, dy, m.map, players)) {
                m.target_x = nx;
                m.target_y = ny;
                m.moving = true;
                m.move_is_run = false;
                m.dirty_move = true;
                m.idle_left = 0.0f;
                m.path_waypoints.clear();
                return;
            }
        }
        m.moving = false;
        m.idle_left = 2.0f + static_cast<float>(irand(0, 20)) * 0.1f;
    }

    bool GameWorld::pick_chase_target(MonsterEntity& m,
                                      int px,
                                      int py,
                                      const std::vector<PlayerPose>& players) {
        // GetChasePt: consume a stored detour, else a 32px octant step, else A*.
        const int dx = px - m.x;
        const int dy = py - m.y;
        const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
        const int engage = std::max(12, m.attack_range);

        auto apply_step = [&](int step_x, int step_y) -> bool {
            if (step_x == 0 && step_y == 0) {
                return false;
            }
            if (!is_move_tile(m.id, m.x, m.y, step_x, step_y, m.map, players)) {
                return false;
            }
            m.target_x = m.x + step_x;
            m.target_y = m.y + step_y;
            m.moving = true;
            m.move_is_run = true;
            m.dirty_move = true;
            m.idle_left = 0.0f;
            m.ai = MonsterEntity::Ai::Chase;
            m.dir = dir_from_delta(dx, dy);
            return true;
        };

        auto sidestep_overlap = [&]() -> bool {
            if (!occupancy_blocked(m.id, m.x, m.y, m.map, &players)) {
                return false;
            }
            constexpr int kSidestep = 8;
            static constexpr int kDx[8] = {
                kSidestep, kSidestep, 0, -kSidestep, -kSidestep, -kSidestep, 0, kSidestep};
            static constexpr int kDy[8] = {
                0, kSidestep, kSidestep, kSidestep, 0, -kSidestep, -kSidestep, -kSidestep};
            for (int dir = 0; dir < 8; ++dir) {
                if (apply_step(kDx[dir], kDy[dir])) {
                    m.path_waypoints.clear();
                    return true;
                }
            }
            return false;
        };

        if (dist <= static_cast<float>(engage)) {
            if (sidestep_overlap()) {
                return true;
            }
            m.moving = false;
            m.move_busy_left = 0.0f;
            m.idle_left = 0.0f;
            m.ai = MonsterEntity::Ai::Attack;
            m.path_waypoints.clear();
            return true;
        }

        if (!m.path_waypoints.empty()) {
            const auto [wx, wy] = m.path_waypoints.front();
            const int step_x = wx - m.x;
            const int step_y = wy - m.y;
            m.path_waypoints.erase(m.path_waypoints.begin());
            if (apply_step(step_x, step_y)) {
                return true;
            }
            m.path_waypoints.clear();
        }

        int step_x = 0;
        int step_y = 0;
        const int rplus32 = engage + 32;
        if (dx * dx + dy * dy <= rplus32 * rplus32) {
            const float want = std::max(4.0f, dist - static_cast<float>(engage));
            step_x = static_cast<int>(static_cast<float>(dx) / dist * want);
            step_y = static_cast<int>(static_cast<float>(dy) / dist * want);
        } else {
            octant_step(dx, dy, step_x, step_y);
        }

        if (apply_step(step_x, step_y)) {
            return true;
        }

        // Compatibility behavior rejects a target on SM_ATTR_VILLAGE before searching.
        if (!monster_spawnable_pixel(px, py)) {
            m.moving = false;
            return false;
        }

        const int range_sq = engage * engage;
        const FindPathResult path = find_monster_path(
            m.x, m.y, px, py, range_sq, [&](int from_x, int from_y, int sdx, int sdy) {
                return is_move_tile(m.id, from_x, from_y, sdx, sdy, m.map, players);
            });
        if (path.found && !path.waypoints.empty()) {
            m.path_waypoints.assign(path.waypoints.begin() + 1, path.waypoints.end());
            const int first_x = path.waypoints.front().first;
            const int first_y = path.waypoints.front().second;
            if (apply_step(first_x - m.x, first_y - m.y)) {
                return true;
            }
            m.path_waypoints.clear();
        }
        if (sidestep_overlap()) {
            return true;
        }
        m.moving = false;
        return false;
    }

    MonsterEntity* GameWorld::find_monster(std::uint32_t id) {
        for (auto& m : m_monsters) {
            if (m.id == id) {
                return &m;
            }
        }
        return nullptr;
    }

    const MonsterEntity* GameWorld::find_monster(std::uint32_t id) const {
        for (const auto& m : m_monsters) {
            if (m.id == id) {
                return &m;
            }
        }
        return nullptr;
    }

    std::vector<std::uint32_t> GameWorld::monsters_in_radius(
        int center_x, int center_y, int map, int radius_pixels, int max_targets) const {
        std::vector<std::uint32_t> ids;
        if (radius_pixels < 0 || max_targets < 0) {
            return ids;
        }
        const std::int64_t radius = radius_pixels;
        const std::int64_t radius_squared = radius * radius;
        for (const auto& monster : m_monsters) {
            if (monster.hp <= 0 || monster.map != map ||
                !MonsterRaceInfo::accepts_damage(monster.race)) {
                continue;
            }
            const std::int64_t dx = static_cast<std::int64_t>(monster.x) - center_x;
            const std::int64_t dy = static_cast<std::int64_t>(monster.y) - center_y;
            if (dx * dx + dy * dy > radius_squared) {
                continue;
            }
            ids.push_back(monster.id);
            if (max_targets > 0 && static_cast<int>(ids.size()) >= max_targets) {
                break;
            }
        }
        return ids;
    }

    std::vector<std::uint32_t> GameWorld::monsters_in_tile_rect(
        int center_x, int center_y, int map, int radius_tiles, int max_targets) const {
        std::vector<std::uint32_t> ids;
        if (radius_tiles < 0 || max_targets < 0) {
            return ids;
        }
        for (const auto& monster : m_monsters) {
            if (monster.hp <= 0 || monster.map != map ||
                !MonsterRaceInfo::accepts_damage(monster.race)) {
                continue;
            }
            if (!coords::in_tile_rect(center_x, center_y, monster.x, monster.y, radius_tiles)) {
                continue;
            }
            ids.push_back(monster.id);
            if (max_targets > 0 && static_cast<int>(ids.size()) >= max_targets) {
                break;
            }
        }
        return ids;
    }

    bool GameWorld::apply_monster_control(std::uint32_t monster_id,
                                          float duration_sec,
                                          bool break_on_damage) {
        MonsterEntity* monster = find_monster(monster_id);
        if (!monster || monster->castle_flag >= 0 || monster->hp <= 0 || duration_sec <= 0.0f ||
            !MonsterRaceInfo::accepts_damage(monster->race)) {
            return false;
        }
        monster->control_left = duration_sec;
        monster->control_break_on_damage = break_on_damage;
        monster->moving = false;
        monster->move_busy_left = 0.0f;
        monster->attack_swinging = false;
        monster->dirty_stop = true;
        monster->dirty_slow = true;
        monster->target_x = monster->x;
        monster->target_y = monster->y;
        return true;
    }

    bool GameWorld::apply_monster_slow(std::uint32_t monster_id, float duration_sec, int percent) {
        MonsterEntity* monster = find_monster(monster_id);
        if (!monster || monster->castle_flag >= 0 || monster->hp <= 0 || duration_sec <= 0.0f ||
            percent >= 0 || !MonsterRaceInfo::accepts_damage(monster->race)) {
            return false;
        }
        monster->slow_left = duration_sec;
        monster->slow_percent = percent;
        monster->dirty_slow = true;
        return true;
    }

    void GameWorld::set_castle_flag_damage(
        std::function<std::int32_t(std::uint32_t, MonsterEntity&, std::int32_t)> handler) {
        m_castle_flag_damage = std::move(handler);
    }

    std::int32_t GameWorld::apply_player_attack(std::uint32_t player_id,
                                                std::uint32_t monster_id,
                                                std::int32_t damage,
                                                int px,
                                                int py,
                                                bool physical) {
        MonsterEntity* m = find_monster(monster_id);
        if (!m || m->hp <= 0) {
            return 0;
        }
        if (m->castle_flag >= 0) {
            return m_castle_flag_damage ? m_castle_flag_damage(player_id, *m, damage) : 0;
        }
        if (m->spawn_protection_left > 0.0f) {
            return 0;
        }
        // Chests / magic fields / eggs / fire flowers reject normal attack damage.
        if (!MonsterRaceInfo::accepts_damage(m->race)) {
            return 0;
        }
        const std::int32_t dealt = combat::clamp_nonneg_i32(damage);
        const std::int32_t applied = std::min(dealt, m->hp);
        if (dealt > 0) {
            m->hp = combat::clamp_nonneg_i32(static_cast<std::int64_t>(m->hp) - applied);
            if (m->def_index == 213 && dealt >= 100) {
                m->doggebi_pending_reaction_damage = dealt;
                m->doggebi_pending_reaction_physical = physical;
            }
            if (m->control_break_on_damage) {
                if (m->control_left > 0.0f) {
                    m->dirty_slow = true;
                }
                m->control_left = 0.0f;
                m->control_break_on_damage = false;
            }
        }
        // compatibility behavior rewards are derived from hostility, not from the killing blow.
        // Safe-zone and out-of-sight damage may still land but must not establish a
        // reward claim.
        if (can_record_hostility(*this, *m, px, py)) {
            add_hostility(*m, player_id, applied);
            if (applied > 0 && player_id != 0) {
                m->reward_damage_by_player[player_id] += applied;
            }
            if (applied == 0 && !m->aggro) {
                m->aggro = true;
                m->aggro_player_id = player_id;
                m->aggro_hostility = 0;
            }
            m->ai = MonsterEntity::Ai::Attack;
            m->idle_left = 0.0f;
            if (m->moving) {
                m->moving = false;
                m->move_busy_left = 0.0f;
                m->dirty_stop = true;  // client must abort roam waypoint
            }
            m->dir = dir_from_delta(px - m->x, py - m->y);
        }
        return applied;
    }

    bool GameWorld::apply_monster_hostility(
        std::uint32_t player_id, std::uint32_t monster_id, std::int32_t hostility, int px, int py) {
        MonsterEntity* monster = find_monster(monster_id);
        if (!monster || monster->castle_flag >= 0 || monster->hp <= 0 || hostility <= 0 ||
            !can_record_hostility(*this, *monster, px, py)) {
            return false;
        }
        add_hostility(*monster, player_id, hostility);
        monster->ai = MonsterEntity::Ai::Attack;
        monster->idle_left = 0.0f;
        if (monster->moving) {
            monster->moving = false;
            monster->move_busy_left = 0.0f;
            monster->dirty_stop = true;
        }
        monster->dir = dir_from_delta(px - monster->x, py - monster->y);
        return true;
    }

    bool GameWorld::set_monster_hostility(
        std::uint32_t player_id, std::uint32_t monster_id, std::int32_t hostility, int px, int py) {
        MonsterEntity* monster = find_monster(monster_id);
        if (!monster || monster->hp <= 0 || hostility < 0 ||
            !MonsterRaceInfo::can_pursue(monster->race)) {
            return false;
        }
        if (hostility == 0) {
            monster->hostility_by_player.erase(player_id);
            monster->reward_damage_by_player.erase(player_id);
            refresh_aggro_target(*monster);
            return true;
        }
        if (!can_record_hostility(*this, *monster, px, py)) {
            return false;
        }
        monster->hostility_by_player[player_id] = hostility;
        refresh_aggro_target(*monster);
        monster->ai = MonsterEntity::Ai::Attack;
        monster->idle_left = 0.0f;
        monster->dir = dir_from_delta(px - monster->x, py - monster->y);
        return true;
    }

    void GameWorld::set_harvest_plant_definitions(const std::vector<int>& indices) {
        m_harvest_plant_indices.clear();
        m_harvest_plant_indices.insert(indices.begin(), indices.end());
    }

    int
    GameWorld::begin_open_chest(std::uint32_t player_id, std::uint32_t monster_id, float open_sec) {
        MonsterEntity* m = find_monster(monster_id);
        if (!m || m->hp <= 0 || !MonsterRaceInfo::is_openable(m->race)) {
            return 1;
        }
        if (m->open_player_id != 0 && m->open_player_id != player_id) {
            return 2;
        }
        if (m->open_player_id == player_id && m->open_left > 0.0f) {
            return 0;  // already opening
        }
        m->open_player_id = player_id;
        m->open_left = open_sec > 0.0f ? open_sec : 6.0f;
        m->open_remove_left = 0.0f;
        m->aggro = false;
        m->aggro_player_id = 0;
        m->moving = false;
        m->move_busy_left = 0.0f;
        m->idle_left = 0.0f;
        return 0;
    }

    bool GameWorld::cancel_open_chest(std::uint32_t player_id, std::uint32_t monster_id) {
        bool canceled = false;
        for (auto& m : m_monsters) {
            if (!MonsterRaceInfo::is_openable(m.race)) {
                continue;
            }
            if (m.open_player_id != player_id) {
                continue;
            }
            if (monster_id != 0 && m.id != monster_id) {
                continue;
            }
            if (m.open_left <= 0.0f && m.open_remove_left <= 0.0f) {
                continue;
            }
            // Only cancel while still counting down; do not abort die/remove.
            if (m.open_left > 0.0f) {
                m.open_player_id = 0;
                m.open_left = 0.0f;
                canceled = true;
            }
        }
        return canceled;
    }

    void GameWorld::clear_aggro_on_player(std::uint32_t player_id,
                                          std::vector<MonsterMoveEvent>* moves_out) {
        if (player_id == 0) {
            return;
        }
        for (auto& m : m_monsters) {
            const bool was_target = m.aggro_player_id == player_id;
            const bool had_hostility = m.hostility_by_player.erase(player_id) > 0;
            const bool had_reward_damage = m.reward_damage_by_player.erase(player_id) > 0;
            if (!was_target && !had_hostility && !had_reward_damage) {
                continue;
            }
            refresh_aggro_target(m);
            if (!m.aggro) {
                clear_combat(m);
            }
            if (moves_out && !m.aggro && m.dirty_stop) {
                m.move_busy_left = 0.0f;
                emit_move_end(m, *moves_out);
            }
        }
    }

    bool GameWorld::clear_monster_aggro(std::uint32_t monster_id,
                                        std::vector<MonsterMoveEvent>* moves_out) {
        MonsterEntity* monster = find_monster(monster_id);
        if (!monster || monster->hp <= 0 || !MonsterRaceInfo::can_pursue(monster->race)) {
            return false;
        }
        clear_combat(*monster);
        if (moves_out && monster->dirty_stop) {
            monster->move_busy_left = 0.0f;
            emit_move_end(*monster, *moves_out);
        }
        return true;
    }

    void GameWorld::spawn_from_config(std::uint32_t& id_gen,
                                      int max_monsters,
                                      int prefer_x,
                                      int prefer_y) {
        m_monsters.clear();
        if (max_monsters <= 0) {
            max_monsters = 64;
        }
        std::vector<MonsterSpawn> ordered = m_spawns;
        std::sort(ordered.begin(),
                  ordered.end(),
                  [prefer_x, prefer_y](const MonsterSpawn& a, const MonsterSpawn& b) {
                      const long long acx = (static_cast<long long>(a.x0) + a.x1) / 2;
                      const long long acy = (static_cast<long long>(a.y0) + a.y1) / 2;
                      const long long bcx = (static_cast<long long>(b.x0) + b.x1) / 2;
                      const long long bcy = (static_cast<long long>(b.y0) + b.y1) / 2;
                      const long long adx = acx - prefer_x;
                      const long long ady = acy - prefer_y;
                      const long long bdx = bcx - prefer_x;
                      const long long bdy = bcy - prefer_y;
                      const long long ada = adx * adx + ady * ady;
                      const long long bda = bdx * bdx + bdy * bdy;
                      return ada < bda;
                  });

        for (const auto& g : ordered) {
            if (static_cast<int>(m_monsters.size()) >= max_monsters) {
                break;
            }
            const MonsterDef* def = find_def(g.def_index);
            if (!def) {
                continue;
            }
            const int remain = max_monsters - static_cast<int>(m_monsters.size());
            const int spawn_count = std::min(g.count, remain);
            for (int i = 0; i < spawn_count; ++i) {
                if (static_cast<int>(m_monsters.size()) >= max_monsters) {
                    break;
                }
                MonsterEntity m;
                if (!try_place_monster(g, *def, id_gen, m)) {
                    continue;
                }
                ++id_gen;
                m_monsters.push_back(std::move(m));
            }
        }
        for (auto& stored : m_spawns) {
            if (count_live_in_gen(m_monsters, stored) > 0) {
                arm_gen_cycle(stored);
            }
        }
        log::info("GameWorld: spawned " + std::to_string(m_monsters.size()) + " monsters (cap=" +
                  std::to_string(max_monsters) + ", gens=" + std::to_string(m_spawns.size()) + ")");
    }

    void GameWorld::tick(float dt, std::vector<MonsterMoveEvent>& moves_out) {
        std::vector<MonsterAttackEvent> ignored_attacks;
        std::vector<ChestOpenEvent> ignored_chests;
        tick(dt, {}, moves_out, ignored_attacks, ignored_chests);
    }

    void GameWorld::tick(float dt,
                         const std::vector<PlayerPose>& players,
                         std::vector<MonsterMoveEvent>& moves_out) {
        std::vector<MonsterAttackEvent> ignored_attacks;
        std::vector<ChestOpenEvent> ignored_chests;
        tick(dt, players, moves_out, ignored_attacks, ignored_chests);
    }

    void GameWorld::tick(float dt,
                         const std::vector<PlayerPose>& players,
                         std::vector<MonsterMoveEvent>& moves_out,
                         std::vector<MonsterAttackEvent>& attacks_out) {
        std::vector<ChestOpenEvent> ignored_chests;
        tick(dt, players, moves_out, attacks_out, ignored_chests);
    }

    void GameWorld::tick(float dt,
                         const std::vector<PlayerPose>& players,
                         std::vector<MonsterMoveEvent>& moves_out,
                         std::vector<MonsterAttackEvent>& attacks_out,
                         std::vector<ChestOpenEvent>& chests_out) {
        std::vector<MonsterCorpseEvent> ignored_corpses;
        tick(dt, players, moves_out, attacks_out, chests_out, ignored_corpses);
    }

    void GameWorld::tick(float dt,
                         const std::vector<PlayerPose>& players,
                         std::vector<MonsterMoveEvent>& moves_out,
                         std::vector<MonsterAttackEvent>& attacks_out,
                         std::vector<ChestOpenEvent>& chests_out,
                         std::vector<MonsterCorpseEvent>& corpses_out) {
        moves_out.clear();
        attacks_out.clear();
        chests_out.clear();
        corpses_out.clear();
        for (auto& g : m_spawns) {
            if (g.next_gen_left > 0.0f) {
                g.next_gen_left = std::max(0.0f, g.next_gen_left - dt);
            }
        }
        struct MapPlayers {
            std::vector<PlayerPose> targets;
            std::vector<PlayerPose> beacons;
        };
        const bool index_players = players.size() > 1 && !m_monsters.empty();
        std::unordered_map<std::uint32_t, const PlayerPose*> players_by_id;
        std::unordered_map<int, MapPlayers> players_by_map;
        if (index_players) {
            players_by_id.reserve(players.size());
            for (const auto& player : players) {
                players_by_id.try_emplace(player.id, &player);
                if (player.aggro_immune) {
                    continue;
                }
                auto& local = players_by_map[player.map];
                local.targets.push_back(player);
                if (player.beacon_active) {
                    local.beacons.push_back(player);
                }
            }
        }
        const auto find_player = [&](std::uint32_t id) -> const PlayerPose* {
            if (!index_players) {
                return !players.empty() && players.front().id == id ? &players.front() : nullptr;
            }
            const auto found = players_by_id.find(id);
            return found == players_by_id.end() ? nullptr : found->second;
        };
        const MapPlayers empty_map;
        TickOccupancyScope occupancy(*this, players);
        for (auto& m : m_monsters) {
            occupancy.advance(m);
            if (m.castle_flag >= 0) {
                continue;
            }
            (void) m.skill_buffs.tick(static_cast<std::int64_t>(std::time(nullptr)));
            m.spawn_protection_left = std::max(0.0f, m.spawn_protection_left - dt);
            m.doggebi_absorb_left = std::max(0.0f, m.doggebi_absorb_left - dt);
            m.doggebi_resist_all_left = std::max(0.0f, m.doggebi_resist_all_left - dt);
            if (m.slow_left > 0.0f) {
                m.slow_left = std::max(0.0f, m.slow_left - dt);
                if (m.slow_left <= 0.0f) {
                    m.slow_percent = 0;
                    m.dirty_slow = true;
                }
            }
            if (m.knee_left > 0.0f) {
                m.knee_left -= dt;
                if (m.knee_left <= 0.0f) {
                    m.knee_left = 0.0f;
                    m.gstate &= ~protocol::kCgsKnee;
                    MonsterCorpseEvent ev;
                    ev.kind = MonsterCorpseEvent::Kind::KneeTimeout;
                    ev.monster_id = m.id;
                    corpses_out.push_back(ev);
                }
                continue;
            }

            if (m.die_remove_left > 0.0f) {
                m.die_remove_left -= dt;
                if (m.die_remove_left <= 0.0f) {
                    m.die_remove_left = 0.0f;
                    MonsterCorpseEvent ev;
                    ev.kind = MonsterCorpseEvent::Kind::Remove;
                    ev.monster_id = m.id;
                    corpses_out.push_back(ev);
                }
                continue;
            }

            if (m.hp <= 0 && m.open_remove_left <= 0.0f) {
                continue;
            }

            // Post-open die delay → RemoveMonster.
            if (m.open_remove_left > 0.0f) {
                m.open_remove_left -= dt;
                if (m.open_remove_left <= 0.0f) {
                    ChestOpenEvent ev;
                    ev.kind = ChestOpenEvent::Kind::Remove;
                    ev.monster_id = m.id;
                    ev.player_id = m.open_player_id;
                    ev.def_index = m.def_index;
                    ev.x = m.x;
                    ev.y = m.y;
                    chests_out.push_back(ev);
                    m.hp = 0;
                    m.open_player_id = 0;
                    m.open_remove_left = 0.0f;
                }
                continue;
            }

            if (m.hp <= 0) {
                continue;
            }

            const auto map_players = players_by_map.find(m.map);
            const auto& local =
                map_players == players_by_map.end() ? empty_map : map_players->second;
            const auto& targets = index_players ? local.targets : players;
            const auto& beacons = index_players ? local.beacons : players;
            if (m.race == static_cast<int>(protocol::MonsterRace::FireFlower)) {
                // compatibility behavior does not acquire a target or use the regular
                // monster combat FSM. Its compatibility behavior explosion affects every
                // player in its five-tile radius.
                if (m.fire_flower_removed) {
                    continue;
                }
                if (!m.fire_flower_exploded) {
                    m.fire_flower_explode_left -= dt;
                    if (m.fire_flower_explode_left <= 0.0f) {
                        m.fire_flower_explode_left = 0.0f;
                        m.fire_flower_exploded = true;
                        m.fire_flower_skill_fire_left = 4.0f;
                        m.fire_flower_remove_left = 7.0f;
                        attacks_out.push_back(
                            {MonsterAttackEvent::Kind::SkillPrepare, m.id, 0, 34, 1, 0});
                    }
                    continue;
                }

                m.fire_flower_remove_left -= dt;
                if (m.fire_flower_skill_fire_left > 0.0f) {
                    m.fire_flower_skill_fire_left -= dt;
                    if (m.fire_flower_skill_fire_left <= 0.0f) {
                        m.fire_flower_skill_fire_left = 0.0f;
                        for (const PlayerPose& player : targets) {
                            if (player.map != m.map || player.aggro_immune ||
                                !coords::in_tile_rect(m.x, m.y, player.x, player.y, 5)) {
                                continue;
                            }
                            attacks_out.push_back(
                                {MonsterAttackEvent::Kind::SkillFire, m.id, player.id, 34, 1, 0});
                        }
                    }
                }
                if (m.fire_flower_remove_left <= 0.0f) {
                    m.fire_flower_remove_left = 0.0f;
                    m.fire_flower_removed = true;
                    corpses_out.push_back({MonsterCorpseEvent::Kind::Die, m.id});
                }
                continue;
            }

            // Treasure-box open countdown.
            if (m.open_player_id != 0 && m.open_left > 0.0f) {
                m.open_left -= dt;
                if (m.open_left <= 0.0f) {
                    ChestOpenEvent ev;
                    ev.kind = ChestOpenEvent::Kind::Completed;
                    ev.monster_id = m.id;
                    ev.player_id = m.open_player_id;
                    ev.def_index = m.def_index;
                    ev.x = m.x;
                    ev.y = m.y;
                    chests_out.push_back(ev);
                    m.open_left = 0.0f;
                    // Gathering stays visible until the host commits its inventory reward.
                    if (m_harvest_plant_indices.count(m.def_index) == 0) {
                        m.hp = 0;
                        m.open_remove_left = 1.2f;  // let AT_DIE (action 3) play
                    }
                }
                continue;  // no combat/roam while opening
            }

            m.attack_cooldown = std::max(0.0f, m.attack_cooldown - dt);
            m.skill_cooldown = std::max(0.0f, m.skill_cooldown - dt);
            m.detect_cooldown = std::max(0.0f, m.detect_cooldown - dt);
            if (m.pending_skill_slot >= 0) {
                m.skill_precast_left = std::max(0.0f, m.skill_precast_left - dt);
            }

            // Flush interrupt flags first (hit while roaming must abort client
            // waypoint).
            if (m.dirty_stop) {
                m.moving = false;
                m.move_busy_left = 0.0f;
                emit_move_end(m, moves_out);
            }
            if (m.dirty_move && m.moving) {
                emit_move_on(m, moves_out);
            }
            if (m.control_left > 0.0f) {
                m.control_left = std::max(0.0f, m.control_left - dt);
                if (m.control_left <= 0.0f) {
                    m.control_break_on_damage = false;
                    m.dirty_slow = true;
                }
                continue;
            }

            m.beacon_pursuit = false;
            if (MonsterRaceInfo::can_pursue(m.race) && m.spawn_protection_left <= 0.0f) {
                for (const auto& player : beacons) {
                    if (!player.beacon_active || player.aggro_immune || player.map != m.map ||
                        (m.aggro_player_id != 0 && m.aggro_player_id != player.id) ||
                        m_grid.has_attr_world_pixel(player.x, player.y, map::kAttrVillage)) {
                        continue;
                    }
                    const auto dx = static_cast<long long>(player.x) - m.x;
                    const auto dy = static_cast<long long>(player.y) - m.y;
                    if (dx * dx + dy * dy > static_cast<long long>(protocol::kBeaconRadiusPixels) *
                                                protocol::kBeaconRadiusPixels) {
                        continue;
                    }
                    m.beacon_pursuit = true;
                    if (!m.aggro && m.aggro_player_id == 0 && m.hostility_by_player.empty()) {
                        add_hostility(m, player.id, 1);
                        m.ai = MonsterEntity::Ai::Chase;
                        m.idle_left = 0.0f;
                        m.path_waypoints.clear();
                    }
                    break;
                }
            }
            const PlayerPose* prey = nullptr;
            if (m.aggro) {
                prey = find_player(m.aggro_player_id);
                if (prey && (prey->map != m.map || prey->aggro_immune)) {
                    prey = nullptr;
                }
                if (!prey) {
                    clear_combat(m);
                    if (m.dirty_stop) {
                        m.move_busy_left = 0.0f;
                        emit_move_end(m, moves_out);
                    }
                }
            }

            const int atk_range =
                std::max(12, m.attack_range + static_cast<int>(m.skill_buffs.archer_range_delta()));
            const int sight =
                std::max(atk_range + 32,
                         m.beacon_pursuit ? std::max(m.sight_range, protocol::kBeaconRadiusPixels)
                                          : (m.sight_range > 0 ? m.sight_range : 240));

            auto dist_to_prey = [&]() -> float {
                if (!prey) {
                    return 1.0e9f;
                }
                const float dx = static_cast<float>(prey->x - m.x);
                const float dy = static_cast<float>(prey->y - m.y);
                return std::sqrt(dx * dx + dy * dy);
            };

            // Count down MoveMonster travel time (server already snapped; mirrors
            // runtime state).
            bool finished_move = false;
            if (m.moving && m.move_busy_left > 0.0f) {
                m.move_busy_left -= dt;
                if (m.move_busy_left <= 0.0f) {
                    m.move_busy_left = 0.0f;
                    m.moving = false;
                    finished_move = true;
                    if (!m.aggro) {
                        m.idle_left = 3.0f + static_cast<float>(irand(0, 50)) * 0.1f;
                        emit_move_end(m, moves_out);
                    } else {
                        // Chase segments chain without End (MoveMonster
                        // bEndPos=FALSE) unless this step landed in melee.
                        m.idle_left = 0.0f;
                    }
                }
            }

            const bool may_pursue = MonsterRaceInfo::can_pursue(m.race);
            const bool may_roam = MonsterRaceInfo::can_roam(m.race);

            // Static races must never keep an in-flight roam/chase from older ticks.
            if (!may_roam && m.moving && !m.aggro) {
                m.moving = false;
                m.move_busy_left = 0.0f;
                m.target_x = m.x;
                m.target_y = m.y;
                emit_move_end(m, moves_out);
            }

            // --- Proximity aggro (compatibility behavior while resting) ---
            if (may_pursue && !m.aggro && m.aggressive && !m.moving && m.detect_cooldown <= 0.0f) {
                const int detect = m.detect_range > 0 ? m.detect_range : 160;
                const float detect_f = static_cast<float>(detect);
                const PlayerPose* closest = nullptr;
                float best_dist = detect_f + 1.0f;
                for (const PlayerPose& p : targets) {
                    if (p.aggro_immune || p.map != m.map) {
                        continue;
                    }
                    const float dx = static_cast<float>(p.x - m.x);
                    const float dy = static_cast<float>(p.y - m.y);
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= detect_f && dist < best_dist) {
                        best_dist = dist;
                        closest = &p;
                    }
                }
                if (closest) {
                    m.aggro = true;
                    m.aggro_player_id = closest->id;
                    m.ai = MonsterEntity::Ai::Attack;
                    m.idle_left = 0.0f;
                    prey = closest;
                }
            }

            // --- Combat FSM (compatibility behavior / Move) ---
            // Chase only re-evaluates after runtime state (move_busy). Interrupting a
            // live run because the server already snapped into range aborts the client
            // walk every step. After a committed swing: still in sight → AIS_CHASE
            // (run); out of sight → AIS_ROAM.
            if (may_pursue && m.aggro && prey) {
                const MonsterDef* def = find_def(m.def_index);
                const float atk_range_f = static_cast<float>(atk_range);
                const float sight_f = static_cast<float>(sight);
                auto resume_chase = [&]() {
                    m.ai = MonsterEntity::Ai::Chase;
                    if (m.moving) {
                        return;
                    }
                    m.idle_left -= dt;
                    if (m.idle_left > 0.0f) {
                        return;
                    }
                    if (!pick_chase_target(m, prey->x, prey->y, players)) {
                        if (!monster_spawnable_pixel(prey->x, prey->y)) {
                            clear_combat(m);
                        } else {
                            // A packed group can temporarily occupy every route. Keep
                            // the target and earned damage while retrying the chase.
                            m.path_waypoints.clear();
                            m.idle_left = 0.25f;
                        }
                        emit_move_end(m, moves_out);
                    } else if (m.dirty_move && m.moving) {
                        emit_move_on(m, moves_out);
                    } else if (!m.moving) {
                        m.idle_left = 0.15f;
                    }
                };

                if (dist_to_prey() > sight_f && !m.attack_swinging) {
                    clear_combat(m);
                } else {
                    if (m.ai == MonsterEntity::Ai::Roam) {
                        m.ai = MonsterEntity::Ai::Attack;
                    }

                    bool skill_fired = false;
                    if (def && m.pending_skill_slot >= 0 &&
                        static_cast<std::size_t>(m.pending_skill_slot) < def->skills.size() &&
                        m.skill_precast_left <= 0.0f) {
                        const MonsterSkillSlot& slot =
                            def->skills[static_cast<std::size_t>(m.pending_skill_slot)];
                        if (dist_to_prey() <= std::min(sight_f, 170.0f)) {
                            MonsterAttackEvent ev;
                            ev.kind = MonsterAttackEvent::Kind::SkillFire;
                            ev.attacker_id = m.id;
                            ev.target_player_id = prey->id;
                            ev.skill_index = static_cast<std::uint8_t>(slot.index);
                            ev.skill_level =
                                static_cast<std::uint8_t>(std::clamp(slot.level, 1, 255));
                            attacks_out.push_back(ev);
                            m.attack_cooldown = std::max(m.attack_cooldown, 1.4f);
                            skill_fired = true;
                        }
                        m.pending_skill_slot = -1;
                    }

                    const bool has_skills = def && !def->skills.empty() &&
                                            monster_skill_ai(*def) != MonsterSkillAi::None;
                    const bool can_start_skill = has_skills && m.pending_skill_slot < 0 &&
                                                 m.skill_cooldown <= 0.0f && !m.attack_swinging &&
                                                 !m.moving && dist_to_prey() <= atk_range_f &&
                                                 !finished_move;
                    if (can_start_skill) {
                        const std::size_t slot_index = m.next_skill_slot % def->skills.size();
                        const MonsterSkillSlot& slot = def->skills[slot_index];
                        m.next_skill_slot = (slot_index + 1) % def->skills.size();
                        int precast_ms = 0;
                        int delay_ms = 0;
                        for (const SkillDef& skill : m_skill_defs) {
                            if (skill.clazz == -2 && skill.index == slot.index) {
                                precast_ms = std::max(0, skill.delay_pre_ms);
                                delay_ms = std::max(0, skill.delay_ms);
                                break;
                            }
                        }
                        m.pending_skill_slot = static_cast<int>(slot_index);
                        m.skill_precast_left = static_cast<float>(precast_ms) / 1000.0f;
                        m.skill_cooldown =
                            std::max(2.0f, static_cast<float>(precast_ms + delay_ms) / 1000.0f);
                        m.moving = false;
                        m.move_busy_left = 0.0f;
                        emit_move_end(m, moves_out);
                        m.dir = dir_from_delta(prey->x - m.x, prey->y - m.y);
                        MonsterAttackEvent ev;
                        ev.kind = MonsterAttackEvent::Kind::SkillPrepare;
                        ev.attacker_id = m.id;
                        ev.target_player_id = prey->id;
                        ev.skill_index = static_cast<std::uint8_t>(slot.index);
                        ev.skill_level = static_cast<std::uint8_t>(std::clamp(slot.level, 1, 255));
                        attacks_out.push_back(ev);
                    }

                    // AI(): Move() only sets AIS_ATTACK when a chase step
                    // finishes in range; Attack() runs on the next tick. Swinging in
                    // the same tick replayed the clip while the client was still
                    // interpolating the run.
                    if (!skill_fired && m.pending_skill_slot < 0 && !m.attack_swinging &&
                        !m.moving && dist_to_prey() <= atk_range_f && m.attack_cooldown <= 0.0f &&
                        !finished_move && (!def || !skills_replace_basic_attack(*def))) {
                        m.moving = false;
                        m.move_busy_left = 0.0f;
                        emit_move_end(m, moves_out);
                        begin_melee_swing(m, find_def(m.def_index), *prey, attacks_out);
                    } else if (!m.attack_swinging && finished_move &&
                               dist_to_prey() <= atk_range_f) {
                        m.ai = MonsterEntity::Ai::Attack;
                        emit_move_end(m, moves_out);
                    }

                    if (m.attack_swinging) {
                        m.moving = false;
                        m.move_busy_left = 0.0f;
                        m.dir = dir_from_delta(prey->x - m.x, prey->y - m.y);
                        if (m.attack_cooldown <= 0.0f) {
                            m.attack_swinging = false;
                            if (dist_to_prey() > sight_f) {
                                clear_combat(m);
                            } else if (dist_to_prey() > atk_range_f) {
                                m.idle_left = 0.0f;
                                m.path_waypoints.clear();
                                resume_chase();
                            }
                        }
                    } else if (m.aggro && prey && dist_to_prey() > atk_range_f) {
                        resume_chase();
                    }
                }
            } else if (m.aggro && !may_pursue) {
                clear_combat(m);
            }

            // --- Roam (only combat races, and only when not in combat) ---
            if (may_roam && !m.aggro && m.ai == MonsterEntity::Ai::Roam && !m.moving) {
                m.idle_left -= dt;
                if (m.idle_left <= 0.0f) {
                    pick_roam_target(m, players);
                    if (m.dirty_move && m.moving) {
                        emit_move_on(m, moves_out);
                    }
                }
            }
        }
    }

    GroundItem& GameWorld::spawn_ground_item(std::uint32_t iid,
                                             const DropOutcome& drop,
                                             std::uint32_t owner_player_id,
                                             float ownership_sec,
                                             float despawn_sec,
                                             int map,
                                             std::uint32_t owner_party_id,
                                             bool server_owned) {
        GroundItem item;
        item.id = iid;
        item.item_index = drop.item_index;
        item.prefix = drop.prefix;
        item.count = drop.count > 0 ? drop.count : 1;
        item.x = drop.x;
        item.y = drop.y;
        item.map = map;
        item.owner_player_id = owner_player_id;
        item.owner_party_id = owner_party_id;
        item.server_owned = server_owned;
        item.ownership_left =
            owner_player_id != 0 || owner_party_id != 0 || server_owned ? ownership_sec : 0.0f;
        item.despawn_left = despawn_sec > 0.0f ? despawn_sec : 180.0f;
        m_ground_items.push_back(item);
        return m_ground_items.back();
    }

    bool GameWorld::remove_ground_item(std::uint32_t id) {
        const auto it = std::remove_if(m_ground_items.begin(),
                                       m_ground_items.end(),
                                       [id](const GroundItem& g) { return g.id == id; });
        if (it == m_ground_items.end()) {
            return false;
        }
        m_ground_items.erase(it, m_ground_items.end());
        return true;
    }

    GroundItem* GameWorld::find_ground_item(std::uint32_t id) {
        for (auto& g : m_ground_items) {
            if (g.id == id) {
                return &g;
            }
        }
        return nullptr;
    }

    const GroundItem* GameWorld::find_ground_item(std::uint32_t id) const {
        for (const auto& g : m_ground_items) {
            if (g.id == id) {
                return &g;
            }
        }
        return nullptr;
    }

    void GameWorld::tick_ground_items(float dt, std::vector<std::uint32_t>& removed_ids) {
        removed_ids.clear();
        for (auto& g : m_ground_items) {
            if (g.ownership_left > 0.0f) {
                g.ownership_left -= dt;
                if (g.ownership_left <= 0.0f) {
                    g.ownership_left = 0.0f;
                    g.owner_player_id = 0;
                    g.owner_party_id = 0;
                    g.server_owned = false;
                }
            }
            g.despawn_left -= dt;
        }
        auto it = m_ground_items.begin();
        while (it != m_ground_items.end()) {
            if (it->despawn_left <= 0.0f) {
                removed_ids.push_back(it->id);
                it = m_ground_items.erase(it);
            } else {
                ++it;
            }
        }
    }

}  // namespace openkal::server
