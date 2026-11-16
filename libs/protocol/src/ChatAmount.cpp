/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ChatAmount.h"

#include <charconv>
#include <limits>
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

        [[nodiscard]] bool parse_int64_token(std::string_view token, std::int64_t& out) {
            if (token.empty()) {
                return false;
            }
            std::int64_t value = 0;
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

    bool ChatAmount::parse_args(std::string_view args, ChatAmountArgs& out) {
        std::string_view rest = args;
        const std::string_view tok = next_token(rest);
        skip_ws(rest);
        if (tok.empty() || !rest.empty()) {
            return false;
        }

        ChatAmountOp op = ChatAmountOp::Add;
        std::string_view number;
        if (tok.size() >= 2 && tok[0] == '+' && tok[1] == '=') {
            op = ChatAmountOp::Add;
            number = tok.substr(2);
        } else if (tok.size() >= 2 && tok[0] == '-' && tok[1] == '=') {
            op = ChatAmountOp::Subtract;
            number = tok.substr(2);
        } else if (tok.front() == '=') {
            op = ChatAmountOp::Set;
            number = tok.substr(1);
        } else {
            return false;
        }

        std::int64_t value = 0;
        if (!parse_int64_token(number, value)) {
            return false;
        }
        if (op == ChatAmountOp::Set) {
            if (value < 0) {
                return false;
            }
        } else if (value <= 0) {
            return false;
        }

        ChatAmountArgs parsed;
        parsed.op = op;
        parsed.value = value;
        out = parsed;
        return true;
    }

    std::int64_t ChatAmount::apply(std::int64_t current, const ChatAmountArgs& parsed) {
        constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
        constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
        switch (parsed.op) {
        case ChatAmountOp::Add:
            if (parsed.value > 0 && current > kMax - parsed.value) {
                return kMax;
            }
            return current + parsed.value;
        case ChatAmountOp::Subtract:
            if (parsed.value > 0 && current < kMin + parsed.value) {
                return kMin;
            }
            return current - parsed.value;
        case ChatAmountOp::Set:
        default:
            return parsed.value;
        }
    }

}  // namespace openkal::protocol
