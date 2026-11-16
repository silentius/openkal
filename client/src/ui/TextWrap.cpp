/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/TextWrap.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {
    namespace {

        void flush_line(std::vector<std::string>& lines, std::string& current) {
            lines.push_back(current);
            current.clear();
        }

        void append_hard(std::vector<std::string>& lines,
                         std::string& current,
                         std::string_view word,
                         float max_width,
                         const std::function<float(std::string_view)>& measure) {
            for (char ch : word) {
                std::string trial = current;
                trial.push_back(ch);
                if (!current.empty() && measure(trial) > max_width) {
                    flush_line(lines, current);
                    current.push_back(ch);
                } else {
                    current = std::move(trial);
                }
            }
        }

        void wrap_paragraph(std::vector<std::string>& lines,
                            std::string_view paragraph,
                            float max_width,
                            const std::function<float(std::string_view)>& measure) {
            if (paragraph.empty()) {
                lines.emplace_back();
                return;
            }
            std::string current;
            std::size_t i = 0;
            while (i < paragraph.size()) {
                while (i < paragraph.size() && paragraph[i] == ' ') {
                    ++i;
                }
                if (i >= paragraph.size()) {
                    break;
                }
                std::size_t j = i;
                while (j < paragraph.size() && paragraph[j] != ' ') {
                    ++j;
                }
                const std::string_view word = paragraph.substr(i, j - i);
                i = j;
                const std::string candidate =
                    current.empty() ? std::string(word) : current + " " + std::string(word);
                if (measure(candidate) <= max_width) {
                    current = candidate;
                    continue;
                }
                if (!current.empty()) {
                    flush_line(lines, current);
                }
                if (measure(word) <= max_width) {
                    current = std::string(word);
                } else {
                    append_hard(lines, current, word, max_width, measure);
                }
            }
            flush_line(lines, current);
        }

    }  // namespace

    std::vector<TextLine> layout_text_lines(std::string_view text,
                                            float width,
                                            const std::function<float(std::string_view)>& measure) {
        std::vector<TextLine> lines;
        std::size_t start = 0;
        while (start <= text.size()) {
            const auto newline = text.find('\n', start);
            const auto end = newline == text.npos ? text.size() : newline;
            if (start == end) {
                lines.push_back({start, 0});
            }
            while (start < end) {
                std::size_t cursor = start, last_space = start;
                while (cursor < end) {
                    auto next = cursor + 1;
                    while (next < end && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80) {
                        ++next;
                    }
                    if (cursor > start && measure && width > 0 &&
                        measure(text.substr(start, next - start)) > width) {
                        break;
                    }
                    if (text[cursor] == ' ') {
                        last_space = next;
                    }
                    cursor = next;
                }
                if (cursor < end && last_space > start) {
                    cursor = last_space;
                }
                lines.push_back({start, cursor - start});
                start = cursor;
            }
            if (newline == text.npos) {
                break;
            }
            start = newline + 1;
        }
        return lines;
    }

    std::vector<std::string> wrap_text(std::string_view text,
                                       float max_width,
                                       const std::function<float(std::string_view)>& measure) {
        std::vector<std::string> lines;
        if (text.empty()) {
            return lines;
        }
        auto split_newlines = [&]() {
            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t nl = text.find('\n', start);
                const std::size_t end = nl == std::string_view::npos ? text.size() : nl;
                lines.emplace_back(text.substr(start, end - start));
                if (nl == std::string_view::npos) {
                    break;
                }
                start = nl + 1;
            }
        };
        if (!measure || max_width <= 0.0f) {
            split_newlines();
            return lines;
        }
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t nl = text.find('\n', start);
            const std::size_t end = nl == std::string_view::npos ? text.size() : nl;
            wrap_paragraph(lines, text.substr(start, end - start), max_width, measure);
            if (nl == std::string_view::npos) {
                break;
            }
            start = nl + 1;
        }
        return lines;
    }

}  // namespace openkal::client
