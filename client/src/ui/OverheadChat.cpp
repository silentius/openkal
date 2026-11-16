/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/OverheadChat.h"

#include "openkal/client/Nameplate.h"
#include "openkal/client/TextWrap.h"
#include "openkal/protocol/Chatting.h"

#include <algorithm>
#include <utility>

namespace openkal::client {

    void OverheadChat::show(std::string speaker, std::string text) {
        if (speaker.empty() || speaker == "Server" || text.empty()) {
            return;
        }
        const auto body = protocol::Chatting::display_body(text);
        if (body.empty()) {
            return;
        }
        m_messages[std::move(speaker)] = {std::string(body), 5.0f};
    }

    void OverheadChat::update(float dt) {
        for (auto it = m_messages.begin(); it != m_messages.end();) {
            it->second.remaining -= std::max(0.0f, dt);
            if (it->second.remaining <= 0.0f) {
                it = m_messages.erase(it);
            } else {
                ++it;
            }
        }
    }

    void OverheadChat::remove(const std::string& speaker) {
        m_messages.erase(speaker);
    }

    std::string_view OverheadChat::message(const std::string& speaker) const {
        const auto it = m_messages.find(speaker);
        return it == m_messages.end() ? std::string_view{} : it->second.text;
    }

    void OverheadChat::draw(engine::IEngine& engine,
                            const FreetypeFont& font,
                            const std::string& speaker,
                            float x,
                            float y) const {
        const auto text = message(speaker);
        if (text.empty() || !font.ready()) {
            return;
        }
        constexpr float width = 280.0f;
        const float scale = Nameplate::kTextScale;
        auto lines = wrap_text(font, text, width, scale);
        const float line_height = font.line_height() * scale;
        float top = y + 4.0f;
        for (const auto& line : lines) {
            const float line_width = font.text_width(line, scale);
            engine.draw_ui_rect(x - line_width * 0.5f - 3.0f,
                                top - 1.0f,
                                line_width + 6.0f,
                                line_height + 2.0f,
                                0,
                                0,
                                0,
                                0.65f);
            font.draw_centered(engine, x, top, line, 1.0f, 1.0f, 1.0f, scale);
            top += line_height;
        }
    }

}  // namespace openkal::client
