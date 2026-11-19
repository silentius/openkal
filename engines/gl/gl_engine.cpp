/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/engine/engine.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>

namespace openkal::engine {
    namespace {

        const char* kVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMVP;
out vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

        // uMVP is proj * view_rotation. Clip in camera-relative world space so
        // large map coordinates (dungeon GBs ~5e4) do not swim in float32.
        const char* kMeshVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 6) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uCamPos;
uniform float uSpecularPower;
uniform vec2 uUvOffset;
out vec2 vUv;
out vec3 vWorldPos;
out vec3 vNormal;
void main() {
  vUv = aUv + uUvOffset;
  vec4 wp = uModel * vec4(aPos, 1.0);
  vWorldPos = wp.xyz;
  mat3 linear = mat3(uModel);
  vNormal = uSpecularPower > 0.0 && abs(determinant(linear)) > 1e-12
    ? transpose(inverse(linear)) * aNormal : vec3(0.0);
  gl_Position = uMVP * vec4(wp.xyz - uCamPos, 1.0);
}
)";

        const char* kMeshInstanceVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in mat4 aModel;
layout(location = 6) in vec3 aNormal;
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uSpecularPower;
out vec2 vUv;
out vec3 vWorldPos;
out vec3 vNormal;
void main() {
  vUv = aUv;
  vec4 wp = aModel * vec4(aPos, 1.0);
  vWorldPos = wp.xyz;
  mat3 linear = mat3(aModel);
  vNormal = uSpecularPower > 0.0 && abs(determinant(linear)) > 1e-12
    ? transpose(inverse(linear)) * aNormal : vec3(0.0);
  gl_Position = uMVP * vec4(wp.xyz - uCamPos, 1.0);
}
)";

        const char* kFragSrc = R"(#version 330 core
in vec2 vUv;
in vec3 vWorldPos;
in vec3 vNormal;
uniform vec3 uCamPos;
uniform float uSpecularPower;
uniform sampler2D uTex;
uniform float uAlphaRef;  // >0 => discard if texel.a < ref (D3D ALPHAREF 128)
uniform vec4 uColorMul;
uniform float uWhiteMix;
out vec4 FragColor;
void main() {
  vec4 c = texture(uTex, vUv) * uColorMul;
  if (uAlphaRef > 0.0 && c.a < uAlphaRef) discard;
  if (uSpecularPower > 0.0 && dot(vNormal, vNormal) > 1e-12) {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 light = normalize(vec3(-0.35, 0.8, -0.5));
    vec3 toEye = uCamPos - vWorldPos;
    if (dot(n, light) > 0.0 && dot(toEye, toEye) > 1e-12) {
      float highlight = pow(max(dot(reflect(-light, n), normalize(toEye)), 0.0),
                            uSpecularPower);
      c.rgb += vec3(highlight) * uColorMul.rgb;
    }
  }
  c.rgb = mix(c.rgb, vec3(1.0), uWhiteMix);
  FragColor = c;
}
)";

        const char* kSpriteBatchVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uMVP;
uniform vec3 uCamPos;
out vec2 vUv;
out vec4 vColor;
void main() {
  vUv = aUv;
  vColor = aColor;
  gl_Position = uMVP * vec4(aPos - uCamPos, 1.0);
}
)";

        const char* kSpriteBatchFragSrc = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
  FragColor = texture(uTex, vUv) * vColor;
}
)";

        const char* kLmVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec2 aLmUv;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uCamPos;
uniform vec2 uUvOffset;
out vec2 vUv;
out vec2 vLmUv;
void main() {
  vUv = aUv + uUvOffset;
  vLmUv = aLmUv;
  vec4 wp = uModel * vec4(aPos, 1.0);
  gl_Position = uMVP * vec4(wp.xyz - uCamPos, 1.0);
}
)";

        // compatibility behavior: (lightmap + TFACTOR 51/255) * diffuse.
        const char* kLmFragSrc = R"(#version 330 core
in vec2 vUv;
in vec2 vLmUv;
uniform sampler2D uTex;
uniform sampler2D uLightmap;
uniform float uAlphaRef;
uniform vec4 uColorMul;
out vec4 FragColor;
void main() {
  vec4 diff = texture(uTex, vUv);
  vec4 lm = texture(uLightmap, vLmUv);
  vec3 lit = (lm.rgb + vec3(51.0 / 255.0)) * diff.rgb;
  vec4 c = vec4(lit, diff.a * lm.a) * uColorMul;
  if (uAlphaRef > 0.0 && c.a < uAlphaRef) discard;
  FragColor = c;
}
)";

        const char* kWaterFragSrc = R"(#version 330 core
in vec2 vUv;
in vec3 vWorldPos;
uniform sampler2D uTex;
uniform vec3 uCamPos;
uniform vec3 uSkyColor;
uniform vec3 uDeepColor;
uniform float uTime;
out vec4 FragColor;
void main() {
  // Dual scroll approximates bump/distortion without a noise RT.
  vec2 uv0 = vUv + vec2(uTime * 0.015, uTime * 0.010);
  vec2 uv1 = vUv * 1.7 + vec2(-uTime * 0.012, uTime * 0.018);
  vec4 t0 = texture(uTex, uv0);
  vec4 t1 = texture(uTex, uv1);
  vec3 water = mix(t0.rgb, t1.rgb, 0.35);
  vec3 N = vec3(0.0, 1.0, 0.0);
  vec3 V = normalize(uCamPos - vWorldPos);
  float ndotv = max(dot(N, V), 0.0);
  float fresnel = pow(1.0 - ndotv, 2.5);
  vec3 col = mix(water * uDeepColor, uSkyColor, fresnel * 0.72);
  // compatibility behavior uses ~40% constant alpha (TFACTOR).
  FragColor = vec4(col, 0.40);
}
)";

        // Terrain keeps a separate vertex shader that ignores model (world-space mesh).
        const char* kTerrainVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
out vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = uViewProj * vec4(aPos - uCamPos, 1.0);
}
)";

        // Mirrors Terrain::Render fixed-function stages:
        //   color * sunlight, then MODULATE2X with tiled detail; alpha from color
        //   splat.
        const char* kTerrainFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uColor;
uniform sampler2D uDetail;
uniform float uRepeat;
uniform vec3 uSun;
out vec4 FragColor;
void main() {
  // Half-texel bias like Patch::Init (runtime state += 0.5/MAP_SIZE).
  const float halfTexel = 0.5 / 256.0;
  vec2 colorUv = vUv + vec2(halfTexel);
  vec4 color = texture(uColor, colorUv);
  vec2 detailUv = colorUv * uRepeat;
  vec4 detail = texture(uDetail, detailUv);
  vec3 lit = color.rgb * uSun;
  FragColor = vec4(clamp(detail.rgb * lit * 2.0, 0.0, 1.0), color.a);
}
)";

        const char* kLitVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uCamPos;
void main() {
  vec4 wp = uModel * vec4(aPos, 1.0);
  gl_Position = uMVP * vec4(wp.xyz - uCamPos, 1.0);
}
)";

        const char* kLitFragSrc = R"(#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
  FragColor = vec4(uColor, 1.0);
}
)";

        const char* kUiVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMVP;
uniform vec4 uUvRect; // xy = uv min, zw = uv size
out vec2 vUv;
void main() {
  vUv = uUvRect.xy + aUv * uUvRect.zw;
  gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

        const char* kUiFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform vec3 uClipCircle;
uniform sampler2D uSceneDepth;
uniform float uWorldDepth;
out vec4 FragColor;
void main() {
  if (uWorldDepth >= 0.0 &&
      uWorldDepth > texelFetch(uSceneDepth, ivec2(gl_FragCoord.xy), 0).r + 0.0000001) {
    discard;
  }
  vec4 t = texture(uTex, vUv);
  FragColor = t * uColor;
  if (uClipCircle.z > 0.0) {
    float edge = length(gl_FragCoord.xy - uClipCircle.xy) - uClipCircle.z;
    FragColor.a *= 1.0 - smoothstep(-0.5, 0.5, edge);
  }
}
)";

        const char* kPostVertSrc = R"(#version 330 core
out vec2 vUv;
void main() {
  vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  vUv = pos;
  gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

        const char* kOutlineMaskFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform float uAlphaCutoff;
out float MaskDepth;
void main() {
  if (uAlphaCutoff > 0.0 && texture(uTex, vUv).a < uAlphaCutoff) discard;
  MaskDepth = gl_FragCoord.z;
}
)";

        const char* kOutlineFragSrc = R"(#version 330 core
uniform sampler2D uMask;
uniform ivec4 uBounds;
uniform float uRadius;
uniform float uOpacity;
out vec4 FragColor;
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  if (texelFetch(uMask, p, 0).r < 1.0) discard;

  // Find the nearest point on the visible silhouette in a circular pixel neighborhood.
  float reach = uRadius + 1.4;
  float nearest = reach * reach;
  float depth = 1.0;
  int extent = int(ceil(reach));
  for (int y = max(-extent, uBounds.y - p.y); y <= min(extent, uBounds.w - 1 - p.y); ++y) {
    for (int x = max(-extent, uBounds.x - p.x); x <= min(extent, uBounds.z - 1 - p.x); ++x) {
      float distanceSquared = float(x * x + y * y);
      if (distanceSquared > nearest) continue;
      float sampleDepth = texelFetch(uMask, p + ivec2(x, y), 0).r;
      if (sampleDepth >= 1.0) continue;
      depth = distanceSquared < nearest ? sampleDepth : min(depth, sampleDepth);
      nearest = distanceSquared;
    }
  }
  if (depth >= 1.0) discard;
  float coverage = 1.0 - smoothstep(0.4, 1.4, abs(sqrt(nearest) - uRadius));
  gl_FragDepth = depth;
  FragColor = vec4(1.0, 1.0, 1.0, uOpacity * coverage);
}
)";

        const char* kBrightPassFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uScene;
out vec4 FragColor;
void main() {
  vec3 color = texture(uScene, vUv).rgb;
  float peak = max(color.r, max(color.g, color.b));
  float contribution = smoothstep(0.52, 0.92, peak);
  FragColor = vec4(color * contribution, 1.0);
}
)";

        const char* kBlurFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uImage;
uniform vec2 uDirection;
out vec4 FragColor;
void main() {
  vec2 texel = 1.0 / vec2(textureSize(uImage, 0));
  vec2 stepUv = uDirection * texel;
  vec3 color = texture(uImage, vUv).rgb * 0.227027;
  color += texture(uImage, vUv + stepUv * 1.384615).rgb * 0.316216;
  color += texture(uImage, vUv - stepUv * 1.384615).rgb * 0.316216;
  color += texture(uImage, vUv + stepUv * 3.230769).rgb * 0.070270;
  color += texture(uImage, vUv - stepUv * 3.230769).rgb * 0.070270;
  FragColor = vec4(color, 1.0);
}
)";

        const char* kBloomCompositeFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uStrength;
out vec4 FragColor;
void main() {
  vec3 scene = texture(uScene, vUv).rgb;
  vec3 glow = texture(uBloom, vUv).rgb * (uStrength * 1.8);
  vec3 composed = vec3(1.0) - (vec3(1.0) - scene) * (vec3(1.0) - clamp(glow, 0.0, 1.0));
  FragColor = vec4(clamp(composed, 0.0, 1.0), 1.0);
}
)";

        const char* kShadowVertSrc = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUv;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uCamPos;
uniform float uGroundY;
uniform sampler2D uHeightMap;
uniform vec2 uReceiverOrigin;
uniform float uReceiverSize;
uniform int uUseHeightMap;
out vec2 vUv;
out vec2 vReceiverUv;
float sampleReceiverHeight(vec2 uv) {
  ivec2 size = textureSize(uHeightMap, 0);
  vec2 grid = clamp(uv, 0.0, 1.0) * vec2(size - ivec2(1));
  ivec2 cell = clamp(ivec2(floor(grid)), ivec2(0), size - ivec2(2));
  vec2 local = grid - vec2(cell);
  float h00 = texelFetch(uHeightMap, cell, 0).r;
  float h10 = texelFetch(uHeightMap, cell + ivec2(1, 0), 0).r;
  float h01 = texelFetch(uHeightMap, cell + ivec2(0, 1), 0).r;
  float h11 = texelFetch(uHeightMap, cell + ivec2(1, 1), 0).r;
  if (local.x + local.y <= 1.0) {
    return h00 + local.x * (h10 - h00) + local.y * (h01 - h00);
  }
  return h11 + (1.0 - local.x) * (h01 - h11) +
         (1.0 - local.y) * (h10 - h11);
}
void main() {
  vec3 caster = (uModel * vec4(aPos, 1.0)).xyz;
  const vec3 lightRay = vec3(0.57735027, -0.57735027, 0.57735027);
  vec3 world = caster;
  float receiverY = uGroundY;
  for (int i = 0; i < 3; ++i) {
    float height = max(caster.y - receiverY, 0.0);
    world = caster + lightRay * (height / -lightRay.y);
    vReceiverUv = (world.xz - uReceiverOrigin) / uReceiverSize;
    if (uUseHeightMap != 0) {
      receiverY = sampleReceiverHeight(vReceiverUv);
    }
  }
  world.y = receiverY + 0.8;
  vUv = aUv;
  gl_Position = uViewProj * vec4(world - uCamPos, 1.0);
}
)";

        const char* kShadowFragSrc = R"(#version 330 core
