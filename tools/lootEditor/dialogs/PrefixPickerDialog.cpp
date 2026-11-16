/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/PrefixPickerDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

PrefixPickerDialog::PrefixPickerDialog(LootDataStore* store, int initial_id, QWidget* parent)
    : QDialog(parent), m_store(store), m_selected(initial_id) {
    setWindowTitle("Pick Prefix");
    resize(480, 520);

    auto* layout = new QVBoxLayout(this);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search prefix name or id…");
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto* hint = new QLabel("Double-click a prefix. Choose “None” for no enchant.", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &PrefixPickerDialog::refresh_list);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &PrefixPickerDialog::accept_selection);
    connect(m_list, &QListWidget::itemActivated, this, &PrefixPickerDialog::accept_selection);
    connect(buttons, &QDialogButtonBox::accepted, this, &PrefixPickerDialog::accept_selection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refresh_list();

    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toInt() == initial_id) {
            m_list->setCurrentRow(row);
            break;
        }
    }

    m_search->setFocus();
}

void PrefixPickerDialog::refresh_list() {
    m_list->clear();

    const QString filter = m_search->text().trimmed();

    struct Row {
        int id;
        QString label;
    };
    std::vector<Row> rows;
    rows.push_back({0, QStringLiteral("None (no prefix)")});

    for (const auto& [id, def] : m_store->prefixes().by_index()) {
        QString label = m_store->prefix_name(id);
        if (def.level() > 0) {
            label += QString("  (+%1)").arg(def.level());
        }
        rows.push_back({id, label});
    }

    std::sort(rows.begin() + 1, rows.end(), [](const Row& a, const Row& b) {
        return QString::compare(a.label, b.label, Qt::CaseInsensitive) < 0;
    });

    for (const Row& row : rows) {
        if (!filter.isEmpty()) {
            if (!row.label.contains(filter, Qt::CaseInsensitive) &&
                !QString::number(row.id).contains(filter)) {
                continue;
            }
        }
        auto* item = new QListWidgetItem(row.label);
        item->setData(Qt::UserRole, row.id);
        if (row.id > 0) {
            item->setToolTip(QString("#%1 — %2").arg(row.id).arg(row.label));
        }
        m_list->addItem(item);
    }
}

void PrefixPickerDialog::accept_selection() {
    const QListWidgetItem* current = m_list->currentItem();
    if (!current) {
        return;
    }
    m_selected = current->data(Qt::UserRole).toInt();
    accept();
}
