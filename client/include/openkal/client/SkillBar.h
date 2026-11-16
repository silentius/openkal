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
#include "openkal/client/SkillWindow.h"
#include "openkal/client/Skills.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <cstdint>

namespace openkal::client {

    struct Bag;

    /** Outcome of a click on the skill quickslot bar. */
    enum class BarResult {
        Ignored = 0,
        Consumed,
        Cast,
        SetActive,
        UseItem,
        DropItem,
    };

    /**
     * Controls the Dragon HUD active slot and two rows of ten skill/item hotkeys.
     * Slot geometry comes from config/ui/dragon_hud.json when a layout is bound.
     */
    class SkillBar {
      public:
        /**
         * Binds the skill catalog used for icons and cast kinds.
         * @param catalog Catalog; must outlive the bar. Pass nullptr to unbind.
         */
        void set_catalog(const Catalog* catalog) {
            m_catalog = catalog;
        }

        /**
         * Binds the item catalog used for item-shortcut icons and names.
         * @param items Item catalog; must outlive the bar. Pass nullptr to unbind.
         */
        void set_items(const ItemDb* items) {
            m_items = items;
        }

        /** @return Bound item catalog, or nullptr. */
        [[nodiscard]] const ItemDb* items() const {
            return m_items;
        }

        /**
         * Binds the player bag used for item-shortcut counts and world drops.
         * @param bag Bag; must outlive the bar. Pass nullptr to unbind.
         */
        void set_bag(const Bag* bag) {
            m_bag = bag;
        }

        /** @return Bound bag, or nullptr. */
        [[nodiscard]] const Bag* bag() const {
            return m_bag;
        }

        /** @return Bound skill catalog, or nullptr. */
        [[nodiscard]] const Catalog* catalog() const {
            return m_catalog;
        }

        /**
         * Binds player skill state (hotbar slots and active skill).
         * @param player Player skills; must outlive the bar. Pass nullptr to unbind.
         */
        void set_player(PlayerSkills* player) {
            m_player = player;
        }

        /** @return Bound player skill state, or nullptr. */
        [[nodiscard]] PlayerSkills* player() const {
            return m_player;
        }

        /** Binds the skill window used for cooldown dimming.
         * @param window Skill window; must outlive the bar. Pass nullptr to unbind.
         */
        void set_skill_window(const SkillWindow* window) {
            m_skill_window = window;
        }

        /** @return Bound skill window, or nullptr. */
        [[nodiscard]] const SkillWindow* skill_window() const {
            return m_skill_window;
        }

        /**
         * Sets the player class index used for catalog lookups.
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

        /** @return Player class index. */
        [[nodiscard]] int player_clazz() const {
            return m_player_clazz;
        }

        /** @return Skill index of the last Cast result, or -1. */
        [[nodiscard]] int last_cast_index() const {
            return m_last_cast_index;
        }

        /** @return Cast kind of the last Cast result. */
        [[nodiscard]] CastKind last_cast_kind() const {
            return m_last_cast_kind;
        }

        /** @return item definition index of the last UseItem result, or -1. */
        [[nodiscard]] int last_item_index() const {
            return m_last_item_index;
        }

        /** @return Instance id of the last DropItem result, or 0. */
        [[nodiscard]] std::uint32_t last_drop_iid() const {
            return m_last_drop_iid;
        }

        /** @return item definition index of the last DropItem result, or 0. */
        [[nodiscard]] std::int32_t last_drop_index() const {
            return m_last_drop_index;
        }

        /** @return Stack count of the last DropItem result. */
        [[nodiscard]] int last_drop_count() const {
            return m_last_drop_count;
        }

        /**
         * Tests whether the pointer is over the bar panel.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the pointer is inside the panel.
         */
        [[nodiscard]] bool contains(float mx, float my) const;

        /**
         * Binds the layout document holding slot geometry.
         * @param layout Layout; must outlive the bar. Pass nullptr for baked
         * defaults.
         */
        void set_layout(const UiLayout* layout);

        /** @return Bound layout document, or nullptr. */
        [[nodiscard]] const UiLayout* layout() const {
            return m_layout;
        }

        /** @return Panel left in pixels. */
        [[nodiscard]] float panel_x() const {
            return m_panel_x;
        }

        /** @return Panel top in pixels. */
        [[nodiscard]] float panel_y() const {
            return m_panel_y;
        }

        /** @return Panel width in pixels. */
        [[nodiscard]] float panel_w() const {
            return m_panel_w;
        }

        /** @return Panel height in pixels. */
        [[nodiscard]] float panel_h() const {
            return m_panel_h;
        }

        /** @return Display scale (1 = native JSON units). */
        [[nodiscard]] float scale() const {
            return m_scale;
        }

        /**
         * Sets panel placement and display scale.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param s Display scale.
         */
        void set_panel(float x, float y, float w, float h, float s);