in vec2 vUv;
in vec2 vReceiverUv;
uniform sampler2D uTex;
uniform float uAlphaRef;
uniform float uOpacity;
uniform int uUseHeightMap;
out vec4 FragColor;
void main() {
  if (uUseHeightMap != 0 &&
      (vReceiverUv.x < 0.0 || vReceiverUv.x > 1.0 ||
       vReceiverUv.y < 0.0 || vReceiverUv.y > 1.0)) discard;
  float alpha = texture(uTex, vUv).a;
  if (uAlphaRef > 0.0 && alpha < uAlphaRef) discard;
  FragColor = vec4(uOpacity * (uAlphaRef > 0.0 ? alpha : 1.0));
}
)";

        const char* kShadowCompositeFragSrc = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uMask;
out vec4 FragColor;
void main() {
  FragColor = vec4(0.015, 0.02, 0.025, texture(uMask, vUv).r);
}
)";

        GLuint compile_shader(GLenum type, const char* src) {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            GLint ok = 0;
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[1024];
                glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
                std::fprintf(stderr, "[openkal:gl] shader error: %s\n", log);
            }
            return sh;
        }

        GLuint link_program(const char* vs_src, const char* fs_src) {
            const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
            const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
            GLuint prog = glCreateProgram();
            glAttachShader(prog, vs);
            glAttachShader(prog, fs);
            glLinkProgram(prog);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return prog;
        }

        // Map a typed letter (KalOnline WM_CHAR hotkeys) to the GLFW key that produces
        // it on the current keyboard layout (fixes QWERTZ Z/Y swap for haste).
        int glfw_key_for_letter(char letter, int fallback) {
            const char want = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; ++k) {
                const char* name = glfwGetKeyName(k, 0);
                if (name && name[0] != '\0' &&
                    std::tolower(static_cast<unsigned char>(name[0])) == want) {
                    return k;
                }
            }
            return fallback;
        }

        GLFWmonitor* monitor_for_window(GLFWwindow* window) {
            int window_x = 0;
            int window_y = 0;
            int window_w = 0;
            int window_h = 0;
            glfwGetWindowPos(window, &window_x, &window_y);
            glfwGetWindowSize(window, &window_w, &window_h);

            int monitor_count = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
            GLFWmonitor* best = glfwGetPrimaryMonitor();
            int best_overlap = -1;
            for (int index = 0; index < monitor_count; ++index) {
                int monitor_x = 0;
                int monitor_y = 0;
                glfwGetMonitorPos(monitors[index], &monitor_x, &monitor_y);
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[index]);
                if (mode == nullptr) {
                    continue;
                }
                const int overlap_w =
                    std::max(0,
                             std::min(window_x + window_w, monitor_x + mode->width) -
                                 std::max(window_x, monitor_x));
                const int overlap_h =
                    std::max(0,
                             std::min(window_y + window_h, monitor_y + mode->height) -
                                 std::max(window_y, monitor_y));
                const int overlap = overlap_w * overlap_h;
                if (overlap > best_overlap) {
                    best = monitors[index];
                    best_overlap = overlap;
                }
            }
            return best;
        }

        struct GpuMesh {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            GLuint m_normal_buffer = 0;
            float m_specular_power = 0.0f;
            std::vector<VertexP3T2> m_surface_vertices;
            std::vector<std::uint32_t> m_surface_indices;
            GLsizei index_count = 0;
            std::size_t vert_count = 0;
        };

        struct GpuTexture {
            GLuint id = 0;
        };

        class GlEngine final : public IEngine {
          public:
            bool init(const char* title, int width, int height) override {
                if (!glfwInit()) {
                    return false;
                }
                key_letter_z_ = glfw_key_for_letter('z', GLFW_KEY_Z);
                key_letter_x_ = glfw_key_for_letter('x', GLFW_KEY_X);
                key_letter_q_ = glfw_key_for_letter('q', GLFW_KEY_Q);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_DEPTH_BITS, 24);
                window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
                if (!window_) {
                    glfwTerminate();
                    return false;
                }
                glfwMakeContextCurrent(window_);
                windowed_w_ = width;
                windowed_h_ = height;
                glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
                glfwSwapInterval(1);
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                cursor_mode_ = CursorMode::Free;
                glfwSetWindowUserPointer(window_, this);
                glfwSetCursorPosCallback(window_, &GlEngine::cursor_cb);
                glfwSetScrollCallback(window_, &GlEngine::scroll_cb);
                glfwSetCharCallback(window_, &GlEngine::char_cb);
                glfwSetKeyCallback(window_, &GlEngine::key_cb);
                m_backspace_action = false;
                glfwSetDropCallback(window_, &GlEngine::drop_cb);
                glfwGetCursorPos(window_, &last_mx_, &last_my_);

                glewExperimental = GL_TRUE;
                if (glewInit() != GLEW_OK) {
                    return false;
                }

                program_ = link_program(kMeshVertSrc, kFragSrc);
                u_mvp_ = glGetUniformLocation(program_, "uMVP");
                u_model_ = glGetUniformLocation(program_, "uModel");
                u_cam_pos_ = glGetUniformLocation(program_, "uCamPos");
                u_uv_offset_ = glGetUniformLocation(program_, "uUvOffset");
                u_tex_ = glGetUniformLocation(program_, "uTex");
                u_alpha_ref_ = glGetUniformLocation(program_, "uAlphaRef");
                u_color_mul_ = glGetUniformLocation(program_, "uColorMul");
                m_specular_power_uniform = glGetUniformLocation(program_, "uSpecularPower");
                m_white_mix_uniform = glGetUniformLocation(program_, "uWhiteMix");

                mesh_instance_program_ = link_program(kMeshInstanceVertSrc, kFragSrc);
                u_mesh_instance_mvp_ = glGetUniformLocation(mesh_instance_program_, "uMVP");
                u_mesh_instance_cam_pos_ = glGetUniformLocation(mesh_instance_program_, "uCamPos");
                u_mesh_instance_tex_ = glGetUniformLocation(mesh_instance_program_, "uTex");
                u_mesh_instance_alpha_ref_ =
                    glGetUniformLocation(mesh_instance_program_, "uAlphaRef");
                u_mesh_instance_color_mul_ =
                    glGetUniformLocation(mesh_instance_program_, "uColorMul");
                m_instance_specular_uniform =
                    glGetUniformLocation(mesh_instance_program_, "uSpecularPower");
                glGenBuffers(1, &mesh_instance_vbo_);

                static_assert(sizeof(SpriteBatchVertex) == sizeof(float) * 9);
                sprite_batch_program_ = link_program(kSpriteBatchVertSrc, kSpriteBatchFragSrc);
                u_sprite_batch_mvp_ = glGetUniformLocation(sprite_batch_program_, "uMVP");
                u_sprite_batch_cam_pos_ = glGetUniformLocation(sprite_batch_program_, "uCamPos");
                u_sprite_batch_tex_ = glGetUniformLocation(sprite_batch_program_, "uTex");
                glGenVertexArrays(1, &sprite_batch_vao_);
                glGenBuffers(1, &sprite_batch_vbo_);
                glBindVertexArray(sprite_batch_vao_);
                glBindBuffer(GL_ARRAY_BUFFER, sprite_batch_vbo_);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteBatchVertex), nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1,
                                      2,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(SpriteBatchVertex),
                                      reinterpret_cast<void*>(sizeof(float) * 3));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2,
                                      4,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(SpriteBatchVertex),
                                      reinterpret_cast<void*>(sizeof(float) * 5));
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);

                lm_program_ = link_program(kLmVertSrc, kLmFragSrc);
                u_lm_mvp_ = glGetUniformLocation(lm_program_, "uMVP");
                u_lm_model_ = glGetUniformLocation(lm_program_, "uModel");
                u_lm_cam_pos_ = glGetUniformLocation(lm_program_, "uCamPos");
                u_lm_uv_offset_ = glGetUniformLocation(lm_program_, "uUvOffset");
                u_lm_tex_ = glGetUniformLocation(lm_program_, "uTex");
                u_lm_lightmap_ = glGetUniformLocation(lm_program_, "uLightmap");
                u_lm_alpha_ref_ = glGetUniformLocation(lm_program_, "uAlphaRef");
                u_lm_color_mul_ = glGetUniformLocation(lm_program_, "uColorMul");

                water_program_ = link_program(kMeshVertSrc, kWaterFragSrc);
                u_water_mvp_ = glGetUniformLocation(water_program_, "uMVP");
                u_water_model_ = glGetUniformLocation(water_program_, "uModel");
                u_water_uv_ = glGetUniformLocation(water_program_, "uUvOffset");
                u_water_tex_ = glGetUniformLocation(water_program_, "uTex");
                u_water_cam_ = glGetUniformLocation(water_program_, "uCamPos");
                u_water_sky_ = glGetUniformLocation(water_program_, "uSkyColor");
                u_water_deep_ = glGetUniformLocation(water_program_, "uDeepColor");
                u_water_time_ = glGetUniformLocation(water_program_, "uTime");

                ui_program_ = link_program(kUiVertSrc, kUiFragSrc);
                u_ui_mvp_ = glGetUniformLocation(ui_program_, "uMVP");
                u_ui_tex_ = glGetUniformLocation(ui_program_, "uTex");
                u_ui_color_ = glGetUniformLocation(ui_program_, "uColor");
                u_ui_uv_rect_ = glGetUniformLocation(ui_program_, "uUvRect");
                m_ui_clip_uniform = glGetUniformLocation(ui_program_, "uClipCircle");
                m_ui_depth_sampler = glGetUniformLocation(ui_program_, "uSceneDepth");
                m_ui_depth_uniform = glGetUniformLocation(ui_program_, "uWorldDepth");

                bright_pass_program_ = link_program(kPostVertSrc, kBrightPassFragSrc);
                u_bright_scene_ = glGetUniformLocation(bright_pass_program_, "uScene");
                blur_program_ = link_program(kPostVertSrc, kBlurFragSrc);
                u_blur_image_ = glGetUniformLocation(blur_program_, "uImage");
                u_blur_direction_ = glGetUniformLocation(blur_program_, "uDirection");
                bloom_program_ = link_program(kPostVertSrc, kBloomCompositeFragSrc);
                u_bloom_scene_ = glGetUniformLocation(bloom_program_, "uScene");
                u_bloom_blur_ = glGetUniformLocation(bloom_program_, "uBloom");
                u_bloom_strength_ = glGetUniformLocation(bloom_program_, "uStrength");
                glGenVertexArrays(1, &post_vao_);

                m_outline_mask_program = link_program(kMeshVertSrc, kOutlineMaskFragSrc);
                m_outline_mvp_uniform = glGetUniformLocation(m_outline_mask_program, "uMVP");
                m_outline_model_uniform = glGetUniformLocation(m_outline_mask_program, "uModel");
                m_outline_camera_uniform = glGetUniformLocation(m_outline_mask_program, "uCamPos");
                m_outline_alpha_uniform =
                    glGetUniformLocation(m_outline_mask_program, "uAlphaCutoff");
                m_outline_program = link_program(kPostVertSrc, kOutlineFragSrc);
                m_outline_bounds_uniform = glGetUniformLocation(m_outline_program, "uBounds");
                m_outline_radius_uniform = glGetUniformLocation(m_outline_program, "uRadius");
                m_outline_opacity_uniform = glGetUniformLocation(m_outline_program, "uOpacity");

                shadow_program_ = link_program(kShadowVertSrc, kShadowFragSrc);
                u_shadow_vp_ = glGetUniformLocation(shadow_program_, "uViewProj");
                u_shadow_model_ = glGetUniformLocation(shadow_program_, "uModel");
                u_shadow_cam_pos_ = glGetUniformLocation(shadow_program_, "uCamPos");
                u_shadow_ground_y_ = glGetUniformLocation(shadow_program_, "uGroundY");
                u_shadow_height_map_ = glGetUniformLocation(shadow_program_, "uHeightMap");
                u_shadow_receiver_origin_ =
                    glGetUniformLocation(shadow_program_, "uReceiverOrigin");
                u_shadow_receiver_size_ = glGetUniformLocation(shadow_program_, "uReceiverSize");
                u_shadow_use_height_map_ = glGetUniformLocation(shadow_program_, "uUseHeightMap");
                u_shadow_tex_ = glGetUniformLocation(shadow_program_, "uTex");
                u_shadow_alpha_ref_ = glGetUniformLocation(shadow_program_, "uAlphaRef");
                u_shadow_opacity_ = glGetUniformLocation(shadow_program_, "uOpacity");
                m_shadow_composite_program = link_program(kPostVertSrc, kShadowCompositeFragSrc);
                m_shadow_mask_uniform = glGetUniformLocation(m_shadow_composite_program, "uMask");

                terrain_program_ = link_program(kTerrainVertSrc, kTerrainFragSrc);
                u_terrain_vp_ = glGetUniformLocation(terrain_program_, "uViewProj");
                u_terrain_cam_ = glGetUniformLocation(terrain_program_, "uCamPos");
                u_terrain_color_ = glGetUniformLocation(terrain_program_, "uColor");
                u_terrain_detail_ = glGetUniformLocation(terrain_program_, "uDetail");
                u_terrain_repeat_ = glGetUniformLocation(terrain_program_, "uRepeat");
                u_terrain_sun_ = glGetUniformLocation(terrain_program_, "uSun");

                lit_program_ = link_program(kLitVertSrc, kLitFragSrc);
                u_lit_mvp_ = glGetUniformLocation(lit_program_, "uMVP");
                u_lit_model_ = glGetUniformLocation(lit_program_, "uModel");
                u_lit_cam_ = glGetUniformLocation(lit_program_, "uCamPos");
                u_lit_color_ = glGetUniformLocation(lit_program_, "uColor");

                const std::uint8_t white[4] = {255, 255, 255, 255};
                white_tex_ = create_texture_rgba8(white, 1, 1, false);

                // Unit quad in pixel space will be transformed; local 0..1 square.
                const VertexP3T2 qv[4] = {
                    {0, 0, 0, 0, 0},
                    {1, 0, 0, 1, 0},
                    {1, 1, 0, 1, 1},
                    {0, 1, 0, 0, 1},
                };
                const std::uint32_t qi[6] = {0, 1, 2, 0, 2, 3};
                ui_quad_ = create_mesh(qv, 4, qi, 6);
                const VertexP3T2 tv[3] = {
                    {0, 0, 0, 0, 0},
                    {1, 0, 0, 1, 0},
                    {0, 1, 0, 0, 1},
                };
                const std::uint32_t ti[3] = {0, 1, 2};
                ui_tri_ = create_mesh(tv, 3, ti, 3);

                glEnable(GL_DEPTH_TEST);
                // D3D default is LESSEQUAL; later coplanar draws (MAP_OBJECT on
                // region floors) must win. GL_LESS leaves the floor in the buffer.
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                // View matrix reflects X (D3D LH → OpenGL RH); winding becomes compatibility
                // behavior in clip space.
                glFrontFace(GL_CW);
                last_time_ = std::chrono::steady_clock::now();
                frame_start_ = last_time_;
                return true;
            }

            void shutdown() override {
                destroy_shadow_target();
                destroy_outline_target();
                destroy_offscreen();
                destroy_scene_target();
                if (sprite_batch_vbo_) {
                    glDeleteBuffers(1, &sprite_batch_vbo_);
                    sprite_batch_vbo_ = 0;
                }
                if (mesh_instance_vbo_) {
                    glDeleteBuffers(1, &mesh_instance_vbo_);
                    mesh_instance_vbo_ = 0;
                }
                if (sprite_batch_vao_) {
                    glDeleteVertexArrays(1, &sprite_batch_vao_);
                    sprite_batch_vao_ = 0;
                }
                for (auto& [id, mesh] : meshes_) {
                    destroy_gpu_mesh(mesh);
                }
                meshes_.clear();
                for (auto& [id, tex] : textures_) {
                    if (tex.id) {
                        glDeleteTextures(1, &tex.id);
                    }
                }
                textures_.clear();
                white_tex_ = {};
                if (program_) {
                    glDeleteProgram(program_);
                    program_ = 0;
                }
                if (mesh_instance_program_) {
                    glDeleteProgram(mesh_instance_program_);
                    mesh_instance_program_ = 0;
                }
                if (sprite_batch_program_) {
                    glDeleteProgram(sprite_batch_program_);
                    sprite_batch_program_ = 0;
                }
                if (lm_program_) {
                    glDeleteProgram(lm_program_);
                    lm_program_ = 0;
                }
                if (water_program_) {
                    glDeleteProgram(water_program_);
                    water_program_ = 0;
                }
                if (terrain_program_) {
                    glDeleteProgram(terrain_program_);
                    terrain_program_ = 0;
                }
                if (lit_program_) {
                    glDeleteProgram(lit_program_);
                    lit_program_ = 0;
                }
                if (ui_program_) {
                    glDeleteProgram(ui_program_);
                    ui_program_ = 0;
                }
                if (m_ui_scene_depth) {
                    glDeleteTextures(1, &m_ui_scene_depth);
                    m_ui_scene_depth = 0;
                    m_ui_depth_width = m_ui_depth_height = 0;
                }
                if (bloom_program_) {
                    glDeleteProgram(bloom_program_);
                    bloom_program_ = 0;
                }
                if (bright_pass_program_) {
                    glDeleteProgram(bright_pass_program_);
                    bright_pass_program_ = 0;
                }
                if (blur_program_) {
                    glDeleteProgram(blur_program_);
                    blur_program_ = 0;
                }
                if (shadow_program_) {
                    glDeleteProgram(shadow_program_);
                    shadow_program_ = 0;
                }
                if (m_shadow_composite_program) {
                    glDeleteProgram(m_shadow_composite_program);
                    m_shadow_composite_program = 0;
                }
                if (m_outline_mask_program) {
                    glDeleteProgram(m_outline_mask_program);
                    m_outline_mask_program = 0;
                }
                if (m_outline_program) {
                    glDeleteProgram(m_outline_program);
                    m_outline_program = 0;
                }
                if (post_vao_) {
                    glDeleteVertexArrays(1, &post_vao_);
                    post_vao_ = 0;
                }
                if (window_) {
                    glfwDestroyWindow(window_);
                    window_ = nullptr;
                }
                glfwTerminate();
            }

            std::string clipboard_text() const override {
                const char* text = window_ ? glfwGetClipboardString(window_) : nullptr;
                return text ? text : "";
            }

            bool set_clipboard_text(const std::string& text) override {
                if (!window_) {
                    return false;
                }
                glfwSetClipboardString(window_, text.c_str());
                return true;
            }

            bool poll(FrameInput& out) override {
                out = {};
                // Mark the start of this frame for 60 Hz pacing in end_frame().
                frame_start_ = std::chrono::steady_clock::now();
                glfwPollEvents();
                if (!window_ || glfwWindowShouldClose(window_)) {
                    out.quit = true;
                    return false;
                }
                glfwGetFramebufferSize(window_, &out.width, &out.height);
                int win_w = 0, win_h = 0;
                glfwGetWindowSize(window_, &win_w, &win_h);
                const float sx =
                    (win_w > 0) ? static_cast<float>(out.width) / static_cast<float>(win_w) : 1.0f;
                const float sy =
                    (win_h > 0) ? static_cast<float>(out.height) / static_cast<float>(win_h) : 1.0f;

                out.key_w = glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS;
                out.key_a = glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS;
                out.key_s = glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS;
                out.key_d = glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS;
                out.key_space = glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS;
                out.key_ctrl = glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                               glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
                out.key_shift = glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                out.key_escape = glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
                out.key_enter = glfwGetKey(window_, GLFW_KEY_ENTER) == GLFW_PRESS ||
                                glfwGetKey(window_, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
                out.key_backspace = glfwGetKey(window_, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
                out.key_delete = glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS;
                out.key_i = glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS;
                out.key_e = glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS;
                out.key_k = glfwGetKey(window_, GLFW_KEY_K) == GLFW_PRESS;
                out.key_p = glfwGetKey(window_, glfw_key_for_letter('p', GLFW_KEY_P)) == GLFW_PRESS;
                out.key_o = glfwGetKey(window_, glfw_key_for_letter('o', GLFW_KEY_O)) == GLFW_PRESS;
                out.key_g = glfwGetKey(window_, glfw_key_for_letter('g', GLFW_KEY_G)) == GLFW_PRESS;
                out.key_q = glfwGetKey(window_, key_letter_q_) == GLFW_PRESS;
                out.key_f = glfwGetKey(window_, glfw_key_for_letter('f', GLFW_KEY_F)) == GLFW_PRESS;
                // Letter hotkeys (KalOnline WM_CHAR 'z'/'x'): resolve by key *label* so
                // QWERTZ Z still toggles haste (GLFW_KEY_Z is the US-Z physical key =
                // labeled Y on DE).
                out.key_z = glfwGetKey(window_, key_letter_z_) == GLFW_PRESS;
                out.key_x = glfwGetKey(window_, key_letter_x_) == GLFW_PRESS;
                out.key_h = glfwGetKey(window_, glfw_key_for_letter('h', GLFW_KEY_H)) == GLFW_PRESS;
                out.key_c = glfwGetKey(window_, glfw_key_for_letter('c', GLFW_KEY_C)) == GLFW_PRESS;
                out.key_v = glfwGetKey(window_, glfw_key_for_letter('v', GLFW_KEY_V)) == GLFW_PRESS;
                out.key_l = glfwGetKey(window_, glfw_key_for_letter('l', GLFW_KEY_L)) == GLFW_PRESS;
                out.key_m = glfwGetKey(window_, glfw_key_for_letter('m', GLFW_KEY_M)) == GLFW_PRESS;
                out.key_r = glfwGetKey(window_, glfw_key_for_letter('r', GLFW_KEY_R)) == GLFW_PRESS;
                out.key_left = glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS;
                out.key_right = glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS;
                out.key_up = glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS;
                out.key_down = glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS;
                static constexpr int kDigitKeys[10][2] = {
                    {GLFW_KEY_0, GLFW_KEY_KP_0},
                    {GLFW_KEY_1, GLFW_KEY_KP_1},
                    {GLFW_KEY_2, GLFW_KEY_KP_2},
                    {GLFW_KEY_3, GLFW_KEY_KP_3},
                    {GLFW_KEY_4, GLFW_KEY_KP_4},
                    {GLFW_KEY_5, GLFW_KEY_KP_5},
                    {GLFW_KEY_6, GLFW_KEY_KP_6},
                    {GLFW_KEY_7, GLFW_KEY_KP_7},
                    {GLFW_KEY_8, GLFW_KEY_KP_8},
                    {GLFW_KEY_9, GLFW_KEY_KP_9},
                };
                for (int d = 0; d < 10; ++d) {
                    out.key_digit[d] = glfwGetKey(window_, kDigitKeys[d][0]) == GLFW_PRESS ||
                                       glfwGetKey(window_, kDigitKeys[d][1]) == GLFW_PRESS;
                    out.key_digit_pressed[d] = out.key_digit[d] && !prev_digit_[d];
                    prev_digit_[d] = out.key_digit[d];
                }
                out.key_escape_pressed = out.key_escape && !prev_escape_;
                out.key_enter_pressed = out.key_enter && !prev_enter_;
                const bool tab = glfwGetKey(window_, GLFW_KEY_TAB) == GLFW_PRESS;
                out.key_tab_pressed = tab && !m_prev_tab;
                m_prev_tab = tab;
                out.key_backspace_pressed =
                    (out.key_backspace && !prev_backspace_) || m_backspace_action;
                m_backspace_action = false;
                out.key_delete_pressed = out.key_delete && !prev_delete_;
                out.key_i_pressed = out.key_i && !prev_i_;
                out.key_e_pressed = out.key_e && !prev_e_;
                out.key_k_pressed = out.key_k && !prev_k_;
                out.key_p_pressed = out.key_p && !prev_p_;
                out.key_o_pressed = out.key_o && !prev_o_;
                out.key_g_pressed = out.key_g && !prev_g_;
                out.key_q_pressed = out.key_q && !prev_q_;
                out.key_f_pressed = out.key_f && !prev_f_;
                out.key_s_pressed = out.key_s && !prev_s_;
                out.key_z_pressed = out.key_z && !prev_z_;
                out.key_x_pressed = out.key_x && !prev_x_;
                out.key_h_pressed = out.key_h && !prev_h_;
                out.key_c_pressed = out.key_c && !prev_c_;
                out.key_v_pressed = out.key_v && !prev_v_;
                out.key_l_pressed = out.key_l && !prev_l_;
                out.key_m_pressed = out.key_m && !prev_m_;
                out.key_r_pressed = out.key_r && !prev_r_;
                out.key_space_pressed = out.key_space && !prev_space_;
                prev_escape_ = out.key_escape;
                prev_enter_ = out.key_enter;
                prev_backspace_ = out.key_backspace;
                prev_delete_ = out.key_delete;
                prev_i_ = out.key_i;
                prev_e_ = out.key_e;
                prev_k_ = out.key_k;
                prev_p_ = out.key_p;
                prev_o_ = out.key_o;
                prev_g_ = out.key_g;
                prev_q_ = out.key_q;
                prev_f_ = out.key_f;
                prev_s_ = out.key_s;
                prev_z_ = out.key_z;
                prev_x_ = out.key_x;
                prev_h_ = out.key_h;
                prev_c_ = out.key_c;
                prev_v_ = out.key_v;
                prev_l_ = out.key_l;
                prev_m_ = out.key_m;
                prev_r_ = out.key_r;
                prev_space_ = out.key_space;

                out.mouse_left = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                out.mouse_right =
                    glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
                out.mouse_middle =
                    glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
                out.mouse_left_clicked = out.mouse_left && !prev_mouse_left_;
                out.mouse_right_clicked = out.mouse_right && !prev_mouse_right_;
                out.mouse_middle_clicked = out.mouse_middle && !prev_mouse_middle_;
                prev_mouse_left_ = out.mouse_left;
                prev_mouse_right_ = out.mouse_right;
                prev_mouse_middle_ = out.mouse_middle;

                out.mouse_dx = mouse_dx_;
                out.mouse_dy = mouse_dy_;
                out.mouse_scroll = mouse_scroll_;
                mouse_dx_ = mouse_dy_ = 0;
                mouse_scroll_ = 0;
                out.mouse_x = static_cast<float>(last_mx_) * sx;
                out.mouse_y = static_cast<float>(last_my_) * sy;

                out.text_char_count = 0;
                for (char c : pending_chars_) {
                    if (out.text_char_count >= 31) {
                        break;
                    }
                    out.text_chars[out.text_char_count++] = c;
                }
                out.text_chars[out.text_char_count] = 0;
                pending_chars_.clear();

                out.dropped_files = std::move(pending_drops_);
                pending_drops_.clear();

                fb_w_ = out.width;
                fb_h_ = out.height;

                const auto now = std::chrono::steady_clock::now();
                out.dt = std::chrono::duration<float>(now - last_time_).count();
                last_time_ = now;
                if (out.dt > 0.1f) {
                    out.dt = 0.1f;
                }
                return true;
            }

            void maximize_window() override {
                if (window_) {
                    glfwMaximizeWindow(window_);
                }
            }

            bool set_window_mode(WindowMode mode, int width, int height) override {
                if (!window_ || width <= 0 || height <= 0) {
                    return false;
                }

                if (window_mode_ != WindowMode::Borderless) {
                    glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
                    glfwGetWindowSize(window_, &windowed_w_, &windowed_h_);
                }
                windowed_w_ = width;
                windowed_h_ = height;

                if (mode == WindowMode::Borderless) {
                    GLFWmonitor* monitor = monitor_for_window(window_);
                    if (!monitor) {
                        return false;
                    }
                    int work_x = 0;
                    int work_y = 0;
                    int work_w = 0;
                    int work_h = 0;
                    glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_w, &work_h);
                    if (work_w <= 0 || work_h <= 0) {
                        return false;
                    }
                    glfwRestoreWindow(window_);
                    glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_FALSE);
                    glfwSetWindowMonitor(
                        window_, nullptr, work_x, work_y, work_w, work_h, GLFW_DONT_CARE);
                } else {
                    glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_TRUE);
                    glfwSetWindowMonitor(window_,
                                         nullptr,
                                         windowed_x_,
                                         windowed_y_,
                                         windowed_w_,
                                         windowed_h_,
                                         GLFW_DONT_CARE);
                    if (mode == WindowMode::Maximized) {
                        glfwMaximizeWindow(window_);
                    } else {
                        glfwRestoreWindow(window_);
                    }
                }
                window_mode_ = mode;
                return true;
            }

            void set_cursor_mode(CursorMode mode) override {
                if (!window_) {
                    return;
                }
                cursor_mode_ = mode;
                if (mode == CursorMode::Captured) {
                    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                } else {
                    // Free mouse + software Kal cursors: hide the OS pointer.
                    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                }
                glfwGetCursorPos(window_, &last_mx_, &last_my_);
                mouse_dx_ = mouse_dy_ = 0;
            }

            RenderCapabilities render_capabilities() const override {
                return {bloom_program_ != 0 && bright_pass_program_ != 0 && blur_program_ != 0};
            }

            void set_bloom_strength(float strength) override {
                bloom_strength_ = std::clamp(strength, 0.0f, 1.0f);
            }

            void begin_frame(float r, float g, float b) override {
                m_ui_depth_captured = false;
                m_ui_pass_started = false;
                m_ui_world_depth = -1.0f;
                draw_mesh_ = 0;
                draw_terrain_ = 0;
                draw_ui_ = 0;
                draw_other_ = 0;
                if (offscreen_fbo_) {
                    scene_pending_ = false;
                    glBindFramebuffer(GL_FRAMEBUFFER, offscreen_fbo_);
                    glViewport(0, 0, offscreen_w_, offscreen_h_);
                    fb_w_ = offscreen_w_;
                    fb_h_ = offscreen_h_;
                } else {
                    int w = 0;
                    int h = 0;
                    glfwGetFramebufferSize(window_, &w, &h);
                    glViewport(0, 0, w, h);
                    fb_w_ = w;
                    fb_h_ = h;
                    scene_pending_ = bloom_strength_ > 0.0f && ensure_scene_target(w, h);
                    glBindFramebuffer(GL_FRAMEBUFFER,
                                      scene_pending_ ? scene_fbo_ : static_cast<GLuint>(0));
                }
                glClearColor(r, g, b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
            }

            void begin_ui_pass() override {
                if (!m_ui_pass_started) {
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_ui_depth_source);
                    m_ui_pass_started = true;
                }
                compose_scene();
            }

            void set_ui_world_anchor(const Vec3* anchor) override {
                m_ui_world_depth = -1.0f;
                if (!anchor || fb_w_ <= 0 || fb_h_ <= 0) {
                    return;
                }
                if (!m_ui_depth_captured) {
                    capture_ui_depth();
                }
                const float x = anchor->x - cam_pos_.x;
                const float y = anchor->y - cam_pos_.y;
                const float z = anchor->z - cam_pos_.z;
                const auto& vp = view_rot_proj_.m;
                const float clip_z = vp[2] * x + vp[6] * y + vp[10] * z + vp[14];
                const float clip_w = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
                m_ui_world_depth = clip_w > 0.0f ? (clip_z / clip_w + 1.0f) * 0.5f : 2.0f;
                if (m_ui_world_depth < 0.0f) {
                    m_ui_world_depth = 2.0f;
                }
            }

            FrameDrawStats frame_stats() const override {
                return published_;
            }

            bool begin_offscreen(
                int width, int height, float clear_r, float clear_g, float clear_b) override {
                if (width <= 0 || height <= 0) {
                    return false;
                }
                destroy_offscreen();
                m_ui_depth_captured = false;
                m_ui_pass_started = false;
                m_ui_world_depth = -1.0f;
                offscreen_w_ = width;
                offscreen_h_ = height;

                glGenTextures(1, &offscreen_color_);
                glBindTexture(GL_TEXTURE_2D, offscreen_color_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA8,
                             width,
                             height,
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             nullptr);

                glGenRenderbuffers(1, &offscreen_depth_);
                glBindRenderbuffer(GL_RENDERBUFFER, offscreen_depth_);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

                glGenFramebuffers(1, &offscreen_fbo_);
                glBindFramebuffer(GL_FRAMEBUFFER, offscreen_fbo_);
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, offscreen_color_, 0);
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, offscreen_depth_);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    destroy_offscreen();
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return false;
                }

                fb_w_ = width;
                fb_h_ = height;
                glViewport(0, 0, width, height);
                glClearColor(clear_r, clear_g, clear_b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_DEPTH_TEST);
                return true;
            }

            bool end_offscreen_rgba(std::vector<std::uint8_t>& rgba_out) override {
                if (!offscreen_fbo_ || offscreen_w_ <= 0 || offscreen_h_ <= 0) {
                    return false;
                }
                glBindFramebuffer(GL_FRAMEBUFFER, offscreen_fbo_);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                std::vector<std::uint8_t> bottom_up(static_cast<std::size_t>(offscreen_w_) *
                                                    static_cast<std::size_t>(offscreen_h_) * 4);
                glReadPixels(
                    0, 0, offscreen_w_, offscreen_h_, GL_RGBA, GL_UNSIGNED_BYTE, bottom_up.data());

                // Flip to top-left origin.
                rgba_out.resize(bottom_up.size());
                const std::size_t row_bytes = static_cast<std::size_t>(offscreen_w_) * 4;
                for (int y = 0; y < offscreen_h_; ++y) {
                    const std::size_t src =
                        static_cast<std::size_t>(offscreen_h_ - 1 - y) * row_bytes;
                    const std::size_t dst = static_cast<std::size_t>(y) * row_bytes;
                    std::memcpy(rgba_out.data() + dst, bottom_up.data() + src, row_bytes);
                }

                destroy_offscreen();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                int w = 0;
                int h = 0;
                if (window_) {
                    glfwGetFramebufferSize(window_, &w, &h);
                }
                fb_w_ = w;
                fb_h_ = h;
                if (w > 0 && h > 0) {
                    glViewport(0, 0, w, h);
                }
                return true;
            }

            void set_camera(const Mat4& view, const Mat4& proj) override {
                m_mesh_view = view;
                view_rot_proj_ = view;
                view_rot_proj_.m[12] = 0.0f;
                view_rot_proj_.m[13] = 0.0f;
                view_rot_proj_.m[14] = 0.0f;
                view_rot_proj_ = mul(proj, view_rot_proj_);
                Mat4 inv{};
                if (Mat4::invert(view, inv)) {
                    cam_pos_.x = inv.m[12];
                    cam_pos_.y = inv.m[13];
                    cam_pos_.z = inv.m[14];
                } else {
                    cam_pos_ = {};
                }
            }

            void set_polygon_offset(float factor, float units) override {
                if (factor == 0.0f && units == 0.0f) {
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(0.0f, 0.0f);
                    return;
                }
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(factor, units);
            }

            void set_depth_less_equal(bool enable) override {
                glDepthFunc(enable ? GL_LEQUAL : GL_LESS);
            }

            TextureHandle create_texture_rgba8(const std::uint8_t* rgba,
                                               int width,
                                               int height,
                                               bool wrap = false,
                                               bool nearest = false) override {
                GpuTexture tex;
                glGenTextures(1, &tex.id);
                glBindTexture(GL_TEXTURE_2D, tex.id);
                if (nearest) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
                const GLint wrap_mode = wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
                glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                if (!nearest) {
                    glGenerateMipmap(GL_TEXTURE_2D);
                    if (wrap && GLEW_EXT_texture_filter_anisotropic) {
                        GLfloat max_aniso = 1.0f;
                        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
                        if (max_aniso > 1.0f) {
                            glTexParameterf(GL_TEXTURE_2D,
                                            GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                            max_aniso > 8.0f ? 8.0f : max_aniso);
                        }
                    }
                }
                glBindTexture(GL_TEXTURE_2D, 0);
                const TextureHandle handle{++next_tex_};
                textures_[handle.id] = tex;
                return handle;
            }

            TextureHandle create_texture_r32f(const float* values, int width, int height) override {
                if (values == nullptr || width <= 0 || height <= 0) {
                    return {};
                }
                GpuTexture tex;
                glGenTextures(1, &tex.id);
                glBindTexture(GL_TEXTURE_2D, tex.id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, values);
                glBindTexture(GL_TEXTURE_2D, 0);
                const TextureHandle handle{++next_tex_};
                textures_[handle.id] = tex;
                return handle;
            }

            void destroy_texture(TextureHandle handle) override {
                auto it = textures_.find(handle.id);
                if (it == textures_.end()) {
                    return;
                }
                if (it->second.id) {
                    glDeleteTextures(1, &it->second.id);
                }
                textures_.erase(it);
            }

            MeshHandle create_mesh(const VertexP3T2* verts,
                                   std::size_t vert_count,
                                   const std::uint32_t* indices,
                                   std::size_t index_count) override {
                GpuMesh mesh;
                glGenVertexArrays(1, &mesh.vao);
                glGenBuffers(1, &mesh.vbo);
                glGenBuffers(1, &mesh.ebo);
                glBindVertexArray(mesh.vao);
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(vert_count * sizeof(VertexP3T2)),
                             verts,
                             GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(index_count * sizeof(std::uint32_t)),
                             indices,
                             GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0,
                                      3,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(VertexP3T2),
                                      reinterpret_cast<void*>(offsetof(VertexP3T2, px)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1,
                                      2,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(VertexP3T2),
                                      reinterpret_cast<void*>(offsetof(VertexP3T2, u)));
                glBindVertexArray(0);
                mesh.index_count = static_cast<GLsizei>(index_count);
                mesh.vert_count = vert_count;
                const MeshHandle handle{++next_mesh_};
                meshes_[handle.id] = mesh;
                return handle;
            }

            MeshHandle create_mesh_lm(const VertexP3T2T2* verts,
                                      std::size_t vert_count,
                                      const std::uint32_t* indices,
                                      std::size_t index_count) override {
                GpuMesh mesh;
                glGenVertexArrays(1, &mesh.vao);
                glGenBuffers(1, &mesh.vbo);
                glGenBuffers(1, &mesh.ebo);
                glBindVertexArray(mesh.vao);
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(vert_count * sizeof(VertexP3T2T2)),
                             verts,
                             GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(index_count * sizeof(std::uint32_t)),
                             indices,
                             GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0,
                                      3,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(VertexP3T2T2),
                                      reinterpret_cast<void*>(offsetof(VertexP3T2T2, px)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1,
                                      2,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(VertexP3T2T2),
                                      reinterpret_cast<void*>(offsetof(VertexP3T2T2, u)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2,
                                      2,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(VertexP3T2T2),
                                      reinterpret_cast<void*>(offsetof(VertexP3T2T2, lu)));
                glBindVertexArray(0);
                mesh.index_count = static_cast<GLsizei>(index_count);
                mesh.vert_count = vert_count;
                const MeshHandle handle{++next_mesh_};
                meshes_[handle.id] = mesh;
                return handle;
            }

            bool update_mesh(MeshHandle handle,
                             const VertexP3T2* verts,
                             std::size_t vert_count) override {
                auto it = meshes_.find(handle.id);
                if (it == meshes_.end() || !verts || vert_count != it->second.vert_count) {
                    return false;
                }
                glBindBuffer(GL_ARRAY_BUFFER, it->second.vbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(vert_count * sizeof(VertexP3T2)),
                             verts,
                             GL_DYNAMIC_DRAW);
                if (!it->second.m_surface_vertices.empty()) {
                    it->second.m_surface_vertices.assign(verts, verts + vert_count);
                }
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                return true;
            }

            void set_mesh_specular(MeshHandle handle,
                                   const std::vector<std::array<float, 3>>& normals,
                                   float power) override {
                const auto it = meshes_.find(handle.id);
                if (it == meshes_.end()) {
                    return;
                }
                auto& mesh = it->second;
                mesh.m_specular_power = 0.0f;
                if (!std::isfinite(power) || power <= 0.0f || normals.size() != mesh.vert_count ||
                    normals.empty()) {
                    return;
                }
                for (const auto& n : normals) {
                    for (const float component : n) {
                        if (!std::isfinite(component)) {
                            return;
                        }
                    }
                }
                if (!mesh.m_normal_buffer) {
                    glGenBuffers(1, &mesh.m_normal_buffer);
                }
                glBindVertexArray(mesh.vao);
                if (mesh.m_surface_vertices.empty()) {
                    // Only glossy meshes need CPU geometry for transparent triangle ordering.
                    mesh.m_surface_vertices.resize(mesh.vert_count);
                    mesh.m_surface_indices.resize(static_cast<std::size_t>(mesh.index_count));
                    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
                    glGetBufferSubData(
                        GL_ARRAY_BUFFER,
                        0,
                        static_cast<GLsizeiptr>(mesh.vert_count * sizeof(VertexP3T2)),
                        mesh.m_surface_vertices.data());
                    glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                                       0,
                                       static_cast<GLsizeiptr>(mesh.m_surface_indices.size() *
                                                               sizeof(std::uint32_t)),
                                       mesh.m_surface_indices.data());
                }
                glBindBuffer(GL_ARRAY_BUFFER, mesh.m_normal_buffer);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(normals.size() * sizeof(normals[0])),
                             normals.data(),
                             GL_DYNAMIC_DRAW);
                glEnableVertexAttribArray(6);
                glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), nullptr);
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                mesh.m_specular_power = std::clamp(power, 1.0f, 1024.0f);
            }

            void destroy_mesh(MeshHandle handle) override {
                auto it = meshes_.find(handle.id);
                if (it == meshes_.end()) {
                    return;
                }
                destroy_gpu_mesh(it->second);
                meshes_.erase(it);
            }

            void draw_mesh(MeshHandle mesh, TextureHandle texture) override {
                draw_mesh_textured(mesh,
                                   texture,
                                   Mat4::identity(),
                                   false,
                                   false,
                                   false,
                                   false,
                                   0.0f,
                                   0.0f,
                                   1.0f,
                                   1.0f,
                                   1.0f,
                                   1.0f);
            }

            void draw_mesh_textured(MeshHandle mesh,
                                    TextureHandle texture,
                                    const Mat4& model,
                                    bool alpha_blend,
                                    bool two_sided,
                                    bool alpha_test,
                                    bool additive,
                                    float uv_u,
                                    float uv_v,
                                    float mul_r,
                                    float mul_g,
                                    float mul_b,
                                    float mul_a,
                                    float white_mix = 0.0f) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end()) {
                    return;
                }
                glUseProgram(program_);
                glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_model_, 1, GL_FALSE, model.m.data());
                glUniform3f(u_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform2f(u_uv_offset_, uv_u, uv_v);
                glUniform4f(u_color_mul_, mul_r, mul_g, mul_b, mul_a);
                glUniform1f(m_specular_power_uniform, mit->second.m_specular_power);
                glUniform1f(m_white_mix_uniform, std::clamp(white_mix, 0.0f, 1.0f));
                glUniform1i(u_tex_, 0);
                // Soft blend wins over cutout (MAP_ALPHA_RGB path turns alphatest off).
                const float alpha_ref =
                    (!alpha_blend && !additive && alpha_test) ? (128.0f / 255.0f) : 0.0f;
                glUniform1f(u_alpha_ref_, alpha_ref);
                if (additive && alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    glDepthMask(GL_FALSE);
                } else if (additive) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                    glDepthMask(GL_FALSE);
                } else if (alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                } else {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                }
                if (two_sided) {
                    glDisable(GL_CULL_FACE);
                } else {
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CW);
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(mit->second.vao);
                const bool sorted_surface =
                    alpha_blend && two_sided && mit->second.m_specular_power > 0.0f;
                if (sorted_surface) {
                    sort_surface_indices(mit->second, model);
                }
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                if (sorted_surface) {
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                 static_cast<GLsizeiptr>(mit->second.m_surface_indices.size() *
                                                         sizeof(std::uint32_t)),
                                 mit->second.m_surface_indices.data(),
                                 GL_STREAM_DRAW);
                }
                glBindVertexArray(0);
                ++draw_mesh_;
                glUniform1f(m_white_mix_uniform, 0.0f);
                glUniform1f(m_specular_power_uniform, 0.0f);
                glUniform1f(u_alpha_ref_, 0.0f);
                glFrontFace(GL_CW);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
            }

            void draw_mesh_textured_instances(MeshHandle mesh,
                                              TextureHandle texture,
                                              const std::vector<Mat4>& models,
                                              bool two_sided,
                                              bool alpha_test) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end() || models.empty()) {
                    return;
                }
                static_assert(sizeof(Mat4) == sizeof(float) * 16);
                glUseProgram(mesh_instance_program_);
                glUniformMatrix4fv(u_mesh_instance_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniform3f(u_mesh_instance_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform1f(m_instance_specular_uniform, mit->second.m_specular_power);
                glUniform1i(u_mesh_instance_tex_, 0);
                glUniform1f(u_mesh_instance_alpha_ref_, alpha_test ? (128.0f / 255.0f) : 0.0f);
                glUniform4f(u_mesh_instance_color_mul_, 1.0f, 1.0f, 1.0f, 1.0f);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                if (two_sided) {
                    glDisable(GL_CULL_FACE);
                } else {
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CW);
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(mit->second.vao);
                glBindBuffer(GL_ARRAY_BUFFER, mesh_instance_vbo_);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(models.size() * sizeof(Mat4)),
                             models.data(),
                             GL_STREAM_DRAW);
                for (GLuint column = 0; column < 4; ++column) {
                    const GLuint attribute = 2 + column;
                    glEnableVertexAttribArray(attribute);
                    glVertexAttribPointer(attribute,
                                          4,
                                          GL_FLOAT,
                                          GL_FALSE,
                                          sizeof(Mat4),
                                          reinterpret_cast<void*>(sizeof(float) * 4 * column));
                    glVertexAttribDivisor(attribute, 1);
                }
                glDrawElementsInstanced(GL_TRIANGLES,
                                        mit->second.index_count,
                                        GL_UNSIGNED_INT,
                                        nullptr,
                                        static_cast<GLsizei>(models.size()));
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                ++draw_mesh_;
                glUniform1f(u_mesh_instance_alpha_ref_, 0.0f);
                glFrontFace(GL_CW);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
            }

            bool begin_mesh_outline(const Mat4& model,
                                    const Vec3& bounds_min,
                                    const Vec3& bounds_max,
                                    float radius,
                                    float opacity) override {
                if (m_outline_active || !m_outline_mask_program || !m_outline_program ||
                    fb_w_ <= 0 || fb_h_ <= 0 || !std::isfinite(radius) || radius <= 0.0f ||
                    !std::isfinite(opacity) || opacity <= 0.0f || bounds_min.x > bounds_max.x ||
                    bounds_min.y > bounds_max.y || bounds_min.z > bounds_max.z) {
                    return false;
                }
                radius = std::min(radius, 5.0f);
                const auto bounds = outline_screen_bounds(model, bounds_min, bounds_max, radius);
                const auto [left, bottom, right, top] = bounds;
                if (left >= right || bottom >= top) {
                    return false;
                }
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_outline_draw_target);
                if (!ensure_outline_target()) {
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outline_draw_target);
                    return false;
                }

                GLint previous_read = 0;
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outline_draw_target);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outline_fbo);
                // Keep only parts of the silhouette visible against the opaque scene.
                glBlitFramebuffer(left,
                                  bottom,
                                  right,
                                  top,
                                  left,
                                  bottom,
                                  right,
                                  top,
                                  GL_DEPTH_BUFFER_BIT,
                                  GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, previous_read);
                glEnable(GL_SCISSOR_TEST);
                glScissor(left, bottom, right - left, top - bottom);
                const GLfloat clear[] = {1.0f, 0.0f, 0.0f, 0.0f};
                glClearBufferfv(GL_COLOR, 0, clear);
                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LEQUAL);

                glUseProgram(m_outline_program);
                glUniform4iv(m_outline_bounds_uniform, 1, bounds.data());
                glUniform1f(m_outline_radius_uniform, radius);
                glUniform1f(m_outline_opacity_uniform, std::min(opacity, 1.0f));
                glUseProgram(m_outline_mask_program);
                glUniformMatrix4fv(m_outline_mvp_uniform, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(m_outline_model_uniform, 1, GL_FALSE, model.m.data());
                glUniform3f(m_outline_camera_uniform, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                m_outline_active = true;
                return true;
            }

            void draw_mesh_outline_part(MeshHandle mesh,
                                        TextureHandle texture,
                                        bool two_sided,
                                        float alpha_cutoff) override {
                const auto part = meshes_.find(mesh.id);
                if (!m_outline_active || part == meshes_.end()) {
                    return;
                }
                glUniform1f(m_outline_alpha_uniform, alpha_cutoff);
                if (two_sided) {
                    glDisable(GL_CULL_FACE);
                } else {
                    glEnable(GL_CULL_FACE);
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(part->second.vao);
                glDrawElements(GL_TRIANGLES, part->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_other_;
            }

            void end_mesh_outline() override {
                if (!m_outline_active) {
                    return;
                }
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outline_draw_target);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(m_outline_program);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_outline_mask);
                glBindVertexArray(post_vao_);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                ++draw_other_;
                glDisable(GL_SCISSOR_TEST);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                m_outline_active = false;
            }

            void begin_shadow_pass() override {
                if (m_shadow_pass_active) {
                    return;
                }
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_shadow_draw_target);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_shadow_read_target);
                if (!ensure_shadow_target(fb_w_, fb_h_)) {
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shadow_draw_target);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_shadow_read_target);
                    return;
                }
                // Test the mask against the same receivers as the scene, without
                // letting projected triangles occlude one another or later geometry.
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_shadow_draw_target);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shadow_fbo);
                glBlitFramebuffer(
                    0, 0, fb_w_, fb_h_, 0, 0, fb_w_, fb_h_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                const GLfloat clear[4] = {};
                glClearBufferfv(GL_COLOR, 0, clear);
                m_shadow_pass_active = true;
            }

            void end_shadow_pass() override {
                if (!m_shadow_pass_active) {
                    return;
                }
                m_shadow_pass_active = false;
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shadow_draw_target);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_shadow_read_target);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(m_shadow_composite_program);
                glUniform1i(m_shadow_mask_uniform, 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_shadow_mask);
                glBindVertexArray(post_vao_);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                ++draw_other_;
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_CULL_FACE);
            }

            void draw_mesh_shadow(MeshHandle mesh,
                                  TextureHandle texture,
                                  const Mat4& model,
                                  const ShadowReceiver& receiver,
                                  float ground_y,
                                  bool alpha_test,
                                  float opacity) override {
                const auto mesh_it = meshes_.find(mesh.id);
                if (!m_shadow_pass_active || mesh_it == meshes_.end() || !shadow_program_ ||
                    opacity <= 0.0f) {
                    return;
                }
                glUseProgram(shadow_program_);
                glUniformMatrix4fv(u_shadow_vp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_shadow_model_, 1, GL_FALSE, model.m.data());
                glUniform3f(u_shadow_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform1f(u_shadow_ground_y_, ground_y);
                const bool use_height_map = static_cast<bool>(receiver);
                glUniform1i(u_shadow_height_map_, 1);
                glUniform2f(u_shadow_receiver_origin_, receiver.origin_x, receiver.origin_z);
                glUniform1f(u_shadow_receiver_size_, use_height_map ? receiver.world_size : 1.0f);
                glUniform1i(u_shadow_use_height_map_, use_height_map ? 1 : 0);
                glUniform1i(u_shadow_tex_, 0);
                glUniform1f(u_shadow_alpha_ref_, alpha_test ? (96.0f / 255.0f) : 0.0f);
                glUniform1f(u_shadow_opacity_, std::clamp(opacity, 0.0f, 1.0f));
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                if (use_height_map) {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, resolve_tex(receiver.height_map));
                    glActiveTexture(GL_TEXTURE0);
                }
                glEnable(GL_BLEND);
                // Union all mesh parts, including self-overlapping triangles, in
                // an order-independent mask instead of repeatedly darkening the scene.
                glBlendEquation(GL_MAX);
                glDepthMask(GL_FALSE);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glDisable(GL_CULL_FACE);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(-4.0f, -64.0f);
                glBindVertexArray(mesh_it->second.vao);
                glDrawElements(GL_TRIANGLES, mesh_it->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_other_;
                glDisable(GL_POLYGON_OFFSET_FILL);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
                if (use_height_map) {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glActiveTexture(GL_TEXTURE0);
                }
            }

            void draw_sprite_batch(const std::vector<SpriteBatchVertex>& vertices,
                                   TextureHandle texture,
                                   bool alpha_blend,
                                   bool additive) override {
                if (vertices.empty() || vertices.size() % 3 != 0 || !texture ||
                    !sprite_batch_program_ || !sprite_batch_vao_ || !sprite_batch_vbo_) {
                    return;
                }
                glUseProgram(sprite_batch_program_);
                glUniformMatrix4fv(u_sprite_batch_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniform3f(u_sprite_batch_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform1i(u_sprite_batch_tex_, 0);
                if (additive && alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    glDepthMask(GL_FALSE);
                } else if (additive) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                    glDepthMask(GL_FALSE);
                } else if (alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                } else {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                }
                glDisable(GL_CULL_FACE);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(sprite_batch_vao_);
                glBindBuffer(GL_ARRAY_BUFFER, sprite_batch_vbo_);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(vertices.size() * sizeof(SpriteBatchVertex)),
                             vertices.data(),
                             GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
                glBindVertexArray(0);
                ++draw_mesh_;
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
            }

            void draw_mesh_lightmapped(MeshHandle mesh,
                                       TextureHandle texture,
                                       TextureHandle lightmap,
                                       const Mat4& model,
                                       bool alpha_blend,
                                       bool two_sided,
                                       bool alpha_test,
                                       float uv_u,
                                       float uv_v,
                                       float mul_r,
                                       float mul_g,
                                       float mul_b,
                                       float mul_a) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end()) {
                    return;
                }
                glUseProgram(lm_program_);
                glUniformMatrix4fv(u_lm_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_lm_model_, 1, GL_FALSE, model.m.data());
                glUniform3f(u_lm_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform2f(u_lm_uv_offset_, uv_u, uv_v);
                glUniform4f(u_lm_color_mul_, mul_r, mul_g, mul_b, mul_a);
                glUniform1i(u_lm_tex_, 0);
                glUniform1i(u_lm_lightmap_, 1);
                const float alpha_ref = (!alpha_blend && alpha_test) ? (128.0f / 255.0f) : 0.0f;
                glUniform1f(u_lm_alpha_ref_, alpha_ref);
                if (alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                } else {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                }
                if (two_sided) {
                    glDisable(GL_CULL_FACE);
                } else {
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CW);
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(lightmap));
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, 0);
                glActiveTexture(GL_TEXTURE0);
                ++draw_mesh_;
                glUniform1f(u_lm_alpha_ref_, 0.0f);
                glFrontFace(GL_CW);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
            }

            void draw_water_plane(MeshHandle mesh,
                                  TextureHandle texture,
                                  const Mat4& model,
                                  const Vec3& cam_eye,
                                  float time_sec) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end() || !water_program_) {
                    return;
                }
                glUseProgram(water_program_);
                glUniformMatrix4fv(u_water_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_water_model_, 1, GL_FALSE, model.m.data());
                glUniform2f(u_water_uv_, 0.0f, 0.0f);
                glUniform1i(u_water_tex_, 0);
                glUniform3f(u_water_cam_, cam_eye.x, cam_eye.y, cam_eye.z);
                // Match begin_frame clear sky (0.45, 0.62, 0.85).
                glUniform3f(u_water_sky_, 0.55f, 0.72f, 0.95f);
                glUniform3f(u_water_deep_, 0.55f, 0.70f, 0.80f);
                glUniform1f(u_water_time_, time_sec);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_other_;
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
            }

            void draw_sky_face(MeshHandle mesh, TextureHandle texture, const Mat4& model) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end()) {
                    return;
                }
                glUseProgram(program_);
                glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_model_, 1, GL_FALSE, model.m.data());
                glUniform3f(u_cam_pos_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform2f(u_uv_offset_, 0.0f, 0.0f);
                glUniform4f(u_color_mul_, 1.0f, 1.0f, 1.0f, 1.0f);
                glUniform1f(m_specular_power_uniform, 0.0f);
                glUniform1i(u_tex_, 0);
                glUniform1f(u_alpha_ref_, 0.0f);
                glDisable(GL_BLEND);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_other_;
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
            }

            void draw_terrain_layer(MeshHandle mesh, const TerrainLayerDraw& layer) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end()) {
                    return;
                }
                glUseProgram(terrain_program_);
                glUniformMatrix4fv(u_terrain_vp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniform3f(u_terrain_cam_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform1i(u_terrain_color_, 0);
                glUniform1i(u_terrain_detail_, 1);
                glUniform1f(u_terrain_repeat_, layer.repeat);
                glUniform3f(u_terrain_sun_, layer.sun_r, layer.sun_g, layer.sun_b);

                if (layer.alpha_blend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    glDepthFunc(GL_LEQUAL);
                } else {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                    glDepthFunc(GL_LEQUAL);
                }

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(layer.color));
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(layer.detail));

                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_terrain_;

                if (layer.alpha_blend) {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                    glDepthFunc(GL_LEQUAL);
                }
            }

            void
            draw_mesh_lit(MeshHandle mesh, const Mat4& model, float r, float g, float b) override {
                auto mit = meshes_.find(mesh.id);
                if (mit == meshes_.end()) {
                    return;
                }
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LEQUAL);
                glUseProgram(lit_program_);
                glUniformMatrix4fv(u_lit_mvp_, 1, GL_FALSE, view_rot_proj_.m.data());
                glUniformMatrix4fv(u_lit_model_, 1, GL_FALSE, model.m.data());
                glUniform3f(u_lit_cam_, cam_pos_.x, cam_pos_.y, cam_pos_.z);
                glUniform3f(u_lit_color_, r, g, b);
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_other_;
            }

            void set_ui_clip_circle(float cx, float cy, float radius) override {
                m_ui_clip_x = cx;
                m_ui_clip_y = cy;
                m_ui_clip_radius = std::max(0.0f, radius);
            }

            void draw_ui_quad(TextureHandle texture,
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
                              float v1 = 1.0f) override {
                draw_ui_internal(texture, x, y, w, h, r, g, b, a, u0, v0, u1, v1);
            }

            void draw_ui_quad_rotated(TextureHandle texture,
                                      float x,
                                      float y,
                                      float w,
                                      float h,
                                      float rotation_rad,
                                      float r = 1,
                                      float g = 1,
                                      float b = 1,
                                      float a = 1) override {
                draw_ui_internal(
                    texture, x, y, w, h, r, g, b, a, 0.0f, 0.0f, 1.0f, 1.0f, rotation_rad);
            }

            void draw_ui_rect(
                float x, float y, float w, float h, float r, float g, float b, float a) override {
                draw_ui_internal(white_tex_, x, y, w, h, r, g, b, a, 0.0f, 0.0f, 1.0f, 1.0f);
            }

            void draw_ui_triangle(float x0,
                                  float y0,
                                  float x1,
                                  float y1,
                                  float x2,
                                  float y2,
                                  float r,
                                  float g,
                                  float b,
                                  float a) override {
                if (!ui_tri_ || fb_w_ <= 0 || fb_h_ <= 0 || a <= 0.0f) {
                    return;
                }
                auto mit = meshes_.find(ui_tri_.id);
                if (mit == meshes_.end()) {
                    return;
                }
                Mat4 model{};
                model.m = {
                    x1 - x0, y1 - y0, 0, 0, x2 - x0, y2 - y0, 0, 0, 0, 0, 1, 0, x0, y0, 0, 1};
                const Mat4 proj = Mat4::orthographic(
                    0, static_cast<float>(fb_w_), static_cast<float>(fb_h_), 0, -1, 1);
                const Mat4 mvp = Mat4::multiply(proj, model);

                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(ui_program_);
                bind_ui_depth();
                glUniformMatrix4fv(u_ui_mvp_, 1, GL_FALSE, mvp.m.data());
                glUniform1i(u_ui_tex_, 0);
                glUniform4f(u_ui_color_, r, g, b, a);
                glUniform4f(u_ui_uv_rect_, 0.0f, 0.0f, 1.0f, 1.0f);
                glUniform3f(m_ui_clip_uniform,
                            m_ui_clip_x,
                            static_cast<float>(fb_h_) - m_ui_clip_y,
                            m_ui_clip_radius);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(white_tex_));
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_ui_;
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
                glDisable(GL_BLEND);
            }

            void end_frame() override {
                compose_scene();
                const auto busy_end = std::chrono::steady_clock::now();
                const float busy_ms =
                    std::chrono::duration<float, std::milli>(busy_end - frame_start_).count();
                ++fps_frames_;
                const float window_s =
                    std::chrono::duration<float>(busy_end - fps_window_start_).count();
                if (window_s >= 1.0f) {
                    published_.fps = static_cast<float>(fps_frames_) / window_s;
                    fps_frames_ = 0;
                    fps_window_start_ = busy_end;
                }
                published_.busy_ms = busy_ms;
                published_.mesh = draw_mesh_;
                published_.terrain = draw_terrain_;
                published_.ui = draw_ui_;
                published_.other = draw_other_;

                const auto present_start = std::chrono::steady_clock::now();
                glfwSwapBuffers(window_);
                const auto present_end = std::chrono::steady_clock::now();
                published_.present_ms =
                    std::chrono::duration<float, std::milli>(present_end - present_start).count();
                // Target 60 FPS. glfwSwapInterval(1) already waits on vsync when the driver
                // honors it; sleep only the remainder so we never busy-spin if vsync is
                // off, and never add a second wait on top of a successful vsync (that
                // dropped FPS).
                constexpr auto kFrameBudget = std::chrono::nanoseconds(16'666'667);  // 1/60 s
                const auto target = frame_start_ + kFrameBudget;
                const auto now = present_end;
                if (now < target) {
                    std::this_thread::sleep_until(target);
                }
            }

          private:
            // Copy once, entirely on the GPU. Bloom may have moved color to another target.
            void capture_ui_depth() {
                GLint previous_read = 0;
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_ui_depth_source));
                glActiveTexture(GL_TEXTURE2);
                if (!m_ui_scene_depth) {
                    glGenTextures(1, &m_ui_scene_depth);
                }
                glBindTexture(GL_TEXTURE_2D, m_ui_scene_depth);
                if (m_ui_depth_width != fb_w_ || m_ui_depth_height != fb_h_) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexImage2D(GL_TEXTURE_2D,
                                 0,
                                 GL_DEPTH_COMPONENT24,
                                 fb_w_,
                                 fb_h_,
                                 0,
                                 GL_DEPTH_COMPONENT,
                                 GL_UNSIGNED_INT,
                                 nullptr);
                    m_ui_depth_width = fb_w_;
                    m_ui_depth_height = fb_h_;
                }
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fb_w_, fb_h_);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read));
                glActiveTexture(GL_TEXTURE0);
                m_ui_depth_captured = true;
            }

            void bind_ui_depth() {
                glUniform1f(m_ui_depth_uniform, m_ui_world_depth);
                glUniform1i(m_ui_depth_sampler, 2);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, m_ui_scene_depth);
                glActiveTexture(GL_TEXTURE0);
            }

            void draw_ui_internal(TextureHandle texture,
                                  float x,
                                  float y,
                                  float w,
                                  float h,
                                  float r,
                                  float g,
                                  float b,
                                  float a,
                                  float u0,
                                  float v0,
                                  float u1,
                                  float v1,
                                  float rotation_rad = 0.0f) {
                if (!ui_quad_ || fb_w_ <= 0 || fb_h_ <= 0 || w == 0.0f || h == 0.0f) {
                    return;
                }
                auto mit = meshes_.find(ui_quad_.id);
                if (mit == meshes_.end()) {
                    return;
                }
                // Top-left origin ?? OpenGL NDC via ortho (y down).
                const Mat4 proj = Mat4::orthographic(
                    0, static_cast<float>(fb_w_), static_cast<float>(fb_h_), 0, -1, 1);
                Mat4 model = Mat4::scaling(w, h, 1);
                if (rotation_rad != 0.0f) {
                    Mat4 rotation = Mat4::identity();
                    const float cosine = std::cos(rotation_rad);
                    const float sine = std::sin(rotation_rad);
                    rotation.m[0] = cosine;
                    rotation.m[1] = sine;
                    rotation.m[4] = -sine;
                    rotation.m[5] = cosine;
                    model = Mat4::multiply(model, Mat4::translation(-0.5f, -0.5f, 0.0f));
                    model = Mat4::multiply(rotation, model);
                    model =
                        Mat4::multiply(Mat4::translation(x + w * 0.5f, y + h * 0.5f, 0.0f), model);
                } else {
                    model = Mat4::multiply(Mat4::translation(x, y, 0), model);
                }
                const Mat4 mvp = Mat4::multiply(proj, model);

                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(ui_program_);
                bind_ui_depth();
                glUniformMatrix4fv(u_ui_mvp_, 1, GL_FALSE, mvp.m.data());
                glUniform1i(u_ui_tex_, 0);
                glUniform4f(u_ui_color_, r, g, b, a);
                glUniform4f(u_ui_uv_rect_, u0, v0, u1 - u0, v1 - v0);
                glUniform3f(m_ui_clip_uniform,
                            m_ui_clip_x,
                            static_cast<float>(fb_h_) - m_ui_clip_y,
                            m_ui_clip_radius);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolve_tex(texture));
                glBindVertexArray(mit->second.vao);
                glDrawElements(GL_TRIANGLES, mit->second.index_count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++draw_ui_;
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
                glDisable(GL_BLEND);
            }

            GLuint resolve_tex(TextureHandle texture) {
                auto tit = textures_.find(texture.id);
                if (tit != textures_.end()) {
                    return tit->second.id;
                }
                if (white_tex_) {
                    return textures_[white_tex_.id].id;
                }
                return 0;
            }

            static void cursor_cb(GLFWwindow* window, double x, double y) {
                auto* self = static_cast<GlEngine*>(glfwGetWindowUserPointer(window));
                if (!self) {
                    return;
                }
                self->mouse_dx_ += static_cast<float>(x - self->last_mx_);
                self->mouse_dy_ += static_cast<float>(y - self->last_my_);
                self->last_mx_ = x;
                self->last_my_ = y;
            }

            static void scroll_cb(GLFWwindow* window, double /*xoffset*/, double yoffset) {
                auto* self = static_cast<GlEngine*>(glfwGetWindowUserPointer(window));
                if (!self) {
                    return;
                }
                self->mouse_scroll_ += static_cast<float>(yoffset);
            }

            static void
            key_cb(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
                auto* self = static_cast<GlEngine*>(glfwGetWindowUserPointer(window));
                if (self && key == GLFW_KEY_BACKSPACE &&
                    (action == GLFW_PRESS || action == GLFW_REPEAT)) {
                    // The OS supplies its configured initial delay and repeat interval.
                    self->m_backspace_action = true;
                }
            }

            static void char_cb(GLFWwindow* window, unsigned int codepoint) {
                auto* self = static_cast<GlEngine*>(glfwGetWindowUserPointer(window));
                if (!self) {
                    return;
                }
                if (codepoint >= 32 && codepoint < 127) {
                    self->pending_chars_.push_back(static_cast<char>(codepoint));
                }
            }

            static void drop_cb(GLFWwindow* window, int count, const char** paths) {
                auto* self = static_cast<GlEngine*>(glfwGetWindowUserPointer(window));
                if (!self || !paths || count <= 0) {
                    return;
                }
                for (int i = 0; i < count; ++i) {
                    if (paths[i] && paths[i][0] != '\0') {
                        self->pending_drops_.emplace_back(paths[i]);
                    }
                }
            }

            static Mat4 mul(const Mat4& a, const Mat4& b) {
                Mat4 r{};
                for (int col = 0; col < 4; ++col) {
                    for (int row = 0; row < 4; ++row) {
                        r.m[col * 4 + row] = a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                                             a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                                             a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                                             a.m[3 * 4 + row] * b.m[col * 4 + 3];
                    }
                }
                return r;
            }

            void sort_surface_indices(const GpuMesh& mesh, const Mat4& model) {
                if (mesh.m_surface_indices.size() % 3 != 0) {
                    return;
                }
                const auto camera_model = Mat4::multiply(m_mesh_view, model);
                std::vector<float> depths;
                depths.reserve(mesh.m_surface_vertices.size());
                for (const auto& vertex : mesh.m_surface_vertices) {
                    const auto p =
                        Mat4::transform_point(camera_model, vertex.px, vertex.py, vertex.pz);
                    if (!std::isfinite(p.z)) {
                        return;
                    }
                    depths.push_back(p.z);
                }
                std::vector<std::pair<float, std::size_t>> triangles;
                for (std::size_t i = 0; i < mesh.m_surface_indices.size(); i += 3) {
                    float depth = 0.0f;
                    for (std::size_t j = 0; j < 3; ++j) {
                        const auto index = mesh.m_surface_indices[i + j];
                        if (index >= depths.size()) {
                            return;
                        }
                        depth += depths[index] / 3.0f;
                    }
                    if (!std::isfinite(depth)) {
                        return;
                    }
                    triangles.emplace_back(depth, i);
                }
                std::stable_sort(triangles.begin(),
                                 triangles.end(),
                                 [](const auto& a, const auto& b) { return a.first < b.first; });
                std::vector<std::uint32_t> ordered;
                ordered.reserve(mesh.m_surface_indices.size());
                for (const auto& triangle : triangles) {
                    for (std::size_t j = 0; j < 3; ++j) {
                        ordered.push_back(mesh.m_surface_indices[triangle.second + j]);
                    }
                }
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(ordered.size() * sizeof(std::uint32_t)),
                             ordered.data(),
                             GL_STREAM_DRAW);
            }

            static void destroy_gpu_mesh(GpuMesh& mesh) {
                if (mesh.m_normal_buffer) {
                    glDeleteBuffers(1, &mesh.m_normal_buffer);
                }
                if (mesh.ebo) {
                    glDeleteBuffers(1, &mesh.ebo);
                }
                if (mesh.vbo) {
                    glDeleteBuffers(1, &mesh.vbo);
                }
                if (mesh.vao) {
                    glDeleteVertexArrays(1, &mesh.vao);
                }
                mesh = {};
            }

            std::array<GLint, 4> outline_screen_bounds(const Mat4& model,
                                                       const Vec3& bounds_min,
                                                       const Vec3& bounds_max,
                                                       float radius) const {
                float left = 1.0f;
                float bottom = 1.0f;
                float right = -1.0f;
                float top = -1.0f;
                for (int corner = 0; corner < 8; ++corner) {
                    Vec3 p = Mat4::transform_point(model,
                                                   corner & 1 ? bounds_max.x : bounds_min.x,
                                                   corner & 2 ? bounds_max.y : bounds_min.y,
                                                   corner & 4 ? bounds_max.z : bounds_min.z);
                    p.x -= cam_pos_.x;
                    p.y -= cam_pos_.y;
                    p.z -= cam_pos_.z;
                    const auto& vp = view_rot_proj_.m;
                    const float w = vp[3] * p.x + vp[7] * p.y + vp[11] * p.z + vp[15];
                    if (w <= 0.0f) {
                        // A box crossing the camera plane is not bounded by its projected corners.
                        return {0, 0, fb_w_, fb_h_};
                    }
                    const float x = (vp[0] * p.x + vp[4] * p.y + vp[8] * p.z + vp[12]) / w;
                    const float y = (vp[1] * p.x + vp[5] * p.y + vp[9] * p.z + vp[13]) / w;
                    left = std::min(left, x);
                    bottom = std::min(bottom, y);
                    right = std::max(right, x);
                    top = std::max(top, y);
                }
                const float pad = radius + 2.0f;
                const auto pixel = [](float ndc, float padding, int size) {
                    return std::clamp(
                        (ndc + 1.0f) * 0.5f * size + padding, 0.0f, static_cast<float>(size));
                };
                return {static_cast<GLint>(std::floor(pixel(left, -pad, fb_w_))),
                        static_cast<GLint>(std::floor(pixel(bottom, -pad, fb_h_))),
                        static_cast<GLint>(std::ceil(pixel(right, pad, fb_w_))),
                        static_cast<GLint>(std::ceil(pixel(top, pad, fb_h_)))};
            }

            bool ensure_outline_target() {
                if (m_outline_fbo && m_outline_width == fb_w_ && m_outline_height == fb_h_) {
                    return true;
                }
                destroy_outline_target();
                glGenTextures(1, &m_outline_mask);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_outline_mask);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, fb_w_, fb_h_, 0, GL_RED, GL_FLOAT, nullptr);
                glGenRenderbuffers(1, &m_outline_depth);
                glBindRenderbuffer(GL_RENDERBUFFER, m_outline_depth);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, fb_w_, fb_h_);
                glGenFramebuffers(1, &m_outline_fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outline_fbo);
                glFramebufferTexture2D(
                    GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outline_mask, 0);
                glFramebufferRenderbuffer(
                    GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_outline_depth);
                if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    std::fprintf(stderr, "Failed to create the mesh outline framebuffer\n");
                    destroy_outline_target();
                    return false;
                }
                m_outline_width = fb_w_;
                m_outline_height = fb_h_;
                return true;
            }

            void destroy_outline_target() {
                if (m_outline_fbo) {
                    glDeleteFramebuffers(1, &m_outline_fbo);
                    glDeleteTextures(1, &m_outline_mask);
                    glDeleteRenderbuffers(1, &m_outline_depth);
                    m_outline_fbo = 0;
                    m_outline_mask = 0;
                    m_outline_depth = 0;
                }
            }

            bool ensure_shadow_target(int width, int height) {
                if (width <= 0 || height <= 0 || !shadow_program_ || !m_shadow_composite_program ||
                    !post_vao_) {
                    return false;
                }
                if (m_shadow_fbo && m_shadow_width == width && m_shadow_height == height) {
                    return true;
                }
                destroy_shadow_target();
                glGenTextures(1, &m_shadow_mask);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_shadow_mask);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
                glGenRenderbuffers(1, &m_shadow_depth);
                glBindRenderbuffer(GL_RENDERBUFFER, m_shadow_depth);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
                glGenFramebuffers(1, &m_shadow_fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, m_shadow_fbo);
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_shadow_mask, 0);
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_shadow_depth);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    std::fprintf(stderr, "Failed to create the shadow mask framebuffer\n");
                    destroy_shadow_target();
                    return false;
                }
                m_shadow_width = width;
                m_shadow_height = height;
                return true;
            }

            void destroy_shadow_target() {
                if (m_shadow_fbo) {
                    glDeleteFramebuffers(1, &m_shadow_fbo);
                    m_shadow_fbo = 0;
                }
                if (m_shadow_mask) {
                    glDeleteTextures(1, &m_shadow_mask);
                    m_shadow_mask = 0;
                }
                if (m_shadow_depth) {
                    glDeleteRenderbuffers(1, &m_shadow_depth);
                    m_shadow_depth = 0;
                }
                m_shadow_width = 0;
                m_shadow_height = 0;
            }

            void destroy_offscreen() {
                if (offscreen_fbo_) {
                    glDeleteFramebuffers(1, &offscreen_fbo_);
                    offscreen_fbo_ = 0;
                }
                if (offscreen_color_) {
                    glDeleteTextures(1, &offscreen_color_);
                    offscreen_color_ = 0;
                }
                if (offscreen_depth_) {
                    glDeleteRenderbuffers(1, &offscreen_depth_);
                    offscreen_depth_ = 0;
                }
                offscreen_w_ = 0;
                offscreen_h_ = 0;
            }

            bool ensure_scene_target(int width, int height) {
                if (width <= 0 || height <= 0 || !bloom_program_ || !bright_pass_program_ ||
                    !blur_program_ || !post_vao_) {
                    return false;
                }
                if (scene_fbo_ && scene_w_ == width && scene_h_ == height) {
                    return true;
                }
                destroy_scene_target();
                scene_w_ = width;
                scene_h_ = height;
                glGenTextures(1, &scene_color_);
                glBindTexture(GL_TEXTURE_2D, scene_color_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA8,
                             width,
                             height,
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             nullptr);
                glGenRenderbuffers(1, &scene_depth_);
                glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
                glGenFramebuffers(1, &scene_fbo_);
                glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_color_, 0);
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, scene_depth_);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    destroy_scene_target();
                    return false;
                }

                bloom_w_ = std::max(1, width / 4);
                bloom_h_ = std::max(1, height / 4);
                glGenTextures(2, bloom_color_);
                glGenFramebuffers(2, bloom_fbo_);
                for (int i = 0; i < 2; ++i) {
                    glBindTexture(GL_TEXTURE_2D, bloom_color_[i]);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexImage2D(GL_TEXTURE_2D,
                                 0,
                                 GL_RGBA16F,
                                 bloom_w_,
                                 bloom_h_,
                                 0,
                                 GL_RGBA,
                                 GL_FLOAT,
                                 nullptr);
                    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_[i]);
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_color_[i], 0);
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                        destroy_scene_target();
                        return false;
                    }
                }
                return true;
            }

            void compose_scene() {
                if (!scene_pending_) {
                    return;
                }
                scene_pending_ = false;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, fb_w_, fb_h_);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glDisable(GL_BLEND);

                glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_[0]);
                glViewport(0, 0, bloom_w_, bloom_h_);
                glUseProgram(bright_pass_program_);
                glUniform1i(u_bright_scene_, 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, scene_color_);
                glBindVertexArray(post_vao_);
                glDrawArrays(GL_TRIANGLES, 0, 3);

                int source = 0;
                for (int pass = 0; pass < 6; ++pass) {
                    const int destination = 1 - source;
                    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_[destination]);
                    glUseProgram(blur_program_);
                    glUniform1i(u_blur_image_, 0);
                    glUniform2f(u_blur_direction_,
                                pass % 2 == 0 ? 1.0f : 0.0f,
                                pass % 2 == 0 ? 0.0f : 1.0f);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, bloom_color_[source]);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                    source = destination;
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, fb_w_, fb_h_);
                glUseProgram(bloom_program_);
                glUniform1i(u_bloom_scene_, 0);
                glUniform1i(u_bloom_blur_, 1);
                glUniform1f(u_bloom_strength_, bloom_strength_);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, scene_color_);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, bloom_color_[source]);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                ++draw_other_;
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CW);
            }

            void destroy_scene_target() {
                scene_pending_ = false;
                if (scene_fbo_) {
                    glDeleteFramebuffers(1, &scene_fbo_);
                    scene_fbo_ = 0;
                }
                if (scene_color_) {
                    glDeleteTextures(1, &scene_color_);
                    scene_color_ = 0;
                }
                if (scene_depth_) {
                    glDeleteRenderbuffers(1, &scene_depth_);
                    scene_depth_ = 0;
                }
                glDeleteFramebuffers(2, bloom_fbo_);
                glDeleteTextures(2, bloom_color_);
                bloom_fbo_[0] = bloom_fbo_[1] = 0;
                bloom_color_[0] = bloom_color_[1] = 0;
                scene_w_ = 0;
                scene_h_ = 0;
                bloom_w_ = 0;
                bloom_h_ = 0;
            }

            GLFWwindow* window_ = nullptr;
            WindowMode window_mode_ = WindowMode::Windowed;
            int windowed_x_ = 100;
            int windowed_y_ = 100;
            int windowed_w_ = 1920;
            int windowed_h_ = 1080;
            GLuint offscreen_fbo_ = 0;
            GLuint offscreen_color_ = 0;
            GLuint offscreen_depth_ = 0;
            int offscreen_w_ = 0;
            int offscreen_h_ = 0;
            GLuint scene_fbo_ = 0;
            GLuint scene_color_ = 0;
            GLuint scene_depth_ = 0;
            int scene_w_ = 0;
            int scene_h_ = 0;
            GLuint bloom_fbo_[2]{};
            GLuint bloom_color_[2]{};
            int bloom_w_ = 0;
            int bloom_h_ = 0;
            bool scene_pending_ = false;
            float bloom_strength_ = 0.0f;
            GLuint program_ = 0;
            GLint u_mvp_ = -1;
            GLint u_model_ = -1;
            GLint u_cam_pos_ = -1;
            GLint u_uv_offset_ = -1;
            GLint u_tex_ = -1;
            GLint u_alpha_ref_ = -1;
            GLint u_color_mul_ = -1;
            GLint m_white_mix_uniform = -1;
            GLint m_specular_power_uniform = -1;
            GLint m_instance_specular_uniform = -1;
            Mat4 m_mesh_view;
            GLuint mesh_instance_program_ = 0;
            GLint u_mesh_instance_mvp_ = -1;
            GLint u_mesh_instance_cam_pos_ = -1;
            GLint u_mesh_instance_tex_ = -1;
            GLint u_mesh_instance_alpha_ref_ = -1;
            GLint u_mesh_instance_color_mul_ = -1;
            GLuint mesh_instance_vbo_ = 0;
            GLuint sprite_batch_program_ = 0;
            GLint u_sprite_batch_mvp_ = -1;
            GLint u_sprite_batch_cam_pos_ = -1;
            GLint u_sprite_batch_tex_ = -1;
            GLuint sprite_batch_vao_ = 0;
            GLuint sprite_batch_vbo_ = 0;
            GLuint lm_program_ = 0;
            GLint u_lm_mvp_ = -1;
            GLint u_lm_model_ = -1;
            GLint u_lm_cam_pos_ = -1;
            GLint u_lm_uv_offset_ = -1;
            GLint u_lm_tex_ = -1;
            GLint u_lm_lightmap_ = -1;
            GLint u_lm_alpha_ref_ = -1;
            GLint u_lm_color_mul_ = -1;
            GLuint water_program_ = 0;
            GLint u_water_mvp_ = -1;
            GLint u_water_model_ = -1;
            GLint u_water_uv_ = -1;
            GLint u_water_tex_ = -1;
            GLint u_water_cam_ = -1;
            GLint u_water_sky_ = -1;
            GLint u_water_deep_ = -1;
            GLint u_water_time_ = -1;
            GLuint ui_program_ = 0;
            GLint u_ui_mvp_ = -1;
            GLint u_ui_tex_ = -1;
            GLint u_ui_color_ = -1;
            GLint u_ui_uv_rect_ = -1;
            /** Scene depth snapshot shared by all world labels in the current UI pass. */
            GLuint m_ui_scene_depth = 0;
            GLint m_ui_depth_sampler = -1;
            GLint m_ui_depth_uniform = -1;
            GLint m_ui_depth_source = 0;
            int m_ui_depth_width = 0;
            int m_ui_depth_height = 0;
            bool m_ui_depth_captured = false;
            bool m_ui_pass_started = false;
            float m_ui_world_depth = -1.0f;
            GLint m_ui_clip_uniform = -1;
            float m_ui_clip_x = 0.0f;
            float m_ui_clip_y = 0.0f;
            float m_ui_clip_radius = 0.0f;
            GLuint bloom_program_ = 0;
            GLuint bright_pass_program_ = 0;
            GLuint blur_program_ = 0;
            GLint u_bright_scene_ = -1;
            GLint u_blur_image_ = -1;
            GLint u_blur_direction_ = -1;
            GLint u_bloom_scene_ = -1;
            GLint u_bloom_blur_ = -1;
            GLint u_bloom_strength_ = -1;
            GLuint post_vao_ = 0;
            GLuint m_outline_mask_program = 0;
            GLint m_outline_mvp_uniform = -1;
            GLint m_outline_model_uniform = -1;
            GLint m_outline_camera_uniform = -1;
            GLint m_outline_alpha_uniform = -1;
            GLuint m_outline_program = 0;
            GLint m_outline_bounds_uniform = -1;
            GLint m_outline_radius_uniform = -1;
            GLint m_outline_opacity_uniform = -1;
            GLuint m_outline_fbo = 0;
            GLuint m_outline_mask = 0;
            GLuint m_outline_depth = 0;
            GLint m_outline_draw_target = 0;
            int m_outline_width = 0;
            int m_outline_height = 0;
            bool m_outline_active = false;
            GLuint shadow_program_ = 0;
            GLuint m_shadow_composite_program = 0;
            GLint m_shadow_mask_uniform = -1;
            GLuint m_shadow_fbo = 0;
            GLuint m_shadow_mask = 0;
            GLuint m_shadow_depth = 0;
            GLint m_shadow_draw_target = 0;
            GLint m_shadow_read_target = 0;
            int m_shadow_width = 0;
            int m_shadow_height = 0;
            bool m_shadow_pass_active = false;
            GLint u_shadow_vp_ = -1;
            GLint u_shadow_model_ = -1;
            GLint u_shadow_cam_pos_ = -1;
            GLint u_shadow_ground_y_ = -1;
            GLint u_shadow_height_map_ = -1;
            GLint u_shadow_receiver_origin_ = -1;
            GLint u_shadow_receiver_size_ = -1;
            GLint u_shadow_use_height_map_ = -1;
            GLint u_shadow_tex_ = -1;
            GLint u_shadow_alpha_ref_ = -1;
            GLint u_shadow_opacity_ = -1;
            GLuint terrain_program_ = 0;
            GLint u_terrain_vp_ = -1;
            GLint u_terrain_cam_ = -1;
            GLint u_terrain_color_ = -1;
            GLint u_terrain_detail_ = -1;
            GLint u_terrain_repeat_ = -1;
            GLint u_terrain_sun_ = -1;
            GLuint lit_program_ = 0;
            GLint u_lit_mvp_ = -1;
            GLint u_lit_model_ = -1;
            GLint u_lit_cam_ = -1;
            GLint u_lit_color_ = -1;
            Mat4 view_rot_proj_{};
            Vec3 cam_pos_{};
            std::unordered_map<std::uint32_t, GpuMesh> meshes_;
            std::unordered_map<std::uint32_t, GpuTexture> textures_;
            TextureHandle white_tex_{};
            MeshHandle ui_quad_{};
            MeshHandle ui_tri_{};
            std::uint32_t next_mesh_ = 0;
            std::uint32_t next_tex_ = 0;
            double last_mx_ = 0;
            double last_my_ = 0;
            float mouse_dx_ = 0;
            float mouse_dy_ = 0;
            float mouse_scroll_ = 0;
            std::vector<std::string> pending_drops_;
            int fb_w_ = 0;
            int fb_h_ = 0;
            CursorMode cursor_mode_ = CursorMode::Free;
            bool prev_mouse_left_ = false;
            bool prev_mouse_right_ = false;
            bool prev_mouse_middle_ = false;
            bool prev_escape_ = false;
            bool prev_enter_ = false;
            bool m_prev_tab = false;
            bool prev_backspace_ = false;
            /** Text-edit press/repeat events waiting for the next frame. */
            bool m_backspace_action = false;
            bool prev_delete_ = false;
            bool prev_i_ = false;
            bool prev_e_ = false;
            bool prev_k_ = false;
            bool prev_p_ = false;
            bool prev_o_ = false;
            bool prev_g_ = false;
            bool prev_q_ = false;
            bool prev_f_ = false;
            bool prev_s_ = false;
            bool prev_z_ = false;
            bool prev_x_ = false;
            bool prev_h_ = false;
            bool prev_c_ = false;
            bool prev_v_ = false;
            bool prev_l_ = false;
            bool prev_m_ = false;
            bool prev_r_ = false;
            bool prev_space_ = false;
            bool prev_digit_[10] = {};
            int key_letter_z_ = GLFW_KEY_Z;
            int key_letter_x_ = GLFW_KEY_X;
            int key_letter_q_ = GLFW_KEY_Q;
            std::vector<char> pending_chars_;
            std::chrono::steady_clock::time_point last_time_{};
            std::chrono::steady_clock::time_point frame_start_{};
            int draw_mesh_ = 0;
            int draw_terrain_ = 0;
            int draw_ui_ = 0;
            int draw_other_ = 0;
            FrameDrawStats published_{};
            int fps_frames_ = 0;
            std::chrono::steady_clock::time_point fps_window_start_{
                std::chrono::steady_clock::now()};
        };

    }  // namespace

    IEngine* create_engine() {
        return new GlEngine();
    }

}  // namespace openkal::engine
