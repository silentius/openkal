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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::platform {

    /** Provides stateless file-system utility operations. */
    class File final {
      public:
        /** Disables construction of this static utility class. */
        File() = delete;

        /** Reads an entire binary file.
         * @param path Path to the file.
         * @param error Optional destination for a human-readable failure reason.
         * @return File bytes, or std::nullopt when the file cannot be read.
         */
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>>
        read(const std::string& path, std::string* error = nullptr);

        /** Writes binary data, creating or truncating the file.
         * @param path Path to the file.
         * @param data Data to write.
         * @param size Number of bytes to write.
         * @return True on success.
         */
        [[nodiscard]] static bool
        write(const std::string& path, const void* data, std::size_t size);

        /** Writes text, creating or truncating the file.
         * @param path Path to the file.
         * @param text Text to write.
         * @return True on success.
         */
        [[nodiscard]] static bool write(const std::string& path, std::string_view text);

        /** Checks whether a regular file exists.
         * @param path Path to inspect.
         * @return True when the path names a regular file.
         */
        [[nodiscard]] static bool exists(const std::string& path);

        /** Gets a file's modification time.
         * @param path Path to the file.
         * @return Unix time in seconds, or std::nullopt when unavailable.
         */
        [[nodiscard]] static std::optional<std::int64_t> mtime_sec(const std::string& path);

        /** Creates a directory and its parents.
         * @param path Directory path.
         * @return True when the path exists as a directory afterward.
         */
        [[nodiscard]] static bool ensure_directory(const std::string& path);

        /** Joins two path segments with a slash.
         * @param a First path segment.
         * @param b Second path segment.
         * @return Joined path.
         */
        [[nodiscard]] static std::string join(std::string_view a, std::string_view b);
    };

    /** Compatibility wrapper for File::read.
     * @param path Path to the file.
     * @param error Optional destination for a human-readable failure reason.
     * @return File bytes, or std::nullopt when the file cannot be read.
     */
    [[nodiscard]] inline std::optional<std::vector<std::uint8_t>>
    read_file(const std::string& path, std::string* error = nullptr) {
        return File::read(path, error);
    }

    /** Compatibility wrapper for File::write.
     * @param path Path to the file.
     * @param data Data to write.
     * @param size Number of bytes to write.
     * @return True on success.
     */
    [[nodiscard]] inline bool
    write_file(const std::string& path, const void* data, std::size_t size) {
        return File::write(path, data, size);
    }

    /** Compatibility wrapper for File::write.
     * @param path Path to the file.
     * @param text Text to write.
     * @return True on success.
     */
    [[nodiscard]] inline bool write_file(const std::string& path, std::string_view text) {
        return File::write(path, text);
    }

    /** Compatibility wrapper for File::exists.
     * @param path Path to inspect.
     * @return True when the path names a regular file.
     */
    [[nodiscard]] inline bool file_exists(const std::string& path) {
        return File::exists(path);
    }

    /** Compatibility wrapper for File::mtime_sec.
     * @param path Path to the file.
     * @return Unix time in seconds, or std::nullopt when unavailable.
     */
    [[nodiscard]] inline std::optional<std::int64_t> file_mtime_sec(const std::string& path) {
        return File::mtime_sec(path);
    }

    /** Compatibility wrapper for File::ensure_directory.
     * @param path Directory path.
     * @return True when the path exists as a directory afterward.
     */
    [[nodiscard]] inline bool ensure_directory(const std::string& path) {
        return File::ensure_directory(path);
    }

    /** Compatibility wrapper for File::join.
     * @param a First path segment.
     * @param b Second path segment.
     * @return Joined path.
     */
    [[nodiscard]] inline std::string join_path(std::string_view a, std::string_view b) {
        return File::join(a, b);
    }

}  // namespace openkal::platform
