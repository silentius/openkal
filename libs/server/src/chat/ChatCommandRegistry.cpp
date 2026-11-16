/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/chat/ChatCommandRegistry.h"

#include <cctype>

namespace openkal::server {
    namespace {

        [[nodiscard]] std::string to_lower(std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (char c : text) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

    }  // namespace

    void ChatCommandRegistry::register_command(std::string name, ChatCommandHandler handler) {
        m_handlers[to_lower(name)] = std::move(handler);
    }

    ChatCommandResult ChatCommandRegistry::dispatch(std::string_view command_name,
                                                    const ChatCommandContext& ctx) const {
        const auto it = m_handlers.find(to_lower(command_name));
        if (it == m_handlers.end() || !it->second) {
            return ChatCommandResult(false, "Unknown command.");
        }
        return it->second(ctx);
    }

    bool ChatCommandRegistry::has_command(std::string_view command_name) const {
        return m_handlers.find(to_lower(command_name)) != m_handlers.end();
    }

}  // namespace openkal::server
