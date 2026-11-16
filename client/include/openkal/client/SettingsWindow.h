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
#include "openkal/client/UiDocument.h"
#include "openkal/settings/ClientSettings.h"

namespace openkal::engine {
    class IEngine;
    struct FrameInput;
}  // namespace openkal::engine

namespace openkal::client {

    class FreetypeFont;
    class UiLayout;
    class WidgetTools;

    /** Result of input handled by the in-game settings window. */
    enum class SettingsWindowAction {
        Ignored,
        Consumed,
        Save,
        Close,
    };

    /** Data-driven editor for a draft ClientSettings value. */
    class SettingsWindow final {
      public:
        /** Binds the data-driven settings layout.
         * @param layout Layout that outlives this window.
         */
        void set_layout(const UiLayout* layout);

        /** Sets the display scale.
         * @param scale Display scale where 1 is one layout pixel.
         * @param screen_w Available framebuffer width, or zero to keep the last viewport.
         * @param screen_h Available framebuffer height, or zero to keep the last viewport.
         */
        void set_scale(float scale, float screen_w = 0.0f, float screen_h = 0.0f);

        /** Opens a centered draft without modifying the source value.
         * @param settings Initial settings.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param filters Initial character message filters.
         * @param character_available Whether character-specific filters can be edited.
         */
        void show(const settings::ClientSettings& settings,
                  float screen_w,
                  float screen_h,
                  const ChatFilterModel& filters = ChatFilterModel{},
                  bool character_available = true);

        /** Closes the draft. */
        void close();

        /** @return True while the settings window is visible. */
        [[nodiscard]] bool open() const;

        /** Handles one left click.
         * @param mouse_x Pointer X.
         * @param mouse_y Pointer Y.
         * @param ui Artwork helper used for button dimensions.
         * @return Window action.
         */
        [[nodiscard]] SettingsWindowAction
        handle_click(float mouse_x, float mouse_y, WidgetTools* ui = nullptr);

        /** Tests whether a point is over the framed window.
         * @param mouse_x Pointer X.
         * @param mouse_y Pointer Y.
         * @return True when contained.
         */
        [[nodiscard]] bool contains(float mouse_x, float mouse_y) const;

        /** Draws the settings draft using TrueType text.
         * @param engine Rendering engine.
         * @param ui UI artwork helper.
         * @param font TrueType font.
         * @param input Optional pointer state for button feedback.
         */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  const engine::FrameInput* input = nullptr) const;

        /** @return Validated draft settings. */
        [[nodiscard]] const settings::ClientSettings& settings() const;

        /** @return Draft character message filters, applied only on Save. */
        [[nodiscard]] const ChatFilterModel& chat_filters() const {
            return m_chat_filters;
        }

        /** Sets feedback shown below the restart note.
         * @param status Human-readable status text.
         */
        void set_status(std::string status);

      private:
        void reload_document();
        void sync_from_layout();
        void change_advanced(const std::string& action);
        [[nodiscard]] UiVarMap make_vars() const;

        bool m_open = false;
        bool m_character_available = true;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_w = 640.0f;
        float m_h = 440.0f;
        float m_scale = 1.0f;
        float m_requested_scale = 1.0f;
        float m_screen_w = 0.0f;
        float m_screen_h = 0.0f;
        const UiLayout* m_layout = nullptr;
        UiDocument m_document;
        settings::ClientSettings m_settings;
        std::string m_status;
        ChatFilterModel m_chat_filters;
        std::string m_tab = "display";
    };

}  // namespace openkal::client
