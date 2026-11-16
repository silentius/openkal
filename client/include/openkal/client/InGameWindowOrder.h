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

#include <optional>
#include <vector>

namespace openkal::client {

    /** Identifies an in-game window participating in Escape close order. */
    enum class InGameWindowId {
        Inventory,
        Skills,
        CharacterInfo,
        Transform,
        Riding,
        QuestJournal,
        Mail,
        Guild,
        PlayerOptions,
        NpcTalk,
        Blacksmith,
        Mixing,
        Shop,
        Storage,
        SpeedMedicine,
        ConsumableCrafting,
        Settings,
    };

    /** Tracks in-game windows from oldest to most recently opened. */
    class InGameWindowOrder final {
      public:
        /** Marks a window as most recently opened.
         * @param window Window that was opened.
         */
        void opened(InGameWindowId window);

        /** Removes a window from the opening order.
         * @param window Window that was closed.
         */
        void closed(InGameWindowId window);

        /** Returns the most recently opened tracked window.
         * @return Latest window, or no value when none is tracked.
         */
        [[nodiscard]] std::optional<InGameWindowId> latest() const;

      private:
        std::vector<InGameWindowId> m_windows;
    };

}  // namespace openkal::client
