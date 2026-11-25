/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/File.h"
#include "openkal/platform/ProfileDocument.h"
#include "openkal/settings/ClientSettings.h"
#include "test_util.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace {

    std::string temporary_profile_path() {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "/tmp/openkal_client_settings_" + std::to_string(stamp) + ".profile";
    }

}  // namespace

int main() {
    using openkal::test::expect;

    const std::string path = temporary_profile_path();
    const std::string initial = "# Keep this comment\n"
                                "data_root=data\n"
                                "unknown.future=value\n"
                                "client.display.width = 1280\n"
                                "\n"
                                "client.quality.preset=ultra\n"
                                "\n"
                                "client.audio.effect_volume=12\n";
    expect(openkal::platform::File::write(path, initial), "write settings profile fixture");

    std::string error;
    auto document = openkal::platform::ProfileDocument::load(path, &error);
    expect(document.has_value(), "load editable profile document");
    if (!document) {
        return 1;
    }

    auto loaded = openkal::settings::load_client_settings(*document);
    expect(loaded.settings.clock_enabled, "clock defaults to enabled in existing profiles");
    expect(loaded.settings.display.width == 1280, "load display width after preset");
    expect(loaded.settings.preset == openkal::settings::QualityPreset::Ultra, "load ultra preset");
    expect(loaded.settings.world.view_distance_tiles == 4, "expand ultra preset");
    expect(loaded.settings.render.grass_enabled, "ultra enables grass");
    expect(loaded.settings.render.bloom == openkal::settings::BloomQuality::High,
           "ultra enables high bloom");
    expect(loaded.settings.render.shadows == openkal::settings::ShadowQuality::All,
           "ultra enables all projected shadows");
    expect(loaded.settings.render.max_rendered_monsters == 0, "ultra renders all monster bodies");
    expect(loaded.settings.audio.effect_volume == 5, "clamp effect volume");
    expect(!loaded.settings.audio.listener_follows_camera,
           "use character-based audio when the setting is absent");
    expect(!loaded.settings.show_own_name,
           "hide the own nameplate by default in existing profiles");
    expect(!loaded.warnings.empty(), "report corrected profile value");

    const auto medium =
        openkal::settings::settings_for_preset(openkal::settings::QualityPreset::Medium);
    expect(medium.render.grass_enabled, "medium enables terrain decorations");
    expect(medium.render.bloom == openkal::settings::BloomQuality::Subtle,
           "medium enables subtle bloom");
    expect(medium.render.shadows == openkal::settings::ShadowQuality::All,
           "medium enables actor and world shadows");
    expect(medium.render.max_rendered_monsters == 25,
           "medium renders the 25 closest monster bodies");
    expect(openkal::settings::settings_for_preset(openkal::settings::QualityPreset::Low)
                   .render.max_rendered_monsters == 10,
           "low renders the 10 closest monster bodies");
    expect(openkal::settings::settings_for_preset(openkal::settings::QualityPreset::High)
                   .render.max_rendered_monsters == 50,
           "high renders the 50 closest monster bodies");

    {
        auto maximum = loaded.settings;
        maximum.world.view_distance_tiles = 8;
        maximum.world.preload_margin_tiles = 4;
        maximum.world.resident_tile_limit = 1;
        (void) openkal::settings::validate_client_settings(maximum);
        expect(maximum.world.resident_tile_limit == 625,
               "resident limit covers the largest supported preload region");
    }
    {
        auto invalid_limit = loaded.settings;
        invalid_limit.render.max_rendered_monsters = 12;
        const auto warnings = openkal::settings::validate_client_settings(invalid_limit);
        expect(invalid_limit.render.max_rendered_monsters == 25,
               "replace unsupported monster render limit with medium value");
        expect(!warnings.empty(), "report unsupported monster render limit");
    }

    {
        auto invalid = medium;
        invalid.render.max_rendered_players = -1;
        invalid.render.max_player_names = 12;
        const auto warnings = openkal::settings::validate_client_settings(invalid);
        expect(invalid.render.max_rendered_players == 25 && invalid.render.max_player_names == 10 &&
                   !warnings.empty(),
               "invalid player limits fall back to supported defaults");
    }
    loaded.settings.display.mode = openkal::settings::DisplayMode::Borderless;
    loaded.settings.ignore_party_requests = true;
    loaded.settings.clock_enabled = false;
    loaded.settings.show_own_name = true;
    loaded.settings.audio.bgm_volume = 3;
    loaded.settings.audio.listener_follows_camera = true;
    loaded.settings.render.bloom = openkal::settings::BloomQuality::Subtle;
    loaded.settings.render.max_rendered_monsters = 10;
    loaded.settings.render.max_rendered_players = 50;
    loaded.settings.render.max_player_names = -1;
    openkal::settings::store_client_settings(*document, loaded.settings);
    expect(document->text().find("# Keep this comment") != std::string::npos, "preserve comments");
    expect(document->text().find("unknown.future=value") != std::string::npos,
           "preserve unknown keys");
    expect(document->text().find("data_root=data") != std::string::npos,
           "preserve relative asset root");
    expect(document->text().find("client.display.width = 1280") != std::string::npos,
           "preserve key spacing");
    expect(document->text().find("client.render.bloom=subtle") != std::string::npos,
           "store bloom quality");
    expect(document->text().find("client.render.grass_enabled=1") != std::string::npos,
           "store grass toggle");
    expect(document->text().find("client.render.max_rendered_monsters=10") != std::string::npos,
           "store monster render limit");
    expect(document->text().find("client.audio.listener_follows_camera=1") != std::string::npos,
           "store camera-based audio toggle");
    expect(document->text().find("\n\nclient.") == std::string::npos,
           "do not insert blank lines between client settings");
    expect(document->save(&error) == openkal::platform::ProfileSaveStatus::Saved,
           "atomically save settings profile");

    const auto party_settings = openkal::settings::load_client_settings(*document);
    expect(party_settings.settings.render.max_rendered_players == 50 &&
               party_settings.settings.render.max_player_names == -1,
           "independent player body and disabled name limits survive profile roundtrip");
    expect(party_settings.settings.preset == openkal::settings::QualityPreset::Custom,
           "explicit player limits differing from a preset mark the profile custom");
    expect(!party_settings.settings.clock_enabled, "clock preference survives profile roundtrip");
    expect(party_settings.settings.show_own_name,
           "own nameplate preference survives profile roundtrip with remote names disabled");
    expect(party_settings.settings.ignore_party_requests,
           "party ignore preference survives profile roundtrip");
    auto conflict_document = openkal::platform::ProfileDocument::load(path, &error);
    expect(conflict_document.has_value(), "reload settings profile");
    if (conflict_document) {
        conflict_document->set("client.audio.bgm_volume", "1");
        expect(openkal::platform::File::write(path, conflict_document->text() + "# external\n"),
               "simulate external profile edit");
        expect(conflict_document->save(&error) == openkal::platform::ProfileSaveStatus::Conflict,
               "reject conflicting profile save");
    }

    (void) std::remove(path.c_str());
    auto new_document = openkal::platform::ProfileDocument::create(path);
    new_document.set("client.audio.bgm_volume", "2");
    expect(openkal::platform::File::write(path, "# concurrent first save\n"),
           "simulate a competing first save");
    expect(new_document.save(&error) == openkal::platform::ProfileSaveStatus::Conflict,
           "new document does not overwrite a newly created file");
    (void) std::remove(path.c_str());
    return openkal::test::g_failures == 0 ? 0 : 1;
}
