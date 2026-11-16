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

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace {
    // Qt parents own the returned children; raw results are non-owning widget handles.
    template <class T, class... Args> T* child(QWidget* parent, Args&&... args) {
        auto widget = std::make_unique<T>(std::forward<Args>(args)..., parent);
        return widget.release();
    }
    const QStringList roles = {"Player", "Moderator", "Co-Admin", "Admin"};
    QVBoxLayout* vertical(QWidget* parent) {
        auto layout = std::make_unique<QVBoxLayout>(parent);
        return layout.release();
    }
    QHBoxLayout* horizontal(QWidget* parent) {
        auto layout = std::make_unique<QHBoxLayout>(parent);
        return layout.release();
    }
    QString role_name(int role) {
        return role >= 0 && role < roles.size() ? roles[role] : "Unknown";
    }
}  // namespace
ManagementWindow::ManagementWindow(QWidget* parent)
    : QMainWindow(parent), m_backend(std::make_unique<ManagementBackend>()) {
    setWindowTitle("OpenKal Management Console");
    resize(1240, 860);
    setMinimumSize(1000, 700);
    auto* central = child<QWidget>(this);
    setCentralWidget(central);
    auto* main = vertical(central);
    main->setContentsMargins(20, 16, 20, 16);
    main->setSpacing(12);
    auto* heading = child<QLabel>(central, "OpenKal  /  Management Console");
    auto font = heading->font();
    font.setPointSize(19);
    font.setBold(true);
    heading->setFont(font);
    main->addWidget(heading);
    m_connection = child<QLabel>(
        central, "Connect to the local server API; database settings are supplied by the server.");
    main->addWidget(m_connection);
    auto* connection = child<QGroupBox>(central, "Connection");
    auto form = std::make_unique<QFormLayout>(connection);
    m_dsn = child<QLineEdit>(connection, qEnvironmentVariable("OPENKAL_DATABASE_DSN"));
    m_dsn->setObjectName("databaseDsn");
    m_dsn->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_dsn->setPlaceholderText("Leave empty to use the server's database settings");
    m_socket = child<QLineEdit>(
        connection, QSettings{}.value("socket", "/tmp/openkal-management.sock").toString());
    m_socket->setObjectName("managementSocket");
    m_token = child<QLineEdit>(connection, qEnvironmentVariable("OPENKAL_MANAGEMENT_TOKEN"));
    m_token->setEchoMode(QLineEdit::Password);
    m_token->setPlaceholderText("Management token (32–256 bytes; never saved)");
    form->addRow("Local API socket", m_socket);
    form->addRow("API token", m_token);
    form->addRow("Database override (optional)", m_dsn);
    auto* buttons = child<QWidget>(connection);
    auto* controls = horizontal(buttons);
    controls->setContentsMargins(0, 0, 0, 0);
    auto* connectButton = child<QPushButton>(buttons, "Connect");
    connectButton->setObjectName("connectButton");
    auto* disconnect = child<QPushButton>(buttons, "Disconnect");
    auto* refresh = child<QPushButton>(buttons, "Refresh");
    controls->addWidget(connectButton);
    controls->addWidget(disconnect);
    controls->addWidget(refresh);
    controls->addStretch();
    auto* privacy = child<QLabel>(buttons, "Connection secrets stay in memory.");
    controls->addWidget(privacy);
    form->addRow(buttons);
    form.release();
    main->addWidget(connection);
    connect(connectButton, &QPushButton::clicked, this, [this] {
        m_messages = {};
        m_search->clear();
        m_backend->submit({{"op", "connect"},
                           {"dsn", m_dsn->text()},
                           {"socket", m_socket->text()},
                           {"token", m_token->text()}});
    });
    connect(disconnect, &QPushButton::clicked, this, [this] {
        m_backend->submit({{"op", "disconnect"}});
    });
    connect(
        refresh, &QPushButton::clicked, this, [this] { m_backend->submit({{"op", "refresh"}}); });
    auto* split = child<QSplitter>(central, Qt::Horizontal);
    auto* roster = child<QWidget>(split);
    auto* rosterLayout = vertical(roster);
    rosterLayout->setContentsMargins(0, 0, 8, 0);
    auto* rosterTitle = child<QLabel>(roster, "Players");
    auto rosterFont = rosterTitle->font();
    rosterFont.setBold(true);
    rosterFont.setPointSize(13);
    rosterTitle->setFont(rosterFont);
    rosterLayout->addWidget(rosterTitle);
    m_search = child<QLineEdit>(roster);
    m_search->setObjectName("playerSearch");
    m_search->setPlaceholderText("Search character name or exact character ID");
    rosterLayout->addWidget(m_search);
    auto* searchTimer = child<QTimer>(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);
    connect(m_search, &QLineEdit::textChanged, this, [searchTimer] { searchTimer->start(); });
    connect(searchTimer, &QTimer::timeout, this, [this] {
        if (m_connected) {
            m_backend->submit({{"op", "search"}, {"search", m_search->text()}});
        }
    });
    m_table = child<QTableWidget>(roster, 0, 6);
    m_table->setObjectName("playerTable");
    m_table->setHorizontalHeaderLabels({"ID", "Character", "Account", "Level", "Role", "Presence"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    rosterLayout->addWidget(m_table, 1);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ManagementWindow::select_player);
    auto* paging = child<QWidget>(roster);
    auto* pagingLayout = horizontal(paging);
    pagingLayout->setContentsMargins(0, 0, 0, 0);
    m_previous = child<QPushButton>(paging, "Previous");
    m_next = child<QPushButton>(paging, "Next");
    m_paging = child<QLabel>(paging, "0 players");
    pagingLayout->addWidget(m_previous);
    pagingLayout->addWidget(m_paging, 1, Qt::AlignCenter);
    pagingLayout->addWidget(m_next);
    rosterLayout->addWidget(paging);
    connect(m_previous, &QPushButton::clicked, this, [this] {
        m_backend->submit({{"op", "page"}, {"page", m_page - 1}});
    });
    connect(m_next, &QPushButton::clicked, this, [this] {
        m_backend->submit({{"op", "page"}, {"page", m_page + 1}});
    });
    auto* details = child<QWidget>(split);
    auto* detailLayout = vertical(details);
    detailLayout->setContentsMargins(8, 0, 0, 0);
    m_target = child<QLabel>(details, "Select a player");
    m_target->setFont(rosterFont);
    detailLayout->addWidget(m_target);
    auto* roleRow = child<QWidget>(details);
    auto* roleLayout = horizontal(roleRow);
    roleLayout->setContentsMargins(0, 0, 0, 0);
    roleLayout->addWidget(child<QLabel>(roleRow, "Role"));
    m_role = child<QComboBox>(roleRow);
    m_role->setObjectName("playerRole");
    for (int i = 0; i < roles.size(); ++i) {
        m_role->addItem(QString::number(i) + " · " + roles[i], i);
    }
    roleLayout->addWidget(m_role, 1);
    m_applyRole = child<QPushButton>(roleRow, "Apply role");
    roleLayout->addWidget(m_applyRole);
    detailLayout->addWidget(roleRow);
    connect(m_applyRole, &QPushButton::clicked, this, [this] {
        m_backend->submit({{"op", "role"},
                           {"character", m_selected},
                           {"expected", m_selectedRole},
                           {"role", m_role->currentData().toInt()}});
    });
    auto* tabs = child<QTabWidget>(details);
    auto* mail = child<QWidget>(tabs);
    auto* mailLayout = vertical(mail);
    mailLayout->addWidget(
        child<QLabel>(mail, "In-game mail reaches online and offline characters."));
    m_subject = child<QLineEdit>(mail);
    m_subject->setObjectName("mailSubject");
    m_subject->setPlaceholderText("Subject");
    m_subject->setMaxLength(96);
    mailLayout->addWidget(m_subject);
    m_body = child<QPlainTextEdit>(mail);
    m_body->setObjectName("mailBody");
    m_body->setPlaceholderText("Message body (up to 1024 UTF-8 bytes)");
    mailLayout->addWidget(m_body, 1);
    m_attachment = child<QCheckBox>(mail, "Attach an item");
    mailLayout->addWidget(m_attachment);
    auto* itemRow = child<QWidget>(mail);
    auto* itemLayout = horizontal(itemRow);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->addWidget(child<QLabel>(itemRow, "Item ID"));
    m_item = child<QSpinBox>(itemRow);
    m_item->setRange(1, 65535);
    itemLayout->addWidget(m_item, 1);
    itemLayout->addWidget(child<QLabel>(itemRow, "Quantity"));
    m_quantity = child<QSpinBox>(itemRow);
    m_quantity->setRange(1, 1000000);
    itemLayout->addWidget(m_quantity, 1);
    mailLayout->addWidget(itemRow);
    m_sendMail = child<QPushButton>(mail, "Send in-game mail");
    m_sendMail->setObjectName("sendMailButton");
    mailLayout->addWidget(m_sendMail);
    connect(m_subject, &QLineEdit::textChanged, this, [this] {
        reset_mail_id();
        update_actions();
    });
    connect(m_body, &QPlainTextEdit::textChanged, this, [this] {
        reset_mail_id();
        update_actions();
    });
    connect(m_attachment, &QCheckBox::toggled, this, [this] {
        reset_mail_id();
        update_actions();
    });
    connect(m_item, &QSpinBox::valueChanged, this, [this] { reset_mail_id(); });
    connect(m_quantity, &QSpinBox::valueChanged, this, [this] { reset_mail_id(); });
    connect(m_sendMail, &QPushButton::clicked, this, [this] {
        m_backend->submit({{"op", "mail"},
                           {"character", m_selected},
                           {"request_id", m_mailId},
                           {"subject", m_subject->text()},
                           {"body", m_body->toPlainText()},
                           {"item_index", m_attachment->isChecked() ? m_item->value() : 0},
                           {"item_count", m_attachment->isChecked() ? m_quantity->value() : 0}});
    });
    tabs->addTab(mail, "Mail");
    auto* chat = child<QWidget>(tabs);
    auto* chatLayout = vertical(chat);
    auto* hint = child<QLabel>(chat,
                               "Support identity: !Support\nPlayers reply in-game with @!Support "
                               "message. PMs require the player to be online.");
    hint->setWordWrap(true);
    chatLayout->addWidget(hint);
    m_chat = child<QPlainTextEdit>(chat);
    m_chat->setObjectName("pmHistory");
    m_chat->setReadOnly(true);
    chatLayout->addWidget(m_chat, 1);
    m_pm = child<QLineEdit>(chat);
    m_pm->setObjectName("pmMessage");
    m_pm->setMaxLength(95);
    m_pm->setPlaceholderText("Private message (up to 95 UTF-8 bytes)");
    chatLayout->addWidget(m_pm);
    m_sendPm = child<QPushButton>(chat, "Send private message");
    m_sendPm->setObjectName("sendPmButton");
    chatLayout->addWidget(m_sendPm);
    connect(m_pm, &QLineEdit::textChanged, this, [this] { update_actions(); });
    const auto sendPm = [this] {
        if (m_sendPm->isEnabled()) {
            m_backend->submit({{"op", "pm"}, {"character", m_selected}, {"body", m_pm->text()}});
        }
    };
    connect(m_sendPm, &QPushButton::clicked, this, sendPm);
    connect(m_pm, &QLineEdit::returnPressed, this, sendPm);
    tabs->addTab(chat, "Private messages");
    detailLayout->addWidget(tabs, 1);
    auto* planned = child<QGroupBox>(details, "Planned server actions");
    auto* plannedLayout = horizontal(planned);
    for (const auto& name :
         {"World chat · Horn of Spirit", "Announcements", "Shutdown countdown"}) {
        auto* button = child<QPushButton>(planned, name);
        button->setEnabled(false);
        button->setToolTip("Planned for a later update");
        plannedLayout->addWidget(button);
    }
    detailLayout->addWidget(planned);
    split->addWidget(roster);
    split->addWidget(details);
    split->setSizes({610, 570});
    main->addWidget(split, 1);
    m_log = child<QPlainTextEdit>(central);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(300);
    m_log->setMaximumHeight(100);
    m_log->setPlaceholderText("Activity and connection results");
    main->addWidget(m_log);
    connect(
        m_backend.get(), &ManagementBackend::stateChanged, this, &ManagementWindow::display_state);
    connect(m_backend.get(), &ManagementBackend::busyChanged, this, [this](bool busy) {
        m_busy = busy;
        update_actions();
    });
    connect(m_backend.get(),
            &ManagementBackend::completed,
            this,
            [this](const QString& op, bool ok, const QString& message, const QString& requestId) {
                log((ok ? "✓ " : "Error: ") + message);
                if (ok && op == "pm") {
                    m_pm->clear();
                }
                if (ok && op == "mail" && requestId == m_mailId) {
                    m_subject->clear();
                    m_body->clear();
                    m_attachment->setChecked(false);
                    reset_mail_id();
                }
                update_actions();
            });
    reset_mail_id();
    update_actions();
    QSettings settings;
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
}
ManagementWindow::~ManagementWindow() {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("socket", m_socket->text());
    m_backend.reset();
}
void ManagementWindow::reset_mail_id() {
    m_mailId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}
void ManagementWindow::log(const QString& message) {
    m_log->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + message);
}
void ManagementWindow::display_state(const QJsonObject& state) {
    m_connected = state["connected"].toBool();
    m_api = state["api"].toBool();
    m_page = state["page"].toInt();
    m_total = state["total"].toInt();
    m_connection->setText(
        m_connected
            ? (m_api ? "PostgreSQL connected  •  Local API live  •  Auto refresh every 2 seconds"
                     : "PostgreSQL connected  •  Local API unavailable  •  Presence unknown")
            : "Disconnected");
    m_connection->setToolTip(state["api_error"].toString());
    m_players = state["players"].toArray();
    m_table->blockSignals(true);
    m_table->setRowCount(m_players.size());
    int selectedRow = -1;
    for (int n = 0; n < m_players.size(); ++n) {
        const auto player = m_players[n].toObject();
        const QStringList values = {QString::number(player["id"].toInt()),
                                    player["name"].toString(),
                                    QString::number(player["account"].toInt()),
                                    QString::number(player["level"].toInt()),
                                    role_name(player["role"].toInt()),
                                    player["presence"].toString()};
        for (int c = 0; c < values.size(); ++c) {
            auto item = std::make_unique<QTableWidgetItem>(values[c]);
            if (c == 5) {
                item->setForeground(player["presence"].toString() == "Online" ? QColor("#278650")
                                                                              : QColor("#77808D"));
            }
            if (player["deletion"].toInt() != 0) {
                item->setToolTip("Character is pending deletion");
            }
            m_table->setItem(n, c, item.release());
        }
        if (player["id"].toInt() == m_selected) {
            selectedRow = n;
        }
    }
    if (selectedRow >= 0) {
        m_table->selectRow(selectedRow);
    } else if (!m_players.isEmpty()) {
        m_table->selectRow(0);
    }
    m_table->blockSignals(false);
    for (const auto& event : state["events"].toArray()) {
        m_messages.append(event);
        const auto message = event.toObject();
        if (!message["outgoing"].toBool()) {
            log("PM from " + message["name"].toString() + ": " + message["body"].toString());
        }
    }
    while (m_messages.size() > 1000) {
        m_messages.removeFirst();
    }
    if (state["gap"].toBool()) {
        log("Support history has a gap or the server restarted; older PMs are unavailable.");
    }
    m_paging->setText(QString("%1 players · Page %2 / %3")
                          .arg(m_total)
                          .arg(m_page + 1)
                          .arg(qMax(1, (m_total + 99) / 100)));
    select_player();
}
void ManagementWindow::select_player() {
    const int row = m_table->currentRow();
    const auto player =
        row >= 0 && row < m_players.size() ? m_players[row].toObject() : QJsonObject{};
    const int id = player["id"].toInt(), role = player["role"].toInt();
    if (id != m_selected || role != m_selectedRole) {
        m_role->setCurrentIndex(role);
        if (id != m_selected) {
            reset_mail_id();
        }
    }
    m_selected = id;
    m_selectedRole = role;
    m_selectedName = player["name"].toString();
    m_presence = player["presence"].toString();
    m_target->setText(id ? QString("%1  ·  #%2  ·  %3").arg(m_selectedName).arg(id).arg(m_presence)
                         : "Select a player");
    m_applyRole->setProperty("available", id > 0 && player["deletion"].toInt() == 0);
    show_conversation();
    update_actions();
}
void ManagementWindow::show_conversation() {
    QStringList lines;
    for (const auto& value : m_messages) {
        const auto event = value.toObject();
        if (event["character"].toInt() != m_selected) {
            continue;
        }
        lines.append(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(event["time"].toDouble()))
                         .toString("HH:mm:ss") +
                     "  " + (event["outgoing"].toBool() ? "Support → " : m_selectedName + " → ") +
                     event["body"].toString());
    }
    if (m_chat->toPlainText() != lines.join('\n')) {
        m_chat->setPlainText(lines.join('\n'));
        m_chat->moveCursor(QTextCursor::End);
    }
}
void ManagementWindow::update_actions() {
    const bool ready =
        m_connected && !m_busy && m_selected > 0 && m_applyRole->property("available").toBool();
    m_applyRole->setEnabled(ready && m_applyRole->property("available").toBool());
    m_role->setEnabled(ready);
    m_sendMail->setEnabled(ready && m_api && !m_subject->text().trimmed().isEmpty() &&
                           m_subject->text().toUtf8().size() <= 96 &&
                           m_body->toPlainText().toUtf8().size() <= 1024);
    m_sendPm->setEnabled(ready && m_api && m_presence == "Online" &&
                         !m_pm->text().trimmed().isEmpty() && m_pm->text().toUtf8().size() <= 95);
    m_pm->setEnabled(m_connected && !m_busy);
    m_item->setEnabled(m_attachment->isChecked());
    m_quantity->setEnabled(m_attachment->isChecked());
    m_previous->setEnabled(m_connected && !m_busy && m_page > 0);
    m_next->setEnabled(m_connected && !m_busy && (m_page + 1) * 100 < m_total);
}
