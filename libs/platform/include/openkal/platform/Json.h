/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::platform {

    /** Provides a minimal JSON document object model. */
    class Json final {
      public:
        /** Identifies the stored JSON value type. */
        enum class Type { Null, Bool, Number, String, Array, Object };

        /** Gets the stored value type.
         * @return Stored value type.
         */
        [[nodiscard]] Type type() const;

        /** Sets the stored value type.
         * @param value Stored value type.
         */
        void set_type(Type value);

        /** Gets the stored Boolean value.
         * @return Stored Boolean value.
         */
        [[nodiscard]] bool boolean() const;

        /** Sets the stored Boolean value.
         * @param value Boolean value.
         */
        void set_boolean(bool value);

        /** Gets the stored number.
         * @return Stored number.
         */
        [[nodiscard]] double number() const;

        /** Sets the stored number.
         * @param value Number value.
         */
        void set_number(double value);

        /** Gets mutable string storage.
         * @return Mutable string storage.
         */
        [[nodiscard]] std::string& string();

        /** Gets string storage.
         * @return String storage.
         */
        [[nodiscard]] const std::string& string() const;

        /** Gets mutable array storage.
         * @return Mutable array storage.
         */
        [[nodiscard]] std::vector<Json>& array();

        /** Gets array storage.
         * @return Array storage.
         */
        [[nodiscard]] const std::vector<Json>& array() const;

        /** Gets mutable object storage.
         * @return Mutable object storage.
         */
        [[nodiscard]] std::map<std::string, Json>& object();

        /** Gets object storage.
         * @return Object storage.
         */
        [[nodiscard]] const std::map<std::string, Json>& object() const;

        /** Checks whether the value is null.
         * @return True when the value is null.
         */
        [[nodiscard]] bool is_null() const;

        /** Checks whether the value is an object.
         * @return True when the value is an object.
         */
        [[nodiscard]] bool is_object() const;

        /** Checks whether the value is an array.
         * @return True when the value is an array.
         */
        [[nodiscard]] bool is_array() const;

        /** Checks whether the value is a number.
         * @return True when the value is a number.
         */
        [[nodiscard]] bool is_number() const;

        /** Checks whether the value is a string.
         * @return True when the value is a string.
         */
        [[nodiscard]] bool is_string() const;

        /** Finds an object property.
         * @param key Property name.
         * @return Property value, or nullptr when absent or not an object.
         */
        [[nodiscard]] const Json* get(std::string_view key) const;

        /** Gets an object property or a shared null value.
         * @param key Property name.
         * @return Property value or a null value.
         */
        [[nodiscard]] const Json& at(std::string_view key) const;

        /** Converts the value to an integer.
         * @param fallback Value returned when conversion fails.
         * @return Converted integer or fallback.
         */
        [[nodiscard]] int as_int(int fallback = 0) const;

        /** Converts the value to a float.
         * @param fallback Value returned when conversion fails.
         * @return Converted float or fallback.
         */
        [[nodiscard]] float as_float(float fallback = 0.0f) const;

        /** Converts the value to a string.
         * @param fallback Value returned when conversion fails.
         * @return Converted string or fallback.
         */
        [[nodiscard]] std::string as_string(std::string fallback = {}) const;

        /** Parses JSON text and throws on malformed input.
         * @param text JSON text.
         * @return Parsed JSON value.
         */
        [[nodiscard]] static Json parse(std::string_view text);

        /** Parses JSON text without propagating parse errors.
         * @param text JSON text.
         * @param out Parsed value destination.
         * @param error Optional parse error destination.
         * @return True when parsing succeeds.
         */
        [[nodiscard]] static bool
        parse(std::string_view text, Json& out, std::string* error = nullptr);

        /** Serializes a JSON value.
         * @param value JSON value.
         * @param indent Spaces per indentation level, or zero for compact output.
         * @return Serialized JSON text.
         */
        [[nodiscard]] static std::string stringify(const Json& value, int indent = 2);

      private:
        /** Stored value type. */
        Type m_type = Type::Null;

        /** Stored Boolean value. */
        bool m_bool = false;

        /** Stored numeric value. */
        double m_number = 0.0;

        /** Stored string value. */
        std::string m_string;

        /** Stored array value. */
        std::vector<Json> m_array;

        /** Stored object value. */
        std::map<std::string, Json> m_object;
    };

    /** Compatibility wrapper for Json::parse.
     * @param text JSON text.
     * @return Parsed JSON value.
     */
    [[nodiscard]] inline Json json_parse(std::string_view text) {
        return Json::parse(text);
    }

    /** Compatibility wrapper for Json::parse.
     * @param text JSON text.
     * @param out Parsed value destination.
     * @param error Optional parse error destination.
     * @return True when parsing succeeds.
     */
    [[nodiscard]] inline bool
    json_parse(std::string_view text, Json& out, std::string* error = nullptr) {
        return Json::parse(text, out, error);
    }

    /** Compatibility wrapper for Json::stringify.
     * @param value JSON value.
     * @param indent Spaces per indentation level, or zero for compact output.
     * @return Serialized JSON text.
     */
    [[nodiscard]] inline std::string json_stringify(const Json& value, int indent = 2) {
        return Json::stringify(value, indent);
    }

}  // namespace openkal::platform
