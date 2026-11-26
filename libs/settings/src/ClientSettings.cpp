/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/settings/ClientSettings.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace openkal::settings {
    namespace {

        constexpr int kSettingsVersion = 2;

        std::optional<int> parse_int(const platform::ProfileDocument& document,
                                     std::string_view key,
                                     std::vector<std::string>& warnings) {
            const auto value = document.value(key);
            if (!value) {
                return std::nullopt;
            }
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value->c_str(), &end, 10);
            if (errno != 0 || end == value->c_str() || *end != '\0' ||
                parsed < std::numeric_limits<int>::min() ||
                parsed > std::numeric_limits<int>::max()) {
                warnings.push_back(std::string(key) + " is invalid; using its default");
                return std::nullopt;
            }
            return static_cast<int>(parsed);
        }

        std::optional<double> parse_double(const platform::ProfileDocument& document,
                                           std::string_view key,
                                           std::vector<std::string>& warnings) {
            const auto value = document.value(key);
            if (!value) {
                return std::nullopt;
            }
            char* end = nullptr;
            errno = 0;
            const double parsed = std::strtod(value->c_str(), &end);
            if (errno != 0 || end == value->c_str() || *end != '\0' || !std::isfinite(parsed)) {
                warnings.push_back(std::string(key) + " is invalid; using its default");
                return std::nullopt;
            }
            return parsed;
        }

        std::optional<bool> parse_bool(const platform::ProfileDocument& document,
                                       std::string_view key,
                                       std::vector<std::string>& warnings) {
            const auto value = document.value(key);
            if (!value) {
                return std::nullopt;
            }
            if (*value == "1" || *value == "true" || *value == "yes" || *value == "on") {
                return true;
            }
            if (*value == "0" || *value == "false" || *value == "no" || *value == "off") {
                return false;
            }
            warnings.push_back(std::string(key) + " is invalid; using its default");
            return std::nullopt;
        }

        std::optional<DisplayMode> parse_display_mode(std::string_view value) {
            if (value == "windowed") {
                return DisplayMode::Windowed;
            }
            if (value == "maximized") {
                return DisplayMode::Maximized;
            }
            if (value == "borderless") {
                return DisplayMode::Borderless;
            }
            return std::nullopt;
        }

        std::optional<QualityPreset> parse_preset(std::string_view value) {
            if (value == "low") {
                return QualityPreset::Low;
            }
            if (value == "medium") {
                return QualityPreset::Medium;
            }
            if (value == "high") {
                return QualityPreset::High;
            }
            if (value == "ultra") {
                return QualityPreset::Ultra;
            }
            if (value == "custom") {
                return QualityPreset::Custom;
            }
            return std::nullopt;
        }

        std::optional<BloomQuality> parse_bloom_quality(std::string_view value) {
            if (value == "off") {
                return BloomQuality::Off;
            }
            if (value == "subtle") {
                return BloomQuality::Subtle;
            }
            if (value == "high") {
                return BloomQuality::High;
            }
            return std::nullopt;
        }

        std::optional<ShadowQuality> parse_shadow_quality(std::string_view value) {
            if (value == "off") {
                return ShadowQuality::Off;
            }
            if (value == "characters") {
                return ShadowQuality::Characters;
            }
            if (value == "all") {
                return ShadowQuality::All;
            }
            return std::nullopt;
        }

        bool same_world(const WorldSettings& lhs, const WorldSettings& rhs) {
            return lhs.view_distance_tiles == rhs.view_distance_tiles &&
                   lhs.preload_margin_tiles == rhs.preload_margin_tiles &&
                   lhs.resident_tile_limit == rhs.resident_tile_limit &&
                   std::abs(lhs.upload_budget_ms - rhs.upload_budget_ms) < 0.001 &&
                   lhs.animation_distance == rhs.animation_distance &&
                   lhs.effect_distance == rhs.effect_distance;
        }

        bool same_render(const RenderSettings& lhs, const RenderSettings& rhs) {
            return lhs.grass_enabled == rhs.grass_enabled && lhs.bloom == rhs.bloom &&
                   lhs.shadows == rhs.shadows &&
                   lhs.max_rendered_monsters == rhs.max_rendered_monsters &&
                   lhs.max_rendered_players == rhs.max_rendered_players &&
                   lhs.max_player_names == rhs.max_player_names;
        }

        void validate_monster_render_limit(int& value, std::vector<std::string>& warnings) {
            if (value == 0 || value == 10 || value == 25 || value == 50) {
                return;
            }
            value = 25;
            warnings.emplace_back("client.render.max_rendered_monsters is invalid; using 25");
        }

        template <typename T>
        void clamp_value(T& value,
                         T minimum,
                         T maximum,
                         std::string_view key,
                         std::vector<std::string>& warnings) {
            const T clamped = std::clamp(value, minimum, maximum);
            if (clamped != value) {
                value = clamped;
                warnings.push_back(std::string(key) + " was outside the supported range");
            }
        }

    }  // namespace

    std::string_view to_string(TargetHpDisplay value) {
        switch (value) {
        case TargetHpDisplay::TargetWidget:
            return "target_widget";
        case TargetHpDisplay::LargeBar:
            return "large_bar";
        case TargetHpDisplay::SideWidget:
            return "side_widget";
        case TargetHpDisplay::Standard:
        default:
            return "standard";
        }
    }

    std::optional<TargetHpDisplay> parse_target_hp_display(std::string_view value) {
        for (const auto mode : {TargetHpDisplay::Standard,
                                TargetHpDisplay::TargetWidget,
                                TargetHpDisplay::LargeBar,
                                TargetHpDisplay::SideWidget}) {
            if (value == to_string(mode)) {
                return mode;
            }
        }
        return std::nullopt;
    }

    std::string_view to_string(DisplayMode value) {
        switch (value) {
        case DisplayMode::Maximized:
            return "maximized";
        case DisplayMode::Borderless:
            return "borderless";
        case DisplayMode::Windowed:
        default:
            return "windowed";
        }
    }

    std::string_view to_string(QualityPreset value) {
        switch (value) {
        case QualityPreset::Low:
            return "low";
        case QualityPreset::High:
            return "high";
        case QualityPreset::Ultra:
            return "ultra";
        case QualityPreset::Custom:
            return "custom";
        case QualityPreset::Medium:
        default:
            return "medium";
        }
    }

    std::string_view to_string(BloomQuality value) {
        switch (value) {
        case BloomQuality::Subtle:
            return "subtle";
        case BloomQuality::High:
            return "high";
        case BloomQuality::Off:
        default:
            return "off";
        }
    }

    std::string_view to_string(ShadowQuality value) {
        switch (value) {
        case ShadowQuality::Characters:
            return "characters";
        case ShadowQuality::All:
            return "all";
        case ShadowQuality::Off:
        default:
            return "off";
        }
    }

    ClientSettings settings_for_preset(QualityPreset preset) {
        ClientSettings settings;
        settings.preset = preset;
        switch (preset) {
        case QualityPreset::Low:
            settings.world = {1, 0, 9, 0.75, 180, 180};
            settings.render = {false, BloomQuality::Off, ShadowQuality::Off, 10, 10, 5};
            break;
        case QualityPreset::High:
            settings.world = {3, 1, 81, 2.5, 500, 500};
            settings.render = {true, BloomQuality::Subtle, ShadowQuality::All, 50, 50, 25};
            break;
        case QualityPreset::Ultra:
            settings.world = {4, 2, 169, 4.0, 800, 800};
            settings.render = {true, BloomQuality::High, ShadowQuality::All, 0, 0, 0};
            break;
        case QualityPreset::Custom:
        case QualityPreset::Medium:
        default:
            settings.preset =
                preset == QualityPreset::Custom ? QualityPreset::Custom : QualityPreset::Medium;
            settings.world = {2, 0, 25, 1.5, 300, 300};
            settings.render = {true, BloomQuality::Subtle, ShadowQuality::All, 25, 25, 10};
            break;
        }
        return settings;
    }

    ClientSettingsLoadResult load_client_settings(const platform::ProfileDocument& document) {
        ClientSettingsLoadResult result;
        if (const auto value = document.value("client.quality.preset")) {
            if (const auto preset = parse_preset(*value)) {
                result.settings = settings_for_preset(*preset);
            } else {
                result.warnings.emplace_back("client.quality.preset is invalid; using medium");
            }
        }
        if (const auto version = parse_int(document, "client.settings_version", result.warnings)) {
            result.settings.version = *version;
        }

        if (const auto value = document.value("client.display.mode")) {
            if (const auto mode = parse_display_mode(*value)) {
                result.settings.display.mode = *mode;
            } else {
                result.warnings.emplace_back("client.display.mode is invalid; using windowed");
            }
        }
        if (const auto value = parse_int(document, "client.display.width", result.warnings)) {
            result.settings.display.width = *value;
        }
        if (const auto value = parse_int(document, "client.display.height", result.warnings)) {
            result.settings.display.height = *value;
        }

        bool has_explicit_world = false;
        const auto assign_world_int = [&](std::string_view key, int& destination) {
            if (const auto value = parse_int(document, key, result.warnings)) {
                destination = *value;
                has_explicit_world = true;
            }
        };
        assign_world_int("client.world.view_distance_tiles",
                         result.settings.world.view_distance_tiles);
        assign_world_int("client.world.preload_margin_tiles",
                         result.settings.world.preload_margin_tiles);
        assign_world_int("client.world.resident_tile_limit",
                         result.settings.world.resident_tile_limit);
        if (const auto value =
                parse_double(document, "client.world.upload_budget_ms", result.warnings)) {
            result.settings.world.upload_budget_ms = *value;
            has_explicit_world = true;
        }
        assign_world_int("client.world.animation_distance",
                         result.settings.world.animation_distance);
        assign_world_int("client.world.effect_distance", result.settings.world.effect_distance);

        bool has_explicit_render = false;
        if (const auto value =
                parse_bool(document, "client.render.grass_enabled", result.warnings)) {
            result.settings.render.grass_enabled = *value;
            has_explicit_render = true;
        }
        if (const auto value = document.value("client.render.bloom")) {
            if (const auto quality = parse_bloom_quality(*value)) {
                result.settings.render.bloom = *quality;
                has_explicit_render = true;
            } else {
                result.warnings.emplace_back("client.render.bloom is invalid; using its default");
            }
        }
        if (const auto value = document.value("client.render.shadows")) {
            if (const auto quality = parse_shadow_quality(*value)) {
                result.settings.render.shadows = *quality;
                has_explicit_render = true;
            } else {
                result.warnings.emplace_back("client.render.shadows is invalid; using its default");
            }
        }
        if (const auto value =
                parse_int(document, "client.render.max_rendered_monsters", result.warnings)) {
            result.settings.render.max_rendered_monsters = *value;
            has_explicit_render = true;
        }

        if (const auto value =
                parse_int(document, "client.render.max_rendered_players", result.warnings)) {
            result.settings.render.max_rendered_players = *value;
            has_explicit_render = true;
        }

        if (const auto value =
                parse_int(document, "client.render.max_player_names", result.warnings)) {
            result.settings.render.max_player_names = *value;
            has_explicit_render = true;
        }

        if (const auto value = parse_int(document, "client.audio.effect_volume", result.warnings)) {
            result.settings.audio.effect_volume = *value;
        }
        if (const auto value = parse_int(document, "client.audio.bgm_volume", result.warnings)) {
            result.settings.audio.bgm_volume = *value;
        }
        if (const auto value = parse_bool(document, "client.audio.bgm_enabled", result.warnings)) {
            result.settings.audio.bgm_enabled = *value;
        }
        if (const auto value =
                parse_bool(document, "client.audio.listener_follows_camera", result.warnings)) {
            result.settings.audio.listener_follows_camera = *value;
        }

        if (const auto value =
                parse_bool(document, "client.misc.ignore_party_requests", result.warnings)) {
            result.settings.ignore_party_requests = *value;
        }
        if (const auto value = parse_bool(document, "client.misc.clock_enabled", result.warnings)) {
            result.settings.clock_enabled = *value;
        }
        if (const auto value = parse_bool(document, "client.misc.show_own_name", result.warnings)) {
            result.settings.show_own_name = *value;
        }
        if (const auto value = document.value("client.misc.monster_hp_display")) {
            if (const auto mode = parse_target_hp_display(*value)) {
                result.settings.target_hp_display = *mode;
            } else {
                result.warnings.emplace_back(
                    "client.misc.monster_hp_display is invalid; using standard");
            }
        }
        auto corrections = validate_client_settings(result.settings);
        result.warnings.insert(result.warnings.end(), corrections.begin(), corrections.end());

        if ((has_explicit_world || has_explicit_render) &&
            result.settings.preset != QualityPreset::Custom) {
            const ClientSettings preset = settings_for_preset(result.settings.preset);
            if (!same_world(result.settings.world, preset.world) ||
                !same_render(result.settings.render, preset.render)) {
                result.settings.preset = QualityPreset::Custom;
            }
        }
        return result;
    }

    std::vector<std::string> validate_client_settings(ClientSettings& settings) {
        std::vector<std::string> warnings;
        if (settings.version != kSettingsVersion) {
            settings.version = kSettingsVersion;
            warnings.emplace_back("client.settings_version was migrated to version 2");
        }
        clamp_value(settings.display.width, 640, 16384, "client.display.width", warnings);
        clamp_value(settings.display.height, 480, 16384, "client.display.height", warnings);
        clamp_value(
            settings.world.view_distance_tiles, 1, 8, "client.world.view_distance_tiles", warnings);
        clamp_value(settings.world.preload_margin_tiles,
                    0,
                    4,
                    "client.world.preload_margin_tiles",
                    warnings);
        clamp_value(
            settings.world.upload_budget_ms, 0.25, 12.0, "client.world.upload_budget_ms", warnings);
        clamp_value(settings.world.animation_distance,
                    100,
                    2000,
                    "client.world.animation_distance",
                    warnings);
        clamp_value(
            settings.world.effect_distance, 100, 2000, "client.world.effect_distance", warnings);
        const int radius = settings.world.view_distance_tiles + settings.world.preload_margin_tiles;
        const int minimum_resident = (radius * 2 + 1) * (radius * 2 + 1);
        if (settings.world.resident_tile_limit < minimum_resident) {
            settings.world.resident_tile_limit = minimum_resident;
            warnings.emplace_back(
                "client.world.resident_tile_limit was raised to cover the preload region");
        }
        clamp_value(settings.world.resident_tile_limit,
                    minimum_resident,
                    1024,
                    "client.world.resident_tile_limit",
                    warnings);
        validate_monster_render_limit(settings.render.max_rendered_monsters, warnings);
        auto& players = settings.render.max_rendered_players;
        if (players != 0 && players != 10 && players != 25 && players != 50) {
            players = 25;
            warnings.emplace_back("client.render.max_rendered_players is invalid; using 25");
        }
        auto& names = settings.render.max_player_names;
        if (names != -1 && names != 0 && names != 5 && names != 10 && names != 25 && names != 50) {
            names = 10;
            warnings.emplace_back("client.render.max_player_names is invalid; using 10");
        }
        clamp_value(settings.audio.effect_volume, 0, 5, "client.audio.effect_volume", warnings);
        clamp_value(settings.audio.bgm_volume, 0, 5, "client.audio.bgm_volume", warnings);
        return warnings;
    }

    void store_client_settings(platform::ProfileDocument& document, const ClientSettings& source) {
        ClientSettings settings = source;
        (void) validate_client_settings(settings);
        document.set("client.misc.clock_enabled", settings.clock_enabled ? "1" : "0");
        document.set("client.misc.ignore_party_requests",
                     settings.ignore_party_requests ? "1" : "0");
        document.set("client.misc.show_own_name", settings.show_own_name ? "1" : "0");
        document.set("client.misc.monster_hp_display",
                     std::string(to_string(settings.target_hp_display)));
        document.set("client.settings_version", std::to_string(settings.version));
        document.set("client.display.mode", std::string(to_string(settings.display.mode)));
        document.set("client.display.width", std::to_string(settings.display.width));
        document.set("client.display.height", std::to_string(settings.display.height));
        document.set("client.quality.preset", std::string(to_string(settings.preset)));
        document.set("client.world.view_distance_tiles",
                     std::to_string(settings.world.view_distance_tiles));
        document.set("client.world.preload_margin_tiles",
                     std::to_string(settings.world.preload_margin_tiles));
        document.set("client.world.resident_tile_limit",
                     std::to_string(settings.world.resident_tile_limit));
        std::ostringstream budget;
        budget << std::fixed << std::setprecision(2) << settings.world.upload_budget_ms;
        document.set("client.world.upload_budget_ms", budget.str());
        document.set("client.world.animation_distance",
                     std::to_string(settings.world.animation_distance));
        document.set("client.world.effect_distance",
                     std::to_string(settings.world.effect_distance));
        document.set("client.render.grass_enabled", settings.render.grass_enabled ? "1" : "0");
        document.set("client.render.bloom", std::string(to_string(settings.render.bloom)));
        document.set("client.render.shadows", std::string(to_string(settings.render.shadows)));
        document.set("client.render.max_rendered_monsters",
                     std::to_string(settings.render.max_rendered_monsters));
        document.set("client.render.max_rendered_players",
                     std::to_string(settings.render.max_rendered_players));
        document.set("client.render.max_player_names",
                     std::to_string(settings.render.max_player_names));
        document.set("client.audio.effect_volume", std::to_string(settings.audio.effect_volume));
        document.set("client.audio.bgm_volume", std::to_string(settings.audio.bgm_volume));
        document.set("client.audio.bgm_enabled", settings.audio.bgm_enabled ? "1" : "0");
        document.set("client.audio.listener_follows_camera",
                     settings.audio.listener_follows_camera ? "1" : "0");
    }

}  // namespace openkal::settings
