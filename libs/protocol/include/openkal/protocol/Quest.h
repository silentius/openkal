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
#include <vector>

namespace openkal::protocol {

    /** Direct NPC interaction distance in server horizontal pixels. */
    inline constexpr int kNpcInteractionRange = 24;

    /** Maximum NPC conversation distance accepted by the server, in horizontal pixels. */
    inline constexpr int kNpcReplyRange = 64;

    /** One quest flag entry as carried by SaveQuest / QuestInfo. */
    struct QuestFlagEntry {
        /** Quest index (WORD). */
        std::uint16_t quest = 0;

        /** Current step flag. */
        std::uint8_t flag = 0;

        /** Non-zero when the quest is cleared / completed. */
        std::uint8_t clear = 0;
    };

    /** NPC spawn payload for S2C_CREATENPC ("dwbdddwdd"). */
    struct CreateNpcInfo {
        /** Runtime NPC entity id. */
        std::uint32_t id = 0;

        /** GenNPC / NPC definition index. */
        std::uint16_t index = 0;

        /** Visual shape id. */
        std::uint8_t shape = 0;

        /** Server pixel X. */
        std::int32_t x = 0;

        /** Server pixel Y. */
        std::int32_t y = 0;

        /** Server Z / height. */
        std::int32_t z = 0;

        /** Facing direction word. */
        std::uint16_t dir = 0;

        /** GState bitmask. */
        std::uint32_t gstate = 0;

        /** Extra field (siege standard / unused). */
        std::uint32_t standard = 0;
    };

    /** Encodes and decodes quest / NPC talk packets. */
    class Quest final {
      public:
        Quest() = delete;

        /**
         * Builds C2S_ASKNPC.
         * @param npc_id Runtime NPC entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_ask_npc(std::uint32_t npc_id);

        /**
         * Parses C2S_ASKNPC payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param npc_id Out NPC entity id.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_ask_npc(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id);

        /**
         * Builds C2S_QUEST.
         * @param quest_key Combined (index << 16) | flag.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_quest(std::uint32_t quest_key);

        /**
         * Parses C2S_QUEST payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param quest_key Out quest key.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_quest(const std::uint8_t* data, std::size_t size, std::uint32_t& quest_key);

        /**
         * Builds S2C_SENDHTML.
         * @param html_id Dialog / script id.
         * @param npc_id Optional source NPC entity, appended after the legacy dialog id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_send_html(std::int32_t html_id,
                                                                       std::uint32_t npc_id = 0);

        /**
         * Parses S2C_SENDHTML payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param html_id Out dialog id.
         * @param npc_id Optional output for the source NPC; zero for legacy packets.
         * @return True when valid.
         */
        [[nodiscard]] static bool parse_send_html(const std::uint8_t* data,
                                                  std::size_t size,
                                                  std::int32_t& html_id,
                                                  std::uint32_t* npc_id = nullptr);

        /**
         * Builds S2C_SAVEQUEST ("wbb").
         * @param entry Quest flag entry.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_save_quest(const QuestFlagEntry& entry);

        /**
         * Parses S2C_SAVEQUEST payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param entry Out quest flag entry.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_save_quest(const std::uint8_t* data, std::size_t size, QuestFlagEntry& entry);

        /**
         * Builds S2C_QUESTINFO ("b" count + count * "wbb").
         * @param entries Quest flag entries.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_quest_info(const std::vector<QuestFlagEntry>& entries);

        /**
         * Parses S2C_QUESTINFO payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param entries Out quest flag entries.
         * @return True when valid.
         */
        [[nodiscard]] static bool parse_quest_info(const std::uint8_t* data,
                                                   std::size_t size,
                                                   std::vector<QuestFlagEntry>& entries);

        /**
         * Builds S2C_CREATENPC ("dwbdddwdd").
         * @param info NPC create payload.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_create_npc(const CreateNpcInfo& info);

        /**
         * Parses S2C_CREATENPC payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param info Out NPC create payload.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_create_npc(const std::uint8_t* data, std::size_t size, CreateNpcInfo& info);

        /**
         * Builds S2C_REMOVENPC ("d").
         * @param npc_id Runtime NPC entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_remove_npc(std::uint32_t npc_id);

        /**
         * Parses S2C_REMOVENPC payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param npc_id Out NPC entity id.
         * @return True when valid.
         */
        [[nodiscard]] static bool
        parse_remove_npc(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id);

        /**
         * Packs quest index and flag into a protocol key.
         * @param quest Quest index.
         * @param flag Step flag.
         * @return Combined key.
         */
        [[nodiscard]] static constexpr std::uint32_t make_key(std::uint16_t quest,
                                                              std::uint8_t flag) {
            return (static_cast<std::uint32_t>(quest) << 16) | static_cast<std::uint32_t>(flag);
        }

        /**
         * Extracts the quest index from a key.
         * @param key Combined key.
         * @return Quest index.
         */
        [[nodiscard]] static constexpr std::uint16_t key_quest(std::uint32_t key) {
            return static_cast<std::uint16_t>(key >> 16);
        }

        /**
         * Extracts the flag from a key.
         * @param key Combined key.
         * @return Flag byte.
         */
        [[nodiscard]] static constexpr std::uint8_t key_flag(std::uint32_t key) {
            return static_cast<std::uint8_t>(key & 0xFFFFu);
        }
    };

}  // namespace openkal::protocol
