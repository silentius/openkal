/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/FrameDrain.h"

namespace openkal::protocol {

    void FrameDrain::discard() {
        m_buffer.clear();
        m_consumed = 0;
    }

    std::size_t FrameDrain::compact() {
        if (!m_consumed) {
            return 0;
        }
        const auto moved = m_buffer.size() - m_consumed;
        m_buffer.erase(m_buffer.begin(),
                       m_buffer.begin() + static_cast<std::ptrdiff_t>(m_consumed));
        m_consumed = 0;
        return moved;
    }

}  // namespace openkal::protocol
