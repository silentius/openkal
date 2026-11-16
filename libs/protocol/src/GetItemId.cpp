/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/GetItemId.h"

#include <charconv>
#include <system_error>

namespace openkal::protocol {
    namespace {

        void skip_ws(std::string_view& text) {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                text.remove_prefix(1);
            }
        }

        [[nodiscard]] std::string_view next_token(std::string_view& text) {
            skip_ws(text);
            std::size_t n = 0;
            while (n < text.size() && text[n] != ' ' && text[n] != '\t') {
                ++n;
            }
            const std::string_view tok = text.substr(0, n);
            text.remove_prefix(n);
            return tok;
        }

        [[nodiscard]] bool parse_int_token(std::string_view token, int& out) {
            if (token.empty()) {
                return false;
            }
            int value = 0;
            const char* const first = token.data();
            const char* const last = token.data() + token.size();
            const std::from_chars_result parsed = std::from_chars(first, last, value);
            if (parsed.ec != std::errc{} || parsed.ptr != last) {
                return false;
            }
            out = value;
            return true;
        }

    }  // namespace

    bool GetItemId::parse_args(std::string_view args, GetItemIdArgs& out) {
        int values[3] = {0, 0, 0};
        int n = 0;
        std::string_view rest = args;
        while (n < 4) {
            const std::string_view tok = next_token(rest);
            if (tok.empty()) {
                break;
            }
            if (n == 3) {
                return false;
            }
            if (!parse_int_token(tok, values[n])) {
                return false;
            }
            ++n;
        }
        skip_ws(rest);
        if (!rest.empty()) {
            return false;
        }

        GetItemIdArgs parsed;
        if (n == 1) {
            parsed.index = values[0];
            parsed.prefix = 0;
            parsed.count = 1;
        } else if (n == 2) {
            parsed.index = values[0];
            parsed.prefix = 0;
            parsed.count = values[1];
        } else if (n == 3) {
            parsed.index = values[0];
            parsed.prefix = values[1];
            parsed.count = values[2];
        } else {
            return false;
        }
        if (parsed.index <= 0 || parsed.count <= 0) {
            return false;
        }
        if (parsed.prefix < 0 || parsed.prefix > 255) {
            return false;
        }
        out = parsed;
        return true;
    }

}  // namespace openkal::protocol
