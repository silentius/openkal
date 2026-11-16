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

#include "openkal/engine/engine.h"

#include <array>
#include <string>
#include <string_view>

namespace openkal::client {

    /** GPU atlas of a TrueType face rasterized with FreeType (ASCII 32..126). */
    class FreetypeFont {
      public:
        /** Pixel size used for overhead names. */
        static constexpr int kNamePixelSize = 14;

        /** Pixel size used for inventory and skill tooltips. */
        static constexpr int kTipPixelSize = 18;

        /** Pixel size used for full-screen banners (enchant success / fail). */
        static constexpr int kBannerPixelSize = 72;

        /** Pixel size used for floating combat numbers (bold, outlined). */
        static constexpr int kCombatPixelSize = 22;

        /**
         * Searches well-known Linux paths for a metric-compatible Arial stand-in.
         * @return Existing .ttf path, or an empty string when none is installed.
         */
        [[nodiscard]] static std::string find_system_sans();

        /**
         * Searches well-known Linux paths for a bold Arial stand-in.
         * @return Existing bold .ttf path, or an empty string when none is installed.
         */
        [[nodiscard]] static std::string find_system_sans_bold();

        /**
         * Rasterizes the face into a GPU atlas.
         * @param engine Engine used to create the atlas texture.
         * @param ttf_path Path of a .ttf / .otf file.
         * @param pixel_size Em size in pixels.
         * @return True when at least the printable ASCII set was packed.
         */
        [[nodiscard]] bool
        load(engine::IEngine& engine, const std::string& ttf_path, int pixel_size);

        /**
         * Releases the atlas texture.
         * @param engine Engine that owns the texture.
         */
        void destroy(engine::IEngine& engine);

        /** @return True when the atlas is ready to draw. */
        [[nodiscard]] bool ready() const {
            return static_cast<bool>(m_atlas);
        }

        /**
         * Advance width of a string in the baked size, optionally scaled for UI zoom.
         * @param text UTF-8; curly quotes/dashes fold to ASCII, other unknown
         *             codepoints become '?'.
         * @param scale Display scale (1 = native atlas pixels).
         * @return Width in pixels.
         */
        [[nodiscard]] float text_width(std::string_view text, float scale = 1.0f) const;

        /** Adds text advances to an existing width in rendering order.
         * @param text UTF-8 bytes folded onto the baked glyph set.
         * @param initial_width Width accumulated before this text.
         * @param scale Display scale (1 = native atlas pixels).
         * @return Initial width plus each rendered glyph advance, accumulated individually.
         */
        [[nodiscard]] float
        append_text_width(std::string_view text, float initial_width, float scale = 1.0f) const;

        /** @return Line height in pixels (ascender to descender). */
        [[nodiscard]] float line_height() const {
            return m_line_height;
        }

        /** @return Em size in pixels used when the atlas was rasterized. */
        [[nodiscard]] int pixel_size() const {
            return m_pixel_size;
        }

        /**
         * Draws left-aligned text. (x, y) is the top-left of the line box.
         * @param engine Engine used to submit UI quads.
         * @param x Left of the line box.
         * @param y Top of the line box.
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param scale Display scale (1 = native atlas pixels); matches window HUD
         * zoom.
         * @param a Alpha (1 = opaque).
         */
        void draw(engine::IEngine& engine,
                  float x,
                  float y,
                  std::string_view text,
                  float r,
                  float g,
                  float b,
                  float scale = 1.0f,
                  float a = 1.0f) const;

        /**
         * Draws text centered on a horizontal position, top-aligned at y.
         * @param engine Engine used to submit UI quads.
         * @param cx Center X of the line box.
         * @param y Top of the line box.
         * @param text Text to draw.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param scale Display scale (1 = native atlas pixels).
         * @param a Alpha (1 = opaque).
         */
        void draw_centered(engine::IEngine& engine,
                           float cx,
                           float y,
                           std::string_view text,
                           float r,
                           float g,
                           float b,
                           float scale = 1.0f,
                           float a = 1.0f) const;

