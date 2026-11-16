/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TileExporter.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplLoader.h"

#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::tile_view {
    namespace {

        std::string to_lower(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool iequals(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                const unsigned char ca = static_cast<unsigned char>(a[i]);
                const unsigned char cb = static_cast<unsigned char>(b[i]);
                if (std::tolower(ca) != std::tolower(cb)) {
                    return false;
                }
            }
            return true;
        }

        std::string dirname_of(const std::string& path) {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string::npos) {
                return ".";
            }
            if (slash == 0) {
                return "/";
            }
            return path.substr(0, slash);
        }

        std::string basename_of(const std::string& path) {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string::npos) {
                return path;
            }
            return path.substr(slash + 1);
        }

        std::string strip_ext(std::string name) {
            const auto dot = name.find_last_of('.');
            if (dot != std::string::npos) {
                name.resize(dot);
            }
            return name;
        }

        std::string patch_stem(char prefix, int patch_x, int patch_y) {
            char stem[32];
            std::snprintf(stem, sizeof(stem), "%c_%03d_%03d", prefix, patch_x, patch_y);
            return stem;
        }

        std::string find_existing(const std::vector<std::string>& candidates) {
            for (const auto& c : candidates) {
                if (platform::file_exists(c)) {
                    return c;
                }
            }
            return {};
        }

        std::string
        find_maps_file(const std::string& maps_dir, int patch_x, int patch_y, const char* ext) {
            const std::string n = patch_stem('n', patch_x, patch_y);
            const std::string m = patch_stem('m', patch_x, patch_y);
            std::string ext_upper = ext;
            for (char& c : ext_upper) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return find_existing({
                platform::join_path(maps_dir, n + ext),
                platform::join_path(maps_dir, n + ext_upper),
                platform::join_path(maps_dir, m + ext),
                platform::join_path(maps_dir, m + ext_upper),
            });
        }

        std::string relative_to_root(const std::string& abs_path, const std::string& root) {
            const std::string a = platform::normalize_separators(abs_path);
            std::string r = platform::normalize_separators(root);
            while (!r.empty() && r.back() == '/') {
                r.pop_back();
            }
            if (a.size() > r.size() && a[r.size()] == '/' && iequals(a.substr(0, r.size()), r)) {
                return a.substr(r.size() + 1);
            }
            return {};
        }

        std::string find_tex_in_dir(const std::string& dir, const std::string& stem) {
            if (dir.empty() || stem.empty() || !platform::is_directory(dir)) {
                return {};
            }
            static const char* kExts[] = {".gtx", ".GTX", ".dds", ".DDS"};
            for (const char* ext : kExts) {
                const std::string path = platform::join_path(dir, stem + ext);
                if (platform::file_exists(path)) {
                    return path;
                }
            }
            const std::string want = to_lower(strip_ext(stem));
            for (const auto& ent : platform::list_directory(dir)) {
                const std::string e_stem = to_lower(strip_ext(ent));
                std::string e_ext = ent;
                const auto dot = e_ext.find_last_of('.');
                e_ext = dot == std::string::npos ? std::string{} : to_lower(e_ext.substr(dot));
                if (e_stem == want && (e_ext == ".gtx" || e_ext == ".dds")) {
                    return platform::join_path(dir, ent);
                }
            }
            return {};
        }

        std::string objects_dir_from_model(const std::string& model_gb_path) {
            std::string cur = platform::normalize_separators(model_gb_path);
            while (true) {
                const auto slash = cur.find_last_of('/');
                if (slash == std::string::npos) {
                    break;
                }
                if (to_lower(cur.substr(slash + 1)) == "objects") {
                    return cur;
                }
                cur = cur.substr(0, slash);
            }
            return {};
        }

        std::string find_gb_texture_file(const std::string& model_gb_path,
                                         const std::string& texture_name) {
            if (texture_name.empty()) {
                return {};
            }
            const std::string dir = dirname_of(model_gb_path);
            std::string base = basename_of(texture_name);
            const std::string stem = strip_ext(base);
            const std::string found = find_tex_in_dir(platform::join_path(dir, "tex"), stem);
            if (!found.empty()) {
                return found;
            }
            const std::string found_caps = find_tex_in_dir(platform::join_path(dir, "Tex"), stem);
            if (!found_caps.empty()) {
                return found_caps;
            }
            const std::string beside = find_tex_in_dir(dir, stem);
            if (!beside.empty()) {
                return beside;
            }
            const std::string objects = objects_dir_from_model(model_gb_path);
            if (!objects.empty()) {
                const std::string common =
                    find_tex_in_dir(platform::join_path(objects, "common/tex"), stem);
                if (!common.empty()) {
                    return common;
                }
            }
            return {};
        }

        std::vector<std::string> dest_aliases(const std::string& dest) {
            const std::string dir = dirname_of(dest);
            const std::string want = basename_of(dest);
            std::vector<std::string> aliases;
            if (platform::is_directory(dir)) {
                for (const auto& ent : platform::list_directory(dir)) {
                    if (iequals(ent, want)) {
                        aliases.push_back(platform::join_path(dir, ent));
                    }
                }
            }
            if (aliases.empty()) {
                aliases.push_back(dest);
            }
            return aliases;
        }

        bool copy_abs(const std::string& src, const std::string& dest, ExportResult& result) {
            if (src.empty() || !platform::file_exists(src)) {
                result.set_files_missing(result.files_missing() + 1);
                if (!src.empty()) {
                    result.add_missing_path(src);
                }
                return false;
            }
            const auto bytes = platform::File::read(src);
            if (!bytes) {
                result.set_files_missing(result.files_missing() + 1);
                result.add_missing_path(src);
                return false;
            }
            const auto aliases = dest_aliases(dest);
            if (!platform::ensure_directory(dirname_of(aliases.front()))) {
                result.set_files_missing(result.files_missing() + 1);
                result.add_missing_path(aliases.front());
                return false;
            }
            bool any = false;
            for (const auto& out_path : aliases) {
                if (!platform::File::write(out_path, bytes->data(), bytes->size())) {
                    result.set_files_missing(result.files_missing() + 1);
                    result.add_missing_path(out_path);
                    continue;
                }
                any = true;
            }
            if (any) {
                result.set_files_copied(result.files_copied() + 1);
            }
            return any;
        }

        bool copy_under_data(const std::string& src,
                             const std::string& data_root,
                             const std::string& dest_root,
                             ExportResult& result) {
            if (src.empty()) {
                result.set_files_missing(result.files_missing() + 1);
                return false;
            }
            std::string rel = relative_to_root(src, data_root);
            if (rel.empty()) {
                rel = platform::join_path("_external", basename_of(src));
            }
            const std::string dest =
                platform::join_path(platform::join_path(dest_root, "data"), rel);
            return copy_abs(src, dest, result);
        }

    }  // namespace

    ExportResult TileExporter::export_tile(const std::string& data_root,
                                           const std::string& dest_root,
                                           int patch_x,
                                           int patch_y) {
        ExportResult result;
        if (data_root.empty() || dest_root.empty()) {
            result.set_message("export needs --data and --export directories");
            return result;
        }

        const std::string maps_dir = platform::resolve_path_case(data_root, "MAPS");
        const std::string kcm = find_maps_file(maps_dir, patch_x, patch_y, ".kcm");
        const std::string opl = find_maps_file(maps_dir, patch_x, patch_y, ".opl");
        const std::string dat = find_maps_file(maps_dir, patch_x, patch_y, ".dat");
        const std::string env = find_existing({
            platform::join_path(maps_dir, "n.env"),
            platform::join_path(maps_dir, "N.ENV"),
            platform::join_path(maps_dir, "m.env"),
        });
        const std::string tex_dir = platform::join_path(maps_dir, "Tex");

        if (kcm.empty()) {
            result.set_message("missing KCM for patch in " + maps_dir);
            result.add_missing_path(
                platform::join_path(maps_dir, patch_stem('n', patch_x, patch_y) + ".kcm"));
            result.set_files_missing(1);
            return result;
        }

        copy_under_data(kcm, data_root, dest_root, result);
        if (!opl.empty()) {
            copy_under_data(opl, data_root, dest_root, result);
        } else {
            result.set_files_missing(result.files_missing() + 1);
            result.add_missing_path(patch_stem('n', patch_x, patch_y) + ".opl");
        }
        if (!dat.empty()) {
            copy_under_data(dat, data_root, dest_root, result);
        }

        std::set<std::string> terrain_tex;
        terrain::HeightPatch patch;
        if (terrain::load_kcm(kcm, patch)) {
            std::vector<terrain::LayerInfo> env_layers;
            if (!env.empty()) {
                (void) terrain::load_env_layers(env, env_layers);
            }
            for (unsigned char idx : patch.layers) {
                if (idx < env_layers.size()) {
                    const std::string path = find_tex_in_dir(tex_dir, env_layers[idx].texture_name);
                    if (!path.empty()) {
                        terrain_tex.insert(path);
                    } else if (!env_layers[idx].texture_name.empty()) {
                        result.set_files_missing(result.files_missing() + 1);
                        result.add_missing_path("MAPS/Tex/" + env_layers[idx].texture_name);
                    }
                }
            }
        }
        for (const auto& path : terrain_tex) {
            copy_under_data(path, data_root, dest_root, result);
        }

        std::set<std::string> gb_paths;
        if (!opl.empty()) {
            terrain::OplFile file;
            if (terrain::load_opl(opl, file)) {
                for (const auto& inst : file.instances) {
                    if (inst.name.empty()) {
                        continue;
                    }
                    const std::string gb =
                        platform::resolve_client_asset(data_root, inst.name, ".gb");
                    if (gb.empty() || !platform::file_exists(gb)) {
                        result.set_files_missing(result.files_missing() + 1);
                        result.add_missing_path(inst.name);
                        continue;
                    }
                    gb_paths.insert(gb);
                }
            }
        }

        std::set<std::string> gb_tex;
        for (const auto& gb : gb_paths) {
            copy_under_data(gb, data_root, dest_root, result);
            terrain::GbModel model;
            if (!terrain::load_gb(gb, model)) {
                continue;
            }
            for (const auto& mat : model.materials) {
                const std::string tex = find_gb_texture_file(gb, mat.texture);
                if (!tex.empty()) {
                    gb_tex.insert(tex);
                } else if (!mat.texture.empty()) {
                    result.set_files_missing(result.files_missing() + 1);
                    result.add_missing_path(gb + " | " + mat.texture);
                }
            }
        }
        for (const auto& path : gb_tex) {
            copy_under_data(path, data_root, dest_root, result);
        }

        result.set_ok(!kcm.empty() && result.files_copied() > 0);
        result.set_message("copied " + std::to_string(result.files_copied()) + " files, missing " +
                           std::to_string(result.files_missing()) + " -> " +
                           platform::join_path(dest_root, "data"));
        log::info("tile_view export " + result.message());
        for (const auto& miss : result.missing_paths()) {
            log::warn("tile_view export missing " + miss);
        }
        return result;
    }

}  // namespace openkal::tile_view
