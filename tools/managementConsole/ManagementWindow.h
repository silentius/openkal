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

#include "ManagementBackend.h"

#include <QJsonArray>
#include <QMainWindow>
#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

/** Native player roster, role, in-game mail and two-way support PM console. */
class ManagementWindow final : public QMainWindow {
    Q_OBJECT
  public:
    /** Builds the console. @param parent Optional Qt parent. */
    explicit ManagementWindow(QWidget* parent = nullptr);

    /** Stops the background worker before Qt destroys child widgets. */
    ~ManagementWindow() override;

    /** Displays a backend snapshot. @param state Roster, health and newly received support PMs. */
    void display_state(const QJsonObject& state);

  private:
    void select_player();
    void update_actions();
    void show_conversation();
    void log(const QString& message);
    void reset_mail_id();
    std::unique_ptr<ManagementBackend> m_backend;
    QJsonArray m_players, m_messages;
    bool m_connected = false, m_api = false, m_busy = false;
    int m_selected = 0, m_selectedRole = 0, m_page = 0, m_total = 0;
    QString m_selectedName, m_presence, m_mailId;
    QLineEdit *m_dsn = nullptr, *m_socket = nullptr, *m_token = nullptr, *m_search = nullptr,
              *m_subject = nullptr, *m_pm = nullptr;
    QTableWidget* m_table = nullptr;
    QComboBox* m_role = nullptr;
    QSpinBox *m_item = nullptr, *m_quantity = nullptr;
    QCheckBox* m_attachment = nullptr;
    QPlainTextEdit *m_body = nullptr, *m_chat = nullptr, *m_log = nullptr;
    QLabel *m_connection = nullptr, *m_target = nullptr, *m_paging = nullptr;
    QPushButton *m_applyRole = nullptr, *m_sendMail = nullptr, *m_sendPm = nullptr,
                *m_previous = nullptr, *m_next = nullptr;
};
