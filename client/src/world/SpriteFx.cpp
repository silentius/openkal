/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SpriteFx.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/Decode4.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

namespace openkal::client {
    namespace {

        constexpr int kMaxParticles = 64;
        constexpr int kMaxTrailPoints = 24;
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDegToRad = kPi / 180.0f;

        [[nodiscard]] float hash01(std::uint32_t n) {
            n ^= n >> 16u;
            n *= 0x7feb352du;
            n ^= n >> 15u;
            n *= 0x846ca68bu;
            n ^= n >> 16u;
            return static_cast<float>(n >> 8u) * (1.0f / 16777216.0f);
        }

        [[nodiscard]] float hash_signed(std::uint32_t n) {
            return hash01(n) * 2.0f - 1.0f;
        }

        void normalize3(float& x, float& y, float& z) {
            const float len = std::sqrt(x * x + y * y + z * z);
            if (len < 1e-6f) {
                x = 0.0f;
                y = 1.0f;
                z = 0.0f;
                return;
            }
            x /= len;
            y /= len;
            z /= len;
        }

        void cone_direction(float ax,
                            float ay,
                            float az,
                            float cone_deg,
                            std::uint32_t seed,
                            float& ox,
                            float& oy,
                            float& oz) {
            normalize3(ax, ay, az);
            float lx = 0.0f;
            float ly = 0.0f;
            float lz = 1.0f;
            if (std::abs(lx - ax) < 1e-5f && std::abs(ly - ay) < 1e-5f) {
                ly = 1.0f;
                lz = 0.0f;
            }
            float tx = ly * az - lz * ay;
            float ty = lz * ax - lx * az;
            float tz = lx * ay - ly * ax;
            normalize3(tx, ty, tz);
            const float spin = hash01(seed + 1u) * (kPi * 2.0f);
            const float cs = std::cos(spin);
            const float ss = std::sin(spin);
            float cx = ay * tz - az * ty;
            float cy = az * tx - ax * tz;
            float cz = ax * ty - ay * tx;
            tx = tx * cs + cx * ss;
            ty = ty * cs + cy * ss;
            tz = tz * cs + cz * ss;
            const float tilt = hash01(seed + 2u) * cone_deg * kDegToRad;
            const float ct = std::cos(tilt);
            const float st = std::sin(tilt);
            cx = ty * az - tz * ay;
            cy = tz * ax - tx * az;
            cz = tx * ay - ty * ax;
            ox = ax * ct + cx * st;
            oy = ay * ct + cy * st;
            oz = az * ct + cz * st;
            normalize3(ox, oy, oz);
        }

        void rotate_axis(float ax, float ay, float az, float angle, float& x, float& y, float& z) {
            normalize3(ax, ay, az);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float ic = 1.0f - c;
            const float px = x;
            const float py = y;
            const float pz = z;
            const float dot = ax * px + ay * py + az * pz;
            x = px * c + (ay * pz - az * py) * s + ax * dot * ic;
            y = py * c + (az * px - ax * pz) * s + ay * dot * ic;
            z = pz * c + (ax * py - ay * px) * s + az * dot * ic;
        }

        [[nodiscard]] bool iequals(std::string_view a, std::string_view b) {
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

        [[nodiscard]] std::string ascii_lowered(std::string_view text) {
            std::string out(text);
            for (char& c : out) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return out;
        }

        [[nodiscard]] bool ends_with_ci(std::string_view name, std::string_view ext) {
            if (name.size() < ext.size()) {
                return false;
            }
            return iequals(name.substr(name.size() - ext.size()), ext);
        }

        void skip_ws(const char*& p) {
            for (;;) {
                while (*p && std::isspace(static_cast<unsigned char>(*p))) {
                    ++p;
                }
                if (p[0] == '/' && p[1] == '/') {
                    while (*p && *p != '\n') {
                        ++p;
                    }
                    continue;
                }
                if (*p == ';') {
                    while (*p && *p != '\n') {
                        ++p;
                    }
                    continue;
                }
                break;
            }
        }

        [[nodiscard]] bool next_token(const char*& p, std::string& out) {
            skip_ws(p);
            if (!*p) {
                return false;
            }
            if (*p == '"') {
                ++p;
                out.clear();
                while (*p && *p != '"') {
                    // Kal shaders use Windows paths (`Data\Effect\...`). Only \" is an
                    // escape; treating \E as C-escape dropped the slash and 1x1-fallback
                    // quads replaced the real GTX.
                    if (*p == '\\' && p[1] == '"') {
                        out.push_back('"');
                        p += 2;
                        continue;
                    }
                    out.push_back(*p++);
                }
                if (*p == '"') {
                    ++p;
                }
                return true;
            }
            if (std::strchr("{}()", *p) != nullptr) {
                out.assign(1, *p++);
                return true;
            }
            out.clear();
            while (*p && !std::isspace(static_cast<unsigned char>(*p)) &&
                   std::strchr("{}()\"", *p) == nullptr) {
                out.push_back(*p++);
            }
            return !out.empty();
        }

        [[nodiscard]] std::string fx_model_stem(std::string_view path) {
            std::string stem(path);
            for (char& c : stem) {
                if (c == '\\') {
                    c = '/';
                }
            }
            const auto slash = stem.find_last_of('/');
            if (slash != std::string::npos) {
                stem.erase(0, slash + 1);
            }
            if (stem.size() >= 3 && iequals(stem.substr(stem.size() - 3), ".gb")) {
                stem.resize(stem.size() - 3);
            }
            return stem;
        }

        [[nodiscard]] bool parse_floats(const char*& p, int count, float* out) {
            std::string tok;
            if (!next_token(p, tok) || tok != "(") {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (!next_token(p, tok)) {
                    return false;
                }
                out[i] = std::strtof(tok.c_str(), nullptr);
            }
            return next_token(p, tok) && tok == ")";
        }

        [[nodiscard]] std::string decode_script(const std::vector<std::uint8_t>& bytes,
                                                bool decode_dat) {
            std::string text;
            text.resize(bytes.size());
            if (decode_dat) {
                for (std::size_t i = 0; i < bytes.size(); ++i) {
                    text[i] = static_cast<char>(terrain::kDecodeTable4[bytes[i]]);
                }
            } else {
                std::memcpy(text.data(), bytes.data(), bytes.size());
            }
            return text;
        }

        void index_quoted_shaders(const std::string& text,
                                  std::unordered_map<std::string, std::string>& out) {
            const char* p = text.c_str();
            std::string tok;
            while (next_token(p, tok)) {
                if (tok.empty() || tok == "{" || tok == "}" || tok == "(" || tok == ")") {
                    continue;
                }
                const char* look = p;
                std::string brace;
                if (!next_token(look, brace) || brace != "{") {
                    continue;
                }
                const char* body = look - 1;  // points at '{'
                int depth = 0;
                const char* q = body;
                while (*q) {
                    if (*q == '"') {
                        ++q;
                        while (*q && *q != '"') {
                            if (*q == '\\' && q[1] == '"') {
                                q += 2;
                                continue;
                            }
                            ++q;
                        }
                        if (*q == '"') {
                            ++q;
                        }
                        continue;
                    }
                    if (*q == '{') {
                        ++depth;
                    } else if (*q == '}') {
                        --depth;
                        if (depth == 0) {
                            ++q;
                            break;
                        }
                    }
                    ++q;
                }
                if (depth == 0 && out.find(tok) == out.end()) {
                    out.emplace(tok, std::string(body, static_cast<std::size_t>(q - body)));
                }
                p = q;
            }
        }

    }  // namespace

    SpriteFx::Emitter::Emitter(std::vector<SpriteStage> stages,
                               float cull_distance,
                               int effect_bone,
                               float ox,
                               float oy,
                               float oz)
        : m_stages(std::move(stages)), m_cull_distance(cull_distance), m_x(0.0f), m_y(0.0f),
          m_z(0.0f), m_effect_bone(effect_bone), m_ox(ox), m_oy(oy), m_oz(oz) {}

    bool SpriteFx::init(engine::IEngine& engine, const std::string& data_root) {
        destroy(engine);
        m_data_root = data_root;
        index_scripts(platform::join_path(data_root, "Effect/Sprite"), true, m_sprite_scripts);
        index_scripts(platform::join_path(data_root, "Effect/Fx"), true, m_fx_scripts);
        m_ready = true;
        if (m_sprite_scripts.empty()) {
            log::warn("SpriteFx: no particle shaders in Effect/Sprite");
        }
        return m_ready;
    }

