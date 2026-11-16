/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>

struct TestPlayerWindowSmoke {
    static bool test(TestPlayerWindow& window) {
        const auto player = [](int id, int level, const char* name, int guild, int rank) {
            return QJsonObject{{"id", id},
                               {"ordinal", id},
                               {"level", level},
                               {"name", name},
                               {"guild", guild},
                               {"rank", rank}};
        };
        const QJsonArray guilds{
            QJsonObject{{"id", 100}, {"name", "Zebra"}, {"test_ordinal", 1}},
            QJsonObject{{"id", 200}, {"name", "Alpha"}, {"test_ordinal", 2}},
        };
        const QJsonArray players{player(10, 90, "Charlie", 100, 1),
                                 player(2, 8, "Zed", 200, 5),
                                 player(100, 100, "Ada", 100, 2)};
        const QJsonObject roster{{"players", players}, {"guilds", guilds}};
        auto& table = *window.m_players;
        auto& inventory = *window.m_items;
        const QSignalBlocker rosterSignals(table);
        window.updateState(roster);
        table.sortItems(3, Qt::AscendingOrder);
        if (table.item(0, 3)->data(Qt::DisplayRole).toInt() != 8 ||
            table.item(2, 3)->data(Qt::DisplayRole).toInt() != 100) {
            return false;
        }
        table.selectRow(0);
        table.sortItems(4, Qt::AscendingOrder);
        window.updateState(roster);
        const auto* combo = qobject_cast<QComboBox*>(table.cellWidget(0, 4));
        if (!combo || combo->currentText() != "Alpha" ||
            window.selectedPlayers() != QJsonArray{2}) {
            return false;
        }
        table.sortItems(1, Qt::AscendingOrder);
        if (table.item(0, 1)->text() != "Ada" || window.selectedPlayers() != QJsonArray{2}) {
            return false;
        }

        const auto item = [](int id, int count, int slot) {
            return QJsonObject{{"iid", id}, {"index", id}, {"count", count}, {"slot", slot}};
        };
        const QJsonArray items{item(10, 100, 0), item(2, 9, 0), item(100, 30, 1)};
        window.m_detailId = 2;
        const QJsonObject detail{{"id", 2}, {"detail", QJsonObject{{"items", items}}}};
        window.updateState(detail);
        inventory.sortItems(0, Qt::AscendingOrder);
        if (inventory.item(0, 0)->data(Qt::DisplayRole).toInt() != 2) {
            return false;
        }
        inventory.selectRow(0);
        inventory.sortItems(2, Qt::DescendingOrder);
        window.updateState(detail);
        if (inventory.item(0, 2)->data(Qt::DisplayRole).toInt() != 100 ||
            inventory.selectionModel()->selectedRows(0).front().data().toInt() != 2) {
            return false;
        }
        table.clearSelection();
        window.updateState({{"players", QJsonArray{}}, {"guilds", QJsonArray{}}});
        window.inspectSelected();
        if (window.m_castleStart->isEnabled() ||
            !window.m_castleStartBlocked->text().contains("Connect to the test server")) {
            return false;
        }
        window.updateCastleWar({{"available", false}});
        if (window.m_castleStart->isEnabled() ||
            !window.m_castleStartBlocked->text().contains("castle_war_file")) {
            return false;
        }
        const QString blocked = "The regular siege still has participants returning.";
        window.updateCastleWar(
            {{"available", true}, {"phase", "Idle"}, {"start_blocked", blocked}});
        if (window.m_castleStart->isEnabled() || window.m_castleStartBlocked->text() != blocked) {
            return false;
        }
        window.updateCastleWar({{"available", true}, {"phase", "Idle"}});
        if (!window.m_castleStart->isEnabled() || window.m_castleFinish->isEnabled() ||
            !window.m_castleStartBlocked->isHidden()) {
            return false;
        }
        window.updateCastleWar({{"available", true}, {"simulation", true}, {"phase", "Admission"}});
        if (!window.m_castleAdmit->isEnabled() || !window.m_castleRestore->isEnabled() ||
            window.m_castleStart->isEnabled()) {
            return false;
        }
        window.updateCastleWar({});
        const QJsonArray serverGuilds{
            QJsonObject{{"id", 100}, {"name", "Zebra"}, {"managed", true}},
            QJsonObject{{"id", 200}, {"name", "Alpha"}, {"managed", true}},
            QJsonObject{{"id", 300}, {"name", "MyGuild"}, {"managed", false}},
        };
        window.updateState({{"castle_war_guilds", serverGuilds}});
        auto& guildCatalog = *window.m_castleGuilds;
        if (guildCatalog.rowCount() != 3 || guildCatalog.item(1, 0)->text() != "MyGuild" ||
            guildCatalog.item(1, 1)->text() != "Regular") {
            return false;
        }
        window.m_castleGuildFilter->setText("myg");
        if (!guildCatalog.isRowHidden(0) || guildCatalog.isRowHidden(1) ||
            !guildCatalog.isRowHidden(2)) {
            return false;
        }
        const auto selectGuild = [&](const QString& name) {
            for (int row = 0; row < guildCatalog.rowCount(); ++row) {
                if (guildCatalog.item(row, 0)->text() == name && !guildCatalog.isRowHidden(row)) {
                    guildCatalog.selectRow(row);
                    return true;
                }
            }
            return false;
        };
        const auto click = [&](const QString& label) {
            for (auto* button : window.m_castleTab->findChildren<QPushButton*>()) {
                if (button->text() == label && button->isEnabled()) {
                    button->click();
                    return true;
                }
            }
            return false;
        };
        auto& alliance = *window.m_castleAlliance;
        alliance.setCurrentIndex(alliance.findData(-1));
        if (!selectGuild("MyGuild") || !click("Set as leader") ||
            window.castleWarStartRequest().value("defense").toArray() != QJsonArray{"MyGuild"}) {
            return false;
        }
        if (!click("New attack alliance")) {
            return false;
        }
        window.m_castleGuildFilter->clear();
        if (!selectGuild("Zebra") || !click("Add as partner") || !selectGuild("Alpha") ||
            !click("New attack alliance")) {
            return false;
        }
        const auto request = window.castleWarStartRequest();
        if (request.value("attack").toArray() !=
                QJsonArray{QJsonArray{"MyGuild", "Zebra"}, QJsonArray{"Alpha"}} ||
            !request.value("defense").toArray().isEmpty()) {
            return false;
        }
        const QString rejection = "An alliance exceeds the server limit.";
        window.updateState({{"castle_war_start_error", rejection}});
        window.updateCastleWar(
            {{"available", true}, {"phase", "Idle"}, {"start_blocked", QString{}}});
        if (!window.m_castleStart->isEnabled() || window.m_castleStartError->isHidden() ||
            window.m_castleStartError->text() != rejection) {
            return false;
        }
        window.m_castleDefense->setText("Defenders");
        if (!window.m_castleStartError->isHidden()) {
            return false;
        }
        return true;
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OpenKal");
    QCoreApplication::setApplicationName("TestPlayers");
    TestPlayerWindow window;
    window.show();
    if (app.arguments().contains("--smoke-test")) {
        if (!TestPlayerWindowSmoke::test(window)) {
            qCritical(
                "Test Players table sorting or Castle War controls did not match the fixture.");
            return 1;
        }
        for (auto* tabs : window.findChildren<QTabWidget*>()) {
            for (int index = 0; index < tabs->count(); ++index) {
                if (tabs->tabText(index) == "Autonomous play") {
                    tabs->setCurrentIndex(index);
                }
            }
        }
        QTimer::singleShot(500, &app, [&] {
            const auto screenshot = qEnvironmentVariable("OPENKAL_TEST_PLAYERS_SCREENSHOT");
            if (!screenshot.isEmpty()) {
                window.grab().save(screenshot);
            }
            app.quit();
        });
    }
    return app.exec();
}
