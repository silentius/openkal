/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/LevelUp.h"

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

    bool LevelUp::parse_args(std::string_view args, LevelUpArgs& out) {
        std::string_view rest = args;
        const std::string_view tok = next_token(rest);
        skip_ws(rest);
        if (!rest.empty()) {
            return false;
        }

        LevelUpArgs parsed;
        if (tok.empty()) {
            parsed.op = LevelOp::Add;
            parsed.value = 1;
            out = parsed;
            return true;
        }

        LevelOp op = LevelOp::Add;
        std::string_view number;
        if (tok.size() >= 2 && tok[0] == '+' && tok[1] == '=') {
            op = LevelOp::Add;
            number = tok.substr(2);
        } else if (tok.size() >= 2 && tok[0] == '-' && tok[1] == '=') {
            op = LevelOp::Subtract;
            number = tok.substr(2);
        } else if (tok.front() == '=') {
            op = LevelOp::Set;
            number = tok.substr(1);
        } else {
            return false;
        }

        int value = 0;
        if (!parse_int_token(number, value) || value <= 0) {
            return false;
        }
        parsed.op = op;
        parsed.value = value;
        out = parsed;
        return true;
    }

}  // namespace openkal::protocol