      private:
        /** One packed glyph in the atlas. */
        class Glyph {
          public:
            /** @return True when this slot was rasterized. */
            [[nodiscard]] bool present() const {
                return m_present;
            }

            /** @param v True when the glyph was packed. */
            void set_present(bool v) {
                m_present = v;
            }

            /** @return Atlas U0. */
            [[nodiscard]] float u0() const {
                return m_u0;
            }

            /** @param v Atlas U0. */
            void set_u0(float v) {
                m_u0 = v;
            }

            /** @return Atlas V0. */
            [[nodiscard]] float v0() const {
                return m_v0;
            }

            /** @param v Atlas V0. */
            void set_v0(float v) {
                m_v0 = v;
            }

            /** @return Atlas U1. */
            [[nodiscard]] float u1() const {
                return m_u1;
            }

            /** @param v Atlas U1. */
            void set_u1(float v) {
                m_u1 = v;
            }

            /** @return Atlas V1. */
            [[nodiscard]] float v1() const {
                return m_v1;
            }

            /** @param v Atlas V1. */
            void set_v1(float v) {
                m_v1 = v;
            }

            /** @return Bitmap width in pixels. */
            [[nodiscard]] float width() const {
                return m_width;
            }

            /** @param v Bitmap width in pixels. */
            void set_width(float v) {
                m_width = v;
            }

            /** @return Bitmap height in pixels. */
            [[nodiscard]] float height() const {
                return m_height;
            }

            /** @param v Bitmap height in pixels. */
            void set_height(float v) {
                m_height = v;
            }

            /** @return Left bearing in pixels. */
            [[nodiscard]] float bearing_x() const {
                return m_bearing_x;
            }

            /** @param v Left bearing in pixels. */
            void set_bearing_x(float v) {
                m_bearing_x = v;
            }

            /** @return Top bearing in pixels (from baseline). */
            [[nodiscard]] float bearing_y() const {
                return m_bearing_y;
            }

            /** @param v Top bearing in pixels. */
            void set_bearing_y(float v) {
                m_bearing_y = v;
            }

            /** @return Horizontal advance in pixels. */
            [[nodiscard]] float advance() const {
                return m_advance;
            }

            /** @param v Horizontal advance in pixels. */
            void set_advance(float v) {
                m_advance = v;
            }

          private:
            /** Packed flag. */
            bool m_present = false;

            /** Atlas U0. */
            float m_u0 = 0.0f;

            /** Atlas V0. */
            float m_v0 = 0.0f;

            /** Atlas U1. */
            float m_u1 = 0.0f;

            /** Atlas V1. */
            float m_v1 = 0.0f;

            /** Bitmap width in pixels. */
            float m_width = 0.0f;

            /** Bitmap height in pixels. */
            float m_height = 0.0f;

            /** Left bearing in pixels. */
            float m_bearing_x = 0.0f;

            /** Top bearing in pixels. */
            float m_bearing_y = 0.0f;

            /** Horizontal advance in pixels. */
            float m_advance = 0.0f;
        };

        /**
         * Looks up a baked glyph, substituting '?' for missing characters.
         * @param ch ASCII 32..126 (already folded from UTF-8).
         * @return Glyph slot.
         */
        [[nodiscard]] const Glyph& glyph_for(unsigned char ch) const;

        /** GPU atlas texture. */
        engine::TextureHandle m_atlas;

        /** Atlas width in pixels. */
        int m_atlas_w = 0;

        /** Atlas height in pixels. */
        int m_atlas_h = 0;

        /** Line height in pixels. */
        float m_line_height = 0.0f;

        /** Ascender in pixels (baseline offset from the top of the line box). */
        float m_ascender = 0.0f;

        /** Em size passed to load(), or 0. */
        int m_pixel_size = 0;

        /** Glyphs for ASCII 32..126. */
        std::array<Glyph, 95> m_glyphs{};
    };

}  // namespace openkal::client
