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

#include "TestPlayerBackend.h"

#include <QJsonArray>
#include <QMainWindow>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QWidget;

/** Desktop population editor for an explicitly enabled OpenKal test server. */
class TestPlayerWindow final : public QMainWindow {
  public:
    /** Builds the management UI and network worker. */
    TestPlayerWindow();

    /** Stops sessions after closing all owned graphical clients. */
    ~TestPlayerWindow() override;

  private:
    friend struct TestPlayerWindowSmoke;

    QJsonArray selectedPlayers() const;
    QJsonArray allPlayers() const;
    QJsonArray positionTargets() const;
    void updatePositionCount();
    void addCastleWarTab(QTabWidget& tabs);
    void updateCastleGuilds(const QJsonArray& guilds);
    void filterCastleGuilds(const QString& query);
    void updateCastleAlliances();
    void assignCastleGuild(const QString& guild, int alliance, bool asLeader);
    QJsonObject castleWarStartRequest() const;
    void updateCastleWar(const QJsonObject& state);
    void showCastleStartError(const QString& message);
    void startCastleWar();
    void send(QJsonObject command);
    void updateState(const QJsonObject& state);
    void launchClient(const QString& endpoint);
    void inspectSelected();
    void saveSettings();
    TestPlayerBackend m_backend;
    QLineEdit *m_host, *m_token, *m_ca, *m_serverName, *m_client, *m_profile;
    QSpinBox *m_port, *m_playerCount, *m_guildCount;
    QTableWidget *m_players, *m_items;
    QComboBox *m_targetGuild, *m_targetRank, *m_itemResults;
    QComboBox *m_moveScope, *m_moveGuild;
    QTabWidget* m_detailTabs;
    QLabel* m_moveCount;
    QWidget* m_positionPane;
    QSpinBox *m_level, *m_itemIndex, *m_itemCount, *m_prefix;
    QSpinBox *m_map, *m_x, *m_y, *m_z, *m_radius, *m_spacing, *m_seed;
    QPlainTextEdit* m_log;
    QPlainTextEdit* m_castleAttack;
    QLineEdit* m_castleDefense;
    QLineEdit* m_castleGuildFilter;
    QTableWidget* m_castleGuilds;
    QComboBox* m_castleAlliance;
    QWidget* m_castleTab;
    QLabel *m_castleStatus, *m_castleStartBlocked, *m_castleStartError;
    QPushButton *m_castleStart, *m_castleAdmit, *m_castleFinish, *m_castleRestore;
    QSpinBox *m_castleDeclaration, *m_castlePreparation, *m_castleAdmission, *m_castleBattle;
    QSpinBox *m_castleCaptureUnlock, *m_castleHold, *m_castleFlagHp;
    QCheckBox* m_castleAutoAdmit;
    QWidget* m_commands;
    QJsonArray m_guilds;
    int m_detailId = 0;
};
