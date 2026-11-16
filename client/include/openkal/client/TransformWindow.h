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
#include "openkal/client/ShinsuDb.h"
#include "openkal/client/Skills.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiView.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <cstdint>
#include <string>

namespace openkal::client {

    /** Outcome of a click offered to the transform window. */
    enum class TransformClickResult {
        Ignored = 0,
        Consumed,
        CastTransform,
        CastAnimal,
        DragBegin,
    };

    /**
     * H-key Ancient animal window: egg stats, draggable sinsu001–003 stage icons, animal skills.
     */
    class TransformWindow {
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
         * @param x Client box left.
         * @param y Client box top.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
        }

        /** @return Display scale. */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the display scale.
         * @param scale Scale (1 = JSON units).
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /**
         * Binds the layout document.
         * @param layout Layout; must outlive the window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /**
         * Binds the skill catalog.
         * @param catalog Catalog; must outlive the window.
         */
        void set_catalog(const Catalog* catalog) {
            m_catalog = catalog;
        }

        /**
         * Binds the item catalog for the egg icon.
         * @param items Item catalog.
         */
        void set_items(const ItemDb* items) {
            m_items = items;
        }

        /**
         * Binds converted shinsu names.
         * @param names Name table.
         */
        void set_shinsu(const ShinsuDb* names) {
            m_shinsu = names;
        }

        /**
         * Sets the equipped egg snapshot.
         * @param equipped True when a sinsu item is worn.
         * @param item_index item definition index.
         * @param race extra.race.
         * @param level Egg level.
         * @param exp Current egg EXP.
         * @param need EXP needed for the next egg level.
         * @param form_stage Current transform stage, or 0.
         */
        void set_egg(
            bool equipped, int item_index, int race, int level, int exp, int need, int form_stage);

        /** @return Last transform-into skill (200–202) or animal skill, or -1. */
        [[nodiscard]] int last_skill_index() const {
            return m_last_skill_index;
        }

        /** @return Skill currently dragged to the skill bar, or -1. */
        [[nodiscard]] int drag_skill_index() const {
            return m_drag_skill_index;
        }

        /**
         * Sets the dragged skill.
         * @param index Skill index, or -1.
         */
        void set_drag_skill_index(int index) {
            m_drag_skill_index = index;
        }

        /** @return True while the window itself is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /** Applies window.w / window.h from the layout. */
        void sync_from_layout();

        /**
         * Loads FreeType faces.
         * @param engine Engine used to create atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases owned FreeType textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Centers the window.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * Tests whether the pointer is over the framed window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when open and under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Applies pointer dragging.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Applies a left click.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return What the window did.
         */
        TransformClickResult handle_click(float mx, float my);

        /**
         * Draws the window chrome and contents (no hover tip).
         * @param engine Engine for untextured rects.
         * @param ui Shared UI helpers.
         */
        void draw(engine::IEngine& engine, WidgetTools& ui) const;

        /**
         * Draws the egg / stage / animal-skill hover overlay on top of the cursor.
         * @param engine Engine for untextured rects.
         * @param ui Shared UI helpers.
         * @param font Tip face.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param player_clazz Local class (0–3).
         * @param player_level Local level.
         */
        void draw_tooltip(engine::IEngine& engine,
                          WidgetTools& ui,
                          const FreetypeFont& font,
                          float screen_w,
                          float screen_h,
                          int player_clazz,
                          int player_level) const;

        /**
         * Draws the dragged skill icon.
         * @param ui Shared UI helpers.
         * @param mx Pointer X.
         * @param my Pointer Y.
         */
        void draw_drag_ghost(WidgetTools& ui, float mx, float my) const;

      private:
        /** Supplies pointer state for the shared dialog buttons. */
        class ViewHost;

        /** True while shown. */
        bool m_open = false;

        /** Client left. */
        float m_x = 80.0f;

        /** Client top. */
        float m_y = 80.0f;

        /** Client width. */
        float m_w = 430.0f;

        /** Client height. */
        float m_h = 270.0f;

        /** Display scale. */
        float m_scale = 1.0f;

        /** Layout; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Parsed openkal.ui/v1 document. */
        mutable UiDocument m_v1;

        /** Skill catalog; not owned. */
        const Catalog* m_catalog = nullptr;

        /** Item catalog; not owned. */
        const ItemDb* m_items = nullptr;

        /** Shinsu names; not owned. */
        const ShinsuDb* m_shinsu = nullptr;

        /** True when an egg is worn. */
        bool m_equipped = false;

        /** Worn egg item definition index. */
        int m_item_index = 0;

        /** extra.race. */
        int m_race = 0;

        /** Egg level. */
        int m_level = 1;

        /** Current egg EXP. */
        int m_exp = 0;

        /** EXP needed for the next egg level. */
        int m_need = 0;

        /** Current form stage, or 0. */
        int m_form_stage = 0;

        /** Last cast skill index. */
        int m_last_skill_index = -1;

        /** Dragged skill index. */
        int m_drag_skill_index = -1;

        /** True while dragging the window. */
        bool m_dragging = false;

        /** Drag offset X. */
        float m_drag_dx = 0;

        /** Drag offset Y. */
        float m_drag_dy = 0;

        /** Hovered icon kind while the pointer is over the window. */
        enum class HoverKind {
            /** No hover target. */
            None,

            /** Worn egg icon. */
            Egg,

            /** Transform stage icon (sinsu001–003). */
            Stage,

            /** Animal skill icon. */
            Animal
        };

        /** Current hover target. */
        HoverKind m_hover_kind = HoverKind::None;

        /** Stage or animal slot index, or -1. */
        int m_hover_index = -1;

        /** Last pointer X. */
        float m_hover_mx = 0;

        /** Last pointer Y. */
        float m_hover_my = 0;

        /** True while the left mouse button is held. */
        bool m_mouse_down = false;

        /** Body font. */
        mutable FreetypeFont m_body_font;

        /** Bold face for the title and section headings. */
        mutable FreetypeFont m_title_font;

        /** Pixel size last loaded. */
        mutable int m_body_font_pixel_size = 0;
    };

}  // namespace openkal::client