    void SpriteFx::destroy(engine::IEngine& engine) {
        m_oneshots.clear();
        m_emitters.clear();
        m_sprites.clear();
        m_sprite_scripts.clear();
        m_fx_scripts.clear();
        for (auto& [name, tex] : m_tex_cache) {
            (void) name;
            if (tex) {
                engine.destroy_texture(tex);
            }
        }
        m_tex_cache.clear();
        m_draw_batches.clear();
        m_ready = false;
    }

    void SpriteFx::index_scripts(const std::string& dir,
                                 bool decode_dat,
                                 std::unordered_map<std::string, std::string>& out) {
        if (!platform::is_directory(dir)) {
            return;
        }
        std::vector<std::string> txt;
        std::vector<std::string> dat;
        for (const auto& name : platform::list_directory(dir)) {
            if (ends_with_ci(name, ".txt")) {
                txt.push_back(name);
            } else if (ends_with_ci(name, ".dat")) {
                dat.push_back(name);
            }
        }
        auto load_list = [&](const std::vector<std::string>& names, bool decode) {
            for (const auto& name : names) {
                const std::string path = platform::join_path(dir, name);
                const auto bytes = platform::read_file(path);
                if (!bytes || bytes->empty()) {
                    continue;
                }
                index_quoted_shaders(decode_script(*bytes, decode && decode_dat), out);
            }
        };
        load_list(txt, false);
        load_list(dat, true);
    }

    engine::TextureHandle SpriteFx::load_tex(engine::IEngine& engine, const std::string& name) {
        auto it = m_tex_cache.find(name);
        if (it != m_tex_cache.end()) {
            return it->second;
        }
        terrain::ImageRgba img;
        engine::TextureHandle tex{};
        if (terrain::load_effect_tex_rgba(m_data_root, name, img) && !img.rgba.empty()) {
            tex = engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, false);
        }
        if (!tex) {
            std::uint8_t px[4] = {180, 220, 255, 255};
            tex = engine.create_texture_rgba8(px, 1, 1, true);
            log::warn("SpriteFx: missing Effect texture " + name);
        }
        m_tex_cache.emplace(name, tex);
        return tex;
    }

