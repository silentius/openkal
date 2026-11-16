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
#include "openkal/client/LocationMapView.h"
#include "openkal/client/QuestSnitch.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/client/UiView.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/Quest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Item catalog; full type in ItemDb.h. */
    class ItemDb;

    /** Player bag DTO; full type in Inventory.h. */
    struct Bag;

    /** Dragon quest journal with a data-driven frame and wrapped quest content. */
    class QuestJournalWindow {
      public:
        /** Binds the shared UI layout.
         * @param layout Layout owned by the UI store. */
        void set_layout(const UiLayout* layout);

        /** Binds the compact objective map and runtime catalogs.
         * @param layout Snitch UI layout, owned by the UI store.
         * @param root Runtime configuration root.
         * @param npcs Shared NPC spawns; must outlive this window. */
        void set_snitch_context(const UiLayout* layout,
                                const std::string& root,
                                const std::vector<NpcSpawn>& npcs);

        /** @return True while the compact objective window is visible. */
        bool snitch_open() const {
            return m_snitch_open;
        }

        /** Closes only the objective window.
         * @return True when an open objective window was closed. */
        bool close_snitch();

        /**
         * Formats a journal item-progress line.
         * @param name Item display name.
         * @param have Count currently in the bag.
         * @param need Required count.
         * @return Text such as "Nou-Woong's Money 0 / 1".
         */
        [[nodiscard]] static std::string
        format_item_progress(std::string_view name, int have, int need) {
            return std::string(name) + " " + std::to_string(have) + " / " + std::to_string(need);
        }

        /**
         * Checks whether a required item count is met.
         * @param have Count currently in the bag.
         * @param need Required count.
         * @return True when need is positive and have is at least need.
         */
        [[nodiscard]] static bool item_requirement_met(int have, int need) {
            return need > 0 && have >= need;
        }

        /** @return True while shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Shows or hides. @param open True to show. */
        void set_open(bool open) {
            m_open = open;
            if (!open) {
                m_dragging = false;
                close_snitch();
            }
        }

        /** Toggles visibility. */
        void toggle() {
            m_open = !m_open;
            if (!m_open) {
                m_dragging = false;
                close_snitch();
            }
        }

        /** @return Left. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Top. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return Width. */
        [[nodiscard]] float w() const {
            return m_w;
        }

        /** @return Height. */
        [[nodiscard]] float h() const {
            return m_h;
        }

        /** @return UI scale factor (1 = native pixels). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets the UI scale factor.
         * @param scale Scale factor (1 = native pixels).
         */
        void set_scale(float scale) {
            m_scale = scale;
        }

        /**
         * Centers on screen.
         * @param screen_w Width.
         * @param screen_h Height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /**
         * Replaces active quest entries from network state.
         * @param entries Quest flags.
         * @param tasks Task catalog for titles/descriptions.
         * @param items Item catalog for requirement names.
         */
        void sync_quests(const std::vector<protocol::QuestFlagEntry>& entries,
                         const TaskCatalog& tasks,
                         const ItemDb& items);

        /** Loads or reloads the journal FreeType faces.
         * @param engine Engine used to create atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases owned FreeType atlas textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /**
         * Hit-tests the window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Drags the window while the left button is held on the title strip.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Handles a left click (select / close / drag).
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when consumed.
         */
        [[nodiscard]] bool handle_click(float mx, float my);

        /**
         * Draws the journal.
         * @param engine Engine.
         * @param ui Helpers.
         * @param font Font.
         * @param bag Player bag for live item counts.
         */
        void
        draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font, const Bag& bag);

      private:
        /** Supplies canvas content and footer interaction state to the shared renderer. */
        class ViewHost;
        class SnitchHost;
        void sync_snitch(const Bag& bag);
        UiVarMap snitch_variables() const;
        bool snitch_contains(float mx, float my) const;
        void draw_snitch(engine::IEngine& engine,
                         WidgetTools& ui,
                         const FreetypeFont& body,
                         const FreetypeFont& title) const;

        /** Reloads geometry from the bound document. */
        void sync_from_layout() const;

        /** @return Caption and section-label bindings. */
        [[nodiscard]] UiVarMap variables() const;

        /** Draws wrapped rows and details within the journal canvas.
         * @param engine Engine.
         * @param font Body font.
         * @param bag Bag supplying live item counts.
         * @param x Canvas left.
         * @param y Canvas top.
         * @param width Canvas width.
         * @param height Canvas height.
         * @param scale UI scale. */
        void draw_content(engine::IEngine& engine,
                          const FreetypeFont& font,
                          const Bag& bag,
                          float x,
                          float y,
                          float width,
                          float height,
                          float scale) const;

        /** @return Frame style from the shared document. */
        [[nodiscard]] WindowFrameStyle frame_style() const;

        /** One list row. */
        struct Row {
            /** Quest index. */
            std::uint16_t quest = 0;

            /** Current flag. */
            std::uint8_t flag = 0;

            TaskFlagDef stage; /**< Current objective and destination IDs. */

            /** Required player level for the current flag, or 0. */
            int level_required = 0;

            /** Title. */
            std::string title;

            /** Overall desc. */
            std::string desc;

            /** Current flag desc. */
            std::string flag_desc;

            /** Return NPC description. */
            std::string npc_desc;

            /** Required item item definition index, or 0. */
            int item_index = 0;

            /** Required item count, or 0. */
            int item_need = 0;

            /** Required item display name. */
            std::string item_name;
        };

        /** Child objective map, lazily loaded from optional runtime catalogs. */
        UiDocument m_snitch_document;
        const UiLayout* m_snitch_layout = nullptr;
        const std::vector<NpcSpawn>* m_npcs = nullptr;
        std::string m_configuration_root;
        PowerbookCatalog m_snitch_catalog;
        LocationMapView m_snitch_map;
        std::vector<QuestLocation> m_locations;
        bool m_snitch_loaded = false;
        bool m_snitch_open = false;
        bool m_snitch_dragging = false;
        float m_snitch_x = 0, m_snitch_y = 0;
        float m_snitch_drag_dx = 0, m_snitch_drag_dy = 0;
        int m_location = 0;
        /** Objective identity and bag count used to invalidate destination selection. */
        int m_snitch_have = -1;
        int m_snitch_quest = -1;
        int m_snitch_flag = -1;
        int m_screen_w = 1280, m_screen_h = 900;

        /** Visible. */
        bool m_open = false;

        /** Geometry. */
        float m_x = 40.0f;

        /** Geometry. */
        float m_y = 80.0f;

        /** Geometry. */
        mutable float m_w = 720.0f;

        /** Geometry. */
        mutable float m_h = 440.0f;

        /** UI scale factor (1 = native pixels). */
        float m_scale = 1.0f;

        /** Dedicated title font. */
        FreetypeFont m_title_font;

        /** Dedicated body font. */
        FreetypeFont m_body_font;

        /** Layout owned by the UI store. */
        const UiLayout* m_layout = nullptr;

        /** Parsed layout, refreshed before drawing and hit testing. */
        mutable UiDocument m_document;

        /** Last pointer X for footer artwork. */
        float m_mouse_x = -1.0f;

        /** Last pointer Y for footer artwork. */
        float m_mouse_y = -1.0f;

        /** Last pointer button state for footer artwork. */
        bool m_mouse_down = false;

        /** List rows. */
        std::vector<Row> m_rows;

        /** Selected row index, or -1. */
        int m_selected = -1;

        /** True while the journal is being dragged. */
        bool m_dragging = false;

        /** Pointer offset to keep the client box under the cursor while dragging. */
        float m_drag_dx = 0.0f;

        /** Pointer offset to keep the client box under the cursor while dragging. */
        float m_drag_dy = 0.0f;

        /** Cached native row offsets relative to the canvas, preserved while dragging. */
        mutable std::vector<float> m_row_y;

        /** Cached list-row heights from the last draw. */
        mutable std::vector<float> m_row_h;
    };

}  // namespace openkal::client
