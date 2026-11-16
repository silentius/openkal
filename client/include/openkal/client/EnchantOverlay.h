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
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"

#include <cstdint>

namespace openkal::client {

    /**
     * Charge bar after dropping a talisman, then a TTF success/fail banner.
     * Matches View_EnchantItem: lock input until the bar finishes, then
     * send C2S_ENCHANTITEM.
     */
    class EnchantOverlay {
      public:
        /**
         * Starts the 2s charge bar. Ignores a second drop while already charging.
         * @param from_id Talisman instance id.
         * @param to_id Destination gear instance id.
         */
        void begin_charge(std::uint32_t from_id, std::uint32_t to_id);

        /**
         * Starts the 2s charge bar for C2S_UPGRADEITEM.
         * @param iid Weapon instance id.
         * @param rate_up True for UI_RATEUP, false for UI_LEVEL.
         */
        void begin_upgrade_charge(std::uint32_t iid, bool rate_up);

        /**
         * Starts a charge bar for C2S_MIXING. Fill uses the same panel; duration
         * only changes how long the bar takes to complete.
         * @param duration_sec Charge length in seconds (must be > 0).
         * @param mode Mixing kind (0 upgrade, 1 making).
         * @param weapon_iid Bag iid of the weapon instance.
         * @param extra_weapon_iid Bag iid of the extra weapon.
         * @param armor_iid Bag iid of the material armor instance.
         * @param stone1_iid Bag iid of the first stone instance.
         * @param stone2_iid Bag iid of the second stone instance (0 unless making or G3).
         */
        void begin_mix_charge(float duration_sec,
                              std::uint8_t mode,
                              std::uint32_t weapon_iid,
                              std::uint32_t extra_weapon_iid,
                              std::uint32_t armor_iid,
                              std::uint32_t stone1_iid,
                              std::uint32_t stone2_iid);

        /** @return True when the pending charge is a Mix Master mix. */
        [[nodiscard]] bool mix_charge() const {
            return m_mix_charge;
        }

        /** @return Mixing kind captured at begin_mix_charge. */
        [[nodiscard]] std::uint8_t mix_mode() const {
            return m_mix_mode;
        }

        /** @return Weapon iid captured at begin_mix_charge. */
        [[nodiscard]] std::uint32_t mix_weapon_iid() const {
            return m_mix_weapon_iid;
        }

        /** @return Extra weapon iid captured at begin_mix_charge. */
        [[nodiscard]] std::uint32_t mix_extra_weapon_iid() const {
            return m_mix_extra_weapon_iid;
        }

        /** @return Armor iid captured at begin_mix_charge. */
        [[nodiscard]] std::uint32_t mix_armor_iid() const {
            return m_mix_armor_iid;
        }

        /** @return First stone iid captured at begin_mix_charge. */
        [[nodiscard]] std::uint32_t mix_stone1_iid() const {
            return m_mix_stone1_iid;
        }

        /** @return Second stone iid captured at begin_mix_charge. */
        [[nodiscard]] std::uint32_t mix_stone2_iid() const {
            return m_mix_stone2_iid;
        }

        /** @return True when the pending charge is a blacksmith upgrade. */
        [[nodiscard]] bool upgrade_charge() const {
            return m_upgrade_charge;
        }

        /** @return True when the upgrade charge is UI_RATEUP. */
        [[nodiscard]] bool upgrade_rate_up() const {
            return m_upgrade_rate_up;
        }

        /** Shows a green Enchanting Success banner. */
        void show_success();

        /** Shows a red Enchanting Failed banner. */
        void show_failed();

        /**
         * Advances the charge bar and banner timers.
         * @param dt Elapsed seconds.
         */
        void tick(float dt);

        /**
         * Draws the charge bar or the result banner.
         * @param ui Shared UI helpers (chat-box overlay chrome).
         * @param engine Engine used to submit UI quads.
         * @param font TrueType font for the banner.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void draw(WidgetTools& ui,
                  engine::IEngine& engine,
                  const FreetypeFont& font,
                  float screen_w,
                  float screen_h) const;

        /** @return True while the charge bar or result banner is on screen. */
        [[nodiscard]] bool visible() const {
            return m_charging || m_active;
        }

        /**
         * @return True while the charge bar is running. UI and movement must stay
         * locked.
         */
        [[nodiscard]] bool locking() const {
            return m_charging;
        }

        /**
         * Consumes the one-shot flag set when the charge bar finishes.
         * @return True once after the charge delay, then false until the next charge.
         */
        [[nodiscard]] bool take_charge_complete();

        /** @return Talisman instance id captured at begin_charge. */
        [[nodiscard]] std::uint32_t charge_from_id() const {
            return m_from_id;
        }

        /** @return Destination gear instance id captured at begin_charge. */
        [[nodiscard]] std::uint32_t charge_to_id() const {
            return m_to_id;
        }

      private:
        /**
         * Draws the locked charge panel and fill bar.
         * @param ui Shared UI helpers.
         * @param engine Engine used to submit UI quads.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         */
        void
        draw_charge(WidgetTools& ui, engine::IEngine& engine, float screen_w, float screen_h) const;

        /**
         * Starts a charge of the given length. Ignores a second start while charging.
         * @param duration_sec Charge length in seconds (clamped to > 0).
         * @return True when the charge started.
         */
        [[nodiscard]] bool start_charge(float duration_sec);

        /** Seconds elapsed on the current result banner. */
        float m_elapsed = 0.0f;

        /** Seconds elapsed on the current charge bar. */
        float m_charge_elapsed = 0.0f;

        /** Charge length in seconds for the current bar (fill = elapsed / this). */
        float m_charge_sec = 2.0f;

        /** True while the result banner animation is running. */
        bool m_active = false;

        /** True while the charge bar is filling. */
        bool m_charging = false;

        /** True once after the charge bar finishes, until take_charge_complete. */
        bool m_charge_ready = false;

        /** True when the current banner is a success. */
        bool m_success = false;

        /** Talisman instance id to send after the charge. */
        std::uint32_t m_from_id = 0;

        /** Destination gear instance id to send after the charge. */
        std::uint32_t m_to_id = 0;

        /** True when the charge should send C2S_UPGRADEITEM. */
        bool m_upgrade_charge = false;

        /** True when the upgrade charge is UI_RATEUP. */
        bool m_upgrade_rate_up = false;

        /** True when the charge should send C2S_MIXING. */
        bool m_mix_charge = false;

        /** Mixing kind captured at begin_mix_charge. */
        std::uint8_t m_mix_mode = 0;

        /** Weapon iid captured at begin_mix_charge. */
        std::uint32_t m_mix_weapon_iid = 0;

        /** Extra weapon iid captured at begin_mix_charge. */
        std::uint32_t m_mix_extra_weapon_iid = 0;

        /** Armor iid captured at begin_mix_charge. */
        std::uint32_t m_mix_armor_iid = 0;

        /** First stone iid captured at begin_mix_charge. */
        std::uint32_t m_mix_stone1_iid = 0;

        /** Second stone iid captured at begin_mix_charge. */
        std::uint32_t m_mix_stone2_iid = 0;
    };

}  // namespace openkal::client
