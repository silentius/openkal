/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ManagementWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QJsonArray>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <clocale>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setApplicationName("OpenKal Management Console");
    QApplication::setOrganizationName("OpenKal");
    QApplication::setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    const auto args = app.arguments();
    ManagementWindow window;
    window.show();
    if (args.contains("--smoke-test") || args.contains("--capture")) {
        window.display_state({{"connected", true},
                              {"api", true},
                              {"total", 2},
                              {"players",
                               QJsonArray{QJsonObject{{"id", 1},
                                                      {"account", 1},
                                                      {"name", "Aster"},
                                                      {"level", 55},
                                                      {"role", 3},
                                                      {"presence", "Online"}},
                                          QJsonObject{{"id", 2},
                                                      {"account", 2},
                                                      {"name", "Mira"},
                                                      {"level", 34},
                                                      {"role", 0},
                                                      {"presence", "Offline"}}}}});
        QTimer::singleShot(400, &app, [&] {
            if (args.contains("--capture")) {
                const int position = args.indexOf("--capture");
                if (position + 1 < args.size() && !window.grab().save(args[position + 1])) {
                    app.exit(1);
                } else {
                    app.quit();
                }
            } else {
                auto* table = window.findChild<QTableWidget*>("playerTable");
                auto* role = window.findChild<QComboBox*>("playerRole");
                auto* mail = window.findChild<QPushButton*>("sendMailButton");
                auto* pm = window.findChild<QPushButton*>("sendPmButton");
                auto* subject = window.findChild<QLineEdit*>("mailSubject");
                auto* message = window.findChild<QLineEdit*>("pmMessage");
                bool valid = table && role && mail && pm && subject && message &&
                             table->rowCount() == 2 && role->currentIndex() == 3;
                if (valid) {
                    subject->setText("Owned smoke draft");
                    message->setText("Owned smoke PM");
                    valid = mail->isEnabled() && pm->isEnabled();
                    table->selectRow(1);
                    valid = valid && !pm->isEnabled() && role->currentIndex() == 0;
                    window.display_state(
                        {{"connected", false}, {"api", false}, {"players", QJsonArray{}}});
                    valid = valid && !mail->isEnabled() && !pm->isEnabled();
                }
                app.exit(valid ? 0 : 1);
            }
        });
    }
    return app.exec();
}
