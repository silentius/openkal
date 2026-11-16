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

#include <array>
#include <string>
#include <string_view>

namespace openkal::server {
    /** OpenKal Low Class Essence Box item definition index. */
    inline constexpr int kEssenceBoxItem = 23012;

    /** Server-owned quantity range and independent grade probabilities for essence boxes. */
    class EssenceBox {
      public:
        /** Loads the box configuration; invalid input disables opening.
         * @param path Server JSON file.
         * @return True for a valid quantity range and probabilities totaling 100 percent.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Validates and replaces the configuration.
         * @param text JSON document.
         * @return True on success.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** @return Minimum number of essences, or zero when disabled. */
        int min_count() const {
            return m_min_count;
        }

        /** @return Maximum number of essences, or zero when disabled. */
        int max_count() const {
            return m_max_count;
        }

        /** Selects one essence grade using cumulative decimal probabilities.
         * @param roll Uniform random value in [0,1).
         * @return Essence item index, or zero for invalid configuration/input.
         */
        [[nodiscard]] int item_for_roll(double roll) const;

      private:
        int m_min_count = 0;
        int m_max_count = 0;
        std::array<double, 3> m_chances{};
    };
}  // namespace openkal::server
