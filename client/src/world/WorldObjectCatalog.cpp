/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WorldObjectCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <cctype>
#include <utility>

namespace openkal::client {
    namespace {

        [[nodiscard]] std::string canon_key(std::string_view path) {
            std::string out;
            out.reserve(path.size());
            for (char c : path) {
                if (c == '/') {
                    out.push_back('\\');
                } else {
                    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            if (out.size() >= 3 && out.compare(out.size() - 3, 3, ".gb") == 0) {
                out.resize(out.size() - 3);
            }
            while (!out.empty() && (out.back() == '\\' || out.back() == ' ')) {
                out.pop_back();
            }
            return out;
        }

        [[nodiscard]] bool starts_with_ci(std::string_view s, std::string_view prefix) {
            if (s.size() < prefix.size()) {
                return false;
            }
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                const unsigned char a = static_cast<unsigned char>(s[i]);
                const unsigned char b = static_cast<unsigned char>(prefix[i]);
                if (std::tolower(a) != std::tolower(b)) {
                    return false;
                }
            }
            return true;
        }

        void parse_particles(const platform::Json& arr, std::vector<MonsterFxAttach>& out) {
            if (!arr.is_array()) {
                return;
            }
            for (const auto& row : arr.array()) {
                MonsterFxAttach attach;
                if (row.is_string()) {
                    attach.set_name(row.as_string());
                } else if (row.is_object()) {
                    attach.set_name(row.at("name").as_string(""));
                    attach.set_effect_bone(row.at("effectBone").as_int(-1));
                    const platform::Json& off = row.at("offset");
                    if (off.is_array() && off.array().size() >= 3) {
                        attach.set_ox(off.array()[0].as_float(0.0f));
                        attach.set_oy(off.array()[1].as_float(0.0f));
                        attach.set_oz(off.array()[2].as_float(0.0f));
                    }
                } else if (row.is_array() && !row.array().empty()) {
                    attach.set_name(row.array()[0].as_string());
                    if (row.array().size() > 1) {
                        attach.set_effect_bone(row.array()[1].as_int(-1));
                    }
                    if (row.array().size() > 2) {
                        attach.set_ox(row.array()[2].as_float(0.0f));
                    }
                    if (row.array().size() > 3) {
                        attach.set_oy(row.array()[3].as_float(0.0f));
                    }
                    if (row.array().size() > 4) {
                        attach.set_oz(row.array()[4].as_float(0.0f));
                    }
                } else {
                    continue;
                }
                if (!attach.name().empty()) {
                    out.push_back(std::move(attach));
                }
            }
        }

    }  // namespace

    void WorldObjectCatalog::index_path(std::string_view path, std::size_t index) {
        const std::string key = canon_key(path);
        if (key.empty()) {
            return;
        }
        auto put = [this, index](std::string k) {
            if (k.empty()) {
                return;
            }
            m_index.emplace(std::move(k), index);
        };
        put(key);
        if (key.rfind("map\\", 0) == 0) {
            put(key.substr(4));
        } else if (key.rfind("data\\", 0) != 0) {
            put("map\\" + key);
        }
    }

    bool WorldObjectCatalog::load_json(const std::string& path) {
        m_entries.clear();
        m_index.clear();
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            return false;
        }
        platform::Json root;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
            !root.is_object()) {
            return false;
        }
        const platform::Json& models = root.at("models");
        if (!models.is_array()) {
            return false;
        }
        m_entries.reserve(models.array().size());
        for (const auto& row : models.array()) {
            if (!row.is_object()) {
                continue;
            }
            const std::string model_path = row.at("path").as_string("");
            std::vector<MonsterFxAttach> particles;
            parse_particles(row.at("particles"), particles);
            if (model_path.empty() || particles.empty()) {
                continue;
            }
            Entry entry;
            entry.particles = std::move(particles);
            const std::size_t index = m_entries.size();
            m_entries.push_back(std::move(entry));
            index_path(model_path, index);
        }
        return !m_entries.empty();
    }

    const std::vector<MonsterFxAttach>*
    WorldObjectCatalog::find(std::string_view model_path) const {
        const std::string key = canon_key(model_path);
        if (key.empty()) {
            return nullptr;
        }
        const std::string* candidates[] = {&key, nullptr, nullptr};
        std::string stripped;
        std::string prefixed;
        if (key.rfind("map\\", 0) == 0) {
            stripped = key.substr(4);
            candidates[1] = &stripped;
        } else if (key.rfind("data\\", 0) != 0) {
            prefixed = "map\\" + key;
            candidates[1] = &prefixed;
        }
        for (const std::string* c : candidates) {
            if (c == nullptr) {
                continue;
            }
            const auto it = m_index.find(*c);
            if (it != m_index.end() && it->second < m_entries.size()) {
                return &m_entries[it->second].particles;
            }
        }
        return nullptr;
    }

    std::string WorldObjectCatalog::indoor_lookup_key(std::string_view map_relative_path) {
        if (map_relative_path.empty()) {
            return {};
        }
        if (starts_with_ci(map_relative_path, "MAP\\") ||
            starts_with_ci(map_relative_path, "MAP/") ||
            starts_with_ci(map_relative_path, "Data\\") ||
            starts_with_ci(map_relative_path, "Data/") ||
            starts_with_ci(map_relative_path, "data\\") ||
            starts_with_ci(map_relative_path, "data/")) {
            return std::string(map_relative_path);
        }
        std::string out = "MAP\\";
        out.append(map_relative_path);
        return out;
    }

    std::uint32_t WorldObjectCatalog::take_fx_entity(std::uint32_t& serial) {
        if (serial == 0) {
            serial = 1;
        }
        const std::uint32_t id = kFxBase | (serial & 0x0FFFFFFFu);
        ++serial;
        if ((serial & 0x0FFFFFFFu) == 0) {
            serial = 1;
        }
        return id;
    }

}  // namespace openkal::client
