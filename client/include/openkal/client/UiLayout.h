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

#include "openkal/platform/Json.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace openkal::client {

    class UiDocument;
    class UiView;

    /** Linear RGB triple in the 0..1 range, as read from a UI layout document. */
    class RgbColor {
      public:
        constexpr RgbColor() = default;

        /**
         * Builds a color from its channels.
         * @param r Red in 0..1.
         * @param g Green in 0..1.
         * @param b Blue in 0..1.
         */
        constexpr RgbColor(float r, float g, float b) : m_r(r), m_g(g), m_b(b) {}

        /** @return Red channel in 0..1. */
        [[nodiscard]] constexpr float r() const {
            return m_r;
        }

        /** @return Green channel in 0..1. */
        [[nodiscard]] constexpr float g() const {
            return m_g;
        }

        /** @return Blue channel in 0..1. */
        [[nodiscard]] constexpr float b() const {
            return m_b;
        }

        /**
         * Replaces the red channel.
         * @param v Red in 0..1.
         */
        void set_r(float v) {
            m_r = v;
        }

        /**
         * Replaces the green channel.
         * @param v Green in 0..1.
         */
        void set_g(float v) {
            m_g = v;
        }

        /**
         * Replaces the blue channel.
         * @param v Blue in 0..1.
         */
        void set_b(float v) {
            m_b = v;
        }

      private:
        /** Red channel in 0..1. */
        float m_r = 1.0f;

        /** Green channel in 0..1. */
        float m_g = 1.0f;

        /** Blue channel in 0..1. */
        float m_b = 1.0f;
    };

    /**
     * One JSON UI layout document (window / widget chrome and geometry).
     *
     * Values are read with dotted paths such as "grid.icon" or "footer.close.x";
     * every getter falls back to a baked default so a missing or malformed document
     * still renders.
     */
    class UiLayout {
      public:
        /**
         * Loads and parses a layout document, replacing any previously loaded one.
         * @param path Path to the JSON file.
         * @return True when the file parsed into a JSON object.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /**
         * Re-reads the document from disk when its modification time changed.
         * @return True when the document was reloaded successfully.
         */
        [[nodiscard]] bool reload_if_stale();

        /** @return True when a document is loaded. */
        [[nodiscard]] bool loaded() const {
            return m_loaded;
        }

        /** @return Path the document was loaded from. */
        [[nodiscard]] const std::string& path() const {
            return m_path;
        }

        /** @return Parsed document root. */
        [[nodiscard]] const platform::Json& root() const {
            return m_root;
        }

        /**
         * Reads a number.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] float get_f(std::string_view dotted, float fallback = 0.0f) const;

        /**
         * Reads an integer.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] int get_i(std::string_view dotted, int fallback = 0) const;

        /**
         * Reads a string.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] std::string get_s(std::string_view dotted, std::string fallback = {}) const;

        /**
         * Reads a color from "#FFAAFF" or an { r, g, b } object (0..1 or 0..255).
         * When the path is an object holding a "color" field, that field wins.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the path is missing or malformed.
         * @return Parsed color, or the fallback.
         */
        [[nodiscard]] RgbColor get_color(std::string_view dotted, RgbColor fallback = {}) const;

        /**
         * Reads a bitmap glyph scale (1 = native), allowing fractional sizes.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the path is missing.
         * @return Scale clamped to [0.5, 8].
         */
        [[nodiscard]] float get_font_size(std::string_view dotted, float fallback = 1.0f) const;

        /**
         * Tests whether a dotted path exists.
         * @param dotted Dotted path to test.
         * @return True when the path resolves to a node.
         */
        [[nodiscard]] bool has(std::string_view dotted) const {
            return resolve(dotted) != nullptr;
        }

        /**
         * Reads an object with x / y / w / h fields; missing keys keep the
         * out-params.
         * @param dotted Dotted path to the rect object.
         * @param x In/out rect left.
         * @param y In/out rect top.
         * @param w In/out rect width.
         * @param h In/out rect height.
         * @return True when the path resolved to an object.
         */
        bool read_rect(std::string_view dotted, float& x, float& y, float& w, float& h) const;

        /**
         * Null-tolerant number read for widgets that may have no layout bound.
         * @param lay Layout, or nullptr.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the layout or path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] static float
        get_f(const UiLayout* lay, std::string_view dotted, float fallback) {
            return lay ? lay->get_f(dotted, fallback) : fallback;
        }

        /**
         * Null-tolerant integer read for widgets that may have no layout bound.
         * @param lay Layout, or nullptr.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the layout or path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] static int get_i(const UiLayout* lay, std::string_view dotted, int fallback) {
            return lay ? lay->get_i(dotted, fallback) : fallback;
        }

        /**
         * Null-tolerant string read for widgets that may have no layout bound.
         * @param lay Layout, or nullptr.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the layout or path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] static std::string
        get_s(const UiLayout* lay, std::string_view dotted, std::string fallback) {
            return lay ? lay->get_s(dotted, std::move(fallback)) : fallback;
        }

        /**
         * Null-tolerant color read for widgets that may have no layout bound.
         * @param lay Layout, or nullptr.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the layout or path is missing.
         * @return Value at the path, or the fallback.
         */
        [[nodiscard]] static RgbColor
        get_color(const UiLayout* lay, std::string_view dotted, RgbColor fallback) {
            return lay ? lay->get_color(dotted, fallback) : fallback;
        }

        /**
         * Null-tolerant glyph scale read for widgets that may have no layout bound.
         * @param lay Layout, or nullptr.
         * @param dotted Dotted path to the value.
         * @param fallback Value used when the layout or path is missing.
         * @return Scale clamped to [0.5, 8].
         */
        [[nodiscard]] static float
        get_font_size(const UiLayout* lay, std::string_view dotted, float fallback) {
            return lay ? lay->get_font_size(dotted, fallback) : std::clamp(fallback, 0.5f, 8.0f);
        }

      private:
        friend class UiView;

        /** Immutable widget document, shared by layout copies and invalidated on load. */
        mutable std::shared_ptr<const UiDocument> m_document;

        /**
         * Parses a compatibility behavior-like hex color: #RGB, #RRGGBB or #RRGGBBAA (alpha
         * ignored).
         * @param text Color text, with or without the leading '#'.
         * @param out Destination color, untouched on failure.
         * @return True when the text was a valid hex color.
         */
        [[nodiscard]] static bool parse_css_color(std::string_view text, RgbColor& out);

        /**
         * Resolves a dotted path against the document root.
         * @param dotted Dotted path.
         * @return Node at the path, or nullptr.
         */
        [[nodiscard]] const platform::Json* resolve(std::string_view dotted) const;

        /**
         * Reads a color out of an already resolved node.
         * @param node Node holding a hex string or an { r, g, b } object.
         * @param fallback Value used when the node is malformed.
         * @return Parsed color, or the fallback.
         */
        [[nodiscard]] static RgbColor color_from_json(const platform::Json& node,
                                                      RgbColor fallback);

        /** Path the document was loaded from. */
        std::string m_path;

        /** Modification time of the loaded file, used for hot-reload. */
        std::int64_t m_mtime_sec = 0;

        /** Parsed document root. */
        platform::Json m_root;

        /** True once a document parsed successfully. */
        bool m_loaded = false;
    };

    /** Directory of named layouts loaded on demand from
     * config/ui/&lt;name&gt;.json. */
    class UiLayoutStore {
      public:
        /**
         * Points the store at a layout directory and drops every cached document.
         * @param dir Directory holding the JSON layouts.
         */
        void set_directory(std::string dir);

        /** @return Directory the store reads layouts from. */
        [[nodiscard]] const std::string& directory() const {
            return m_dir;
        }

        /**
         * Returns a layout, loading it on first use. Missing files yield an empty
         * layout so widgets fall back to their baked defaults.
         * @param name Layout name without the .json extension.
         * @return Cached layout reference, stable for the lifetime of the store.
         */
        [[nodiscard]] const UiLayout& get(std::string_view name) const;

        /**
         * Looks up an already loaded layout without touching the disk.
         * @param name Layout name without the .json extension.
         * @return Layout, or nullptr when it was never loaded.
         */
        [[nodiscard]] const UiLayout* find(std::string_view name) const;

        /**
         * Hot-reloads every cached layout whose file changed on disk.
         * @return Number of layouts reloaded.
         */
        int poll();

        /** Loads the layouts the client always needs, so hot-reload sees them from
         * frame one. */
        void preload_defaults();

      private:
        /** Directory holding the JSON layouts. */
        std::string m_dir;

        /** Cached layouts keyed by name; filled lazily by get(). */
        mutable std::unordered_map<std::string, UiLayout> m_layouts;
    };

}  // namespace openkal::client
