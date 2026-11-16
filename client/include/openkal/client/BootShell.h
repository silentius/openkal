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
#include "openkal/client/LoginMapStage.h"
#include "openkal/client/Player.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiWidgets.h"
#include "openkal/engine/engine.h"
#include "openkal/protocol/PlayerInfo.h"
#include "openkal/server/store/DataStore.h"

#include <cstdint>
#include <string>

namespace openkal::client {

    /** Boot UI phases before entering the world. */
    enum class ShellPhase { ModeSelect, CharSelect, CharCreate, EnterWorld, Startup, Quit };

    /** Result of a completed (or aborted) boot-shell session. */
    struct ShellResult {
        /** Final phase reached by the shell. */
        ShellPhase phase = ShellPhase::ModeSelect;

        /** True when the user chose multiplayer mode. */
        bool multiplayer = false;

        /** Character selected or created for world entry. */
        server::CharacterRecord selected{};
    };

    /**
     * Boot shell for character select and create (single-player).
     *
     * Optional mode select (single vs multiplayer) is gated by
     * kOfferMultiplayerModeSelect in BootShell.cpp. Character select / create follow
     * KalOnline:
     * PLAYERINFO-shaped data → 3D stage (charsel) / 3D class line-up (chargen).
     * The indoor `Map\\login_map` room is shared by both phases.
     * Overview, creation and deletion panels use openkal.ui/v1 Dragon layouts.
     */
    class BootShell {
      public:
        /** Maps a KalOnline job byte to a player class. */
        [[nodiscard]] static Class clazz_from_job(std::uint8_t job);

        /** Places a selection slot on a shared horizontal radius around the room center.
         * Preserves each slot's bearing and fallback floor height.
         * @param index Slot index in [0, count).
         * @param count Occupied slot count in [1, 8].
         * @param x Out X.
         * @param y Out Y.
         * @param z Out Z.
         * @return True if index and count are valid.
         */
        [[nodiscard]] static bool
        charsel_slot_pos(int index, int count, float& x, float& y, float& z);

        /** Centers the four creation previews on a symmetric row in the login room.
         * @param job Job index in [0, 4).
         * @param x Out X.
         * @param y Out Y.
         * @param z Out Z.
         * @return True if job is valid.
         */
        [[nodiscard]] static bool chargen_job_pos(int job, float& x, float& y, float& z);

        /**
         * Builds a protocol PLAYERINFO char from a store record.
         * Includes ordinary equipment and skin rows up to the protocol byte-count limit.
         * @param r Character record from the data store.
         * @param items Inventory rows for this character; bag slots (slot 0) are skipped.
         * @return Protocol character fields including wear.
         */
        [[nodiscard]] static protocol::PlayerInfoChar
        char_from_record(const server::CharacterRecord& r,
                         const std::vector<server::ItemRecord>& items);

        /**
         * SP stand-in for MainSvr login → S2C_PLAYERINFO: store → frame → parse.
         * @param store Account/character store.
         * @param account_id Account to load.
         * @param out Parsed PLAYERINFO message.
         * @return True on success.
         */
        [[nodiscard]] static bool load_playerinfo_from_store(server::ICharacterRoster& store,
                                                             std::int32_t account_id,
                                                             protocol::PlayerInfoMsg& out);

        /**
         * Runs the interactive boot shell until EnterWorld or Quit.
         * @param engine Render/input engine.
         * @param ui UI asset helpers.
         * @param store Character store.
         * @param account_id Logged-in account id.
         * @param preferred_character_id Character selected on the first character-list load, or
         * zero.
         * @param data_root Game data root path.
         * @param configuration_root Selected loose or package-backed configuration root.
         * @param starters_dir Per-class starter JSON directory (config/starters).
         * @param stage_paths Indoor login-room assets.
         * @param ui_font TrueType font for the shell's interactive labels.
         * @param ui_settings Shared client UI scaling settings.
         * @param multiplayer True when the supplied roster belongs to a remote session.
         * @return Shell outcome including selected character.
         */
        [[nodiscard]] static ShellResult run(engine::IEngine& engine,
                                             UiAssets& ui,
                                             server::ICharacterRoster& store,
                                             std::int32_t account_id,
                                             std::int32_t preferred_character_id,
                                             const std::string& data_root,
                                             const std::string& configuration_root,
                                             const std::string& starters_dir,
                                             const LoginMapStagePaths& stage_paths,
                                             const FreetypeFont& ui_font,
                                             const UiSettings& ui_settings,
                                             bool multiplayer = false);
    };

}  // namespace openkal::client
