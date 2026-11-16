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

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/StatusWidget.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <cstdint>

namespace openkal::client {

    /** Per-frame experience and Rage totals shown by the Dragon HUD. */
    class DragonHudData {
      public:
        /** @return Experience earned toward the current level. */
        [[nodiscard]] std::int64_t current_exp() const {
            return m_current_exp;
        }

        /** @return Experience needed for the current level. */
        [[nodiscard]] std::int64_t total_exp() const {
            return m_total_exp;
        }

        /** @return Current transform rage. */
        [[nodiscard]] std::int64_t rage_current() const {
            return m_rage_current;
        }

        /** @return Rage cap (adult need). */
        [[nodiscard]] std::int64_t rage_total() const {
            return m_rage_total;
        }

        /**
         * Sets the player experience pair.
         * @param current Experience earned toward the current level.
         * @param total Experience needed for the current level.
         */
        void set_exp(std::int64_t current, std::int64_t total) {
            m_current_exp = current;
            m_total_exp = total;
        }

        /**
         * Sets the transform Rage pair shown above EXP.
         * @param current Current rage.
         * @param total Rage cap.
         */
        void set_rage(std::int64_t current, std::int64_t total) {
            m_rage_current = current;
            m_rage_total = total;
        }

        /** Sets the stocked Auto-HP medicine threshold shown inside the HP orb.
         * @param percent Selected percentage, or 0 to hide the marker.
         */
        void set_auto_hp(int percent) {
            m_auto_hp_percent = percent;
        }

        /** @return Auto-HP threshold, or 0 when inactive. */
        int auto_hp_percent() const {
            return m_auto_hp_percent;
        }

      private:
        /** Zero when the slot is empty or the selected medicine has run out. */
        int m_auto_hp_percent = 0;

        /** Experience earned toward the current level. */
        std::int64_t m_current_exp = 0;

        /** Experience needed for the current level. */
        std::int64_t m_total_exp = 0;

        /** Current transform rage. */
        std::int64_t m_rage_current = 0;

        /** Rage cap. */
        std::int64_t m_rage_total = 0;
    };

    /** Draws the bottom Dragon HUD artwork, resources and FreeType labels. */
    class DragonHud final {
      public:
        /** Draws the data-driven HUD; SkillBar overlays the interactive icons.
         * @param ui Shared texture helpers.
         * @param engine Renderer.
         * @param font Regular FreeType face.
         * @param bold Bold FreeType face.
         * @param x Panel left.
         * @param y Panel top.
         * @param scale Uniform display scale.
         * @param status Current HP/MP values.
         * @param data Current experience and Rage totals.
         * @param layout Live HUD layout.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         const FreetypeFont& bold,
                         float x,
                         float y,
                         float scale,
                         const StatusWidgetData& status,
                         const DragonHudData& data,
                         const UiLayout* layout);
    };

}  // namespace openkal::client
