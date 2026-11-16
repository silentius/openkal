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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    /**
     * Folds UTF-8 (and stray Windows-1252 bytes) onto the ASCII 32..126 glyph
     * set used by FreeType and bitmap UI fonts.
     */
    class LatinText {
      public:
        /**
         * Maps one Unicode codepoint onto a single ASCII glyph.
         * @param cp Unicode scalar value.
         * @return Printable ASCII, or '?' when the atlas has no stand-in.
         */
        [[nodiscard]] static char fold_codepoint(std::uint32_t cp) {
            if (cp >= 32 && cp <= 126) {
                return static_cast<char>(cp);
            }
            switch (cp) {
            case 0x00A0:
            case 0x2007:
            case 0x202F:
                return ' ';
            case 0x2018:
            case 0x2019:
            case 0x201A:
            case 0x201B:
            case 0x2032:
                return '\'';
            case 0x201C:
            case 0x201D:
            case 0x201E:
            case 0x2033:
                return '"';
            case 0x2010:
            case 0x2011:
            case 0x2012:
            case 0x2013:
            case 0x2014:
            case 0x2015:
            case 0x2212:
                return '-';
            default:
                return '?';
            }
        }

        /**
         * Calls fn(ascii) for each atlas glyph after UTF-8 decode and punctuation
         * folding. U+2026 becomes three periods.
         * @param utf8 Source bytes (JSON/config strings).
         * @param fn Invoked once per output glyph.
         */
        template <typename Fn> static void for_each_ascii(std::string_view utf8, Fn&& fn) {
            const auto is_cont = [](unsigned char b) { return (b & 0xC0) == 0x80; };
            const auto emit = [&](std::uint32_t cp) {
                if (cp == 0x2026) {
                    fn('.');
                    fn('.');
                    fn('.');
                    return;
                }
                fn(fold_codepoint(cp));
            };
            for (std::size_t i = 0; i < utf8.size();) {
                const auto b0 = static_cast<unsigned char>(utf8[i]);
                if (b0 < 0x80) {
                    emit(b0);
                    ++i;
                    continue;
                }
                if ((b0 & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
                    const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                    if (is_cont(b1)) {
                        emit(((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu));
                        i += 2;
                        continue;
                    }
                } else if ((b0 & 0xF0) == 0xE0 && i + 2 < utf8.size()) {
                    const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                    const auto b2 = static_cast<unsigned char>(utf8[i + 2]);
                    if (is_cont(b1) && is_cont(b2)) {
                        emit(((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu));
                        i += 3;
                        continue;
                    }
                } else if ((b0 & 0xF8) == 0xF0 && i + 3 < utf8.size()) {
                    const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                    const auto b2 = static_cast<unsigned char>(utf8[i + 2]);
                    const auto b3 = static_cast<unsigned char>(utf8[i + 3]);
                    if (is_cont(b1) && is_cont(b2) && is_cont(b3)) {
                        emit(((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) |
                             (b3 & 0x3Fu));
                        i += 4;
                        continue;
                    }
                }
                emit(cp1252_codepoint(b0));
                ++i;
            }
        }

        /**
         * Folds UTF-8 onto the ASCII glyph set.
         * @param utf8 Source bytes.
         * @return Atlas-safe ASCII.
         */
        [[nodiscard]] static std::string fold(std::string_view utf8) {
            std::string out;
            out.reserve(utf8.size());
            for_each_ascii(utf8, [&](char ch) { out.push_back(ch); });
            return out;
        }

      private:
        /**
         * Interprets a lone non-UTF-8 byte as Windows-1252 (smart quotes in mixed
         * encodings).
         * @param b Raw byte.
         * @return Unicode scalar to fold.
         */
        [[nodiscard]] static std::uint32_t cp1252_codepoint(unsigned char b) {
            switch (b) {
            case 0x91:
                return 0x2018;
            case 0x92:
                return 0x2019;
            case 0x93:
                return 0x201C;
            case 0x94:
                return 0x201D;
            case 0x96:
                return 0x2013;
            case 0x97:
                return 0x2014;
            case 0xA0:
                return 0x00A0;
            default:
                return b;
            }
        }
    };

}  // namespace openkal::client
