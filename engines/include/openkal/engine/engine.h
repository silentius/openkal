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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::engine {

    struct Vec3 {
        float x = 0;
        float y = 0;
        float z = 0;
        Vec3() = default;
        Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    };

    struct Mat4 {
        std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        static Mat4 identity() {
            return {};
        }
        static Mat4 perspective(float fovy_rad, float aspect, float znear, float zfar) {
            Mat4 r{};
            const float f = 1.0f / std::tan(fovy_rad * 0.5f);
            r.m = {f / aspect,
                   0,
                   0,
                   0,
                   0,
                   f,
                   0,
                   0,
                   0,
                   0,
                   (zfar + znear) / (znear - zfar),
                   -1,
                   0,
                   0,
                   (2 * zfar * znear) / (znear - zfar),
                   0};
            return r;
        }
        static Mat4 look_at(Vec3 eye, Vec3 center, Vec3 up) {
            auto sub = [](Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; };
            auto cross = [](Vec3 a, Vec3 b) {
                return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
            };
            auto norm = [](Vec3 v) {
                const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                return len > 0 ? Vec3{v.x / len, v.y / len, v.z / len} : Vec3{};
            };
            auto dot = [](Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
            const Vec3 f = norm(sub(center, eye));
            const Vec3 s = norm(cross(f, up));
            const Vec3 u = cross(s, f);
            // Column-major, matches GLM lookAtRH (column 0 = (s.x, u.x, -f.x, 0)).
            Mat4 r{};
            r.m = {s.x,
                   u.x,
                   -f.x,
                   0,
                   s.y,
                   u.y,
                   -f.y,
                   0,
                   s.z,
                   u.z,
                   -f.z,
                   0,
                   -dot(s, eye),
                   -dot(u, eye),
                   dot(f, eye),
                   1};
            return r;
        }

        static Mat4 multiply(const Mat4& a, const Mat4& b) {
            Mat4 r{};
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    r.m[col * 4 + row] =
                        a.m[0 * 4 + row] * b.m[col * 4 + 0] + a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                        a.m[2 * 4 + row] * b.m[col * 4 + 2] + a.m[3 * 4 + row] * b.m[col * 4 + 3];
                }
            }
            return r;
        }

        // Invert a rigid-ish 4x4 (column-major). Returns false if singular.
        static bool invert(const Mat4& in, Mat4& out) {
            const float* m = in.m.data();
            float inv[16];
            inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
                     m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
            inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
                     m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
            inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
                     m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
            inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
                      m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
            inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
                     m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
            inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
                     m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
            inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
                     m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
            inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
                      m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
            inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
                     m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
            inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
                     m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
            inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
                      m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
            inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
                      m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
            inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
                     m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
            inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
                     m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
            inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
                      m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
            inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
                      m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
            const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
            if (std::fabs(det) < 1e-12f) {
                return false;
            }
            const float idet = 1.0f / det;
            for (int i = 0; i < 16; ++i) {
                out.m[static_cast<std::size_t>(i)] = inv[i] * idet;
            }
            return true;
        }

        static Mat4
        orthographic(float left, float right, float bottom, float top, float znear, float zfar) {
            Mat4 r{};
            r.m = {2.0f / (right - left),
                   0,
                   0,
                   0,
                   0,
                   2.0f / (top - bottom),
                   0,
                   0,
                   0,
                   0,
                   -2.0f / (zfar - znear),
                   0,
                   -(right + left) / (right - left),
                   -(top + bottom) / (top - bottom),
                   -(zfar + znear) / (zfar - znear),
                   1};
            return r;
        }

        static Mat4 translation(float x, float y, float z) {
            Mat4 r = identity();
            r.m[12] = x;
            r.m[13] = y;
            r.m[14] = z;
            return r;
        }

        static Mat4 scaling(float x, float y, float z) {
            Mat4 r{};
            r.m = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1};
            return r;
        }

        static Vec3 transform_point(const Mat4& m, float x, float y, float z) {
            return {m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12],
                    m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13],
                    m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14]};
        }

        // Column-major rotation for OpenGL (column vectors).
        static Mat4 from_quaternion(float qx, float qy, float qz, float qw) {
            const float xx = qx * qx;
            const float yy = qy * qy;
            const float zz = qz * qz;
            const float xy = qx * qy;
            const float xz = qx * qz;
            const float yz = qy * qz;
            const float wx = qw * qx;
            const float wy = qw * qy;
            const float wz = qw * qz;
            Mat4 r = identity();
            r.m[0] = 1.0f - 2.0f * (yy + zz);
            r.m[1] = 2.0f * (xy + wz);
            r.m[2] = 2.0f * (xz - wy);
            r.m[4] = 2.0f * (xy - wz);
            r.m[5] = 1.0f - 2.0f * (xx + zz);
            r.m[6] = 2.0f * (yz + wx);
            r.m[8] = 2.0f * (xz + wy);
            r.m[9] = 2.0f * (yz - wx);
            r.m[10] = 1.0f - 2.0f * (xx + yy);
            return r;
        }
    };

    struct MeshHandle {
        std::uint32_t id = 0;
        explicit operator bool() const {
            return id != 0;
        }
    };

    struct TextureHandle {
        std::uint32_t id = 0;
        explicit operator bool() const {
            return id != 0;
        }
    };

    /** Height-map receiver used by projected world shadows. */
    struct ShadowReceiver {
        /** Single-channel floating-point terrain-height texture. */
        TextureHandle height_map{};

        /** World X coordinate represented by texture U=0. */
        float origin_x = 0.0f;

        /** World Z coordinate represented by texture V=0. */
        float origin_z = 0.0f;

        /** World-space width and depth represented by the texture. */
        float world_size = 0.0f;

        /** @return True when the receiver can be sampled. */
        [[nodiscard]] explicit operator bool() const {
            return static_cast<bool>(height_map) && world_size > 0.0f;
        }
    };

    struct VertexP3T2 {
        float px, py, pz;
        float u, v;  // patch-local 0..1 (color splat UV)
    };

    /** One world-space vertex in a streamed sprite triangle batch. */
    class SpriteBatchVertex {
      public:
        /**
         * Creates a textured, colored sprite vertex.
         * @param px World X.
         * @param py World Y.
         * @param pz World Z.
         * @param u Texture U.
         * @param v Texture V.
         * @param r Red multiplier.
         * @param g Green multiplier.
         * @param b Blue multiplier.
         * @param a Alpha multiplier.
         */
        SpriteBatchVertex(
            float px, float py, float pz, float u, float v, float r, float g, float b, float a)
            : m_values{px, py, pz, u, v, r, g, b, a} {}

        /** @return Contiguous position, UV, and color values for GPU upload. */
        [[nodiscard]] const std::array<float, 9>& values() const {
            return m_values;
        }

      private:
        /** Position XYZ, texture UV, and color RGBA. */
        std::array<float, 9> m_values{};
    };

    /** Indoor GB lightmap vertex: diffuse UV then lightmap UV (VT_RIGID_DOUBLE). */
    struct VertexP3T2T2 {
        float px, py, pz;
        float u, v;
        float lu, lv;
    };

    // Matches client terrain stages: color (clamp) * sunlight, then MODULATE2X with
    // detail (wrap).
    struct TerrainLayerDraw {
        TextureHandle color;   // RGBA splat (RGB from KCM, A = layer alpha)
        TextureHandle detail;  // tiled GTX detail
        float repeat = 1.0f;   // LayerInfo::runtime state
        bool alpha_blend = false;
        float sun_r = 1.0f;
        float sun_g = 1.0f;
        float sun_b = 1.0f;
    };

    struct FrameInput {
        bool quit = false;
        bool key_w = false;
        bool key_a = false;
        bool key_s = false;
        bool key_d = false;
        bool key_space = false;
        bool key_ctrl = false;
        bool key_shift = false;
        bool key_escape = false;
        bool key_enter = false;
        bool key_backspace = false;
        bool key_delete = false;
        bool key_i = false;
        bool key_e = false;
        bool key_k = false;
        bool key_p = false; /**< Powerbook hotkey. */
        bool key_o = false; /**< Selected-player options hotkey. */
        bool key_g = false; /**< Guild window hotkey. */
        bool key_q = false;
        bool key_f = false;
        bool key_z = false;
        bool key_x = false;
        bool key_h = false;
        bool key_c = false;
        bool key_v = false;
        bool key_l = false;
        bool key_m = false;
        bool key_r = false;
        bool key_left = false;
        bool key_right = false;
        bool key_up = false;
        bool key_down = false;
        // Digit keys 1..9 (index 1..9; index 0 unused). Top-row and keypad.
        bool key_digit[10] = {};
        // Edge-triggered (pressed this frame).
        bool key_escape_pressed = false;
        bool key_enter_pressed = false;
        /** Pressed edge for account-form field navigation. */
        bool key_tab_pressed = false;
        /** Text-edit action: initial press plus OS-delayed repeats while held. */
        bool key_backspace_pressed = false;
        bool key_delete_pressed = false;
        bool key_i_pressed = false;
        bool key_e_pressed = false;
        bool key_k_pressed = false;
        bool key_p_pressed = false; /**< Rising edge of the Powerbook hotkey. */
        bool key_o_pressed = false; /**< Rising edge of the player options hotkey. */
        bool key_g_pressed = false; /**< Rising edge of the guild hotkey. */
        bool key_q_pressed = false;
        bool key_f_pressed = false;
        bool key_s_pressed = false;  // Movement (reserved; skill window is K)
        bool key_z_pressed = false;
        bool key_x_pressed = false;
        bool key_h_pressed = false;
        bool key_c_pressed = false;
        bool key_v_pressed = false;
        bool key_l_pressed = false;
        bool key_m_pressed = false;
        bool key_r_pressed = false;
        bool key_space_pressed = false;
        bool key_digit_pressed[10] = {};
        bool mouse_left = false;
        bool mouse_right = false;
        bool mouse_middle = false;
        bool mouse_left_clicked = false;  // rising edge
        bool mouse_right_clicked = false;
        bool mouse_middle_clicked = false;
        float mouse_dx = 0;
        float mouse_dy = 0;
        float mouse_scroll = 0;  // wheel ticks this frame (+ = up/away)
        float mouse_x = 0;       // framebuffer pixels, origin top-left
        float mouse_y = 0;
        // UTF-8-ish raw codepoints typed this frame (ASCII for now).
        char text_chars[32] = {};
        int text_char_count = 0;
        // OS file drops this frame (absolute paths). Cleared after poll consumes
        // them.
        std::vector<std::string> dropped_files;
        int width = 0;
        int height = 0;
        float dt = 0;
    };

    enum class CursorMode {
        Captured = 0,  // FPS look
        Free = 1,      // OS/software cursor for UI / click-to-move
    };

    // Per-frame draw counters + pacing (see GlEngine). Values are from the last
    // completed frame.
    struct FrameDrawStats {
        int mesh = 0;
        int terrain = 0;
        int ui = 0;
        int other = 0;  // water / sky / lit
        float fps = 0.0f;
        float busy_ms = 0.0f;
        float present_ms = 0.0f;

        [[nodiscard]] int total() const {
            return mesh + terrain + ui + other;
        }
    };

    /** Optional rendering capabilities exposed by an engine backend. */
    struct RenderCapabilities {
        bool bloom = false;
    };

    /** OS window presentation mode. */
    enum class WindowMode {
        Windowed,
        Maximized,
        Borderless,
    };

    class IEngine {
      public:
        virtual ~IEngine() = default;
        virtual bool init(const char* title, int width, int height) = 0;
        virtual void shutdown() = 0;
        virtual bool poll(FrameInput& out) = 0;
        /** Reads UTF-8 text from the system clipboard.
         * @return Clipboard text, or empty when unavailable.
         */
        [[nodiscard]] virtual std::string clipboard_text() const {
            return {};
        }

        /** Writes UTF-8 text to the system clipboard.
         * @param text Text to copy.
         * @return True when a clipboard backend is available.
         */
        [[nodiscard]] virtual bool set_clipboard_text(const std::string& text) {
            (void) text;
            return false;
        }

        /** Maximizes the OS window. No-op on the null backend. */
        virtual void maximize_window() = 0;
        /** Applies a window mode while retaining the preferred windowed size.
         * @param mode Window presentation mode.
         * @param width Preferred windowed width.
         * @param height Preferred windowed height.
         * @return True when the mode was applied.
         */
        virtual bool set_window_mode(WindowMode mode, int width, int height) = 0;
        virtual void set_cursor_mode(CursorMode mode) = 0;
        /** @return Optional rendering features supported by this backend. */
        [[nodiscard]] virtual RenderCapabilities render_capabilities() const = 0;
        /** Sets the live bloom contribution; zero disables postprocessing.
         * @param strength Bloom contribution in the range 0..1.
         */
        virtual void set_bloom_strength(float strength) = 0;
        virtual void begin_frame(float clear_r, float clear_g, float clear_b) = 0;
        /** Completes world postprocessing and restores the default framebuffer for UI. */
        virtual void begin_ui_pass() = 0;

        /** Clips subsequent UI pixels against scene depth at a world label's distance.
         * Call after begin_ui_pass. Does not write depth or affect world rendering.
         * @param anchor World-space label anchor, or nullptr to restore ordinary overlay UI.
         */
        virtual void set_ui_world_anchor(const Vec3* anchor) {
            (void) anchor;
        }
        [[nodiscard]] virtual FrameDrawStats frame_stats() const = 0;
        virtual void set_camera(const Mat4& view, const Mat4& proj) = 0;

        /** glPolygonOffset; (0,0) disables. Indoor MAP_OBJECT uses the same
         * negative bias as characters so feet win against coplanar region floors.
         * @param factor Depth slope scale.
         * @param units Depth constant units.
         */
        virtual void set_polygon_offset(float factor, float units) = 0;

        /**
         * Uses GL_LEQUAL while true so a later draw wins equal quantized depths.
         * Restore with false (GL_LESS). Held shields need this at long far-planes.
         * @param enable True for less-equal, false for less.
         */
        virtual void set_depth_less_equal(bool enable) = 0;

        // wrap=true => GL_REPEAT (detail tiles); false => clamp (color splat).
        // nearest: point sampling, no mipmaps (UI bitmap fonts / pixel icons).
        virtual TextureHandle create_texture_rgba8(const std::uint8_t* rgba,
                                                   int width,
                                                   int height,
                                                   bool wrap = false,
                                                   bool nearest = false) = 0;
        /** Uploads a linearly sampled single-channel floating-point texture.
         * @param values Row-major floating-point values.
         * @param width Texture width.
         * @param height Texture height.
         * @return Texture handle.
         */
        virtual TextureHandle create_texture_r32f(const float* values, int width, int height) = 0;
        virtual void destroy_texture(TextureHandle tex) = 0;
        virtual MeshHandle create_mesh(const VertexP3T2* verts,
                                       std::size_t vert_count,
                                       const std::uint32_t* indices,
                                       std::size_t index_count) = 0;
        /** Uploads a dual-UV mesh used by indoor baked lightmaps.
         * @param verts Position, diffuse UV, and lightmap UV.
         * @param vert_count Number of vertices.
         * @param indices Triangle indices.
         * @param index_count Number of indices.
         * @return GPU mesh handle.
         */
        virtual MeshHandle create_mesh_lm(const VertexP3T2T2* verts,
                                          std::size_t vert_count,
                                          const std::uint32_t* indices,
                                          std::size_t index_count) = 0;
        // Replace vertex payload (same count as create_mesh). Used for compatibility
        // behavior-skinned animation.
        virtual bool
        update_mesh(MeshHandle mesh, const VertexP3T2* verts, std::size_t vert_count) = 0;

        /** Updates optional per-vertex normals and a white Phong highlight.
         * Backends without lighting may ignore this metadata. Existing unconfigured
         * meshes retain their texture-only appearance. Packed asset colors are not
         * interpreted as highlight colors.
         * @param mesh Uploaded textured mesh.
         * @param normals Object-space normals, one per uploaded vertex.
         * @param power Highlight exponent; nonpositive or nonfinite values disable it.
         */
        virtual void set_mesh_specular(MeshHandle mesh,
                                       const std::vector<std::array<float, 3>>& normals,
                                       float power) {
            (void) mesh;
            (void) normals;
            (void) power;
        }

        virtual void destroy_mesh(MeshHandle mesh) = 0;
        // Legacy single-texture draw (world-space verts).
        virtual void draw_mesh(MeshHandle mesh, TextureHandle texture) = 0;
        // Textured mesh with model matrix (GB props).
        // alpha_test: discard texels with alpha < ~0.5 (MAP_OPACITY cutout); keeps
        // Z-write. alpha_blend: SRC_ALPHA blend with Z-write off (MAP_ALPHA_RGB /
        // water). additive: ONE/ONE blend (lightning polylines); implies Z-write off.
        // additive + alpha_blend together: SRC_ALPHA/ONE (particle addblend).
        // uv_u/uv_v: MATERIALEX_TEXTURE scroll (KalOnline D3DTS_TEXTURE0._31/_32).
        // mul_*: vertex/texture color modulate (TFACTOR / material opacity).
        // white_mix: blend RGB toward unlit white (0..1), preserving texture alpha.
        virtual void draw_mesh_textured(MeshHandle mesh,
                                        TextureHandle texture,
                                        const Mat4& model,
                                        bool alpha_blend = false,
                                        bool two_sided = false,
                                        bool alpha_test = false,
                                        bool additive = false,
                                        float uv_u = 0.0f,
                                        float uv_v = 0.0f,
                                        float mul_r = 1.0f,
                                        float mul_g = 1.0f,
                                        float mul_b = 1.0f,
                                        float mul_a = 1.0f,
                                        float white_mix = 0.0f) = 0;

        /** Begins a depth-tested screen-space outline for one multipart model.
         * @param model World transform shared by all submitted parts.
         * @param bounds_min Lower corner of the model-space bounds.
         * @param bounds_max Upper corner of the model-space bounds.
         * @param radius Distance from the silhouette to the line in framebuffer pixels.
         * @param opacity Line opacity in the range 0..1.
         * @return True when the backend is ready to accept parts and composite the outline.
         */
        [[nodiscard]] virtual bool
        begin_mesh_outline(const Mat4&, const Vec3&, const Vec3&, float, float) {
            return false;
        }

        /** Adds a textured mesh to the active outline silhouette.
         * @param mesh Model part.
         * @param texture Diffuse texture used when testing alpha.
         * @param two_sided True to include both sides of the mesh.
         * @param alpha_cutoff Minimum texture alpha included in the silhouette.
         */
        virtual void draw_mesh_outline_part(MeshHandle, TextureHandle, bool, float) {}

        /** Composites the outline and restores scene rendering. */
        virtual void end_mesh_outline() {}

        /** Begins a shared shadow mask. Overlapping silhouettes use maximum opacity. */
        virtual void begin_shadow_pass() {}

        /** Composites the shared shadow mask onto the scene once. */
        virtual void end_shadow_pass() {}

        /** Projects a textured mesh silhouette into the active shadow pass.
         * Call between begin_shadow_pass() and end_shadow_pass().
         * @param mesh Caster mesh.
         * @param texture Diffuse texture used for cutout alpha.
         * @param model Caster world transform.
         * @param receiver Optional terrain height-map receiver.
         * @param ground_y Receiver-plane height.
         * @param alpha_test Whether transparent texels are excluded.
         * @param opacity Shadow opacity in the range 0..1.
         */
        virtual void draw_mesh_shadow(MeshHandle mesh,
                                      TextureHandle texture,
                                      const Mat4& model,
                                      const ShadowReceiver& receiver,
                                      float ground_y,
                                      bool alpha_test,
                                      float opacity) = 0;

        /**
         * Draws repeated opaque static meshes with one hardware-instanced draw.
         * @param mesh Shared mesh.
         * @param texture Shared diffuse texture.
         * @param models Instance transforms.
         * @param two_sided True to disable back-face culling.
         * @param alpha_test True to discard diffuse alpha below 128/255.
         */
        virtual void draw_mesh_textured_instances(MeshHandle mesh,
                                                  TextureHandle texture,
                                                  const std::vector<Mat4>& models,
                                                  bool two_sided,
                                                  bool alpha_test) = 0;

        /**
         * Streams and draws colored world-space sprite triangles in one call.
         * @param vertices Triangle-list vertices sharing one texture and blend state.
         * @param texture Sprite texture.
         * @param alpha_blend True for source-alpha blending.
         * @param additive True for additive destination blending.
         */
        virtual void draw_sprite_batch(const std::vector<SpriteBatchVertex>& vertices,
                                       TextureHandle texture,
                                       bool alpha_blend,
                                       bool additive) = 0;

        /** Draws an indoor mesh as (lightmap + 0.2) * diffuse, matching compatibility behavior.
         * @param mesh Dual-UV mesh from create_mesh_lm.
         * @param texture Diffuse texture (TEXCOORD0).
         * @param lightmap Baked lightmap (TEXCOORD1); white when missing.
         * @param model Instance matrix.
         * @param alpha_blend SRC_ALPHA blend with Z-write off.
         * @param two_sided Disable back-face cull.
         * @param alpha_test Discard texels below ALPHAREF 128.
         * @param uv_u Diffuse UV scroll U.
         * @param uv_v Diffuse UV scroll V.
         * @param mul_r Color modulate R.
         * @param mul_g Color modulate G.
         * @param mul_b Color modulate B.
         * @param mul_a Color modulate A.
         */
        virtual void draw_mesh_lightmapped(MeshHandle mesh,
                                           TextureHandle texture,
                                           TextureHandle lightmap,
                                           const Mat4& model,
                                           bool alpha_blend = false,
                                           bool two_sided = false,
                                           bool alpha_test = false,
                                           float uv_u = 0.0f,
                                           float uv_v = 0.0f,
                                           float mul_r = 1.0f,
                                           float mul_g = 1.0f,
                                           float mul_b = 1.0f,
                                           float mul_a = 1.0f) = 0;
        // Global water plane: scrolling water_001 + sky-tint fresnel (compatibility behavior +
        // cheap reflect).
        virtual void draw_water_plane(MeshHandle mesh,
                                      TextureHandle texture,
                                      const Mat4& model,
                                      const Vec3& cam_eye,
                                      float time_sec) = 0;
        // Camera-locked sky face (depth test/write off). Call after begin_frame with
        // translation-cleared view.
        virtual void draw_sky_face(MeshHandle mesh, TextureHandle texture, const Mat4& model) = 0;
        // Client-accurate terrain layer pass (color clamp + detail wrap +
        // MODULATE2X).
        virtual void draw_terrain_layer(MeshHandle mesh, const TerrainLayerDraw& layer) = 0;
        // Solid-color mesh with model matrix (debug / fallback).
        virtual void
        draw_mesh_lit(MeshHandle mesh, const Mat4& model, float r, float g, float b) = 0;
        /** Clips subsequent UI quads, rectangles and triangles to a circle.
         * @param cx Circle center X in top-left framebuffer pixels.
         * @param cy Circle center Y in top-left framebuffer pixels.
         * @param radius Radius in pixels; zero disables clipping. Headless engines may ignore it.
         */
        virtual void set_ui_clip_circle(float cx, float cy, float radius) {
            (void) cx;
            (void) cy;
            (void) radius;
        }

        // Screen-space UI quad. (x,y) top-left in framebuffer pixels; w/h in pixels.
        // Optional u0/v0/u1/v1 select a normalized sub-rect of the texture (full =
        // 0,0,1,1).
        virtual void draw_ui_quad(TextureHandle texture,
                                  float x,
                                  float y,
                                  float w,
                                  float h,
                                  float r = 1,
                                  float g = 1,
                                  float b = 1,
                                  float a = 1,
                                  float u0 = 0.0f,
                                  float v0 = 0.0f,
                                  float u1 = 1.0f,
                                  float v1 = 1.0f) = 0;

        /**
         * Draws a UI texture rotated around the centre of its destination rectangle.
         * Headless renderers may draw the unrotated rectangle.
         * @param texture UI texture handle.
         * @param x Destination left.
         * @param y Destination top.
         * @param w Destination width.
         * @param h Destination height.
         * @param rotation_rad Clockwise rotation in screen coordinates, in radians.
         * @param r Red tint.
         * @param g Green tint.
         * @param b Blue tint.
         * @param a Alpha multiplier.
         */
        virtual void draw_ui_quad_rotated(TextureHandle texture,
                                          float x,
                                          float y,
                                          float w,
                                          float h,
                                          float rotation_rad,
                                          float r = 1,
                                          float g = 1,
                                          float b = 1,
                                          float a = 1) {
            (void) rotation_rad;
            draw_ui_quad(texture, x, y, w, h, r, g, b, a);
        }
        virtual void
        draw_ui_rect(float x, float y, float w, float h, float r, float g, float b, float a) = 0;
        // Screen-space filled triangle. Vertices are top-left origin framebuffer pixels.
        virtual void draw_ui_triangle(float x0,
                                      float y0,
                                      float x1,
                                      float y1,
                                      float x2,
                                      float y2,
                                      float r,
                                      float g,
                                      float b,
                                      float a) = 0;
        // Offscreen render target for one-shot captures (e.g. top-down bake).
        // begin_offscreen clears and binds an FBO; subsequent draws go there instead
        // of the window. end_offscreen_rgba restores the default FB and fills
        // rgba_out (width*height*4, top-left origin).
        virtual bool
        begin_offscreen(int width, int height, float clear_r, float clear_g, float clear_b) = 0;
        virtual bool end_offscreen_rgba(std::vector<std::uint8_t>& rgba_out) = 0;
        virtual void end_frame() = 0;
    };

    IEngine* create_engine();

}  // namespace openkal::engine