    SpriteFx::SpriteShader SpriteFx::parse_sprite_fields(std::string_view body) const {
        SpriteShader shader;
        const char* p = body.data();
        const char* end = body.data() + body.size();
        std::string tok;
        if (!next_token(p, tok) || tok != "{") {
            return shader;
        }
        while (p < end && next_token(p, tok)) {
            if (tok == "}") {
                break;
            }
            if (iequals(tok, "cullDistance")) {
                if (next_token(p, tok)) {
                    shader.set_cull_distance(std::strtof(tok.c_str(), nullptr));
                }
                continue;
            }
            if (tok != "{") {
                continue;
            }
            SpriteStage stage;
            while (p < end && next_token(p, tok)) {
                if (tok == "}") {
                    break;
                }
                if (iequals(tok, "texture")) {
                    if (next_token(p, tok)) {
                        stage.set_texture(tok);
                    }
                } else if (iequals(tok, "shader") || iequals(tok, "shaderFunc")) {
                    if (next_token(p, tok)) {
                        if (iequals(tok, "bumpenvmap") || iequals(tok, "heathaze")) {
                            stage.set_post_process(true);
                            float v[3] = {};
                            (void) parse_floats(p, 3, v);
                        }
                    }
                } else if (iequals(tok, "animTexture")) {
                    if (next_token(p, tok)) {
                        stage.set_anim_frames(std::max(1, std::atoi(tok.c_str())));
                    }
                    if (next_token(p, tok)) {
                        stage.set_anim_fps(std::max(1, std::atoi(tok.c_str())));
                    }
                    if (next_token(p, tok)) {
                        stage.set_texture(tok);
                    }
                } else if (iequals(tok, "size")) {
                    float v[2] = {1.0f, 1.0f};
                    if (parse_floats(p, 2, v)) {
                        stage.set_size0(v[0]);
                        stage.set_size1(v[1]);
                    }
                } else if (iequals(tok, "aspect")) {
                    float v[2] = {1.0f, 1.0f};
                    if (parse_floats(p, 2, v)) {
                        stage.set_aspect0(v[0]);
                        stage.set_aspect1(v[1]);
                    }
                } else if (iequals(tok, "color")) {
                    float v[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                    if (parse_floats(p, 4, v)) {
                        stage.set_r(v[0]);
                        stage.set_g(v[1]);
                        stage.set_b(v[2]);
                        stage.set_a(v[3]);
                    }
                } else if (iequals(tok, "fadeColor")) {
                    float v[4] = {1.0f, 1.0f, 1.0f, 0.0f};
                    if (parse_floats(p, 4, v)) {
                        stage.set_fade_r(v[0]);
                        stage.set_fade_g(v[1]);
                        stage.set_fade_b(v[2]);
                        stage.set_fade_a(v[3]);
                        stage.set_has_fade(true);
                    }
                } else if (iequals(tok, "fade")) {
                    if (next_token(p, tok)) {
                        stage.set_fade(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "count")) {
                    if (next_token(p, tok)) {
                        stage.set_count(std::max(1, std::atoi(tok.c_str())));
                    }
                } else if (iequals(tok, "time")) {
                    if (next_token(p, tok)) {
                        stage.set_time(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "deadTime")) {
                    if (next_token(p, tok)) {
                        stage.set_dead_time(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "sound")) {
                    if (next_token(p, tok)) {
                        stage.set_sound(tok);
                    }
                } else if (iequals(tok, "blendFunc") || iequals(tok, "blend")) {
                    if (next_token(p, tok)) {
                        stage.set_additive(iequals(tok, "add") || iequals(tok, "addblend"));
                        stage.set_addblend(iequals(tok, "addblend"));
                    }
                } else if (iequals(tok, "speed")) {
                    float v[2] = {};
                    if (parse_floats(p, 2, v)) {
                        stage.set_speed0(v[0]);
                        stage.set_speed1(v[1]);
                    }
                } else if (iequals(tok, "rotation")) {
                    float v[2] = {};
                    if (parse_floats(p, 2, v)) {
                        stage.set_rot0(v[0]);
                        stage.set_rot1(v[1]);
                    }
                } else if (iequals(tok, "initialAngle")) {
                    if (next_token(p, tok)) {
                        stage.set_initial_angle(std::strtof(tok.c_str(), nullptr));
                        stage.set_has_initial_angle(true);
                    }
                } else if (iequals(tok, "gravity")) {
                    if (next_token(p, tok)) {
                        stage.set_gravity(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "bunching")) {
                    if (next_token(p, tok)) {
                        stage.set_bunching(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "cycles")) {
                    if (next_token(p, tok)) {
                        stage.set_cycles(std::atoi(tok.c_str()));
                    }
                } else if (iequals(tok, "customPath")) {
                    if (next_token(p, tok)) {
                        if (iequals(tok, "helix")) {
                            float v[5] = {};
                            if (parse_floats(p, 5, v)) {
                                stage.set_path_mode(1);
                                stage.set_path0(v[0]);
                                stage.set_path1(v[1]);
                                stage.set_path2(v[2]);
                                stage.set_path3(v[3]);
                                stage.set_path4(v[4]);
                            }
                        } else if (iequals(tok, "spherical")) {
                            float v[3] = {};
                            if (parse_floats(p, 3, v)) {
                                stage.set_path_mode(2);
                                stage.set_path0(v[0]);
                                stage.set_path1(v[1]);
                                stage.set_path2(v[2]);
                            }
                        } else if (iequals(tok, "files")) {
                            float v[3] = {};
                            if (parse_floats(p, 3, v)) {
                                stage.set_path_mode(3);
                                stage.set_path0(v[0]);
                                stage.set_path1(v[1]);
                                stage.set_path2(v[2]);
                            }
                        } else if (iequals(tok, "cone")) {
                            float v[3] = {};
                            if (parse_floats(p, 3, v)) {
                                stage.set_path_mode(4);
                                stage.set_path0(v[0]);
                                stage.set_path1(v[1]);
                                stage.set_path2(v[2]);
                            }
                        } else {
                            float v[3] = {};
                            (void) parse_floats(p, 3, v);
                        }
                    }
                } else if (iequals(tok, "orientation")) {
                    if (next_token(p, tok)) {
                        using Orientation = SpriteFx::SpriteStage::Orientation;
                        if (iequals(tok, "viewy")) {
                            stage.set_orientation(Orientation::ViewY);
                        } else if (iequals(tok, "aimed")) {
                            stage.set_orientation(Orientation::Aimed);
                        } else if (iequals(tok, "aimedy")) {
                            stage.set_orientation(Orientation::AimedY);
                        } else if (iequals(tok, "x")) {
                            stage.set_orientation(Orientation::AxisX);
                        } else if (iequals(tok, "y")) {
                            stage.set_orientation(Orientation::AxisY);
                        } else if (iequals(tok, "z")) {
                            stage.set_orientation(Orientation::AxisZ);
                        } else {
                            stage.set_orientation(Orientation::View);
                        }
                    }
                } else if (iequals(tok, "trails")) {
                    if (next_token(p, tok)) {
                        stage.set_trails(std::max(0, std::atoi(tok.c_str())));
                    }
                } else if (iequals(tok, "trailTime")) {
                    if (next_token(p, tok)) {
                        stage.set_trail_time(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "direction")) {
                    if (next_token(p, tok)) {
                        if (iequals(tok, "cone")) {
                            stage.set_dir_mode(1);
                        } else if (iequals(tok, "outward")) {
                            stage.set_dir_mode(2);
                        }
                        float v[4] = {};
                        if (parse_floats(p, 4, v)) {
                            stage.set_dir0(v[0]);
                            stage.set_dir_x(v[1]);
                            stage.set_dir_y(v[2]);
                            stage.set_dir_z(v[3]);
                        }
                    }
                } else if (iequals(tok, "offset")) {
                    float v[3] = {};
                    if (parse_floats(p, 3, v)) {
                        stage.set_ox(v[0]);
                        stage.set_oy(v[1]);
                        stage.set_oz(v[2]);
                    }
                } else if (iequals(tok, "distribution")) {
                    if (next_token(p, tok) && iequals(tok, "box")) {
                        stage.set_dist_mode(SpriteDistribution::Box);
                        float v[3] = {};
                        if (parse_floats(p, 3, v)) {
                            stage.set_dist_x(v[0]);
                            stage.set_dist_y(v[1]);
                            stage.set_dist_z(v[2]);
                        }
                    } else if (iequals(tok, "sphere")) {
                        stage.set_dist_mode(SpriteDistribution::Sphere);
                        float v[4] = {};
                        if (parse_floats(p, 4, v)) {
                            stage.set_dist_x(v[0]);
                            stage.set_dist_y(v[1]);
                            stage.set_dist_z(v[2]);
                            stage.set_dist_w(v[3]);
                        }
                    } else if (iequals(tok, "cylinder")) {
                        stage.set_dist_mode(SpriteDistribution::Cylinder);
                        float v[4] = {};
                        if (parse_floats(p, 4, v)) {
                            stage.set_dist_x(v[0]);
                            stage.set_dist_y(v[1]);
                            stage.set_dist_z(v[2]);
                            stage.set_dist_w(v[3]);
                        }
                    }
                }
            }
            shader.stages().push_back(std::move(stage));
        }
        return shader;
    }

    SpriteFx::SpriteShader SpriteFx::parse_sprite(engine::IEngine& engine, std::string_view body) {
        SpriteShader shader = parse_sprite_fields(body);
        for (auto& stage : shader.stages()) {
            if (!stage.texture().empty()) {
                stage.set_tex(load_tex(engine, stage.texture()));
            }
        }
        return shader;
    }

    std::vector<SpriteFx::FxParticleRef> SpriteFx::parse_fx(std::string_view body) const {
        std::vector<FxParticleRef> out;
        const char* p = body.data();
        const char* end = body.data() + body.size();
        std::string tok;
        if (!next_token(p, tok) || tok != "{") {
            return out;
        }
        while (p < end && next_token(p, tok)) {
            if (tok == "}") {
                break;
            }
            if (iequals(tok, "sound")) {
                if (next_token(p, tok)) {
                    FxParticleRef ref;
                    ref.set_sound(tok);
                    out.push_back(std::move(ref));
                }
                continue;
            }
            if (tok != "{") {
                continue;
            }
            FxParticleRef ref;
            while (p < end && next_token(p, tok)) {
                if (tok == "}") {
                    break;
                }
                if (iequals(tok, "particle")) {
                    if (next_token(p, tok)) {
                        ref.set_particle(tok);
                    }
                } else if (iequals(tok, "model")) {
                    if (next_token(p, tok)) {
                        ref.set_model(fx_model_stem(tok));
                    }
                } else if (iequals(tok, "offset")) {
                    float v[3] = {};
                    if (parse_floats(p, 3, v)) {
                        ref.set_ox(v[0]);
                        ref.set_oy(v[1]);
                        ref.set_oz(v[2]);
                    }
                } else if (iequals(tok, "sound")) {
                    if (next_token(p, tok)) {
                        ref.set_sound(tok);
                    }
                } else if (iequals(tok, "duration")) {
                    if (next_token(p, tok)) {
                        ref.set_duration(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "effectBoneIndex")) {
                    if (next_token(p, tok)) {
                        ref.set_effect_bone(std::atoi(tok.c_str()));
                    }
                } else if (iequals(tok, "realeffectBoneIndex")) {
                    if (next_token(p, tok)) {
                        ref.set_real_effect_bone(std::atoi(tok.c_str()));
                    }
                } else if (iequals(tok, "useEffectBoneRotation")) {
                    ref.set_use_effect_bone_rotation(true);
                } else if (iequals(tok, "delay")) {
                    if (next_token(p, tok)) {
                        ref.set_delay(std::strtof(tok.c_str(), nullptr));
                    }
                } else if (iequals(tok, "restart")) {
                    if (next_token(p, tok)) {
                        ref.set_restart(std::atoi(tok.c_str()));
                    }
                }
            }
            if (!ref.particle().empty() || !ref.model().empty() || !ref.sound().empty()) {
                out.push_back(std::move(ref));
            }
        }
        return out;
    }

    const SpriteFx::SpriteShader* SpriteFx::find_sprite(engine::IEngine& engine,
                                                        std::string_view name) {
        const std::string key = ascii_lowered(name);
        auto cached = m_sprites.find(key);
        if (cached != m_sprites.end()) {
            return &cached->second;
        }
        auto script = find_script(m_sprite_scripts, name);
        if (!script) {
            return nullptr;
        }
        SpriteShader parsed = parse_sprite(engine, *script);
        if (parsed.stages().empty()) {
            log::warn("SpriteFx: empty particle shader " + key);
            return nullptr;
        }
        auto [it, _] = m_sprites.emplace(key, std::move(parsed));
        return &it->second;
    }

    void SpriteFx::attach_particle(engine::IEngine& engine,
                                   std::uint32_t entity,
                                   std::string_view name,
                                   int effect_bone,
                                   float ox,
                                   float oy,
                                   float oz) {
        if (!m_ready) {
            return;
        }
        const SpriteShader* shader = find_sprite(engine, name);
        if (!shader) {
            return;
        }
        m_emitters[entity].emplace_back(
            shader->stages(), shader->cull_distance(), effect_bone, ox, oy, oz);
    }

    void SpriteFx::attach_fx(engine::IEngine& engine,
                             std::uint32_t entity,
                             std::string_view name,
                             int effect_bone,
                             float delay_override) {
        if (!m_ready) {
            return;
        }
        auto script = find_script(m_fx_scripts, name);
        if (!script) {
            return;
        }
        for (const auto& ref : parse_fx(*script)) {
            if (ref.particle().empty()) {
                continue;
            }
            const int bone = ref.has_effect_bone() ? ref.effect_bone() : effect_bone;
            attach_particle(engine, entity, ref.particle(), bone, ref.ox(), ref.oy(), ref.oz());
            auto list = m_emitters.find(entity);
            if (list != m_emitters.end() && !list->second.empty()) {
                auto& em = list->second.back();
                em.set_fx_delay(delay_override >= 0.0f ? delay_override : ref.delay());
                em.set_fx_duration(ref.duration());
                em.set_fx_restart(ref.restart());
                if (ref.has_real_effect_bone()) {
                    em.set_real_effect_bone(ref.real_effect_bone());
                }
                em.set_use_effect_bone_rotation(ref.use_effect_bone_rotation());
            }
        }
    }

    void SpriteFx::loop_attached(std::uint32_t entity) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            em.set_fx_restart(1);
        }
    }

    void SpriteFx::persist_attached(std::uint32_t entity) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            em.set_fx_delay(0.0f);
            em.set_fx_duration(0.0f);
            em.set_fx_restart(0);
        }
    }

    bool SpriteFx::has(std::uint32_t entity) const {
        auto it = m_emitters.find(entity);
        return it != m_emitters.end() && !it->second.empty();
    }

    int SpriteFx::real_effect_bone(std::uint32_t entity, int effect_slot) const {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return -1;
        }
        for (const auto& em : it->second) {
            if (em.effect_bone() == effect_slot && em.has_real_effect_bone()) {
                return em.real_effect_bone();
            }
        }
        return -1;
    }

    void SpriteFx::follow_bone(std::uint32_t entity, int effect_bone, float x, float y, float z) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            if (em.effect_bone() == effect_bone) {
                em.set_world(x, y, z);
            }
        }
    }

    void SpriteFx::set_target_scale(std::uint32_t entity, float scale) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            em.set_target_scale(scale);
        }
    }

    void
    SpriteFx::follow_oriented(std::uint32_t entity, int effect_bone, const engine::Mat4& world) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            if (em.effect_bone() == effect_bone) {
                em.set_world_matrix(world);
            }
        }
    }

    void SpriteFx::follow_beam(std::uint32_t entity,
                               int effect_bone,
                               const engine::Vec3& source,
                               const engine::Vec3& target) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            if (em.effect_bone() == effect_bone) {
                em.set_beam(source, target);
            }
        }
    }

    void SpriteFx::detach(std::uint32_t entity) {
        m_emitters.erase(entity);
    }

    void SpriteFx::update(float dt, float eye_x, float eye_y, float eye_z) {
        m_cam_x = eye_x;
        m_cam_y = eye_y;
        m_cam_z = eye_z;
        for (auto& [id, list] : m_emitters) {
            (void) id;
            for (auto& em : list) {
                em.advance(dt);
            }
        }
        for (auto it = m_oneshots.begin(); it != m_oneshots.end();) {
            it->set_remaining(it->remaining() - dt);
            if (it->remaining() <= 0.0f) {
                detach(it->key());
                it = m_oneshots.erase(it);
            } else {
                ++it;
            }
        }
    }

    void SpriteFx::set_cull_distance_scale(float scale) {
        m_cull_distance_scale = std::clamp(scale, 0.25f, 8.0f);
    }

    void SpriteFx::follow_all(std::uint32_t entity, float x, float y, float z) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            em.set_world(x, y, z);
        }
    }

    void SpriteFx::follow_oneshots(std::uint32_t entity, float x, float y, float z) {
        follow_oneshots(entity, x, y, z, x, y, z);
    }

    void SpriteFx::follow_oneshots(std::uint32_t entity,
                                   float x,
                                   float y,
                                   float z,
                                   float origin_x,
                                   float origin_y,
                                   float origin_z) {
        if (entity == 0) {
            return;
        }
        for (const auto& burst : m_oneshots) {
            if (burst.follow_entity() != entity) {
                continue;
            }
            auto it = m_emitters.find(burst.key());
            if (it == m_emitters.end()) {
                continue;
            }
            for (auto& em : it->second) {
                if (em.effect_bone() >= 0) {
                    em.set_world(x, y, z);
                } else {
                    em.set_world(origin_x, origin_y, origin_z);
                }
            }
        }
    }

    engine::Mat4 SpriteFx::effect_bone_attach_matrix(const engine::Mat4& origin,
                                                     const engine::Mat4& bone,
                                                     bool use_effect_bone_rotation) {
        engine::Mat4 mat = use_effect_bone_rotation ? bone : origin;
        mat.m[12] = bone.m[12];
        mat.m[13] = bone.m[13];
        mat.m[14] = bone.m[14];
        return mat;
    }

    engine::Mat4 SpriteFx::offset_in_matrix_axes(engine::Mat4 mat, float ox, float oy, float oz) {
        mat.m[12] += mat.m[0] * ox + mat.m[4] * oy + mat.m[8] * oz;
        mat.m[13] += mat.m[1] * ox + mat.m[5] * oy + mat.m[9] * oz;
        mat.m[14] += mat.m[2] * ox + mat.m[6] * oy + mat.m[10] * oz;
        return mat;
    }

    bool SpriteFx::parsed_fx_uses_effect_bone_rotation(std::string_view body) const {
        for (const auto& ref : parse_fx(body)) {
            if (ref.use_effect_bone_rotation()) {
                return true;
            }
        }
        return false;
    }

    float SpriteFx::parsed_fx_oneshot_life(std::string_view body) const {
        float life = 0.0f;
        for (const auto& ref : parse_fx(body)) {
            const float play = ref.duration() > 0.0f ? ref.duration() : 3.0f;
            life = std::max(life, std::max(ref.delay(), 0.0f) + play);
        }
        if (life <= 0.0f) {
            return 1.5f;
        }
        return life;
    }

    int SpriteFx::parsed_sprite_anim_frames(std::string_view body) const {
        const SpriteShader shader = parse_sprite_fields(body);
        if (shader.stages().empty()) {
            return 1;
        }
        return shader.stages().front().anim_frames();
    }

    float SpriteFx::parsed_sprite_cull_distance(std::string_view body) const {
        return parse_sprite_fields(body).cull_distance();
    }

    SpriteFx::SpriteDistribution SpriteFx::parsed_sprite_distribution(std::string_view body) const {
        const SpriteShader shader = parse_sprite_fields(body);
        if (shader.stages().empty()) {
            return SpriteDistribution::Box;
        }
        return shader.stages().front().dist_mode();
    }

    bool SpriteFx::parsed_sprite_has_initial_angle(std::string_view body) const {
        const SpriteShader shader = parse_sprite_fields(body);
        if (shader.stages().empty()) {
            return false;
        }
        return shader.stages().front().has_initial_angle();
    }

    float SpriteFx::parsed_sprite_initial_angle(std::string_view body) const {
        const SpriteShader shader = parse_sprite_fields(body);
        if (shader.stages().empty()) {
            return 0.0f;
        }
        return shader.stages().front().initial_angle();
    }

    void SpriteFx::follow_oneshots(std::uint32_t entity,
                                   const engine::Mat4& origin,
                                   const engine::Mat4* effect_bones,
                                   int effect_bone_count) {
        if (entity == 0) {
            return;
        }
        for (const auto& burst : m_oneshots) {
            if (burst.follow_entity() != entity) {
                continue;
            }
            auto it = m_emitters.find(burst.key());
            if (it == m_emitters.end()) {
                continue;
            }
            for (auto& em : it->second) {
                engine::Mat4 mat = origin;
                const int bone = em.effect_bone();
                if (bone >= 0 && effect_bones != nullptr && bone < effect_bone_count) {
                    mat = effect_bone_attach_matrix(
                        origin, effect_bones[bone], em.use_effect_bone_rotation());
                }
                em.set_world_matrix(mat);
            }
        }
    }

    void SpriteFx::rewind(std::uint32_t entity) {
        auto it = m_emitters.find(entity);
        if (it == m_emitters.end()) {
            return;
        }
        for (auto& em : it->second) {
            em.rewind();
        }
    }

    std::vector<std::string> SpriteFx::fx_sounds(engine::IEngine& engine, std::string_view name) {
        std::vector<std::string> sounds;
        auto add_sound = [&sounds](const std::string& sound) {
            if (sound.empty()) {
                return;
            }
            for (const auto& existing : sounds) {
                if (iequals(existing, sound)) {
                    return;
                }
            }
            sounds.push_back(sound);
        };
        const std::string* body = find_script(m_fx_scripts, name);
        if (!body) {
            return sounds;
        }
        for (const auto& ref : parse_fx(*body)) {
            add_sound(ref.sound());
            if (ref.particle().empty()) {
                continue;
            }
            const SpriteShader* shader = find_sprite(engine, ref.particle());
            if (!shader) {
                continue;
            }
            for (const auto& stage : shader->stages()) {
                add_sound(stage.sound());
            }
        }
        return sounds;
    }

    std::vector<std::string> SpriteFx::spawn_oneshot_fx(engine::IEngine& engine,
                                                        std::string_view name,
                                                        float x,
                                                        float y,
                                                        float z,
                                                        std::uint32_t follow_entity) {
        std::vector<std::string> sounds;
        if (!m_ready || !has_fx(name)) {
            return sounds;
        }
        ++m_next_oneshot;
        if (m_next_oneshot < 0x40000000u) {
            m_next_oneshot = 0x40000001u;
        }
        const std::uint32_t key = m_next_oneshot;
        attach_fx(engine, key, name, -1);
        follow_all(key, x, y, z);
        m_last_oneshot_key = key;

        float life = 0.0f;
        sounds = fx_sounds(engine, name);
        const std::string* body = find_script(m_fx_scripts, name);
        if (body) {
            life = parsed_fx_oneshot_life(*body);
            for (const auto& ref : parse_fx(*body)) {
                if (ref.particle().empty()) {
                    continue;
                }
                const SpriteShader* shader = find_sprite(engine, ref.particle());
                if (!shader) {
                    continue;
                }
                const float delay = std::max(ref.delay(), 0.0f);
                for (const auto& stage : shader->stages()) {
                    life = std::max(life, delay + stage.time());
                }
            }
        }
        if (life <= 0.0f) {
            life = 1.5f;
        }
        Oneshot burst;
        burst.set_key(key);
        burst.set_remaining(life);
        burst.set_follow_entity(follow_entity);
        m_oneshots.push_back(burst);
        return sounds;
    }

    void SpriteFx::spawn_oneshot_particle(engine::IEngine& engine,
                                          std::string_view name,
                                          float x,
                                          float y,
                                          float z,
                                          std::uint32_t follow_entity,
                                          int effect_bone,
                                          float ox,
                                          float oy,
                                          float oz) {
        if (!m_ready || !has_sprite(name)) {
            return;
        }
        ++m_next_oneshot;
        if (m_next_oneshot < 0x40000000u) {
            m_next_oneshot = 0x40000001u;
        }
        const std::uint32_t key = m_next_oneshot;
        attach_particle(engine, key, name, effect_bone, ox, oy, oz);
        follow_all(key, x, y, z);
        m_last_oneshot_key = key;

        float life = 0.0f;
        const SpriteShader* shader = find_sprite(engine, name);
        if (shader) {
            for (const auto& stage : shader->stages()) {
                float stage_life = stage.time();
                if (stage.cycles() > 0) {
                    stage_life *= static_cast<float>(stage.cycles());
                }
                life = std::max(life, stage_life);
            }
        }
        if (life <= 0.0f) {
            life = 1.5f;
        }
        Oneshot burst;
        burst.set_key(key);
        burst.set_remaining(life);
        burst.set_follow_entity(follow_entity);
        m_oneshots.push_back(burst);
    }

    std::vector<std::string> SpriteFx::sprite_names() const {
        return sorted_names(m_sprite_scripts);
    }

    std::vector<std::string> SpriteFx::fx_names() const {
        return sorted_names(m_fx_scripts);
    }

    bool SpriteFx::has_sprite(std::string_view name) const {
        return find_script(m_sprite_scripts, name) != nullptr;
    }

    bool SpriteFx::has_fx(std::string_view name) const {
        return find_script(m_fx_scripts, name) != nullptr;
    }

    std::vector<std::string> SpriteFx::fx_particles(std::string_view name) const {
        std::vector<std::string> out;
        const std::string* body = find_script(m_fx_scripts, name);
        if (!body) {
            return out;
        }
        for (const auto& ref : parse_fx(*body)) {
            if (!ref.particle().empty()) {
                out.push_back(ref.particle());
            }
        }
        return out;
    }

    std::vector<SpriteFx::FxModelSpawn> SpriteFx::fx_model_spawns(std::string_view name) const {
        std::vector<FxModelSpawn> out;
        const std::string* body = find_script(m_fx_scripts, name);
        if (!body) {
            return out;
        }
        for (const auto& ref : parse_fx(*body)) {
            if (ref.model().empty()) {
                continue;
            }
            FxModelSpawn spawn;
            spawn.set_stem(ref.model());
            spawn.set_ox(ref.ox());
            spawn.set_oy(ref.oy());
            spawn.set_oz(ref.oz());
            spawn.set_delay(ref.delay());
            spawn.set_duration(ref.duration());
            spawn.set_restart(ref.restart());
            spawn.set_effect_bone(ref.has_effect_bone() ? ref.effect_bone() : -1);
            spawn.set_use_effect_bone_rotation(ref.use_effect_bone_rotation());
            out.push_back(std::move(spawn));
        }
        return out;
    }

    std::vector<std::string> SpriteFx::fx_models(std::string_view name) const {
        std::vector<std::string> out;
        const std::string* body = find_script(m_fx_scripts, name);
        if (!body) {
            return out;
        }
        for (const auto& ref : parse_fx(*body)) {
            if (!ref.model().empty()) {
                out.push_back(ref.model());
            }
        }
        return out;
    }

    float SpriteFx::sprite_radius(engine::IEngine& engine, std::string_view name) {
        const SpriteShader* shader = find_sprite(engine, name);
        if (!shader) {
            return 24.0f;
        }
        return shader_radius(*shader);
    }

    float SpriteFx::fx_radius(engine::IEngine& engine, std::string_view name) {
        float radius = 24.0f;
        for (const auto& particle : fx_particles(name)) {
            radius = std::max(radius, sprite_radius(engine, particle));
        }
        return radius;
    }

    const std::string*
    SpriteFx::find_script(const std::unordered_map<std::string, std::string>& scripts,
                          std::string_view name) const {
        const std::string key(name);
        auto it = scripts.find(key);
        if (it != scripts.end()) {
            return &it->second;
        }
        for (const auto& [script_name, body] : scripts) {
            if (iequals(script_name, name)) {
                return &body;
            }
        }
        return nullptr;
    }

    std::vector<std::string>
    SpriteFx::sorted_names(const std::unordered_map<std::string, std::string>& scripts) {
        std::vector<std::string> names;
        names.reserve(scripts.size());
        for (const auto& [name, body] : scripts) {
            (void) body;
            names.push_back(name);
        }
        std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
            return ascii_lowered(a) < ascii_lowered(b);
        });
        return names;
    }

    float SpriteFx::shader_radius(const SpriteShader& shader) {
        float radius = 8.0f;
        for (const auto& stage : shader.stages()) {
            const float size = std::max(stage.size0(), stage.size1()) *
                               std::max(1.0f, std::max(stage.aspect0(), stage.aspect1()));
            const float spread =
                std::max(std::abs(stage.ox()),
                         std::max(std::abs(stage.oy()), std::abs(stage.oz()))) +
                std::max(stage.dist_w(),
                         std::max(stage.dist_x(), std::max(stage.dist_y(), stage.dist_z())));
            float travel = std::max(std::abs(stage.speed0()), std::abs(stage.speed1())) +
                           std::abs(stage.gravity());
            if (stage.path_mode() != 0) {
                travel = std::max({std::abs(stage.path1()),
                                   std::abs(stage.path2()),
                                   std::abs(stage.path3()),
                                   std::abs(stage.path4())});
            }
            radius = std::max(radius, 0.5f * size + spread + travel);
        }
        return radius;
    }

    void SpriteFx::append_billboard(std::vector<engine::SpriteBatchVertex>& vertices,
                                    float x,
                                    float y,
                                    float z,
                                    float size,
                                    float aspect,
                                    float r,
                                    float g,
                                    float b,
                                    float a,
                                    float angle_rad,
                                    SpriteStage::Orientation orientation,
                                    float u0,
                                    float u1) const {
        const float width = size * aspect;
        if (a <= 0.01f) {
            return;
        }
        if (size <= 0.01f && width <= 0.01f) {
            return;
        }
        float fx = m_cam_x - x;
        float fy = m_cam_y - y;
        float fz = m_cam_z - z;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (fl < 1e-4f) {
            return;
        }
        fx /= fl;
        fy /= fl;
        fz /= fl;
        float rx = 0.0f;
        float ry = 0.0f;
        float rz = 0.0f;
        float ux = 0.0f;
        float uy = 0.0f;
        float uz = 0.0f;
        switch (orientation) {
        case SpriteStage::Orientation::AxisX:
            rx = 0.0f;
            ry = 0.0f;
            rz = 1.0f;
            ux = 0.0f;
            uy = 1.0f;
            uz = 0.0f;
            break;
        case SpriteStage::Orientation::AxisY:
            rx = 1.0f;
            ry = 0.0f;
            rz = 0.0f;
            ux = 0.0f;
            uy = 0.0f;
            uz = 1.0f;
            break;
        case SpriteStage::Orientation::AxisZ:
            rx = -1.0f;
            ry = 0.0f;
            rz = 0.0f;
            ux = 0.0f;
            uy = 1.0f;
            uz = 0.0f;
            break;
        case SpriteStage::Orientation::ViewY:
            rx = fz;
            ry = 0.0f;
            rz = -fx;
            {
                const float rl = std::sqrt(rx * rx + rz * rz);
                if (rl < 1e-4f) {
                    rx = 1.0f;
                    rz = 0.0f;
                } else {
                    rx /= rl;
                    rz /= rl;
                }
            }
            ux = 0.0f;
            uy = 1.0f;
            uz = 0.0f;
            break;
        case SpriteStage::Orientation::View:
        case SpriteStage::Orientation::Aimed:
        case SpriteStage::Orientation::AimedY:
        default:
            rx = fz;
            ry = 0.0f;
            rz = -fx;
            {
                const float rl = std::sqrt(rx * rx + rz * rz);
                if (rl < 1e-4f) {
                    rx = 1.0f;
                    rz = 0.0f;
                } else {
                    rx /= rl;
                    rz /= rl;
                }
            }
            ux = fy * rz - fz * ry;
            uy = fz * rx - fx * rz;
            uz = fx * ry - fy * rx;
            break;
        }
        fx = ry * uz - rz * uy;
        fy = rz * ux - rx * uz;
        fz = rx * uy - ry * ux;
        const float ll = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (ll > 1e-4f) {
            fx /= ll;
            fy /= ll;
            fz /= ll;
        }
        if (std::abs(angle_rad) > 1e-5f) {
            const float c = std::cos(-angle_rad);
            const float s = std::sin(-angle_rad);
            const float nrx = rx * c + ux * s;
            const float nry = ry * c + uy * s;
            const float nrz = rz * c + uz * s;
            const float nux = ux * c - rx * s;
            const float nuy = uy * c - ry * s;
            const float nuz = uz * c - rz * s;
            rx = nrx;
            ry = nry;
            rz = nrz;
            ux = nux;
            uy = nuy;
            uz = nuz;
        }
        engine::Mat4 M = engine::Mat4::identity();
        M.m[0] = rx * width;
        M.m[1] = ry * width;
        M.m[2] = rz * width;
        M.m[4] = ux * size;
        M.m[5] = uy * size;
        M.m[6] = uz * size;
        M.m[8] = fx;
        M.m[9] = fy;
        M.m[10] = fz;
        M.m[12] = x;
        M.m[13] = y;
        M.m[14] = z;
        const std::array<engine::Vec3, 4> positions = {
            engine::Mat4::transform_point(M, -0.5f, -0.5f, 0.0f),
            engine::Mat4::transform_point(M, 0.5f, -0.5f, 0.0f),
            engine::Mat4::transform_point(M, 0.5f, 0.5f, 0.0f),
            engine::Mat4::transform_point(M, -0.5f, 0.5f, 0.0f),
        };
        const std::array<float, 4> us = {u0, u1, u1, u0};
        const std::array<float, 4> vs = {0.0f, 0.0f, 1.0f, 1.0f};
        constexpr std::array<std::size_t, 6> kTriangleIndices = {0, 1, 2, 0, 2, 3};
        for (const std::size_t index : kTriangleIndices) {
            const auto& position = positions[index];
            vertices.emplace_back(
                position.x, position.y, position.z, us[index], vs[index], r, g, b, a);
        }
        ++m_last_draw_billboards;
    }

    void SpriteFx::append_aimed_segment(std::vector<engine::SpriteBatchVertex>& vertices,
                                        float x0,
                                        float y0,
                                        float z0,
                                        float x1,
                                        float y1,
                                        float z1,
                                        float size,
                                        float r,
                                        float g,
                                        float b,
                                        float a,
                                        float v0,
                                        float v1) const {
        if (a <= 0.01f || size <= 0.01f) {
            return;
        }
        const float tx = x1 - x0;
        const float ty = y1 - y0;
        const float tz = z1 - z0;
        const float tl = std::sqrt(tx * tx + ty * ty + tz * tz);
        if (tl < 1e-4f) {
            return;
        }
        const float mx = (x0 + x1) * 0.5f;
        const float my = (y0 + y1) * 0.5f;
        const float mz = (z0 + z1) * 0.5f;
        float cx = m_cam_x - mx;
        float cy = m_cam_y - my;
        float cz = m_cam_z - mz;
        normalize3(cx, cy, cz);
        float rx = ty * cz - tz * cy;
        float ry = tz * cx - tx * cz;
        float rz = tx * cy - ty * cx;
        const float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (rl < 1e-4f) {
            return;
        }
        const float half = size * 0.5f / rl;
        rx *= half;
        ry *= half;
        rz *= half;
        const std::array<engine::Vec3, 4> positions = {
            engine::Vec3{x0 - rx, y0 - ry, z0 - rz},
            engine::Vec3{x0 + rx, y0 + ry, z0 + rz},
            engine::Vec3{x1 + rx, y1 + ry, z1 + rz},
            engine::Vec3{x1 - rx, y1 - ry, z1 - rz},
        };
        const std::array<float, 4> us = {0.0f, 1.0f, 1.0f, 0.0f};
        const std::array<float, 4> vs = {v0, v0, v1, v1};
        constexpr std::array<std::size_t, 6> kTriangleIndices = {0, 1, 2, 0, 2, 3};
        for (const std::size_t index : kTriangleIndices) {
            const auto& position = positions[index];
            vertices.emplace_back(
                position.x, position.y, position.z, us[index], vs[index], r, g, b, a);
        }
        ++m_last_draw_ribbons;
    }

    void SpriteFx::stage_local_pos(const SpriteStage& stage,
                                   float em_ox,
                                   float em_oy,
                                   float em_oz,
                                   std::uint32_t seed,
                                   float frac,
                                   float& lx,
                                   float& ly,
                                   float& lz) {
        const int path = stage.path_mode();
        if (path != 0) {
            float ix = 0.0f;
            float iy = 0.0f;
            float iz = 0.0f;
            float dir_y = 0.0f;
            if (path == 1) {
                ix = hash_signed(seed + 3u) * stage.path2();
                iy = hash_signed(seed + 4u) * stage.path3();
                const float as = stage.path2() * stage.path2();
                const float bs = stage.path4() * stage.path4();
                iz = stage.path4();
                if (as > 0.0f) {
                    const float inner = 1.0f - (ix * ix) / as;
                    iz = std::sqrt(std::max(0.0f, inner) * bs);
                }
                iz *= hash_signed(seed + 5u) > 0.0f ? 1.0f : -1.0f;
                dir_y = hash_signed(seed + 6u) * stage.path1();
            } else if (path == 4) {
                float ux = hash_signed(seed + 3u);
                float uy = 0.0f;
                float uz = hash_signed(seed + 5u);
                normalize3(ux, uy, uz);
                const float r = stage.path1() + (stage.path2() - stage.path1()) * hash01(seed + 6u);
                ix = ux * r;
                iy = 0.0f;
                iz = uz * r;
            } else {
                float ux = hash_signed(seed + 3u);
                float uy = hash_signed(seed + 4u);
                float uz = hash_signed(seed + 5u);
                normalize3(ux, uy, uz);
                const float r = path == 3 ? stage.path2() : hash01(seed + 6u) * stage.path2();
                ix = ux * r;
                iy = uy * r;
                iz = uz * r;
            }
            float px = 0.0f;
            float py = 0.0f;
            float pz = 0.0f;
            if (path == 1) {
                const float theta = frac * stage.path0();
                const float c = std::cos(theta);
                const float s = std::sin(theta);
                px = ix * c + iz * s;
                py = iy + frac * dir_y;
                pz = iz * c - ix * s;
            } else if (path == 4) {
                const float theta = frac * stage.path0();
                const float shrink = 1.0f - frac;
                const float c = std::cos(theta) * shrink;
                const float s = std::sin(theta) * shrink;
                px = ix * c + iz * s;
                py = 0.0f;
                pz = iz * c - ix * s;
            } else {
                const float radial = frac * stage.path0();
                const float axial = frac * stage.path1();
                float vx = ix;
                float vy = iy;
                float vz = iz;
                float tx = ix;
                float ty = iy;
                float tz = iz;
                normalize3(tx, ty, tz);
                float ax = tz;
                float ay = 0.0f;
                float az = -tx;
                if (ax * ax + ay * ay + az * az > 1e-8f) {
                    rotate_axis(ax, ay, az, axial, vx, vy, vz);
                }
                const float c = std::cos(radial);
                const float s = std::sin(radial);
                px = vx * c + vz * s;
                py = vy;
                pz = vz * c - vx * s;
            }
            lx = em_ox + stage.ox() + px;
            ly = em_oy + stage.oy() + py - stage.gravity() * frac;
            lz = em_oz + stage.oz() + pz;
            return;
        }
        const float dist =
            stage.speed0() * frac + (stage.speed1() - stage.speed0()) * 0.5f * frac * frac;
        float sx = 0.0f;
        float sy = 0.0f;
        float sz = 0.0f;
        if (stage.dist_mode() == SpriteFx::SpriteDistribution::Cylinder) {
            const float vx = stage.dist_x();
            const float vy = stage.dist_y();
            const float vz = stage.dist_z();
            const float aa = vx * vx;
            const float bb = vz * vz;
            const float ab = vx * vz;
            const float angle = hash01(seed + 3u) * (kPi * 2.0f);
            const float s = std::sin(angle);
            const float c = std::cos(angle);
            const float d = aa * s * s + bb * c * c;
            float r = d > 0.0f ? ab / std::sqrt(d) : ab;
            float thick = stage.dist_w();
            if (thick > 1.0f) {
                thick = 1.0f;
            }
            if (thick > 0.0f) {
                const float inner = r * (1.0f - thick);
                r = inner + (r - inner) * hash01(seed + 7u);
            }
            sx = r * c;
            sy = hash_signed(seed + 4u) * vy;
            sz = r * s;
        } else if (stage.dist_mode() == SpriteFx::SpriteDistribution::Sphere) {
            sx = hash_signed(seed + 3u);
            sy = hash_signed(seed + 4u);
            sz = hash_signed(seed + 5u);
            normalize3(sx, sy, sz);
            sx = stage.dist_x() + sx * stage.dist_w();
            sy = stage.dist_y() + sy * stage.dist_w();
            sz = stage.dist_z() + sz * stage.dist_w();
        } else {
            sx = hash_signed(seed + 3u) * stage.dist_x();
            sy = hash_signed(seed + 4u) * stage.dist_y();
            sz = hash_signed(seed + 5u) * stage.dist_z();
        }
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        if (stage.dir_mode() == 1) {
            cone_direction(
                stage.dir_x(), stage.dir_y(), stage.dir_z(), stage.dir0(), seed, dx, dy, dz);
        } else if (stage.dir_mode() == 2) {
            dx = sx + stage.dir_x() * stage.dir0();
            dy = sy + stage.dir_y() * stage.dir0();
            dz = sz + stage.dir_z() * stage.dir0();
            normalize3(dx, dy, dz);
        }
        lx = em_ox + stage.ox() + sx + dx * dist;
        ly = em_oy + stage.oy() + sy + dy * dist - stage.gravity() * frac;
        lz = em_oz + stage.oz() + sz + dz * dist;
    }

    void SpriteFx::draw(engine::IEngine& engine) const {
        m_last_draw_emitters = 0;
        m_last_culled_emitters = 0;
        m_last_draw_billboards = 0;
        m_last_draw_ribbons = 0;
        m_last_draw_batches = 0;
        if (!m_ready) {
            return;
        }
        for (auto& batch : m_draw_batches) {
            batch.clear();
        }
        const auto select_batch =
            [&](engine::TextureHandle texture, bool alpha_blend, bool additive) -> SpriteBatch& {
            const auto it =
                std::find_if(m_draw_batches.begin(), m_draw_batches.end(), [&](const auto& batch) {
                    return batch.matches(texture, alpha_blend, additive);
                });
            if (it != m_draw_batches.end()) {
                return *it;
            }
            return m_draw_batches.emplace_back(texture, alpha_blend, additive);
        };
        for (const auto& [id, list] : m_emitters) {
            (void) id;
            for (const auto& em : list) {
                ++m_last_draw_emitters;
                const float dx = m_cam_x - em.x();
                const float dy = m_cam_y - em.y();
                const float dz = m_cam_z - em.z();
                const float cull_distance = em.cull_distance() * m_cull_distance_scale;
                const float distance_sq = dx * dx + dy * dy + dz * dz;
                if (cull_distance < 0.0f || distance_sq > cull_distance * cull_distance) {
                    ++m_last_culled_emitters;
                    continue;
                }
                for (const auto& stage : em.stages()) {
                    if (!stage.tex() || stage.post_process()) {
                        continue;
                    }
                    if (em.has_beam_target() &&
                        (std::abs(stage.speed0()) > 0.01f || std::abs(stage.speed1()) > 0.01f)) {
                        const engine::Vec3 target = em.beam_target();
                        auto& batch = select_batch(
                            stage.tex(), stage.addblend() || !stage.additive(), stage.additive());
                        append_aimed_segment(batch.vertices(),
                                             em.x(),
                                             em.y(),
                                             em.z(),
                                             target.x,
                                             target.y,
                                             target.z,
                                             stage.size0() * em.target_scale(),
                                             stage.r(),
                                             stage.g(),
                                             stage.b(),
                                             stage.a(),
                                             0.0f,
                                             1.0f);
                        const float life = stage.time() > 0.05f ? stage.time() : 1.5f;
                        const int particle_count =
                            std::min(std::max(stage.count(), 1), kMaxParticles);
                        const float bunching = stage.bunching();
                        const float cycle_time = life + std::max(stage.dead_time(), 0.0f);
                        const float cycle_age = std::fmod(em.age(), cycle_time);
                        for (int i = 0; i < particle_count; ++i) {
                            const float generation = static_cast<float>(i) * life * bunching /
                                                     static_cast<float>(particle_count);
                            float elapsed = cycle_age - generation;
                            if (elapsed < 0.0f) {
                                elapsed += cycle_time;
                            }
                            if (elapsed <= 0.0f || elapsed >= life) {
                                continue;
                            }
                            const float fraction = elapsed / life;
                            const float size =
                                (stage.size0() + (stage.size1() - stage.size0()) * fraction) *
                                em.target_scale();
                            float r = stage.r();
                            float g = stage.g();
                            float b = stage.b();
                            float a = stage.a();
                            if (stage.has_fade()) {
                                const float fade = std::clamp(stage.fade(), 0.01f, 0.99f);
                                if (fraction < fade) {
                                    const float blend = fraction / fade;
                                    r = stage.fade_r() * (1.0f - blend) + stage.r() * blend;
                                    g = stage.fade_g() * (1.0f - blend) + stage.g() * blend;
                                    b = stage.fade_b() * (1.0f - blend) + stage.b() * blend;
                                    a = stage.fade_a() * (1.0f - blend) + stage.a() * blend;
                                } else {
                                    const float blend = (fraction - fade) / (1.0f - fade);
                                    r = stage.r() * (1.0f - blend) + stage.fade_r() * blend;
                                    g = stage.g() * (1.0f - blend) + stage.fade_g() * blend;
                                    b = stage.b() * (1.0f - blend) + stage.fade_b() * blend;
                                    a = stage.a() * (1.0f - blend) + stage.fade_a() * blend;
                                }
                            }
                            const engine::Vec3 head{em.x() + (target.x - em.x()) * fraction,
                                                    em.y() + (target.y - em.y()) * fraction,
                                                    em.z() + (target.z - em.z()) * fraction};
                            if (stage.aimed()) {
                                const float tail_fraction = std::max(
                                    0.0f, fraction - std::clamp(stage.trail_time(), 0.05f, 0.5f));
                                const engine::Vec3 tail{
                                    em.x() + (target.x - em.x()) * tail_fraction,
                                    em.y() + (target.y - em.y()) * tail_fraction,
                                    em.z() + (target.z - em.z()) * tail_fraction};
                                append_aimed_segment(batch.vertices(),
                                                     tail.x,
                                                     tail.y,
                                                     tail.z,
                                                     head.x,
                                                     head.y,
                                                     head.z,
                                                     size,
                                                     r,
                                                     g,
                                                     b,
                                                     a,
                                                     tail_fraction,
                                                     fraction);
                            } else {
                                const float aspect = stage.aspect0() +
                                                     (stage.aspect1() - stage.aspect0()) * fraction;
                                append_billboard(batch.vertices(),
                                                 head.x,
                                                 head.y,
                                                 head.z,
                                                 size,
                                                 aspect,
                                                 r,
                                                 g,
                                                 b,
                                                 a,
                                                 0.0f,
                                                 stage.orientation(),
                                                 0.0f,
                                                 1.0f);
                            }
                        }
                        continue;
                    }
                    const float life = stage.time() > 0.05f ? stage.time() : 1.5f;
                    const float cycle = life + std::max(stage.dead_time(), 0.0f);
                    const int n = std::min(std::max(stage.count(), 1), kMaxParticles);
                    const float bunch = stage.bunching();
                    const int cycles = stage.cycles();
                    float time = em.age();
                    // Compatibility behavior removes a stage after delay+duration
                    // even when the particle shader uses cycles 0. Skipping this gate
                    // for restart-0 / delay-0 oneshots left HaGang-style Calls looping
                    // for the FX editor default of 3 s.
                    if (em.fx_delay() > 0.0f || em.fx_duration() > 0.0f || em.fx_restart() != 0) {
                        const float play = em.fx_duration() > 0.0f ? em.fx_duration() : 3.0f;
                        const float delay = std::max(em.fx_delay(), 0.0f);
                        const float period = delay + play;
                        if (period <= 0.0f) {
                            continue;
                        }
                        if (em.fx_restart() == 0) {
                            if (time < delay || time > delay + play) {
                                continue;
                            }
                            time -= delay;
                        } else {
                            const float phase = std::fmod(time, period);
                            if (phase < delay) {
                                continue;
                            }
                            time = phase - delay;
                        }
                    }
                    const int cur_cycles = static_cast<int>(time / cycle);
                    if (cycles > 0 && cur_cycles > cycles) {
                        continue;
                    }
                    const float cur_frac = time - static_cast<float>(cur_cycles) * cycle;
                    float anim_u0 = 0.0f;
                    float anim_u1 = 1.0f;
                    const int anim_frames = stage.anim_frames();
                    if (anim_frames > 1) {
                        const int fps = std::max(stage.anim_fps(), 1);
                        const int frame =
                            static_cast<int>(time * static_cast<float>(fps)) % anim_frames;
                        const float nf = static_cast<float>(anim_frames);
                        anim_u0 = static_cast<float>(frame) / nf;
                        anim_u1 = anim_u0 + 1.0f / nf;
                    }
                    for (int i = 0; i < n; ++i) {
                        const float gen =
                            static_cast<float>(i) * life * bunch / static_cast<float>(n);
                        float elapsed = cur_frac - gen;
                        const bool wrapped = cur_cycles > 0 && elapsed < 0.0f;
                        if (wrapped) {
                            elapsed += cycle;
                        }
                        if (elapsed <= 0.0f || elapsed >= life) {
                            continue;
                        }
                        if (cycles > 0 && cur_cycles >= cycles && !wrapped) {
                            continue;
                        }
                        const float frac = elapsed / life;
                        const std::uint32_t seed = static_cast<std::uint32_t>(i) * 7477u + 19u;
                        const float size =
                            (stage.size0() + (stage.size1() - stage.size0()) * frac) *
                            em.target_scale();
                        const float aspect =
                            stage.aspect0() + (stage.aspect1() - stage.aspect0()) * frac;
                        float r = stage.r();
                        float g = stage.g();
                        float b = stage.b();
                        float a = stage.a();
                        if (stage.has_fade()) {
                            const float kFade = std::clamp(stage.fade(), 0.01f, 0.99f);
                            if (frac < kFade) {
                                const float f = frac / kFade;
                                r = stage.fade_r() * (1.0f - f) + stage.r() * f;
                                g = stage.fade_g() * (1.0f - f) + stage.g() * f;
                                b = stage.fade_b() * (1.0f - f) + stage.b() * f;
                                a = stage.fade_a() * (1.0f - f) + stage.a() * f;
                            } else {
                                const float f = (frac - kFade) / (1.0f - kFade);
                                r = stage.r() * (1.0f - f) + stage.fade_r() * f;
                                g = stage.g() * (1.0f - f) + stage.fade_g() * f;
                                b = stage.b() * (1.0f - f) + stage.fade_b() * f;
                                a = stage.a() * (1.0f - f) + stage.fade_a() * f;
                            }
                        }
                        auto world_at = [&](float elapsed_at) {
                            const float f = elapsed_at / life;
                            float lx = 0.0f;
                            float ly = 0.0f;
                            float lz = 0.0f;
                            stage_local_pos(stage, em.ox(), em.oy(), em.oz(), seed, f, lx, ly, lz);
                            const float target_scale = em.target_scale();
                            return engine::Mat4::transform_point(em.world(),
                                                                 lx * target_scale,
                                                                 ly * target_scale,
                                                                 lz * target_scale);
                        };
                        if (stage.aimed()) {
                            int numtrails = stage.trails();
                            numtrails += 1;
                            const int npts = std::min(numtrails + 1, kMaxTrailPoints);
                            engine::Vec3 pts[kMaxTrailPoints];
                            for (int j = 0; j < npts; ++j) {
                                float elapsed2 = elapsed;
                                if (j > 0 && numtrails > 0) {
                                    elapsed2 -= (life * stage.trail_time()) *
                                                static_cast<float>(j) /
                                                static_cast<float>(numtrails);
                                }
                                pts[j] = world_at(elapsed2);
                            }
                            for (int k = 0; k < npts - 1; ++k) {
                                const float v0 =
                                    static_cast<float>(k) / static_cast<float>(numtrails);
                                const float v1 =
                                    static_cast<float>(k + 1) / static_cast<float>(numtrails);
                                auto& batch = select_batch(stage.tex(),
                                                           stage.addblend() || !stage.additive(),
                                                           stage.additive());
                                append_aimed_segment(batch.vertices(),
                                                     pts[k].x,
                                                     pts[k].y,
                                                     pts[k].z,
                                                     pts[k + 1].x,
                                                     pts[k + 1].y,
                                                     pts[k + 1].z,
                                                     size,
                                                     r,
                                                     g,
                                                     b,
                                                     a,
                                                     v0,
                                                     v1);
                            }
                            continue;
                        }
                        const float rot = stage.rot0() + (stage.rot1() - stage.rot0()) * frac;
                        const float initial_deg = stage.has_initial_angle()
                                                      ? stage.initial_angle()
                                                      : hash_signed(seed + 9u) * 180.0f;
                        const float sign = initial_deg < 0.0f ? -1.0f : 1.0f;
                        const float angle_rad = (initial_deg + elapsed * rot * sign) * kDegToRad;
                        const int extras = std::max(stage.trails(), 0);
                        const int samples = extras + 1;
                        for (int j = 0; j < samples; ++j) {
                            float elapsed2 = elapsed;
                            if (j > 0 && extras > 0) {
                                elapsed2 -= (life * stage.trail_time()) * static_cast<float>(j) /
                                            static_cast<float>(extras);
                            }
                            if (elapsed2 <= 0.0f || elapsed2 >= life) {
                                continue;
                            }
                            const engine::Vec3 wp = world_at(elapsed2);
                            const float trail_scale =
                                static_cast<float>(samples - j) / static_cast<float>(samples);
                            auto& batch = select_batch(stage.tex(),
                                                       stage.addblend() || !stage.additive(),
                                                       stage.additive());
                            append_billboard(batch.vertices(),
                                             wp.x,
                                             wp.y,
                                             wp.z,
                                             size * trail_scale,
                                             aspect,
                                             r,
                                             g,
                                             b,
                                             a,
                                             angle_rad,
                                             stage.orientation(),
                                             anim_u0,
                                             anim_u1);
                        }
                    }
                }
            }
        }
        for (const auto& batch : m_draw_batches) {
            if (batch.vertices().empty()) {
                continue;
            }
            engine.draw_sprite_batch(
                batch.vertices(), batch.texture(), batch.alpha_blend(), batch.additive());
            ++m_last_draw_batches;
        }
    }

}  // namespace openkal::client
