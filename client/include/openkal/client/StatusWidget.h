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
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <string>
#include <utility>

namespace openkal::client {

    /** Per-frame snapshot of the local player shown by the status panel. */
    class StatusWidgetData {
      public:
        /** @return Class portrait bitmap file name. */
        [[nodiscard]] const char* class_icon() const {
            return m_class_icon;
        }

        /** @return Character name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /** @return Character level. */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /** @return Current hit points. */
        [[nodiscard]] int hp() const {
            return m_hp;
        }

        /** @return Maximum hit points. */
        [[nodiscard]] int max_hp() const {
            return m_max_hp;
        }

        /** @return Current mana points. */
        [[nodiscard]] int mp() const {
            return m_mp;
        }

        /** @return Maximum mana points. */
        [[nodiscard]] int max_mp() const {
            return m_max_mp;
        }

        /** @return HP bar fill ratio in 0..1. */
        [[nodiscard]] float hp_ratio() const {
            return m_hp_ratio;
        }

        /** @return MP bar fill ratio in 0..1. */
        [[nodiscard]] float mp_ratio() const {
            return m_mp_ratio;
        }

        /**
         * Sets the class portrait bitmap.
         * @param icon Bitmap file name; must outlive the draw call.
         */
        void set_class_icon(const char* icon) {
            m_class_icon = icon;
        }

        /**
         * Sets the character name.
         * @param name Character name.
         */
        void set_name(std::string name) {
            m_name = std::move(name);
        }

        /**
         * Sets the character level.
         * @param level Character level.
         */
        void set_level(int level) {
            m_level = level;
        }

        /**
         * Sets the hit point values and the derived bar ratio.
         * @param hp Current hit points.
         * @param max_hp Maximum hit points.
         */
        void set_hp(int hp, int max_hp);

        /**
         * Sets the mana point values and the derived bar ratio.
         * @param mp Current mana points.
         * @param max_mp Maximum mana points.
         */
        void set_mp(int mp, int max_mp);

      private:
        /** Class portrait bitmap file name. */
        const char* m_class_icon = "main-character-icon(musa).bmp";

        /** Character name. */
        std::string m_name;

        /** Character level. */
        int m_level = 1;

        /** Current hit points. */
        int m_hp = 0;

        /** Maximum hit points. */
        int m_max_hp = 0;

        /** Current mana points. */
        int m_mp = 0;

        /** Maximum mana points. */
        int m_max_mp = 0;

        /** HP bar fill ratio in 0..1. */
        float m_hp_ratio = 1.0f;

        /** MP bar fill ratio in 0..1. */
        float m_mp_ratio = 1.0f;
    };

    /** Dragon identity panel with a round class emblem, character name and level. */
    class StatusWidget {
      public:
        /** Tests the always-visible emblem and level artwork for HUD dragging.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param x Panel left.
         * @param y Panel top.
         * @param scale UI scale.
         * @param layout Status layout document.
         * @return True when the pointer touches the persistent status artwork.
         */
        [[nodiscard]] static bool
        contains(float mx, float my, float x, float y, float scale, const UiLayout* layout);

        /**
         * Draws the status panel.
         * @param ui Shared UI helpers.
         * @param engine Engine used for FreeType text.
         * @param font Body face, or nullptr to skip labels.
         * @param x Panel left.
         * @param y Panel top.
         * @param data Player snapshot to render.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param pointer_enabled Whether other UI permits hover interaction.
         * @param scale UI scale.
         * @param layout Status layout document, or nullptr for baked defaults.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont* font,
                         float x,
                         float y,
                         const StatusWidgetData& data,
                         float mx,
                         float my,
                         bool pointer_enabled,
                         float scale = 1.0f,
                         const UiLayout* layout = nullptr);
    };

}  // namespace openkal::client
