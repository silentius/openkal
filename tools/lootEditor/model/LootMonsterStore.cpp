/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "model/LootMonsterStore.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <optional>
#include <sstream>

namespace {

    /** Skips ASCII whitespace.
     * @param text Source JSON text.
     * @param index Current index.
     * @return Index after whitespace.
     */
    [[nodiscard]] std::size_t skip_ws(const std::string& text, std::size_t index) {
        while (index < text.size()) {
            const char c = text[index];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
            ++index;
        }
        return index;
    }

    /** Returns the end index (exclusive) of a JSON string starting at a quote.
     * @param text Source JSON text.
     * @param quote_index Index of the opening quote.
     * @return End index after the closing quote, or nullopt on failure.
     */
    [[nodiscard]] std::optional<std::size_t> json_string_end(const std::string& text,
                                                             std::size_t quote_index) {
        if (quote_index >= text.size() || text[quote_index] != '"') {
            return std::nullopt;
        }
        for (std::size_t i = quote_index + 1; i < text.size(); ++i) {
            const char c = text[i];
            if (c == '\\') {
                if (i + 1 >= text.size()) {
                    return std::nullopt;
                }
                ++i;
                continue;
            }
            if (c == '"') {
                return i + 1;
            }
        }
        return std::nullopt;
    }

