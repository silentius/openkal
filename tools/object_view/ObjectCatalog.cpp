/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ObjectCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>

namespace openkal::object_view {
    namespace {

        [[nodiscard]] char ascii_lower(char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        [[nodiscard]] std::string ascii_lowered(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                c = ascii_lower(c);
            }
            return out;
        }

        [[nodiscard]] bool ends_with_igb(std::string_view path) {
            if (path.size() < 3) {
                return false;
            }
            return ascii_lower(path[path.size() - 3]) == '.' &&
                   ascii_lower(path[path.size() - 2]) == 'g' &&
                   ascii_lower(path[path.size() - 1]) == 'b';
        }

        [[nodiscard]] std::string stem_of(std::string_view relative) {
            const auto slash = relative.find_last_of('/');
            std::string_view name =
                slash == std::string_view::npos ? relative : relative.substr(slash + 1);
            const auto dot = name.find_last_of('.');
            if (dot != std::string_view::npos) {
                name = name.substr(0, dot);
            }
            return std::string(name);
        }

        [[nodiscard]] std::string folder_of(std::string_view relative) {
            const auto slash = relative.find_last_of('/');
            if (slash == std::string_view::npos) {
                return {};
            }
            return std::string(relative.substr(0, slash));
        }

        [[nodiscard]] bool iequals(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (ascii_lower(a[i]) != ascii_lower(b[i])) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::string relative_to_root(const std::string& abs_path,
                                                   const std::string& root) {
            const std::string a = platform::normalize_separators(abs_path);
            std::string r = platform::normalize_separators(root);
            while (!r.empty() && r.back() == '/') {
                r.pop_back();
            }
            if (a.size() > r.size() && a[r.size()] == '/' && iequals(a.substr(0, r.size()), r)) {
                return a.substr(r.size() + 1);
            }
            return {};
        }

        [[nodiscard]] std::string display_relative(const std::string& abs_path,
                                                   const std::string& objects_dir) {
            std::string rel = relative_to_root(abs_path, objects_dir);
            if (!rel.empty()) {
                return rel;
            }
            const std::string nabs = platform::normalize_separators(abs_path);
            const auto slash = nabs.find_last_of('/');
            return slash == std::string::npos ? nabs : nabs.substr(slash + 1);
        }

    }  // namespace

    void ObjectEntry::set_relative_path(std::string value) {
        m_relative_path = std::move(value);
    }

    void ObjectEntry::set_absolute_path(std::string value) {
        m_absolute_path = std::move(value);
    }

    void ObjectEntry::set_label(std::string value) {
        m_label = std::move(value);
    }

    void ObjectEntry::set_folder(std::string value) {
        m_folder = std::move(value);
    }

    std::size_t ObjectCatalog::scan(const std::string& objects_dir) {
        m_entries.clear();
        m_visible.clear();
        m_filter.clear();
        if (objects_dir.empty() || !platform::is_directory(objects_dir)) {
            return 0;
        }

        const auto files = platform::list_files_recursive(objects_dir);
        m_entries.reserve(files.size());
        for (const auto& rel_raw : files) {
            std::string rel = platform::normalize_separators(rel_raw);
            if (!ends_with_igb(rel)) {
                continue;
            }
            append_gb(rel, platform::join_path(objects_dir, rel));
        }
        std::sort(
            m_entries.begin(), m_entries.end(), [](const ObjectEntry& a, const ObjectEntry& b) {
                return a.relative_path() < b.relative_path();
            });
        rebuild_visible();
        return m_entries.size();
    }

    std::size_t ObjectCatalog::load_files(const std::vector<std::string>& gb_paths,
                                          const std::string& objects_dir) {
        m_entries.clear();
        m_visible.clear();
        m_filter.clear();
        std::set<std::string> seen;
        m_entries.reserve(gb_paths.size());
        for (const auto& raw : gb_paths) {
            if (raw.empty() || !platform::file_exists(raw) || !ends_with_igb(raw)) {
                continue;
            }
            const std::string abs = platform::normalize_separators(raw);
            if (!seen.insert(ascii_lowered(abs)).second) {
                continue;
            }
            append_gb(display_relative(abs, objects_dir), abs);
        }
        std::sort(
            m_entries.begin(), m_entries.end(), [](const ObjectEntry& a, const ObjectEntry& b) {
                return a.relative_path() < b.relative_path();
            });
        rebuild_visible();
        return m_entries.size();
    }

    void ObjectCatalog::append_gb(std::string relative_path, std::string absolute_path) {
        ObjectEntry entry;
        entry.set_relative_path(std::move(relative_path));
        entry.set_absolute_path(std::move(absolute_path));
        entry.set_label(stem_of(entry.relative_path()));
        entry.set_folder(folder_of(entry.relative_path()));
        m_entries.push_back(std::move(entry));
    }

    void ObjectCatalog::set_filter(std::string query) {
        if (query == m_filter) {
            return;
        }
        m_filter = std::move(query);
        rebuild_visible();
    }

    const ObjectEntry& ObjectCatalog::visible_entry(int visible_index) const {
        return m_entries[static_cast<std::size_t>(
            m_visible[static_cast<std::size_t>(visible_index)])];
    }

    void ObjectCatalog::rebuild_visible() {
        m_visible.clear();
        m_visible.reserve(m_entries.size());
        const std::string needle = ascii_lowered(m_filter);
        for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
            if (!needle.empty()) {
                const std::string hay =
                    ascii_lowered(m_entries[static_cast<std::size_t>(i)].relative_path());
                if (hay.find(needle) == std::string::npos) {
                    continue;
                }
            }
            m_visible.push_back(i);
        }
    }

}  // namespace openkal::object_view
