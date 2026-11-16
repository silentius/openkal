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

#include <string>
#include <vector>

namespace openkal::tile_view {

    /** Summary of a tile asset export. */
    class ExportResult {
      public:
        /** @return Number of files written under the destination data/ tree. */
        [[nodiscard]] int files_copied() const {
            return m_files_copied;
        }

        /** @param value Number of files written. */
        void set_files_copied(int value) {
            m_files_copied = value;
        }

        /** @return Number of expected files that could not be found. */
        [[nodiscard]] int files_missing() const {
            return m_files_missing;
        }

        /** @param value Number of missing files. */
        void set_files_missing(int value) {
            m_files_missing = value;
        }

        /** @return True when every required tile file was copied. */
        [[nodiscard]] bool ok() const {
            return m_ok;
        }

        /** @param value Success flag. */
        void set_ok(bool value) {
            m_ok = value;
        }

        /** @return Human-readable summary. */
        [[nodiscard]] const std::string& message() const {
            return m_message;
        }

        /** @param value Human-readable summary. */
        void set_message(std::string value) {
            m_message = std::move(value);
        }

        /** @return Paths that could not be resolved. */
        [[nodiscard]] const std::vector<std::string>& missing_paths() const {
            return m_missing_paths;
        }

        /** @param path A missing source path or asset name. */
        void add_missing_path(std::string path) {
            m_missing_paths.push_back(std::move(path));
        }

      private:
        /** Number of files written. */
        int m_files_copied = 0;

        /** Number of missing files. */
        int m_files_missing = 0;

        /** True when the KCM (and OPL when present) were copied. */
        bool m_ok = false;

        /** Human-readable summary. */
        std::string m_message;

        /** Unresolved source paths. */
        std::vector<std::string> m_missing_paths;
    };

    /** Copies one outdoor tile and every referenced object/texture, preserving
     * data/ paths. */
    class TileExporter {
      public:
        /**
         * Exports n_XXX_YYY (and runtime state if that is what exists on disk) into
         * dest_root/data/.
         * @param data_root Client data root (contains MAPS/ and OBJECTS/).
         * @param dest_root Destination folder; files are written under
         * dest_root/data/.
         * @param patch_x Horizontal patch index (0..63).
         * @param patch_y Vertical patch index (0..63).
         * @return Copy counts and missing-file list.
         */
        [[nodiscard]] static ExportResult export_tile(const std::string& data_root,
                                                      const std::string& dest_root,
                                                      int patch_x,
                                                      int patch_y);
    };

}  // namespace openkal::tile_view
