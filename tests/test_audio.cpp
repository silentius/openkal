/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/client/AnimSound.h"
#include "openkal/client/Audio.h"
#include "openkal/client/BgmController.h"
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GbModel.h"
#include "test_util.h"

#include <filesystem>
#include <string>

int main() {
    using openkal::client::AnimSound;
    using openkal::client::Audio;
    using openkal::client::BgmController;
    using openkal::client::BgmScene;
    using openkal::client::SoundCue;
    using openkal::test::expect;

    {
        const SoundCue cue = Audio::parse_name("efs011_L)");
        expect(cue.stem() == "efs011", "parse_name strips _L and )");
        expect(cue.looping(), "parse_name _L loops");
    }

    {
        const SoundCue cue = Audio::parse_name("efs021");
        expect(cue.stem() == "efs021", "parse_name plain stem");
        expect(!cue.looping(), "parse_name no loop without _L");
    }

    {
        SoundCue cue;
        expect(Audio::parse_option("(*SOUND efc001)", cue), "parse_option SOUND");
        expect(cue.stem() == "efc001", "parse_option efc001 stem");
        expect(!cue.looping(), "parse_option efc001 one-shot");
    }

    {
        SoundCue cue;
        expect(Audio::parse_option("walk (*SOUND efs011_L) extra", cue),
               "parse_option embedded _L");
        expect(cue.stem() == "efs011", "parse_option strips _L");
        expect(cue.looping(), "parse_option _L loops");
    }

    {
        SoundCue cue;
        expect(!Audio::parse_option("(*EFFECT_ARROW)", cue), "parse_option ignores non-SOUND");
    }

    {
        const std::string root = openkal::test::temp_dir("audio_sound");
        const std::string wav = openkal::platform::join_path(root, "Sound/efs021.wav");
        expect(openkal::test::write_bytes(wav, {1, 2, 3, 4}), "write stub wav");
        const std::string path = Audio::resolve_wav_path(root, "efs021");
        expect(path.find("Sound") != std::string::npos, "resolve_wav_path has Sound/");
        expect(path.find("efs021.wav") != std::string::npos, "resolve_wav_path efs021.wav");
        expect(openkal::platform::file_exists(path), "resolve_wav_path exists");
        expect(Audio::resolve_wav_path(root, "").empty(), "empty stem");
    }

    {
        const std::string root = openkal::test::temp_dir("audio_archive");
        std::string error;
        expect(openkal::platform::ConfigArchive::mount(root, OPENKAL_TEST_CONFIG_ARCHIVE, &error),
               "mount packaged runtime sounds");
        for (const auto* stem : {"quest_accept", "quest_update", "quest_done"}) {
            const std::string packaged = Audio::resolve_wav_path(
                {}, stem, openkal::platform::join_path(root, "RuntimeData"));
            expect(packaged.find(std::string("sounds/") + stem + ".mp3") != std::string::npos,
                   "default config.pak contains the synthesized quest cue");
            expect(openkal::platform::file_exists(packaged), "packaged quest cue is readable");
        }
        openkal::platform::ConfigArchive::unmount();
    }

    {
        const std::string root = openkal::test::temp_dir("audio_runtime_priority");
        std::filesystem::remove_all(root);
        const auto runtime = openkal::platform::join_path(root, "RuntimeData");
        const auto owned = openkal::platform::join_path(runtime, "sounds/quest_accept.mp3");
        const auto client = openkal::platform::join_path(root, "Sound/quest_accept.wav");
        expect(openkal::test::write_bytes(owned, {1}), "write cue fixture");
        expect(openkal::test::write_bytes(client, {3}), "write client cue fixture");
        expect(Audio::resolve_wav_path(root, "quest_accept", runtime) == owned,
               "packaged sounds/ MP3 takes precedence over compatible client WAVs");
        const auto optional = openkal::platform::join_path(runtime, "sounds/legacy.wav");
        expect(openkal::test::write_bytes(optional, {4}), "write legacy-only cue fixture");
        expect(Audio::resolve_wav_path(root, "legacy", runtime) == optional,
               "optional sounds/ cues still resolve when there is no cue");
        std::filesystem::remove_all(root);
    }

    {
        expect(BgmController::track_for(BgmScene::CharacterSelection) == "bgm001",
               "character selection uses bgm001");
        expect(BgmController::track_for(BgmScene::World) == "bgm003", "world uses bgm003");
    }

    {
        Audio mixer;
        const bool opened = mixer.init("/nonexistent/openkal_audio_null");
        (void) opened;
        mixer.set_listener(0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        mixer.set_effect_volume(1.0f);
        mixer.set_music_volume(0.6f);
        expect(mixer.music_volume() == 0.6f, "music volume is independent");
        expect(!mixer.play_music("missing_bgm"), "missing BGM is silent");
        mixer.stop_music();
        const auto h = mixer.play("efs021", 0.0f, 0.0f, 0.0f);
        expect(!h.valid(), "missing wav is silent");
        mixer.stop(h);
        (void) mixer.play_ui("click");
        mixer.update();
        mixer.shutdown();
        expect(true, "mixer init/play without device does not crash");
    }

    {
        Audio mixer;
        (void) mixer.init("/nonexistent/openkal_audio_null");
        openkal::terrain::GbAnimClip clip;
        clip.key_times = {100};
        clip.key_options = {"(*SOUND efc001)"};
        AnimSound tracker;
        tracker.poll(mixer, clip, 50.0f, false, 0.0f, 0.0f, 0.0f);
        tracker.poll(mixer, clip, 150.0f, false, 0.0f, 0.0f, 0.0f);
        tracker.reset();
        expect(true, "AnimSound poll does not crash");
        tracker.stop(mixer);
        expect(true, "AnimSound stop does not crash");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
