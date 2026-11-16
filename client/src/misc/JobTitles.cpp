/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/JobTitles.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>

namespace openkal::client {
    namespace {

        struct ClassJobs {
            std::string class_name;
            std::vector<std::string> titles;
        };

        std::array<ClassJobs, 4> g_classes{};
        bool g_loaded = false;

        [[nodiscard]] const char* class_file(int clazz) {
            switch (clazz) {
            case 0:
                return "knight.json";
            case 1:
                return "mage.json";
            case 2:
                return "archer.json";
            case 3:
                return "thief.json";
            default:
                return nullptr;
            }
        }

        [[nodiscard]] const char* fallback_class_name(int clazz) {
            switch (clazz) {
            case 0:
                return "Knight";
            case 1:
                return "Magician";
            case 2:
                return "Archer";
            case 3:
                return "Thief";
            default:
                return "";
            }
        }

        bool load_one(const std::string& jobs_dir, int clazz) {
            const char* file = class_file(clazz);
            if (!file) {
                return false;
            }
            const std::string path = platform::join_path(jobs_dir, file);
            auto bytes = platform::read_file(path);
            if (!bytes) {
                log::warn("JobTitles: missing " + path);
                return false;
            }
            platform::Json root;
            std::string err;
            if (!platform::Json::parse(std::string(bytes->begin(), bytes->end()), root, &err) ||
                !root.is_object()) {
                log::warn("JobTitles: parse failed " + path + (err.empty() ? "" : (": " + err)));
                return false;
            }
            ClassJobs entry;
            entry.class_name = root.at("class_name").as_string(fallback_class_name(clazz));
            const auto* titles = root.get("titles");
            if (titles && titles->is_array()) {
                for (const auto& t : titles->array()) {
                    const std::string name = t.as_string();
                    if (!name.empty()) {
                        entry.titles.push_back(name);
                    }
                }
            }
            if (entry.titles.empty()) {
                log::warn("JobTitles: no titles in " + path);
                return false;
            }
            g_classes[static_cast<std::size_t>(clazz)] = std::move(entry);
            return true;
        }

    }  // namespace

    bool JobTitles::load(const std::string& jobs_dir) {
        g_loaded = false;
        g_classes = {};
        if (jobs_dir.empty()) {
            return false;
        }
        int ok = 0;
        for (int i = 0; i < 4; ++i) {
            if (load_one(jobs_dir, i)) {
                ++ok;
            }
        }
        g_loaded = ok > 0;
        if (g_loaded) {
            log::info("JobTitles: loaded " + std::to_string(ok) + " class tables from " + jobs_dir);
        }
        return g_loaded;
    }

    std::string JobTitles::class_name(int clazz) {
        if (clazz < 0 || clazz > 3) {
            return {};
        }
        const auto& entry = g_classes[static_cast<std::size_t>(clazz)];
        if (!entry.class_name.empty()) {
            return entry.class_name;
        }
        return fallback_class_name(clazz);
    }

    std::string JobTitles::title(int clazz, int title_index) {
        if (clazz < 0 || clazz > 3 || title_index < 0) {
            return {};
        }
        const auto& entry = g_classes[static_cast<std::size_t>(clazz)];
        if (entry.titles.empty()) {
            return {};
        }
        const int idx = std::min(title_index, static_cast<int>(entry.titles.size()) - 1);
        return entry.titles[static_cast<std::size_t>(idx)];
    }

}  // namespace openkal::client
