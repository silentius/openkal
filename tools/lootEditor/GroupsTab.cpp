/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "GroupsTab.h"

#include "dialogs/GroupEditorDialog.h"
#include "dialogs/ItemPickerDialog.h"
#include "model/ItemSearch.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "widgets/GroupEntriesWidget.h"
#include "widgets/MatchedItemsBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>
#include <unordered_set>

GroupsTab::GroupsTab(LootDataStore* store, QWidget* parent) : QWidget(parent), m_store(store) {
    m_icons = std::make_unique<ItemIconCache>(store->items());

    auto* layout = new QVBoxLayout(this);

    auto* search_row = new QHBoxLayout();
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search item by name or index…");
    connect(m_search, &QLineEdit::returnPressed, this, &GroupsTab::apply_search_text);
    search_row->addWidget(m_search, 1);

    m_pick_btn = new QPushButton("Pick…", this);
    connect(m_pick_btn, &QPushButton::clicked, this, &GroupsTab::pick_search_item);
    search_row->addWidget(m_pick_btn);

    m_clear_btn = new QPushButton("Clear", this);
    connect(m_clear_btn, &QPushButton::clicked, this, &GroupsTab::clear_search_item);
    search_row->addWidget(m_clear_btn);
    layout->addLayout(search_row);

    m_matched_items = new MatchedItemsBar(m_store, m_icons.get(), this);
    layout->addWidget(m_matched_items);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* left = new QWidget(splitter);
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->addWidget(new QLabel("Groups", left));
    m_groups = new QListWidget(left);
    left_layout->addWidget(m_groups, 1);
    m_edit_group_btn = new QPushButton("Edit Group…", left);
    left_layout->addWidget(m_edit_group_btn);

    auto* right = new QWidget(splitter);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->addWidget(new QLabel("Items + chance", right));
    m_entries = new GroupEntriesWidget(m_store, m_icons.get(), right);
    right_layout->addWidget(m_entries, 1);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);

    connect(m_groups, &QListWidget::currentRowChanged, this, &GroupsTab::on_group_selected);
    connect(m_groups, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        edit_selected_group();
    });
    connect(m_edit_group_btn, &QPushButton::clicked, this, &GroupsTab::edit_selected_group);
    connect(m_store, &LootDataStore::dataChanged, this, &GroupsTab::refresh);

    update_search_status();
    refresh();
}

void GroupsTab::set_search_items(std::vector<int> item_indices, bool sync_search_field) {
    m_search_items = std::move(item_indices);
    m_search_item_set = std::unordered_set<int>(m_search_items.begin(), m_search_items.end());
    m_search_item_set.erase(0);

    if (sync_search_field) {
        if (m_search_items.size() == 1) {
            m_search->setText(QString::number(m_search_items.front()));
        } else if (m_search_items.empty()) {
            m_search->clear();
        }
    }

    update_search_status();
    refresh();
}

void GroupsTab::update_search_status() {
    m_matched_items->set_items(m_search_items);
    if (m_search_items.empty()) {
        m_status->setText("Showing all Groups. Search or pick an item to filter.");
        return;
    }

    const int group_count = static_cast<int>(m_store->groups_containing_any(m_search_items).size());
    if (m_search_items.size() == 1) {
        m_status->setText(QString("%1 — %2 Group(s) contain this item")
                              .arg(m_store->item_name(m_search_items.front()))
                              .arg(group_count));
        return;
    }
    m_status->setText(QString("%1 matching item(s) — %2 Group(s) contain at least one")
                          .arg(static_cast<int>(m_search_items.size()))
                          .arg(group_count));
}

void GroupsTab::pick_search_item() {
    const int initial = m_search_items.size() == 1 ? m_search_items.front() : 0;
    ItemPickerDialog picker(m_store, m_icons.get(), initial, {}, this);
    if (picker.exec() != QDialog::Accepted || picker.selected_index() <= 0) {
        return;
    }
    set_search_items({picker.selected_index()}, true);
}

void GroupsTab::clear_search_item() {
    set_search_items({}, true);
}

void GroupsTab::apply_search_text() {
    const QString text = m_search->text().trimmed();
    if (text.isEmpty()) {
        set_search_items({}, false);
        return;
    }

    const std::vector<int> matches = find_items_matching_search(*m_store, text);
    if (matches.empty()) {
        QMessageBox::information(this, "Item not found", "No item matches that search.");
        return;
    }
    set_search_items(matches, false);
}

QString GroupsTab::group_label(int index) const {
    QString label = QString("Group #%1").arg(index);
    const openkal::server::GroupDef* def = m_store->drop_db().find_group(index);
    if (def && !def->name.empty()) {
        label += " — " + QString::fromStdString(def->name);
    }
    if (def) {
        label += QString("  (%1 entries)").arg(static_cast<int>(def->entries.size()));
    }
    return label;
}

void GroupsTab::refresh() {
    const int prev =
        m_groups->currentItem() ? m_groups->currentItem()->data(Qt::UserRole).toInt() : 0;

    m_groups->clear();
    m_entries->clear();

    std::vector<int> indices;
    if (!m_search_items.empty()) {
        indices = m_store->groups_containing_any(m_search_items);
    } else {
        indices.reserve(m_store->drop_db().groups().size());
        for (const auto& [idx, _] : m_store->drop_db().groups()) {
            (void) _;
            indices.push_back(idx);
        }
        std::sort(indices.begin(), indices.end());
    }

    int restore_row = -1;
    for (int idx : indices) {
        auto* item = new QListWidgetItem(group_label(idx));
        item->setData(Qt::UserRole, idx);
        m_groups->addItem(item);
        if (idx == prev) {
            restore_row = m_groups->count() - 1;
        }
    }

    if (restore_row >= 0) {
        m_groups->setCurrentRow(restore_row);
    } else if (m_groups->count() > 0) {
        m_groups->setCurrentRow(0);
    } else {
        on_group_selected();
    }

    update_search_status();
    m_edit_group_btn->setEnabled(m_groups->currentItem() != nullptr);
}

void GroupsTab::on_group_selected() {
    const QListWidgetItem* current = m_groups->currentItem();
    m_edit_group_btn->setEnabled(current != nullptr);
    if (!current) {
        m_entries->clear();
        return;
    }
    m_entries->show_group(current->data(Qt::UserRole).toInt(), m_search_item_set);
}

void GroupsTab::edit_selected_group() {
    const QListWidgetItem* current = m_groups->currentItem();
    if (!current) {
        return;
    }
    const int group_index = current->data(Qt::UserRole).toInt();
    openkal::server::GroupDef* g = m_store->drop_db().find_group_mut(group_index);
    if (!g) {
        return;
    }
    GroupEditorDialog dlg(*g, m_store, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_store->upsert_group(dlg.result_def());
}
