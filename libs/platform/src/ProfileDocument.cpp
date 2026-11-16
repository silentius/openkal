/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/ProfileDocument.h"

#include "openkal/platform/File.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <utility>

namespace openkal::platform {
    namespace {

        std::string trim(std::string_view value) {
            std::size_t begin = 0;
            while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
                ++begin;
            }
            std::size_t end = value.size();
            while (end > begin &&
                   (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r')) {
                --end;
            }
            return std::string(value.substr(begin, end - begin));
        }

        std::optional<std::pair<std::string, std::size_t>> parse_key(const std::string& line) {
            const std::string stripped = trim(line);
            if (stripped.empty() || stripped.front() == '#' || stripped.front() == ';') {
                return std::nullopt;
            }
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) {
                return std::nullopt;
            }
            const std::string key = trim(std::string_view(line).substr(0, equals));
            if (key.empty()) {
                return std::nullopt;
            }
            return std::make_pair(key, equals);
        }

        std::string_view key_group(std::string_view key) {
            const std::size_t separator = key.find('.');
            return key.substr(0, separator);
        }

        void compact_group_spacing(std::vector<std::string>& lines, std::string_view key) {
            if (key.find('.') == std::string_view::npos) {
                return;
            }
            const std::string_view group = key_group(key);
            for (std::size_t index = 0; index < lines.size(); ++index) {
                const auto current = parse_key(lines[index]);
                if (!current || key_group(current->first) != group) {
                    continue;
                }
                std::size_t next = index + 1;
                while (next < lines.size() && lines[next].empty()) {
                    ++next;
                }
                if (next == index + 1 || next == lines.size()) {
                    continue;
                }
                const auto following = parse_key(lines[next]);
                if (following && key_group(following->first) == group) {
                    lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(index + 1),
                                lines.begin() + static_cast<std::ptrdiff_t>(next));
                }
            }
        }

        std::optional<std::string> read_text(const std::string& path, std::string* error) {
            const auto bytes = File::read(path, error);
            if (!bytes) {
                return std::nullopt;
            }
            return std::string(bytes->begin(), bytes->end());
        }

        void set_error(std::string* error, std::string value) {
            if (error != nullptr) {
                *error = std::move(value);
            }
        }

    }  // namespace

    ProfileDocument::ProfileDocument(std::string path, std::string original_text)
        : m_path(std::move(path)), m_original_text(std::move(original_text)),
          m_final_newline(!m_original_text.empty() && m_original_text.back() == '\n') {
        std::istringstream input(m_original_text);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            m_lines.push_back(std::move(line));
        }
    }

    std::optional<ProfileDocument> ProfileDocument::load(const std::string& path,
                                                         std::string* error) {
        auto source = read_text(path, error);
        if (!source) {
            return std::nullopt;
        }
        return ProfileDocument(path, std::move(*source));
    }

    const std::string& ProfileDocument::path() const {
        return m_path;
    }

    ProfileDocument ProfileDocument::create(std::string path) {
        ProfileDocument document(std::move(path), {});
        document.m_new_document = true;
        return document;
    }

    std::optional<std::string> ProfileDocument::value(std::string_view key) const {
        for (const auto& line : m_lines) {
            const auto parsed = parse_key(line);
            if (parsed && parsed->first == key) {
                return trim(std::string_view(line).substr(parsed->second + 1));
            }
        }
        return std::nullopt;
    }

    void ProfileDocument::set(std::string key, std::string value) {
        for (auto& line : m_lines) {
            const auto parsed = parse_key(line);
            if (parsed && parsed->first == key) {
                const std::string prefix = line.substr(0, parsed->second + 1);
                std::size_t whitespace_end = parsed->second + 1;
                while (whitespace_end < line.size() &&
                       (line[whitespace_end] == ' ' || line[whitespace_end] == '\t')) {
                    ++whitespace_end;
                }
                line = prefix +
                       line.substr(parsed->second + 1, whitespace_end - parsed->second - 1) + value;
                compact_group_spacing(m_lines, key);
                return;
            }
        }
        if (!m_lines.empty() && !m_lines.back().empty()) {
            const auto previous = parse_key(m_lines.back());
            if (!previous || key_group(previous->first) != key_group(key)) {
                m_lines.emplace_back();
            }
        }
        const std::string appended_key = key;
        m_lines.push_back(std::move(key) + "=" + std::move(value));
        compact_group_spacing(m_lines, appended_key);
        m_final_newline = true;
    }

    std::string ProfileDocument::text() const {
        std::string result;
        for (std::size_t index = 0; index < m_lines.size(); ++index) {
            if (index != 0) {
                result.push_back('\n');
            }
            result += m_lines[index];
        }
        if (m_final_newline && (result.empty() || result.back() != '\n')) {
            result.push_back('\n');
        }
        return result;
    }

    ProfileSaveStatus ProfileDocument::save(std::string* error) {
        if (m_new_document) {
            std::error_code filesystem_error;
            const bool exists = std::filesystem::exists(m_path, filesystem_error);
            if (filesystem_error) {
                set_error(error,
                          "failed to inspect profile destination: " + filesystem_error.message());
                return ProfileSaveStatus::IoError;
            }
            if (exists) {
                set_error(error, "profile appeared on disk; reload it before saving");
                return ProfileSaveStatus::Conflict;
            }
        } else {
            std::string read_error;
            const auto current = read_text(m_path, &read_error);
            if (!current) {
                set_error(error, "failed to re-read profile before saving: " + read_error);
                return ProfileSaveStatus::IoError;
            }
            if (*current != m_original_text) {
                set_error(error, "profile changed on disk; reload it before saving");
                return ProfileSaveStatus::Conflict;
            }
        }

        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::string temporary = m_path + ".tmp." + std::to_string(stamp);
        const std::string updated = text();
        if (!File::write(temporary, updated)) {
            set_error(error, "failed to write temporary profile");
            return ProfileSaveStatus::IoError;
        }
        if (std::rename(temporary.c_str(), m_path.c_str()) != 0) {
            (void) std::remove(temporary.c_str());
            set_error(error, "failed to replace profile with temporary file");
            return ProfileSaveStatus::IoError;
        }
        m_original_text = updated;
        m_new_document = false;
        return ProfileSaveStatus::Saved;
    }

}  // namespace openkal::platform
