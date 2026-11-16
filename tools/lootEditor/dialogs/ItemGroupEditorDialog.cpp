/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/ItemGroupEditorDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    enum Col { Group = 0, Chance, CountCols };
}  // namespace

ItemGroupEditorDialog::ItemGroupEditorDialog(openkal::server::ItemGroupDef def,
                                             LootDataStore* store,
                                             QWidget* parent)
    : QDialog(parent), m_def(std::move(def)), m_store(store) {
    setWindowTitle(QString("Edit ItemGroup #%1").arg(m_def.index));
    resize(480, 360);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_name = new QLineEdit(QString::fromStdString(m_def.name), this);
    form->addRow("Name:", m_name);
    layout->addLayout(form);

    m_table = new QTableWidget(0, CountCols, this);
    m_table->setHorizontalHeaderLabels({"Group", "Chance %"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table);
    populate_table();

    auto* row_btns = new QHBoxLayout();
    auto* add = new QPushButton("Add Group Ref", this);
    auto* remove = new QPushButton("Remove Selected", this);
    connect(add, &QPushButton::clicked, this, &ItemGroupEditorDialog::add_row);
    connect(remove, &QPushButton::clicked, this, &ItemGroupEditorDialog::remove_selected);
    row_btns->addWidget(add);
    row_btns->addWidget(remove);
    row_btns->addStretch();
    layout->addLayout(row_btns);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        apply_table();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ItemGroupEditorDialog::populate_table() {
    m_table->setRowCount(0);
    for (const auto& e : m_def.entries) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* group = new QSpinBox();
        group->setRange(1, 99999);
        group->setValue(e.group_index);
        m_table->setCellWidget(row, Group, group);
        auto* chance = new QDoubleSpinBox();
        chance->setRange(0.01, 100.0);
        chance->setDecimals(2);
        chance->setValue(e.chance);
        m_table->setCellWidget(row, Chance, chance);
    }
}

void ItemGroupEditorDialog::apply_table() {
    m_def.name = m_name->text().toStdString();
    m_def.entries.clear();
    float total = 0.0f;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* group = qobject_cast<QSpinBox*>(m_table->cellWidget(row, Group));
        const auto* chance = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(row, Chance));
        if (!group || !chance) {
            continue;
        }
        openkal::server::ItemGroupEntry e;
        e.group_index = group->value();
        e.chance = static_cast<float>(chance->value());
        if (e.group_index <= 0 || e.chance <= 0) {
            continue;
        }
        total += e.chance;
        m_def.entries.push_back(e);
    }
    (void) m_store;
    (void) total;
}

void ItemGroupEditorDialog::add_row() {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto* group = new QSpinBox();
    group->setRange(1, 99999);
    m_table->setCellWidget(row, Group, group);
    auto* chance = new QDoubleSpinBox();
    chance->setRange(0.01, 100.0);
    chance->setDecimals(2);
    chance->setValue(1.0);
    m_table->setCellWidget(row, Chance, chance);
}

void ItemGroupEditorDialog::remove_selected() {
    const auto selected = m_table->selectionModel()->selectedRows();
    QList<int> rows;
    for (const auto& idx : selected) {
        rows.push_back(idx.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        m_table->removeRow(row);
    }
}
