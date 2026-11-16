/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ChatCommandRegistry.h"

#include "openkal/protocol/Chatting.h"

#include <cctype>

namespace openkal::client {
    namespace {

        [[nodiscard]] std::string to_lower(std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (char c : text) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        [[nodiscard]] std::string_view args_after_command(std::string_view raw) {
            if (raw.empty() || raw.front() != '/') {
                return {};
            }
            std::string_view rest = raw.substr(1);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                rest.remove_prefix(1);
            }
            const auto sp = rest.find_first_of(" \t");
            if (sp == std::string_view::npos) {
                return {};
            }
            std::string_view args = rest.substr(sp + 1);
            while (!args.empty() && (args.front() == ' ' || args.front() == '\t')) {
                args.remove_prefix(1);
            }
            return args;
        }

    }  // namespace

    void ChatCommandRegistry::register_command(std::string name, ClientChatCommandHandler handler) {
        m_handlers[to_lower(name)] = std::move(handler);
    }

    ClientChatCommandResult ChatCommandRegistry::try_dispatch(std::string_view raw) const {
        if (!protocol::Chatting::is_slash_command(raw)) {
            return ClientChatCommandResult(false, {});
        }
        const std::string name = protocol::Chatting::command_name(raw);
        const auto it = m_handlers.find(name);
        if (it == m_handlers.end() || !it->second) {
            // Unknown slash commands still go to the server (e.g. /time).
            return ClientChatCommandResult(false, {});
        }
        const ClientChatCommandContext ctx(raw, args_after_command(raw));
        return it->second(ctx);
    }

}  // namespace openkal::client
