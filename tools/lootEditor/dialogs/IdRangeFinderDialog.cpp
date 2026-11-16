/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/IdRangeFinderDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

std::vector<IdRangeFinderDialog::Range>
IdRangeFinderDialog::compute_ranges(const std::set<int>& used, int min_id, int max_id) {
    std::vector<Range> ranges;
    if (min_id > max_id) {
        return ranges;
    }

    std::vector<int> occupied;
    occupied.reserve(used.size());
    for (int id : used) {
        if (id >= min_id && id <= max_id) {
            occupied.push_back(id);
        }
    }
    std::sort(occupied.begin(), occupied.end());
    occupied.erase(std::unique(occupied.begin(), occupied.end()), occupied.end());

    int cursor = min_id;
    for (int id : occupied) {
        if (id > cursor) {
            ranges.push_back({cursor, id - 1});
        }
        cursor = id + 1;
    }
    if (cursor <= max_id) {
        ranges.push_back({cursor, max_id});
    }
    return ranges;
}

QString IdRangeFinderDialog::range_label(const Range& range) {
    const int count = range.last - range.first + 1;
    if (range.first == range.last) {
        return QString("%1  (1 free)").arg(range.first);
    }
    return QString("%1 – %2  (%3 free)").arg(range.first).arg(range.last).arg(count);
}

IdRangeFinderDialog::IdRangeFinderDialog(
    const QString& title, const std::set<int>& used, int min_id, int max_id, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    resize(420, 480);

    auto* layout = new QVBoxLayout(this);
    m_status = new QLabel(this);
    layout->addWidget(m_status);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    connect(m_list,
            &QListWidget::itemDoubleClicked,
            this,
            &IdRangeFinderDialog::on_item_double_clicked);
    layout->addWidget(m_list, 1);

    const std::vector<Range> ranges = compute_ranges(used, min_id, max_id);
    m_status->setText(QString("%1 free range(s) between %2 and %3.")
                          .arg(static_cast<int>(ranges.size()))
                          .arg(min_id)
                          .arg(max_id));

    for (const Range& range : ranges) {
        auto* item = new QListWidgetItem(range_label(range));
        item->setData(Qt::UserRole, range.first);
        item->setData(Qt::UserRole + 1, range.last);
        item->setToolTip(QString("First free ID: %1").arg(range.first));
        m_list->addItem(item);
    }

    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(m_list->count() > 0);
    connect(buttons, &QDialogButtonBox::accepted, this, &IdRangeFinderDialog::accept_selection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void IdRangeFinderDialog::accept_selection() {
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return;
    }
    m_selected_id = item->data(Qt::UserRole).toInt();
    accept();
}

void IdRangeFinderDialog::on_item_double_clicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    m_list->setCurrentItem(item);
    accept_selection();
}
