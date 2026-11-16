/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/FindItemGroupDialog.h"

#include "dialogs/ItemPickerDialog.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <unordered_set>

FindItemGroupDialog::FindItemGroupDialog(LootDataStore* store,
                                         int preferred_itemgroup,
                                         QWidget* parent)
    : QDialog(parent), m_store(store) {
    m_icons = std::make_unique<ItemIconCache>(store->items());
    setWindowTitle("Find Item in Groups");
    resize(900, 620);

    auto* layout = new QVBoxLayout(this);

    auto* item_row = new QHBoxLayout();
    m_item_icon = new QLabel(this);
    m_item_icon->setFixedSize(32, 32);
    m_item_icon->setScaledContents(true);
    item_row->addWidget(m_item_icon);

    m_item_name = new QLabel("No item selected", this);
    m_item_name->setMinimumWidth(200);
    item_row->addWidget(m_item_name, 1);

    m_pick_item = new QPushButton("Pick Item…", this);
    connect(m_pick_item, &QPushButton::clicked, this, &FindItemGroupDialog::pick_item);
    item_row->addWidget(m_pick_item);
    layout->addLayout(item_row);

    m_status = new QLabel("Pick an item to list matching loot groups.", this);
    layout->addWidget(m_status);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_groups = new QListWidget(splitter);
    m_groups->setMinimumWidth(280);
    connect(
        m_groups, &QListWidget::currentRowChanged, this, &FindItemGroupDialog::on_group_selected);
    connect(m_groups, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        add_to_itemgroup();
    });

    m_details = new QTreeWidget(splitter);
    m_details->setHeaderLabels({"", "Entry", "Chance %"});
    m_details->setIconSize(QSize(32, 32));
    m_details->setRootIsDecorated(false);
    m_details->setColumnWidth(0, 40);
    m_details->setColumnWidth(1, 280);

    splitter->addWidget(m_groups);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    auto* add_form = new QFormLayout();
    m_target_ig = new QComboBox(this);
    std::set<int> ig_options;
    if (preferred_itemgroup > 0) {
        ig_options.insert(preferred_itemgroup);
    }
    const MonsterRecord* monster = m_store->monsters().find(m_store->selected_monster());
    if (monster) {
        for (const auto& [ig, _] : monster->item_groups) {
            if (ig > 0) {
                ig_options.insert(ig);
            }
        }
    }
    if (ig_options.empty()) {
        for (const auto& [idx, def] : m_store->drop_db().itemgroups()) {
            (void) def;
            ig_options.insert(idx);
        }
    }
    for (int ig : ig_options) {
        const openkal::server::ItemGroupDef* def = m_store->drop_db().find_itemgroup(ig);
        QString label = QString("ItemGroup #%1").arg(ig);
        if (def && !def->name.empty()) {
            label += " — " + QString::fromStdString(def->name);
        }
        m_target_ig->addItem(label, ig);
    }
    if (preferred_itemgroup > 0) {
        const int idx = m_target_ig->findData(preferred_itemgroup);
        if (idx >= 0) {
            m_target_ig->setCurrentIndex(idx);
        }
    }
    add_form->addRow("Add to:", m_target_ig);

    m_chance = new QDoubleSpinBox(this);
    m_chance->setRange(0.01, 100.0);
    m_chance->setDecimals(2);
    m_chance->setValue(1.0);
    m_chance->setSuffix(" %");
    add_form->addRow("Chance:", m_chance);
    layout->addLayout(add_form);

    auto* buttons = new QDialogButtonBox(this);
    m_add = buttons->addButton("Add Group to ItemGroup", QDialogButtonBox::AcceptRole);
    m_add->setEnabled(false);
    buttons->addButton(QDialogButtonBox::Close);
    connect(m_add, &QPushButton::clicked, this, &FindItemGroupDialog::add_to_itemgroup);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void FindItemGroupDialog::pick_item() {
    std::unordered_set<int> used_in_any_group;
    for (const auto& [_, def] : m_store->drop_db().groups()) {
        for (const auto& e : def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Item && e.item_index > 0) {
                used_in_any_group.insert(e.item_index);
            }
        }
    }
    ItemPickerDialog picker(
        m_store, m_icons.get(), m_item_index, std::move(used_in_any_group), this);
    if (picker.exec() != QDialog::Accepted || picker.selected_index() <= 0) {
        return;
    }
    m_item_index = picker.selected_index();
    m_item_icon->setPixmap(m_icons->icon(m_item_index));
    m_item_name->setText(m_store->item_name(m_item_index));
    m_item_name->setToolTip(m_store->item_tooltip(m_item_index));
    m_item_icon->setToolTip(m_store->item_tooltip(m_item_index));
    refresh_matches();
}

