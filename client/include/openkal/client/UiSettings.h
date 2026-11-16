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

#include "openkal/client/ChatFilterModel.h"
#include "openkal/client/GameHud.h"

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace openkal::client {

    /** Per-character Auto-HP binding, independent of bag stack instances. */
    struct AutoHpSettings {
        /** Item definition index, or 0 for an empty slot. */
        int item_index = 0;

        /** HP threshold in percent (25, 50, 75 or 100). */
        int percent = 75;
    };

    /** Saved logical position of one HUD panel (screen coordinates divided by the
     * UI scale). */
    class HudPanelPos {
      public:
        /** @return Logical X. */
        [[nodiscard]] float x() const {
            return m_x;
        }

        /** @return Logical Y. */
        [[nodiscard]] float y() const {
            return m_y;
        }

        /** @return True when a position was stored for this panel. */
        [[nodiscard]] bool has_pos() const {
            return m_has_pos;
        }

        /**
         * Stores a logical position and marks the entry as present.
         * @param x Logical X.
         * @param y Logical Y.
         */
        void set_pos(float x, float y) {
            m_x = x;
            m_y = y;
            m_has_pos = true;
        }

      private:
        /** Logical X. */
        float m_x = 0;

        /** Logical Y. */
        float m_y = 0;

        /** True when a position was stored for this panel. */
        bool m_has_pos = false;
    };

    /** Saved HUD arrangement for one character. */
    class HudCharacterLayout {
      public:
        /** @return Saved logical party-list position, independent of membership. */
        const HudPanelPos& party_position() const {
            return m_party_position;
        }

        /** Stores the party-list anchor.
         * @param x Logical X. @param y Logical Y. */
        void set_party_position(float x, float y) {
            m_party_position.set_pos(x, y);
        }

        /** @return Saved Auto-HP binding and threshold. */
        const AutoHpSettings& auto_hp() const {
            return m_auto_hp;
        }

        /** Stores the Auto-HP preferences.
         * @param settings Item definition index and HP threshold.
         */
        void set_auto_hp(AutoHpSettings settings) {
            m_auto_hp = settings;
        }

        /** @return Character name, kept for readability of the settings file. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the character name.
         * @param name Character name.
         */
        void set_name(std::string name) {
            m_name = std::move(name);
        }

        /**
         * Reads one panel position.
         * @param id Panel to read.
         * @return Stored logical position; has_pos() is false when unset.
         */
        [[nodiscard]] const HudPanelPos& panel(HudPanelId id) const {
            return m_panels[static_cast<std::size_t>(id)];
        }

        /**
         * Stores one panel position.
         * @param id Panel to store.
         * @param x Logical X.
         * @param y Logical Y.
         */
        void set_panel_pos(HudPanelId id, float x, float y) {
            m_panels[static_cast<std::size_t>(id)].set_pos(x, y);
        }

        /** @return Chat filter model for this character. */
        [[nodiscard]] const ChatFilterModel& chat_filters() const {
            return m_chat_filters;
        }

        /** @return Mutable chat filter model. */
        [[nodiscard]] ChatFilterModel& chat_filters() {
            return m_chat_filters;
        }

        /** @return Locally read mail identifiers. */
        const std::set<std::uint32_t>& read_mail_ids() const {
            return m_read_mail_ids;
        }

        /** Replaces the remembered read mail.
         * @param ids Durable mail identifiers.
         */
        void set_read_mail_ids(std::set<std::uint32_t> ids) {
            m_read_mail_ids = std::move(ids);
        }

      private:
        /** Preferred anchor, retained when the growing list is temporarily clamped. */
        HudPanelPos m_party_position;

        /** Auto-HP preferences retained even while the medicine is depleted. */
        AutoHpSettings m_auto_hp;

        std::set<std::uint32_t> m_read_mail_ids;
        /** Character name. */
        std::string m_name;

        /** Logical panel positions indexed by HudPanelId. */
        std::array<HudPanelPos, static_cast<std::size_t>(HudPanelId::Count)> m_panels{};

        /** Per-character chat channel / battle toggles. */
        ChatFilterModel m_chat_filters;
    };

    /**
     * Shared client UI config (JSON): global scale plus per-character HUD panel
     * layouts.
     *
     * The file lives at &lt;save_dir&gt;/ui_settings.json. Panel positions are
     * stored logically (screen position divided by the UI scale) so a saved
     * arrangement survives resolution and scale changes.
     */
    class UiSettings {
      public:
        /** Reads a character's party-list anchor.
         * @param character_id Character whose position to return.
         * @return Logical position; has_pos() is false for older or missing settings. */
        const HudPanelPos& party_position(std::int32_t character_id) const;

        /** Stores a finite logical party-list position.
         * @param character_id Character whose position to update.
         * @param x Logical X. @param y Logical Y. */
        void set_party_position(std::int32_t character_id, float x, float y);

        /** @return True when the scale follows the framebuffer height instead of
         * manual_scale(). */
        [[nodiscard]] bool scale_auto() const {
            return m_scale_auto;
        }

        /** @return Fixed scale used when scale_auto() is false. */
        [[nodiscard]] float manual_scale() const {
            return m_manual_scale;
        }

        /** @return Framebuffer height that maps to scale 1 in auto mode. */
        [[nodiscard]] float reference_height() const {
            return m_reference_height;
        }

        /** @return Lower clamp applied to the resolved scale. */
        [[nodiscard]] float min_scale() const {
            return m_min_scale;
        }

        /** @return Upper clamp applied to the resolved scale. */
        [[nodiscard]] float max_scale() const {
            return m_max_scale;
        }

        /**
         * Switches between automatic and fixed scaling.
         * @param automatic True to derive the scale from the framebuffer height.
         */
        void set_scale_auto(bool automatic) {
            m_scale_auto = automatic;
        }

        /**
         * Sets the fixed scale used when automatic scaling is off.
         * @param scale Fixed scale.
         */
        void set_manual_scale(float scale) {
            m_manual_scale = scale;
        }

        /**
         * Loads settings from disk, replacing everything held in memory.
         * @param path Path to ui_settings.json.
         * @return True when the file parsed.
         */
        [[nodiscard]] bool load(const std::string& path);

        /**
         * Writes the current settings to disk.
         * @param path Path to ui_settings.json.
         * @return True when the file was written.
         */
        [[nodiscard]] bool save(const std::string& path) const;

        /**
         * Resolves the effective UI scale for a framebuffer height.
         * @param framebuffer_height Current framebuffer height.
         * @return Scale clamped to [min_scale(), max_scale()].
         */
        [[nodiscard]] float resolve_scale(float framebuffer_height) const;

        /**
         * Applies a character's saved logical positions to the HUD. Panels without a
         * saved position keep the defaults computed by the HUD.
         * @param hud HUD to reposition.
         * @param character_id Character whose layout to apply.
         * @param scale Scale the logical positions are multiplied by.
         */
        void apply_character(GameHud& hud, std::int32_t character_id, float scale) const;

        /**
         * Captures the HUD's current panel positions as logical coordinates for a
         * character.
         * @param hud HUD to read.
         * @param character_id Character to store the layout under.
         * @param name Character name, stored for readability.
         * @param scale Scale the screen positions are divided by.
         */
        void capture_character(const GameHud& hud,
                               std::int32_t character_id,
                               const std::string& name,
                               float scale);

        /** Reads a character's locally remembered mail state.
         * @param character_id Character whose read state to return.
         * @return Read mail identifiers, or an empty set for a new character.
         */
        const std::set<std::uint32_t>& read_mail_ids(std::int32_t character_id) const;

        /** Stores a character's read mail state.
         * @param character_id Character whose state to update.
         * @param ids Durable identifiers of read messages.
         */
        void set_read_mail_ids(std::int32_t character_id, std::set<std::uint32_t> ids);

        /** Reads a character's Auto-HP preferences.
         * @param character_id Character whose preferences to return.
         * @return Saved preferences, or an empty slot with a 75 percent threshold.
         */
        const AutoHpSettings& auto_hp(std::int32_t character_id) const;

        /** Stores a character's Auto-HP preferences.
         * @param character_id Character whose preferences to update.
         * @param settings Item definition index and HP threshold.
         */
        void set_auto_hp(std::int32_t character_id, AutoHpSettings settings);

        /**
         * Maps a panel id to its KalOnline settings key.
         * @param id Panel id.
         * @return Stable key such as "status" or "destinfo".
         */
        [[nodiscard]] static const char* panel_key(HudPanelId id);

        /**
         * Maps a settings key back to a panel id, accepting both the KalOnline and
         * short names.
         * @param key Key read from the settings file.
         * @param out Out panel id, untouched when the key is unknown.
         * @return True when the key was recognised.
         */
        [[nodiscard]] static bool panel_id_from_key(const std::string& key, HudPanelId& out);

      private:
        /** True when the scale follows the framebuffer height. */
        bool m_scale_auto = true;

        /** Fixed scale used when automatic scaling is off. */
        float m_manual_scale = 2.0f;

        /** Framebuffer height that maps to scale 1 in auto mode. */
        float m_reference_height = 1080.0f;

        /** Lower clamp applied to the resolved scale. */
        float m_min_scale = 1.0f;

        /** Upper clamp applied to the resolved scale. */
        float m_max_scale = 3.0f;

        /** Saved HUD arrangements keyed by character id. */
        std::unordered_map<std::int32_t, HudCharacterLayout> m_characters;
    };

}  // namespace openkal::client
