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

#include "openkal/platform/ProfileDocument.h"

#include <string>
#include <string_view>
#include <vector>

namespace openkal::settings {

    /** Supported client window modes. */
    enum class DisplayMode {
        Windowed,
        Maximized,
        Borderless,
    };

    /** Client resource and quality presets. */
    enum class QualityPreset {
        Low,
        Medium,
        High,
        Ultra,
        Custom,
    };

    /** Bloom postprocessing quality. */
    enum class BloomQuality {
        Off,
        Subtle,
        High,
    };

    /** Projected mesh-shadow coverage. */
    enum class ShadowQuality {
        Off,
        Characters,
        All,
    };

    /** Display settings retained in the active profile. */
    struct DisplaySettings {
        DisplayMode mode = DisplayMode::Windowed;
        int width = 1920;
        int height = 1080;
    };

    /** World streaming and distant-update settings. */
    struct WorldSettings {
        int view_distance_tiles = 2;
        int preload_margin_tiles = 0;
        int resident_tile_limit = 25;
        double upload_budget_ms = 1.5;
        int animation_distance = 300;
        int effect_distance = 300;
    };

    /** Renderer quality controls that can be applied while the client is running. */
    struct RenderSettings {
        bool grass_enabled = true;
        BloomQuality bloom = BloomQuality::Subtle;
        ShadowQuality shadows = ShadowQuality::All;
        /** Maximum visible monster bodies; zero means unlimited. */
        int max_rendered_monsters = 25;
        /** Maximum non-party remote player bodies; own-party members are extra, zero is unlimited.
         */
        int max_rendered_players = 25;
        /** Non-party nameplate limit; zero is unlimited, -1 hides them. Own-party labels stay on.
         */
        int max_player_names = 10;
    };

    /** Effect and background-music settings on the configured 0--5 scale. */
    struct AudioSettings {
        int effect_volume = 5;
        int bgm_volume = 5;
        bool bgm_enabled = true;
        /** Uses the zoom-dependent camera position as the spatial-audio listener. */
        bool listener_follows_camera = false;
    };

    /** Complete UI-independent client settings value. */
    struct ClientSettings {
        int version = 2;
        DisplaySettings display;
        QualityPreset preset = QualityPreset::Medium;
        WorldSettings world;
        RenderSettings render;
        AudioSettings audio;
        /** Blocks party invitations for this client profile. */
        bool ignore_party_requests = false;
        /** Shows the draggable server/session clock HUD. */
        bool clock_enabled = true;
        /** Shows the local character's overhead nameplate. */
        bool show_own_name = false;
    };

    /** Settings plus non-fatal corrections made while loading. */
    struct ClientSettingsLoadResult {
        ClientSettings settings;
        std::vector<std::string> warnings;
    };

    /** Gets a normalized lowercase display-mode name.
     * @param value Display mode.
     * @return Profile value.
     */
    [[nodiscard]] std::string_view to_string(DisplayMode value);

    /** Gets a normalized lowercase preset name.
     * @param value Quality preset.
     * @return Profile value.
     */
    [[nodiscard]] std::string_view to_string(QualityPreset value);

    /** Gets a normalized lowercase bloom-quality name.
     * @param value Bloom quality.
     * @return Profile value.
     */
    [[nodiscard]] std::string_view to_string(BloomQuality value);

    /** Gets a normalized lowercase shadow-quality name.
     * @param value Shadow quality.
     * @return Profile value.
     */
    [[nodiscard]] std::string_view to_string(ShadowQuality value);

    /** Expands a built-in preset to concrete settings.
     * @param preset Preset other than Custom.
     * @return Concrete settings using that preset.
     */
    [[nodiscard]] ClientSettings settings_for_preset(QualityPreset preset);

    /** Loads client settings from a profile document and validates them.
     * @param document Source profile document.
     * @return Settings and any corrections made.
     */
    [[nodiscard]] ClientSettingsLoadResult
    load_client_settings(const platform::ProfileDocument& document);

    /** Validates and normalizes settings in place.
     * @param settings Settings to validate.
     * @return Human-readable corrections made.
     */
    [[nodiscard]] std::vector<std::string> validate_client_settings(ClientSettings& settings);

    /** Writes validated client settings into a profile document.
     * @param document Destination profile document.
     * @param settings Settings to write.
     */
    void store_client_settings(platform::ProfileDocument& document, const ClientSettings& settings);

}  // namespace openkal::settings
