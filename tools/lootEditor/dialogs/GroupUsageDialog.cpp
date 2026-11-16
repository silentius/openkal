/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/GroupUsageDialog.h"

#include "dialogs/GroupEditorDialog.h"
#include "dialogs/ItemGroupEditorDialog.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

GroupUsageDialog::GroupUsageDialog(int group_index, LootDataStore* store, QWidget* parent)
    : QDialog(parent), m_group_index(group_index), m_store(store) {
    const openkal::server::GroupDef* group = m_store->drop_db().find_group(m_group_index);
    QString title = QString("ItemGroups using Group #%1").arg(m_group_index);
    if (group && !group->name.empty()) {
        title += " — " + QString::fromStdString(group->name);
    }
    setWindowTitle(title);
    resize(720, 420);

    auto* layout = new QVBoxLayout(this);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"ItemGroup", "Chance %", "Used by monsters"});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 90);
    m_tree->setSortingEnabled(true);
    connect(
        m_tree, &QTreeWidget::itemDoubleClicked, this, &GroupUsageDialog::on_item_double_clicked);
    layout->addWidget(m_tree, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* edit_group_btn = buttons->addButton("Edit Group…", QDialogButtonBox::ActionRole);
    auto* edit_ig_btn = buttons->addButton("Edit ItemGroup…", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(edit_group_btn, &QPushButton::clicked, this, &GroupUsageDialog::edit_group);
    connect(edit_ig_btn, &QPushButton::clicked, this, &GroupUsageDialog::edit_selected_itemgroup);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    populate();
}

std::vector<GroupUsageDialog::UsageRow> GroupUsageDialog::matching_itemgroups() const {
    std::vector<UsageRow> rows;
    for (const auto& [idx, def] : m_store->drop_db().itemgroups()) {
        for (const auto& e : def.entries) {
            if (e.group_index == m_group_index) {
                rows.push_back({idx, e.chance});
                break;
            }
        }
    }
    std::sort(rows.begin(), rows.end(), [](const UsageRow& a, const UsageRow& b) {
        return a.itemgroup_index < b.itemgroup_index;
    });
    return rows;
}

QString GroupUsageDialog::monsters_using(int itemgroup_index) const {
    QStringList names;
    for (const MonsterRecord* rec : m_store->monsters().sorted_monsters()) {
        if (!rec) {
            continue;
        }
        for (const auto& [ig, _] : rec->item_groups) {
            if (ig == itemgroup_index) {
                QString label = QString::fromStdString(rec->name);
                if (label.isEmpty()) {
                    label = QString("Monster #%1").arg(rec->index);
                } else {
                    label += QString(" (#%1)").arg(rec->index);
                }
                names << label;
                break;
            }
        }
    }
    if (names.isEmpty()) {
        return QStringLiteral("—");
    }
    return names.join(", ");
}

void GroupUsageDialog::populate() {
    m_tree->clear();
    const std::vector<UsageRow> rows = matching_itemgroups();
    m_status->setText(QString("Group #%1 is referenced by %2 itemgroup(s).")
                          .arg(m_group_index)
                          .arg(static_cast<int>(rows.size())));

    for (const UsageRow& row : rows) {
        const openkal::server::ItemGroupDef* def =
            m_store->drop_db().find_itemgroup(row.itemgroup_index);
        QString label = QString("ItemGroup #%1").arg(row.itemgroup_index);
        if (def && !def->name.empty()) {
            label += " — " + QString::fromStdString(def->name);
        }

        auto* item = new QTreeWidgetItem();
        item->setText(0, label);
        item->setData(0, Qt::UserRole, row.itemgroup_index);
        item->setText(1, QString::number(row.chance, 'f', 2));
        item->setText(2, monsters_using(row.itemgroup_index));
        item->setToolTip(2, item->text(2));
        m_tree->addTopLevelItem(item);
    }

    if (m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
    }
}

void GroupUsageDialog::edit_group() {
    openkal::server::GroupDef* g = m_store->drop_db().find_group_mut(m_group_index);
    if (!g) {
        return;
    }
    GroupEditorDialog dlg(*g, m_store, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_store->upsert_group(dlg.result_def());
    m_group_changed = true;
    const openkal::server::GroupDef* updated = m_store->drop_db().find_group(m_group_index);
    QString title = QString("ItemGroups using Group #%1").arg(m_group_index);
    if (updated && !updated->name.empty()) {
        title += " — " + QString::fromStdString(updated->name);
    }
    setWindowTitle(title);
}

void GroupUsageDialog::edit_selected_itemgroup() {
    const QTreeWidgetItem* current = m_tree->currentItem();
    if (!current) {
        return;
    }
    const int ig_index = current->data(0, Qt::UserRole).toInt();
    openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(ig_index);
    if (!ig) {
        return;
    }
    ItemGroupEditorDialog dlg(*ig, m_store, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_store->upsert_itemgroup(dlg.result_def());
    m_itemgroup_changed = true;
    populate();
}

void GroupUsageDialog::on_item_double_clicked(QTreeWidgetItem* item, int column) {
    (void) column;
    if (!item) {
        return;
    }
    m_tree->setCurrentItem(item);
    edit_selected_itemgroup();
}
