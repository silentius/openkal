/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BgmController.h"

#include "openkal/client/Audio.h"

#include <algorithm>

namespace openkal::client {

    BgmController::BgmController(Audio& audio) : m_audio(audio) {}

    void BgmController::configure(bool enabled, int volume) {
        m_enabled = enabled;
        m_audio.set_music_volume(static_cast<float>(std::clamp(volume, 0, 5)) / 5.0f);
        if (!m_enabled) {
            m_audio.stop_music();
        } else if (m_has_scene) {
            (void) m_audio.play_music(track_for(m_scene));
        }
    }

    void BgmController::enter(BgmScene scene) {
        m_scene = scene;
        m_has_scene = true;
        if (m_enabled) {
            (void) m_audio.play_music(track_for(scene));
        } else {
            m_audio.stop_music();
        }
    }

    std::string_view BgmController::track_for(BgmScene scene) {
        return scene == BgmScene::CharacterSelection ? "bgm001" : "bgm003";
    }

}  // namespace openkal::client
