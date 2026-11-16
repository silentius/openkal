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
#include <string_view>

namespace openkal::client {
    /** Bounded account-form text with select-all editing and UTF-8 backspace. */
    class AccountTextInput {
      public:
        /** Returns the transient form text. @return Current value. */
        [[nodiscard]] const std::string& value() const;

        /** Selects the entire current value for copying or replacement. */
        void select_all();

        /** Removes the selection without changing the value. */
        void deselect();

        /** Returns whether the entire value is selected. @return True for selected text. */
        [[nodiscard]] bool selected() const;

        /** Appends text or replaces the selection, without silently truncating pasted text.
         * @param text Inserted bytes; control characters are rejected.
         * @param byte_limit Maximum resulting byte length.
         * @return True when the whole insertion fits and contains no controls.
         */
        [[nodiscard]] bool insert(std::string_view text, std::size_t byte_limit);

        /** Deletes the selection or the last UTF-8 codepoint. */
        void backspace();

        /** Clears the value and selection. */
        void clear();

      private:
        std::string m_text;
        bool m_selected = false;
    };
}  // namespace openkal::client
