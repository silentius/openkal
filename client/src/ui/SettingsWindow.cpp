/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SettingsWindow.h"

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace openkal::client {
    namespace {

        constexpr std::array<std::pair<int, int>, 6> kResolutions = {
            {{1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}}};

        std::string display_mode_label(settings::DisplayMode mode) {
            switch (mode) {
            case settings::DisplayMode::Maximized:
                return "Maximized";
            case settings::DisplayMode::Borderless:
                return "Borderless maximized";
            case settings::DisplayMode::Windowed:
            default:
                return "Windowed";
            }
        }

        std::string preset_label(settings::QualityPreset preset) {
            std::string value(settings::to_string(preset));
            if (!value.empty()) {
                value.front() = static_cast<char>(std::toupper(value.front()));
            }
            return value;
        }

        std::string bloom_label(settings::BloomQuality quality) {
            switch (quality) {
            case settings::BloomQuality::Subtle:
                return "Subtle";
            case settings::BloomQuality::High:
                return "High";
            case settings::BloomQuality::Off:
            default:
                return "Off";
            }
        }

        std::string shadow_label(settings::ShadowQuality quality) {
            switch (quality) {
            case settings::ShadowQuality::Characters:
                return "Dynamic actors";
            case settings::ShadowQuality::All:
                return "Actors + world";
            case settings::ShadowQuality::Off:
            default:
                return "Off";
            }
        }

        constexpr std::array<int, 6> kPlayerNameLimits = {-1, 5, 10, 25, 50, 0};

        constexpr std::array<int, 4> kMonsterRenderLimits = {10, 25, 50, 0};

        int monster_render_limit_index(int limit) {
            const auto found =
                std::find(kMonsterRenderLimits.begin(), kMonsterRenderLimits.end(), limit);
            return found == kMonsterRenderLimits.end()
                       ? 1
                       : static_cast<int>(std::distance(kMonsterRenderLimits.begin(), found));
        }

        std::string monster_render_limit_label(int limit) {
            return limit == 0 ? "Max" : std::to_string(limit);
        }

        int resolution_index(int width, int height) {
            for (std::size_t index = 0; index < kResolutions.size(); ++index) {
                if (kResolutions[index].first == width && kResolutions[index].second == height) {
                    return static_cast<int>(index);
                }
            }
            return 3;
        }

    }  // namespace

    void SettingsWindow::set_layout(const UiLayout* layout) {
        m_layout = layout;
        sync_from_layout();
    }

    void SettingsWindow::set_scale(float scale, float screen_w, float screen_h) {
        m_requested_scale = std::max(0.25f, scale);
        if (screen_w > 0 && screen_h > 0) {
            m_screen_w = screen_w;
            m_screen_h = screen_h;
        }
        sync_from_layout();
    }

    void SettingsWindow::show(const settings::ClientSettings& settings,
                              float screen_w,
                              float screen_h,
                              const ChatFilterModel& filters,
                              bool character_available) {
        m_character_available = character_available;
        m_chat_filters = filters;
        m_tab = "display";
        m_settings = settings;
        m_status.clear();
        (void) settings::validate_client_settings(m_settings);
        m_screen_w = screen_w;
        m_screen_h = screen_h;
        sync_from_layout();
        m_open = true;
    }

    void SettingsWindow::close() {
        m_open = false;
    }

    bool SettingsWindow::open() const {
        return m_open;
    }

    SettingsWindowAction
    SettingsWindow::handle_click(float mouse_x, float mouse_y, WidgetTools* ui) {
        if (!m_open) {
            return SettingsWindowAction::Ignored;
        }
        reload_document();
        const UiHit hit = UiView::hit(m_document,
                                      ui,
                                      mouse_x,
                                      mouse_y,
                                      m_x,
                                      m_y,
                                      m_w,
                                      m_h,
                                      m_scale,
                                      make_vars(),
                                      nullptr,
                                      true);
        if (hit.bind() == "settings.tab" && !hit.tab_id().empty()) {
            if (hit.tab_id() != "battle" || m_character_available) {
                m_tab = hit.tab_id();
            }
            return SettingsWindowAction::Consumed;
        }
        const std::string& action = hit.action();
        if (!m_character_available && action.rfind("settings.battle.", 0) == 0) {
            return SettingsWindowAction::Consumed;
        }
        if (action == "settings.misc.clock") {
            m_settings.clock_enabled = !m_settings.clock_enabled;
            return SettingsWindowAction::Consumed;
        }
        if (action == "settings.misc.show_own_name") {
            m_settings.show_own_name = !m_settings.show_own_name;
            return SettingsWindowAction::Consumed;
        }
        if (action == "settings.misc.ignore_party") {
            m_settings.ignore_party_requests = !m_settings.ignore_party_requests;
            return SettingsWindowAction::Consumed;
        }
        if (action == "settings.battle.system") {
            m_chat_filters.set_system_enabled(!m_chat_filters.system_enabled());
            return SettingsWindowAction::Consumed;
        }
        for (std::size_t i = 0; i < ChatFilterModel::kBattleCount; ++i) {
            if (action == "settings.battle." + std::to_string(i)) {
                const auto kind = static_cast<protocol::BattleMsgKind>(i);
                m_chat_filters.set_battle_enabled(kind, !m_chat_filters.battle_enabled(kind));
                return SettingsWindowAction::Consumed;
            }
        }
        if (action.empty()) {
            return SettingsWindowAction::Consumed;
        }
        if (action == "settings.close" || action == "window.close") {
            close();
            return SettingsWindowAction::Close;
        }
        if (action == "settings.save") {
            (void) settings::validate_client_settings(m_settings);
            return SettingsWindowAction::Save;
        }
        if (action == "settings.display.previous" || action == "settings.display.next") {
            int mode = static_cast<int>(m_settings.display.mode);
            mode += action.find(".next") != std::string::npos ? 1 : -1;
            mode = (mode + 3) % 3;
            m_settings.display.mode = static_cast<settings::DisplayMode>(mode);
        } else if (action == "settings.resolution.previous" ||
                   action == "settings.resolution.next") {
            int index = resolution_index(m_settings.display.width, m_settings.display.height);
            index += action.find(".next") != std::string::npos ? 1 : -1;
            index = (index + static_cast<int>(kResolutions.size())) %
                    static_cast<int>(kResolutions.size());
            m_settings.display.width = kResolutions[static_cast<std::size_t>(index)].first;
            m_settings.display.height = kResolutions[static_cast<std::size_t>(index)].second;
        } else if (action == "settings.preset.previous" || action == "settings.preset.next") {
            int preset = static_cast<int>(m_settings.preset);
            if (preset >= static_cast<int>(settings::QualityPreset::Custom)) {
                preset = static_cast<int>(settings::QualityPreset::Medium);
            }
            preset += action.find(".next") != std::string::npos ? 1 : -1;
            preset = (preset + 4) % 4;
            const auto quality =
                settings::settings_for_preset(static_cast<settings::QualityPreset>(preset));
            m_settings.preset = quality.preset;
            m_settings.world = quality.world;
            m_settings.render = quality.render;
        } else if (action == "settings.bgm.toggle") {
            m_settings.audio.bgm_enabled = !m_settings.audio.bgm_enabled;
        } else if (action == "settings.listener_follows_camera.toggle") {
            m_settings.audio.listener_follows_camera = !m_settings.audio.listener_follows_camera;
        } else if (action == "settings.grass.toggle") {
            m_settings.render.grass_enabled = !m_settings.render.grass_enabled;
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.bloom.previous" || action == "settings.bloom.next") {
            int quality = static_cast<int>(m_settings.render.bloom);
            quality += action.find(".next") != std::string::npos ? 1 : -1;
            quality = (quality + 3) % 3;
            m_settings.render.bloom = static_cast<settings::BloomQuality>(quality);
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.shadows.previous" || action == "settings.shadows.next") {
            int quality = static_cast<int>(m_settings.render.shadows);
            quality += action.find(".next") != std::string::npos ? 1 : -1;
            quality = (quality + 3) % 3;
            m_settings.render.shadows = static_cast<settings::ShadowQuality>(quality);
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.monsters.previous" || action == "settings.monsters.next") {
            int index = monster_render_limit_index(m_settings.render.max_rendered_monsters);
            index += action.find(".next") != std::string::npos ? 1 : -1;
            index = (index + static_cast<int>(kMonsterRenderLimits.size())) %
                    static_cast<int>(kMonsterRenderLimits.size());
            m_settings.render.max_rendered_monsters =
                kMonsterRenderLimits[static_cast<std::size_t>(index)];
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.players.previous" || action == "settings.players.next") {
            int index = monster_render_limit_index(m_settings.render.max_rendered_players);
            index = (index + (action.find(".next") != std::string::npos ? 1 : -1) + 4) % 4;
            m_settings.render.max_rendered_players =
                kMonsterRenderLimits[static_cast<std::size_t>(index)];
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.player_names.previous" ||
                   action == "settings.player_names.next") {
            const auto found = std::find(kPlayerNameLimits.begin(),
                                         kPlayerNameLimits.end(),
                                         m_settings.render.max_player_names);
            int index = static_cast<int>(std::distance(kPlayerNameLimits.begin(), found));
            index = (index + (action.find(".next") != std::string::npos ? 1 : -1) + 6) % 6;
            m_settings.render.max_player_names = kPlayerNameLimits[static_cast<std::size_t>(index)];
            m_settings.preset = settings::QualityPreset::Custom;
        } else if (action == "settings.effect_volume.decrease") {
            --m_settings.audio.effect_volume;
        } else if (action == "settings.effect_volume.increase") {
            ++m_settings.audio.effect_volume;
        } else if (action == "settings.bgm_volume.decrease") {
            --m_settings.audio.bgm_volume;
        } else if (action == "settings.bgm_volume.increase") {
            ++m_settings.audio.bgm_volume;
        } else {
            change_advanced(action);
        }
        (void) settings::validate_client_settings(m_settings);
        return SettingsWindowAction::Consumed;
    }

    void SettingsWindow::change_advanced(const std::string& action) {
        int direction = 0;
        if (action.size() >= 9 && action.compare(action.size() - 9, 9, ".increase") == 0) {
            direction = 1;
        } else if (action.size() >= 9 && action.compare(action.size() - 9, 9, ".decrease") == 0) {
            direction = -1;
        }
        if (direction == 0) {
            return;
        }
        bool changed = true;
        if (action.find("view_distance") != std::string::npos) {
            m_settings.world.view_distance_tiles += direction;
        } else if (action.find("preload_margin") != std::string::npos) {
            m_settings.world.preload_margin_tiles += direction;
        } else if (action.find("resident_limit") != std::string::npos) {
            m_settings.world.resident_tile_limit += direction * 8;
        } else if (action.find("upload_budget") != std::string::npos) {
            m_settings.world.upload_budget_ms += static_cast<double>(direction) * 0.25;
        } else if (action.find("effect_distance") != std::string::npos) {
            m_settings.world.effect_distance += direction * 50;
        } else {
            changed = false;
        }
        if (changed) {
            m_settings.preset = settings::QualityPreset::Custom;
        }
    }

    bool SettingsWindow::contains(float mouse_x, float mouse_y) const {
        return m_open &&
               WidgetTools::window_outer_hit(
                   mouse_x, mouse_y, m_x, m_y, m_w, m_h, m_scale, UiView::frame_style(m_document));
    }

    void SettingsWindow::draw(engine::IEngine& engine,
                              WidgetTools& ui,
                              const FreetypeFont& font,
                              const engine::FrameInput* input) const {
        if (!m_open) {
            return;
        }
        UiViewHost host;
        if (input) {
            const UiHit hit = UiView::hit(m_document,
                                          &ui,
                                          input->mouse_x,
                                          input->mouse_y,
                                          m_x,
                                          m_y,
                                          m_w,
                                          m_h,
                                          m_scale,
                                          make_vars());
            host.set_pointer_hit(hit.widget_id(), input->mouse_left);
        }
        UiView::draw(engine,
                     ui,
                     m_document,
                     m_x,
                     m_y,
                     m_w,
                     m_h,
                     m_scale,
                     make_vars(),
                     font,
                     font,
                     &host,
                     true);
    }

    const settings::ClientSettings& SettingsWindow::settings() const {
        return m_settings;
    }

    void SettingsWindow::set_status(std::string status) {
        m_status = std::move(status);
    }

    void SettingsWindow::reload_document() {
        UiView::reload(m_document, m_layout);
    }

    void SettingsWindow::sync_from_layout() {
        reload_document();
        const auto frame = UiView::frame_style(m_document);
        m_scale = m_requested_scale;
        if (m_screen_w > 0 && m_screen_h > 0) {
            const float outer_w = m_document.window_w() + frame.pad_left() + frame.pad_right();
            const float outer_h = m_document.window_h() + frame.pad_top() + frame.pad_bottom();
            m_scale = std::max(
                0.25f,
                std::min(
                    {m_scale, (m_screen_w - 24.0f) / outer_w, (m_screen_h - 24.0f) / outer_h}));
        }
        m_w = m_document.window_w() * m_scale;
        m_h = m_document.window_h() * m_scale;
        if (m_screen_w > 0 && m_screen_h > 0) {
            WidgetTools::center_rect(m_x, m_y, m_w, m_h, m_screen_w, m_screen_h);
            m_x += (frame.pad_left() - frame.pad_right()) * m_scale * 0.5f;
            m_y += (frame.pad_top() - frame.pad_bottom()) * m_scale * 0.5f;
        }
    }

    UiVarMap SettingsWindow::make_vars() const {
        UiVarMap vars;
        ui_fill_theme_vars(m_document, vars);
        vars["settings.title"] = "Options";
        vars["settings.tab"] = m_tab;
        vars["settings.world_tab"] = "World";
        vars["settings.render_tab"] = "Graphics";
        for (const auto* tab : {"display", "world", "render", "audio", "battle", "misc"}) {
            vars[std::string("settings.page.") + tab] = m_tab == tab ? "true" : "false";
        }
        vars["settings.misc_heading"] = "Misc";
        vars["settings.clock_label"] = "Show server clock";
        vars["settings.clock_checked"] = m_settings.clock_enabled ? "X" : "";
        vars["settings.show_own_name_label"] = "Show own name";
        vars["settings.show_own_name_checked"] = m_settings.show_own_name ? "X" : "";
        vars["settings.ignore_party_label"] = "Ignore party invitations";
        vars["settings.ignore_party"] = m_settings.ignore_party_requests ? "On" : "Off";
        vars["settings.battle_heading"] = "Battle messages";
        vars["settings.battle_note"] =
            "Choose which events appear in the feed. Saved for this character.";
        const bool battle_visible = m_tab == "battle" && m_character_available;
        vars["settings.page.battle"] = battle_visible ? "true" : "false";
        vars["settings.battle_available"] = m_character_available ? "true" : "false";
        vars["settings.battle_unavailable"] = m_character_available ? "false" : "true";
        for (std::size_t i = 0; i < ChatFilterModel::kBattleCount; ++i) {
            const auto kind = static_cast<protocol::BattleMsgKind>(i);
            const std::string key = "settings.battle." + std::to_string(i);
            vars[key] = ChatFilterModel::battle_label(kind);
            vars[key + ".active"] =
                battle_visible && m_chat_filters.battle_enabled(kind) ? "true" : "false";
            vars[key + ".inactive"] =
                battle_visible && !m_chat_filters.battle_enabled(kind) ? "true" : "false";
        }
        vars["settings.battle.system"] = "System messages";
        vars["settings.battle.system.active"] =
            battle_visible && m_chat_filters.system_enabled() ? "true" : "false";
        vars["settings.battle.system.inactive"] =
            battle_visible && !m_chat_filters.system_enabled() ? "true" : "false";
        vars["settings.display_heading"] = "Display";
        vars["settings.mode_label"] = "Window mode";
        vars["settings.mode"] = display_mode_label(m_settings.display.mode);
        vars["settings.resolution_label"] = "Resolution";
        vars["settings.resolution"] = std::to_string(m_settings.display.width) + " x " +
                                      std::to_string(m_settings.display.height);
        vars["settings.world_heading"] = "World and effects";
        vars["settings.preset_label"] = "Quality preset";
        vars["settings.preset"] = preset_label(m_settings.preset);
        vars["settings.view_distance_label"] = "View radius (tiles)";
        vars["settings.view_distance"] = std::to_string(m_settings.world.view_distance_tiles);
        vars["settings.preload_margin_label"] = "Preload margin (tiles)";
        vars["settings.preload_margin"] = std::to_string(m_settings.world.preload_margin_tiles);
        vars["settings.resident_limit_label"] = "Resident tile limit";
        vars["settings.resident_limit"] = std::to_string(m_settings.world.resident_tile_limit);
        vars["settings.upload_budget_label"] = "Terrain upload budget";
        std::ostringstream budget;
        budget << std::fixed << std::setprecision(2) << m_settings.world.upload_budget_ms << " ms";
        vars["settings.upload_budget"] = budget.str();
        vars["settings.effect_distance_label"] = "Effect distance";
        vars["settings.effect_distance"] = std::to_string(m_settings.world.effect_distance);
        vars["settings.restart_note"] = m_character_available
                                            ? "World settings apply after restarting the client."
                                            : "World settings apply when you start Single Player.";
        vars["settings.status"] = m_status;
        vars["settings.render_heading"] = "Rendering";
        vars["settings.grass_label"] = "Terrain grass";
        vars["settings.grass"] = m_settings.render.grass_enabled ? "On" : "Off";
        vars["settings.bloom_label"] = "Bloom";
        vars["settings.bloom"] = bloom_label(m_settings.render.bloom);
        vars["settings.shadows_label"] = "Ground shadows";
        vars["settings.shadows"] = shadow_label(m_settings.render.shadows);
        vars["settings.monsters_label"] = "Rendered monsters";
        vars["settings.monsters"] =
            monster_render_limit_label(m_settings.render.max_rendered_monsters);
        vars["settings.players_label"] = "Rendered players";
        vars["settings.players"] =
            monster_render_limit_label(m_settings.render.max_rendered_players);
        vars["settings.player_names_label"] = "Player nameplates";
        vars["settings.player_names"] =
            m_settings.render.max_player_names < 0
                ? "Off"
                : monster_render_limit_label(m_settings.render.max_player_names);
        vars["settings.audio_heading"] = "Audio";
        vars["settings.effect_volume_label"] = "Effects";
        vars["settings.effect_volume"] = std::to_string(m_settings.audio.effect_volume);
        vars["settings.bgm_volume_label"] = "Background music";
        vars["settings.bgm_volume"] = std::to_string(m_settings.audio.bgm_volume);
        vars["settings.bgm_enabled_label"] = "BGM";
        vars["settings.bgm_enabled"] = m_settings.audio.bgm_enabled ? "On" : "Off";
        vars["settings.listener_follows_camera_label"] = "Camera-based audio";
        vars["settings.listener_follows_camera"] =
            m_settings.audio.listener_follows_camera ? "On" : "Off";
        vars["settings.save"] = "Save";
        vars["settings.cancel"] = "Cancel";
        vars["settings.previous"] = "<";
        vars["settings.next"] = ">";
        return vars;
    }

}  // namespace openkal::client
