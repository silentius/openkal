/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/NewIndexDialog.h"

#include "dialogs/IdRangeFinderDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

NewIndexDialog::NewIndexDialog(const QString& title,
                               int suggested,
                               std::set<int> used,
                               QWidget* parent)
    : QDialog(parent), m_used(std::move(used)) {
    setWindowTitle(title);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    auto* index_row = new QHBoxLayout();
    m_index = new QSpinBox(this);
    m_index->setRange(kMinId, kMaxId);
    m_index->setValue(std::clamp(suggested, kMinId, kMaxId));
    m_index->setMinimumWidth(120);
    index_row->addWidget(m_index, 1);

    m_find = new QPushButton("…", this);
    m_find->setFixedWidth(32);
    m_find->setToolTip("Show free ID ranges");
    connect(m_find, &QPushButton::clicked, this, &NewIndexDialog::open_id_finder);
    index_row->addWidget(m_find);

    form->addRow("Index:", index_row);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &NewIndexDialog::accept_if_free);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

int NewIndexDialog::index() const {
    return m_index->value();
}

void NewIndexDialog::open_id_finder() {
    IdRangeFinderDialog dlg(windowTitle() + " — Free IDs", m_used, kMinId, kMaxId, this);
    if (dlg.exec() != QDialog::Accepted || dlg.selected_id() <= 0) {
        return;
    }
    m_index->setValue(dlg.selected_id());
}

void NewIndexDialog::accept_if_free() {
    const int idx = m_index->value();
    if (m_used.find(idx) != m_used.end()) {
        QMessageBox::warning(
            this, "Index in use", QString("Index %1 is already taken. Pick a free ID.").arg(idx));
        return;
    }
    accept();
}
