/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/QuestDb.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>

namespace openkal::server {
    namespace {

        [[nodiscard]] QuestCondition parse_condition(const platform::Json& j) {
            QuestCondition c;
            c.op = j.at("op").as_string();
            if (c.op == "class") {
                c.a = j.at("class").as_int(0);
            } else if (c.op == "level") {
                c.a = j.at("level").as_int(0);
            } else if (c.op == "specialty") {
                c.a = j.at("class").as_int(0);
                c.b = j.at("specialty").as_int(0);
            } else if (c.op == "contribute" || c.op == "party") {
                c.a = j.at(c.op).as_int(0);
            } else if (c.op == "quest") {
                c.a = j.at("quest").as_int(0);
                c.b = j.at("flag").as_int(0);
            } else if (c.op == "clear" || c.op == "notclear") {
                c.a = j.at("quest").as_int(0);
            } else if (c.op == "gstate") {
                c.a = j.at("gstate").as_int(0);
                c.b = j.at("value").as_int(0);
            } else if (c.op == "item") {
                const platform::Json& items = j.at("items");
                if (items.is_array()) {
                    for (const auto& row : items.array()) {
                        if (!row.is_array() || row.array().size() < 2) {
                            continue;
                        }
                        c.items.emplace_back(row.array()[0].as_int(0), row.array()[1].as_int(0));
                    }
                }
            }
            return c;
        }

        [[nodiscard]] QuestAction parse_action(const platform::Json& j) {
            QuestAction a;
            a.op = j.at("op").as_string();
            if (j.at("raw").is_array()) {
                a.op = "unsupported:" + a.op;
                return a;
            }
            if (a.op == "html") {
                a.a = j.at("id").as_int(0);
            } else if (a.op == "link" || a.op == "save" || a.op == "clear") {
                a.a = j.at("quest").as_int(0);
                a.b = j.at("flag").as_int(0);
            } else if (a.op == "exp" || a.op == "contribute" || a.op == "supoint" ||
                       a.op == "specialty" || a.op == "guide" || a.op == "itemrand") {
                a.a = j.at(a.op == "itemrand" ? "group" : a.op).as_int(0);
            } else if (a.op == "gstate") {
                a.a = j.at("gstate").as_int(0);
                a.b = j.at("value").as_int(0);
            } else if (a.op == "teleport") {
                a.a = j.at("map").as_int(0);
                a.b = j.at("x").as_int(0);
                a.c = j.at("y").as_int(0);
            } else if (a.op == "item") {
                const platform::Json& outs = j.at("out");
                if (outs.is_array()) {
                    for (const auto& row : outs.array()) {
                        if (!row.is_array() || row.array().size() < 2) {
                            continue;
                        }
                        a.item_out.push_back({row.array()[0].as_int(0), row.array()[1].as_int(0)});
                    }
                }
                const platform::Json& ins = j.at("in");
                if (ins.is_array()) {
                    for (const auto& row : ins.array()) {
                        if (!row.is_array() || row.array().size() < 3) {
                            continue;
                        }
                        std::vector<int> item = {row.array()[0].as_int(0),
                                                 row.array()[1].as_int(0),
                                                 row.array()[2].as_int(0)};
                        if (row.array().size() >= 4) {
                            item.push_back(row.array()[3].as_int(0));
                        }
                        a.item_in.push_back(std::move(item));
                    }
                }
            }
            return a;
        }

        [[nodiscard]] bool parse_quest_json(const platform::Json& root, QuestStep& out) {
            const int index = root.at("index").as_int(-1);
            const int flag = root.at("flag").as_int(-1);
            if (index < 0 || index > 65535 || flag < 0 || flag > 255) {
                return false;
            }
            out.index = static_cast<std::uint16_t>(root.at("index").as_int(0));
            out.flag = static_cast<std::uint8_t>(root.at("flag").as_int(0));
            out.linked = root.at("linked").boolean();
            out.cases.clear();
            const platform::Json& cases = root.at("cases");
            if (!cases.is_array()) {
                return true;
            }
            for (const auto& cj : cases.array()) {
                if (!cj.is_object()) {
                    continue;
                }
                QuestCase qc;
                const platform::Json& conds = cj.at("if");
                if (conds.is_array()) {
                    for (const auto& cond : conds.array()) {
                        if (cond.is_object()) {
                            qc.conditions.push_back(parse_condition(cond));
                        }
                    }
                }
                const platform::Json& actions = cj.at("then");
                if (actions.is_array()) {
                    for (const auto& act : actions.array()) {
                        if (act.is_object()) {
                            qc.actions.push_back(parse_action(act));
                        }
                    }
                }
                out.cases.push_back(std::move(qc));
            }
            return true;
        }

    }  // namespace