    /** Returns the end index (exclusive) of a JSON value.
     * @param text Source JSON text.
     * @param start Index of the first value character.
     * @return End index after the value, or nullopt on failure.
     */
    [[nodiscard]] std::optional<std::size_t> json_value_end(const std::string& text,
                                                            std::size_t start) {
        if (start >= text.size()) {
            return std::nullopt;
        }

        const char lead = text[start];
        if (lead == '"') {
            return json_string_end(text, start);
        }
        if (lead == '{') {
            const char open = '{';
            const char close = '}';
            std::size_t depth = 0;
            for (std::size_t i = start; i < text.size(); ++i) {
                const char c = text[i];
                if (c == '"') {
                    const auto end = json_string_end(text, i);
                    if (!end) {
                        return std::nullopt;
                    }
                    i = *end - 1;
                    continue;
                }
                if (c == open) {
                    ++depth;
                } else if (c == close) {
                    --depth;
                    if (depth == 0) {
                        return i + 1;
                    }
                }
            }
            return std::nullopt;
        }
        if (lead == '[') {
            const char open = '[';
            const char close = ']';
            std::size_t depth = 0;
            for (std::size_t i = start; i < text.size(); ++i) {
                const char c = text[i];
                if (c == '"') {
                    const auto end = json_string_end(text, i);
                    if (!end) {
                        return std::nullopt;
                    }
                    i = *end - 1;
                    continue;
                }
                if (c == open) {
                    ++depth;
                } else if (c == close) {
                    --depth;
                    if (depth == 0) {
                        return i + 1;
                    }
                }
            }
            return std::nullopt;
        }
        if (text.compare(start, 4, "true") == 0) {
            return start + 4;
        }
        if (text.compare(start, 5, "false") == 0) {
            return start + 5;
        }
        if (text.compare(start, 4, "null") == 0) {
            return start + 4;
        }

        std::size_t i = start;
        if (text[i] == '-') {
            ++i;
        }
        while (i < text.size()) {
            const char c = text[i];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' ||
                c == '-') {
                ++i;
                continue;
            }
            break;
        }
        return i > start ? std::optional<std::size_t>(i) : std::nullopt;
    }

    /** Span of the itemGroups JSON array value plus key line position. */
    struct ItemGroupsSpan {
        /** Index of the opening quote in "itemGroups". */
        std::size_t key_pos = 0;

        /** Start index of the array value ( '[' ). */
        std::size_t value_start = 0;

        /** End index after the array value. */
        std::size_t value_end = 0;
    };

    /** Finds the value span of stats.itemGroups in monster JSON text.
     * @param text Full monster JSON file text.
     * @return Array value span, or nullopt when not found.
     */
    [[nodiscard]] std::optional<ItemGroupsSpan>
    find_itemgroups_value_span(const std::string& text) {
        const std::string key = "\"itemGroups\"";
        std::size_t search_from = 0;
        while (search_from < text.size()) {
            const std::size_t key_pos = text.find(key, search_from);
            if (key_pos == std::string::npos) {
                return std::nullopt;
            }

            std::size_t index = key_pos + key.size();
            index = skip_ws(text, index);
            if (index >= text.size() || text[index] != ':') {
                search_from = key_pos + 1;
                continue;
            }
            index = skip_ws(text, index + 1);
            const std::optional<std::size_t> end = json_value_end(text, index);
            if (!end) {
                search_from = key_pos + 1;
                continue;
            }
            ItemGroupsSpan span;
            span.key_pos = key_pos;
            span.value_start = index;
            span.value_end = *end;
            return span;
        }
        return std::nullopt;
    }

    /** Returns indentation on the line containing a position.
     * @param text Source text.
     * @param position Index within the line.
     * @return Leading whitespace on that line.
     */
    [[nodiscard]] std::string line_indent_before(const std::string& text, std::size_t position) {
        const std::size_t line_start = text.rfind('\n', position);
        const std::size_t begin = line_start == std::string::npos ? 0 : line_start + 1;
        std::string indent;
        for (std::size_t i = begin; i < position && i < text.size(); ++i) {
            const char c = text[i];
            if (c == ' ' || c == '\t') {
                indent.push_back(c);
            } else {
                break;
            }
        }
        return indent;
    }

    /** Formats stats.itemGroups like monster_config_convert (compact inner rows).
     * @param item_groups Itemgroup assignments.
     * @param key_line_indent Indentation before the itemGroups key.
     * @return JSON array text only.
     */
    [[nodiscard]] std::string
    format_itemgroups_value(const std::vector<std::pair<int, int>>& item_groups,
                            const std::string& key_line_indent) {
        if (item_groups.empty()) {
            return "[]";
        }

        const std::string row_indent = key_line_indent + "  ";
        std::ostringstream out;
        out << "[\n";
        for (std::size_t i = 0; i < item_groups.size(); ++i) {
            out << row_indent << "[" << item_groups[i].first << ", " << item_groups[i].second
                << "]";
            if (i + 1 < item_groups.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << key_line_indent << "]";
        return out.str();
    }

    [[nodiscard]] bool
    parse_monster(const std::string& text, const std::string& path, MonsterRecord& out) {
        openkal::platform::Json root;
        std::string err;
        if (!openkal::platform::json_parse(text, root, &err) || !root.is_object()) {
            openkal::log::warn("LootMonsterStore: bad json " + path + ": " + err);
            return false;
        }

        out.index = root.at("index").as_int(0);
        if (out.index <= 0) {
            return false;
        }
        out.name = root.at("name").as_string("Monster" + std::to_string(out.index));
        out.level = 0;
        out.race = 0;
        out.item_groups.clear();
        out.file_path = path;
        out.raw_json = text;

        const openkal::platform::Json& stats = root.at("stats");
        if (stats.is_object()) {
            if (stats.get("level") != nullptr) {
                out.level = stats.at("level").as_int(0);
            }
            out.race = stats.at("race").as_int(0);
            const openkal::platform::Json& item_groups = stats.at("itemGroups");
            if (item_groups.is_array()) {
                for (const auto& row : item_groups.array()) {
                    if (!row.is_array() || row.array().size() < 2) {
                        continue;
                    }
                    const int ig = row.array()[0].as_int(0);
                    const int attempts = row.array()[1].as_int(0);
                    if (ig > 0 && attempts > 0) {
                        out.item_groups.emplace_back(ig, attempts);
                    }
                }
            }
        }

        // Many monsters only store the display/combat level under appearance.level
        // (from MonsterInfo), especially when monster definition has no (level …) field.
        if (out.level <= 0) {
            const openkal::platform::Json& appearance = root.at("appearance");
            if (appearance.is_object() && appearance.get("level") != nullptr) {
                out.level = appearance.at("level").as_int(0);
            }
        }
        if (out.level <= 0) {
            out.level = 1;
        }
        return true;
    }

    [[nodiscard]] bool write_monster_item_groups(const MonsterRecord& rec) {
        const std::optional<ItemGroupsSpan> span = find_itemgroups_value_span(rec.raw_json);
        if (!span) {
            openkal::log::error("LootMonsterStore: itemGroups not found in " + rec.file_path);
            return false;
        }

        const std::string key_indent = line_indent_before(rec.raw_json, span->key_pos);
        const std::string replacement = format_itemgroups_value(rec.item_groups, key_indent);

        std::string patched = rec.raw_json;
        patched.replace(span->value_start, span->value_end - span->value_start, replacement);
        if (!patched.empty() && patched.back() != '\n') {
            patched.push_back('\n');
        }
        return openkal::platform::write_file(rec.file_path, patched);
    }

}  // namespace

void LootMonsterStore::clear() {
    m_monsters.clear();
    m_dirty.clear();
}

int LootMonsterStore::load_dir(const std::string& dir) {
    clear();
    if (!openkal::platform::is_directory(dir)) {
        openkal::log::warn("LootMonsterStore: dir missing " + dir);
        return 0;
    }

    int loaded = 0;
    for (const auto& name : openkal::platform::list_directory(dir)) {
        if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
            continue;
        }
        if (name == "bones.json" || name == "manifest.json") {
            continue;
        }
        const std::string path = openkal::platform::join_path(dir, name);
        const auto bytes = openkal::platform::read_file(path);
        if (!bytes) {
            continue;
        }
        const std::string text(bytes->begin(), bytes->end());
        MonsterRecord rec;
        if (!parse_monster(text, path, rec)) {
            continue;
        }
        m_monsters[rec.index] = std::move(rec);
        ++loaded;
    }
    openkal::log::info("LootMonsterStore: loaded " + std::to_string(loaded) + " monsters from " +
                       dir);
    return loaded;
}

