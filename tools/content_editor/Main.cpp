/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ContentWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

namespace {
    QString config_directory(const QString& value) {
        if (value.isEmpty()) {
            return {};
        }
        const QDir directory(value);
        const auto nested = directory.absoluteFilePath("config");
        return QFileInfo(nested).isDir() ? nested : directory.absolutePath();
    }
}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OpenKal Content Editor");
    app.setOrganizationName("OpenKal");

    QCommandLineParser options;
    options.setApplicationDescription(
        "Edit OpenKal game definitions and place NPCs and spawns on the client map.");
    options.addHelpOption();
    options.addPositionalArgument("profile", "Optional OpenKal profile.");
    const QCommandLineOption config(
        "config", "Editable loose configuration directory.", "directory");
    const QCommandLineOption base(
        "base-config", "Read-only configuration used for lookups.", "directory");
    const QCommandLineOption client(
        "client", "Compatible client installation containing data and map.", "directory");
    const QCommandLineOption profile("profile", "OpenKal profile.", "file");
    options.addOptions({config, base, client, profile});
    options.process(app);

    openkal::content_editor::Workspace workspace;
    const auto source = config_directory(QString::fromUtf8(OPENKAL_SOURCE_CONFIG));
    const auto external = qEnvironmentVariable("OPENKAL_DATA_ROOT", OPENKAL_EXTERNAL_DATA_ROOT);
    if (!external.isEmpty()) {
        const auto catalogs = QDir(external).absoluteFilePath("config");
        if (QFileInfo(catalogs).isDir()) {
            workspace.external_config = catalogs;
        }
    }
    workspace.base = options.isSet(base) ? config_directory(options.value(base)) : source;
    const auto arguments = options.positionalArguments();
    if (arguments.size() > 1 || (!arguments.isEmpty() && options.isSet(profile))) {
        options.showHelp(1);
    }
    const auto profile_path = options.isSet(profile)
                                  ? options.value(profile)
                                  : arguments.value(0, qEnvironmentVariable("OPENKAL_PROFILE"));
    const auto explicit_config = config_directory(options.value(config));
    const auto override_config =
        explicit_config.isEmpty() && profile_path.isEmpty() && workspace.external_config.isEmpty()
            ? source
            : explicit_config;
    if (const auto problem = openkal::content_editor::ContentWindow::read_profile(
            workspace, profile_path, override_config);
        !problem.isEmpty()) {
        if (!profile_path.isEmpty()) {
            QMessageBox::warning(nullptr, app.applicationName(), problem);
            return 1;
        }
        workspace.config = explicit_config.isEmpty() ? workspace.external_config : explicit_config;
        if (workspace.config.isEmpty()) {
            workspace.config = source.isEmpty() ? QDir::current().filePath("config") : source;
        }
    }
    if (options.isSet(client)) {
        const QDir root(options.value(client));
        workspace.data = root.absoluteFilePath("data");
        workspace.map = root.absoluteFilePath("map");
        workspace.maps_subdirectory = "MAPS";
    }

    openkal::content_editor::ContentWindow window(std::move(workspace));
    window.show();
    return app.exec();
}
