/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/Json.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace openkal::platform {
    namespace {

        void skip_whitespace(std::string_view text, std::size_t& index) {
            while (index < text.size() && (text[index] == ' ' || text[index] == '\t' ||
                                           text[index] == '\n' || text[index] == '\r')) {
                ++index;
            }
        }

        Json parse_value(std::string_view text, std::size_t& index);

        Json parse_string(std::string_view text, std::size_t& index) {
            if (index >= text.size() || text[index] != '"') {
                throw std::runtime_error("json: expected string");
            }
            ++index;
            std::string output;
            while (index < text.size()) {
                const char character = text[index++];
                if (character == '"') {
                    Json value;
                    value.set_type(Json::Type::String);
                    value.string() = std::move(output);
                    return value;
                }
                if (character == '\\' && index < text.size()) {
                    const char escape = text[index++];
                    switch (escape) {
                    case '"':
                    case '\\':
                    case '/':
                        output.push_back(escape);
                        break;
                    case 'b':
                        output.push_back('\b');
                        break;
                    case 'f':
                        output.push_back('\f');
                        break;
                    case 'n':
                        output.push_back('\n');
                        break;
                    case 'r':
                        output.push_back('\r');
                        break;
                    case 't':
                        output.push_back('\t');
                        break;
                    case 'u':
                        for (int digit = 0; digit < 4 && index < text.size(); ++digit) {
                            ++index;
                        }
                        output.push_back('?');
                        break;
                    default:
                        output.push_back(escape);
                        break;
                    }
                } else {
                    output.push_back(character);
                }
            }
            throw std::runtime_error("json: unterminated string");
        }

        Json parse_number(std::string_view text, std::size_t& index) {
            const std::size_t start = index;
            if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
                ++index;
            }
            while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
                ++index;
            }
            if (index < text.size() && text[index] == '.') {
                ++index;
                while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
                    ++index;
                }
            }
            if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
                ++index;
                if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
                    ++index;
                }
                while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
                    ++index;
                }
            }
            Json value;
            value.set_type(Json::Type::Number);
            value.set_number(std::stod(std::string(text.substr(start, index - start))));
            return value;
        }

        Json parse_array(std::string_view text, std::size_t& index) {
            if (index >= text.size() || text[index] != '[') {
                throw std::runtime_error("json: expected '['");
            }
            ++index;
            Json value;
            value.set_type(Json::Type::Array);
            skip_whitespace(text, index);
            if (index < text.size() && text[index] == ']') {
                ++index;
                return value;
            }
            while (index < text.size()) {
                value.array().push_back(parse_value(text, index));
                skip_whitespace(text, index);
                if (index < text.size() && text[index] == ',') {
                    ++index;
                    skip_whitespace(text, index);
                    continue;
                }
                if (index < text.size() && text[index] == ']') {
                    ++index;
                    return value;
                }
                throw std::runtime_error("json: expected ',' or ']'");
            }
            throw std::runtime_error("json: unterminated array");
        }

        Json parse_object(std::string_view text, std::size_t& index) {
            if (index >= text.size() || text[index] != '{') {
                throw std::runtime_error("json: expected '{'");
            }
            ++index;
            Json value;
            value.set_type(Json::Type::Object);
            skip_whitespace(text, index);
            if (index < text.size() && text[index] == '}') {
                ++index;
                return value;
            }
            while (index < text.size()) {
                skip_whitespace(text, index);
                Json key = parse_string(text, index);
                skip_whitespace(text, index);
                if (index >= text.size() || text[index] != ':') {
                    throw std::runtime_error("json: expected ':'");
                }
                ++index;
                value.object().emplace(std::move(key.string()), parse_value(text, index));
                skip_whitespace(text, index);
                if (index < text.size() && text[index] == ',') {
                    ++index;
                    continue;
                }
                if (index < text.size() && text[index] == '}') {
                    ++index;
                    return value;
                }
                throw std::runtime_error("json: expected ',' or '}'");
            }
            throw std::runtime_error("json: unterminated object");
        }

        Json parse_value(std::string_view text, std::size_t& index) {
            skip_whitespace(text, index);
            if (index >= text.size()) {
                throw std::runtime_error("json: unexpected end");
            }
            const char character = text[index];
            if (character == '{') {
                return parse_object(text, index);
            }
            if (character == '[') {
                return parse_array(text, index);
            }
            if (character == '"') {
                return parse_string(text, index);
            }
            if (character == 't' && text.substr(index, 4) == "true") {
                index += 4;
                Json value;
                value.set_type(Json::Type::Bool);
                value.set_boolean(true);
                return value;
            }
            if (character == 'f' && text.substr(index, 5) == "false") {
                index += 5;
                Json value;
                value.set_type(Json::Type::Bool);
                value.set_boolean(false);
                return value;
            }
            if (character == 'n' && text.substr(index, 4) == "null") {
                index += 4;
                return {};
            }
            if (character == '-' || character == '+' || (character >= '0' && character <= '9')) {
                return parse_number(text, index);
            }
            throw std::runtime_error("json: unexpected token");
        }

        /** Formats a JSON number with a period decimal, independent of LC_NUMERIC.
         * @param value Number to write.
         * @return JSON number text, or null for non-finite values.
         */
        [[nodiscard]] std::string format_json_number(double value) {
            if (!std::isfinite(value)) {
                return "null";
            }
            if (std::floor(value) == value && std::fabs(value) < 1e15) {
                return std::to_string(static_cast<long long>(value));
            }
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << std::defaultfloat << std::setprecision(9) << value;
            return out.str();
        }

        void escape_string(std::string& output, std::string_view value) {
            output.push_back('"');
            for (const unsigned char character : value) {
                switch (character) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (character < 0x20) {
                        char buffer[8];
                        std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
                        output += buffer;
                    } else {
                        output.push_back(static_cast<char>(character));
                    }
                    break;
                }
            }
            output.push_back('"');
        }

    }  // namespace

    Json::Type Json::type() const {
        return m_type;
    }

    void Json::set_type(Type value) {
        m_type = value;
    }

    bool Json::boolean() const {
        return m_bool;
    }

    void Json::set_boolean(bool value) {
        m_type = Type::Bool;
        m_bool = value;
    }

    double Json::number() const {
        return m_number;
    }

    void Json::set_number(double value) {
        m_type = Type::Number;
        m_number = value;
    }

    std::string& Json::string() {
        return m_string;
    }

    const std::string& Json::string() const {
        return m_string;
    }

    std::vector<Json>& Json::array() {
        return m_array;
    }

    const std::vector<Json>& Json::array() const {
        return m_array;
    }

    std::map<std::string, Json>& Json::object() {
        return m_object;
    }

    const std::map<std::string, Json>& Json::object() const {
        return m_object;
    }

    bool Json::is_null() const {
        return m_type == Type::Null;
    }

    bool Json::is_object() const {
        return m_type == Type::Object;
    }

    bool Json::is_array() const {
        return m_type == Type::Array;
    }

    bool Json::is_number() const {
        return m_type == Type::Number;
    }

    bool Json::is_string() const {
        return m_type == Type::String;
    }

    const Json* Json::get(std::string_view key) const {
        if (!is_object()) {
            return nullptr;
        }
        const auto iterator = m_object.find(std::string(key));
        return iterator == m_object.end() ? nullptr : &iterator->second;
    }

    const Json& Json::at(std::string_view key) const {
        static const Json null_value;
        const Json* value = get(key);
        return value != nullptr ? *value : null_value;
    }

    int Json::as_int(int fallback) const {
        if (is_number()) {
            return static_cast<int>(std::llround(m_number));
        }
        if (is_string()) {
            try {
                return std::stoi(m_string);
            } catch (...) {
                return fallback;
            }
        }
        return fallback;
    }

    float Json::as_float(float fallback) const {
        if (is_number()) {
            return static_cast<float>(m_number);
        }
        if (is_string()) {
            try {
                return std::stof(m_string);
            } catch (...) {
                return fallback;
            }
        }
        return fallback;
    }

    std::string Json::as_string(std::string fallback) const {
        if (is_string()) {
            return m_string;
        }
        if (is_number()) {
            if (std::floor(m_number) == m_number) {
                return std::to_string(static_cast<long long>(m_number));
            }
            return std::to_string(m_number);
        }
        return fallback;
    }

    Json Json::parse(std::string_view text) {
        std::size_t index = 0;
        Json value = parse_value(text, index);
        skip_whitespace(text, index);
        if (index != text.size()) {
            throw std::runtime_error("json: trailing data");
        }
        return value;
    }

    bool Json::parse(std::string_view text, Json& out, std::string* error) {
        try {
            out = parse(text);
            return true;
        } catch (const std::exception& exception) {
            if (error != nullptr) {
                *error = exception.what();
            }
            return false;
        }
    }

    std::string Json::stringify(const Json& value, int indent) {
        std::string output;
        std::function<void(const Json&, int)> write = [&](const Json& current, int depth) {
            const auto newline = [&] {
                if (indent > 0) {
                    output.push_back('\n');
                    output.append(static_cast<std::size_t>(indent * depth), ' ');
                }
            };
            switch (current.type()) {
            case Type::Null:
                output += "null";
                return;
            case Type::Bool:
                output += current.boolean() ? "true" : "false";
                return;
            case Type::Number:
                output += format_json_number(current.number());
                return;
            case Type::String:
                escape_string(output, current.string());
                return;
            case Type::Array: {
                output.push_back('[');
                if (current.array().empty()) {
                    output.push_back(']');
                    return;
                }
                for (std::size_t index = 0; index < current.array().size(); ++index) {
                    if (index != 0) {
                        output.push_back(',');
                    }
                    newline();
                    if (indent > 0) {
                        output.append(static_cast<std::size_t>(indent * (depth + 1)), ' ');
                    }
                    write(current.array()[index], depth + 1);
                }
                newline();
                if (indent > 0) {
                    output.append(static_cast<std::size_t>(indent * depth), ' ');
                }
                output.push_back(']');
                return;
            }
            case Type::Object: {
                output.push_back('{');
                if (current.object().empty()) {
                    output.push_back('}');
                    return;
                }
                bool first = true;
                for (const auto& [key, child] : current.object()) {
                    if (!first) {
                        output.push_back(',');
                    }
                    first = false;
                    newline();
                    if (indent > 0) {
                        output.append(static_cast<std::size_t>(indent * (depth + 1)), ' ');
                    }
                    escape_string(output, key);
                    output += indent > 0 ? ": " : ":";
                    write(child, depth + 1);
                }
                newline();
                if (indent > 0) {
                    output.append(static_cast<std::size_t>(indent * depth), ' ');
                }
                output.push_back('}');
                return;
            }
            }
        };
        write(value, 0);
        if (indent > 0) {
            output.push_back('\n');
        }
        return output;
    }

}  // namespace openkal::platform
