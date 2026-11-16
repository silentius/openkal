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

#include "openkal/client/ChatCommandRegistry.h"
#include "openkal/client/ChatFilterModel.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/Chatting.h"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace openkal::client {

    /** Outcome of ChatWidget::handle_input for one frame. */
    enum class ChatInputResult : int {
        Ignored = 0,
        Handled = 1,
        Submitted = 2,
    };

    /**
     * Stateful chat panel: chrome, scrollback, text input, channel/battle filters,
     * options UI.
     */
    class ChatWidget {
      public:
        ChatWidget() = default;

        /** Selects the unframed, command-only single-player feed before layout.
         * @param enabled True for the single-player battle feed.
         */
        void set_battle_feed(bool enabled);

        /** Releases the feed background and FreeType atlas before engine shutdown.
         * @param engine Engine owning the GPU resources.
         */
        void destroy(engine::IEngine& engine);

        /** @return True when the single-player battle feed replaces player chat. */
        bool battle_feed() const {
            return m_battle_feed;
        }

        /** @return Layout document for the selected presentation. */
        const char* layout_name() const {
            return m_battle_feed ? "battle_feed" : "chat";
        }

        /** @return True while scrollback is held open without fading. */
        bool history_open() const {
            return m_history_open;
        }

        /** @return Message history, including retained faded events. */
        const ChatLog& message_log() const {
            return m_message_log;
        }

        /** @return Retained player chat, including hidden channel messages. */
        const ChatLog& chat_log() const {
            return m_chat_log;
        }

        /** Opens a personal-message draft without sending it.
         * @param partner Recipient's character name.
         */
        void begin_whisper(std::string_view partner);

        /** @return Visual rows scrolled back from the latest message. */
        int history_scroll() const {
            return m_message_scroll;
        }

        /** @return Filter model (tabs, channels, battle toggles). */
        [[nodiscard]] ChatFilterModel& filters() {
            return m_filters;
        }

        /** @return Filter model. */
        [[nodiscard]] const ChatFilterModel& filters() const {
            return m_filters;
        }

        /** @return Local command registry (server commands are forwarded). */
        [[nodiscard]] ChatCommandRegistry& commands() {
            return m_commands;
        }

        /** @return True while the input field is focused. */
        [[nodiscard]] bool focused() const {
            return m_focused;
        }

        /** @return True while the options panel is open. */
        [[nodiscard]] bool options_open() const {
            return m_options_open;
        }

        /** Opens or closes the options panel. @param open True to show. */
        void set_options_open(bool open) {
            m_options_open = open;
        }

        /** Clears focus without submitting. */
        void blur() {
            m_focused = false;
            if (m_battle_feed) {
                m_history_open = false;
                m_message_scroll = 0;
            }
        }

        /**
         * Appends a player chat line (speaker + wire text).
         * @param speaker Speaker name.
         * @param wire_text Message text as received (prefix preserved for channel).
         * @param local_player Local name used to identify outgoing personal-message echoes.
         */
        void push_chat(std::string_view speaker,
                       std::string_view wire_text,
                       std::string_view local_player = {});

        /**
         * Appends a battle or system info line.
         * @param text Display text.
         * @param kind Battle kind, or nullopt for untyped info.
         * @param color Optional RGB override for this line.
         */
        void push_info(std::string_view text,
                       std::optional<protocol::BattleMsgKind> kind,
                       std::optional<std::array<float, 3>> color = std::nullopt);

        /**
         * Feeds keyboard/mouse for focus, typing, submit, options, and chat scroll.
         * Wheel over the panel or options is consumed (history scroll, not zoom).
         * @param in Frame input.
         * @param panel_x Chat panel left.
         * @param panel_y Chat panel top.
         * @param panel_w Chat panel width.
         * @param panel_h Chat panel height.
         * @param scale UI scale.
         * @param layout Chat layout JSON, or nullptr.
         * @param send_chat Called with text to send to the server (after local
         * command check).
         * @return Whether input was consumed / a line was submitted.
         */
        ChatInputResult handle_input(const engine::FrameInput& in,
                                     float panel_x,
                                     float panel_y,
                                     float panel_w,
                                     float panel_h,
                                     float scale,
                                     const UiLayout* layout,
                                     const std::function<void(std::string_view)>& send_chat);

        /**
         * Draws the chat panel.
         * @param ui Shared UI helpers.
         * @param engine Engine for FreeType.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param scale UI scale.
         * @param layout Chat layout document, or nullptr for baked defaults.
         */
        void draw(WidgetTools& ui,
                  engine::IEngine& engine,
                  float x,
                  float y,
                  float w,
                  float h,
                  float scale = 1.0f,
                  const UiLayout* layout = nullptr);

        /**
         * Tests whether the pointer is over the chat panel (history + input).
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param panel_x Chat panel left.
         * @param panel_y Chat panel top.
         * @param panel_w Chat panel width.
         * @param panel_h Chat panel height.
         * @return True when the pointer is inside the chat panel.
         */
        [[nodiscard]] static bool panel_contains_pointer(
            float mx, float my, float panel_x, float panel_y, float panel_w, float panel_h) {
            return mx >= panel_x && mx <= panel_x + panel_w && my >= panel_y &&
                   my <= panel_y + panel_h;
        }

        /**
         * Tests whether the pointer is over the chat panel or the open options panel.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param panel_x Chat panel left.
         * @param panel_y Chat panel top.
         * @param panel_w Chat panel width.
         * @param panel_h Chat panel height.
         * @param scale UI scale.
         * @return True when the pointer is over chat UI that should consume the wheel.
         */
        [[nodiscard]] bool contains_pointer(float mx,
                                            float my,
                                            float panel_x,
                                            float panel_y,
                                            float panel_w,
                                            float panel_h,
                                            float scale) const;

        /**
         * Tests whether the pointer is over the open options/settings panel.
         * @param mx Pointer X.
         * @param my Pointer Y.
         * @param panel_x Chat panel left.
         * @param panel_y Chat panel top.
         * @param panel_w Chat panel width.
         * @param panel_h Chat panel height.
         * @param scale UI scale.
         * @return True when options are open and the pointer is over that panel.
         */
        [[nodiscard]] bool options_contains_pointer(float mx,
                                                    float my,
                                                    float panel_x,
                                                    float panel_y,
                                                    float panel_w,
                                                    float panel_h,
                                                    float scale) const;

        /** Options panel rectangle next to the chat box. */
        struct OptionsRect {
            /** Left. */
            float x = 0;

            /** Top. */
            float y = 0;

            /** Width. */
            float w = 0;

            /** Height. */
            float h = 0;
        };

        /**
         * Computes the options panel rect for the current chat panel.
         * @param panel_x Chat panel left.
         * @param panel_y Chat panel top.
         * @param panel_w Chat panel width.
         * @param panel_h Chat panel height.
         * @param scale UI scale.
         * @return Options rect (valid even when options are closed).
         */
        [[nodiscard]] static OptionsRect
        options_rect(float panel_x, float panel_y, float panel_w, float panel_h, float scale) {
            constexpr float kTabBand = 22.0f;
            constexpr float kRow = 16.0f;
            constexpr float kGap = 4.0f;
            constexpr float kBottomPad = 8.0f;
            const float content_h =
                kTabBand + kRow * static_cast<float>(ChatFilterModel::kChannelCount) + kGap + kRow +
                kRow * static_cast<float>(ChatFilterModel::kBattleCount) + kBottomPad;
            const float s = std::max(0.25f, scale);
            OptionsRect r;
            r.x = panel_x + panel_w + 6.0f * s;
            r.w = 220.0f * s;
            r.h = content_h * s;
            r.y = panel_y + panel_h - r.h;
            if (r.y < 0.0f) {
                r.y = 0.0f;
            }
            return r;
        }

      private:
        /** Draws the fading feed and the command editor when focused.
         * @param ui Shared rendering helpers.
         * @param engine Text renderer.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param scale UI scale.
         * @param layout Feed layout, or nullptr for defaults.
         */
        void draw_battle_feed(WidgetTools& ui,
                              engine::IEngine& engine,
                              float x,
                              float y,
                              float w,
                              float h,
                              float scale,
                              const UiLayout* layout);

        /** Ensures the FreeType face is loaded. @param engine Engine. */
        void ensure_font(engine::IEngine& engine);

        /**
         * Submits the current input buffer.
         * @param send_chat Server send callback.
         * @return True when something was submitted or handled locally.
         */
        [[nodiscard]] bool submit(const std::function<void(std::string_view)>& send_chat);

        /** Draws the options overlay. */
        void draw_options(WidgetTools& ui,
                          engine::IEngine& engine,
                          float x,
                          float y,
                          float w,
                          float h,
                          float scale);

        /** Handles clicks inside the options panel. @return True when consumed. */
        [[nodiscard]] bool
        handle_options_click(float mx, float my, float x, float y, float w, float h, float scale);

        /** One visible personal-message row from the last rendered frame. */
        struct ReplyHit {
            float x = 0, y = 0, w = 0, h = 0;
            std::string partner;
        };

        /** Visible rows retain their own partner across wrapping and history scrolling. */
        std::vector<ReplyHit> m_reply_hits;
        ChatTab m_reply_tab = ChatTab::Normal;
        int m_reply_scroll = 0;

        /** Filter model. */
        ChatFilterModel m_filters;

        /** Player-chat scrollback, filtered by the selected chat tab when drawn. */
        ChatLog m_chat_log;

        /** Battle and system message scrollback, filtered by message kind when drawn. */
        ChatLog m_message_log;

        /** Local slash commands. */
        ChatCommandRegistry m_commands;

        /** Input buffer. */
        std::string m_input;

        /** True while typing. */
        bool m_focused = false;

        /** Whether player chat is replaced by the single-player battle feed. */
        bool m_battle_feed = false;

        /** Whether scrollback is visible until dismissed. */
        bool m_history_open = false;

        /** Elapsed frame time, shared by receipt timestamps and fading. */
        double m_elapsed = 0.0;

        /** Last rendered wrapping geometry, used to preserve a scrollback anchor. */
        float m_message_width = 400.0f;
        float m_message_font_scale = 1.0f;

        /** True while options UI is visible. */
        bool m_options_open = false;

        /** Player-chat scroll offset in visual lines from the bottom (0 = latest). */
        int m_chat_scroll = 0;

        /** Message-feed scroll offset in visual lines from the bottom (0 = latest). */
        int m_message_scroll = 0;

        /** Wrapped player-chat line count from the last draw (for wheel scrolling). */
        int m_chat_visual_count = 0;

        /** Wrapped message-feed line count from the last draw (for wheel scrolling). */
        int m_message_visual_count = 0;

        /** FreeType face for lines and input. */
        mutable FreetypeFont m_font;

        /** Procedural soft alpha mask, tinted at draw time. */
        engine::TextureHandle m_feed_backdrop;
    };

}  // namespace openkal::client
