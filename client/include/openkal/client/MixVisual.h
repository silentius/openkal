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

#include <string>

namespace openkal::client {

    /**
     * Mix_type nibble encoding and weapon-mix particle / FX names.
     * Low 4 bits = stone type 1..11, high nibble = mix level 1..2. Legacy
     * level-one instances omit the high nibble.
     */
    class MixVisual {
      public:
        /**
         * Mix stone encoded in mix_type low nibble.
         * @param mix_type Encoded mix_type low 8 bits.
         * @return Stone type 1..11, or 0 when unmixed.
         */
        [[nodiscard]] static int stone_type(int mix_type);

        /**
         * Mix level encoded in mix_type high nibble.
         * @param mix_type Encoded mix_type low 8 bits.
         * @return Mix level 1..2, or 0 when unmixed.
         */
        [[nodiscard]] static int mix_level(int mix_type);

        /**
         * Runtime state (compatibility behavior units, plus 10 at level 2).
         * @param mix_type Encoded mix_type low 8 bits.
         * @return Particle index, or 0. A valid legacy mix_type without a
         * level nibble selects the level-one particle.
         */
        [[nodiscard]] static int magic_stone(int mix_type);

        /**
         * Knight/mage/archer particle shader (`e%c_%04d`).
         * @param job_letter Lowercase job letter (w/r/m/t).
         * @param magic Mix magic-stone index.
         * @return Shader name, or empty.
         */
        [[nodiscard]] static std::string particle_name(char job_letter, int magic);

        /**
         * Thief looping FX shader for a stone type (same name at level 1 and 2).
         * @param stone Mix stone type 1..11.
         * @return FX name, or empty.
         */
        [[nodiscard]] static std::string thief_fx_name(int stone);
    };

}  // namespace openkal::client