        /**
         * Fills the active-slot icon rect.
         * @param ix Out X.
         * @param iy Out Y.
         * @param iw Out W.
         * @param ih Out H.
         * @return True when layout resolved a rect.
         */
        [[nodiscard]] bool active_rect(float& ix, float& iy, float& iw, float& ih) const;

        /**
         * Fills a hotbar slot icon rect.
         * @param slot Hotbar index 0..19.
         * @param ix Out X.
         * @param iy Out Y.
         * @param iw Out W.
         * @param ih Out H.
         * @return True when layout resolved a rect.
         */
        [[nodiscard]] bool hotbar_rect(int slot, float& ix, float& iy, float& iw, float& ih) const;

        /**
         * Hits a hotbar slot under the pointer.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return Slot index, or -1.
         */
        [[nodiscard]] int hit_hotbar(float mx, float my) const;

        /**
         * Tests whether the pointer is over the active slot.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when over the active slot.
         */
        [[nodiscard]] bool hit_active(float mx, float my) const;

        /** Fills the Auto-HP icon rectangle.
         * @param ix Out X.
         * @param iy Out Y.
         * @param iw Out width.
         * @param ih Out height.
         * @return True when the rectangle is available.
         */
        [[nodiscard]] bool auto_hp_rect(float& ix, float& iy, float& iw, float& ih) const;

        /** Tests the Auto-HP slot, including its frame.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when over the slot.
         */
        [[nodiscard]] bool hit_auto_hp(float mx, float my) const;

        /** @return Selected Auto-HP medicine index, or 0 for an empty slot. */
        int auto_hp_item_index() const {
            return m_auto_hp_item_index;
        }

        /** @return Configured HP threshold in percent. */
        int auto_hp_percent() const {
            return m_auto_hp_percent;
        }

        /** Restores saved preferences after the item catalog is available.
         * Unknown or ineligible items clear the slot; invalid thresholds use 75.
         * @param item_index Saved item definition index, independent of bag contents.
         * @param percent Saved HP threshold.
         */
        void restore_auto_hp(int item_index, int percent);

        /** Clears the Auto-HP preferences change flag.
         * @return True when binding or threshold changed since the last call.
         */
        [[nodiscard]] bool take_auto_hp_dirty();

        /** @return True when the selected medicine has an available bag stack. */
        bool auto_hp_available() const;

        /** Tests whether automatic medicine use is needed and available.
         * @param hp Current HP.
         * @param max_hp Maximum HP.
         * @return True when alive, below the threshold, stocked and off cooldown.
         */
        [[nodiscard]] bool auto_hp_needed(std::int32_t hp, std::int32_t max_hp) const;

        /**
         * Drops a dragged skill or item binding onto the slot under the pointer.
         * Skills may land on the active mouse slot; Auto-HP medicine only binds
         * the Auto-HP slot. Other items bind hotbar slots.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param skill_index Skill index or packed item binding.
         * @return True when a slot accepted the drop.
         */
        [[nodiscard]] bool try_drop(float mx, float my, int skill_index);

        /**
         * Starts a potential hotbar drag or click on left-button down.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return True when the bar consumed the press.
         */
        [[nodiscard]] bool begin_press(float mx, float my);

        /**
         * Promotes a press into a drag once the pointer moves past the threshold.
         * @param in Frame input.
         */
        void update_pointer(const engine::FrameInput& in);

        /**
         * Completes a hotbar press: swap on drag, drop an item, or activate/cast.
         * Skills dragged off the bar are unbound. Items dragged onto the open
         * inventory are unbound (the stack stays in the bag); items dragged onto
         * the world are dropped from the bag.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param over_inventory True when the pointer is over the open inventory.
         * @return What the bar did with the release.
         */
        [[nodiscard]] BarResult handle_release(float mx, float my, bool over_inventory = false);

        /**
         * Applies a left click on the bar.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @return What the bar did with the click.
         */
        [[nodiscard]] BarResult handle_click(float mx, float my);

        /**
         * Sets the usable-item cooldown overlay for the item that was just used.
         * HP/MP refresh items share the overlay with each other; other items only
         * overlay their own hotbar icon.
         * @param fraction Remaining cooldown as 0..1 (1 = just started).
         * @param item_index item definition index that started the cooldown, or -1.
         */
        void set_item_refresh_cooldown(float fraction, int item_index = -1);

        /**
         * Draws skill icons over the quickslot panel.
         * @param ui Shared UI helpers.
         * @param count_font Optional FreeType face for item-stack counts.
         */
        void draw(WidgetTools& ui, const FreetypeFont* count_font = nullptr) const;

        /** Updates the authoritative Deathblow resource shown on Brutal Attack.
         * @param points Server-reported points in the range 0..5.
         */
        void set_mortal_points(int points);

        /**
         * Draws the dragged hotbar icon under the cursor.
         * @param ui Shared UI helpers.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param count_font Optional FreeType face for item-stack counts.
         */
        void draw_drag_ghost(WidgetTools& ui,
                             float mx,
                             float my,
                             const FreetypeFont* count_font = nullptr) const;

