/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MainWindow.h"
#include "openkal/platform/Profile.h"

#include <QApplication>
#include <clocale>
#include <cstring>
#include <string>
#include <utility>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main(int argc, char** argv) {
    std::string profile_path = openkal::platform::Profile::default_path(OPENKAL_REPO_ROOT);
    bool profile_selected = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--profile") == 0 && index + 1 < argc) {
            profile_path = argv[++index];
            profile_selected = true;
        } else if (!profile_selected && argv[index][0] != '-') {
            profile_path = argv[index];
            profile_selected = true;
        }
    }

    QApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setApplicationName("openkal_settings");
    QApplication::setOrganizationName("OpenKal");
    MainWindow window(std::move(profile_path));
    window.show();
    return app.exec();
}
