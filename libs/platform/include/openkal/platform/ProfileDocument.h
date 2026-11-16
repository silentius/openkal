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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::platform {

    /** Result of saving an editable profile document. */
    enum class ProfileSaveStatus {
        Saved,
        Conflict,
        IoError,
    };

    /** Preserves and safely edits a line-oriented OpenKal profile. */
    class ProfileDocument final {
      public:
        /** Loads a profile document without resolving its path values.
         * @param path Profile file path.
         * @param error Optional destination for a human-readable failure reason.
         * @return Editable document, or no value when the file cannot be read.
         */
        [[nodiscard]] static std::optional<ProfileDocument> load(const std::string& path,
                                                                 std::string* error = nullptr);

        /** Creates an empty document whose first save requires an absent destination.
         * @param path Destination profile path.
         * @return New unsaved document.
         */
        [[nodiscard]] static ProfileDocument create(std::string path);

        /** Gets the source profile path.
         * @return Profile file path.
         */
        [[nodiscard]] const std::string& path() const;

        /** Gets a trimmed value by key.
         * @param key Profile key.
         * @return Value, or no value when the key is absent.
         */
        [[nodiscard]] std::optional<std::string> value(std::string_view key) const;

        /** Updates the first matching key or appends it to the document.
         * @param key Profile key.
         * @param value New value.
         */
        void set(std::string key, std::string value);

        /** Serializes the document while preserving unrelated lines.
         * @return Profile text.
         */
        [[nodiscard]] std::string text() const;

        /** Atomically saves when the source file has not changed externally.
         * @param error Optional destination for a human-readable failure reason.
         * @return Save status.
         */
        [[nodiscard]] ProfileSaveStatus save(std::string* error = nullptr);

      private:
        ProfileDocument(std::string path, std::string original_text);

        std::string m_path;
        std::string m_original_text;
        std::vector<std::string> m_lines;
        bool m_final_newline = false;
        bool m_new_document = false;
    };

}  // namespace openkal::platform
