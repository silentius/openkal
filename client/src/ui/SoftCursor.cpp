/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SoftCursor.h"

#include "openkal/platform/Path.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace openkal::client {
    namespace {

        // winmain.rc remaps IDC_CURSOR3/6 to different .cur files; our BMP names
        // already use the logical IDC index (cursor3.bmp = IDC_CURSOR3 art).
        constexpr int kLoadIndices[] = {0, 1, 2, 3, 4, 5, 6, 8};

        constexpr int kLocatorTextureSize = 128;
        constexpr float kLocatorTextureRadius = 52.0f;
        constexpr float kLocatorPulseSeconds = 0.8f;
        constexpr float kLocatorPulseDelay = 0.15f;
        constexpr int kLocatorPulses = 3;
        constexpr float kLocatorDuration =
            kLocatorPulseSeconds + (kLocatorPulses - 1) * kLocatorPulseDelay;

        engine::TextureHandle create_locator_texture(engine::IEngine& engine) {
            std::vector<std::uint8_t> pixels(kLocatorTextureSize * kLocatorTextureSize * 4);
            for (int y = 0; y < kLocatorTextureSize; ++y) {
                for (int x = 0; x < kLocatorTextureSize; ++x) {
                    const float distance = std::abs(std::hypot(x + 0.5f - 64.0f, y + 0.5f - 64.0f) -
                                                    kLocatorTextureRadius);
                    const float core = std::clamp(2.0f - distance, 0.0f, 1.0f);
                    const float border = std::clamp(3.5f - distance, 0.0f, 1.0f);
                    const float glow = 0.2f * std::clamp(1.0f - distance / 9.0f, 0.0f, 1.0f);
                    const float alpha = std::max(border, glow);
                    // The dark border keeps the bright ring visible on pale scenery.
                    const float brightness = border > 0.0f ? 0.06f + 0.94f * core : 1.0f;
                    const auto offset = static_cast<std::size_t>((y * kLocatorTextureSize + x) * 4);
                    pixels[offset] = pixels[offset + 1] = pixels[offset + 2] =
                        static_cast<std::uint8_t>(255.0f * brightness);
                    pixels[offset + 3] = static_cast<std::uint8_t>(255.0f * alpha);
                }
            }
            return engine.create_texture_rgba8(
                pixels.data(), kLocatorTextureSize, kLocatorTextureSize, false);
        }

    }  // namespace

    bool SoftCursor::load(engine::IEngine& engine, const std::string& configuration_root) {
        destroy(engine);
        const std::string dir = platform::join_path(configuration_root, "ui/cursors");
        bool any = false;
        for (int idx : kLoadIndices) {
            const auto slot_index = static_cast<std::size_t>(idx);
            terrain::ImageRgba img;
            const std::string stem = platform::join_path(dir, "cursor" + std::to_string(idx));
            const int frames = idx == 2 ? 16 : 30;
            const bool animated = idx <= 2 &&
                                  WidgetTools::load_bmp_rgba(stem + "_strip.bmp", img) &&
                                  img.width == 48 * frames && img.height == 48;
            if (!animated && !WidgetTools::load_bmp_rgba(stem + ".bmp", img)) {
                continue;
            }
            auto& slot = m_tex[slot_index];
            slot.set_handle(
                engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, false));
            if (!slot.handle()) {
                continue;
            }
            slot.set_size(img.width, img.height);
            m_frames[slot_index] = animated ? frames : 1;
            m_hot_x[slot_index] = animated ? (idx == 2 ? 24.0f : 6.0f) : 0.0f;
            m_hot_y[slot_index] = animated ? (idx == 2 ? 24.0f : 4.0f) : 0.0f;
            any = true;
        }
        m_locator_texture = create_locator_texture(engine);
        m_ready = any;
        m_active = 0;
        return m_ready;
    }

    void SoftCursor::destroy(engine::IEngine& engine) {
        for (auto& t : m_tex) {
            if (t.handle()) {
                engine.destroy_texture(t.handle());
                t = {};
            }
        }
        if (m_locator_texture) {
            engine.destroy_texture(m_locator_texture);
            m_locator_texture = {};
        }
        m_locator_seconds = -1.0f;
        m_control_down = false;
        m_frames.fill(1);
        m_hot_x.fill(0.0f);
        m_hot_y.fill(0.0f);
        m_ready = false;
        m_active = 0;
        m_animation_seconds = 0.0f;
        m_target_id = 0;
    }

    void SoftCursor::set(CursorKind kind, std::uint32_t target_id) {
        const auto previous_target = m_target_id;
        set_index(static_cast<int>(kind));
        if (previous_target != target_id) {
            m_animation_seconds = 0.0f;
        }
        m_target_id = target_id;
    }

    void SoftCursor::set_index(int index) {
        if (index < 0 || index >= kSlots || !m_tex[static_cast<std::size_t>(index)].handle()) {
            index = 0;
        }
        if (m_active != index) {
            m_animation_seconds = 0.0f;
            m_target_id = 0;
        }
        m_active = index;
    }

    void SoftCursor::draw_locator(
        engine::IEngine& engine, float mouse_x, float mouse_y, float dt, bool control_down) {
        if (control_down && !m_control_down) {
            m_locator_seconds = 0.0f;
        }
        m_control_down = control_down;
        if (m_locator_seconds < 0.0f) {
            return;
        }
        if (m_locator_texture) {
            for (int pulse = 0; pulse < kLocatorPulses; ++pulse) {
                const float age = m_locator_seconds - pulse * kLocatorPulseDelay;
                if (age < 0.0f || age >= kLocatorPulseSeconds) {
                    continue;
                }
                const float remaining = 1.0f - age / kLocatorPulseSeconds;
                const float radius = 12.0f + 84.0f * remaining * remaining;
                const float size = radius * kLocatorTextureSize / kLocatorTextureRadius;
                engine.draw_ui_quad(m_locator_texture,
                                    mouse_x - size * 0.5f,
                                    mouse_y - size * 0.5f,
                                    size,
                                    size,
                                    1.0f,
                                    0.92f,
                                    0.66f,
                                    remaining);
            }
        }
        if (std::isfinite(dt) && dt > 0.0f) {
            m_locator_seconds += dt;
            if (m_locator_seconds >= kLocatorDuration) {
                m_locator_seconds = -1.0f;
            }
        }
    }

    void
    SoftCursor::draw(WidgetTools& ui, float mouse_x, float mouse_y, float dt, bool control_down) {
        if (!ui.renderer()) {
            return;
        }
        draw_locator(*ui.renderer(), mouse_x, mouse_y, dt, control_down);
        const auto slot = static_cast<std::size_t>(m_active);
        const auto& texture = m_tex[slot];
        if (!m_ready || !texture.handle()) {
            ui.renderer()->draw_ui_rect(mouse_x, mouse_y, 14, 2, 1, 1, 1, 1);
            ui.renderer()->draw_ui_rect(mouse_x, mouse_y, 2, 14, 1, 1, 1, 1);
            return;
        }
        const int frames = m_frames[slot];
        const bool grip = m_active == static_cast<int>(CursorKind::Item);
        constexpr float kGripSeconds = 0.4f;
        const float progress =
            grip ? 1.0f - std::abs(1.0f - m_animation_seconds / kGripSeconds) : m_animation_seconds;
        const int frame = !grip && progress >= 1.0f
                              ? 0
                              : std::min(frames - 1, static_cast<int>(progress * frames));
        const float width = static_cast<float>(texture.width()) / static_cast<float>(frames);
        ui.renderer()->draw_ui_quad(texture.handle(),
                                    mouse_x - m_hot_x[slot],
                                    mouse_y - m_hot_y[slot],
                                    width,
                                    static_cast<float>(texture.height()),
                                    1,
                                    1,
                                    1,
                                    1,
                                    static_cast<float>(frame) / static_cast<float>(frames),
                                    0,
                                    static_cast<float>(frame + 1) / static_cast<float>(frames),
                                    1);
        if (std::isfinite(dt) && dt > 0.0f) {
            m_animation_seconds =
                std::fmod(m_animation_seconds + dt, grip ? 2.0f * kGripSeconds : 5.0f);
        }
    }

}  // namespace openkal::client
