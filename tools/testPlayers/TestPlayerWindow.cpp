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

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

namespace {
    template <class T, class... Args> T* child(QWidget* parent, Args&&... args) {
        auto object = std::make_unique<T>(std::forward<Args>(args)..., parent);
        return object.release();  // Qt's parent owns the object from construction.
    }
    QSpinBox* spin(QWidget* parent, int low, int high, int value) {
        auto* field = child<QSpinBox>(parent);
        field->setRange(low, high);
        field->setValue(value);
        return field;
    }
    QComboBox* roles(QWidget* parent) {
        auto* combo = child<QComboBox>(parent);
        for (const auto& entry : {std::pair<const char*, int>{"Temporary", 0},
                                  {"Leader", 1},
                                  {"Sub-Leader", 2},
                                  {"Head Chief", 3},
                                  {"Chief", 4},
                                  {"Official Member", 5}}) {
            combo->addItem(entry.first, entry.second);
        }
        combo->setCurrentIndex(5);
        return combo;
    }
    void cell(QTableWidget& table, int row, int column, QVariant value, int id = 0) {
        auto item = std::make_unique<QTableWidgetItem>();
        item->setData(Qt::DisplayRole, std::move(value));
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        table.setItem(row, column, item.release());
    }
    QStringList guildNames(const QString& text) {
        QStringList names;
        for (const auto& part : text.split(',')) {
            const auto name = part.trimmed();
            if (!name.isEmpty()) {
                names.push_back(name);
            }
        }
        return names;
    }
    QList<QStringList> attackAlliances(const QString& text) {
        QList<QStringList> alliances;
        for (const auto& line : text.split('\n')) {
            const auto names = guildNames(line);
            if (!names.isEmpty()) {
                alliances.push_back(names);
            }
        }
        return alliances;
    }
    int guildPosition(const QStringList& names, const QString& guild) {
        for (int index = 0; index < names.size(); ++index) {
            if (names[index].compare(guild, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
        return -1;
    }
    template <class Callback>
    QPushButton* button(QWidget* parent, QLayout* layout, QString text, Callback callback) {
        auto* result = child<QPushButton>(parent, std::move(text));
        layout->addWidget(result);
        QObject::connect(result, &QPushButton::clicked, parent, std::move(callback));
        return result;
    }
}  // namespace

TestPlayerWindow::TestPlayerWindow() {
    setWindowTitle("OpenKal — Test Players");
    resize(1450, 980);
    QSettings settings;
    auto central = std::make_unique<QWidget>();
    auto* root = central.get();
    auto* layout = std::make_unique<QVBoxLayout>(root).release();
    auto* title = child<QLabel>(root, "Test population");
    auto font = title->font();
    font.setPointSize(18);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto* connection = child<QGroupBox>(root, "Test server and graphical client");
    auto* form = std::make_unique<QFormLayout>(connection).release();
    auto* serverRow = child<QWidget>(connection);
    auto* serverLayout = std::make_unique<QHBoxLayout>(serverRow).release();
    serverLayout->setContentsMargins(0, 0, 0, 0);
    m_host = child<QLineEdit>(serverRow, settings.value("host", "127.0.0.1").toString());
    m_port = spin(serverRow, 1, 65535, settings.value("port", 30001).toInt());
    m_token = child<QLineEdit>(serverRow, qEnvironmentVariable("OPENKAL_TEST_CONTROL_TOKEN"));
    m_token->setEchoMode(QLineEdit::Password);
    m_token->setPlaceholderText("Test-control token (16+ characters)");
    serverLayout->addWidget(m_host);
    serverLayout->addWidget(m_port);
    serverLayout->addWidget(m_token, 2);
    button(serverRow, serverLayout, "Connect", [this] {
        saveSettings();
        send({{"op", "connect"},
              {"host", m_host->text()},
              {"port", m_port->value()},
              {"token", m_token->text()},
              {"ca", m_ca->text()},
              {"server_name", m_serverName->text()},
              {"players", m_playerCount->value()},
              {"guilds", m_guildCount->value()}});
    });
    form->addRow("Address / port / token", serverRow);
    auto addPath = [&](const QString& label, QLineEdit*& field, const QString& initial) {
        auto* row = child<QWidget>(connection);
        auto* line = std::make_unique<QHBoxLayout>(row).release();
        line->setContentsMargins(0, 0, 0, 0);
        field = child<QLineEdit>(row, initial);
        line->addWidget(field);
        auto* input = field;
        button(row, line, "Browse…", [this, input] {
            const auto path = QFileDialog::getOpenFileName(this, "Select file", input->text());
            if (!path.isEmpty()) {
                input->setText(path);
            }
        });
        form->addRow(label, row);
    };
    addPath("Trusted CA certificate",
            m_ca,
            settings.value("ca", QString(OPENKAL_SOURCE_ROOT) + "/save/multiplayer/tls/server.pem")
                .toString());
    m_serverName = child<QLineEdit>(connection, settings.value("server_name").toString());
    m_serverName->setPlaceholderText("Certificate name; empty uses the server address");
    form->addRow("TLS server name", m_serverName);
    addPath(
        "Client executable",
        m_client,
        settings
            .value(
                "client",
                QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../openkal_client"))
            .toString());
    addPath("Client profile", m_profile, settings.value("profile").toString());
    m_profile->setPlaceholderText("Default profile from config.pak");
    m_profile->setToolTip("Shared by autonomous play and the graphical spectator client.");
    layout->addWidget(connection);

    m_commands = child<QWidget>(root);
    auto* commands = std::make_unique<QVBoxLayout>(m_commands).release();
    commands->setContentsMargins(0, 0, 0, 0);
    auto* population = child<QWidget>(m_commands);
    auto* populationLayout = std::make_unique<QHBoxLayout>(population).release();
    populationLayout->setContentsMargins(0, 0, 0, 0);
    m_playerCount = spin(population, 0, 500, settings.value("players", 50).toInt());
    m_guildCount = spin(population, 0, 100, settings.value("guilds", 8).toInt());
    populationLayout->addWidget(child<QLabel>(population, "Active players"));
    populationLayout->addWidget(m_playerCount);
    populationLayout->addWidget(child<QLabel>(population, "Active guilds"));
    populationLayout->addWidget(m_guildCount);
    button(population, populationLayout, "Create / update population", [this] {
        saveSettings();
        send({{"op", "provision"},
              {"players", m_playerCount->value()},
              {"guilds", m_guildCount->value()}});
    });
    button(population, populationLayout, "Refresh", [this] { send({{"op", "refresh"}}); });
    button(population, populationLayout, "Move players…", [this] {
        m_moveScope->setCurrentIndex(selectedPlayers().isEmpty() ? 1 : 0);
        m_detailTabs->setCurrentWidget(m_positionPane);
        m_x->setFocus();
    });
    populationLayout->addStretch();
    commands->addWidget(population);
    commands->addWidget(child<QLabel>(
        m_commands,
        "Select rows with Ctrl / Shift. Reducing counts logs out inactive players "
        "and retains their data. Closing a spectator window keeps its player online."));

    auto* splitter = child<QSplitter>(m_commands, Qt::Horizontal);
    auto* roster = child<QWidget>(splitter);
    auto* rosterLayout = std::make_unique<QVBoxLayout>(roster).release();
    rosterLayout->setContentsMargins(0, 0, 0, 0);
    m_players = child<QTableWidget>(roster);
    m_players->setColumnCount(8);
    m_players->setHorizontalHeaderLabels(
        {"# / Account", "Character", "Class", "Level", "Guild", "Role", "Status", "ID"});
    m_players->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_players->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_players->verticalHeader()->hide();
    m_players->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_players->horizontalHeader()->setStretchLastSection(true);
    m_players->sortItems(0, Qt::AscendingOrder);
    m_players->setSortingEnabled(true);
    rosterLayout->addWidget(m_players);
    QObject::connect(m_players, &QTableWidget::itemSelectionChanged, this, [this] {
        inspectSelected();
        updatePositionCount();
    });
    auto* sessions = child<QWidget>(roster);
    auto* sessionLayout = std::make_unique<QHBoxLayout>(sessions).release();
    sessionLayout->setContentsMargins(0, 0, 0, 0);
    button(sessions, sessionLayout, "Log in selected", [this] {
        send({{"op", "login"}, {"ids", selectedPlayers()}});
    });
    button(sessions, sessionLayout, "Log out selected", [this] {
        send({{"op", "logout"}, {"ids", selectedPlayers()}});
    });
    button(sessions, sessionLayout, "Log in all", [this] {
        send({{"op", "login"}, {"ids", allPlayers()}});
    });
    button(sessions, sessionLayout, "Log out all", [this] {
        send({{"op", "logout"}, {"ids", allPlayers()}});
    });
    button(sessions, sessionLayout, "Watch player", [this] {
        if (!QFileInfo(m_client->text()).isExecutable()) {
            m_log->appendPlainText("Select an executable graphical client first.");
            return;
        }
        if (!m_profile->text().isEmpty() && !QFileInfo(m_profile->text()).isFile()) {
            m_log->appendPlainText("The selected client profile does not exist.");
            return;
        }
        saveSettings();
        send({{"op", "view"}, {"ids", selectedPlayers()}});
    });
    rosterLayout->addWidget(sessions);

    auto* details = child<QWidget>(splitter);
    auto* detailLayout = std::make_unique<QVBoxLayout>(details).release();
    detailLayout->setContentsMargins(0, 0, 0, 0);
    auto* tabs = child<QTabWidget>(details);
    m_detailTabs = tabs;
    detailLayout->addWidget(tabs);
    auto* assign = child<QGroupBox>(details, "Apply to selected players");
    auto* assignForm = std::make_unique<QFormLayout>(assign).release();
    m_targetGuild = child<QComboBox>(assign);
    m_targetGuild->addItem("No guild", 0);
    m_targetRank = roles(assign);
    assignForm->addRow("Guild", m_targetGuild);
    assignForm->addRow("Role", m_targetRank);
    button(assign, assignForm, "Move / assign role", [this] {
        send({{"op", "membership"},
              {"ids", selectedPlayers()},
              {"guild", m_targetGuild->currentData().toInt()},
              {"rank", m_targetRank->currentData().toInt()}});
    });
    m_level = spin(assign, 1, 200, 50);
    assignForm->addRow("Level", m_level);
    button(assign, assignForm, "Apply level", [this] {
        send({{"op", "edit"}, {"ids", selectedPlayers()}, {"level", m_level->value()}});
    });
    tabs->addTab(assign, "Guild / character");
    auto* autonomy = child<QGroupBox>(details, "Normal player actions; survival comes first");
    auto* autonomyForm = std::make_unique<QFormLayout>(autonomy).release();
    autonomyForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    autonomyForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto startPlayers = [this](QJsonArray ids) {
        saveSettings();
        send({{"op", "autoplay"}, {"ids", ids}, {"profile", m_profile->text()}});
    };
    button(autonomy, autonomyForm, "Start selected", [this, startPlayers] {
        startPlayers(selectedPlayers());
    });
    button(
        autonomy, autonomyForm, "Start all", [this, startPlayers] { startPlayers(allPlayers()); });
    button(autonomy, autonomyForm, "Pause selected", [this] {
        send({{"op", "pause"}, {"ids", selectedPlayers()}});
    });
    button(autonomy, autonomyForm, "Pause all", [this] {
        send({{"op", "pause"}, {"ids", allPlayers()}});
    });
    tabs->addTab(autonomy, "Autonomous play");
    auto* teleport = child<QGroupBox>(details, "Position players around a destination");
    m_positionPane = teleport;
    auto* teleForm = std::make_unique<QFormLayout>(teleport).release();
    m_moveScope = child<QComboBox>(teleport);
    m_moveScope->addItems({"Selected players", "All players", "Guild"});
    m_moveGuild = child<QComboBox>(teleport);
    m_moveGuild->addItem("No guild", 0);
    m_moveGuild->setEnabled(false);
    m_moveCount = child<QLabel>(teleport);
    teleForm->addRow("Move", m_moveScope);
    teleForm->addRow("Guild", m_moveGuild);
    teleForm->addRow(m_moveCount);
    QObject::connect(m_moveScope, &QComboBox::currentIndexChanged, this, [this](int scope) {
        m_moveGuild->setEnabled(scope == 2);
        updatePositionCount();
    });
    QObject::connect(
        m_moveGuild, &QComboBox::currentIndexChanged, this, [this] { updatePositionCount(); });
    m_map = spin(teleport, 0, 255, 0);
    m_x = spin(teleport, 0, 524287, 257285);
    m_y = spin(teleport, 0, 524287, 258744);
    m_z = spin(teleport, -100000, 100000, 16100);
    m_radius = spin(teleport, 0, 4096, 160);
    m_spacing = spin(teleport, 0, 1000, 24);
    m_radius->setToolTip("Random horizontal offsets within this radius, in server coordinates.");
    m_spacing->setToolTip(
        "Minimum distance between the moved players. Increase the radius for larger groups.");
    m_seed = spin(teleport, 1, 2000000000, 1);
    teleForm->addRow("Map", m_map);
    teleForm->addRow("X", m_x);
    teleForm->addRow("Y", m_y);
    teleForm->addRow("Z / floor height", m_z);
    teleForm->addRow("Scatter radius", m_radius);
    teleForm->addRow("Minimum spacing", m_spacing);
    teleForm->addRow("Random seed", m_seed);
    button(teleport, teleForm, "Move to destination", [this] {
        const auto ids = positionTargets();
        if (ids.isEmpty()) {
            m_log->appendPlainText("No players match the chosen move target. Select players, all "
                                   "players or a populated guild.");
            return;
        }
        send({{"op", "teleport"},
              {"ids", ids},
              {"map", m_map->value()},
              {"x", m_x->value()},
              {"y", m_y->value()},
              {"z", m_z->value()},
              {"radius", m_radius->value()},
              {"spacing", m_spacing->value()},
              {"seed", m_seed->value()}});
    });
    tabs->addTab(teleport, "Move / teleport");
    addCastleWarTab(*tabs);
    updatePositionCount();
    splitter->addWidget(roster);
    splitter->addWidget(details);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    commands->addWidget(splitter, 1);
    auto* inventory = child<QGroupBox>(
        m_commands,
        "Inventory — select one player to inspect; grants apply to all selected players");
    auto* inventoryLayout = std::make_unique<QVBoxLayout>(inventory).release();
    auto* searchRow = child<QWidget>(inventory);
    auto* searchLayout = std::make_unique<QHBoxLayout>(searchRow).release();
    auto* search = child<QLineEdit>(searchRow);
    search->setPlaceholderText("Search server items by name or definition ID (first 100 matches)");
    m_itemResults = child<QComboBox>(searchRow);
    searchLayout->addWidget(search, 2);
    searchLayout->addWidget(m_itemResults, 2);
    button(searchRow, searchLayout, "Find items", [this, search] {
        send({{"op", "catalog"}, {"query", search->text()}});
    });
    inventoryLayout->addWidget(searchRow);
    QObject::connect(m_itemResults, &QComboBox::activated, this, [this] {
        m_itemIndex->setValue(m_itemResults->currentData().toInt());
    });
    m_items = child<QTableWidget>(inventory);
    m_items->setColumnCount(5);
    m_items->setHorizontalHeaderLabels(
        {"Instance ID", "Item definition", "Count", "Prefix", "Wear slot (0 = bag)"});
    m_items->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_items->setSelectionMode(QAbstractItemView::SingleSelection);
    m_items->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_items->sortItems(4, Qt::AscendingOrder);
    m_items->setSortingEnabled(true);
    m_items->setMaximumHeight(150);
    inventoryLayout->addWidget(m_items);
    auto* itemsRow = child<QWidget>(inventory);
    auto* itemsLine = std::make_unique<QHBoxLayout>(itemsRow).release();
    m_itemIndex = spin(itemsRow, 1, 65535, 1);
    m_itemCount = spin(itemsRow, 1, 1000000, 1);
    m_prefix = spin(itemsRow, 0, 255, 0);
    itemsLine->addWidget(child<QLabel>(itemsRow, "Item ID"));
    itemsLine->addWidget(m_itemIndex);
    itemsLine->addWidget(child<QLabel>(itemsRow, "Count"));
    itemsLine->addWidget(m_itemCount);
    itemsLine->addWidget(child<QLabel>(itemsRow, "Prefix"));
    itemsLine->addWidget(m_prefix);
    button(itemsRow, itemsLine, "Give item", [this] {
        send({{"op", "give"},
              {"ids", selectedPlayers()},
              {"item", m_itemIndex->value()},
              {"count", m_itemCount->value()},
              {"prefix", m_prefix->value()}});
    });
    for (const auto& action : {std::pair<const char*, const char*>{"Equip", "equip"},
                               {"Unequip", "unequip"},
                               {"Remove item", "remove"}}) {
        const QString operation = action.second;
        button(itemsRow, itemsLine, action.first, [this, operation] {
            const auto selectedItems = m_items->selectionModel()->selectedRows(0);
            if (selectedItems.isEmpty() || selectedPlayers().size() != 1 ||
                selectedPlayers().first().toInt() != m_detailId) {
                return;
            }
            send({{"op", operation},
                  {"ids", selectedPlayers()},
                  {"iid", selectedItems.front().data(Qt::DisplayRole).toLongLong()}});
        });
    }
    button(itemsRow, itemsLine, "Refresh inventory", [this] { inspectSelected(); });
    inventoryLayout->addWidget(itemsRow);
    commands->addWidget(inventory);
    layout->addWidget(m_commands, 1);
    m_log = child<QPlainTextEdit>(root);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(300);
    m_log->setMaximumHeight(90);
    layout->addWidget(m_log);
    m_log->appendPlainText("Enable the server with OPENKAL_TEST_CONTROL_TOKEN and enter the same "
                           "token here. Accounts use test0001 / TestPassword0001, and so on.");
    setCentralWidget(central.release());
    QObject::connect(&m_backend,
                     &TestPlayerBackend::snapshot,
                     this,
                     &TestPlayerWindow::updateState,
                     Qt::QueuedConnection);
    QObject::connect(
        &m_backend,
        &TestPlayerBackend::message,
        this,
        [this](const QString& text) {
            m_log->appendPlainText(QTime::currentTime().toString("HH:mm:ss ") + text);
            statusBar()->showMessage(text);
        },
        Qt::QueuedConnection);
    QObject::connect(
        &m_backend,
        &TestPlayerBackend::busyChanged,
        this,
        [this](bool busy) {
            m_commands->setEnabled(!busy);
            statusBar()->showMessage(busy ? "Working…" : "Ready");
        },
        Qt::QueuedConnection);
    QObject::connect(&m_backend,
                     &TestPlayerBackend::openClient,
                     this,
                     &TestPlayerWindow::launchClient,
                     Qt::QueuedConnection);
}

TestPlayerWindow::~TestPlayerWindow() {
    for (auto* process : findChildren<QProcess*>()) {
        process->terminate();
        if (!process->waitForFinished(2000)) {
            process->kill();
            process->waitForFinished(1000);
        }
    }
    saveSettings();
}

void TestPlayerWindow::saveSettings() {
    QSettings settings;
    settings.setValue("host", m_host->text());
    settings.setValue("port", m_port->value());
    settings.setValue("ca", m_ca->text());
    settings.setValue("server_name", m_serverName->text());
    settings.setValue("client", m_client->text());
    settings.setValue("profile", m_profile->text());
    settings.setValue("players", m_playerCount->value());
    settings.setValue("guilds", m_guildCount->value());
}

QJsonArray TestPlayerWindow::selectedPlayers() const {
    QJsonArray ids;
    for (const auto& index : m_players->selectionModel()->selectedRows()) {
        ids.append(m_players->item(index.row(), 0)->data(Qt::UserRole).toInt());
    }
    return ids;
}

QJsonArray TestPlayerWindow::allPlayers() const {
    QJsonArray ids;
    for (int row = 0; row < m_players->rowCount(); ++row) {
        if (const auto* item = m_players->item(row, 0)) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

QJsonArray TestPlayerWindow::positionTargets() const {
    if (m_moveScope->currentIndex() == 0) {
        return selectedPlayers();
    }
    if (m_moveScope->currentIndex() == 1) {
        return allPlayers();
    }
    QJsonArray ids;
    const int guild = m_moveGuild->currentData().toInt();
    for (int row = 0; row < m_players->rowCount(); ++row) {
        const auto* item = m_players->item(row, 0);
        if (item && item->data(Qt::UserRole + 1).toInt() == guild) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

void TestPlayerWindow::updatePositionCount() {
    m_moveCount->setText(QString("Players to move: %1").arg(positionTargets().size()));
}

void TestPlayerWindow::addCastleWarTab(QTabWidget& tabs) {
    auto* tab = child<QScrollArea>(&tabs);
    m_castleTab = tab;
    tab->setWidgetResizable(true);
    auto* content = child<QWidget>(tab);
    auto* form = std::make_unique<QFormLayout>(content).release();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_castleStatus = child<QLabel>(content);
    m_castleStatus->setTextFormat(Qt::PlainText);
    m_castleStatus->setWordWrap(true);
    form->addRow(m_castleStatus);

    auto* instructions = child<QLabel>(
        content,
        "Select a server guild and an alliance. The first guild in each alliance leads it.");
    instructions->setWordWrap(true);
    form->addRow(instructions);

    auto* guildSearch = child<QWidget>(content);
    auto* searchLayout = std::make_unique<QHBoxLayout>(guildSearch).release();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    m_castleGuildFilter = child<QLineEdit>(guildSearch);
    m_castleGuildFilter->setPlaceholderText("Filter guilds by name");
    searchLayout->addWidget(m_castleGuildFilter);
    button(guildSearch, searchLayout, "Refresh guilds", [this] {
        send({{"op", "castle_war_guilds"}});
    });
    form->addRow("Server guilds", guildSearch);

    m_castleGuilds = child<QTableWidget>(content);
    m_castleGuilds->setColumnCount(2);
    m_castleGuilds->setHorizontalHeaderLabels({"Guild", "Type"});
    m_castleGuilds->verticalHeader()->hide();
    m_castleGuilds->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_castleGuilds->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_castleGuilds->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_castleGuilds->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_castleGuilds->setSelectionMode(QAbstractItemView::SingleSelection);
    m_castleGuilds->setMinimumHeight(130);
    m_castleGuilds->setMaximumHeight(170);
    m_castleGuilds->sortItems(0, Qt::AscendingOrder);
    m_castleGuilds->setSortingEnabled(true);
    form->addRow(m_castleGuilds);
    QObject::connect(
        m_castleGuildFilter, &QLineEdit::textChanged, this, &TestPlayerWindow::filterCastleGuilds);
    QObject::connect(m_castleGuilds->model(), &QAbstractItemModel::layoutChanged, this, [this] {
        filterCastleGuilds(m_castleGuildFilter->text());
    });

    m_castleAlliance = child<QComboBox>(content);
    m_castleAttack = child<QPlainTextEdit>(content);
    m_castleAttack->setPlaceholderText("MyGuild, AllyGuild");
    m_castleAttack->setMaximumHeight(78);
    m_castleDefense = child<QLineEdit>(content);
    m_castleDefense->setPlaceholderText("DefenderGuild, AllyGuild");

    form->addRow("Alliance", m_castleAlliance);
    auto* guildActions = child<QWidget>(content);
    auto* guildActionsLayout = std::make_unique<QHBoxLayout>(guildActions).release();
    guildActionsLayout->setContentsMargins(0, 0, 0, 0);
    const auto selectedGuild = [this] {
        const auto rows = m_castleGuilds->selectionModel()->selectedRows(0);
        return rows.isEmpty() ? QString{} : rows.front().data().toString();
    };
    auto* setLeader =
        button(guildActions, guildActionsLayout, "Set as leader", [this, selectedGuild] {
            assignCastleGuild(selectedGuild(), m_castleAlliance->currentData().toInt(), true);
        });
    auto* addPartner =
        button(guildActions, guildActionsLayout, "Add as partner", [this, selectedGuild] {
            assignCastleGuild(selectedGuild(), m_castleAlliance->currentData().toInt(), false);
        });
    auto* newAlliance =
        button(guildActions, guildActionsLayout, "New attack alliance", [this, selectedGuild] {
            assignCastleGuild(
                selectedGuild(), attackAlliances(m_castleAttack->toPlainText()).size(), true);
        });
    const auto updateGuildActions = [this, selectedGuild, setLeader, addPartner, newAlliance] {
        const bool selected = !selectedGuild().isEmpty();
        const int alliance = m_castleAlliance->currentData().toInt();
        const bool destinationHasLeader =
            alliance < 0 ? !guildNames(m_castleDefense->text()).isEmpty()
                         : alliance < attackAlliances(m_castleAttack->toPlainText()).size();
        setLeader->setEnabled(selected);
        addPartner->setEnabled(selected && destinationHasLeader);
        newAlliance->setEnabled(selected);
    };
    QObject::connect(m_castleGuilds, &QTableWidget::itemSelectionChanged, this, updateGuildActions);
    QObject::connect(m_castleAlliance, &QComboBox::currentIndexChanged, this, updateGuildActions);
    form->addRow(guildActions);

    QObject::connect(
        m_castleAttack, &QPlainTextEdit::textChanged, this, [this, updateGuildActions] {
            updateCastleAlliances();
            updateGuildActions();
        });
    QObject::connect(m_castleDefense, &QLineEdit::textChanged, this, [this, updateGuildActions] {
        updateCastleAlliances();
        updateGuildActions();
    });
    form->addRow("Attack alliances", m_castleAttack);
    button(content, form, "Use selected player's guild as lead", [this] {
        const auto rows = m_players->selectionModel()->selectedRows(4);
        if (rows.size() != 1 ||
            !m_players->item(rows.front().row(), 0)->data(Qt::UserRole + 1).toInt()) {
            m_log->appendPlainText("Select one player who belongs to a guild.");
            return;
        }
        assignCastleGuild(rows.front().data().toString(), 0, true);
    });
    form->addRow("Defense alliance", m_castleDefense);
    updateCastleAlliances();
    updateGuildActions();

    m_castleDeclaration = spin(content, 0, 600, 5);
    m_castlePreparation = spin(content, 0, 600, 5);
    m_castleAdmission = spin(content, 1, 600, 30);
    m_castleBattle = spin(content, 1, 3600, 120);
    m_castleCaptureUnlock = spin(content, 0, m_castleBattle->value() - 1, 0);
    m_castleHold = spin(content, 1, 3600, 10);
    for (auto* time : {m_castleDeclaration,
                       m_castlePreparation,
                       m_castleAdmission,
                       m_castleBattle,
                       m_castleCaptureUnlock,
                       m_castleHold}) {
        time->setSuffix(" s");
    }
    QObject::connect(m_castleBattle, &QSpinBox::valueChanged, content, [this](int value) {
        m_castleCaptureUnlock->setMaximum(value - 1);
    });
    m_castleFlagHp = spin(content, 1, 1000000, 100);
    form->addRow("Declaration", m_castleDeclaration);
    form->addRow("Preparation", m_castlePreparation);
    form->addRow("Admission", m_castleAdmission);
    form->addRow("Battle", m_castleBattle);
    form->addRow("Flag capture unlock", m_castleCaptureUnlock);
    form->addRow("Early victory hold", m_castleHold);
    form->addRow("Flag HP", m_castleFlagHp);
    m_castleAutoAdmit = child<QCheckBox>(content, "Auto-admit eligible online players");
    m_castleAutoAdmit->setChecked(true);
    form->addRow(m_castleAutoAdmit);

    m_castleStart = button(content, form, "Start simulation", [this] { startCastleWar(); });
    m_castleStartBlocked = child<QLabel>(content);
    m_castleStartError = child<QLabel>(content);
    for (auto* notice : {m_castleStartBlocked, m_castleStartError}) {
        notice->setTextFormat(Qt::PlainText);
        notice->setWordWrap(true);
        form->addRow(notice);
    }
    showCastleStartError({});
    const auto clearStartError = [this] { showCastleStartError({}); };
    QObject::connect(m_castleAttack, &QPlainTextEdit::textChanged, this, clearStartError);
    QObject::connect(m_castleDefense, &QLineEdit::textChanged, this, clearStartError);
    m_castleAdmit = button(content, form, "Admit online players", [this] {
        send({{"op", "castle_war"}, {"action", "admit"}});
    });
    m_castleFinish = button(content, form, "Finish battle now", [this] {
        send({{"op", "castle_war"}, {"action", "finish"}});
    });
    m_castleRestore = button(content, form, "Restore regular schedule", [this] {
        send({{"op", "castle_war"}, {"action", "restore"}});
    });
    auto* timer = child<QTimer>(tab);
    timer->setInterval(1000);
    const auto poll = [this] { send({{"op", "castle_war_poll"}}); };
    QObject::connect(timer, &QTimer::timeout, tab, poll);
    QObject::connect(
        &tabs, &QTabWidget::currentChanged, tab, [this, &tabs, tab, timer, poll](int index) {
            if (tabs.widget(index) == tab) {
                poll();
                send({{"op", "castle_war_guilds"}});
                timer->start();
            } else {
                timer->stop();
            }
        });
    tab->setWidget(content);
    tabs.addTab(tab, "Castle War");
    updateCastleWar({});
}

void TestPlayerWindow::updateCastleGuilds(const QJsonArray& guilds) {
    const auto selection = m_castleGuilds->selectionModel()->selectedRows(0);
    const qint64 selectedId =
        selection.isEmpty() ? 0 : selection.front().data(Qt::UserRole).toLongLong();
    m_castleGuilds->setSortingEnabled(false);
    m_castleGuilds->setRowCount(0);
    m_castleGuilds->setRowCount(guilds.size());
    for (int row = 0; row < guilds.size(); ++row) {
        const auto guild = guilds[row].toObject();
        cell(*m_castleGuilds, row, 0, guild.value("name").toString());
        m_castleGuilds->item(row, 0)->setData(Qt::UserRole, guild.value("id").toInteger());
        cell(*m_castleGuilds, row, 1, guild.value("managed").toBool() ? "Test pool" : "Regular");
    }
    m_castleGuilds->setSortingEnabled(true);
    filterCastleGuilds(m_castleGuildFilter->text());
    if (selectedId != 0) {
        for (int row = 0; row < m_castleGuilds->rowCount(); ++row) {
            if (!m_castleGuilds->isRowHidden(row) &&
                m_castleGuilds->item(row, 0)->data(Qt::UserRole).toLongLong() == selectedId) {
                m_castleGuilds->selectRow(row);
                break;
            }
        }
    }
}

void TestPlayerWindow::filterCastleGuilds(const QString& query) {
    const auto filter = query.trimmed();
    for (int row = 0; row < m_castleGuilds->rowCount(); ++row) {
        const auto* guild = m_castleGuilds->item(row, 0);
        const bool hidden = !guild || !guild->text().contains(filter, Qt::CaseInsensitive);
        m_castleGuilds->setRowHidden(row, hidden);
        if (hidden && guild && guild->isSelected()) {
            m_castleGuilds->clearSelection();
        }
    }
}

void TestPlayerWindow::updateCastleAlliances() {
    const int selected = m_castleAlliance->currentData().toInt();
    const auto attack = attackAlliances(m_castleAttack->toPlainText());
    const auto defense = guildNames(m_castleDefense->text());
    const QSignalBlocker block(m_castleAlliance);
    m_castleAlliance->clear();
    if (attack.isEmpty()) {
        m_castleAlliance->addItem("Attack alliance 1", 0);
    }
    for (int index = 0; index < attack.size(); ++index) {
        m_castleAlliance->addItem(
            QString("Attack alliance %1 — %2").arg(index + 1).arg(attack[index].front()), index);
    }
    m_castleAlliance->addItem(
        defense.isEmpty() ? "Defense alliance" : "Defense alliance — " + defense.front(), -1);
    m_castleAlliance->setCurrentIndex(std::max(0, m_castleAlliance->findData(selected)));
}

void TestPlayerWindow::assignCastleGuild(const QString& guild, int alliance, bool asLeader) {
    if (guild.isEmpty()) {
        return;
    }
    auto attack = attackAlliances(m_castleAttack->toPlainText());
    auto defense = guildNames(m_castleDefense->text());
    if (alliance >= attack.size()) {
        attack.emplaceBack();
    }
    auto& target = alliance < 0 ? defense : attack[alliance];
    if (!asLeader && guildPosition(target, guild) >= 0) {
        return;
    }
    const auto removeGuild = [&](QStringList& names) {
        names.removeIf(
            [&](const QString& name) { return name.compare(guild, Qt::CaseInsensitive) == 0; });
    };
    for (auto& group : attack) {
        removeGuild(group);
    }
    removeGuild(defense);
    if (asLeader) {
        target.prepend(guild);
    } else {
        target.append(guild);
    }
    QStringList lines;
    const int targetAlliance = alliance;
    for (int index = 0; index < attack.size(); ++index) {
        if (!attack[index].isEmpty()) {
            lines.push_back(attack[index].join(", "));
        } else if (index < targetAlliance) {
            --alliance;
        }
    }
    m_castleAttack->setPlainText(lines.join('\n'));
    m_castleDefense->setText(defense.join(", "));
    m_castleAlliance->setCurrentIndex(m_castleAlliance->findData(alliance));
}

QJsonObject TestPlayerWindow::castleWarStartRequest() const {
    QJsonArray attack;
    for (const auto& group : attackAlliances(m_castleAttack->toPlainText())) {
        attack.append(QJsonArray::fromStringList(group));
    }
    return {{"op", "castle_war"},
            {"action", "start"},
            {"attack", attack},
            {"defense", QJsonArray::fromStringList(guildNames(m_castleDefense->text()))},
            {"declaration_seconds", m_castleDeclaration->value()},
            {"preparation_seconds", m_castlePreparation->value()},
            {"admission_seconds", m_castleAdmission->value()},
            {"battle_seconds", m_castleBattle->value()},
            {"capture_unlock_seconds", m_castleCaptureUnlock->value()},
            {"hold_seconds", m_castleHold->value()},
            {"flag_hit_points", m_castleFlagHp->value()},
            {"auto_admit", m_castleAutoAdmit->isChecked()}};
}

void TestPlayerWindow::startCastleWar() {
    auto request = castleWarStartRequest();
    auto attack = request.value("attack").toArray();
    if (attack.isEmpty()) {
        showCastleStartError("Enter at least one attacking guild.");
        m_castleAttack->setFocus();
        return;
    }
    attack.append(request.value("defense"));
    QSet<QString> assigned;
    for (const auto& group : attack) {
        for (const auto& value : group.toArray()) {
            const auto name = value.toString();
            const auto key = name.toCaseFolded();
            if (assigned.contains(key)) {
                showCastleStartError(QString("Assign %1 to only one alliance.").arg(name));
                return;
            }
            assigned.insert(key);
        }
    }
    showCastleStartError({});
    send(std::move(request));
}

void TestPlayerWindow::showCastleStartError(const QString& message) {
    m_castleStartError->setText(message);
    m_castleStartError->setVisible(!message.isEmpty());
}

void TestPlayerWindow::updateCastleWar(const QJsonObject& state) {
    const bool available = state["available"].toBool();
    const bool simulation = state["simulation"].toBool();
    const bool restoring = state["restoring"].toBool();
    const auto phase = state["phase"].toString();
    const auto serverBlocker = state.value("start_blocked");
    auto startBlocked = serverBlocker.toString();
    if (!available && startBlocked.isEmpty()) {
        startBlocked = state["error"].toString(
            state.contains("available")
                ? "Enable Castle War and configure castle_war_file on the server."
                : "Connect to the test server to start a simulation.");
    } else if (!serverBlocker.isString()) {
        if (restoring) {
            startBlocked = "Wait for the regular schedule to finish restoring.";
        } else if (simulation) {
            startBlocked = "Restore the regular schedule before starting another simulation.";
        } else if (phase != "Idle" && phase != "Declaration" && phase != "Preparation") {
            startBlocked = "Wait for the regular siege and participant returns to finish.";
        }
    }
    m_castleStart->setEnabled(startBlocked.isEmpty());
    m_castleStartBlocked->setText(startBlocked);
    m_castleStartBlocked->setVisible(!startBlocked.isEmpty());
    m_castleAdmit->setEnabled(simulation && !restoring && phase == "Admission");
    m_castleFinish->setEnabled(simulation && !restoring && phase == "Battle");
    m_castleRestore->setEnabled(simulation && !restoring);
    if (!available) {
        m_castleStatus->setText(startBlocked);
        return;
    }
    if (restoring) {
        m_castleStatus->setText("Restoring the regular schedule after players leave the arena.");
        return;
    }
    QString status =
        (simulation ? QString("Simulation") : QString("Regular schedule")) + " · " + phase;
    if (state["seconds_remaining"].toInt() > 0) {
        status += QString(" · %1 s remaining").arg(state["seconds_remaining"].toInt());
    }
    const auto owner = state["owner"].toString();
    status += QString("\nAttack %1   Defense %2\nCastle owner: %3   Required level: %4")
                  .arg(state["attack_score"].toInt())
                  .arg(state["defense_score"].toInt())
                  .arg(owner.isEmpty() ? "Unclaimed" : owner)
                  .arg(state["minimum_level"].toInt());
    if (const auto winner = state["winner"].toString(); !winner.isEmpty()) {
        status += "\nWinner: " + winner;
    }
    m_castleStatus->setText(status);
}

void TestPlayerWindow::send(QJsonObject command) {
    m_backend.submit(std::move(command));
}

void TestPlayerWindow::inspectSelected() {
    const auto ids = selectedPlayers();
    const int character = ids.size() == 1 ? ids.first().toInt() : 0;
    if (character != m_detailId) {
        m_items->setRowCount(0);
        m_detailId = character;
    }
    if (m_detailId) {
        send({{"op", "inspect"}, {"id", m_detailId}});
    }
}

void TestPlayerWindow::updateState(const QJsonObject& state) {
    if (state.contains("castle_war_guilds")) {
        updateCastleGuilds(state.value("castle_war_guilds").toArray());
    }
    if (state.value("castle_war_guild_catalog_reload").toBool()) {
        showCastleStartError({});
        if (m_detailTabs->currentWidget() == m_castleTab) {
            send({{"op", "castle_war_guilds"}});
        }
    }
    if (state.contains("castle_war_guilds") || state.contains("castle_war_guild_catalog_reload")) {
        return;
    }
    if (state.contains("castle_war_start_error")) {
        showCastleStartError(state.value("castle_war_start_error").toString());
        return;
    }
    if (state.contains("castle_war")) {
        updateCastleWar(state["castle_war"].toObject());
        return;
    }
    if (state.contains("catalog")) {
        m_itemResults->clear();
        for (const auto& value : state["catalog"].toArray()) {
            const auto item = value.toObject();
            m_itemResults->addItem(
                QString("%1 — %2").arg(item["index"].toInt()).arg(item["name"].toString()),
                item["index"].toInt());
        }
        if (m_itemResults->count()) {
            m_itemIndex->setValue(m_itemResults->currentData().toInt());
        }
        return;
    }
    if (state.contains("detail")) {
        if (state["id"].toInt() != m_detailId) {
            return;
        }
        const auto selected = m_items->selectionModel()->selectedRows(0);
        const auto previous = selected.isEmpty() ? 0 : selected.front().data().toLongLong();
        const QSignalBlocker blocker(m_items);
        m_items->setSortingEnabled(false);
        m_items->setRowCount(0);
        const auto items = state["detail"].toObject()["items"].toArray();
        m_items->setRowCount(items.size());
        for (int row = 0; row < items.size(); ++row) {
            const auto item = items[row].toObject();
            int col = 0;
            for (const auto* key : {"iid", "index", "count", "prefix", "slot"}) {
                cell(*m_items, row, col++, item[key].toInteger());
            }
        }
        m_items->setSortingEnabled(true);
        for (int row = 0; previous && row < m_items->rowCount(); ++row) {
            if (m_items->item(row, 0)->data(Qt::DisplayRole).toLongLong() == previous) {
                m_items->selectRow(row);
                break;
            }
        }
        return;
    }
    const auto selected = selectedPlayers();
    const auto* current = m_players->item(m_players->currentRow(), 0);
    const int currentId = current ? current->data(Qt::UserRole).toInt() : 0;
    const QSignalBlocker blocker(m_players);
    m_guilds = state["guilds"].toArray();
    const int guildSelection = m_targetGuild->currentData().toInt();
    m_targetGuild->clear();
    m_targetGuild->addItem("No guild", 0);
    for (const auto& group : m_guilds) {
        const auto guild = group.toObject();
        if (guild["test_ordinal"].toInt() <= m_guildCount->value()) {
            m_targetGuild->addItem(guild["name"].toString(), guild["id"].toInt());
        }
    }
    m_targetGuild->setCurrentIndex(std::max(0, m_targetGuild->findData(guildSelection)));
    const QSignalBlocker moveGuildBlocker(m_moveGuild);
    const int moveGuild = m_moveGuild->currentData().toInt();
    m_moveGuild->clear();
    m_moveGuild->addItem("No guild", 0);
    for (const auto& group : m_guilds) {
        const auto guild = group.toObject();
        if (guild["test_ordinal"].toInt() <= m_guildCount->value()) {
            m_moveGuild->addItem(guild["name"].toString(), guild["id"].toInt());
        }
    }
    m_moveGuild->setCurrentIndex(std::max(0, m_moveGuild->findData(moveGuild)));
    const auto players = state["players"].toArray();
    m_players->setSortingEnabled(false);
    m_players->setRowCount(0);
    m_players->setRowCount(players.size());
    for (int row = 0; row < players.size(); ++row) {
        const auto player = players[row].toObject();
        const int id = player["id"].toInt();
        cell(*m_players,
             row,
             0,
             QString::fromStdString(
                 openkal::server::test_identity("test", player["ordinal"].toInt())),
             id);
        m_players->item(row, 0)->setData(Qt::UserRole + 1, player["guild"].toInt());
        cell(*m_players, row, 1, player["name"].toString());
        const QStringList classes{"Knight", "Mage", "Archer", "Thief"};
        cell(*m_players, row, 2, classes.value(player["class"].toInt()));
        cell(*m_players, row, 3, player["level"].toInt());
        auto* guildCombo = child<QComboBox>(m_players);
        guildCombo->addItem("No guild", 0);
        for (const auto& group : m_guilds) {
            const auto guild = group.toObject();
            if (guild["test_ordinal"].toInt() <= m_guildCount->value() ||
                guild["id"] == player["guild"]) {
                guildCombo->addItem(guild["name"].toString(), guild["id"].toInt());
            }
        }
        guildCombo->setCurrentIndex(std::max(0, guildCombo->findData(player["guild"].toInt())));
        cell(*m_players, row, 4, guildCombo->currentText());
        m_players->setCellWidget(row, 4, guildCombo);
        auto* role = roles(m_players);
        role->setCurrentIndex(std::max(0, role->findData(player["rank"].toInt())));
        cell(*m_players, row, 5, role->currentText());
        m_players->setCellWidget(row, 5, role);
        QObject::connect(guildCombo, &QComboBox::activated, this, [this, id, guildCombo, role] {
            send({{"op", "membership"},
                  {"ids", QJsonArray{id}},
                  {"guild", guildCombo->currentData().toInt()},
                  {"rank", role->currentData().toInt()}});
        });
        QObject::connect(role, &QComboBox::activated, this, [this, id, guildCombo, role] {
            send({{"op", "membership"},
                  {"ids", QJsonArray{id}},
                  {"guild", guildCombo->currentData().toInt()},
                  {"rank", role->currentData().toInt()}});
        });
        const auto activity = player["activity"].toString();
        cell(*m_players,
             row,
             6,
             player["status"].toString() + (activity.isEmpty() ? QString{} : " — " + activity));
        cell(*m_players, row, 7, id);
    }
    m_players->setSortingEnabled(true);
    for (int row = 0; row < m_players->rowCount(); ++row) {
        const auto index = m_players->model()->index(row, 0);
        const int id = index.data(Qt::UserRole).toInt();
        if (selected.contains(id)) {
            m_players->selectionModel()->select(
                index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        if (currentId == id) {
            m_players->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }
    updatePositionCount();
}

void TestPlayerWindow::launchClient(const QString& endpoint) {
    auto* process = child<QProcess>(this);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("OPENKAL_TEST_CONTROL_TOKEN");
    environment.insert("OPENKAL_TEST_ATTACH", endpoint);
    environment.insert("OPENKAL_SESSION_READ_ONLY", "1");
    environment.insert("OPENKAL_SAVE_DIR", QFileInfo(endpoint).absolutePath() + "/client-save");
    process->setProcessEnvironment(environment);
    process->setProgram(m_client->text());
    if (!m_profile->text().isEmpty()) {
        process->setArguments({m_profile->text()});
    }
    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [process] {
        process->readAllStandardOutput();
    });
    QObject::connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        const auto text = QString::fromUtf8(process->readAllStandardError());
        if (text.contains("error", Qt::CaseInsensitive)) {
            m_log->appendPlainText(text.right(1000));
        }
    });
    auto closed = [this, endpoint] { send({{"op", "view_closed"}, {"endpoint", endpoint}}); };
    QObject::connect(process, &QProcess::errorOccurred, this, [this, process, closed] {
        m_log->appendPlainText("Client could not start: " + process->errorString());
        closed();
    });
    QObject::connect(
        process,
        &QProcess::finished,
        this,
        [this, process, closed](int code, QProcess::ExitStatus) {
            m_log->appendPlainText(
                QString("Spectator window closed (exit %1). The managed session remains online.")
                    .arg(code));
            closed();
            process->deleteLater();
        });
    process->start();
}
