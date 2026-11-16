/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/StartupScreen.h"

#include "openkal/client/ClientRuntimeConfig.h"
#include "openkal/client/ClientShutdownSignal.h"
#include "openkal/client/SettingsWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {

    StartupScreen::StartupScreen(engine::IEngine& engine) : m_engine(engine) {}

    StartupScreen::~StartupScreen() {
        m_cursor.destroy(m_engine);
        m_font.destroy(m_engine);
        m_ui.destroy();
        for (auto& [path, model] : m_models) {
            for (auto& part : model.parts) {
                if (part.mesh) {
                    m_engine.destroy_mesh(part.mesh);
                }
            }
        }
        for (const auto& [path, texture] : m_textures) {
            if (texture) {
                m_engine.destroy_texture(texture);
            }
        }
    }

    bool StartupScreen::run(const std::string& map_root,
                            ClientRuntimeConfig& runtime,
                            const UiSettings& ui_settings,
                            const ClientShutdownSignal& shutdown_signal,
                            bool wait_for_input,
                            bool* multiplayer) {
        if (multiplayer) {
            *multiplayer = false;
        }
        m_ui.init(m_engine, runtime.profile().data_root());
        const auto& configuration_root = runtime.configuration_root();
        if (!m_cursor.load(m_engine, configuration_root)) {
            log::warn("StartupScreen: cursor artwork unavailable; using fallback pointer");
        }
        UiLayout layout;
        UiDocument document;
        if (!layout.load_file(platform::join_path(configuration_root, "ui/startup.json")) ||
            !m_font.load(m_engine, FreetypeFont::find_system_sans(), 18)) {
            log::error("StartupScreen: startup layout or TrueType font unavailable");
            return false;
        }
        UiView::reload(document, &layout);
        UiVarMap vars;
        ui_fill_theme_vars(document, vars);
        vars["startup.title"] = "OpenKal";
        vars["startup.hint"] = "Your journey begins here";
        vars["startup.single_player"] = "Single Player";
        vars["startup.multiplayer"] = "Multiplayer";
        vars["startup.options"] = "Options";
        vars["startup.close"] = "Close";
        UiLayout settings_layout;
        if (!settings_layout.load_file(
                platform::join_path(configuration_root, "ui/settings.json"))) {
            log::error("StartupScreen: settings layout unavailable");
            return false;
        }
        SettingsWindow options;
        options.set_layout(&settings_layout);
        UiViewHost host;
        const std::string model_path =
            platform::resolve_client_asset(map_root, "login_map\\login_maxl", ".gb");
        const auto* model =
            WorldRenderCache::get_or_load_gb(m_engine, model_path, m_models, m_textures);
        if (!model) {
            log::error("StartupScreen: login entrance missing at " + model_path);
            return false;
        }
        // The entrance uses the gate and stair materials. The same GB also contains
        // the character room and separate sky cards, which are absent in the reference.
        std::vector<const WorldRenderCache::GpuSubmesh*> entrance;
        for (const auto& part : model->parts) {
            const auto& texture = part.uv_material.texture;
            if (texture == "login_01.DDS" || texture == "login_04.DDS") {
                entrance.push_back(&part);
            }
        }
        if (entrance.empty()) {
            log::error("StartupScreen: login model has no entrance materials");
            return false;
        }
        log::info("StartupScreen: temple entrance; Single Player/Enter continues, Options "
                  "configures, Close/Escape quits");

        // Event.dat camera 225; target angles are authored in degrees.
        constexpr float kRadians = 3.14159265f / 180.0f;
        const engine::Vec3 start_eye{769.31f, 3075.25f, -139.96f};
        const engine::Vec3 end_eye{762.0f, 3086.0f, 140.0f};
        constexpr float kStartYaw = 0.40f * kRadians;
        constexpr float kStartPitch = -6.3025355f * kRadians;
        constexpr float kEndYaw = 1.7188733f * kRadians;
        constexpr float kFadeDuration = 1.0f;  // Charsel_Info::OnPaint login fade.
        float elapsed = 0.0f;
        float transition_elapsed = 0.0f;
        bool entering = false;
        bool presented = false;
        engine::FrameInput input;
        while (m_engine.poll(input)) {
            if (input.quit || shutdown_signal.requested()) {
                return false;
            }
            const float width = static_cast<float>(input.width);
            const float height = static_cast<float>(input.height);
            const float requested_scale = ui_settings.resolve_scale(height);
            const float frame_w = document.window_w() + document.pad_left() + document.pad_right();
            const float frame_h = document.window_h() + document.pad_top() + document.pad_bottom();
            const float scale = std::min({requested_scale,
                                          std::max(1.0f, width - 24.0f) / frame_w,
                                          std::max(1.0f, height - 24.0f) / frame_h});
            const float panel_w = document.window_w() * scale;
            const float panel_h = document.window_h() * scale;
            const float panel_x = (width - panel_w) * 0.5f;
            const float panel_y =
                (height - panel_h + (document.pad_top() - document.pad_bottom()) * scale) * 0.5f;
            const UiHit hit = UiView::hit(document,
                                          &m_ui,
                                          input.mouse_x,
                                          input.mouse_y,
                                          panel_x,
                                          panel_y,
                                          panel_w,
                                          panel_h,
                                          scale,
                                          vars);
            options.set_scale(requested_scale, width, height);
            const bool options_was_open = options.open();
            if (options_was_open) {
                if (input.key_escape_pressed) {
                    options.close();
                } else if (input.mouse_left_clicked) {
                    const auto action = options.handle_click(input.mouse_x, input.mouse_y, &m_ui);
                    if (action == SettingsWindowAction::Save) {
                        std::string error;
                        if (runtime.save_client_settings(options.settings(), error)) {
                            const auto& display = runtime.client_settings().display;
                            const auto mode = display.mode == settings::DisplayMode::Borderless
                                                  ? engine::WindowMode::Borderless
                                              : display.mode == settings::DisplayMode::Maximized
                                                  ? engine::WindowMode::Maximized
                                                  : engine::WindowMode::Windowed;
                            if (m_engine.set_window_mode(mode, display.width, display.height)) {
                                options.close();
                            } else {
                                options.set_status(
                                    "Saved, but the display mode could not be applied.");
                            }
                        } else {
                            options.set_status("Save failed: " + error);
                        }
                    }
                }
            } else if (input.key_escape_pressed) {
                return false;
            } else if (!entering && presented) {
                const auto action = input.mouse_left_clicked ? hit.action() : std::string{};
                if (action == "startup.close") {
                    return false;
                }
                if (action == "startup.options") {
                    options.show(
                        runtime.client_settings(), width, height, ChatFilterModel{}, false);
                } else if (input.key_enter_pressed || action == "startup.single_player" ||
                           action == "startup.multiplayer") {
                    if (multiplayer) {
                        *multiplayer = action == "startup.multiplayer";
                    }
                    entering = true;
                }
            }
            const float dt = std::clamp(input.dt, 0.0f, 0.1f);
            elapsed += dt;
            if (entering) {
                transition_elapsed += dt;
            }
            // KCamera::FrameMove uses Catmull-Rom with duplicated endpoint keys.
            // Camera 225 holds the first key until 0.1 s, then moves for 1.2 s (the key durations
            // are cumulative).
            const float t = std::clamp((transition_elapsed - 0.1f) / 1.2f, 0.0f, 1.0f);
            const float blend = 0.5f * t + 1.5f * t * t - t * t * t;
            const engine::Vec3 eye{start_eye.x + (end_eye.x - start_eye.x) * blend,
                                   start_eye.y + (end_eye.y - start_eye.y) * blend,
                                   start_eye.z + (end_eye.z - start_eye.z) * blend};
            const float yaw = kStartYaw + (kEndYaw - kStartYaw) * blend;
            const float pitch = kStartPitch * (1.0f - blend);
            const engine::Vec3 target{eye.x + std::sin(yaw) * std::cos(pitch),
                                      eye.y - std::sin(pitch),
                                      eye.z + std::cos(yaw) * std::cos(pitch)};
            const auto view = engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                                     engine::Mat4::look_at(eye, target, {0, 1, 0}));
            const float aspect = static_cast<float>(std::max(input.width, 1)) /
                                 static_cast<float>(std::max(input.height, 1));
            const auto projection =
                engine::Mat4::perspective(45.0f * 3.14159265f / 180.0f, aspect, 1.0f, 3200.0f);
            m_engine.begin_frame(0.0f, 0.0f, 0.0f);
            m_engine.set_camera(view, projection);
            for (const auto* part : entrance) {
                float u = 0.0f;
                float v = 0.0f;
                float opacity = 1.0f;
                WorldRenderCache::sample_prop_uv(*part, elapsed, u, v, opacity);
                // The gate DDS carries a cutout alpha channel despite the GB flag being unset.
                WorldRenderCache::draw_prop_part(m_engine,
                                                 *part,
                                                 engine::Mat4::identity(),
                                                 part->alpha_blend || opacity < 0.999f,
                                                 part->alpha_test ||
                                                     part->uv_material.texture == "login_01.DDS",
                                                 u,
                                                 v,
                                                 opacity);
            }
            // Login framing reserves black bands above and below the scene.
            const float band = height * 0.1f;
            m_engine.draw_ui_rect(0, 0, width, band, 0, 0, 0, 1);
            m_engine.draw_ui_rect(0, height - band, width, band, 0, 0, 0, 1);
            host.set_pointer_hit(options.open() ? std::string{} : hit.widget_id(),
                                 input.mouse_left);
            UiView::draw(m_engine,
                         m_ui,
                         document,
                         panel_x,
                         panel_y,
                         panel_w,
                         panel_h,
                         scale,
                         vars,
                         m_font,
                         m_font,
                         &host);
            if (options.open()) {
                m_engine.draw_ui_rect(0, 0, width, height, 0, 0, 0, 0.6f);
                options.draw(m_engine, m_ui, m_font, &input);
            }
            if (entering) {
                const float fade = std::clamp(transition_elapsed / kFadeDuration, 0.0f, 1.0f);
                m_engine.draw_ui_rect(0, 0, width, height, 0, 0, 0, fade);
            }
            m_cursor.draw(m_ui, input.mouse_x, input.mouse_y, input.dt, input.key_ctrl);
            m_engine.end_frame();
            presented = true;
            if (!wait_for_input || (entering && transition_elapsed >= kFadeDuration)) {
                return true;
            }
        }
        return false;
    }

}  // namespace openkal::client
