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

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "OpenKal Version " OPENKAL_VERSION "\n"
              << "-------------------------------------------------------------\n"
              << "Engine: " OPENKAL_ENGINE_NAME "\n"
              << std::flush;

    openkal::client::ClientApplication client;
    return client.run(argc, argv);
}
