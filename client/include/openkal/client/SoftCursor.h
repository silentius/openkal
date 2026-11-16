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

#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <array>
#include <cstdint>
#include <string>

namespace openkal::client {

    /**
     * Logical cursor slots, matching IDC_CURSOR0..6 / GetMouseCursorIndex.
     *
     * Slots 4..6 and 8 exist on disk (cannon and other specials) but have no named
     * entry; use SoftCursor::set_index() to select them by raw Kal index.
     */
    enum class CursorKind : std::uint8_t {
        Default = 0,
        Attack = 1,
        Item = 2,
        Special = 3,
        Count = 4,
    };

    /** Animated software cursors with legacy bitmap fallbacks.
     * Arrow and sword highlights run for one second every five seconds. The item hand closes
     * and reopens continuously while hovering an item.
     */
    class SoftCursor {
      public:
        /**
         * Loads every cursor bitmap, releasing any previously loaded set.
         * @param engine Engine used to create the textures.
         * @param configuration_root Root holding ui/cursors.
         * @return True when at least one cursor loaded.
         */
        [[nodiscard]] bool load(engine::IEngine& engine, const std::string& configuration_root);

        /**
         * Releases every cursor texture.
         * @param engine Engine owning the texture handles.
         */
        void destroy(engine::IEngine& engine);

        /** Selects a cursor by logical kind, restarting animation on target changes.
         * @param kind Cursor to show.
         * @param target_id Hovered world target, or zero when none is selected.
         */
        void set(CursorKind kind, std::uint32_t target_id = 0);

        /**
         * Selects a cursor by raw Kal index, falling back to the default when it is
         * unavailable.
         * @param index Raw cursor index in [0, 8].
         */
        void set_index(int index);

        /** @return Raw index of the active cursor. */
        [[nodiscard]] int index() const {
            return m_active;
        }

        /** Draws and advances the active cursor at the pointer position.
         * @param ui Shared UI helpers.
         * @param mouse_x Pointer X.
         * @param mouse_y Pointer Y.
         * @param dt Elapsed frame time in seconds.
         * @param control_down Whether either Control key is held; a press locates the pointer.
         */
        void
        draw(WidgetTools& ui, float mouse_x, float mouse_y, float dt, bool control_down = false);

      private:
        /** Draws the locator rings and advances their one-shot animation.
         * @param engine Renderer owning the locator texture.
         * @param mouse_x Pointer X.
         * @param mouse_y Pointer Y.
         * @param dt Elapsed frame time in seconds.
         * @param control_down Current Control key state.
         */
        void draw_locator(
            engine::IEngine& engine, float mouse_x, float mouse_y, float dt, bool control_down);

        /** Number of raw Kal cursor slots (indices 0..8). */
        static constexpr int kSlots = 9;

        /** Cursor textures indexed by raw Kal cursor index. */
        std::array<TexInfo, kSlots> m_tex{};

        /** Horizontal animation cells per texture; legacy cursors have one. */
        std::array<int, kSlots> m_frames{};

        /** Hotspot X per slot, in pixels within one animation cell. */
        std::array<float, kSlots> m_hot_x{};

        /** Hotspot Y per slot. */
        std::array<float, kSlots> m_hot_y{};

        /** Raw index of the active cursor. */
        int m_active = 0;

        /** Antialiased ring, shared by all locator pulses. */
        engine::TextureHandle m_locator_texture{};

        /** Negative while the locator is idle. */
        float m_locator_seconds = -1.0f;
        bool m_control_down = false;

        float m_animation_seconds = 0.0f;
        std::uint32_t m_target_id = 0;

        /** True once at least one cursor bitmap loaded. */
        bool m_ready = false;
    };

}  // namespace openkal::client
