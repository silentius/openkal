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

#include <memory>

namespace openkal::engine {
    class IEngine;
}

namespace openkal::client {

    /** Owns and runs one configured in-world client session. */
    class ClientSession {
      public:
        /** Internal result requesting character selection while retaining the active engine. */
        static constexpr int kSelectCharacterExitCode = 2;

        /** Internal result requesting the full startup screen with the active engine. */
        static constexpr int kStartupExitCode = 3;

        /** Binds the session to the application-owned rendering backend.
         * @param engine Rendering backend owner.
         */
        explicit ClientSession(std::unique_ptr<engine::IEngine>& engine);
        ~ClientSession();

        ClientSession(const ClientSession&) = delete;
        ClientSession& operator=(const ClientSession&) = delete;

        /** Initializes and runs the selected client session.
         * @param argc Number of command-line arguments.
         * @param argv Command-line arguments.
         * @param skip_startup_screen True to reuse the active engine and enter character selection.
         * @return Process exit code.
         */
        [[nodiscard]] int run(int argc, char** argv, bool skip_startup_screen = false);

      private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}  // namespace openkal::client
