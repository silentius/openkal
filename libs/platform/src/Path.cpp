/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Path.h"

#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <limits.h>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace openkal::platform {
    namespace {

        bool ieq_char(char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        }

        bool iequals(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (!ieq_char(a[i], b[i])) {
                    return false;
                }
            }
            return true;
        }

        bool starts_with_idata(std::string_view path) {
            // "data" or "data/"
            if (path.size() < 4) {
                return false;
            }
            if (!ieq_char(path[0], 'd') || !ieq_char(path[1], 'a') || !ieq_char(path[2], 't') ||
                !ieq_char(path[3], 'a')) {
                return false;
            }
            return path.size() == 4 || path[4] == '/';
        }

    }  // namespace

    std::string Path::executable_directory() {
        char buf[PATH_MAX];
        const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n <= 0) {
            return ".";
        }
        buf[n] = '\0';
        std::string path(buf);
        const auto slash = path.rfind('/');
        if (slash == std::string::npos) {
            return ".";
        }
        if (slash == 0) {
            return "/";
        }
        return path.substr(0, slash);
    }

    bool Path::is_directory(const std::string& path) {
        if (ConfigArchive::is_directory(path)) {
            return true;
        }
        struct stat st {};
        return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    std::vector<std::string> Path::list_directory(const std::string& path) {
        if (ConfigArchive::is_directory(path)) {
            return ConfigArchive::list_directory(path);
        }
        std::vector<std::string> out;
        DIR* dir = ::opendir(path.c_str());
        if (!dir) {
            return out;
        }
        while (dirent* ent = ::readdir(dir)) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
                continue;
            }
            const std::string full = File::join(path, ent->d_name);
            struct stat st {};
            if (::stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                out.emplace_back(ent->d_name);
            }
        }
        ::closedir(dir);
        return out;
    }

    namespace {

        void list_files_recursive_impl(const std::string& root,
                                       const std::string& rel,
                                       std::vector<std::string>& out) {
            const std::string cur = rel.empty() ? root : File::join(root, rel);
            DIR* dir = ::opendir(cur.c_str());
            if (!dir) {
                return;
            }
            while (dirent* ent = ::readdir(dir)) {
                if (ent->d_name[0] == '.' &&
                    (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
                    continue;
                }
                const std::string child_rel =
                    rel.empty() ? ent->d_name : File::join(rel, ent->d_name);
                const std::string full = File::join(root, child_rel);
                struct stat st {};
                if (::stat(full.c_str(), &st) != 0) {
                    continue;
                }
                if (S_ISDIR(st.st_mode)) {
                    list_files_recursive_impl(root, child_rel, out);
                } else if (S_ISREG(st.st_mode)) {
                    out.push_back(child_rel);
                }
            }
            ::closedir(dir);
        }

    }  // namespace

    std::vector<std::string> Path::list_files_recursive(const std::string& path) {
        if (ConfigArchive::is_directory(path)) {
            return ConfigArchive::list_files_recursive(path);
        }
        std::vector<std::string> out;
        list_files_recursive_impl(path, {}, out);
        return out;
    }

    std::vector<std::string> Path::list_directory_entries(const std::string& path) {
        if (ConfigArchive::is_directory(path)) {
            return ConfigArchive::list_directory_entries(path);
        }
        std::vector<std::string> out;
        DIR* dir = ::opendir(path.c_str());
        if (!dir) {
            return out;
        }
        while (dirent* ent = ::readdir(dir)) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
                continue;
            }
            out.emplace_back(ent->d_name);
        }
        ::closedir(dir);
        return out;
    }

    std::string Path::config_dir() {
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
            if (xdg[0] != '\0') {
                return File::join(xdg, "openkal");
            }
        }
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            return "openkal";
        }
        return File::join(File::join(home, ".config"), "openkal");
    }

    std::string Path::normalize_separators(std::string path) {
        for (char& c : path) {
            if (c == '\\') {
                c = '/';
            }
        }
        return path;
    }

    std::string Path::strip_client_data_prefix(std::string path) {
        path = normalize_separators(std::move(path));
        while (!path.empty() && path.front() == '/') {
            path.erase(path.begin());
        }
        // Strip repeated leading data/ (any case).
        while (starts_with_idata(path)) {
            if (path.size() == 4) {
                return {};
            }
            path = path.substr(5);  // skip "data/"
            while (!path.empty() && path.front() == '/') {
                path.erase(path.begin());
            }
        }
        return path;
    }

    std::string Path::resolve_path_case(const std::string& root, const std::string& relative) {
        if (relative.empty()) {
            return root;
        }
        std::string cur = root;
        std::string rel = normalize_separators(relative);
        std::size_t start = 0;
        while (start < rel.size()) {
            auto slash = rel.find('/', start);
            const std::string part =
                slash == std::string::npos ? rel.substr(start) : rel.substr(start, slash - start);
            start = slash == std::string::npos ? rel.size() : slash + 1;
            if (part.empty() || part == ".") {
                continue;
            }
            // Exact match first.
            const std::string exact = File::join(cur, part);
            struct stat st {};
            if (::stat(exact.c_str(), &st) == 0) {
                cur = exact;
                continue;
            }
            // Case-insensitive lookup in current directory.
            const auto entries = list_directory_entries(cur);
            std::string matched;
            for (const auto& e : entries) {
                if (iequals(e, part)) {
                    matched = e;
                    break;
                }
            }
            if (matched.empty()) {
                // Keep remaining path as-is under last good prefix.
                std::string rest = part;
                if (start < rel.size()) {
                    rest = File::join(rest, rel.substr(start));
                }
                return File::join(cur, rest);
            }
            cur = File::join(cur, matched);
        }
        return cur;
    }

    std::string Path::resolve_client_asset(const std::string& data_root,
                                           const std::string& client_path,
                                           const char* default_ext) {
        std::string rel = strip_client_data_prefix(client_path);
        if (rel.empty()) {
            // Empty / data-only paths are not assets (seen in OPL with name_len=0).
            return {};
        }
        if (default_ext && default_ext[0] != '\0') {
            const std::string ext = default_ext;
            // Append if missing (case-insensitive).
            bool has = false;
            if (rel.size() >= ext.size()) {
                has = iequals(std::string_view(rel).substr(rel.size() - ext.size()), ext);
            }
            if (!has) {
                rel += ext;
            }
        }
        return resolve_path_case(data_root, rel);
    }

}  // namespace openkal::platform
