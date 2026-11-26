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

#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    /**
     * Overhead names for players, NPCs and monsters.
     *
     * Labels sit above the projected EFFECTBONE_HEAD point plus the configured
     * monsterinfo/npcinfo vertical scale. NPC colours use the received state;
     * monster colours use player/monster level-difference bands. Indoor monster
     * labels are hidden when a wall blocks the player-to-head segment.
     */
    class Nameplate {
      public:
        /** monster definition / CreateMonster race that never shows a name (MR_MAGIC). */
        static constexpr int kMonsterRaceMagic = 5;

        /** Extra world-Y lift above body height for other-player overhead names.
         * Monster names use EFFECTBONE_HEAD + scale[1] without this player lift. */
        static constexpr float kMonsterNameLift = 12.0f;

        /** Scales the 14px name font to the inventory item-info overlay's 18px size. */
        static constexpr float kTextScale =
            static_cast<float>(FreetypeFont::kTipPixelSize) / FreetypeFont::kNamePixelSize;

        /** Glyph scale for overhead-name fallback text (native bitmap cell is 10px). */
        static constexpr float kGlyphScale =
            static_cast<float>(FreetypeFont::kTipPixelSize) / 10.0f;

        /** Black name backing alpha: 50% transparency. */
        static constexpr float kBackdropAlpha = 0.50f;

        /** Screen-Y lift applied to a selected monster name so the HP bar fits under it. */
        static constexpr float kSelectedHpNameLiftPx = 18.0f;

        /** Extra pixels between the name baseline and the top of the HP bar. */
        static constexpr float kHpBarNameGapPx = 4.0f;

        /** Overhead HP bar width in pixels. */
        static constexpr float kHpBarW = 90.0f;

        /** Overhead HP bar height in pixels. */
        static constexpr float kHpBarH = 10.0f;

        /** Enlarged overhead HP bar width in pixels. */
        static constexpr float kLargeHpBarW = kHpBarW * 1.50f;

        /** Enlarged overhead HP bar height in pixels. */
        static constexpr float kLargeHpBarH = kHpBarH * 1.25f;

        /** Seconds the local player's overhead HP bar stays up after taking damage. */
        static constexpr float kCombatHpTimeoutSec = 5.0f;

        /**
         * Fixed NPC overhead colour, RGB(180, 150, 255) from
         * compatibility behavior.
         * @return Linear RGB in 0..1.
         */
        [[nodiscard]] static RgbColor npc_color();

        /**
         * Other-player overhead colour, white below level 70.
         * @return Linear RGB in 0..1.
         */
        [[nodiscard]] static RgbColor player_color();

        /**
         * Monster overhead colour from player level vs monsterinfo levelno.
         * Treasure boxes (race Item / 4) are always white (ChkEventBoxMonster).
         * @param player_level Local player level.
         * @param monster_level Monster macro / JSON level.
         * @param race CreateMonster race byte (0 = combat colour bands).
         * @return Linear RGB in 0..1.
         */
        [[nodiscard]] static RgbColor
        monster_color(int player_level, int monster_level, int race = 0);

        /**
         * Whether a monster should show an overhead name.
         * @param index Monster index.
         * @param race CreateMonster race byte.
         * @return True when the name should be drawn.
         */
        [[nodiscard]] static bool show_monster(int index, int race);

        /** Whether a selected monster shows an HP bar; gathering plants and boxes do not.
         * @param race CreateMonster race byte.
         * @return False for entities opened or gathered through the Item interaction.
         */
        [[nodiscard]] static bool show_monster_hp(int race);

        /**
         * Whether an NPC should show an overhead name.
         * @param shape Appearance shape (npcinfo key).
         * @param name_enabled False when npcinfo (name 0) hid the label.
         * @return True when the name should be drawn.
         */
        [[nodiscard]] static bool show_npc(int shape, bool name_enabled);

        /**
         * Builds the overhead monster label `Name [Level]`.
         * Treasure boxes (race Item / 4) keep the name without a level suffix.
         * @param name Monster display name.
         * @param level Monsterinfo / JSON level.
         * @param race CreateMonster race byte (0 = append level).
         * @return Label text, or empty when name is empty.
         */
        [[nodiscard]] static std::string
        monster_label(const std::string& name, int level, int race = 0);

        /**
         * Builds a debug-only overhead label with its catalog or entity index.
         * @param label Normal overhead label.
         * @param index NPC/monster catalog index or player entity id.
         * @return Label with the index suffix, or empty when label is empty.
         */
        [[nodiscard]] static std::string debug_label(const std::string& label, std::int64_t index);

        /** Formats the player name without changing the stored identity.
         * @param name Character name.
         * @param rank Visible Honor rank, or empty for the name alone.
         * @return Nameplate text, or empty when the name is empty.
         */
        [[nodiscard]] static std::string player_label(std::string_view name,
                                                      std::string_view rank = {});

        /** Formats the guild subtitle in title/name order.
         * @param title Current member title. @param guild Visible guild name.
         * @return Subtitle, or empty when no guild is visible.
         */
        [[nodiscard]] static std::string guild_label(std::string_view title,
                                                     std::string_view guild);

        /** Draws the player name above the guild subtitle, keeping both above the anchor.
         * Uses FreeType and its line metrics so the two backdrops do not overlap.
         * @param engine Renderer. @param font FreeType name face. @param ui UI helper.
         * @param screen_x Projected anchor X. @param screen_y Projected anchor Y.
         * @param name Formatted player name, optionally including debug information.
         * @param guild Subtitle containing the title and visible guild name.
         * @param color Label color.
         */
        static void draw_player(engine::IEngine& engine,
                                const FreetypeFont& font,
                                WidgetTools& ui,
                                float screen_x,
                                float screen_y,
                                const std::string& name,
                                const std::string& guild,
                                const RgbColor& color);

        /**
         * Draws a centered name above the projected point (text sits
         * above the projected world point). Uses the FreeType atlas when it is ready,
         * otherwise the HUD bitmap font.
         * @param engine Engine used for the backing rect and TTF quads.
         * @param font FreeType name atlas.
         * @param ui HUD text helper used as a bitmap-font fallback.
         * @param screen_x Projected screen X of the name anchor.
         * @param screen_y Projected screen Y of the name anchor.
         * @param name Display name.
         * @param color Label colour.
         */
        static void draw(engine::IEngine& engine,
                         const FreetypeFont& font,
                         WidgetTools& ui,
                         float screen_x,
                         float screen_y,
                         const std::string& name,
                         const RgbColor& color);

        /** Draws an optional NPC symbol above its projected head/name, preserving aspect ratio.
         * @param ui UI texture renderer. @param screen_x Projected horizontal center.
         * @param screen_y Projected name baseline. @param file UI bitmap, empty to disable.
         * @param name_height Visible name height in pixels, zero for a hidden name.
         * @param scale UI scale for the 32-pixel symbol and its gap. */
        static void draw_npc_icon(WidgetTools& ui,
                                  float screen_x,
                                  float screen_y,
                                  const std::string& file,
                                  float name_height,
                                  float scale);

        /**
         * Draws a compact overhead HP bar centered on the projected point. The top
         * of the bar sits on `screen_y` so it hangs under a name drawn with UPPER
         * placement at the same anchor. No HP text.
         * @param ui HUD helper used for the track and LCR fill.
         * @param screen_x Projected screen X of the bar center.
         * @param screen_y Projected screen Y of the bar top.
         * @param ratio Fill ratio in 0..1.
         * @param enlarged Whether to render the larger target HP bar.
         */
        static void draw_hp(
            WidgetTools& ui, float screen_x, float screen_y, float ratio, bool enlarged = false);
    };

}  // namespace openkal::client
