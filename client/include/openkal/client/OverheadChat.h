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

#include <string>
#include <string_view>
#include <unordered_map>

namespace openkal::client {

    /** Keeps the latest player message visible beneath its speaker's name for five seconds. */
    class OverheadChat {
      public:
        /** Replaces a speaker's current message; server replies and empty messages are ignored.
         * @param speaker Character name from the chat packet.
         * @param text Received message.
         */
        void show(std::string speaker, std::string text);

        /** Advances message lifetimes and removes expired entries.
         * @param dt Elapsed seconds.
         */
        void update(float dt);

        /** Removes a departing character's message.
         * @param speaker Character name.
         */
        void remove(const std::string& speaker);

        /** Gets the active message for a character.
         * @param speaker Character name.
         * @return Message, or an empty view when expired or absent.
         */
        std::string_view message(const std::string& speaker) const;

        /** Draws wrapped TrueType text beneath a name anchor.
         * @param engine Renderer for text and background.
         * @param font Name font.
         * @param speaker Character name.
         * @param x Projected name center X.
         * @param y Projected name baseline Y.
         */
        void draw(engine::IEngine& engine,
                  const FreetypeFont& font,
                  const std::string& speaker,
                  float x,
                  float y) const;

      private:
        struct Entry {
            std::string text;
            float remaining = 5.0f;
        };
        std::unordered_map<std::string, Entry> m_messages;
    };

}  // namespace openkal::client
