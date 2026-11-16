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

#include "openkal/protocol/Chatting.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openkal::client {

    /** Multiplayer chat tabs in display order. */
    enum class ChatTab : std::uint8_t {
        Normal = 0,
        Party,
        Guild,
        Alliance,
        Squad,
        Shout,
        Count,
    };

    /** One scrollback line after filtering. */
    class ChatLine {
      public:
        ChatLine() = default;

        /**
         * Builds a chat line.
         * @param channel Channel for tab filters.
         * @param battle_kind Battle kind, or nullopt for untyped / player chat.
         * @param text Display text.
         * @param r Red 0..1.
         * @param g Green 0..1.
         * @param b Blue 0..1.
         * @param received_at Receipt time on the widget's elapsed-seconds clock.
         * @param reply_target Personal-message partner, or empty for other channels.
         */
        ChatLine(protocol::ChatChannel channel,
                 std::optional<protocol::BattleMsgKind> battle_kind,
                 std::string text,
                 float r,
                 float g,
                 float b,
                 double received_at = 0.0,
                 std::string reply_target = {})
            : m_channel(channel), m_battle_kind(battle_kind), m_text(std::move(text)), m_r(r),
              m_g(g), m_b(b), m_received_at(received_at), m_reply_target(std::move(reply_target)) {}

        /** @return Receipt time on the widget's elapsed-seconds clock. */
        double received_at() const {
            return m_received_at;
        }

        /** @return Channel. */
        [[nodiscard]] protocol::ChatChannel channel() const {
            return m_channel;
        }

        /** @return Battle kind, if any. */
        [[nodiscard]] const std::optional<protocol::BattleMsgKind>& battle_kind() const {
            return m_battle_kind;
        }

        /** @return Display text. */
        [[nodiscard]] const std::string& text() const {
            return m_text;
        }

        /** @return Personal-message partner for click-to-reply, or empty. */
        const std::string& reply_target() const {
            return m_reply_target;
        }

        /** @return Red. */
        [[nodiscard]] float r() const {
            return m_r;
        }

        /** @return Green. */
        [[nodiscard]] float g() const {
            return m_g;
        }

        /** @return Blue. */
        [[nodiscard]] float b() const {
            return m_b;
        }

      private:
        /** Channel. */
        protocol::ChatChannel m_channel = protocol::ChatChannel::Common;

        /** Battle kind, or nullopt. */
        std::optional<protocol::BattleMsgKind> m_battle_kind;

        /** Display text. */
        std::string m_text;

        /** Red. */
        float m_r = 1.0f;

        /** Green. */
        float m_g = 1.0f;

        /** Blue. */
        float m_b = 1.0f;

        /** Receipt time used to fade recent battle events. */
        double m_received_at = 0.0;

        /** Reply metadata independent of the displayed text. */
        std::string m_reply_target;
    };

    /**
     * Tab × channel checkbox matrix and battle-message toggles (initial
     * chatting/msg options).
     *
     * Filtering is client-only: the server still delivers packets.
     */
    class ChatFilterModel {
      public:
        /** Builds defaults showing each tab's own channel and personal messages on Normal. */
        ChatFilterModel();

        /** @return Active viewing/sending tab. */
        [[nodiscard]] ChatTab active_tab() const {
            return m_active_tab;
        }

        /**
         * Sets the active tab.
         * @param tab Tab to select.
         */
        void set_active_tab(ChatTab tab);

        /**
         * Reports whether a channel is shown on a tab.
         * @param tab Tab.
         * @param channel Channel column.
         * @return True when enabled.
         */
        [[nodiscard]] bool channel_enabled(ChatTab tab, protocol::ChatChannel channel) const;

        /**
         * Enables or disables a channel on a tab.
         * @param tab Tab.
         * @param channel Channel column.
         * @param enabled True to show.
         */
        void set_channel_enabled(ChatTab tab, protocol::ChatChannel channel, bool enabled);

        /**
         * Reports whether a battle kind is shown.
         * @param kind Battle kind.
         * @return True when enabled.
         */
        [[nodiscard]] bool battle_enabled(protocol::BattleMsgKind kind) const;

        /**
         * Enables or disables a battle kind.
         * @param kind Battle kind.
         * @param enabled True to show.
         */
        void set_battle_enabled(protocol::BattleMsgKind kind, bool enabled);

        /**
         * Tests whether a line should appear on the active tab.
         * @param channel Player-chat channel (ignored for battle lines).
         * @param battle_kind Battle kind, or nullopt for player/system chat.
         * @return True when the line should be appended for the active tab.
         */
        [[nodiscard]] bool accepts(protocol::ChatChannel channel,
                                   std::optional<protocol::BattleMsgKind> battle_kind) const;

        /** Number of supported player channels. */
        static constexpr std::size_t kChannelCount =
            static_cast<std::size_t>(protocol::ChatChannel::Count);

        /** Tab count (6). */
        static constexpr std::size_t kTabCount = static_cast<std::size_t>(ChatTab::Count);

        /** Number of battle message kinds. */
        static constexpr std::size_t kBattleCount =
            static_cast<std::size_t>(protocol::BattleMsgKind::Count);

        /** Flat channel flags for persistence (tab-major, size kTabCount *
         * kChannelCount). */
        [[nodiscard]] const std::array<bool, kTabCount * kChannelCount>& channel_flags() const {
            return m_channels;
        }

        /**
         * Replaces channel flags from persistence.
         * @param flags Flat tab-major flags.
         */
        void set_channel_flags(const std::array<bool, kTabCount * kChannelCount>& flags);

        /** @return True when untyped system messages are visible. */
        [[nodiscard]] bool system_enabled() const {
            return m_system_enabled;
        }

        /** Controls untyped system messages. @param enabled True to show. */
        void set_system_enabled(bool enabled) {
            m_system_enabled = enabled;
        }

        /** @return Battle toggles. */
        [[nodiscard]] const std::array<bool, kBattleCount>& battle_flags() const {
            return m_battle;
        }

        /**
         * Replaces battle toggles from persistence.
         * @param flags Battle toggles.
         */
        void set_battle_flags(const std::array<bool, kBattleCount>& flags);

        /**
         * Default RGB for a channel.
         * @param channel Channel.
         * @param r Out red.
         * @param g Out green.
         * @param b Out blue.
         */
        static void channel_color(protocol::ChatChannel channel, float& r, float& g, float& b);

        /**
         * Default RGB for a battle/info line.
         * @param kind Battle kind, or nullopt for generic info.
         * @param r Out red.
         * @param g Out green.
         * @param b Out blue.
         */
        static void
        battle_color(std::optional<protocol::BattleMsgKind> kind, float& r, float& g, float& b);

        /** Stable UI tab ID. @param tab Tab. @return Lowercase binding value. */
        [[nodiscard]] static const char* tab_id(ChatTab tab);

        /** Primary sending channel. @param tab Tab. @return Channel associated with the tab. */
        [[nodiscard]] static protocol::ChatChannel tab_channel(ChatTab tab);

        /** Human-readable tab label. @param tab Tab. @return Label. */
        [[nodiscard]] static const char* tab_label(ChatTab tab);

        /** Human-readable channel label. @param channel Channel. @return Label. */
        [[nodiscard]] static const char* channel_label(protocol::ChatChannel channel);

        /** Human-readable battle label. @param kind Kind. @return Label. */
        [[nodiscard]] static const char* battle_label(protocol::BattleMsgKind kind);

      private:
        /** Flat index for tab/channel. */
        [[nodiscard]] static std::size_t index(ChatTab tab, protocol::ChatChannel channel);

        /** Active tab. */
        ChatTab m_active_tab = ChatTab::Normal;

        /** Tab × channel enable flags. */
        std::array<bool, kTabCount * kChannelCount> m_channels{};

        /** Visibility of untyped system messages. */
        bool m_system_enabled = true;

        /** Battle message toggles. */
        std::array<bool, kBattleCount> m_battle{};
    };

    /** Ring-buffer chat history. */
    class ChatLog {
      public:
        /** Maximum retained lines. */
        static constexpr std::size_t kCapacity = 200;

        /**
         * Appends a line (drops oldest when full).
         * @param line Line to append.
         */
        void push(ChatLine line);

        /** @return Stored lines oldest→newest. */
        [[nodiscard]] const std::vector<ChatLine>& lines() const {
            return m_lines;
        }

        /** Clears history. */
        void clear() {
            m_lines.clear();
        }

      private:
        /** Scrollback lines. */
        std::vector<ChatLine> m_lines;
    };

}  // namespace openkal::client
