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
#include "openkal/client/MonsterCatalog.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/world/QuestDb.h"

#include <cstdint>
#include <vector>

namespace openkal::client {

    /** Overhead quest glyph shown above an NPC. */
    enum class QuestMarkerKind {
        /** No marker. */
        None,

        /** Yellow exclamation: a level-appropriate quest is available. */
        Available,

        /** White question: the player has accepted a quest at this NPC. */
        Accepted,

        /** Yellow question: the player can turn in a fulfilled quest at this NPC. */
        ReadyToTurnIn,
    };

    /**
     * Small 3D ! / ? glyphs above quest NPCs and hunt-quest monsters.
     *
     * Client-side helpers driven by QuestDb cases, player flags, class, and
     * level.
     * Event-job quests start at flag 1 (NewPlayer). Missing 700xxx dialog
     * JSON still shows a marker; close-only html hides Available.
     * Available and turn-in markers are yellow; active question marks are white.
     * Hunt monsters use stats.quests and always draw Available (!), never ?.
     * When a matching step already has its item quota in the bag, no marker.
     */
    class QuestMarker {
      public:
        /**
         * Picks the glyph for one NPC quest key.
         * @param quest_key Packed (index << 16) | flag; 0 means none.
         * @param player_level Local player level.
         * @param player_clazz Local player class.
         * @param flags Current SaveQuest / QuestInfo entries.
         * @param quests Loaded quest steps.
         * @param dialogs Loaded dialog catalog (used to verify quest buttons).
         * @param tasks Journal catalog used to determine the completed-item marker, or nullptr.
         * @param count_item Counts an item in the player bags, or nullptr when unavailable.
         * @param count_ctx Passed to count_item.
         * @return Glyph to draw, or None.
         */
        [[nodiscard]] static QuestMarkerKind
        kind_for_npc(std::uint32_t quest_key,
                     int player_level,
                     int player_clazz,
                     const std::vector<protocol::QuestFlagEntry>& flags,
                     const server::QuestDb& quests,
                     const DialogCatalog& dialogs,
                     const TaskCatalog* tasks = nullptr,
                     int (*count_item)(int item_index, const void* ctx) = nullptr,
                     const void* count_ctx = nullptr);

        /**
         * Picks the glyph for a hunt-quest monster (stats.quests slots).
         * Matches an uncleared player flag with equal quest index and flag.
         * Always returns Available or None — never Accepted.
         * When tasks and count_item are set, a matching slot whose step already
         * has item_count of item_index in the bag is skipped.
         * @param slots Hunt-quest slots from the monster JSON.
         * @param flags Current SaveQuest / QuestInfo entries.
         * @param tasks Journal catalog used to read item quotas, or nullptr.
         * @param count_item Counts an item definition in the player bags, or nullptr.
         * @param count_ctx Passed to count_item.
         * @return Available when any incomplete hunt slot matches, otherwise None.
         */
        [[nodiscard]] static QuestMarkerKind
        kind_for_monster(const std::vector<MonsterQuestSlot>& slots,
                         const std::vector<protocol::QuestFlagEntry>& flags,
                         const TaskCatalog* tasks = nullptr,
                         int (*count_item)(int item_index, const void* ctx) = nullptr,
                         const void* count_ctx = nullptr);

        /**
         * Builds the ! and ? meshes and the tint texel.
         * @param engine Engine used to create GPU objects.
         * @return True when both glyphs and the texel were created.
         */
        [[nodiscard]] bool init(engine::IEngine& engine);

        /**
         * Releases GPU objects.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Advances bob / pulse time.
         * @param dt Frame delta in seconds.
         */
        void tick(float dt);

        /**
         * Draws one billboarded glyph at a world position.
         * @param engine Engine used to submit the draw.
         * @param kind Glyph to draw.
         * @param wx World X.
         * @param wy World Y (bottom of the glyph).
         * @param wz World Z.
         * @param cam_eye Camera position used to face the glyph.
         */
        void draw(engine::IEngine& engine,
                  QuestMarkerKind kind,
                  float wx,
                  float wy,
                  float wz,
                  const engine::Vec3& cam_eye) const;

      private:
        /** Exclamation mesh, origin at the bottom centre. */
        engine::MeshHandle m_exclamation{};

        /** Question mesh, origin at the bottom centre. */
        engine::MeshHandle m_question{};

        /** 1x1 white texel used to tint both meshes. */
        engine::TextureHandle m_texture{};

        /** Seconds since init, driving bob and pulse. */
        float m_age = 0.0f;

        /** True when the texel was created here. */
        bool m_own_texture = false;
    };

}  // namespace openkal::client
