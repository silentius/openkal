/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/QuestEngine.h"

#include "openkal/platform/Log.h"

#include <algorithm>
#include <limits>
#include <string>

namespace openkal::server {
    namespace {

        void report_diagnostic(const QuestActionSink& sink,
                               std::uint32_t key,
                               const std::string& reason) {
            if (sink.report_diagnostic) {
                sink.report_diagnostic(key, reason);
            } else {
                log::warn("QuestEngine: " + reason + " key=" + std::to_string(key));
            }
        }

        [[nodiscard]] protocol::QuestFlagEntry lookup_flag(const PlayerQuestMap& quests,
                                                           std::uint16_t quest) {
            const auto it = quests.find(quest);
            if (it == quests.end()) {
                return protocol::QuestFlagEntry{quest, 0, 0};
            }
            return it->second;
        }

    }  // namespace

    QuestEngine::QuestEngine(const QuestDb& db) : m_db(db) {}

    bool QuestEngine::call_process(std::uint32_t key,
                                   PlayerQuestMap& quests,
                                   QuestActionSink& sink) const {
        const QuestStep* step = m_db.find(key);
        if (!step) {
            return false;
        }
        if (step->linked) {
            return false;
        }
        return process(key, quests, sink);
    }

    bool
    QuestEngine::process(std::uint32_t key, PlayerQuestMap& quests, QuestActionSink& sink) const {
        PlayerQuestMap simulated_quests = quests;
        QuestActionSink simulated_sink = sink;
        std::unordered_map<int, int> items;
        simulated_sink.count_item = [&](int index) {
            auto [it, inserted] = items.emplace(index, 0);
            if (inserted && sink.count_item) {
                it->second = sink.count_item(index);
            }
            return it->second;
        };
        std::vector<QuestAction> plan;
        if (!build_plan(key, simulated_quests, simulated_sink, items, plan, 0)) {
            return false;
        }
        const bool has_items = std::any_of(
            plan.begin(), plan.end(), [](const auto& action) { return action.op == "item"; });
        if (has_items && (!sink.exchange_items || !sink.exchange_items(plan))) {
            return false;
        }
        for (const auto& action : plan) {
            run_action(action, quests, sink);
        }
        return true;
    }

    bool QuestEngine::build_plan(std::uint32_t key,
                                 PlayerQuestMap& quests,
                                 QuestActionSink& sink,
                                 std::unordered_map<int, int>& items,
                                 std::vector<QuestAction>& plan,
                                 unsigned depth) const {
        const QuestStep* step = m_db.find(key);
        if (!step || depth >= 64) {
            report_diagnostic(sink, key, "missing or cyclic step");
            return false;
        }
        for (const auto& qc : step->cases) {
            if (!is_meet(qc.conditions, quests, sink)) {
                continue;
            }
            for (auto action : qc.actions) {
                if ((action.op == "link" || action.op == "save" || action.op == "clear") &&
                    (action.a < 0 || action.a > 65535 || action.b < 0 || action.b > 255)) {
                    return false;
                }
                if (action.op == "link") {
                    if (!build_plan(protocol::Quest::make_key(static_cast<std::uint16_t>(action.a),
                                                              static_cast<std::uint8_t>(action.b)),
                                    quests,
                                    sink,
                                    items,
                                    plan,
                                    depth + 1)) {
                        return false;
                    }
                    continue;
                }
                if (action.op == "itemrand") {
                    if (!sink.roll_items) {
                        return false;
                    }
                    action = sink.roll_items(action.a);
                    if (action.op != "item") {
                        return false;
                    }
                }
                if (action.op == "item") {
                    for (const auto& row : action.item_out) {
                        if (row.size() != 2 || row[1] <= 0 || sink.count_item(row[0]) < row[1]) {
                            return false;
                        }
                        items[row[0]] -= row[1];
                    }
                    for (const auto& row : action.item_in) {
                        if ((row.size() != 3 && row.size() != 4) || row[2] <= 0 ||
                            (row.size() == 4 && row[3] != 0 && row[3] != 1) ||
                            sink.count_item(row[0]) > std::numeric_limits<int>::max() - row[2]) {
                            return false;
                        }
                        items[row[0]] += row[2];
                    }
                } else if (action.op == "save" || action.op == "clear") {
                    const auto quest = static_cast<std::uint16_t>(action.a);
                    const auto flag = static_cast<std::uint8_t>(action.b);
                    const int required_level = m_db.stage_required_level(quest, flag);
                    if (required_level > 0 && sink.level < required_level) {
                        return false;
                    }
                    const auto previous = lookup_flag(quests, quest);
                    // A completed one-time quest must never be reopened by an old link.
                    if (previous.clear != 0) {
                        return false;
                    }
                    if (action.op == "save" && quests.find(quest) != quests.end() &&
                        action.b <= previous.flag) {
                        return false;
                    }
                    quests[quest] = {quest, flag, static_cast<std::uint8_t>(action.op == "clear")};
                } else if (action.op == "specialty") {
                    if (!sink.add_specialty || action.a < 0 || action.a >= 31) {
                        return false;
                    }
                    sink.specialty |= 1 << action.a;
                } else if (action.op == "gstate") {
                    if (!sink.set_gstate || action.a < 0 || action.a >= 32) {
                        return false;
                    }
                    const auto bit = 1u << static_cast<unsigned>(action.a);
                    sink.gstate = action.b ? sink.gstate | bit : sink.gstate & ~bit;
                } else if ((action.op == "exp" && sink.add_exp) ||
                           (action.op == "supoint" && sink.add_skill_points) ||
                           (action.op == "contribute" && sink.add_contribution) ||
                           (action.op == "teleport" && sink.teleport) ||
                           (action.op == "guide" && sink.show_guide) ||
                           (action.op == "redistribute" && sink.redistribute) ||
                           action.op == "html") {
                    // These callbacks run only after the inventory exchange succeeds.
                } else {
                    report_diagnostic(sink, key, "unsupported action " + action.op);
                    return false;
                }
                plan.push_back(std::move(action));
            }
            return true;
        }
        return false;
    }