    bool QuestDb::load_dir(const std::string& dir) {
        m_steps.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        const auto files = platform::list_directory(dir);
        for (const auto& name : files) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("QuestDb: parse failed " + path + " (" + err + ")");
                continue;
            }
            QuestStep step;
            if (!parse_quest_json(root, step)) {
                log::warn("QuestDb: invalid quest index or flag in " + path);
                continue;
            }
            const std::uint32_t key = protocol::Quest::make_key(step.index, step.flag);
            m_steps[key] = std::move(step);
        }
        log::info("QuestDb: loaded " + std::to_string(m_steps.size()) + " steps from " + dir);
        return !m_steps.empty();
    }

    bool QuestDb::load_task_levels_dir(const std::string& dir) {
        m_task_levels.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        for (const auto& name : platform::list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            std::string err;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("QuestDb: task level parse failed " + path + " (" + err + ")");
                continue;
            }
            const int quest = root.at("key").as_int(0);
            if (quest <= 0 || quest > 65535) {
                continue;
            }
            const platform::Json& flags = root.at("flags");
            if (!flags.is_array()) {
                continue;
            }
            for (const auto& flag_json : flags.array()) {
                if (!flag_json.is_object()) {
                    continue;
                }
                const int flag = flag_json.at("key").as_int(-1);
                const int level = flag_json.at("level").as_int(0);
                if (flag < 0 || flag > 255 || level <= 0) {
                    continue;
                }
                const auto key = protocol::Quest::make_key(static_cast<std::uint16_t>(quest),
                                                           static_cast<std::uint8_t>(flag));
                m_task_levels[key] = level;
            }
        }
        log::info("QuestDb: loaded " + std::to_string(m_task_levels.size()) +
                  " task level requirements from " + dir);
        return !m_task_levels.empty();
    }

    const QuestStep* QuestDb::find(std::uint32_t key) const {
        const auto it = m_steps.find(key);
        return it == m_steps.end() ? nullptr : &it->second;
    }

    const QuestStep* QuestDb::find(std::uint16_t index, std::uint8_t flag) const {
        return find(protocol::Quest::make_key(index, flag));
    }

    std::unordered_set<int> QuestDb::task_item_indices(const ItemGroupDb& drops) const {
        std::unordered_set<int> indices;
        for (const auto& entry : m_steps) {
            for (const auto& branch : entry.second.cases) {
                for (const auto& action : branch.actions) {
                    if (action.op != "item") {
                        continue;
                    }
                    for (const auto& grant : action.item_in) {
                        if (grant.size() >= 3 && grant[0] > 0 && grant[2] > 0) {
                            indices.insert(grant[0]);
                        }
                    }
                }
            }
        }
        for (const auto& group : drops.groups()) {
            for (const auto& entry : group.second.entries) {
                if (entry.kind == GroupEntry::Kind::Item && entry.chance > 0 && entry.count > 0) {
                    indices.erase(entry.item_index);
                }
            }
        }
        return indices;
    }

    int QuestDb::required_level(std::uint16_t quest) const {
        int task_min_lv = 0;
        for (const auto& [key, level] : m_task_levels) {
            if (protocol::Quest::key_quest(key) != quest || level <= 0) {
                continue;
            }
            if (task_min_lv == 0 || level < task_min_lv) {
                task_min_lv = level;
            }
        }
        if (task_min_lv > 0) {
            return task_min_lv;
        }

        int min_lv = 0;
        for (const auto& [_, step] : m_steps) {
            if (step.index != quest) {
                continue;
            }
            for (const auto& qc : step.cases) {
                for (const auto& c : qc.conditions) {
                    if (c.op != "level" || c.a <= 0) {
                        continue;
                    }
                    if (min_lv == 0 || c.a < min_lv) {
                        min_lv = c.a;
                    }
                }
            }
        }
        return min_lv;
    }

    int QuestDb::stage_required_level(std::uint16_t quest, std::uint8_t flag) const {
        const auto it = m_task_levels.find(protocol::Quest::make_key(quest, flag));
        return it == m_task_levels.end() ? 0 : it->second;
    }

}  // namespace openkal::server
