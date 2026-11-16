/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientApplication.h"

#include "openkal/client/ClientSession.h"
#include "openkal/engine/engine.h"

namespace openkal::client {

    class ClientApplication::Impl {
      public:
        explicit Impl(std::unique_ptr<engine::IEngine>& engine) : m_engine(engine) {}

        [[nodiscard]] int run(int argc, char** argv) {
            bool skip_startup_screen = false;
            ClientSession session(m_engine);
            for (;;) {
                const int result = session.run(argc, argv, skip_startup_screen);
                if (result != ClientSession::kSelectCharacterExitCode &&
                    result != ClientSession::kStartupExitCode) {
                    return result;
                }
                skip_startup_screen = result == ClientSession::kSelectCharacterExitCode;
            }
        }

      private:
        std::unique_ptr<engine::IEngine>& m_engine;
    };

    ClientApplication::ClientApplication() : m_impl(std::make_unique<Impl>(m_engine)) {}

    ClientApplication::~ClientApplication() = default;

    int ClientApplication::run(int argc, char** argv) {
        return m_impl->run(argc, argv);
    }

}  // namespace openkal::client
