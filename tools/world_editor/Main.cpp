/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "WorldEditorWindow.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <cstdio>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);
    QApplication::setApplicationName("world_editor");
    QApplication::setApplicationDisplayName("OpenKal World Editor");
    QApplication::setOrganizationName("OpenKal");
    QCommandLineParser parser;
    parser.setApplicationDescription("OpenKal outdoor world editor");
    parser.addHelpOption();
    parser.addPositionalArgument(
        "openkal.profile", "Open an OpenKal runtime profile.", "[openkal.profile]");
    const QCommandLineOption data("data", "Override the client data directory.", "DIR");
    const QCommandLineOption profile(
        "profile", "Use the client data root from a runtime profile.", "FILE");
    const QCommandLineOption overlay(
        "overlay", "Open a previously exported patch directory.", "DIR");
    parser.addOptions({data, profile, overlay});
    parser.process(application);

    auto profile_path = parser.value(profile);
    const auto arguments = parser.positionalArguments();
    if (!arguments.isEmpty()) {
        if (arguments.size() != 1 || parser.isSet(profile) ||
            !openkal::platform::is_profile_path(arguments.front().toStdString())) {
            std::fprintf(stderr,
                         "Supply one .profile path, either as an argument or with --profile.\n");
            return 2;
        }
        profile_path = arguments.front();
    }
    openkal::world_editor::WorldEditorWindow window(
        parser.value(data), parser.value(overlay), profile_path);
    window.show();
    return application.exec();
}
