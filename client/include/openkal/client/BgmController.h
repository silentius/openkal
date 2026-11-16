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

#include <string_view>

namespace openkal::client {

    class Audio;

    /** Compatible-client background-music scenes. */
    enum class BgmScene {
        CharacterSelection,
        World,
    };

    /** Selects and controls the dedicated background-music voice. */
    class BgmController final {
      public:
        /** Binds background music to an audio mixer.
         * @param audio Audio mixer with a dedicated music voice.
         */
        explicit BgmController(Audio& audio);

        /** Applies the profile's enabled state and configured 0--5 volume.
         * @param enabled True to allow scene music.
         * @param volume Compatible-client volume in 0..5.
         */
        void configure(bool enabled, int volume);

        /** Enters a scene and starts its track when enabled.
         * @param scene New client scene.
         */
        void enter(BgmScene scene);

        /** Gets the default track for a scene.
         * @param scene Client scene.
         * @return Track stem.
         */
        [[nodiscard]] static std::string_view track_for(BgmScene scene);

      private:
        Audio& m_audio;
        bool m_enabled = true;
        bool m_has_scene = false;
        BgmScene m_scene = BgmScene::CharacterSelection;
    };

}  // namespace openkal::client
