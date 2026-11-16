/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/QuestMarker.h"

#include "openkal/client/MonsterCatalog.h"
#include "openkal/client/TaskCatalog.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace openkal::client {
    namespace {

        constexpr float kPi = 3.14159265f;
        constexpr float kTwoPi = 6.2831853f;
        constexpr int kSphereSlices = 12;
        constexpr int kSphereStacks = 8;
        constexpr int kTubeSlices = 12;
        constexpr float kWorldScale = 0.5f;
        constexpr float kBobAmp = 0.55f;
        constexpr float kBobHz = 3.2f;
        constexpr float kPulseHz = 4.0f;
        constexpr int kMaxLinkDepth = 6;

        [[nodiscard]] protocol::QuestFlagEntry
        lookup_flag(const std::vector<protocol::QuestFlagEntry>& flags, std::uint16_t quest) {
            for (const auto& e : flags) {
                if (e.quest == quest) {
                    return e;
                }
            }
            return protocol::QuestFlagEntry{quest, 0, 0};
        }

        /** True when the hunt step's item quota is already in the bag. */
        [[nodiscard]] bool hunt_item_quota_met(const TaskCatalog* tasks,
                                               std::uint16_t quest,
                                               std::uint8_t flag,
                                               int (*count_item)(int, const void*),
                                               const void* count_ctx) {
            if (!tasks || !count_item) {
                return false;
            }
            const TaskDef* task = tasks->find(quest);
            if (!task) {
                return false;
            }
            for (const auto& f : task->flags) {
                if (f.key != flag) {
                    continue;
                }
                if (f.item_index <= 0 || f.item_count <= 0) {
                    return false;
                }
                return count_item(f.item_index, count_ctx) >= f.item_count;
            }
            return false;
        }

        [[nodiscard]] bool conditions_met(const std::vector<server::QuestCondition>& conditions,
                                          const std::vector<protocol::QuestFlagEntry>& flags,
                                          int player_level,
                                          int player_clazz) {
            for (const auto& c : conditions) {
                if (c.op == "class") {
                    if (player_clazz != c.a) {
                        return false;
                    }
                } else if (c.op == "level") {
                    if (player_level < c.a) {
                        return false;
                    }
                } else if (c.op == "quest") {
                    const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(c.a));
                    if (entry.clear != 0 || entry.flag != static_cast<std::uint8_t>(c.b)) {
                        return false;
                    }
                } else if (c.op == "clear") {
                    const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(c.a));
                    if (entry.clear == 0) {
                        return false;
                    }
                } else if (c.op == "notclear") {
                    const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(c.a));
                    if (entry.clear != 0) {
                        return false;
                    }
                }
                // Item / party / gstate / etc. are ignored so markers stay visible.
            }
            return true;
        }

        /** True when the active task stage has the same fulfilled item requirement as the journal.
         */
        [[nodiscard]] bool quest_stage_item_quota_met(const protocol::QuestFlagEntry& entry,
                                                      const TaskCatalog* tasks,
                                                      int (*count_item)(int, const void*),
                                                      const void* count_ctx) {
            if (!tasks || !count_item || entry.clear != 0) {
                return false;
            }
            const TaskDef* task = tasks->find(entry.quest);
            if (!task) {
                return false;
            }
            for (const auto& flag : task->flags) {
                if (flag.key != entry.flag) {
                    continue;
                }
                return flag.item_index > 0 && flag.item_count > 0 &&
                       count_item(flag.item_index, count_ctx) >= flag.item_count;
            }
            return false;
        }

        /** True when this NPC case references a completed, active journal task stage. */
        [[nodiscard]] bool
        case_has_completed_task_stage(const server::QuestCase& qc,
                                      const std::vector<protocol::QuestFlagEntry>& flags,
                                      const TaskCatalog* tasks,
                                      int (*count_item)(int, const void*),
                                      const void* count_ctx) {
            for (const auto& condition : qc.conditions) {
                if (condition.op != "quest" && condition.op != "notclear") {
                    continue;
                }
                const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(condition.a));
                if (entry.clear != 0 || entry.flag == 0) {
                    continue;
                }
                if (condition.op == "quest" &&
                    entry.flag != static_cast<std::uint8_t>(condition.b)) {
                    continue;
                }
                if (quest_stage_item_quota_met(entry, tasks, count_item, count_ctx)) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool dialog_has_quest_link(std::int32_t html_id,
                                                 const DialogCatalog& dialogs) {
            std::vector<std::int32_t> pending{html_id};
            std::unordered_set<std::int32_t> visited;
            while (!pending.empty() && visited.size() < 64) {
                const auto id = pending.back();
                pending.pop_back();
                if (!visited.insert(id).second) {
                    continue;
                }
                const auto* dlg = dialogs.find(id);
                if (!dlg) {
                    continue;
                }
                for (const auto& link : dlg->links) {
                    if (link.type == "quest") {
                        return true;
                    }
                    std::int32_t target = 0;
                    const auto parsed = std::from_chars(
                        link.type.data(), link.type.data() + link.type.size(), target);
                    if (parsed.ec == std::errc{} &&
                        parsed.ptr == link.type.data() + link.type.size() && target > 0) {
                        pending.push_back(target);
                    }
                }
            }
            return false;
        }

        /** True when the loaded dialog exists and has no quest button. Missing scripts stay open.
         */
        [[nodiscard]] bool dialog_is_close_only(std::int32_t html_id,
                                                const DialogCatalog& dialogs) {
            return dialogs.find(html_id) != nullptr && !dialog_has_quest_link(html_id, dialogs);
        }

        /** True when the case only forwards to another step (event NPCs with a NewPlayer flag). */
        [[nodiscard]] bool case_is_link_only(const server::QuestCase& qc) {
            bool any_link = false;
            for (const auto& action : qc.actions) {
                if (action.op == "link") {
                    any_link = true;
                    continue;
                }
                if (action.op == "html" || action.op == "save" || action.op == "clear") {
                    return false;
                }
            }
            return any_link;
        }

        /**
         * Event-job offers insert flag 1 before the first talk. Treat a matching save-up as
         * still available when the player has not started that quest yet.
         */
        [[nodiscard]] bool
        case_is_unstarted_offer(const server::QuestCase& qc,
                                const std::vector<protocol::QuestFlagEntry>& flags) {
            int save_quest = 0;
            int save_flag = 0;
            bool has_html = false;
            for (const auto& action : qc.actions) {
                if (action.op == "html") {
                    has_html = true;
                } else if (action.op == "save") {
                    save_quest = action.a;
                    save_flag = action.b;
                }
            }
            if (!has_html || save_quest <= 0) {
                return false;
            }
            for (const auto& c : qc.conditions) {
                if (c.op != "quest" || c.a != save_quest || c.b <= 0 || save_flag <= c.b) {
                    continue;
                }
                const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(c.a));
                if (entry.clear == 0 && entry.flag == 0) {
                    return true;
                }
            }
            return false;
        }

        /** True when this talk saves a higher flag of the same quest (first accept, not turn-in).
         */
        [[nodiscard]] bool
        case_saves_quest_advance(const server::QuestCase& qc, int quest, int flag) {
            for (const auto& action : qc.actions) {
                if (action.op == "save" && action.a == quest && action.b > flag) {
                    return true;
                }
            }
            return false;
        }

        QuestMarkerKind kind_for_step(std::uint32_t key,
                                      int player_level,
                                      int player_clazz,
                                      const std::vector<protocol::QuestFlagEntry>& flags,
                                      const server::QuestDb& quests,
                                      const DialogCatalog& dialogs,
                                      const TaskCatalog* tasks,
                                      int (*count_item)(int, const void*),
                                      const void* count_ctx,
                                      std::unordered_set<std::uint32_t>& visited,
                                      int depth);

        [[nodiscard]] std::int32_t
        resolve_terminal_html_id(std::uint32_t key,
                                 int player_level,
                                 int player_clazz,
                                 const std::vector<protocol::QuestFlagEntry>& flags,
                                 const server::QuestDb& quests,
                                 const DialogCatalog& dialogs,
                                 std::unordered_set<std::uint32_t>& visited,
                                 int depth) {
            if (key == 0 || depth > kMaxLinkDepth || visited.count(key) != 0) {
                return 0;
            }
            const server::QuestStep* step = quests.find(key);
            if (!step) {
                return 0;
            }
            visited.insert(key);
            for (const auto& qc : step->cases) {
                if (!conditions_met(qc.conditions, flags, player_level, player_clazz)) {
                    continue;
                }
                for (const auto& action : qc.actions) {
                    if (action.op == "html") {
                        visited.erase(key);
                        return action.a;
                    }
                    if (action.op == "link") {
                        const std::uint32_t linked =
                            protocol::Quest::make_key(static_cast<std::uint16_t>(action.a),
                                                      static_cast<std::uint8_t>(action.b));
                        const std::int32_t linked_html = resolve_terminal_html_id(linked,
                                                                                  player_level,
                                                                                  player_clazz,
                                                                                  flags,
                                                                                  quests,
                                                                                  dialogs,
                                                                                  visited,
                                                                                  depth + 1);
                        if (linked_html != 0) {
                            visited.erase(key);
                            return linked_html;
                        }
                    }
                }
            }
            visited.erase(key);
            return 0;
        }

        [[nodiscard]] QuestMarkerKind
        interpret_case(const server::QuestCase& qc,
                       int player_level,
                       int player_clazz,
                       const std::vector<protocol::QuestFlagEntry>& flags,
                       const server::QuestDb& quests,
                       const DialogCatalog& dialogs,
                       const TaskCatalog* tasks,
                       int (*count_item)(int, const void*),
                       const void* count_ctx,
                       std::unordered_set<std::uint32_t>& visited,
                       int depth,
                       std::uint8_t step_flag) {
            QuestMarkerKind linked_kind = QuestMarkerKind::None;
            for (const auto& action : qc.actions) {
                if (action.op != "link") {
                    continue;
                }
                const std::uint32_t linked = protocol::Quest::make_key(
                    static_cast<std::uint16_t>(action.a), static_cast<std::uint8_t>(action.b));
                const QuestMarkerKind k = kind_for_step(linked,
                                                        player_level,
                                                        player_clazz,
                                                        flags,
                                                        quests,
                                                        dialogs,
                                                        tasks,
                                                        count_item,
                                                        count_ctx,
                                                        visited,
                                                        depth + 1);
                if (k == QuestMarkerKind::ReadyToTurnIn) {
                    return QuestMarkerKind::ReadyToTurnIn;
                }
                if (k == QuestMarkerKind::Accepted) {
                    return QuestMarkerKind::Accepted;
                }
                if (k == QuestMarkerKind::Available) {
                    linked_kind = QuestMarkerKind::Available;
                }
            }
            if (linked_kind != QuestMarkerKind::None) {
                return linked_kind;
            }

            bool accepted = false;
            bool ready_to_turn_in = false;
            bool available = false;
            std::uint16_t available_quest = 0;
            for (const auto& c : qc.conditions) {
                if (c.op == "notclear") {
                    const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(c.a));
                    if (entry.clear != 0) {
                        continue;
                    }
                    if (entry.flag > 0) {
                        accepted = true;
                    } else {
                        available = true;
                        available_quest = static_cast<std::uint16_t>(c.a);
                    }
                } else if (c.op == "quest") {
                    if (c.b == 0 || (c.b == static_cast<int>(step_flag) &&
                                     case_saves_quest_advance(qc, c.a, c.b))) {
                        available = true;
                        available_quest = static_cast<std::uint16_t>(c.a);
                    } else {
                        accepted = true;
                    }
                }
            }
            for (const auto& action : qc.actions) {
                if (action.op == "clear") {
                    const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(action.a));
                    if (entry.clear == 0 && entry.flag > 0) {
                        accepted = true;
                        ready_to_turn_in =
                            quest_stage_item_quota_met(entry, tasks, count_item, count_ctx);
                    }
                    continue;
                }
                if (action.op != "save") {
                    continue;
                }
                const auto entry = lookup_flag(flags, static_cast<std::uint16_t>(action.a));
                if (entry.clear != 0 || entry.flag == 0) {
                    continue;
                }
                bool first_talk_offer = false;
                for (const auto& c : qc.conditions) {
                    if (c.op == "quest" && c.a == action.a && c.b == static_cast<int>(step_flag) &&
                        action.b > c.b) {
                        first_talk_offer = true;
                        break;
                    }
                }
                if (!first_talk_offer) {
                    accepted = true;
                }
            }
            if (ready_to_turn_in || (accepted && case_has_completed_task_stage(
                                                     qc, flags, tasks, count_item, count_ctx))) {
                return QuestMarkerKind::ReadyToTurnIn;
            }
            if (accepted) {
                return QuestMarkerKind::Accepted;
            }
            if (available) {
                const int need = quests.required_level(available_quest);
                if (need > 0 && player_level < need) {
                    return QuestMarkerKind::None;
                }
                return QuestMarkerKind::Available;
            }
            return QuestMarkerKind::None;
        }

        QuestMarkerKind kind_for_step(std::uint32_t key,
                                      int player_level,
                                      int player_clazz,
                                      const std::vector<protocol::QuestFlagEntry>& flags,
                                      const server::QuestDb& quests,
                                      const DialogCatalog& dialogs,
                                      const TaskCatalog* tasks,
                                      int (*count_item)(int, const void*),
                                      const void* count_ctx,
                                      std::unordered_set<std::uint32_t>& visited,
                                      int depth) {
            if (key == 0 || depth > kMaxLinkDepth || visited.count(key) != 0) {
                return QuestMarkerKind::None;
            }
            const server::QuestStep* step = quests.find(key);
            if (!step) {
                return QuestMarkerKind::None;
            }
            visited.insert(key);
            for (const auto& qc : step->cases) {
                if (!conditions_met(qc.conditions, flags, player_level, player_clazz)) {
                    continue;
                }
                QuestMarkerKind k = interpret_case(qc,
                                                   player_level,
                                                   player_clazz,
                                                   flags,
                                                   quests,
                                                   dialogs,
                                                   tasks,
                                                   count_item,
                                                   count_ctx,
                                                   visited,
                                                   depth,
                                                   step->flag);
                bool advances_on_talk = false;
                for (const auto& action : qc.actions) {
                    if (action.op == "save" &&
                        action.b > lookup_flag(flags, static_cast<std::uint16_t>(action.a)).flag) {
                        advances_on_talk = true;
                    }
                }
                if (k == QuestMarkerKind::None && case_is_link_only(qc)) {
                    for (const auto& offer : step->cases) {
                        if (!case_is_unstarted_offer(offer, flags)) {
                            continue;
                        }
                        const int need = quests.required_level(step->index);
                        if (need > 0 && player_level < need) {
                            visited.erase(key);
                            return QuestMarkerKind::None;
                        }
                        k = QuestMarkerKind::Available;
                        advances_on_talk = true;
                        break;
                    }
                }
                visited.erase(key);
                if (k == QuestMarkerKind::Available && !advances_on_talk) {
                    std::int32_t terminal_html = resolve_terminal_html_id(key,
                                                                          player_level,
                                                                          player_clazz,
                                                                          flags,
                                                                          quests,
                                                                          dialogs,
                                                                          visited,
                                                                          depth + 1);
                    if (terminal_html == 0 || dialog_is_close_only(terminal_html, dialogs)) {
                        return QuestMarkerKind::None;
                    }
                }
                return k;
            }
            visited.erase(key);
            return QuestMarkerKind::None;
        }

        void append_sphere(std::vector<engine::VertexP3T2>& verts,
                           std::vector<std::uint32_t>& idx,
                           float cx,
                           float cy,
                           float cz,
                           float radius) {
            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const int cols = kSphereSlices + 1;
            for (int y = 0; y <= kSphereStacks; ++y) {
                const float v = static_cast<float>(y) / static_cast<float>(kSphereStacks);
                const float phi = v * kPi;
                const float sp = std::sin(phi);
                const float cp = std::cos(phi);
                for (int x = 0; x <= kSphereSlices; ++x) {
                    const float u = static_cast<float>(x) / static_cast<float>(kSphereSlices);
                    const float theta = u * kTwoPi;
                    const float st = std::sin(theta);
                    const float ct = std::cos(theta);
                    verts.push_back(
                        {cx + radius * sp * ct, cy + radius * cp, cz + radius * sp * st, u, v});
                }
            }
            for (int y = 0; y < kSphereStacks; ++y) {
                for (int x = 0; x < kSphereSlices; ++x) {
                    const std::uint32_t i0 = base + static_cast<std::uint32_t>(y * cols + x);
                    const std::uint32_t i1 = i0 + 1;
                    const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(cols);
                    const std::uint32_t i3 = i2 + 1;
                    idx.push_back(i0);
                    idx.push_back(i2);
                    idx.push_back(i1);
                    idx.push_back(i1);
                    idx.push_back(i2);
                    idx.push_back(i3);
                }
            }
        }

        void append_capsule(std::vector<engine::VertexP3T2>& verts,
                            std::vector<std::uint32_t>& idx,
                            float x0,
                            float y0,
                            float z0,
                            float x1,
                            float y1,
                            float z1,
                            float radius) {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float dz = z1 - z0;
            const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len < 1e-4f) {
                append_sphere(verts, idx, x0, y0, z0, radius);
                return;
            }
            const float ax = dx / len;
            const float ay = dy / len;
            const float az = dz / len;
            float bx = 0.0f;
            float by = 1.0f;
            float bz = 0.0f;
            if (std::fabs(ay) > 0.9f) {
                bx = 1.0f;
                by = 0.0f;
            }
            float px = by * az - bz * ay;
            float py = bz * ax - bx * az;
            float pz = bx * ay - by * ax;
            const float plen = std::sqrt(px * px + py * py + pz * pz);
            px /= plen;
            py /= plen;
            pz /= plen;
            const float qx = ay * pz - az * py;
            const float qy = az * px - ax * pz;
            const float qz = ax * py - ay * px;

            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const int rings = 2;
            for (int ring = 0; ring < rings; ++ring) {
                const float t = static_cast<float>(ring);
                const float cx = x0 + ax * len * t;
                const float cy = y0 + ay * len * t;
                const float cz = z0 + az * len * t;
                for (int i = 0; i <= kTubeSlices; ++i) {
                    const float u = static_cast<float>(i) / static_cast<float>(kTubeSlices);
                    const float a = u * kTwoPi;
                    const float ca = std::cos(a);
                    const float sa = std::sin(a);
                    verts.push_back({cx + (px * ca + qx * sa) * radius,
                                     cy + (py * ca + qy * sa) * radius,
                                     cz + (pz * ca + qz * sa) * radius,
                                     u,
                                     t});
                }
            }
            const int cols = kTubeSlices + 1;
            for (int i = 0; i < kTubeSlices; ++i) {
                const std::uint32_t i0 = base + static_cast<std::uint32_t>(i);
                const std::uint32_t i1 = i0 + 1;
                const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(cols);
                const std::uint32_t i3 = i2 + 1;
                idx.push_back(i0);
                idx.push_back(i2);
                idx.push_back(i1);
                idx.push_back(i1);
                idx.push_back(i2);
                idx.push_back(i3);
            }
            append_sphere(verts, idx, x0, y0, z0, radius);
            append_sphere(verts, idx, x1, y1, z1, radius);
        }

        void tube_frame(const engine::Vec3& t,
                        float& px,
                        float& py,
                        float& pz,
                        float& qx,
                        float& qy,
                        float& qz) {
            // Prefer world Z as binormal so a planar XY stroke cannot flip.
            float bx = 0.0f;
            float by = 0.0f;
            float bz = 1.0f;
            if (std::fabs(t.z) > 0.9f) {
                bx = 1.0f;
                bz = 0.0f;
            }
            px = by * t.z - bz * t.y;
            py = bz * t.x - bx * t.z;
            pz = bx * t.y - by * t.x;
            const float plen = std::sqrt(px * px + py * py + pz * pz);
            if (plen < 1e-5f) {
                px = 1.0f;
                py = 0.0f;
                pz = 0.0f;
            } else {
                px /= plen;
                py /= plen;
                pz /= plen;
            }
            qx = t.y * pz - t.z * py;
            qy = t.z * px - t.x * pz;
            qz = t.x * py - t.y * px;
        }

        void append_tube(std::vector<engine::VertexP3T2>& verts,
                         std::vector<std::uint32_t>& idx,
                         const std::vector<engine::Vec3>& path,
                         float radius) {
            if (path.size() < 2) {
                return;
            }
            std::vector<engine::Vec3> tangents(path.size());
            for (std::size_t i = 0; i < path.size(); ++i) {
                if (i == 0) {
                    tangents[i] = {
                        path[1].x - path[0].x, path[1].y - path[0].y, path[1].z - path[0].z};
                } else if (i + 1 == path.size()) {
                    tangents[i] = {path[i].x - path[i - 1].x,
                                   path[i].y - path[i - 1].y,
                                   path[i].z - path[i - 1].z};
                } else {
                    tangents[i] = {path[i + 1].x - path[i - 1].x,
                                   path[i + 1].y - path[i - 1].y,
                                   path[i + 1].z - path[i - 1].z};
                }
                const float len =
                    std::sqrt(tangents[i].x * tangents[i].x + tangents[i].y * tangents[i].y +
                              tangents[i].z * tangents[i].z);
                if (len > 1e-5f) {
                    tangents[i].x /= len;
                    tangents[i].y /= len;
                    tangents[i].z /= len;
                }
            }

            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const int cols = kTubeSlices + 1;
            for (std::size_t p = 0; p < path.size(); ++p) {
                float px = 0.0f;
                float py = 0.0f;
                float pz = 0.0f;
                float qx = 0.0f;
                float qy = 0.0f;
                float qz = 0.0f;
                tube_frame(tangents[p], px, py, pz, qx, qy, qz);
                const float v = static_cast<float>(p) / static_cast<float>(path.size() - 1);
                for (int i = 0; i <= kTubeSlices; ++i) {
                    const float u = static_cast<float>(i) / static_cast<float>(kTubeSlices);
                    const float a = u * kTwoPi;
                    const float ca = std::cos(a);
                    const float sa = std::sin(a);
                    verts.push_back({path[p].x + (px * ca + qx * sa) * radius,
                                     path[p].y + (py * ca + qy * sa) * radius,
                                     path[p].z + (pz * ca + qz * sa) * radius,
                                     u,
                                     v});
                }
            }
            for (std::size_t p = 0; p + 1 < path.size(); ++p) {
                for (int i = 0; i < kTubeSlices; ++i) {
                    const std::uint32_t i0 =
                        base + static_cast<std::uint32_t>(p) * static_cast<std::uint32_t>(cols) +
                        static_cast<std::uint32_t>(i);
                    const std::uint32_t i1 = i0 + 1;
                    const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(cols);
                    const std::uint32_t i3 = i2 + 1;
                    idx.push_back(i0);
                    idx.push_back(i2);
                    idx.push_back(i1);
                    idx.push_back(i1);
                    idx.push_back(i2);
                    idx.push_back(i3);
                }
            }
        }

        void build_exclamation(std::vector<engine::VertexP3T2>& verts,
                               std::vector<std::uint32_t>& idx) {
            append_capsule(verts, idx, 0.0f, 3.3f, 0.0f, 0.0f, 10.4f, 0.0f, 0.82f);
            append_sphere(verts, idx, 0.0f, 1.15f, 0.0f, 0.92f);
        }

        void build_question(std::vector<engine::VertexP3T2>& verts,
                            std::vector<std::uint32_t>& idx) {
            std::vector<engine::Vec3> path;
            const float cx = 0.0f;
            const float cy = 6.7f;
            const float cr = 2.15f;
            const float radius = 0.78f;
            constexpr int kArc = 24;
            // Smooth hook only: left → top → right → down. The stem is a separate
            // capsule so the tube never kinks.
            for (int i = 0; i <= kArc; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(kArc);
                const float deg = 200.0f + t * (-80.0f - 200.0f);
                const float rad = deg * kPi / 180.0f;
                path.push_back({cx + cr * std::cos(rad), cy + cr * std::sin(rad), 0.0f});
            }
            append_tube(verts, idx, path, radius);
            append_sphere(verts, idx, path.front().x, path.front().y, path.front().z, radius);
            append_capsule(
                verts, idx, path.back().x, path.back().y, path.back().z, 0.0f, 2.85f, 0.0f, radius);
            append_sphere(verts, idx, 0.0f, 1.15f, 0.0f, 0.92f);
        }

        void draw_tinted(engine::IEngine& engine,
                         engine::MeshHandle mesh,
                         engine::TextureHandle tex,
                         const engine::Mat4& model,
                         float red,
                         float green,
                         float blue) {
            engine.draw_mesh_textured(mesh,
                                      tex,
                                      model,
                                      /*alpha_blend=*/false,
                                      /*two_sided=*/true,
                                      /*alpha_test=*/false,
                                      /*additive=*/false,
                                      0.0f,
                                      0.0f,
                                      red,
                                      green,
                                      blue,
                                      1.0f);
        }

    }  // namespace

    QuestMarkerKind QuestMarker::kind_for_npc(std::uint32_t quest_key,
                                              int player_level,
                                              int player_clazz,
                                              const std::vector<protocol::QuestFlagEntry>& flags,
                                              const server::QuestDb& quests,
                                              const DialogCatalog& dialogs,
                                              const TaskCatalog* tasks,
                                              int (*count_item)(int, const void*),
                                              const void* count_ctx) {
        std::unordered_set<std::uint32_t> visited;
        return kind_for_step(quest_key,
                             player_level,
                             player_clazz,
                             flags,
                             quests,
                             dialogs,
                             tasks,
                             count_item,
                             count_ctx,
                             visited,
                             0);
    }

    QuestMarkerKind
    QuestMarker::kind_for_monster(const std::vector<MonsterQuestSlot>& slots,
                                  const std::vector<protocol::QuestFlagEntry>& flags,
                                  const TaskCatalog* tasks,
                                  int (*count_item)(int item_index, const void* ctx),
                                  const void* count_ctx) {
        for (const auto& slot : slots) {
            if (slot.quest_index() == 0) {
                continue;
            }
            bool active = false;
            for (const auto& entry : flags) {
                if (entry.quest == slot.quest_index() && entry.flag == slot.flag() &&
                    entry.clear == 0) {
                    active = true;
                    break;
                }
            }
            if (!active) {
                continue;
            }
            if (hunt_item_quota_met(
                    tasks, slot.quest_index(), slot.flag(), count_item, count_ctx)) {
                continue;
            }
            return QuestMarkerKind::Available;
        }
        return QuestMarkerKind::None;
    }

    bool QuestMarker::init(engine::IEngine& engine) {
        destroy(engine);

        std::vector<engine::VertexP3T2> bang_verts;
        std::vector<std::uint32_t> bang_idx;
        build_exclamation(bang_verts, bang_idx);
        m_exclamation = engine.create_mesh(
            bang_verts.data(), bang_verts.size(), bang_idx.data(), bang_idx.size());

        std::vector<engine::VertexP3T2> q_verts;
        std::vector<std::uint32_t> q_idx;
        build_question(q_verts, q_idx);
        m_question = engine.create_mesh(q_verts.data(), q_verts.size(), q_idx.data(), q_idx.size());

        const std::uint8_t px[4] = {255, 255, 255, 255};
        m_texture = engine.create_texture_rgba8(px, 1, 1, true);
        m_own_texture = static_cast<bool>(m_texture);
        m_age = 0.0f;
        return static_cast<bool>(m_exclamation) && static_cast<bool>(m_question) &&
               static_cast<bool>(m_texture);
    }

    void QuestMarker::destroy(engine::IEngine& engine) {
        if (m_exclamation) {
            engine.destroy_mesh(m_exclamation);
            m_exclamation = {};
        }
        if (m_question) {
            engine.destroy_mesh(m_question);
            m_question = {};
        }
        if (m_own_texture && m_texture) {
            engine.destroy_texture(m_texture);
            m_texture = {};
            m_own_texture = false;
        }
        m_age = 0.0f;
    }

    void QuestMarker::tick(float dt) {
        m_age += dt;
    }

    void QuestMarker::draw(engine::IEngine& engine,
                           QuestMarkerKind kind,
                           float wx,
                           float wy,
                           float wz,
                           const engine::Vec3& cam_eye) const {
        if (kind == QuestMarkerKind::None || !m_texture) {
            return;
        }
        const engine::MeshHandle mesh =
            (kind == QuestMarkerKind::Available) ? m_exclamation : m_question;
        if (!mesh) {
            return;
        }
        const float dx = cam_eye.x - wx;
        const float dz = cam_eye.z - wz;
        const float yaw = std::atan2(dx, dz);
        const float hy = yaw * 0.5f;
        const auto R = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
        const float bob = kBobAmp * std::sin(m_age * kBobHz);
        const float pulse = kWorldScale * (0.94f + 0.06f * std::sin(m_age * kPulseHz));
        const auto S = engine::Mat4::scaling(pulse, pulse, pulse);
        const auto T = engine::Mat4::translation(wx, wy + bob, wz);
        const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
        const bool interactive =
            kind == QuestMarkerKind::Available || kind == QuestMarkerKind::ReadyToTurnIn;
        draw_tinted(engine,
                    mesh,
                    m_texture,
                    model,
                    1.0f,
                    interactive ? 0.86f : 1.0f,
                    interactive ? 0.12f : 1.0f);
    }

}  // namespace openkal::client
