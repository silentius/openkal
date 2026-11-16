/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiLayout.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace openkal::client {
    namespace {

        const platform::Json* walk(const platform::Json& node, std::string_view dotted) {
            const platform::Json* cur = &node;
            std::size_t start = 0;
            while (start <= dotted.size()) {
                const std::size_t dot = dotted.find('.', start);
                const std::string_view key = (dot == std::string_view::npos)
                                                 ? dotted.substr(start)
                                                 : dotted.substr(start, dot - start);
                if (key.empty()) {
                    return nullptr;
                }
                cur = cur->get(key);
                if (!cur) {
                    return nullptr;
                }
                if (dot == std::string_view::npos) {
                    return cur;
                }
                start = dot + 1;
            }
            return nullptr;
        }

        int hex_nibble(char c) {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (c >= 'a' && c <= 'f') {
                return 10 + (c - 'a');
            }
            return -1;
        }

        bool hex_byte(std::string_view s, std::size_t i, int& out) {
            if (i + 1 >= s.size()) {
                return false;
            }
            const int hi = hex_nibble(s[i]);
            const int lo = hex_nibble(s[i + 1]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            out = (hi << 4) | lo;
            return true;
        }

        float channel_from_json(const platform::Json& n, float fallback) {
            if (!n.is_number()) {
                return fallback;
            }
            const float v = n.as_float(fallback);
            // Allow 0..255 or 0..1.
            if (v > 1.0f) {
                return std::clamp(v / 255.0f, 0.0f, 1.0f);
            }
            return std::clamp(v, 0.0f, 1.0f);
        }

    }  // namespace

    bool UiLayout::parse_css_color(std::string_view text, RgbColor& out) {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
            text.remove_prefix(1);
        }
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
            text.remove_suffix(1);
        }
        if (!text.empty() && text.front() == '#') {
            text.remove_prefix(1);
        }
        int r = 0, g = 0, b = 0;
        if (text.size() == 3) {
            const int rn = hex_nibble(text[0]);
            const int gn = hex_nibble(text[1]);
            const int bn = hex_nibble(text[2]);
            if (rn < 0 || gn < 0 || bn < 0) {
                return false;
            }
            r = rn * 17;
            g = gn * 17;
            b = bn * 17;
        } else if (text.size() == 6 || text.size() == 8) {
            if (!hex_byte(text, 0, r) || !hex_byte(text, 2, g) || !hex_byte(text, 4, b)) {
                return false;
            }
        } else {
            return false;
        }
        out.set_r(static_cast<float>(r) / 255.0f);
        out.set_g(static_cast<float>(g) / 255.0f);
        out.set_b(static_cast<float>(b) / 255.0f);
        return true;
    }

    bool UiLayout::load_file(const std::string& path) {
        m_document.reset();
        m_path = path;
        m_loaded = false;
        m_mtime_sec = 0;
        m_root = platform::Json{};

        const auto bytes = platform::read_file(path);
        if (!bytes) {
            return false;
        }
        std::string err;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), m_root, &err) ||
            !m_root.is_object()) {
            log::warn("ui layout parse failed: " + path + (err.empty() ? "" : (" — " + err)));
            m_root = platform::Json{};
            return false;
        }
        if (auto mt = platform::file_mtime_sec(path)) {
            m_mtime_sec = *mt;
        }
        m_loaded = true;
        return true;
    }

    bool UiLayout::reload_if_stale() {
#if defined(OPENKAL_DISABLE_UI_HOT_RELOAD)
        return false;
#else
        if (m_path.empty()) {
            return false;
        }
        const auto mt = platform::file_mtime_sec(m_path);
        if (!mt) {
            return false;
        }
        if (m_loaded && *mt == m_mtime_sec) {
            return false;
        }
        UiLayout next;
        if (!next.load_file(m_path)) {
            m_mtime_sec = *mt;
            return false;
        }
        *this = std::move(next);
        log::info("ui layout reloaded: " + m_path);
        return true;
#endif
    }

    const platform::Json* UiLayout::resolve(std::string_view dotted) const {
        if (!m_loaded || dotted.empty()) {
            return nullptr;
        }
        return walk(m_root, dotted);
    }

    float UiLayout::get_f(std::string_view dotted, float fallback) const {
        const platform::Json* n = resolve(dotted);
        return n ? n->as_float(fallback) : fallback;
    }

    int UiLayout::get_i(std::string_view dotted, int fallback) const {
        const platform::Json* n = resolve(dotted);
        return n ? n->as_int(fallback) : fallback;
    }

    std::string UiLayout::get_s(std::string_view dotted, std::string fallback) const {
        const platform::Json* n = resolve(dotted);
        return n ? n->as_string(std::move(fallback)) : fallback;
    }

    RgbColor UiLayout::color_from_json(const platform::Json& node, RgbColor fallback) {
        if (node.is_string()) {
            RgbColor c = fallback;
            if (parse_css_color(node.string(), c)) {
                return c;
            }
            return fallback;
        }
        if (node.is_object()) {
            if (const auto* named = node.get("color")) {
                return color_from_json(*named, fallback);
            }
            RgbColor c = fallback;
            if (const auto* r = node.get("r")) {
                c.set_r(channel_from_json(*r, c.r()));
            }
            if (const auto* g = node.get("g")) {
                c.set_g(channel_from_json(*g, c.g()));
            }
            if (const auto* b = node.get("b")) {
                c.set_b(channel_from_json(*b, c.b()));
            }
            return c;
        }
        return fallback;
    }

    RgbColor UiLayout::get_color(std::string_view dotted, RgbColor fallback) const {
        const platform::Json* n = resolve(dotted);
        return n ? color_from_json(*n, fallback) : fallback;
    }

    float UiLayout::get_font_size(std::string_view dotted, float fallback) const {
        return std::clamp(get_f(dotted, fallback), 0.5f, 8.0f);
    }

    bool
    UiLayout::read_rect(std::string_view dotted, float& x, float& y, float& w, float& h) const {
        const platform::Json* n = resolve(dotted);
        if (!n || !n->is_object()) {
            return false;
        }
        if (const auto* vx = n->get("x")) {
            x = vx->as_float(x);
        }
        if (const auto* vy = n->get("y")) {
            y = vy->as_float(y);
        }
        if (const auto* vw = n->get("w")) {
            w = vw->as_float(w);
        }
        if (const auto* vh = n->get("h")) {
            h = vh->as_float(h);
        }
        return true;
    }

    void UiLayoutStore::set_directory(std::string dir) {
        m_dir = std::move(dir);
        m_layouts.clear();
    }

    const UiLayout& UiLayoutStore::get(std::string_view name) const {
        const std::string key(name);
        auto it = m_layouts.find(key);
        if (it != m_layouts.end()) {
            return it->second;
        }
        UiLayout lay;
        if (!m_dir.empty()) {
            const std::string path = platform::join_path(m_dir, key + ".json");
            if (!lay.load_file(path)) {
                log::warn("ui layout missing or invalid: " + path + " (using baked defaults)");
            }
        }
        auto [ins, _] = m_layouts.emplace(key, std::move(lay));
        return ins->second;
    }

    const UiLayout* UiLayoutStore::find(std::string_view name) const {
        const auto it = m_layouts.find(std::string(name));
        return it == m_layouts.end() ? nullptr : &it->second;
    }

    int UiLayoutStore::poll() {
#if defined(OPENKAL_DISABLE_UI_HOT_RELOAD)
        return 0;
#else
        int n = 0;
        for (auto& [name, lay] : m_layouts) {
            (void) name;
            if (lay.reload_if_stale()) {
                ++n;
            }
        }
        return n;
#endif
    }

    void UiLayoutStore::preload_defaults() {
        static constexpr const char* kNames[] = {
            "skill_window",
            "dragon_hud",
            "status",
            "minimap",
            "chat",
            "battle_feed",
            "menu_bar",
            "inventory",
            "character_info",
            "quest_journal",
            "quest_snitch",
            "mix_master_window",
            "mixing_item_list",
            "transform_window",
            "riding",
            "blacksmith",
            "shop",
            "storage",
            "game_menu",
            "settings",
            "party",
            "party_member",
            "party_invitation",
            "party_actions",
            "party_action_member",
            "powerbook",
        };
        for (const char* name : kNames) {
            // Force-load into the cache; get() already warns if the JSON is
            // missing/invalid.
            const UiLayout& lay = get(name);
            if (!lay.loaded()) {
                log::warn("ui layout preload incomplete: " + std::string(name));
            }
        }
    }

}  // namespace openkal::client
