/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ShinsuDb.h"

#include "openkal/client/MonsterCatalog.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cstdlib>

namespace openkal::client {

    bool ShinsuDb::load_dir(const std::string& dir) {
        m_stage_names.clear();
        m_info.clear();
        m_bones.clear();
        bool ok = false;
        {
            const std::string path = platform::join_path(dir, "names.json");
            const auto bytes = platform::read_file(path);
            if (bytes) {
                platform::Json root;
                std::string err;
                if (platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) &&
                    root.is_object()) {
                    const platform::Json* by = root.get("by_race_stage");
                    if (by && by->is_object()) {
                        for (const auto& race_kv : by->object()) {
                            const int race =
                                static_cast<int>(std::strtol(race_kv.first.c_str(), nullptr, 10));
                            if (!race_kv.second.is_object()) {
                                continue;
                            }
                            for (const auto& st_kv : race_kv.second.object()) {
                                const int stage =
                                    static_cast<int>(std::strtol(st_kv.first.c_str(), nullptr, 10));
                                m_stage_names[race][stage] = st_kv.second.as_string();
                            }
                        }
                        ok = true;
                    }
                } else {
                    log::warn("shinsu names.json: " + err);
                }
            }
        }
        {
            const std::string path = platform::join_path(dir, "info.json");
            const auto bytes = platform::read_file(path);
            if (bytes) {
                platform::Json root;
                std::string err;
                if (platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) &&
                    root.is_object()) {
                    const platform::Json* entries = root.get("entries");
                    if (entries && entries->is_array()) {
                        for (const auto& j : entries->array()) {
                            if (!j.is_object()) {
                                continue;
                            }
                            ShinsuInfo info;
                            info.set_key(j.at("key").as_int(0));
                            info.set_race(j.at("race").as_int(0));
                            info.set_stage(j.at("stage").as_int(0));
                            info.set_bone(j.at("bone").as_int(0));
                            const platform::Json& scale = j.at("scale");
                            if (scale.is_array() && !scale.array().empty()) {
                                info.set_scale(static_cast<float>(scale.array()[0].as_float(1.0)));
                            } else if (scale.is_number()) {
                                info.set_scale(static_cast<float>(scale.as_float(1.0)));
                            }
                            const platform::Json& mspeed = j.at("mspeed");
                            if (mspeed.is_array()) {
                                std::vector<int> vals;
                                for (const auto& n : mspeed.array()) {
                                    vals.push_back(n.as_int(0));
                                }
                                info.set_mspeed(std::move(vals));
                            }
                            std::vector<ShinsuInfo::Particle> parts;
                            const platform::Json& particles = j.at("particles");
                            if (particles.is_array()) {
                                for (const auto& p : particles.array()) {
                                    if (!p.is_object()) {
                                        continue;
                                    }
                                    ShinsuInfo::Particle prt;
                                    prt.set_name(p.at("name").as_string());
                                    prt.set_bone(p.at("bone").as_int(0));
                                    const platform::Json& offset = p.at("offset");
                                    if (offset.is_array() && offset.array().size() >= 3) {
                                        const auto& off = offset.array();
                                        prt.set_ox(static_cast<float>(off[0].as_float(0.0)));
                                        prt.set_oy(static_cast<float>(off[1].as_float(0.0)));
                                        prt.set_oz(static_cast<float>(off[2].as_float(0.0)));
                                    }
                                    parts.push_back(std::move(prt));
                                }
                            }
                            info.set_particles(std::move(parts));
                            m_info[info.key()] = std::move(info);
                        }
                        ok = true;
                    }
                } else {
                    log::warn("shinsu info.json: " + err);
                }
            }
        }
        {
            const std::string path = platform::join_path(dir, "bones.json");
            const auto bytes = platform::read_file(path);
            if (bytes) {
                platform::Json root;
                std::string err;
                if (platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &err) &&
                    root.is_object()) {
                    const platform::Json* entries = root.get("entries");
                    if (entries && entries->is_array()) {
                        static_assert(kEffectBoneCount == 8);
                        for (const auto& j : entries->array()) {
                            if (!j.is_object()) {
                                continue;
                            }
                            std::array<int, kEffectBoneCount> bones{};
                            const platform::Json& bone = j.at("bone");
                            if (bone.is_array()) {
                                const int n = std::min(static_cast<int>(bone.array().size()),
                                                       kEffectBoneCount);
                                for (int i = 0; i < n; ++i) {
                                    bones[static_cast<std::size_t>(i)] =
                                        bone.array()[static_cast<std::size_t>(i)].as_int(0);
                                }
                            }
                            m_bones[j.at("key").as_int(0)] = bones;
                        }
                    }
                } else {
                    log::warn("shinsu bones.json: " + err);
                }
            }
        }
        return ok;
    }

    std::string ShinsuDb::stage_name(int race, int stage) const {
        const auto race_it = m_stage_names.find(race);
        if (race_it != m_stage_names.end()) {
            const auto st_it = race_it->second.find(stage);
            if (st_it != race_it->second.end() && !st_it->second.empty()) {
                return st_it->second;
            }
        }
        const char* kind = "ZilPoongShinRhoe";
        if (race == 0) {
            kind = "Tiger";
        } else if (race == 4) {
            kind = "Panther";
        } else if (race == 6) {
            kind = "Bird";
        }
        const char* age = "Baby";
        if (stage == 2) {
            age = "Young";
        } else if (stage >= 3) {
            age = "Perfect";
        }
        return std::string("Ancient ") + age + " " + kind;
    }

    const ShinsuInfo* ShinsuDb::find(int key) const {
        const auto it = m_info.find(key);
        return it == m_info.end() ? nullptr : &it->second;
    }

    const ShinsuInfo* ShinsuDb::find(int race, int stage) const {
        return find(stage * 10 + race);
    }

    int ShinsuDb::skeleton_bone(int bone_table_key, int effect_slot) const {
        if (effect_slot < 0 || effect_slot >= kEffectBoneCount) {
            return -1;
        }
        const auto it = m_bones.find(bone_table_key);
        if (it == m_bones.end()) {
            return -1;
        }
        return it->second[static_cast<std::size_t>(effect_slot)];
    }

}  // namespace openkal::client
