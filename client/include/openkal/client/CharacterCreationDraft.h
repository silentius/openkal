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
#include <vector>

namespace openkal::client {
    /** Editable character draft with a fixed five-point creation budget. */
    class CharacterCreationDraft {
      public:
        /** Selects a class and resets its stats and appearance.
         * @param job Class index, 0..3.
         * @param faces Available face mesh indices.
         * @param hair Available hair mesh indices.
         * @return True when the class and appearance choices are valid.
         */
        [[nodiscard]] bool select_class(int job, std::vector<int> faces, std::vector<int> hair);

        /** Returns to the class picker, retaining the typed name. */
        void back();

        /** @return Selected class index, or -1 in the class picker. */
        int selected_class() const;

        /** @return Points still available to spend. */
        int remaining_points() const;

        /** @param index Stat in STR, HTH, INT, WIS, DEX order.
         * @return Base plus allocated points, or zero for an invalid index/class.
         */
        int stat(int index) const;

        /** @param index Stat index in 0..4.
         * @param delta One point to add (+1) or refund (-1).
         * @return True if the change fits the budget and base-stat floor.
         */
        [[nodiscard]] bool adjust_stat(int index, int delta);

        /** @param face True selects the face; false selects the hairstyle.
         * @param delta Direction (+1 or -1), wrapping across available assets.
         * @return True if the appearance changed.
         */
        [[nodiscard]] bool cycle_appearance(bool face, int delta);

        /** @return One-based face mesh index. */
        int face() const;

        /** @return One-based hair mesh index. */
        int hair() const;

        /** Accepts ASCII letters and digits, up to 14 characters.
         * @param text Characters received from the text-input event.
         */
        void append_name(std::string_view text);

        /** Deletes the last name character. */
        void erase_name();

        /** @return Current name. */
        const std::string& name() const;

        /** @return Empty if ready to create, otherwise an actionable validation message. */
        std::string validation_error() const;

      private:
        int m_class = -1;
        std::array<int, 5> m_points{};
        std::vector<int> m_faces;
        std::vector<int> m_hair;
        int m_face_index = 0;
        int m_hair_index = 0;
        std::string m_name;
    };
}  // namespace openkal::client