std::vector<int> FindItemGroupDialog::matching_groups() const {
    std::vector<int> matches;
    if (m_item_index <= 0) {
        return matches;
    }
    for (const auto& [idx, def] : m_store->drop_db().groups()) {
        for (const auto& e : def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Item && e.item_index == m_item_index) {
                matches.push_back(idx);
                break;
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

QString FindItemGroupDialog::group_summary(int group_index) const {
    const openkal::server::GroupDef* def = m_store->drop_db().find_group(group_index);
    if (!def) {
        return {};
    }
    QStringList parts;
    for (const auto& e : def->entries) {
        if (e.kind == openkal::server::GroupEntry::Kind::Money) {
            parts << QString("$%1 (%2%)").arg(e.amount).arg(e.chance, 0, 'f', 1);
            continue;
        }
        QString part = m_store->item_name(e.item_index);
        if (e.prefix > 0) {
            part = m_store->prefix_name(e.prefix) + " " + part;
        }
        if (e.count != 1) {
            part += QString(" ×%1").arg(e.count);
        }
        part += QString(" (%1%)").arg(e.chance, 0, 'f', 1);
        parts << part;
    }
    return parts.join(", ");
}

void FindItemGroupDialog::refresh_matches() {
    m_groups->clear();
    m_details->clear();
    m_add->setEnabled(false);

    if (m_item_index <= 0) {
        m_status->setText("Pick an item to list matching loot groups.");
        return;
    }

    const std::vector<int> matches = matching_groups();
    m_status->setText(QString("%1 — %2 matching group(s)")
                          .arg(m_store->item_name(m_item_index))
                          .arg(static_cast<int>(matches.size())));

    for (int gid : matches) {
        const openkal::server::GroupDef* def = m_store->drop_db().find_group(gid);
        QString title = QString("Group #%1").arg(gid);
        if (def && !def->name.empty()) {
            title += " — " + QString::fromStdString(def->name);
        }
        auto* item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, gid);
        item->setToolTip(group_summary(gid));
        m_groups->addItem(item);
    }

    if (m_groups->count() > 0) {
        m_groups->setCurrentRow(0);
    }
}

void FindItemGroupDialog::populate_details(int group_index) {
    m_details->clear();
    const openkal::server::GroupDef* def = m_store->drop_db().find_group(group_index);
    if (!def) {
        return;
    }

    for (const auto& e : def->entries) {
        auto* row = new QTreeWidgetItem();
        if (e.kind == openkal::server::GroupEntry::Kind::Money) {
            row->setIcon(0, QIcon(m_icons->money_icon()));
            row->setText(1, QString("Money × %1").arg(e.amount));
            row->setToolTip(1, QString("Money amount %1").arg(e.amount));
        } else {
            row->setIcon(0, QIcon(m_icons->icon(e.item_index)));
            QString label = m_store->item_name(e.item_index);
            if (e.prefix > 0) {
                label = m_store->prefix_name(e.prefix) + " " + label;
            }
            if (e.count != 1) {
                label += QString(" ×%1").arg(e.count);
            }
            if (e.item_index == m_item_index) {
                label += "  ← match";
                QFont f = row->font(1);
                f.setBold(true);
                row->setFont(1, f);
            }
            row->setText(1, label);
            row->setToolTip(1, m_store->item_tooltip(e.item_index));
        }
        row->setText(2, QString::number(e.chance, 'f', 2));
        m_details->addTopLevelItem(row);
    }
}

void FindItemGroupDialog::on_group_selected() {
    const QListWidgetItem* current = m_groups->currentItem();
    const bool ok = current != nullptr && m_target_ig->count() > 0;
    m_add->setEnabled(ok);
    if (!current) {
        m_details->clear();
        return;
    }
    populate_details(current->data(Qt::UserRole).toInt());
}

void FindItemGroupDialog::add_to_itemgroup() {
    const QListWidgetItem* current = m_groups->currentItem();
    if (!current) {
        return;
    }
    if (m_target_ig->count() <= 0) {
        QMessageBox::warning(this,
                             "No ItemGroup",
                             "No itemgroup is available. Select a monster with itemGroups "
                             "or create an itemgroup first.");
        return;
    }

    const int group_index = current->data(Qt::UserRole).toInt();
    const int ig_index = m_target_ig->currentData().toInt();
    openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(ig_index);
    if (!ig) {
        QMessageBox::warning(
            this, "Missing ItemGroup", QString("ItemGroup #%1 was not found.").arg(ig_index));
        return;
    }

    for (const auto& e : ig->entries) {
        if (e.group_index == group_index) {
            QMessageBox::information(this,
                                     "Already linked",
                                     QString("ItemGroup #%1 already references Group #%2.")
                                         .arg(ig_index)
                                         .arg(group_index));
            return;
        }
    }

    openkal::server::ItemGroupDef updated = *ig;
    openkal::server::ItemGroupEntry entry;
    entry.group_index = group_index;
    entry.chance = static_cast<float>(m_chance->value());
    updated.entries.push_back(entry);
    m_store->upsert_itemgroup(std::move(updated));
    m_did_add = true;

    QMessageBox::information(this,
                             "Added",
                             QString("Group #%1 added to ItemGroup #%2 with %3% chance.")
                                 .arg(group_index)
                                 .arg(ig_index)
                                 .arg(m_chance->value(), 0, 'f', 2));
}
