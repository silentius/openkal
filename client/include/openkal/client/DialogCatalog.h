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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** One clickable link inside an NPC talk dialog. */
    struct DialogLink {
        /** Link kind, e.g. "close", "quest", or a numeric next-dialog id. */
        std::string type;

        /** Button / link label. */
        std::string text;

        /** Quest key parts when type is quest. */
        std::uint16_t quest_index = 0;

        /** Quest flag when type is quest. */
        std::uint8_t quest_flag = 0;
    };

    /** NPC dialog content loaded from JSON definitions. */
    struct DialogDef {
        /** Html / script id. */
        std::int32_t id = 0;

        /** Window caption. */
        std::string caption;

        /** Optional client box width in pixels (0 = use window default). */
        float width = 0.0f;

        /** Optional client box height in pixels (0 = use window default). */
        float height = 0.0f;

        /** Body lines. */
        std::vector<std::string> lines;

        /** Action links. */
        std::vector<DialogLink> links;
    };

    /** Loads dialog JSON files for NpcTalkWindow. */
    class DialogCatalog {
      public:
        /**
         * Loads all dialogs from a directory.
         * @param dir config/dialogs path.
         * @return True when at least one dialog loads.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Finds a dialog by html id.
         * @param id Html id.
         * @return Dialog, or nullptr.
         */
        [[nodiscard]] const DialogDef* find(std::int32_t id) const;

      private:
        /** Dialogs keyed by id. */
        std::unordered_map<std::int32_t, DialogDef> m_by_id;
    };

}  // namespace openkal::client
