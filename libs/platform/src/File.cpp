/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/File.h"

#include "openkal/platform/ConfigArchive.h"

#include <cerrno>
#include <fstream>
#include <sys/stat.h>
#include <system_error>

namespace openkal::platform {
    namespace {

        std::string errno_message(const char* operation, int error) {
            const std::error_code code(error, std::generic_category());
            return std::string(operation) + ": " + code.message() + " (errno " +
                   std::to_string(error) + ")";
        }

        void set_error(std::string* error, std::string message) {
            if (error != nullptr) {
                *error = std::move(message);
            }
        }

    }  // namespace

    std::optional<std::vector<std::uint8_t>> File::read(const std::string& path,
                                                        std::string* error) {
        if (path.empty()) {
            set_error(error, "path is empty");
            return std::nullopt;
        }
        if (auto bytes = ConfigArchive::read(path, error)) {
            return bytes;
        }
        struct stat st {};
        if (::stat(path.c_str(), &st) != 0) {
            set_error(error, errno_message("stat failed", errno));
            return std::nullopt;
        }
        if (!S_ISREG(st.st_mode)) {
            set_error(error, "path is not a regular file");
            return std::nullopt;
        }
        // Guard against broken tellg on special files and absurd sizes.
        constexpr off_t kMaxFileBytes = 512ll * 1024ll * 1024ll;
        if (st.st_size < 0 || st.st_size > kMaxFileBytes) {
            set_error(error, "file size is outside the supported range (0 to 536870912 bytes)");
            return std::nullopt;
        }
        errno = 0;
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            set_error(error, errno != 0 ? errno_message("open failed", errno) : "open failed");
            return std::nullopt;
        }
        std::vector<std::uint8_t> data(static_cast<std::size_t>(st.st_size));
        errno = 0;
        if (st.st_size > 0 && !in.read(reinterpret_cast<char*>(data.data()),
                                       static_cast<std::streamsize>(st.st_size))) {
            set_error(error, errno != 0 ? errno_message("read failed", errno) : "read failed");
            return std::nullopt;
        }
        return data;
    }

    bool File::write(const std::string& path, const void* data, std::size_t size) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        if (size > 0 &&
            !out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size))) {
            return false;
        }
        return static_cast<bool>(out);
    }

    bool File::write(const std::string& path, std::string_view text) {
        return write(path, text.data(), text.size());
    }

    bool File::exists(const std::string& path) {
        if (ConfigArchive::file_exists(path)) {
            return true;
        }
        struct stat st {};
        return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

    std::optional<std::int64_t> File::mtime_sec(const std::string& path) {
        struct stat st {};
        if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(st.st_mtime);
    }

    bool File::ensure_directory(const std::string& path) {
        if (path.empty()) {
            return false;
        }
        struct stat st {};
        if (::stat(path.c_str(), &st) == 0) {
            return S_ISDIR(st.st_mode);
        }
        // mkdir -p
        std::string cur;
        for (std::size_t i = 0; i < path.size(); ++i) {
            const char c = path[i];
            cur.push_back(c);
            if (c == '/' || i + 1 == path.size()) {
                if (cur.empty() || cur == "/") {
                    continue;
                }
                // Strip trailing slash for mkdir except root.
                std::string dir = cur;
                while (dir.size() > 1 && dir.back() == '/') {
                    dir.pop_back();
                }
                if (::stat(dir.c_str(), &st) == 0) {
                    if (!S_ISDIR(st.st_mode)) {
                        return false;
                    }
                    continue;
                }
                if (::mkdir(dir.c_str(), 0755) != 0) {
                    if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                        return false;
                    }
                }
            }
        }
        return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    std::string File::join(std::string_view a, std::string_view b) {
        if (a.empty()) {
            return std::string(b);
        }
        if (b.empty()) {
            return std::string(a);
        }
        std::string out(a);
        if (out.back() != '/') {
            out.push_back('/');
        }
        if (!b.empty() && b.front() == '/') {
            out.append(b.substr(1));
        } else {
            out.append(b);
        }
        return out;
    }

}  // namespace openkal::platform
