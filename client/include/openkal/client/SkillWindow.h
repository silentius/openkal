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
#include "openkal/client/Skills.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Outcome of a click offered to the skill window. */
    enum class SkillClickResult {
        Ignored = 0,
        Consumed,
        Cast,
        Learn,
        DragBegin,
        OpenTalents,
    };

    /** Tab row of the skill window. */
    enum class SkillTab : int {
        Common = 0,
        Job1,
        Job2,
        Job3,
        Awaken,
        Action,
        Talents,
        Count,
    };

    /** Class skill window: Common / Job / Awaken / Action / Talents tabs
     * over an Active and a Passive icon grid.
     *
     * The window is dragged by its title bar or frame padding. Chrome and geometry
     * come from config/ui/skill_window.json when a layout is bound, otherwise
     * from baked defaults.
     */
    class SkillWindow {
      public:
        /** @return True while the window is shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /**
         * Shows or hides the window.
         * @param open True to show it.
         */
        void set_open(bool open) {
            m_open = open;
        }

        /** Flips the open state. */
        void toggle() {
            m_open = !m_open;
        }

        /** @return Client box left in pixels. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Client box top in pixels. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Client box width in pixels. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Client box height in pixels. */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /**
         * Moves the window.
         * @param x Client box left in pixels.
         * @param y Client box top in pixels.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
        }

        /** @return Display scale (1 = native JSON units). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the display scale. Call sync_from_layout() afterwards to resize the
         * client box.
         * @param scale Display scale (1 = native JSON units).
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /** @return Selected tab. */
        [[nodiscard]] SkillTab tab() const {
            return m_tab;
        }

        /**
         * Selects a tab and clears the slot selection. Locked job tabs snap to
         * Common.
         * @param tab Tab to show.
         */
        void set_tab(SkillTab tab);

        /** @return Selected grid slot, or -1. */
        [[nodiscard]] int selected_slot() const {
            return m_selected_slot;
        }

        /**
         * Binds the skill catalog read for icons, names and requirements.
         * @param catalog Catalog; must outlive the window. Pass nullptr to unbind.
         */
        void set_catalog(const Catalog* catalog) {
            m_catalog = catalog;
        }

        /** @return Bound skill catalog, or nullptr. */
        [[nodiscard]] const Catalog* catalog() const {
            return m_catalog;
        }

        /**
         * Binds the player skill state that learning and casting mutate.
         * @param player Player skills; must outlive the window. Pass nullptr to
         * unbind.
         */
        void set_player(PlayerSkills* player) {
            m_player = player;
        }

        /** @return Bound player skill state, or nullptr. */
        [[nodiscard]] PlayerSkills* player() const {
            return m_player;
        }

        /** @return Player class index used to filter the catalog. */
        [[nodiscard]] int player_clazz() const {
            return m_player_clazz;
        }

        /**
         * Sets the player class index.
         * @param clazz Class index.
         */
        void set_player_clazz(int clazz) {
            m_player_clazz = clazz;
        }

        /**
         * Sets character stats used to fill ability tooltip placeholders.
         * @param vars `char_level`, `char_int`, lightning mastery rank, …
         */
        void set_ability_vars(SkillAbilityVars vars) {
            m_ability_vars = vars;
        }

        /** @return Player level used to gate learning. */
        [[nodiscard]] int player_level() const {
            return m_player_level;
        }

        /**
         * Sets the player level.
         * @param level Character level.
         */
        void set_player_level(int level) {
            m_player_level = level;
        }

        /** @return Job specialty bitmask (bit 0 = wandering). */
        [[nodiscard]] int player_specialty() const {
            return m_player_specialty;
        }

        /**
         * Sets the government-position specialty bitmask used to unlock job tabs.
         * Locked tabs snap back to Common.
         * @param specialty Job bitmask (1 wandering, 3 first job, 7/11 second, 23/43
         * third).
         */
        void set_player_specialty(int specialty);

        /** Controls availability of multiplayer interactions. @param enabled Multiplayer mode. */
        void set_multiplayer(bool enabled) {
            if (m_multiplayer != enabled) {
                m_multiplayer = enabled;
                m_cache_catalog = nullptr;
            }
        }

        /** @return Skill index of the last SkillClickResult::Cast, or -1. */
        [[nodiscard]] int last_cast_index() const {
            return m_last_cast_index;
        }

        /** @return Cast kind of the last SkillClickResult::Cast. */
        [[nodiscard]] CastKind last_cast_kind() const {
            return m_last_cast_kind;
        }

        /** @return Skill index currently dragged toward the skill bar, or -1. */
        [[nodiscard]] int drag_skill_index() const {
            return m_drag_skill_index;
        }

        /** Identifies an emote click when releasing a skill without dragging it.
         * @param mx Release pointer X.
         * @param my Release pointer Y.
         * @return Emote shortcut, or -1 after an actual drag or another skill.
         */
        [[nodiscard]] int click_cast_on_release(float mx, float my) const;

        /**
         * Sets the dragged skill index.
         * @param index Skill index, or -1 to end the drag.
         */
        void set_drag_skill_index(int index) {
            m_drag_skill_index = index;
        }

        /** @return True while the window itself is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /**
         * Binds the layout document holding chrome and geometry.
         * @param layout Layout; must outlive the window. Pass nullptr for baked
         * defaults.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** Applies window.w / window.h from the layout, keeping the current position.
         */
        void sync_from_layout();

        /**
         * Loads or reloads FreeType faces from the layout font sizes.
         * @param engine Engine used to create atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases owned FreeType atlas textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Resizes from the layout and centers the window on screen.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * Ages every running skill cooldown.
         * @param dt Frame delta in seconds.
         */
        void tick(float dt);

        /**
         * Starts a cooldown for a skill so its icon dims on the bar.
         * @param skill_index Skill index.
         * @param delay_pre_ms Cast wind-up in milliseconds.
         * @param delay_ms Cooldown after the cast in milliseconds.
         */
        void begin_cooldown(int skill_index, int delay_pre_ms, int delay_ms);

        /** Synchronizes a skill-icon cooldown from the server.
         * @param skill_index Skill index.
         * @param remaining_ms Remaining cooldown, or zero to clear it.
         * @param total_ms Full cooldown duration for the clock overlay.
         */
        void sync_cooldown(int skill_index, int remaining_ms, int total_ms);

        /**
         * Clears every running skill-icon cooldown.
         */
        void clear_cooldowns();

        /**
         * Tests whether a skill is still cooling down.
         * @param skill_index Skill index.
         * @return True while the cooldown is running.
         */
        [[nodiscard]] bool on_cooldown(int skill_index) const;

        /**
         * Remaining cooldown as a 0..1 fraction (1 = just started).
         * @param skill_index Skill index.
         * @return Remaining / total, or 0 when idle.
         */
        [[nodiscard]] float cooldown_fraction(int skill_index) const;

        /**
         * Tests whether the pointer is over the framed window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the window is open and under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Applies one frame of pointer input: window dragging and slot hover for the
         * tooltip.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Applies a left click: close, tabs, footer buttons, then the skill grid.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return What the window did with the click.
         */
        SkillClickResult handle_click(float mx, float my);

        /**
         * Draws the window chrome, grids and labels (no hover tooltip).
         * @param engine Engine used for untextured rects.
         * @param ui Shared UI helpers.
         */
        void draw(engine::IEngine& engine, WidgetTools& ui) const;

        /**
         * Draws the hover skill tooltip on top of the soft cursor.
         * @param engine Engine used for untextured rects.
         * @param ui Shared UI helpers.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void draw_tooltip(engine::IEngine& engine,
                          WidgetTools& ui,
                          float screen_w,
                          float screen_h) const;

        /**
         * Draws the dragged skill icon under the cursor.
         * @param ui Shared UI helpers.
         * @param mx Pointer X.
         * @param my Pointer Y.
         */
        void draw_drag_ghost(WidgetTools& ui, float mx, float my) const;

      private:
        class ViewHost;

        /** Builds the document's text and visibility bindings.
         * @return Values for the current skill page.
         */
        [[nodiscard]] UiVarMap view_vars() const;

        /** Rebuilds the cached Active / Passive lists when catalog, class, specialty
         * or tab changed. */
        void ensure_tab_skills_cache() const;

        /**
         * Tests whether a skill-window tab is unlocked for the current specialty.
         * @param tab Tab to test.
         * @return True when the tab may be selected.
         */
        [[nodiscard]] bool tab_unlocked(SkillTab tab) const;

        /** Snaps runtime state to Common when the current tab is locked. */
        void clamp_tab_to_unlocked();

        /** True while the window is shown. */
        bool m_open = false;

        /** Client box left in pixels. */
        float m_x = 80.0f;

        /** Client box top in pixels. */
        float m_y = 60.0f;

        /** Client box width in pixels. */
        float m_w = 440.0f;

        /** Client box height in pixels. */
        float m_h = 520.0f;

        /** Selected tab. */
        SkillTab m_tab = SkillTab::Common;

        /** Selected grid slot, or -1. */
        int m_selected_slot = -1;

        /** Skill catalog read for icons, names and requirements; not owned. */
        const Catalog* m_catalog = nullptr;

        /** Player skill state mutated by learning and casting; not owned. */
        PlayerSkills* m_player = nullptr;

        /** Availability of player interaction actions in this session. */
        bool m_multiplayer = false;

        /** Player class index used to filter the catalog. */
        int m_player_clazz = 0;

        /** Character stats for ability tooltip placeholders. */
        SkillAbilityVars m_ability_vars;

        /** Player level used to gate learning. */
        int m_player_level = 1;

        /** Job specialty bitmask (1 = wandering). */
        int m_player_specialty = 1;

        /** Skill index of the last Cast result, or -1. */
        int m_last_cast_index = -1;

        /** Cast kind of the last Cast result. */
        CastKind m_last_cast_kind = CastKind::None;

        /** Skill index currently dragged toward the skill bar, or -1. */
        int m_drag_skill_index = -1;

        /** Origin and movement threshold for distinguishing an emote click from a drag. */
        float m_skill_press_x = 0.0f;
        float m_skill_press_y = 0.0f;
        bool m_skill_drag_moved = false;

        /** Grid slot under the pointer, or -1. */
        int m_hover_slot = -1;

        /** Pointer X captured for the tooltip. */
        float m_hover_mx = 0;

        /** Pointer Y captured for the tooltip. */
        float m_hover_my = 0;

        /** Primary pointer state for shared button feedback. */
        bool m_mouse_down = false;

        /** Remaining cooldown seconds per skill index. */
        std::unordered_map<int, float> m_cooldown_left;

        /** Total cooldown seconds per skill index (for the clock overlay). */
        std::unordered_map<int, float> m_cooldown_total;

        /** Layout document holding chrome and geometry; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Parsed openkal.ui/v1 document. */
        mutable UiDocument m_v1;

        /** Display scale (1 = native JSON units). */
        float m_scale = 1.0f;

        /** True while the window itself is being dragged. */
        bool m_dragging = false;

        /** Pointer offset inside the window while dragging, X. */
        float m_drag_dx = 0;

        /** Pointer offset inside the window while dragging, Y. */
        float m_drag_dy = 0;

        /** Catalog the cached tab lists were built from. */
        mutable const Catalog* m_cache_catalog = nullptr;

        /** Class the cached tab lists were built for. */
        mutable int m_cache_clazz = -999;

        /** Tab the cached lists were built for. */
        mutable SkillTab m_cache_tab = SkillTab::Count;

        /** Specialty the cached lists were built for. */
        mutable int m_cache_specialty = -1;

        /** Active skills of the cached tab, in display order. */
        mutable std::vector<const SkillDef*> m_cache_active;

        /** Passive skills of the cached tab, in display order. */
        mutable std::vector<const SkillDef*> m_cache_passive;

        /** Body FreeType face for chrome and tooltips. */
        mutable FreetypeFont m_body_font;

        /** Pixel size last loaded into runtime state. */
        mutable int m_body_font_pixel_size = 0;

        /** Bold FreeType face for the Dragon title. */
        mutable FreetypeFont m_title_font;

        /** Tip FreeType face for hover panels. */
        mutable FreetypeFont m_tip_font;

        /** Pixel size last loaded into runtime state. */
        mutable int m_tip_font_pixel_size = 0;
    };

}  // namespace openkal::client
