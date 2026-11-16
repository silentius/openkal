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

#include "openkal/client/DialogCatalog.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiView.h"
#include "openkal/engine/engine.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::client {

    /** Click result for the NPC talk window. */
    enum class NpcTalkClickResult {
        Ignored = 0,
        Consumed,
        Close,
        OpenDialog,
        QuestLink,
        SaveRevival,
        VillageRevival,
        NpcTeleport,
        Blacksmith,
        Mixing,
        MixingList,
        Shop,
        SkinWeapons,
        SkinFull,
        SkinParts,
        Storage,
        SpeedMedicine,
        CraftMedicine,
        CraftMagicPotion,
        CraftAutoMedicine,
        Stablemaster,
        Guild,
        GuildCreate,
        TradingPostCollection,
    };

    /** Dragon NPC dialog window driven by DialogCatalog entries. */
    class NpcTalkWindow : private UiViewHost {
      public:
        /** @return True while shown. */
        [[nodiscard]] bool open() const {
            return m_open;
        }

        /** Shows or hides the window. @param open True to show. */
        void set_open(bool open) {
            if (!open && !m_closable) {
                return;
            }
            m_open = open;
            if (!open) {
                m_dragging = false;
            }
        }

        /** Closes the window. */
        void close() {
            if (!m_closable) {
                return;
            }
            m_open = false;
            m_dragging = false;
        }

        /** Dismisses an NPC conversation when its source is no longer interactable.
         * @param available True when the source NPC exists within direct interaction range.
         * @return True when an open NPC dialog was dismissed; local dialogs are preserved.
         */
        bool close_if_npc_unavailable(bool available);

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

        /** Binds the shared conversation layout.
         * @param layout Layout; must outlive this window.
         */
        void set_layout(const UiLayout* layout) {
            m_layout = layout;
        }

        /**
         * Centers on screen.
         * @param screen_w Width.
         * @param screen_h Height.
         */
        void center_on_screen(float screen_w, float screen_h);

        /** Opens a dialog definition, resolving text tokens before layout.
         * @param dialog Dialog content; copied.
         * @param npc_id Source NPC entity id, or zero for a local dialog.
         * @param player_name Active character name; empty uses "Player".
         */
        void show_dialog(const DialogDef& dialog,
                         std::uint32_t npc_id = 0,
                         std::string_view player_name = {});

        /**
         * Controls whether the active dialog may be closed by the player.
         * @param closable True to allow the close control and Escape key.
         */
        void set_closable(bool closable) {
            m_closable = closable;
        }

        /**
         * Changes the displayed caption without reopening the window.
         * @param caption New caption.
         */
        void set_caption(std::string caption);

        /**
         * Loads or reloads the title FreeType face.
         * @param engine Engine used to create atlas textures.
         */
        void sync_fonts(engine::IEngine& engine);

        /**
         * Releases owned FreeType atlas textures.
         * @param engine Engine that owns the textures.
         */
        void destroy_fonts(engine::IEngine& engine);

        /** @return Last selected quest index from a quest link. */
        [[nodiscard]] std::uint16_t pending_quest_index() const {
            return m_pending_quest_index;
        }

        /** @return Last selected quest flag from a quest link. */
        [[nodiscard]] std::uint8_t pending_quest_flag() const {
            return m_pending_quest_flag;
        }

        /** @return Pending mixing mode (0 elemental, 1 imperial). */
        [[nodiscard]] std::uint8_t pending_mixing_mode() const {
            return m_pending_mixing_mode;
        }

        /** @return Pending server revival-point id. */
        [[nodiscard]] std::uint8_t pending_revival_point() const {
            return m_pending_revival_point;
        }

        /** @return Pending next dialog id when OpenDialog was selected. */
        [[nodiscard]] std::int32_t pending_dialog_id() const {
            return m_pending_dialog_id;
        }

        /** @return NPC that supplied the open dialog, or zero for a local dialog. */
        [[nodiscard]] std::uint32_t npc_id() const {
            return m_npc_id;
        }

        /**
         * Hit-tests the window.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when under the pointer.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /** @return True while the window itself is being dragged. */
        [[nodiscard]] bool dragging() const {
            return m_dragging;
        }

        /**
         * Applies one frame of pointer input for title-bar dragging.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Handles a left click.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Click result.
         */
        [[nodiscard]] NpcTalkClickResult handle_click(float mx, float my);

        /**
         * Draws the talk window.
         * @param engine Engine.
         * @param ui Widget helpers.
         * @param font Body font.
         */
        void draw(engine::IEngine& engine, WidgetTools& ui, const FreetypeFont& font) const;

      private:
        /** A body line or selectable response positioned within one content page. */
        struct ContentRow {
            std::vector<std::string> lines;
            std::size_t link_index = 0;
            bool response = false;
            float y = 0.0f;
            float h = 0.0f;
        };

        /** Wraps and paginates content in native client coordinates.
         * @param font Measured FreeType face, or null before the first draw.
         */
        void layout_content(const FreetypeFont* font) const;

        /** @param action Navigation action. @return Whether its destination exists. */
        [[nodiscard]] bool button_enabled(std::string_view action) const override;

        /** @return Shared Dragon conversation frame. */
        [[nodiscard]] WindowFrameStyle frame_style() const;

        /** @return Caption, close policy and theme bindings. */
        [[nodiscard]] UiVarMap variables() const;

        /** Shared frame layout. */
        const UiLayout* m_layout = nullptr;

        /** Parsed layout, refreshed when the source changes. */
        mutable UiDocument m_document;

        /** Pointer state for response button feedback. */
        float m_pointer_x = 0.0f;
        float m_pointer_y = 0.0f;
        bool m_pointer_pressed = false;

        /** Visible. */
        bool m_open = false;

        /** True when the player may dismiss the active dialog. */
        bool m_closable = true;

        /** Geometry. */
        float m_x = 40.0f;

        /** Geometry. */
        float m_y = 80.0f;

        /** Geometry. */
        float m_w = 450.0f;

        /** Geometry. */
        float m_h = 360.0f;

        /** UI scale factor (1 = native pixels). */
        float m_scale = 1.0f;

        /** Caption. */
        std::string m_caption = "Talk";

        /** Dedicated bold title font. */
        FreetypeFont m_title_font;

        /** Body lines. */
        std::vector<std::string> m_lines;

        /** Links. */
        std::vector<DialogLink> m_links;

        /** Pending quest link index. */
        std::uint16_t m_pending_quest_index = 0;

        /** Pending quest link flag. */
        std::uint8_t m_pending_quest_flag = 0;

        /** Pending mixing mode (0 elemental, 1 imperial). */
        std::uint8_t m_pending_mixing_mode = 0;

        /** Pending server revival-point id. */
        std::uint8_t m_pending_revival_point = 0;

        /** Pending next dialog id selected by numeric link type. */
        std::int32_t m_pending_dialog_id = 0;

        /** NPC that supplied the active dialog. */
        std::uint32_t m_npc_id = 0;

        /** True while the talk window is being dragged. */
        bool m_dragging = false;

        /** Pointer offset to keep the client box under the cursor while dragging. */
        float m_drag_dx = 0.0f;

        /** Pointer offset to keep the client box under the cursor while dragging. */
        float m_drag_dy = 0.0f;

        /** Wrapped content pages in native coordinates, independent of dragging and scale. */
        mutable std::vector<std::vector<ContentRow>> m_pages;

        /** Currently displayed content page. */
        mutable std::size_t m_page = 0;
    };

}  // namespace openkal::client
