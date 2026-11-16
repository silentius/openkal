/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openkal::platform {

    /** Describes the selected runtime configuration source. */
    struct ConfigurationSource {
        /** Logical root used for all configuration lookups. */
        std::string configuration_root;

        /** Mounted archive path, or no value when loose files are used. */
        std::optional<std::string> archive_path;
    };

    /** Provides a read-only ZIP-backed view of the release configuration tree. */
    class ConfigArchive final {
      public:
        /** Disables construction of this static utility class. */
        ConfigArchive() = delete;

        /** Mounts an uncompressed ZIP archive at a logical resource root.
         * @param root Virtual path represented by the archive's resource entries.
         * @param archive_path ZIP archive to open.
         * @param error Optional destination for a human-readable failure reason.
         * @return True when the archive was validated and mounted.
         */
        [[nodiscard]] static bool mount(const std::string& root,
                                        const std::string& archive_path,
                                        std::string* error = nullptr);

        /** Selects a loose configuration directory or mounts config.pak as fallback.
         * Relative configured roots are resolved beneath the resource root.
         * @param resource_root Directory containing config.pak and the logical config tree.
         * @param configured_root Preferred loose configuration directory; empty selects the
         * package.
         * @param error Optional destination for a human-readable package failure reason.
         * @return Selected source, or no value when config.pak could not be mounted.
         */
        [[nodiscard]] static std::optional<ConfigurationSource>
        select(const std::string& resource_root,
               const std::string& configured_root,
               std::string* error = nullptr);

        /** Removes the currently mounted archive. */
        static void unmount();

        /** Reads a resource file from the mounted archive when the path belongs to it.
         * @param path Logical path to read.
         * @param error Optional destination for a human-readable failure reason.
         * @return File bytes, no value for an unrelated or missing path.
         */
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>>
        read(const std::string& path, std::string* error = nullptr);

        /** Checks whether a logical path is a regular archive entry.
         * @param path Logical path to inspect.
         * @return True when the mounted archive contains the path as a file.
         */
        [[nodiscard]] static bool file_exists(const std::string& path);

        /** Checks whether a logical path is a directory in the mounted archive.
         * @param path Logical path to inspect.
         * @return True when the path is the root or prefixes an archive entry.
         */
        [[nodiscard]] static bool is_directory(const std::string& path);

        /** Lists direct regular-file names in an archive directory.
         * @param path Logical directory path.
         * @return Direct file names, sorted lexicographically.
         */
        [[nodiscard]] static std::vector<std::string> list_directory(const std::string& path);

        /** Lists direct files and directories in an archive directory.
         * @param path Logical directory path.
         * @return Direct entry names, sorted lexicographically.
         */
        [[nodiscard]] static std::vector<std::string>
        list_directory_entries(const std::string& path);

        /** Lists all regular files below an archive directory.
         * @param path Logical directory path.
         * @return Relative paths, sorted lexicographically.
         */
        [[nodiscard]] static std::vector<std::string> list_files_recursive(const std::string& path);
    };

}  // namespace openkal::platform
