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
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

namespace {

    void draw_checkerboard(openkal::engine::IEngine& engine, int width, int height, int cell) {
        const float c = static_cast<float>(std::max(8, cell));
        const int cols = static_cast<int>(std::ceil(static_cast<float>(width) / c));
        const int rows = static_cast<int>(std::ceil(static_cast<float>(height) / c));
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const bool dark = ((x + y) & 1) != 0;
                const float shade = dark ? 0.22f : 0.30f;
                engine.draw_ui_rect(static_cast<float>(x) * c,
                                    static_cast<float>(y) * c,
                                    c,
                                    c,
                                    shade,
                                    shade,
                                    shade,
                                    1.0f);
            }
        }
    }

}  // namespace

int main(int argc, char** argv) {
    std::string profile_path;
    std::string path;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "-h") == 0 || std::strcmp(argv[index], "--help") == 0) {
            std::fprintf(stderr,
                         "usage: %s [openkal.profile] [--profile FILE] "
                         "<file.gtx|file.dds|data-relative path>\n",
                         argv[0]);
            return 0;
        }
        if (std::strcmp(argv[index], "--profile") == 0) {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "missing value for --profile\n");
                return 2;
            }
            profile_path = argv[++index];
        } else if (profile_path.empty() && openkal::platform::is_profile_path(argv[index])) {
            profile_path = argv[index];
        } else if (path.empty()) {
            path = argv[index];
        } else {
            std::fprintf(stderr, "unexpected argument: %s\n", argv[index]);
            return 2;
        }
    }
    if (path.empty()) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] "
                     "<file.gtx|file.dds|data-relative path>\n",
                     argv[0]);
        return 2;
    }

    std::string profile_error;
    const auto runtime = openkal::platform::RuntimeProfile::load_for_tool(
        OPENKAL_REPO_ROOT, profile_path, &profile_error);
    if (!runtime) {
        openkal::log::error(profile_error);
        return 1;
    }
    if (!openkal::platform::file_exists(path)) {
        const std::string candidate =
            openkal::platform::resolve_path_case(runtime->profile.data_root(), path);
        if (openkal::platform::file_exists(candidate)) {
            path = candidate;
        }
    }

    openkal::terrain::ImageRgba img;
    if (!openkal::terrain::load_gtx_rgba(path, img) || img.rgba.empty() || img.width <= 0 ||
        img.height <= 0) {
        openkal::log::error("failed to load GTX/DDS: " + path);
        return 1;
    }

    std::printf(
        "%s  %dx%d  rgba=%zu bytes\n", path.c_str(), img.width, img.height, img.rgba.size());

    const int win_w = std::clamp(img.width + 40, 320, 1280);
    const int win_h = std::clamp(img.height + 40, 240, 800);

    std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
    const std::string title = "gtx_view — " + path;
    if (!engine->init(title.c_str(), win_w, win_h)) {
        openkal::log::error("engine init failed (no display / GLFW?). Decode OK: " +
                            std::to_string(img.width) + "x" + std::to_string(img.height));
        return 1;
    }
    engine->set_cursor_mode(openkal::engine::CursorMode::Free);

    openkal::engine::TextureHandle tex =
        engine->create_texture_rgba8(img.rgba.data(), img.width, img.height, false);

    float zoom = std::min(static_cast<float>(win_w) / static_cast<float>(img.width),
                          static_cast<float>(win_h) / static_cast<float>(img.height)) *
                 0.92f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    bool dragging = false;
    float last_mx = 0.0f;
    float last_my = 0.0f;

    openkal::engine::FrameInput in;
    while (engine->poll(in)) {
        if (in.quit || in.key_escape_pressed) {
            break;
        }

        if (in.mouse_scroll != 0.0f) {
            const float factor = (in.mouse_scroll > 0.0f) ? 1.12f : (1.0f / 1.12f);
            zoom = std::clamp(zoom * factor, 0.05f, 64.0f);
        }

        if (in.mouse_left || in.mouse_middle) {
            if (!dragging) {
                dragging = true;
                last_mx = in.mouse_x;
                last_my = in.mouse_y;
            } else {
                pan_x += in.mouse_x - last_mx;
                pan_y += in.mouse_y - last_my;
                last_mx = in.mouse_x;
                last_my = in.mouse_y;
            }
        } else {
            dragging = false;
        }

        const float dw = static_cast<float>(img.width) * zoom;
        const float dh = static_cast<float>(img.height) * zoom;
        const float x = (static_cast<float>(in.width) - dw) * 0.5f + pan_x;
        const float y = (static_cast<float>(in.height) - dh) * 0.5f + pan_y;

        engine->begin_frame(0.12f, 0.12f, 0.14f);
        draw_checkerboard(*engine, in.width, in.height, 16);
        engine->draw_ui_quad(tex, x, y, dw, dh);
        engine->end_frame();
    }

    engine->destroy_texture(tex);
    engine->shutdown();
    return 0;
}
