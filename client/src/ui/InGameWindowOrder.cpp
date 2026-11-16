/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/InGameWindowOrder.h"

#include <algorithm>

namespace openkal::client {

    void InGameWindowOrder::opened(InGameWindowId window) {
        closed(window);
        m_windows.push_back(window);
    }

    void InGameWindowOrder::closed(InGameWindowId window) {
        m_windows.erase(std::remove(m_windows.begin(), m_windows.end(), window), m_windows.end());
    }

    std::optional<InGameWindowId> InGameWindowOrder::latest() const {
        if (m_windows.empty()) {
            return std::nullopt;
        }
        return m_windows.back();
    }

}  // namespace openkal::client
