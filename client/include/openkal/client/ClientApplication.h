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

    /** Owns and runs one OpenKal client process. */
    class ClientApplication {
      public:
        /** Constructs a client application with no initialized runtime. */
        ClientApplication();

        /** Releases the runtime after its GPU-owning subsystems have been destroyed. */
        ~ClientApplication();

        ClientApplication(const ClientApplication&) = delete;
        ClientApplication& operator=(const ClientApplication&) = delete;

        /** Initializes the runtime and processes frames until the client exits.
         * @param argc Number of command-line arguments.
         * @param argv Command-line arguments.
         * @return Process exit code.
         */
        [[nodiscard]] int run(int argc, char** argv);

      private:
        class Impl;

        /** Rendering backend; it outlives every GPU-owning client subsystem. */
        std::unique_ptr<engine::IEngine> m_engine;

        /** Runtime coordinator, kept out of the public application interface. */
        std::unique_ptr<Impl> m_impl;
    };

}  // namespace openkal::client
