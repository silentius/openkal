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

#include "openkal/platform/File.h"

#include <string>
#include <utility>
#include <vector>

namespace openkal::platform {

    /** Provides stateless path utility operations. */
    class Path final {
      public:
        /** Disables construction of this static utility class. */
        Path() = delete;

        /** Gets the directory containing the current executable.
         * @return Absolute executable directory, or "." when resolution fails.
         */
        [[nodiscard]] static std::string executable_directory();

        /** Lists regular files in a directory.
         * @param path Directory to inspect.
         * @return Non-recursive file names, or an empty vector on error.
         */
        [[nodiscard]] static std::vector<std::string> list_directory(const std::string& path);

        /** Recursively lists regular files below a directory.
         * @param path Root directory.
         * @return Paths relative to the root, or an empty vector on error.
         */
        [[nodiscard]] static std::vector<std::string> list_files_recursive(const std::string& path);

        /** Lists file and subdirectory names.
         * @param path Directory to inspect.
         * @return Non-recursive entry names excluding "." and "..".
         */
        [[nodiscard]] static std::vector<std::string>
        list_directory_entries(const std::string& path);

        /** Gets the OpenKal user configuration directory.
         * @return XDG-based OpenKal configuration path.
         */
        [[nodiscard]] static std::string config_dir();

        /** Checks whether a path names a directory.
         * @param path Path to inspect.
         * @return True when the path is a directory.
         */
        [[nodiscard]] static bool is_directory(const std::string& path);

        /** Replaces backslashes with forward slashes.
         * @param path Path to normalize.
         * @return Normalized path.
         */
        [[nodiscard]] static std::string normalize_separators(std::string path);

        /** Removes repeated leading client "data/" segments.
         * @param path Client path to normalize.
         * @return Path relative to the client data root.
         */
        [[nodiscard]] static std::string strip_client_data_prefix(std::string path);

        /** Resolves path components to their on-disk casing.
         * @param root Root directory.
         * @param relative Relative path below the root.
         * @return Best-effort resolved path.
         */
        [[nodiscard]] static std::string resolve_path_case(const std::string& root,
                                                           const std::string& relative);

        /** Resolves a client asset path below a data root.
         * @param data_root Client data root.
         * @param client_path Client-formatted asset path.
         * @param default_ext Extension appended when missing, or nullptr.
         * @return Resolved asset path, or an empty string for an empty asset.
         */
        [[nodiscard]] static std::string resolve_client_asset(const std::string& data_root,
                                                              const std::string& client_path,
                                                              const char* default_ext = nullptr);
    };

    /** Compatibility wrapper for Path::executable_directory.
     * @return Absolute executable directory, or "." when resolution fails.
     */
    [[nodiscard]] inline std::string executable_directory() {
        return Path::executable_directory();
    }

    /** Compatibility wrapper for Path::list_directory.
     * @param path Directory to inspect.
     * @return Non-recursive file names, or an empty vector on error.
     */
    [[nodiscard]] inline std::vector<std::string> list_directory(const std::string& path) {
        return Path::list_directory(path);
    }

    /** Compatibility wrapper for Path::list_files_recursive.
     * @param path Root directory.
     * @return Paths relative to the root, or an empty vector on error.
     */
    [[nodiscard]] inline std::vector<std::string> list_files_recursive(const std::string& path) {
        return Path::list_files_recursive(path);
    }

    /** Compatibility wrapper for Path::list_directory_entries.
     * @param path Directory to inspect.
     * @return Non-recursive entry names excluding "." and "..".
     */
    [[nodiscard]] inline std::vector<std::string> list_directory_entries(const std::string& path) {
        return Path::list_directory_entries(path);
    }

    /** Compatibility wrapper for Path::config_dir.
     * @return XDG-based OpenKal configuration path.
     */
    [[nodiscard]] inline std::string config_dir() {
        return Path::config_dir();
    }

    /** Compatibility wrapper for Path::is_directory.
     * @param path Path to inspect.
     * @return True when the path is a directory.
     */
    [[nodiscard]] inline bool is_directory(const std::string& path) {
        return Path::is_directory(path);
    }

    /** Compatibility wrapper for Path::normalize_separators.
     * @param path Path to normalize.
     * @return Normalized path.
     */
    [[nodiscard]] inline std::string normalize_separators(std::string path) {
        return Path::normalize_separators(std::move(path));
    }

    /** Compatibility wrapper for Path::strip_client_data_prefix.
     * @param path Client path to normalize.
     * @return Path relative to the client data root.
     */
    [[nodiscard]] inline std::string strip_client_data_prefix(std::string path) {
        return Path::strip_client_data_prefix(std::move(path));
    }

    /** Compatibility wrapper for Path::resolve_path_case.
     * @param root Root directory.
     * @param relative Relative path below the root.
     * @return Best-effort resolved path.
     */
    [[nodiscard]] inline std::string resolve_path_case(const std::string& root,
                                                       const std::string& relative) {
        return Path::resolve_path_case(root, relative);
    }

    /** Compatibility wrapper for Path::resolve_client_asset.
     * @param data_root Client data root.
     * @param client_path Client-formatted asset path.
     * @param default_ext Extension appended when missing, or nullptr.
     * @return Resolved asset path, or an empty string for an empty asset.
     */
    [[nodiscard]] inline std::string resolve_client_asset(const std::string& data_root,
                                                          const std::string& client_path,
                                                          const char* default_ext = nullptr) {
        return Path::resolve_client_asset(data_root, client_path, default_ext);
    }

}  // namespace openkal::platform
