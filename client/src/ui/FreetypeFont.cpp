/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/FreetypeFont.h"

#include "openkal/client/LatinText.h"
#include "openkal/client/TextWrap.h"
#include "openkal/platform/File.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace openkal::client {
    namespace {

        constexpr int kFirstChar = 32;
        constexpr int kGlyphCount = 95;
        constexpr int kAtlasPad = 1;
        constexpr int kSmallAtlasW = 512;
        constexpr int kSmallAtlasH = 256;
        constexpr int kLargeAtlas = 1024;

        [[nodiscard]] int glyph_index(unsigned char ch) {
            if (ch < kFirstChar || ch >= kFirstChar + kGlyphCount) {
                return static_cast<int>('?' - kFirstChar);
            }
            return static_cast<int>(ch - kFirstChar);
        }

        void atlas_dims_for(int pixel_size, int& atlas_w, int& atlas_h) {
            if (pixel_size <= 20) {
                atlas_w = kSmallAtlasW;
                atlas_h = kSmallAtlasH;
                return;
            }
            atlas_w = kLargeAtlas;
            atlas_h = kLargeAtlas;
        }

    }  // namespace

    std::string FreetypeFont::find_system_sans() {
        static const char* kCandidates[] = {
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        };
        for (const char* path : kCandidates) {
            if (platform::file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    std::string FreetypeFont::find_system_sans_bold() {
        static const char* kCandidates[] = {
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        };
        for (const char* path : kCandidates) {
            if (platform::file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    bool FreetypeFont::load(engine::IEngine& engine, const std::string& ttf_path, int pixel_size) {
        destroy(engine);
        if (ttf_path.empty() || pixel_size < 6) {
            return false;
        }

        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0) {
            return false;
        }
        FT_Face face = nullptr;
        if (FT_New_Face(library, ttf_path.c_str(), 0, &face) != 0) {
            FT_Done_FreeType(library);
            return false;
        }
        if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size)) != 0) {
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return false;
        }

        m_line_height = static_cast<float>(face->size->metrics.height) / 64.0f;
        m_ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
        m_pixel_size = pixel_size;
        if (m_line_height < 1.0f) {
            m_line_height = static_cast<float>(pixel_size);
        }
        if (m_ascender < 1.0f) {
            m_ascender = static_cast<float>(pixel_size);
        }

        int atlas_w = 0;
        int atlas_h = 0;
        atlas_dims_for(pixel_size, atlas_w, atlas_h);
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(atlas_w * atlas_h * 4), 0);
        int pen_x = kAtlasPad;
        int pen_y = kAtlasPad;
        int row_h = 0;
        bool packed = true;
        for (int i = 0; i < kGlyphCount; ++i) {
            const FT_ULong ch = static_cast<FT_ULong>(kFirstChar + i);
            if (FT_Load_Char(face, ch, FT_LOAD_RENDER) != 0) {
                continue;
            }
            const FT_GlyphSlot slot = face->glyph;
            const FT_Bitmap& bmp = slot->bitmap;
            const int gw = static_cast<int>(bmp.width);
            const int gh = static_cast<int>(bmp.rows);
            if (pen_x + gw + kAtlasPad > atlas_w) {
                pen_x = kAtlasPad;
                pen_y += row_h + kAtlasPad;
                row_h = 0;
            }
            if (pen_y + gh + kAtlasPad > atlas_h) {
                packed = false;
                break;
            }
            row_h = std::max(row_h, gh);
            if (bmp.buffer && gw > 0 && gh > 0) {
                for (int row = 0; row < gh; ++row) {
                    const std::uint8_t* src =
                        bmp.buffer + static_cast<std::size_t>(row) * bmp.pitch;
                    for (int col = 0; col < gw; ++col) {
                        const std::size_t o =
                            static_cast<std::size_t>(((pen_y + row) * atlas_w + (pen_x + col)) * 4);
                        const std::uint8_t a = src[col];
                        rgba[o + 0] = 255;
                        rgba[o + 1] = 255;
                        rgba[o + 2] = 255;
                        rgba[o + 3] = a;
                    }
                }
            }
            Glyph& g = m_glyphs[static_cast<std::size_t>(i)];
            g.set_present(true);
            g.set_u0(static_cast<float>(pen_x) / static_cast<float>(atlas_w));
            g.set_v0(static_cast<float>(pen_y) / static_cast<float>(atlas_h));
            g.set_u1(static_cast<float>(pen_x + gw) / static_cast<float>(atlas_w));
            g.set_v1(static_cast<float>(pen_y + gh) / static_cast<float>(atlas_h));
            g.set_width(static_cast<float>(gw));
            g.set_height(static_cast<float>(gh));
            g.set_bearing_x(static_cast<float>(slot->bitmap_left));
            g.set_bearing_y(static_cast<float>(slot->bitmap_top));
            g.set_advance(static_cast<float>(slot->advance.x) / 64.0f);
            pen_x += gw + kAtlasPad;
        }

        FT_Done_Face(face);
        FT_Done_FreeType(library);
        if (!packed) {
            m_glyphs = {};
            m_line_height = 0.0f;
            m_ascender = 0.0f;
            m_pixel_size = 0;
            return false;
        }

        m_atlas = engine.create_texture_rgba8(rgba.data(), atlas_w, atlas_h, false, false);
        if (!m_atlas) {
            m_glyphs = {};
            m_pixel_size = 0;
            return false;
        }
        m_atlas_w = atlas_w;
        m_atlas_h = atlas_h;
        return m_glyphs[static_cast<std::size_t>('A' - kFirstChar)].present();
    }

    void FreetypeFont::destroy(engine::IEngine& engine) {
        if (m_atlas) {
            engine.destroy_texture(m_atlas);
            m_atlas = {};
        }
        m_atlas_w = 0;
        m_atlas_h = 0;
        m_line_height = 0.0f;
        m_ascender = 0.0f;
        m_pixel_size = 0;
        m_glyphs = {};
    }

    const FreetypeFont::Glyph& FreetypeFont::glyph_for(unsigned char ch) const {
        const int i = glyph_index(ch);
        const Glyph& g = m_glyphs[static_cast<std::size_t>(i)];
        if (g.present()) {
            return g;
        }
        return m_glyphs[static_cast<std::size_t>('?' - kFirstChar)];
    }

    float FreetypeFont::text_width(std::string_view text, float scale) const {
        return append_text_width(text, 0.0f, scale);
    }

    float
    FreetypeFont::append_text_width(std::string_view text, float initial_width, float scale) const {
        const float s = std::max(0.01f, scale);
        float w = initial_width;
        LatinText::for_each_ascii(
            text, [&](char ch) { w += glyph_for(static_cast<unsigned char>(ch)).advance() * s; });
        return w;
    }

    std::vector<TextLine>
    layout_text_lines(const FreetypeFont& font, std::string_view text, float width, float scale) {
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
                float measured = 0.0f;
                while (cursor < end) {
                    auto next = cursor + 1;
                    while (next < end && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80) {
                        ++next;
                    }
                    if (width > 0) {
                        const float candidate = font.append_text_width(
                            text.substr(cursor, next - cursor), measured, scale);
                        if (cursor > start && candidate > width) {
                            break;
                        }
                        measured = candidate;
                    }
                    if (text[cursor] == ' ') {
                        last_space = next;
                    }
                    cursor = next;
                }
                // Only an unfinished word prefix is revisited after a preferred space break.
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

    void FreetypeFont::draw(engine::IEngine& engine,
                            float x,
                            float y,
                            std::string_view text,
                            float r,
                            float g,
                            float b,
                            float scale,
                            float a) const {
        if (!m_atlas || text.empty() || a <= 0.0f) {
            return;
        }
        const float s = std::max(0.01f, scale);
        const float alpha = std::clamp(a, 0.0f, 1.0f);
        float pen = x;
        LatinText::for_each_ascii(text, [&](char ascii) {
            const Glyph& gl = glyph_for(static_cast<unsigned char>(ascii));
            if (gl.width() > 0.5f && gl.height() > 0.5f) {
                const float gx = pen + gl.bearing_x() * s;
                const float gy = y + m_ascender * s - gl.bearing_y() * s;
                engine.draw_ui_quad(m_atlas,
                                    gx,
                                    gy,
                                    gl.width() * s,
                                    gl.height() * s,
                                    r,
                                    g,
                                    b,
                                    alpha,
                                    gl.u0(),
                                    gl.v0(),
                                    gl.u1(),
                                    gl.v1());
            }
            pen += gl.advance() * s;
        });
    }

    void FreetypeFont::draw_centered(engine::IEngine& engine,
                                     float cx,
                                     float y,
                                     std::string_view text,
                                     float r,
                                     float g,
                                     float b,
                                     float scale,
                                     float a) const {
        draw(engine, cx - text_width(text, scale) * 0.5f, y, text, r, g, b, scale, a);
    }

}  // namespace openkal::client
