/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CharacterCreationDraft.h"

#include "openkal/protocol/CharacterName.h"

#include <numeric>
#include <utility>

namespace openkal::client {
    namespace {
        // Creation baselines before distributing the five optional primary-stat points.
        constexpr int kBaseStats[4][5] = {
            {18, 16, 8, 8, 10}, {8, 10, 18, 16, 8}, {14, 10, 8, 10, 18}, {14, 10, 8, 10, 18}};
    }  // namespace

    bool
    CharacterCreationDraft::select_class(int job, std::vector<int> faces, std::vector<int> hair) {
        if (job < 0 || job >= 4 || faces.empty() || hair.empty()) {
            return false;
        }
        m_class = job;
        m_points.fill(0);
        m_faces = std::move(faces);
        m_hair = std::move(hair);
        m_face_index = m_hair_index = 0;
        return true;
    }
    void CharacterCreationDraft::back() {
        m_class = -1;
    }
    int CharacterCreationDraft::selected_class() const {
        return m_class;
    }
    int CharacterCreationDraft::remaining_points() const {
        return 5 - std::accumulate(m_points.begin(), m_points.end(), 0);
    }
    int CharacterCreationDraft::stat(int index) const {
        return m_class < 0 || index < 0 || index >= 5
                   ? 0
                   : kBaseStats[m_class][index] + m_points[index];
    }
    bool CharacterCreationDraft::adjust_stat(int index, int delta) {
        if (m_class < 0 || index < 0 || index >= 5) {
            return false;
        }
        if ((delta == 1 && remaining_points() > 0) || (delta == -1 && m_points[index] > 0)) {
            m_points[index] += delta;
            return true;
        }
        return false;
    }
    bool CharacterCreationDraft::cycle_appearance(bool face, int delta) {
        const auto& choices = face ? m_faces : m_hair;
        if (m_class < 0 || choices.size() < 2 || (delta != 1 && delta != -1)) {
            return false;
        }
        int& index = face ? m_face_index : m_hair_index;
        index =
            (index + delta + static_cast<int>(choices.size())) % static_cast<int>(choices.size());
        return true;
    }
    int CharacterCreationDraft::face() const {
        return m_faces.empty() ? 1 : m_faces[m_face_index];
    }
    int CharacterCreationDraft::hair() const {
        return m_hair.empty() ? 1 : m_hair[m_hair_index];
    }
    void CharacterCreationDraft::append_name(std::string_view text) {
        for (const char ch : text) {
            if (m_name.size() >= protocol::kCharacterNameMaximum) {
                break;
            }
            if (protocol::character_name_character(ch)) {
                m_name.push_back(ch);
            }
        }
    }
    void CharacterCreationDraft::erase_name() {
        if (!m_name.empty()) {
            m_name.pop_back();
        }
    }
    const std::string& CharacterCreationDraft::name() const {
        return m_name;
    }
    std::string CharacterCreationDraft::validation_error() const {
        if (m_class < 0) {
            return "Choose a class first.";
        }
        if (!protocol::character_name_valid(m_name)) {
            return "Use 2 to 14 letters or digits; no spaces or special characters.";
        }
        if (remaining_points() > 0) {
            return "Spend all 5 starting stat points.";
        }
        return {};
    }
}  // namespace openkal::client