std::vector<const MonsterRecord*> LootMonsterStore::sorted_monsters() const {
    std::vector<const MonsterRecord*> out;
    out.reserve(m_monsters.size());
    for (const auto& [_, rec] : m_monsters) {
        out.push_back(&rec);
    }
    std::sort(out.begin(), out.end(), [](const MonsterRecord* a, const MonsterRecord* b) {
        if (a->level != b->level) {
            return a->level < b->level;
        }
        return a->name < b->name;
    });
    return out;
}

MonsterRecord* LootMonsterStore::find(int index) {
    const auto it = m_monsters.find(index);
    return it == m_monsters.end() ? nullptr : &it->second;
}

const MonsterRecord* LootMonsterStore::find(int index) const {
    const auto it = m_monsters.find(index);
    return it == m_monsters.end() ? nullptr : &it->second;
}

bool LootMonsterStore::set_item_groups(int index, std::vector<std::pair<int, int>> item_groups) {
    MonsterRecord* rec = find(index);
    if (!rec) {
        return false;
    }
    rec->item_groups = std::move(item_groups);
    m_dirty[index] = true;
    return true;
}

std::vector<int> LootMonsterStore::dirty_monsters() const {
    std::vector<int> out;
    out.reserve(m_dirty.size());
    for (const auto& [idx, dirty] : m_dirty) {
        if (dirty) {
            out.push_back(idx);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void LootMonsterStore::mark_clean(int index) {
    m_dirty.erase(index);
    if (MonsterRecord* rec = find(index)) {
        const auto bytes = openkal::platform::read_file(rec->file_path);
        if (bytes) {
            rec->raw_json.assign(bytes->begin(), bytes->end());
        }
    }
}

bool LootMonsterStore::export_dirty() {
    bool ok = true;
    for (int idx : dirty_monsters()) {
        MonsterRecord* rec = find(idx);
        if (!rec) {
            continue;
        }
        if (!write_monster_item_groups(*rec)) {
            ok = false;
            continue;
        }
        mark_clean(idx);
    }
    return ok;
}

void LootMonsterStore::clear_dirty() {
    m_dirty.clear();
}
