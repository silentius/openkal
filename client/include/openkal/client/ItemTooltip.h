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
#include "openkal/client/ItemDb.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/InsertItem.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    class WidgetTools;

    /** One colored text run inside a tooltip line (Kal #0–#9 palette plus #a dark gray). */
    class TipSpan {
      public:
        /** @return Span text (no color codes). */
        [[nodiscard]] const std::string& text() const {
            return m_text;
        }

        /** @param v Span text. */
        void set_text(std::string v) {
            m_text = std::move(v);
        }

        /** @return Red component in [0,1]. */
        [[nodiscard]] float r() const {
            return m_r;
        }

        /** @param v Red component. */
        void set_r(float v) {
            m_r = v;
        }

        /** @return Green component in [0,1]. */
        [[nodiscard]] float g() const {
            return m_g;
        }

        /** @param v Green component. */
        void set_g(float v) {
            m_g = v;
        }

        /** @return Blue component in [0,1]. */
        [[nodiscard]] float b() const {
            return m_b;
        }

        /** @param v Blue component. */
        void set_b(float v) {
            m_b = v;
        }

      private:
        /** Visible characters for this run. */
        std::string m_text;

        /** Red tint. */
        float m_r = 1.0f;

        /** Green tint. */
        float m_g = 1.0f;

        /** Blue tint. */
        float m_b = 1.0f;
    };

    /** One horizontal tooltip line made of colored spans (centered like compatibility behavior).
     */
    class TipLine {
      public:
        /** @return Spans drawn left-to-right. */
        [[nodiscard]] const std::vector<TipSpan>& spans() const {
            return m_spans;
        }

        /** @return Mutable spans. */
        [[nodiscard]] std::vector<TipSpan>& spans() {
            return m_spans;
        }

        /**
         * Appends a colored run.
         * @param text Visible text.
         * @param r Red.
         * @param g Green.
         * @param b Blue.
         */
        void add(std::string text, float r, float g, float b);

      private:
        /** Ordered runs on this line. */
        std::vector<TipSpan> m_spans;
    };

    /** Optional per-instance talisman / Explosive Blow values for make_tip. */
    struct ItemTipExtras {
        /** Talisman of Attack plus. */
        int x_attack = 0;

        /** Talisman of Magic plus. */
        int x_magic = 0;

        /** Talisman of Accuracy plus. */
        int x_hit = 0;

        /** Talisman of Defense plus. */
        int x_defense = 0;

        /** Talisman of Evasion plus. */
        int x_dodge = 0;

        /** Explosive Blow level. */
        int upgrade_level = 0;

        /** Random equipment modifier type. */
        protocol::RandomItemStatType random_stat_type = protocol::RandomItemStatType::None;

        /** Random equipment modifier magnitude (1 through 5), or zero. */
        int random_stat_value = 0;

        /** Mixing effect type encoded as mix_type low 8 bits. */
        int mix_type = 0;

        /** 1 when Bead of Fire is applied to this armor instance. */
        int bof = 0;

        /** 1 after a successful Perforation Shot. */
        int qigong_perforated = 0;

        /** Qigong random effect id in [1, 80], or zero. */
        int qigong_effect = 0;

        /** Dragon Fusing random effect id in [1, 240], or zero. */
        int dragon_effect = 0;

        /** Worn / instance egg level for transform tips (0 = treat as 1). */
        int egg_level = 0;

        /** Current egg EXP toward the next level. */
        int egg_exp = 0;

        /** Grown-egg display name; empty uses item definition name. */
        std::string egg_name;

        /** HP restored every two seconds by a pet; 0 hides the line. */
        int pet_heal_per_two_seconds = 0;

        /** Whether a pet automatically picks eligible drops. */
        bool pet_auto_pickup = false;

        /** Configured pet or riding-item lifetime; 0 means permanent. */
        std::uint64_t lifetime_duration_seconds = 0;

        /** Current authoritative lifetime remaining; 0 means expired when known. */
        std::uint64_t lifetime_remaining_seconds = 0;

        /** True when lifetime_remaining_seconds came from an authoritative expiry. */
        bool lifetime_expiration_known = false;

        /** True when the item is permanently account-bound. */
        bool account_bound = false;

        /** True while one protected enchantment attempt remains. */
        bool enchant_protected = false;

        /** True when the instance cannot leave its character or enter account storage. */
        bool character_bound = false;
    };

    /**
     * Builds and draws inventory item tips using the supported game layout.
     * (color codes #0–#9, #a, and #n, centered lines, Tahoma-equivalent FreeType
     * face).
     */
    class ItemTooltip {
      public:
        /**
         * Maps a Kal tip color code ('0'..'9' or 'a' for dark gray) to RGB floats.
         * @param digit Color code character.
         * @param r Out red.
         * @param g Out green.
         * @param b Out blue.
         */
        static void color_for_digit(char digit, float& r, float& g, float& b);

        /**
         * Parses a MakeTip-style markup string into centered tip lines.
         * @param markup Text with #0–#9/#a color codes and #n newlines.
         * @return Parsed lines (empty lines preserved when they appear).
         */
        [[nodiscard]] static std::vector<TipLine> parse_markup(std::string_view markup);

        /**
         * Builds tip markup for a catalog item (MakeTip layout including endurance,
         * changeprefix chances, optional instance prefix, and Sell price from
         * item definition plus prefix percent and talisman cubes). Mixing does not
         * change the sell price.
         * @param def Item definition.
         * @param catalog Catalog for sys / prefix strings.
         * @param count Stack count in the slot.
         * @param player_clazz Player class index (0 knight .. 3 thief).
         * @param player_level Player level.
         * @param instance_prefix Equipped prefix index, or 0.
         * @param cur_endurance Current endurance, or -1 to use definition max.
         * @param max_endurance Instance max endurance, or -1 to use definition max.
         * @param extras Optional charm, Explosive Blow, mix, and random-stat fields.
         * @return Markup string ready for parse_markup.
         */
        [[nodiscard]] static std::string make_tip(const ItemDef& def,
                                                  const ItemDb& catalog,
                                                  int count,
                                                  int player_clazz,
                                                  int player_level,
                                                  int instance_prefix = 0,
                                                  int cur_endurance = -1,
                                                  int max_endurance = -1,
                                                  ItemTipExtras extras = {});

        /** Draws a tip box near the cursor using the FreeType UI face.
         * @param ui Shared UI helpers (chat-box overlay chrome).
         * @param engine Engine for quads.
         * @param font Baked TTF atlas (Liberation/DejaVu stand-in for Tahoma).
         * @param lines Parsed tip lines.
         * @param cursor_x Pointer X.
         * @param cursor_y Pointer Y.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param scale UI scale for text, panel and pointer spacing.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         const std::vector<TipLine>& lines,
                         float cursor_x,
                         float cursor_y,
                         float screen_w,
                         float screen_h,
                         float scale = 1.0f);

        /** Picks a tip origin near the pointer. Prefers below-right; flips above/left
         * near screen edges. The overlay is drawn on top of the soft cursor.
         * @param cursor_x Pointer X.
         * @param cursor_y Pointer Y.
         * @param box_w Tip box width.
         * @param box_h Tip box height.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param tip_x Out tip left.
         * @param tip_y Out tip top.
         * @param scale UI scale for pointer spacing.
         */
        static void place_near_cursor(float cursor_x,
                                      float cursor_y,
                                      float box_w,
                                      float box_h,
                                      float screen_w,
                                      float screen_h,
                                      float& tip_x,
                                      float& tip_y,
                                      float scale = 1.0f);
    };

}  // namespace openkal::client
