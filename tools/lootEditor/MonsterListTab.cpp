/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MonsterListTab.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>

namespace {
    enum IgCol { IgIndex = 0, IgAttempts, IgCols };

    constexpr int kLevelFilterAll = 0;
    constexpr int kMaxMonsterLevel = 120;
}  // namespace

MonsterListTab::MonsterListTab(LootDataStore* store, QWidget* parent)
    : QWidget(parent), m_store(store) {
    auto* layout = new QVBoxLayout(this);

    auto* filter_row = new QHBoxLayout();
    filter_row->addWidget(new QLabel("Level:", this));

    auto* level_prev = new QPushButton("<", this);
    level_prev->setFixedWidth(28);
    level_prev->setToolTip("Previous level");
    connect(level_prev, &QPushButton::clicked, this, [this]() { step_level(-1); });
    filter_row->addWidget(level_prev);

    m_level_filter = new QComboBox(this);
    m_level_filter->addItem("All", kLevelFilterAll);
    for (int level = 1; level <= kMaxMonsterLevel; ++level) {
        m_level_filter->addItem(QString::number(level), level);
    }
    m_level_filter->setMinimumWidth(80);
    connect(m_level_filter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MonsterListTab::apply_filter);
    filter_row->addWidget(m_level_filter);

    auto* level_next = new QPushButton(">", this);
    level_next->setFixedWidth(28);
    level_next->setToolTip("Next level");
    connect(level_next, &QPushButton::clicked, this, [this]() { step_level(1); });
    filter_row->addWidget(level_next);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText("Filter by name or index…");
    connect(m_filter, &QLineEdit::textChanged, this, &MonsterListTab::apply_filter);
    filter_row->addWidget(m_filter, 1);
    layout->addLayout(filter_row);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_list = new QListWidget(splitter);
    connect(m_list, &QListWidget::currentRowChanged, this, &MonsterListTab::on_selection_changed);

    auto* right = new QWidget(splitter);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->addWidget(
        new QLabel("Monster itemGroups (itemgroup id, attempts per kill):", right));

    m_itemgroups = new QTableWidget(0, IgCols, right);
    m_itemgroups->setHorizontalHeaderLabels({"ItemGroup", "Attempts"});
    m_itemgroups->horizontalHeader()->setStretchLastSection(true);
    right_layout->addWidget(m_itemgroups);

    auto* ig_btns = new QHBoxLayout();
    auto* add = new QPushButton("Add", right);
    auto* remove = new QPushButton("Remove", right);
    auto* apply = new QPushButton("Apply itemGroups", right);
    connect(add, &QPushButton::clicked, this, &MonsterListTab::add_itemgroup_row);
    connect(remove, &QPushButton::clicked, this, &MonsterListTab::remove_itemgroup_rows);
    connect(apply, &QPushButton::clicked, this, &MonsterListTab::apply_item_groups);
    ig_btns->addWidget(add);
    ig_btns->addWidget(remove);
    ig_btns->addStretch();
    ig_btns->addWidget(apply);
    right_layout->addLayout(ig_btns);

    splitter->addWidget(m_list);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter);

    connect(m_store, &LootDataStore::dataChanged, this, &MonsterListTab::refresh);
    refresh();
}

void MonsterListTab::refresh() {
    const int prev = m_store->selected_monster();
    m_list->clear();
    const std::string needle = m_filter->text().trimmed().toLower().toStdString();
    const int level_filter = m_level_filter->currentData().toInt();

    for (const MonsterRecord* rec : m_store->monsters().sorted_monsters()) {
        if (level_filter != kLevelFilterAll && rec->level != level_filter) {
            continue;
        }

        const std::string label = rec->name + " [" + std::to_string(rec->level) + "]";
        if (!needle.empty()) {
            const std::string lower_name = rec->name;
            std::string hay = lower_name;
            for (char& c : hay) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            const std::string idx = std::to_string(rec->index);
            if (hay.find(needle) == std::string::npos && idx.find(needle) == std::string::npos) {
                continue;
            }
        }
        auto* item = new QListWidgetItem(
            QString("%1  (#%2)").arg(QString::fromStdString(label)).arg(rec->index));
        item->setData(Qt::UserRole, rec->index);
        m_list->addItem(item);
        if (rec->index == prev) {
            m_list->setCurrentItem(item);
        }
    }
    load_item_groups_editor();
}

void MonsterListTab::apply_filter() {
    refresh();
}

void MonsterListTab::step_level(int delta) {
    const int count = m_level_filter->count();
    if (count <= 0 || delta == 0) {
        return;
    }
    const int next = (m_level_filter->currentIndex() + delta % count + count) % count;
    m_level_filter->setCurrentIndex(next);
}

void MonsterListTab::on_selection_changed() {
    auto* item = m_list->currentItem();
    if (!item) {
        m_store->set_selected_monster(-1);
        load_item_groups_editor();
        return;
    }
    m_store->set_selected_monster(item->data(Qt::UserRole).toInt());
    load_item_groups_editor();
}

void MonsterListTab::load_item_groups_editor() {
    m_itemgroups->setRowCount(0);
    const MonsterRecord* rec = m_store->monsters().find(m_store->selected_monster());
    if (!rec) {
        return;
    }
    for (const auto& [ig, attempts] : rec->item_groups) {
        const int row = m_itemgroups->rowCount();
        m_itemgroups->insertRow(row);
        auto* ig_spin = new QSpinBox();
        ig_spin->setRange(1, 99999);
        ig_spin->setValue(ig);
        m_itemgroups->setCellWidget(row, IgIndex, ig_spin);
        auto* att = new QSpinBox();
        att->setRange(1, 99);
        att->setValue(attempts);
        m_itemgroups->setCellWidget(row, IgAttempts, att);
    }
}

void MonsterListTab::apply_item_groups() {
    const int mid = m_store->selected_monster();
    if (mid < 0) {
        return;
    }
    std::vector<std::pair<int, int>> rows;
    for (int r = 0; r < m_itemgroups->rowCount(); ++r) {
        const auto* ig = qobject_cast<QSpinBox*>(m_itemgroups->cellWidget(r, IgIndex));
        const auto* att = qobject_cast<QSpinBox*>(m_itemgroups->cellWidget(r, IgAttempts));
        if (!ig || !att) {
            continue;
        }
        if (ig->value() > 0 && att->value() > 0) {
            rows.emplace_back(ig->value(), att->value());
        }
    }
    (void) m_store->set_monster_item_groups(mid, std::move(rows));
}

void MonsterListTab::add_itemgroup_row() {
    const int row = m_itemgroups->rowCount();
    m_itemgroups->insertRow(row);
    auto* ig = new QSpinBox();
    ig->setRange(1, 99999);
    m_itemgroups->setCellWidget(row, IgIndex, ig);
    auto* att = new QSpinBox();
    att->setRange(1, 99);
    att->setValue(1);
    m_itemgroups->setCellWidget(row, IgAttempts, att);
}

void MonsterListTab::remove_itemgroup_rows() {
    const auto selected = m_itemgroups->selectionModel()->selectedRows();
    QList<int> rows;
    for (const auto& idx : selected) {
        rows.push_back(idx.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        m_itemgroups->removeRow(row);
    }
}
