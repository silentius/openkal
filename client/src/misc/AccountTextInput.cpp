/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/AccountTextInput.h"

#include <algorithm>

namespace openkal::client {
    const std::string& AccountTextInput::value() const {
        return m_text;
    }

    void AccountTextInput::select_all() {
        m_selected = !m_text.empty();
    }

    void AccountTextInput::deselect() {
        m_selected = false;
    }

    bool AccountTextInput::selected() const {
        return m_selected;
    }

    bool AccountTextInput::insert(std::string_view text, std::size_t byte_limit) {
        const auto current_size = m_selected ? 0 : m_text.size();
        if (current_size > byte_limit || text.size() > byte_limit - current_size ||
            !std::all_of(
                text.begin(), text.end(), [](unsigned char c) { return c >= 32 && c != 127; })) {
            return false;
        }
        if (!text.empty()) {
            if (m_selected) {
                m_text.clear();
            }
            m_text.append(text);
            m_selected = false;
        }
        return true;
    }

    void AccountTextInput::backspace() {
        if (m_selected) {
            clear();
        } else if (!m_text.empty()) {
            auto start = m_text.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(m_text[start]) & 0xc0) == 0x80) {
                --start;
            }
            m_text.erase(start);
        }
    }

    void AccountTextInput::clear() {
        m_text.clear();
        m_selected = false;
    }
}  // namespace openkal::client
