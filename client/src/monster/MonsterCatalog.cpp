/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MonsterCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace openkal::client {
    namespace {

        /** Resolves a file in a directory using the case-insensitive lookup of the compatible
         * client.
         */
        [[nodiscard]] std::string find_file_ignore_case(const std::string& directory,
                                                        const std::string& filename) {
            const std::string exact_path = platform::join_path(directory, filename);
            if (platform::file_exists(exact_path)) {
                return exact_path;
            }
            for (const std::string& entry : platform::list_directory(directory)) {
                if (entry.size() != filename.size()) {
                    continue;
                }
                bool matches = true;
                for (std::size_t i = 0; i < entry.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(entry[i])) !=
                        std::tolower(static_cast<unsigned char>(filename[i]))) {
                        matches = false;
                        break;
                    }
                }
                if (matches) {
                    return platform::join_path(directory, entry);
                }
            }
            return {};
        }

        void apply_motion_letters(MonsterRig& rig, const std::string& motion) {
            std::string letters;
            for (char c : motion) {
                if (c == '"' || c == ' ') {
                    continue;
                }
                letters.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            if (letters.empty()) {
                return;
            }
            rig.set_idle_letter(letters[0]);
            if (letters.size() > 1) {
                rig.set_walk_letter(letters[1]);
            }
            if (letters.size() > 2) {
                rig.set_run_letter(letters[2]);
            }
            if (letters.size() > 3) {
                rig.set_die_letter(letters[3]);
            }
            if (letters.size() > 4) {
                rig.set_behead_letter(letters[4]);
            }
            if (letters.size() > 7) {
                rig.set_skill_letter(letters[7]);
            }
            if (letters.size() > 8) {
                rig.set_attack_letter(letters[8]);
            }
            if (letters.size() > 9) {
                rig.set_knee_letter(letters[9]);
            }
        }

        [[nodiscard]] std::vector<MonsterFxAttach> parse_attach_list(const platform::Json& arr) {
            std::vector<MonsterFxAttach> out;
            if (!arr.is_array()) {
                return out;
            }
            for (const auto& row : arr.array()) {
                MonsterFxAttach attach;
                if (row.is_string()) {
                    attach.set_name(row.as_string());
                } else if (row.is_array() && !row.array().empty()) {
                    attach.set_name(row.array()[0].as_string());
                    if (row.array().size() > 1) {
                        attach.set_effect_bone(row.array()[1].as_int(-1));
                    }
                    if (row.array().size() > 2) {
                        attach.set_ox(row.array()[2].as_float(0.0f));
                    }
                    if (row.array().size() > 3) {
                        attach.set_oy(row.array()[3].as_float(0.0f));
                    }
                    if (row.array().size() > 4) {
                        attach.set_oz(row.array()[4].as_float(0.0f));
                    }
                } else {
                    continue;
                }
                if (!attach.name().empty()) {
                    out.push_back(std::move(attach));
                }
            }
            return out;
        }

        MonsterRig rig_from_appearance(const platform::Json& app) {
            MonsterRig rig;
            rig.set_owned_model(app.at("owned_model").as_string());
            int bone_no = app.at("boneId").as_int(1);
            if (bone_no <= 0) {
                bone_no = 1;
            }
            rig.set_bone_no(bone_no);
            rig.set_scale(app.at("scale").as_float(1.0f));
            const int clothes = app.at("clothesIndex").as_int(0);
            if (clothes > 0) {
                rig.set_clothes_index(clothes);
            }
            rig.set_name_offset_y(app.at("scaleParam").as_float(5.0f));
            rig.set_level(app.at("level").as_int(1));
            const platform::Json& mspeed = app.at("mspeed");
            if (mspeed.is_array() && mspeed.array().size() >= 2) {
                rig.set_walk_tick(mspeed.array()[0].as_float(1600.0f));
                rig.set_run_tick(mspeed.array()[1].as_float(800.0f));
            }
            apply_motion_letters(rig, app.at("motion").as_string(""));
            rig.set_has_attack_motion(app.at("attackMotion").as_int(1) != 0);
            const platform::Json& basic_attack_visual = app.at("basicAttackVisual");
            if (basic_attack_visual.is_object() &&
                basic_attack_visual.at("action").as_int(0) == 7) {
                rig.set_basic_attack_visual_motion(basic_attack_visual.at("motion").as_int(0));
            }
            rig.set_particles(parse_attach_list(app.at("particles")));
            rig.set_fx(parse_attach_list(app.at("fx")));
            return rig;
        }

        void apply_quest_slots(MonsterRig& rig, const platform::Json& stats) {
            const platform::Json& quests = stats.at("quests");
            if (!quests.is_array()) {
                return;
            }
            std::vector<MonsterQuestSlot> slots;
            const auto add_slot = [&slots](const platform::Json& row) {
                if (!row.is_array() || row.array().size() < 4) {
                    return;
                }
                MonsterQuestSlot slot;
                slot.set_quest_index(
                    static_cast<std::uint16_t>(std::max(0, row.array()[0].as_int(0))));
                slot.set_flag(
                    static_cast<std::uint8_t>(std::clamp(row.array()[1].as_int(0), 0, 255)));
                const int itemgroup = row.array()[2].as_int(0);
                const int drop_num = row.array()[3].as_int(0);
                if (slot.quest_index() > 0 && itemgroup > 0 && drop_num > 0) {
                    slots.push_back(slot);
                }
            };
            if (!quests.array().empty() && !quests.array().front().is_array()) {
                add_slot(quests);
            } else {
                for (const auto& row : quests.array()) {
                    add_slot(row);
                }
            }
            rig.set_quest_slots(std::move(slots));
        }

    }  // namespace

    void MonsterFxAttach::append_looping_effect_fx(std::string_view option,
                                                   std::vector<MonsterFxAttach>& out) {
        constexpr std::string_view kFx = "(*EFFECT_FX";
        constexpr std::string_view kNr = "_NR";
        std::size_t pos = 0;
        while (pos < option.size()) {
            const auto found = option.find(kFx, pos);
            if (found == std::string_view::npos) {
                break;
            }
            std::size_t cur = found + kFx.size();
            if (cur + kNr.size() <= option.size() && option.substr(cur, kNr.size()) == kNr) {
                pos = cur + kNr.size();
                continue;
            }
            while (cur < option.size() && std::isspace(static_cast<unsigned char>(option[cur]))) {
                ++cur;
            }
            const std::size_t name_begin = cur;
            while (cur < option.size() && option[cur] != ')' &&
                   !std::isspace(static_cast<unsigned char>(option[cur]))) {
                ++cur;
            }
            const std::string name(option.substr(name_begin, cur - name_begin));
            pos = (cur < option.size()) ? cur + 1 : cur;
            if (name.empty()) {
                continue;
            }
            bool exists = false;
            for (const auto& attach : out) {
                if (attach.name() == name) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                continue;
            }
            MonsterFxAttach attach;
            attach.set_name(name);
            out.push_back(std::move(attach));
        }
    }

    namespace {
        void append_effect_fx_cues(std::string_view option,
                                   float time_ms,
                                   bool include_non_repeating,
                                   std::vector<MonsterIdleFxCue>& out) {
            constexpr std::string_view kFx = "(*EFFECT_FX";
            constexpr std::string_view kNr = "_NR";
            std::size_t pos = 0;
            while (pos < option.size()) {
                const auto found = option.find(kFx, pos);
                if (found == std::string_view::npos) {
                    break;
                }
                std::size_t cur = found + kFx.size();
                if (cur + kNr.size() <= option.size() && option.substr(cur, kNr.size()) == kNr) {
                    if (!include_non_repeating) {
                        pos = cur + kNr.size();
                        continue;
                    }
                    cur += kNr.size();
                }
                while (cur < option.size() &&
                       std::isspace(static_cast<unsigned char>(option[cur]))) {
                    ++cur;
                }
                const std::size_t name_begin = cur;
                while (cur < option.size() && option[cur] != ')' &&
                       !std::isspace(static_cast<unsigned char>(option[cur]))) {
                    ++cur;
                }
                const std::string name(option.substr(name_begin, cur - name_begin));
                pos = (cur < option.size()) ? cur + 1 : cur;
                if (name.empty()) {
                    continue;
                }
                MonsterIdleFxCue cue;
                cue.set_name(name);
                cue.set_time_ms(time_ms);
                out.push_back(std::move(cue));
            }
        }
    }  // namespace

    void MonsterIdleFxCue::append_from_option(std::string_view option,
                                              float time_ms,
                                              std::vector<MonsterIdleFxCue>& out) {
        append_effect_fx_cues(option, time_ms, false, out);
    }

    void MonsterIdleFxCue::append_idle_gb_key(std::string_view option,
                                              float time_ms,
                                              std::vector<MonsterFxAttach>& looping_fx,
                                              std::vector<MonsterIdleFxCue>& cues) {
        if (option.find("(*LOOP") != std::string_view::npos) {
            MonsterFxAttach::append_looping_effect_fx(option, looping_fx);
            return;
        }
        append_from_option(option, time_ms, cues);
    }

    void MonsterIdleFxCue::append_idle_clip(const std::vector<std::string>& key_options,
                                            const std::vector<std::uint16_t>& key_times,
                                            std::vector<MonsterFxAttach>& looping_fx,
                                            std::vector<MonsterIdleFxCue>& cues) {
        bool looping_clip = false;
        for (const auto& option : key_options) {
            if (option.find("(*LOOP") != std::string_view::npos) {
                looping_clip = true;
                break;
            }
        }
        if (looping_clip) {
            for (const auto& option : key_options) {
                MonsterFxAttach::append_looping_effect_fx(option, looping_fx);
            }
            return;
        }
        const std::size_t n = std::min(key_options.size(), key_times.size());
        for (std::size_t i = 0; i < n; ++i) {
            append_from_option(key_options[i], static_cast<float>(key_times[i]), cues);
        }
    }

    void MonsterIdleFxCue::append_skill_clip(const std::vector<std::string>& key_options,
                                             const std::vector<std::uint16_t>& key_times,
                                             std::vector<MonsterIdleFxCue>& cues) {
        const std::size_t n = std::min(key_options.size(), key_times.size());
        for (std::size_t i = 0; i < n; ++i) {
            append_effect_fx_cues(key_options[i], static_cast<float>(key_times[i]), true, cues);
        }
    }

    MonsterRig MonsterCatalog::read_appearance(const platform::Json& appearance) {
        return rig_from_appearance(appearance);
    }

    bool MonsterCatalog::load_json_dir(const std::string& dir) {
        m_rigs.clear();
        m_bones.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        struct PendingClothes {
            int index = 0;
            int clothes = 0;
            MonsterRig stub;
        };
        std::vector<PendingClothes> pending;
        for (const auto& name : platform::list_directory(dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            if (name == "bones.json" || name == "manifest.json") {
                continue;
            }
            const std::string path = platform::join_path(dir, name);
            const auto bytes = platform::read_file(path);
            if (!bytes) {
                continue;
            }
            platform::Json root;
            if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            const int index = root.at("index").as_int(0);
            if (index <= 0) {
                continue;
            }
            const platform::Json& app = root.at("appearance");
            const int root_clothes = root.at("clothesIndex").as_int(0);
            if (!app.is_object()) {
                if (root_clothes <= 0) {
                    continue;
                }
                PendingClothes row;
                row.index = index;
                row.clothes = root_clothes;
                const int stats_level = root.at("stats").at("level").as_int(1);
                row.stub.set_level(stats_level > 0 ? stats_level : 1);
                apply_quest_slots(row.stub, root.at("stats"));
                pending.push_back(std::move(row));
                continue;
            }
            m_rigs[index] = read_appearance(app);
            if (m_rigs[index].clothes_index() <= 0) {
                m_rigs[index].set_clothes_index(index);
            }
            if (m_rigs[index].level() <= 0) {
                const int stats_level = root.at("stats").at("level").as_int(1);
                m_rigs[index].set_level(stats_level > 0 ? stats_level : 1);
            }
            apply_quest_slots(m_rigs[index], root.at("stats"));
        }
        for (auto& row : pending) {
            auto it = m_rigs.find(row.clothes);
            if (it == m_rigs.end()) {
                continue;
            }
            MonsterRig rig = it->second;
            rig.set_clothes_index(row.clothes);
            if (row.stub.level() > 0) {
                rig.set_level(row.stub.level());
            }
            if (!row.stub.quest_slots().empty()) {
                rig.set_quest_slots(row.stub.quest_slots());
            }
            m_rigs[row.index] = std::move(rig);
        }

        const std::string bones_path = platform::join_path(dir, "bones.json");
        const auto bytes = platform::read_file(bones_path);
        if (bytes) {
            platform::Json root;
            if (platform::json_parse(std::string(bytes->begin(), bytes->end()), root) &&
                root.is_object()) {
                for (const auto& kv : root.object()) {
                    const int bone_no = [&]() {
                        try {
                            return std::stoi(kv.first);
                        } catch (...) {
                            return 0;
                        }
                    }();
                    if (bone_no <= 0 || !kv.second.is_object()) {
                        continue;
                    }
                    MonsterBoneInfo info;
                    const platform::Json& bone = kv.second.at("bone");
                    if (bone.is_array()) {
                        const auto& arr = bone.array();
                        const int n = std::min(static_cast<int>(arr.size()), kEffectBoneCount);
                        for (int i = 0; i < n; ++i) {
                            info.set_skeleton_bone(i, arr[static_cast<std::size_t>(i)].as_int(0));
                        }
                    }
                    const platform::Json& target = kv.second.at("target");
                    if (target.is_array() && target.array().size() >= 4) {
                        info.set_target_height(target.array()[3].as_float(17.0f));
                    }
                    m_bones[bone_no] = info;
                }
            }
        }
        return !m_rigs.empty() || !m_bones.empty();
    }

    const MonsterRig* MonsterCatalog::find_rig(int index) const {
        auto it = m_rigs.find(index);
        return it == m_rigs.end() ? nullptr : &it->second;
    }

    const MonsterBoneInfo* MonsterCatalog::find_bone(int bone_no) const {
        auto it = m_bones.find(bone_no);
        return it == m_bones.end() ? nullptr : &it->second;
    }

    std::vector<std::string>
    MonsterCatalog::clothes_gb_paths(const std::string& data_root, int index, bool beheaded) {
        std::vector<std::string> paths;
        std::array<char, 64> base{};
        std::snprintf(base.data(), base.size(), "M%03d", index);
        const std::string clothes = platform::join_path(data_root, "Monster/Clothes");
        auto try_add = [&](const char* fmt, int n) {
            std::array<char, 80> name{};
            std::snprintf(name.data(), name.size(), fmt, base.data(), n);
            const std::string p = platform::join_path(clothes, name.data());
            if (platform::file_exists(p)) {
                paths.push_back(p);
            }
        };
        for (int i = 1; i <= 8; ++i) {
            try_add("%s_B%d.gb", i);
            try_add("%s_b%d.gb", i);
        }
        if (beheaded) {
            try_add("%s_H2.gb", 2);
            try_add("%s_h2.gb", 2);
        } else {
            for (int i = 1; i <= 8; ++i) {
                if (i == 2) {
                    continue;
                }
                try_add("%s_H%d.gb", i);
                try_add("%s_h%d.gb", i);
            }
        }
        return paths;
    }

    std::string MonsterCatalog::bone_gb(const std::string& data_root, int bone_no) {
        std::array<char, 64> name{};
        const std::string motion = platform::join_path(data_root, "Monster/Motion");
        std::snprintf(name.data(), name.size(), "T%03d_Bone.gb", bone_no);
        std::string p = platform::join_path(motion, name.data());
        if (platform::file_exists(p)) {
            return p;
        }
        std::snprintf(name.data(), name.size(), "T%03d_bone.gb", bone_no);
        p = platform::join_path(motion, name.data());
        return platform::file_exists(p) ? p : std::string{};
    }

    std::string MonsterCatalog::motion_gb(
        const std::string& data_root, int bone_no, int action, char letter, int variant) {
        std::array<char, 64> name{};
        const std::string motion = platform::join_path(data_root, "Monster/Motion");
        const int requested_variant = std::max(0, variant);
        std::snprintf(name.data(),
                      name.size(),
                      "T%03d_%d_%c_%02d.gb",
                      bone_no,
                      action,
                      letter,
                      requested_variant);
        std::string p = find_file_ignore_case(motion, name.data());
        if (!p.empty()) {
            return p;
        }
        if (requested_variant != 1) {
            std::snprintf(name.data(), name.size(), "T%03d_%d_%c_01.gb", bone_no, action, letter);
            p = find_file_ignore_case(motion, name.data());
            if (!p.empty()) {
                return p;
            }
        }
        std::snprintf(name.data(), name.size(), "T%03d_%d_A_01.gb", bone_no, action);
        return find_file_ignore_case(motion, name.data());
    }

}  // namespace openkal::client
