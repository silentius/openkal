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
#include "openkal/client/Player.h"
#include "openkal/client/UiLayout.h"
#include "openkal/engine/engine.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Kind of floating combat line (color and copy). */
    enum class CombatFloatKind : int {
        DamageDealt = 0,
        DamageTaken = 1,
        Missed = 2,
        Blocked = 3,
        Heal = 4,
    };

    /**
     * Rising outlined combat numbers over monsters and the local player.
     * Shared size, bold TTF, gray stroke, and rise/fade for every kind.
     */
    class CombatFloatText {
      public:
        /** Lifetime in seconds. */
        static constexpr float kDurationSec = 1.60f;

        /** Screen-Y rise in pixels over the lifetime. */
        static constexpr float kRisePixels = 48.0f;

        /** Age fraction after which alpha starts falling. */
        static constexpr float kFadeStart = 0.55f;

        /** Draw scale on top of the combat atlas (1.2 * 1.3). */
        static constexpr float kDrawScale = 1.56f;

        /** World-Y lift above the captured head so the glyph starts clear of the skull. */
        static constexpr float kHeadLift = 12.0f;

        /** Extra screen-Y offset when another line already lives on the same anchor. */
        static constexpr float kStackGapPixels = 30.0f;

        /** Outline gray in 0..1. */
        static constexpr float kOutlineGray = 0.35f;

        /**
         * Fill color for a combat-float kind.
         * @param kind Line kind.
         * @return Linear RGB in 0..1.
         */
        [[nodiscard]] static RgbColor color_for(CombatFloatKind kind);

        /** @return Player-on-monster damage red. */
        [[nodiscard]] static RgbColor damage_dealt_color();

        /** @return Monster-on-player damage white. */
        [[nodiscard]] static RgbColor damage_taken_color();

        /** @return Emerald for Missed, Blocked, and heal. */
        [[nodiscard]] static RgbColor emerald_color();

        /** @return English miss copy. */
        [[nodiscard]] static const char* missed_text();

        /** @return English block copy. */
        [[nodiscard]] static const char* blocked_text();

        /**
         * Alpha for a given age.
         * @param age_sec Seconds since spawn.
         * @return 1 until fade start, then linear to 0.
         */
        [[nodiscard]] static float alpha_at(float age_sec);

        /**
         * Screen-Y rise for a given age (negative is up in UI space).
         * @param age_sec Seconds since spawn.
         * @return Pixels to subtract from the projected Y.
         */
        [[nodiscard]] static float rise_at(float age_sec);

        /**
         * Spawns a line over an entity. Several lines may be visible at once;
         * same-anchor spawns are stacked so they do not occupy the same pixel.
         * @param kind Color and copy kind.
         * @param text Display text.
         * @param anchor_id 0 = local player, else monster entity id.
         */
        void spawn(CombatFloatKind kind, std::string text, std::uint32_t anchor_id);

        /**
         * Ages live lines and drops expired ones.
         * @param dt Frame delta seconds.
         */
        void update(float dt);

        /** @return Live line count. */
        [[nodiscard]] std::size_t size() const {
            return m_entries.size();
        }

        /** @return Extra screen-Y of the most recently spawned line. */
        [[nodiscard]] float last_extra_rise() const;

        /**
         * Projects and draws live lines from their frozen world points.
         * @param engine Engine used for TTF quads.
         * @param font Bold combat atlas.
         * @param cam Follow camera.
         * @param player Local player.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param local_x Local player world X.
         * @param local_y Local player nameplate world Y.
         * @param local_z Local player world Z.
         * @param monster_pos Looks up a monster world position; returns false if unknown.
         */
        void draw(engine::IEngine& engine,
                  const FreetypeFont& font,
                  const FollowCamera& cam,
                  const LocalPlayer& player,
                  float screen_w,
                  float screen_h,
                  float local_x,
                  float local_y,
                  float local_z,
                  const std::function<bool(std::uint32_t, float&, float&, float&)>& monster_pos);

      private:
        /** One rising line. */
        class Entry {
          public:
            /** @return Line kind. */
            [[nodiscard]] CombatFloatKind kind() const {
                return m_kind;
            }

            /** @param v Line kind. */
            void set_kind(CombatFloatKind v) {
                m_kind = v;
            }

            /** @return Display text. */
            [[nodiscard]] const std::string& text() const {
                return m_text;
            }

            /** @param v Display text. */
            void set_text(std::string v) {
                m_text = std::move(v);
            }

            /** @return Fill color. */
            [[nodiscard]] const RgbColor& color() const {
                return m_color;
            }

            /** @param v Fill color. */
            void set_color(const RgbColor& v) {
                m_color = v;
            }

            /** @return Anchor monster id, or 0 for the local player. */
            [[nodiscard]] std::uint32_t anchor_id() const {
                return m_anchor_id;
            }

            /** @param v Anchor monster id, or 0 for the local player. */
            void set_anchor_id(std::uint32_t v) {
                m_anchor_id = v;
            }

            /** @return Age in seconds. */
            [[nodiscard]] float age_sec() const {
                return m_age_sec;
            }

            /** @param v Age in seconds. */
            void set_age_sec(float v) {
                m_age_sec = v;
            }

            /** @return Horizontal screen jitter in pixels. */
            [[nodiscard]] float jitter_x() const {
                return m_jitter_x;
            }

            /** @param v Horizontal screen jitter in pixels. */
            void set_jitter_x(float v) {
                m_jitter_x = v;
            }

            /** @return Extra screen-Y offset so stacked spawns do not overlap. */
            [[nodiscard]] float extra_rise() const {
                return m_extra_rise;
            }

            /** @param v Extra screen-Y offset in pixels. */
            void set_extra_rise(float v) {
                m_extra_rise = v;
            }

            /** @return Last known world X. */
            [[nodiscard]] float wx() const {
                return m_wx;
            }

            /** @param v Last known world X. */
            void set_wx(float v) {
                m_wx = v;
            }

            /** @return Last known world Y. */
            [[nodiscard]] float wy() const {
                return m_wy;
            }

            /** @param v Last known world Y. */
            void set_wy(float v) {
                m_wy = v;
            }

            /** @return Last known world Z. */
            [[nodiscard]] float wz() const {
                return m_wz;
            }

            /** @param v Last known world Z. */
            void set_wz(float v) {
                m_wz = v;
            }

            /** @return True when a world point has been stored. */
            [[nodiscard]] bool has_world() const {
                return m_has_world;
            }

            /** @param v True when a world point has been stored. */
            void set_has_world(bool v) {
                m_has_world = v;
            }

          private:
            /** Line kind. */
            CombatFloatKind m_kind = CombatFloatKind::DamageDealt;

            /** Display text. */
            std::string m_text;

            /** Fill color. */
            RgbColor m_color;

            /** Anchor monster id, or 0 for the local player. */
            std::uint32_t m_anchor_id = 0;

            /** Age in seconds. */
            float m_age_sec = 0.0f;

            /** Horizontal screen jitter in pixels. */
            float m_jitter_x = 0.0f;

            /** Extra screen-Y offset in pixels. */
            float m_extra_rise = 0.0f;

            /** Last known world X. */
            float m_wx = 0.0f;

            /** Last known world Y. */
            float m_wy = 0.0f;

            /** Last known world Z. */
            float m_wz = 0.0f;

            /** True when a world point has been stored. */
            bool m_has_world = false;
        };

        /**
         * Draws centered text with a 1px gray outline.
         * @param engine Engine used for TTF quads.
         * @param font Combat atlas.
         * @param cx Center X.
         * @param y Top of the line box.
         * @param text Display text.
         * @param color Fill color.
         * @param alpha Opacity.
         */
        static void draw_outlined_centered(engine::IEngine& engine,
                                           const FreetypeFont& font,
                                           float cx,
                                           float y,
                                           std::string_view text,
                                           const RgbColor& color,
                                           float alpha);

        /**
         * Counts live lines already attached to an anchor.
         * @param anchor_id 0 = local player, else monster entity id.
         * @return Number of live lines on that anchor.
         */
        [[nodiscard]] int live_on_anchor(std::uint32_t anchor_id) const;

        /** Live lines. */
        std::vector<Entry> m_entries;
    };

}  // namespace openkal::client
