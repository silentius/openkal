/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientShutdownSignal.h"

namespace openkal::client {

    volatile std::sig_atomic_t ClientShutdownSignal::s_requested = 0;

    void ClientShutdownSignal::install() {
        s_requested = 0;
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);
    }

    bool ClientShutdownSignal::requested() const {
        return s_requested != 0;
    }

    void ClientShutdownSignal::handle_signal(int) {
        s_requested = 1;
    }

}  // namespace openkal::client
