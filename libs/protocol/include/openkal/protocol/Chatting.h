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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::protocol {

    /** Maximum C2S chat payload length (ScanPacket limit). */
    inline constexpr std::size_t kChatMaxChars = 129;

    /**
     * Chat channel derived from the message text prefix (party-chatting
     * path). Filtering is client-side; the server only uses prefixes for routing.
     */
    enum class ChatChannel : std::uint8_t {
        Common = 0,
        Party,
        Guild,
        Whisper,
        Alliance,
        Shout,
        Squad,
        Count,
    };

    /**
     * Battle and system message categories.
     * Untyped info lines use std::nullopt instead of a kind.
     */
    enum class BattleMsgKind : std::uint8_t {
        Damage = 0,
        Skill,
        Hp,
        Mp,
        Rage,
        Exp,
        TransformExp,
        ItemUse,
        ItemGet,
        MoneyGet,
        Count,
    };

    /** Legacy MESSAGE_TYPE ordinals used beyond item-monster / chest feedback. */
    enum class SystemMessageId : std::uint8_t {
        CantDelPutOnItem = 11,
        InvenIsFull = 17,
        PriorityOfItem = 22,
        NotEnoughInvenSlot = 39,
        EnchantFailed = 54,
        AccountBound = 77,
    };

    /** Encodes and decodes C2S/S2C chatting packets and channel prefixes. */
    class Chatting final {
      public:
        Chatting() = delete;

        /**
         * Frames C2S_CHATTING "s".
         * @param text Chat text (truncated to kChatMaxChars).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::string_view text);

        /**
         * Parses C2S_CHATTING "s".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param out Destination text.
         * @return True when a NUL-terminated string was read.
         */
        [[nodiscard]] static bool
        parse_c2s(const std::uint8_t* data, std::size_t size, std::string& out);

        /**
         * Frames S2C_CHATTING "ss".
         * @param speaker Speaker character name.
         * @param text Message text (may still carry a channel prefix).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_s2c(std::string_view speaker,
                                                                 std::string_view text);

        /**
         * Parses S2C_CHATTING "ss".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param speaker Out speaker name.
         * @param text Out message text.
         * @return True when both strings were read.
         */
        [[nodiscard]] static bool parse_s2c(const std::uint8_t* data,
                                            std::size_t size,
                                            std::string& speaker,
                                            std::string& text);

        /**
         * Maps a message prefix to a chat channel.
         * @param text Message text as stored on the wire.
         * @return Channel for filtering and coloring.
         */
        [[nodiscard]] static ChatChannel channel_of(std::string_view text);

        /**
         * Removes the routing prefix and whisper recipient from displayed chat.
         * @param text Message text.
         * @return Message body with punctuation in the content preserved.
         */
        [[nodiscard]] static std::string_view display_body(std::string_view text);

        /**
         * Parses an @whisper target and body.
         * @param text Raw chat text starting with '@'.
         * @param target Out target character name.
         * @param body Out whisper body (may be empty).
         * @return True when a non-empty target name was found.
         */
        [[nodiscard]] static bool
        parse_whisper(std::string_view text, std::string& target, std::string& body);

        /**
         * Returns true when text is a slash command other than shout/yell chat forms.
         * @param text Raw chat text.
         * @return True for /time-style commands (not /s or /y).
         */
        [[nodiscard]] static bool is_slash_command(std::string_view text);

        /**
         * Extracts the command token after '/' (lowercase, no args).
         * @param text Raw chat text starting with '/'.
         * @return Command name, or empty when not a slash command.
         */
        [[nodiscard]] static std::string command_name(std::string_view text);
    };

}  // namespace openkal::protocol