        /**
         * Draws the compatibility behavior hover tip (skill name + description) next to the cursor.
         * @param ui Shared UI helpers (chat-box overlay chrome).
         * @param font FreeType tip face.
         * @param screen_w Frame width used to clamp the tip.
         * @param screen_h Frame height used to clamp the tip.
         */
        void draw_tooltip(WidgetTools& ui,
                          const FreetypeFont& font,
                          float screen_w,
                          float screen_h) const;

        /** @return True while a hotbar or active-slot press or drag is in progress. */
        [[nodiscard]] bool interacting() const {
            return m_press_slot != -1 || m_drag_slot >= 0;
        }

      private:
        /** Auto-HP uses the next slot index for pointer capture and dragging. */
        static constexpr int kAutoHpSlot = static_cast<int>(PlayerSkills::kHotbarSlots);

        /** Selected medicine type; inventory instances remain in the bag. */
        int m_auto_hp_item_index = 0;

        /** Trigger threshold, cycled by double-clicking the selected icon. */
        int m_auto_hp_percent = 75;

        /** True when an interaction changed preferences that must be saved. */
        bool m_auto_hp_dirty = false;

        /** Seconds since the first Auto-HP click; negative means no pending click. */
        float m_auto_hp_click_age = -1.0f;

        int m_mortal_points = 0;
        /**
         * Casts or sets-active the binding that was pressed, without a second hit-test.
         * @param binding Skill index or packed item shortcut.
         * @return Cast, SetActive, UseItem, or Consumed.
         */
        [[nodiscard]] BarResult activate_binding(int binding);

        /**
         * Tests whether a hotbar item should show the current item cooldown overlay.
         * @param item_index item definition index bound to the slot.
         * @return True when this icon shares the cooldown of the item that was used.
         */
        [[nodiscard]] bool item_shows_refresh_cd(int item_index) const;

        /** Skill catalog used for icons and cast kinds; not owned. */
        const Catalog* m_catalog = nullptr;

        /** Item catalog used for item-shortcut icons; not owned. */
        const ItemDb* m_items = nullptr;

        /** Player bag used for item counts and world drops; not owned. */
        const Bag* m_bag = nullptr;

        /** Player skill state; not owned. */
        PlayerSkills* m_player = nullptr;

        /** Skill window used for cooldown dimming; not owned. */
        const SkillWindow* m_skill_window = nullptr;

        /** Player class index used for catalog lookups. */
        int m_player_clazz = 0;

        /** Skill index of the last Cast result, or -1. */
        int m_last_cast_index = -1;

        /** Cast kind of the last Cast result. */
        CastKind m_last_cast_kind = CastKind::None;

        /** item definition index of the last UseItem result, or -1. */
        int m_last_item_index = -1;

        /** Instance id of the last DropItem result, or 0. */
        std::uint32_t m_last_drop_iid = 0;

        /** item definition index of the last DropItem result, or 0. */
        std::int32_t m_last_drop_index = 0;

        /** Stack count of the last DropItem result. */
        int m_last_drop_count = 0;

        /** Remaining refresh-item cooldown as 0..1 for icon overlay. */
        float m_item_refresh_cd = 0.0f;

        /** item definition index that started the item cooldown overlay, or -1. */
        int m_item_refresh_cd_index = -1;

        /** Layout document holding slot geometry; not owned. */
        const UiLayout* m_layout = nullptr;

        /** Parsed geometry, refreshed when the bound JSON changes. */
        UiDocument m_document;

        /** Panel left in pixels. */
        float m_panel_x = 0;

        /** Panel top in pixels. */
        float m_panel_y = 0;

        /** Panel width in pixels. */
        float m_panel_w = 792;

        /** Panel height in pixels. */
        float m_panel_h = 180;

        /** Display scale (1 = native JSON units). */
        float m_scale = 1.0f;

        /** Hotbar slot captured on LMB down, -2 for the active slot, or -1. */
        int m_press_slot = -1;

        /** Pointer X when the hotbar slot was pressed. */
        float m_press_mx = 0;

        /** Pointer Y when the hotbar slot was pressed. */
        float m_press_my = 0;

        /** Hotbar slot currently dragged, or -1. */
        int m_drag_slot = -1;

        /** Skill or packed item binding of the dragged hotbar icon, or -1. */
        int m_drag_skill = -1;

        /** Character stats for ability tooltip placeholders. */
        SkillAbilityVars m_ability_vars;

        /** Skill or packed item binding under the pointer for the hover tip, or -1. */
        int m_hover_skill = -1;

        /** Pointer X captured for the hover tip. */
        float m_hover_mx = 0;

        /** Pointer Y captured for the hover tip. */
        float m_hover_my = 0;
    };

}  // namespace openkal::client
