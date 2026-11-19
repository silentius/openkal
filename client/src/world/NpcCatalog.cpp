/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/NpcCatalog.h"

#include "openkal/map/Coords.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/NpcDefinitions.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

namespace openkal::client {
    namespace {

        NpcRig rig_from_appearance(const platform::Json& app) {
            NpcRig rig;
            int bone_no = app.at("boneId").as_int(0);
            if (bone_no > 0) {
                rig.set_bone_no(bone_no);
            }
            rig.set_scale(app.at("scale").as_float(1.0f));
            rig.set_name_offset_y(app.at("scaleParam").as_float(5.0f));
            rig.set_show_name(app.at("nameFlag").as_int(1) != 0);
            const std::string motion = app.at("motion").as_string("");
            for (char c : motion) {
                if (c == '"' || c == ' ') {
                    continue;
                }
                rig.set_idle_letter(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                break;
            }
            auto parse_attach = [](const platform::Json& arr) {
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
            };
            rig.set_particles(parse_attach(app.at("particles")));
            rig.set_fx(parse_attach(app.at("fx")));
            return rig;
        }

        bool iequals_ascii(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        bool is_npc_clothes_part(const std::string& filename, const std::string& prefix) {
            // N076_b1.gb: prefix + kind + at least one digit + ".gb"
            if (filename.size() < prefix.size() + 5 ||
                !iequals_ascii(filename.substr(0, prefix.size()), prefix) ||
                !iequals_ascii(filename.substr(filename.size() - 3), ".gb")) {
                return false;
            }
            const char kind = static_cast<char>(
                std::tolower(static_cast<unsigned char>(filename[prefix.size()])));
            if (kind != 'b' && kind != 'p') {
                return false;
            }
            for (std::size_t i = prefix.size() + 1; i + 3 < filename.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(filename[i]))) {
                    return false;
                }
            }
            return true;
        }

    }  // namespace

    NpcRig NpcCatalog::read_appearance(const platform::Json& appearance) {
        return rig_from_appearance(appearance);
    }

