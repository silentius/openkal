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

#include <csignal>

namespace openkal::client {

    /** Receives process termination signals and exposes a frame-loop-safe stop flag. */
    class ClientShutdownSignal {
      public:
        /** Installs SIGINT and SIGTERM handlers and clears an earlier request. */
        void install();

        /** Reports whether a termination signal has been received.
         * @return True after SIGINT or SIGTERM.
         */
        [[nodiscard]] bool requested() const;

      private:
        static void handle_signal(int signal);

        static volatile std::sig_atomic_t s_requested;
    };

}  // namespace openkal::client
