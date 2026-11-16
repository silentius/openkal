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

#include "openkal/client/FreetypeFont.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** A visual line retaining its UTF-8 byte range in the unmodified source. */
    struct TextLine {
        std::size_t start = 0;
        std::size_t length = 0;
    };

    /** Wraps UTF-8 text without discarding spaces or explicit blank lines.
     * @param text Source text. @param width Maximum measured line width.
     * @param measure Width of a source substring. @return Source ranges for every visual line.
     */
    [[nodiscard]] std::vector<TextLine> layout_text_lines(
        std::string_view text, float width, const std::function<float(std::string_view)>& measure);

    /** Wraps source byte ranges using incremental FreeType glyph advances.
     * @param font Face used for measurement, with the same folding as text_width.
     * @param text Source text, including spaces and explicit blank lines.
     * @param width Maximum line width; nonpositive values disable wrapping.
     * @param scale Display scale passed to FreeType measurement.
     * @return The same source ranges as the generic layout with font.text_width.
     */
    [[nodiscard]] std::vector<TextLine> layout_text_lines(const FreetypeFont& font,
                                                          std::string_view text,
                                                          float width,
                                                          float scale = 1.0f);

    /**
     * Wraps text to a pixel width using a measure callback.
     * Honors existing newlines, prefers word breaks, and splits oversized words.
     * @param text Source text (ASCII / Latin-1).
     * @param max_width Maximum line width in pixels.
     * @param measure Returns the rendered width of a substring.
     * @return Wrapped lines (empty text yields an empty vector).
     */
    [[nodiscard]] std::vector<std::string>
    wrap_text(std::string_view text,
              float max_width,
              const std::function<float(std::string_view)>& measure);

    /**
     * Wraps text with a FreeType face. Falls back to newline splits when the font
     * is not ready.
     * @param font Face used for measurement.
     * @param text Source text.
     * @param max_width Maximum line width in pixels.
     * @param scale Display scale passed to text_width.
     * @return Wrapped lines.
     */
    [[nodiscard]] inline std::vector<std::string>
    wrap_text(const FreetypeFont& font, std::string_view text, float max_width, float scale) {
        if (!font.ready()) {
            return wrap_text(text, max_width, nullptr);
        }
        return wrap_text(
            text, max_width, [&](std::string_view s) { return font.text_width(s, scale); });
    }

}  // namespace openkal::client