    bool NpcCatalog::load_json_dir(const std::string& dir) {
        m_spawns.clear();
        m_rigs.clear();
        m_bones.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        for (const auto& root : platform::load_npc_definitions(dir)) {
            const int index = root.at("index").as_int(0);
            const int shape = root.at("shape").as_int(0);
            const platform::Json& xy = root.at("xy");
            if (index <= 0 || !xy.is_array() || xy.array().size() < 2) {
                continue;
            }
            NpcSpawn s;
            s.set_index(index);
            s.set_shape(shape);
            s.set_name(root.at("name").as_string(""));
            s.set_overhead_icon(root.at("overhead_icon").as_string());
            if (root.at("name_offset_y").is_number()) {
                s.set_name_offset_y(root.at("name_offset_y").as_float());
            }
            s.set_name_offset_pixels(root.at("name_offset_pixels").as_float());
            s.set_server_x(xy.array()[0].as_int(0));
            s.set_server_y(xy.array()[1].as_int(0));
            if (xy.array().size() >= 3) {
                s.set_server_z(xy.array()[2].as_int(0));
            }
            const platform::Json& dir_xy = root.at("dir");
            if (dir_xy.is_array() && dir_xy.array().size() >= 2) {
                s.set_dir_x(dir_xy.array()[0].as_int(0));
                s.set_dir_y(dir_xy.array()[1].as_int(0));
            } else {
                s.set_dir_x(s.server_x());
                s.set_dir_y(s.server_y() + 1000);
            }
            s.set_kind(root.at("kind").as_int(0));
            s.set_map(root.at("map").as_int(0));
            const platform::Json& scenario = root.at("scenario");
            if (scenario.is_array() && !scenario.array().empty()) {
                s.set_scenario_slot(scenario.array().front().as_int(-1));
            }
            const platform::Json& quest = root.at("quest");
            if (quest.is_array() && quest.array().size() >= 2) {
                s.set_quest(static_cast<std::uint16_t>(quest.array()[0].as_int(0)),
                            static_cast<std::uint8_t>(quest.array()[1].as_int(0)));
            }
            m_spawns.push_back(std::move(s));

            const platform::Json& app = root.at("appearance");
            if (shape > 0 && app.is_object() && m_rigs.find(shape) == m_rigs.end()) {
                m_rigs[shape] = read_appearance(app);
            }
        }
        std::sort(m_spawns.begin(), m_spawns.end(), [](const NpcSpawn& a, const NpcSpawn& b) {
            return a.index() < b.index();
        });

        const std::string bones_path = platform::join_path(dir, "bones.json");
        const auto bone_bytes = platform::read_file(bones_path);
        if (bone_bytes) {
            platform::Json root;
            if (platform::json_parse(std::string(bone_bytes->begin(), bone_bytes->end()), root) &&
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
                    NpcBoneInfo info;
                    const platform::Json& bone = kv.second.at("bone");
                    if (bone.is_array() && bone.array().size() >= 3) {
                        info.set_head(bone.array()[2].as_int(0));
                    }
                    const platform::Json& target = kv.second.at("target");
                    if (target.is_array() && target.array().size() >= 4) {
                        info.set_target_height(target.array()[3].as_float(17.0f));
                    }
                    m_bones[bone_no] = info;
                }
            }
        }
        return !m_spawns.empty();
    }

    const NpcRig* NpcCatalog::find_rig(int shape) const {
        auto it = m_rigs.find(shape);
        return it == m_rigs.end() ? nullptr : &it->second;
    }

    const NpcBoneInfo* NpcCatalog::find_bone(int bone_no) const {
        auto it = m_bones.find(bone_no);
        return it == m_bones.end() ? nullptr : &it->second;
    }

    float
    NpcCatalog::yaw_from_dir(int origin_patch, int server_x, int server_y, int dir_x, int dir_y) {
        const float wx = coords::to_client(server_x, origin_patch);
        const float wz = coords::to_client(server_y, origin_patch);
        const float dx = wx - coords::to_client(dir_x, origin_patch);
        const float dz = wz - coords::to_client(dir_y, origin_patch);
        return std::atan2(dx, dz);
    }

    std::vector<std::string> NpcCatalog::clothes_gb_paths(const std::string& data_root, int shape) {
        std::vector<std::string> paths;
        char prefix[16];
        std::snprintf(prefix, sizeof(prefix), "N%03d_", shape);
        const std::string clothes = platform::join_path(data_root, "Npc/Clothes");
        for (const auto& ent : platform::list_directory(clothes)) {
            if (!is_npc_clothes_part(ent, prefix)) {
                continue;
            }
            paths.push_back(platform::join_path(clothes, ent));
        }
        std::sort(paths.begin(), paths.end());
        return paths;
    }

    std::string NpcCatalog::bone_gb(const std::string& data_root, int bone_no) {
        char name[64];
        std::snprintf(name, sizeof(name), "T%03d_bone.gb", bone_no);
        std::string p = platform::join_path(platform::join_path(data_root, "Npc/Motion"), name);
        if (platform::file_exists(p)) {
            return p;
        }
        std::snprintf(name, sizeof(name), "T%03d_Bone.gb", bone_no);
        p = platform::join_path(platform::join_path(data_root, "Npc/Motion"), name);
        return platform::file_exists(p) ? p : std::string{};
    }

    std::string NpcCatalog::idle_gb(const std::string& data_root, int bone_no, char letter) {
        char name[64];
        std::snprintf(name, sizeof(name), "T%03d_0_%c_01.gb", bone_no, letter);
        std::string p = platform::join_path(platform::join_path(data_root, "Npc/Motion"), name);
        if (platform::file_exists(p)) {
            return p;
        }
        std::snprintf(name, sizeof(name), "T%03d_0_A_01.gb", bone_no);
        p = platform::join_path(platform::join_path(data_root, "Npc/Motion"), name);
        return platform::file_exists(p) ? p : std::string{};
    }

}  // namespace openkal::client