    bool QuestEngine::is_meet(const std::vector<QuestCondition>& conditions,
                              const PlayerQuestMap& quests,
                              const QuestActionSink& sink) const {
        for (const auto& c : conditions) {
            if ((c.op == "quest" || c.op == "clear" || c.op == "notclear") &&
                (c.a < 0 || c.a > 65535 || (c.op == "quest" && (c.b < 0 || c.b > 255)))) {
                return false;
            }
            if (c.op == "class") {
                if (sink.clazz != c.a) {
                    return false;
                }
            } else if (c.op == "level") {
                if (sink.level < c.a) {
                    return false;
                }
            } else if (c.op == "quest") {
                // CheckQuestFlag compares nFlag only; bClear is ignored.
                const auto entry = lookup_flag(quests, static_cast<std::uint16_t>(c.a));
                if (entry.flag != static_cast<std::uint8_t>(c.b)) {
                    return false;
                }
            } else if (c.op == "clear") {
                const auto entry = lookup_flag(quests, static_cast<std::uint16_t>(c.a));
                if (entry.clear == 0) {
                    return false;
                }
            } else if (c.op == "notclear") {
                const auto entry = lookup_flag(quests, static_cast<std::uint16_t>(c.a));
                if (entry.clear != 0) {
                    return false;
                }
            } else if (c.op == "specialty") {
                if (sink.clazz != c.a) {
                    return false;
                }
                if (c.b < 0 || c.b >= 31) {
                    return false;
                }
                const unsigned bit = 1u << static_cast<unsigned>(c.b);
                if ((static_cast<unsigned>(sink.specialty) & bit) == 0) {
                    return false;
                }
            } else if (c.op == "item") {
                if (!sink.count_item) {
                    return false;
                }
                for (const auto& [index, count] : c.items) {
                    if (sink.count_item(index) < count) {
                        return false;
                    }
                }
            } else if (c.op == "gstate") {
                if (c.a < 0 || c.a >= 32) {
                    return false;
                }
                const bool set = (sink.gstate & (1u << static_cast<unsigned>(c.a))) != 0;
                if (static_cast<int>(set) != c.b) {
                    return false;
                }
            } else {
                // Unsupported conditions fail closed for safety except empty-known stubs.
                if (c.op == "party" || c.op == "contribute") {
                    return false;
                }
                return false;
            }
        }
        return true;
    }

    void QuestEngine::save_flag(std::uint16_t quest,
                                std::uint8_t flag,
                                std::uint8_t clear,
                                PlayerQuestMap& quests,
                                QuestActionSink& sink) const {
        protocol::QuestFlagEntry entry{quest, flag, clear};
        auto it = quests.find(quest);
        if (it != quests.end()) {
            if (flag <= it->second.flag && clear == it->second.clear) {
                log::warn("QuestEngine: already saved quest=" + std::to_string(quest) +
                          " flag=" + std::to_string(flag));
                return;
            }
            it->second = entry;
        } else {
            quests.emplace(quest, entry);
        }
        if (sink.send_save_quest) {
            sink.send_save_quest(entry);
        }
    }

    void QuestEngine::run_action(const QuestAction& action,
                                 PlayerQuestMap& quests,
                                 QuestActionSink& sink) const {
        if (action.op == "html") {
            if (sink.send_html) {
                sink.send_html(action.a);
            }
        } else if (action.op == "save") {
            save_flag(static_cast<std::uint16_t>(action.a),
                      static_cast<std::uint8_t>(action.b),
                      0,
                      quests,
                      sink);
        } else if (action.op == "clear") {
            save_flag(static_cast<std::uint16_t>(action.a),
                      static_cast<std::uint8_t>(action.b),
                      1,
                      quests,
                      sink);
        } else if (action.op == "exp") {
            if (sink.add_exp) {
                sink.add_exp(action.a);
            }
        } else if (action.op == "supoint") {
            sink.add_skill_points(action.a);
        } else if (action.op == "contribute") {
            sink.add_contribution(action.a);
        } else if (action.op == "guide") {
            sink.show_guide(action.a);
        } else if (action.op == "redistribute") {
            sink.redistribute();
        } else if (action.op == "specialty") {
            sink.add_specialty(action.a);
            sink.specialty |= 1 << action.a;
        } else if (action.op == "teleport") {
            if (sink.teleport) {
                sink.teleport(action.a, action.b, action.c);
            }
        } else if (action.op == "gstate") {
            const bool enabled = action.b != 0;
            if (sink.set_gstate) {
                sink.set_gstate(action.a, enabled);
            }
            if (action.a >= 0 && action.a < 32) {
                const std::uint32_t bit = 1u << static_cast<unsigned>(action.a);
                if (enabled) {
                    sink.gstate |= bit;
                } else {
                    sink.gstate &= ~bit;
                }
            }
        }
    }

}  // namespace openkal::server
