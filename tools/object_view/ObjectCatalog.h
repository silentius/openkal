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
#include <string>
#include <string_view>
#include <vector>

namespace openkal::object_view {

    /** One .gb map object discovered under data/OBJECTS. */
    class ObjectEntry {
      public:
        /** @return Path relative to the OBJECTS root, using forward slashes. */
        [[nodiscard]] const std::string& relative_path() const {
            return m_relative_path;
        }

        /** @param value Path relative to the OBJECTS root. */
        void set_relative_path(std::string value);

        /** @return Absolute .gb file path. */
        [[nodiscard]] const std::string& absolute_path() const {
            return m_absolute_path;
        }

        /** @param value Absolute .gb file path. */
        void set_absolute_path(std::string value);

        /** @return File stem used as the list label. */
        [[nodiscard]] const std::string& label() const {
            return m_label;
        }

        /** @param value File stem used as the list label. */
        void set_label(std::string value);

        /** @return Folder path relative to OBJECTS (empty for the root). */
        [[nodiscard]] const std::string& folder() const {
            return m_folder;
        }

        /** @param value Folder path relative to OBJECTS. */
        void set_folder(std::string value);

      private:
        /** Path relative to the OBJECTS root. */
        std::string m_relative_path;

        /** Absolute .gb file path. */
        std::string m_absolute_path;

        /** File stem shown in the list. */
        std::string m_label;

        /** Parent folder relative to OBJECTS. */
        std::string m_folder;
    };

    /** Scans data/OBJECTS for .gb structures and filters them for the list UI. */
    class ObjectCatalog {
      public:
        /**
         * Recursively indexes every .gb file under objects_dir.
         * @param objects_dir Absolute path of the client OBJECTS folder.
         * @return Number of models found.
         */
        std::size_t scan(const std::string& objects_dir);

        /**
         * Indexes only the given absolute .gb paths (compatibility behavior / tile subset).
         * @param gb_paths Absolute .gb file paths.
         * @param objects_dir OBJECTS root used to compute list-relative paths.
         * @return Number of models indexed.
         */
        std::size_t load_files(const std::vector<std::string>& gb_paths,
                               const std::string& objects_dir);

        /**
         * Restricts visible() to relative paths containing the substring
         * (case-insensitive).
         * @param query Filter text; empty shows every entry.
         */
        void set_filter(std::string query);

        /** @return Current filter text. */
        [[nodiscard]] const std::string& filter() const {
            return m_filter;
        }

        /** @return Indexed models in sorted relative-path order. */
        [[nodiscard]] const std::vector<ObjectEntry>& entries() const {
            return m_entries;
        }

        /** @return Indices into entries() that match the current filter. */
        [[nodiscard]] const std::vector<int>& visible() const {
            return m_visible;
        }

        /**
         * Looks up a visible row.
         * @param visible_index Index into visible().
         * @return Matching catalog entry.
         */
        [[nodiscard]] const ObjectEntry& visible_entry(int visible_index) const;

        /** @return Number of indexed .gb files. */
        [[nodiscard]] std::size_t size() const {
            return m_entries.size();
        }

        /** @return Number of rows matching the filter. */
        [[nodiscard]] std::size_t visible_count() const {
            return m_visible.size();
        }

        /** @return True when no .gb files were found. */
        [[nodiscard]] bool empty() const {
            return m_entries.empty();
        }

      private:
        /**
         * Appends one catalog row.
         * @param relative_path Path shown in the list (usually relative to OBJECTS).
         * @param absolute_path Absolute .gb file path.
         */
        void append_gb(std::string relative_path, std::string absolute_path);

        /** Rebuilds runtime state from runtime state. */
        void rebuild_visible();

        /** All discovered models. */
        std::vector<ObjectEntry> m_entries;

        /** Filtered indices into runtime state. */
        std::vector<int> m_visible;

        /** Case-insensitive substring filter. */
        std::string m_filter;
    };

}  // namespace openkal::object_view
