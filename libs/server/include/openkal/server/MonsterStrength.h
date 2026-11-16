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

#include <map>
#include <string>
#include <string_view>

namespace openkal::server {

    /** Startup-only modifiers for authored monster stats, before combat derivation. */
    class MonsterStrength {
      public:
        /** Loads and validates monster strength modifiers, resetting to neutral on failure.
         * @param path JSON configuration path.
         * @return True when the complete configuration is valid.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Applies global percentage, level percentage, then a fixed level offset.
         * @param value Authored stat or its loader default.
         * @param level Monster level.
         * @param stat JSON stat name.
         * @return Rounded nonnegative signed 32-bit value.
         * Unsupported stats are returned unchanged.
         */
        [[nodiscard]] int adjust(int value, int level, std::string_view stat) const;

      private:
        struct LevelAdjustment {
            double percent = 0.0;
            std::map<std::string, int, std::less<>> offsets;
        };

        double m_global_percent = 0.0;
        std::map<int, LevelAdjustment> m_levels;
    };

}  // namespace openkal::server
