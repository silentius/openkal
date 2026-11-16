/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientUiResources.h"

#include "openkal/platform/Log.h"

namespace openkal::client {

    void ClientUiResources::initialize(engine::IEngine& engine, const std::string& data_root) {
        m_ui.init(engine, data_root);
        const std::string font_path = FreetypeFont::find_system_sans();
        if (font_path.empty() ||
            !m_name_font.load(engine, font_path, FreetypeFont::kNamePixelSize)) {
            log::warn("nameplate FreeType font missing; overhead names use the bitmap font");
        } else {
            log::info("nameplate font " + font_path);
        }
        if (!font_path.empty()) {
            if (!m_tip_font.load(engine, font_path, FreetypeFont::kTipPixelSize)) {
                log::warn("item tip FreeType font failed to load");
            }
            if (!m_banner_font.load(engine, font_path, FreetypeFont::kBannerPixelSize)) {
                log::warn("enchant banner FreeType font failed to load");
            }
        }
        const std::string bold_path = FreetypeFont::find_system_sans_bold();
        const std::string combat_path = !bold_path.empty() ? bold_path : font_path;
        if (!combat_path.empty() &&
            !m_combat_font.load(engine, combat_path, FreetypeFont::kCombatPixelSize)) {
            log::warn("combat float FreeType font failed to load");
        }
    }

    void ClientUiResources::shutdown(engine::IEngine& engine) {
        m_name_font.destroy(engine);
        m_tip_font.destroy(engine);
        m_banner_font.destroy(engine);
        m_combat_font.destroy(engine);
        m_ui.destroy();
    }

    WidgetTools& ClientUiResources::ui() {
        return m_ui;
    }
    FreetypeFont& ClientUiResources::name_font() {
        return m_name_font;
    }
    FreetypeFont& ClientUiResources::tip_font() {
        return m_tip_font;
    }
    FreetypeFont& ClientUiResources::banner_font() {
        return m_banner_font;
    }
    FreetypeFont& ClientUiResources::combat_font() {
        return m_combat_font;
    }
    CombatFloatText& ClientUiResources::combat_floats() {
        return m_combat_floats;
    }

}  // namespace openkal::client
