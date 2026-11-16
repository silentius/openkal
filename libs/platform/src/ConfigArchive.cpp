/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/ConfigArchive.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <utility>

namespace openkal::platform {
    namespace {

        constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50;
        constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50;
        constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
        constexpr std::size_t kMaxArchiveBytes = 512u * 1024u * 1024u;
        constexpr std::size_t kMaxEntryCount = 20000;

        struct Entry {
            std::size_t data_offset = 0;
            std::size_t size = 0;
        };

        struct ArchiveState {
            std::string root;
            std::vector<std::uint8_t> bytes;
            std::map<std::string, Entry, std::less<>> entries;
        };

        std::mutex g_archive_mutex;
        std::shared_ptr<const ArchiveState> g_archive;

        void set_error(std::string* error, std::string value) {
            if (error != nullptr) {
                *error = std::move(value);
            }
        }

        [[nodiscard]] std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes,
                                             std::size_t offset) {
            return static_cast<std::uint16_t>(bytes[offset]) |
                   (static_cast<std::uint16_t>(bytes[offset + 1]) << 8u);
        }

        [[nodiscard]] std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes,
                                             std::size_t offset) {
            return static_cast<std::uint32_t>(bytes[offset]) |
                   (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
                   (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
                   (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
        }

        [[nodiscard]] bool range_is_valid(std::size_t offset, std::size_t size, std::size_t total) {
            return offset <= total && size <= total - offset;
        }

        [[nodiscard]] std::string normalize_path(std::string path) {
            while (path.size() > 1 && path.back() == '/') {
                path.pop_back();
            }
            return path;
        }

        [[nodiscard]] bool valid_entry_name(const std::string& name) {
            if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos) {
                return false;
            }
            std::size_t start = 0;
            while (start <= name.size()) {
                const std::size_t slash = name.find('/', start);
                const std::string component = name.substr(
                    start, slash == std::string::npos ? std::string::npos : slash - start);
                if (component.empty() || component == "." || component == "..") {
                    return false;
                }
                if (slash == std::string::npos) {
                    break;
                }
                start = slash + 1;
            }
            return true;
        }

        [[nodiscard]] std::shared_ptr<const ArchiveState> current_archive() {
            std::lock_guard<std::mutex> lock(g_archive_mutex);
            return g_archive;
        }

        [[nodiscard]] std::optional<std::string> relative_path(const ArchiveState& state,
                                                               const std::string& path) {
            const std::string normalized = normalize_path(path);
            if (normalized == state.root) {
                return std::string{};
            }
            if (normalized.size() <= state.root.size() ||
                normalized.compare(0, state.root.size(), state.root) != 0 ||
                normalized[state.root.size()] != '/') {
                return std::nullopt;
            }
            return normalized.substr(state.root.size() + 1);
        }

        [[nodiscard]] bool has_directory(const ArchiveState& state, const std::string& relative) {
            const std::string prefix = relative.empty() ? std::string{} : relative + "/";
            const auto entry = state.entries.lower_bound(prefix);
            return entry != state.entries.end() &&
                   entry->first.compare(0, prefix.size(), prefix) == 0;
        }

    }  // namespace

    bool ConfigArchive::mount(const std::string& root,
                              const std::string& archive_path,
                              std::string* error) {
        const std::string normalized_root = normalize_path(root);
        if (normalized_root.empty() || archive_path.empty()) {
            set_error(error, "configuration root or archive path is empty");
            return false;
        }

        std::ifstream input(archive_path, std::ios::binary | std::ios::ate);
        if (!input) {
            set_error(error, "could not open archive: " + archive_path);
            return false;
        }
        const std::streamoff size = input.tellg();
        if (size < 22 || static_cast<std::uintmax_t>(size) > kMaxArchiveBytes) {
            set_error(error, "archive size is outside the supported range");
            return false;
        }
        input.seekg(0);
        auto state = std::make_shared<ArchiveState>();
        state->root = normalized_root;
        state->bytes.resize(static_cast<std::size_t>(size));
        if (!input.read(reinterpret_cast<char*>(state->bytes.data()), size)) {
            set_error(error, "could not read archive: " + archive_path);
            return false;
        }

        const std::size_t search_start =
            state->bytes.size() > 65557 ? state->bytes.size() - 65557 : 0;
        std::size_t eocd = state->bytes.size();
        for (std::size_t offset = state->bytes.size() - 22;; --offset) {
            if (read_u32(state->bytes, offset) == kEndOfCentralDirectorySignature) {
                eocd = offset;
                break;
            }
            if (offset == search_start) {
                break;
            }
        }
        if (eocd == state->bytes.size() || !range_is_valid(eocd, 22, state->bytes.size())) {
            set_error(error, "archive has no valid end-of-central-directory record");
            return false;
        }

        const std::uint16_t disk = read_u16(state->bytes, eocd + 4);
        const std::uint16_t central_disk = read_u16(state->bytes, eocd + 6);
        const std::uint16_t entry_count = read_u16(state->bytes, eocd + 10);
        const std::uint32_t central_size = read_u32(state->bytes, eocd + 12);
        const std::uint32_t central_offset = read_u32(state->bytes, eocd + 16);
        const std::uint16_t comment_size = read_u16(state->bytes, eocd + 20);
        if (disk != 0 || central_disk != 0 || entry_count > kMaxEntryCount ||
            !range_is_valid(eocd, 22 + comment_size, state->bytes.size()) ||
            !range_is_valid(central_offset, central_size, state->bytes.size())) {
            set_error(error, "archive uses unsupported ZIP layout");
            return false;
        }

        std::size_t cursor = central_offset;
        for (std::uint16_t index = 0; index < entry_count; ++index) {
            if (!range_is_valid(cursor, 46, state->bytes.size()) ||
                read_u32(state->bytes, cursor) != kCentralHeaderSignature) {
                set_error(error, "archive has an invalid central-directory entry");
                return false;
            }
            const std::uint16_t flags = read_u16(state->bytes, cursor + 8);
            const std::uint16_t method = read_u16(state->bytes, cursor + 10);
            const std::uint32_t compressed_size = read_u32(state->bytes, cursor + 20);
            const std::uint32_t uncompressed_size = read_u32(state->bytes, cursor + 24);
            const std::uint16_t name_size = read_u16(state->bytes, cursor + 28);
            const std::uint16_t extra_size = read_u16(state->bytes, cursor + 30);
            const std::uint16_t entry_comment_size = read_u16(state->bytes, cursor + 32);
            const std::uint32_t local_offset = read_u32(state->bytes, cursor + 42);
            const std::size_t header_size = 46u + name_size + extra_size + entry_comment_size;
            if ((flags & 0x0001u) != 0 || method != 0 || compressed_size != uncompressed_size ||
                !range_is_valid(cursor, header_size, state->bytes.size())) {
                set_error(error, "archive contains encrypted or compressed entries");
                return false;
            }
            const std::string name(reinterpret_cast<const char*>(state->bytes.data() + cursor + 46),
                                   name_size);
            cursor += header_size;
            if (name.empty() || name.back() == '/') {
                continue;
            }
            if (!valid_entry_name(name) ||
                (name.compare(0, 7, "config/") != 0 && name.compare(0, 12, "RuntimeData/") != 0)) {
                set_error(error, "archive contains an invalid resource path");
                return false;
            }
            const std::string relative = name;
            if (relative.empty()) {
                set_error(error, "archive contains an invalid resource file path");
                return false;
            }
            if (!range_is_valid(local_offset, 30, state->bytes.size()) ||
                read_u32(state->bytes, local_offset) != kLocalHeaderSignature) {
                set_error(error, "archive has an invalid local file header");
                return false;
            }
            const std::uint16_t local_name_size = read_u16(state->bytes, local_offset + 26);
            const std::uint16_t local_extra_size = read_u16(state->bytes, local_offset + 28);
            const std::size_t data_offset = 30u + local_offset + local_name_size + local_extra_size;
            if (!range_is_valid(data_offset, uncompressed_size, state->bytes.size()) ||
                !state->entries.emplace(relative, Entry{data_offset, uncompressed_size}).second) {
                set_error(error, "archive contains duplicate or invalid entries");
                return false;
            }
        }
        if (cursor != static_cast<std::size_t>(central_offset) + central_size ||
            state->entries.empty()) {
            set_error(error, "archive central directory is malformed or contains no files");
            return false;
        }

        std::lock_guard<std::mutex> lock(g_archive_mutex);
        g_archive = std::move(state);
        return true;
    }

    std::optional<ConfigurationSource> ConfigArchive::select(const std::string& resource_root,
                                                             const std::string& configured_root,
                                                             std::string* error) {
        if (!configured_root.empty()) {
            std::filesystem::path loose_root(configured_root);
            if (loose_root.is_relative()) {
                loose_root = std::filesystem::path(resource_root) / loose_root;
            }
            loose_root = loose_root.lexically_normal();
            std::error_code filesystem_error;
            if (std::filesystem::is_directory(loose_root, filesystem_error)) {
                unmount();
                return ConfigurationSource{loose_root.string(), std::nullopt};
            }
        }

        const std::filesystem::path archive_path =
            std::filesystem::path(resource_root) / "config.pak";
        unmount();
        if (!mount(resource_root, archive_path.string(), error)) {
            return std::nullopt;
        }
        return ConfigurationSource{
            (std::filesystem::path(resource_root) / "config").lexically_normal().string(),
            archive_path.lexically_normal().string()};
    }

    void ConfigArchive::unmount() {
        std::lock_guard<std::mutex> lock(g_archive_mutex);
        g_archive.reset();
    }

    std::optional<std::vector<std::uint8_t>> ConfigArchive::read(const std::string& path,
                                                                 std::string* error) {
        const auto state = current_archive();
        if (!state) {
            return std::nullopt;
        }
        const auto relative = relative_path(*state, path);
        if (!relative) {
            return std::nullopt;
        }
        const auto entry = state->entries.find(*relative);
        if (entry == state->entries.end()) {
            set_error(error, "file is not present in the mounted configuration archive");
            return std::nullopt;
        }
        const auto begin =
            state->bytes.begin() + static_cast<std::ptrdiff_t>(entry->second.data_offset);
        return std::vector<std::uint8_t>(begin,
                                         begin + static_cast<std::ptrdiff_t>(entry->second.size));
    }

    bool ConfigArchive::file_exists(const std::string& path) {
        const auto state = current_archive();
        if (!state) {
            return false;
        }
        const auto relative = relative_path(*state, path);
        return relative && state->entries.find(*relative) != state->entries.end();
    }

    bool ConfigArchive::is_directory(const std::string& path) {
        const auto state = current_archive();
        if (!state) {
            return false;
        }
        const auto relative = relative_path(*state, path);
        return relative && has_directory(*state, *relative);
    }

    std::vector<std::string> ConfigArchive::list_directory(const std::string& path) {
        std::vector<std::string> result;
        const auto state = current_archive();
        if (!state) {
            return result;
        }
        const auto relative = relative_path(*state, path);
        if (!relative || !has_directory(*state, *relative)) {
            return result;
        }
        const std::string prefix = relative->empty() ? std::string{} : *relative + "/";
        for (auto entry = state->entries.lower_bound(prefix); entry != state->entries.end();
             ++entry) {
            if (entry->first.compare(0, prefix.size(), prefix) != 0) {
                break;
            }
            const std::string name = entry->first.substr(prefix.size());
            if (name.find('/') == std::string::npos) {
                result.push_back(name);
            }
        }
        return result;
    }

    std::vector<std::string> ConfigArchive::list_directory_entries(const std::string& path) {
        std::vector<std::string> result;
        const auto state = current_archive();
        if (!state) {
            return result;
        }
        const auto relative = relative_path(*state, path);
        if (!relative || !has_directory(*state, *relative)) {
            return result;
        }
        const std::string prefix = relative->empty() ? std::string{} : *relative + "/";
        std::set<std::string, std::less<>> names;
        for (auto entry = state->entries.lower_bound(prefix); entry != state->entries.end();
             ++entry) {
            if (entry->first.compare(0, prefix.size(), prefix) != 0) {
                break;
            }
            const std::string remaining = entry->first.substr(prefix.size());
            names.insert(remaining.substr(0, remaining.find('/')));
        }
        result.assign(names.begin(), names.end());
        return result;
    }

    std::vector<std::string> ConfigArchive::list_files_recursive(const std::string& path) {
        std::vector<std::string> result;
        const auto state = current_archive();
        if (!state) {
            return result;
        }
        const auto relative = relative_path(*state, path);
        if (!relative || !has_directory(*state, *relative)) {
            return result;
        }
        const std::string prefix = relative->empty() ? std::string{} : *relative + "/";
        for (auto entry = state->entries.lower_bound(prefix); entry != state->entries.end();
             ++entry) {
            if (entry->first.compare(0, prefix.size(), prefix) != 0) {
                break;
            }
            result.push_back(entry->first.substr(prefix.size()));
        }
        return result;
    }

}  // namespace openkal::platform
