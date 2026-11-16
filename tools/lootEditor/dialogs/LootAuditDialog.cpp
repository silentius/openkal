/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/LootAuditDialog.h"

#include "dialogs/GroupEditorDialog.h"
#include "dialogs/ItemGroupEditorDialog.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace {
    /** Returns true when an ItemGroup references at least one existing Group.
     * @param store Session store.
     * @param def ItemGroup definition.
     * @return True when a valid nested group exists.
     */
    [[nodiscard]] bool itemgroup_has_valid_group(const LootDataStore& store,
                                                 const openkal::server::ItemGroupDef& def) {
        for (const auto& e : def.entries) {
            if (e.group_index > 0 && store.drop_db().find_group(e.group_index) != nullptr) {
                return true;
            }
        }
        return false;
    }

    /** Returns true when a Group has money or an existing catalog item.
     * @param store Session store.
     * @param def Group definition.
     * @return True when a valid drop entry exists.
     */
    [[nodiscard]] bool group_has_valid_item(const LootDataStore& store,
                                            const openkal::server::GroupDef& def) {
        for (const auto& e : def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Money) {
                return true;
            }
            if (e.kind == openkal::server::GroupEntry::Kind::Item && e.item_index > 0 &&
                store.items().find(e.item_index) != nullptr) {
                return true;
            }
        }
        return false;
    }

    /** Returns true when a group entry references a missing item definition.
     * @param store Session store.
     * @param entry Group entry.
     * @return True for invalid item rows.
     */
    [[nodiscard]] bool is_invalid_item_entry(const LootDataStore& store,
                                             const openkal::server::GroupEntry& entry) {
        if (entry.kind != openkal::server::GroupEntry::Kind::Item) {
            return false;
        }
        return entry.item_index <= 0 || store.items().find(entry.item_index) == nullptr;
    }

    /** Collects missing item definition indices from a group.
     * @param store Session store.
     * @param def Group definition.
     * @return Sorted unique missing item indices.
     */
    [[nodiscard]] std::vector<int> missing_item_indices(const LootDataStore& store,
                                                        const openkal::server::GroupDef& def) {
        std::set<int> missing;
        for (const auto& e : def.entries) {
            if (e.kind != openkal::server::GroupEntry::Kind::Item) {
                continue;
            }
            if (is_invalid_item_entry(store, e)) {
                missing.insert(e.item_index);
            }
        }
        return std::vector<int>(missing.begin(), missing.end());
    }

    /** Formats missing group refs for an ItemGroup.
     * @param store Session store.
     * @param def ItemGroup definition.
     * @return Human-readable detail string.
     */
    [[nodiscard]] QString itemgroup_detail(const LootDataStore& store,
                                           const openkal::server::ItemGroupDef& def) {
        if (def.entries.empty()) {
            return "No group entries";
        }
        QStringList missing;
        int valid = 0;
        for (const auto& e : def.entries) {
            if (e.group_index > 0 && store.drop_db().find_group(e.group_index) != nullptr) {
                ++valid;
            } else {
                missing << QString("#%1").arg(e.group_index);
            }
        }
        if (valid == 0) {
            return QString("No valid groups (refs: %1)").arg(missing.join(", "));
        }
        return {};
    }

    /** Formats empty-group details.
     * @param def Group definition.
     * @return Human-readable detail string.
     */
    [[nodiscard]] QString empty_group_detail(const openkal::server::GroupDef& def) {
        if (def.entries.empty()) {
            return "No entries";
        }
        return QString("%1 entr(y/ies), none valid").arg(static_cast<int>(def.entries.size()));
    }
}  // namespace

LootAuditDialog::LootAuditDialog(Mode mode, LootDataStore* store, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_store(store) {
    switch (m_mode) {
    case Mode::EmptyItemGroups:
        setWindowTitle("Empty ItemGroups");
        break;
    case Mode::EmptyGroups:
        setWindowTitle("Empty Groups");
        break;
    case Mode::InvalidItemGroups:
        setWindowTitle("Groups with Invalid Items");
        break;
    }
    resize(720, 520);

    auto* layout = new QVBoxLayout(this);
    m_status = new QLabel(this);
    layout->addWidget(m_status);

    m_tree = new QTreeWidget(this);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setSortingEnabled(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(
        m_tree, &QTreeWidget::itemDoubleClicked, this, &LootAuditDialog::on_item_double_clicked);
    layout->addWidget(m_tree, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* edit = buttons->addButton("Edit…", QDialogButtonBox::ActionRole);
    auto* cleanup = buttons->addButton("Cleanup Selected…", QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(edit, &QPushButton::clicked, this, &LootAuditDialog::edit_selected);
    connect(cleanup, &QPushButton::clicked, this, &LootAuditDialog::cleanup_selected);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    populate();
}

void LootAuditDialog::populate() {
    m_tree->clear();

    switch (m_mode) {
    case Mode::EmptyItemGroups: {
        m_tree->setHeaderLabels({"ItemGroup", "Name", "Detail"});
        std::vector<int> indices;
        for (const auto& [idx, def] : m_store->drop_db().itemgroups()) {
            if (!itemgroup_has_valid_group(*m_store, def)) {
                indices.push_back(idx);
            }
        }
        std::sort(indices.begin(), indices.end());
        for (int idx : indices) {
            const openkal::server::ItemGroupDef* def = m_store->drop_db().find_itemgroup(idx);
            if (!def) {
                continue;
            }
            auto* row = new QTreeWidgetItem();
            row->setData(0, Qt::UserRole, idx);
            row->setText(0, QString::number(idx));
            row->setText(1, QString::fromStdString(def->name));
            row->setText(2, itemgroup_detail(*m_store, *def));
            m_tree->addTopLevelItem(row);
        }
        m_status->setText(QString("%1 ItemGroup(s) without a valid nested Group")
                              .arg(m_tree->topLevelItemCount()));
        break;
    }
    case Mode::EmptyGroups: {
        m_tree->setHeaderLabels({"Group", "Name", "Detail"});
        std::vector<int> indices;
        for (const auto& [idx, def] : m_store->drop_db().groups()) {
            if (!group_has_valid_item(*m_store, def)) {
                indices.push_back(idx);
            }
        }
        std::sort(indices.begin(), indices.end());
        for (int idx : indices) {
            const openkal::server::GroupDef* def = m_store->drop_db().find_group(idx);
            if (!def) {
                continue;
            }
            auto* row = new QTreeWidgetItem();
            row->setData(0, Qt::UserRole, idx);
            row->setText(0, QString::number(idx));
            row->setText(1, QString::fromStdString(def->name));
            row->setText(2, empty_group_detail(*def));
            m_tree->addTopLevelItem(row);
        }
        m_status->setText(QString("%1 Group(s) without a valid item or money entry")
                              .arg(m_tree->topLevelItemCount()));
        break;
    }
    case Mode::InvalidItemGroups: {
        m_tree->setHeaderLabels({"Group", "Name", "Missing item(s)"});
        std::vector<int> indices;
        for (const auto& [idx, def] : m_store->drop_db().groups()) {
            if (!missing_item_indices(*m_store, def).empty()) {
                indices.push_back(idx);
            }
        }
        std::sort(indices.begin(), indices.end());
        for (int idx : indices) {
            const openkal::server::GroupDef* def = m_store->drop_db().find_group(idx);
            if (!def) {
                continue;
            }
            const std::vector<int> missing = missing_item_indices(*m_store, *def);
            QStringList parts;
            for (int item_index : missing) {
                parts << QString("#%1").arg(item_index);
            }
            auto* row = new QTreeWidgetItem();
            row->setData(0, Qt::UserRole, idx);
            row->setText(0, QString::number(idx));
            row->setText(1, QString::fromStdString(def->name));
            row->setText(2, parts.join(", "));
            m_tree->addTopLevelItem(row);
        }
        m_status->setText(QString("%1 Group(s) reference missing item definition indices")
                              .arg(m_tree->topLevelItemCount()));
        break;
    }
    }

    m_tree->resizeColumnToContents(0);
    m_tree->resizeColumnToContents(1);
    if (m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
    }
}

void LootAuditDialog::on_item_double_clicked(QTreeWidgetItem* item, int column) {
    (void) column;
    if (item) {
        edit_selected();
    }
}

void LootAuditDialog::edit_selected() {
    const QTreeWidgetItem* current = m_tree->currentItem();
    if (!current) {
        return;
    }
    const int index = current->data(0, Qt::UserRole).toInt();

    if (m_mode == Mode::EmptyItemGroups) {
        openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(index);
        if (!ig) {
            return;
        }
        ItemGroupEditorDialog dlg(*ig, m_store, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_store->upsert_itemgroup(dlg.result_def());
            populate();
        }
        return;
    }

    openkal::server::GroupDef* g = m_store->drop_db().find_group_mut(index);
    if (!g) {
        return;
    }
    GroupEditorDialog dlg(*g, m_store, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_store->upsert_group(dlg.result_def());
        populate();
    }
}

std::vector<int> LootAuditDialog::selected_indices() const {
    std::vector<int> indices;
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    indices.reserve(static_cast<std::size_t>(selected.size()));
    for (const QTreeWidgetItem* item : selected) {
        if (item) {
            indices.push_back(item->data(0, Qt::UserRole).toInt());
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

void LootAuditDialog::unlink_group_from_all_itemgroups(int group_index) {
    std::vector<int> ig_indices;
    for (const auto& [ig_index, def] : m_store->drop_db().itemgroups()) {
        for (const auto& e : def.entries) {
            if (e.group_index == group_index) {
                ig_indices.push_back(ig_index);
                break;
            }
        }
    }
    for (int ig_index : ig_indices) {
        openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(ig_index);
        if (!ig) {
            continue;
        }
        openkal::server::ItemGroupDef updated = *ig;
        updated.entries.erase(
            std::remove_if(updated.entries.begin(),
                           updated.entries.end(),
                           [group_index](const openkal::server::ItemGroupEntry& e) {
                               return e.group_index == group_index;
                           }),
            updated.entries.end());
        m_store->upsert_itemgroup(std::move(updated));
    }
}

int LootAuditDialog::strip_invalid_items_from_group(int group_index) {
    openkal::server::GroupDef* group = m_store->drop_db().find_group_mut(group_index);
    if (!group) {
        return 0;
    }

    openkal::server::GroupDef updated = *group;
    const std::size_t before = updated.entries.size();
    updated.entries.erase(std::remove_if(updated.entries.begin(),
                                         updated.entries.end(),
                                         [this](const openkal::server::GroupEntry& e) {
                                             return is_invalid_item_entry(*m_store, e);
                                         }),
                          updated.entries.end());
    const int removed = static_cast<int>(before - updated.entries.size());
    if (removed <= 0) {
        return 0;
    }
    m_store->upsert_group(std::move(updated));
    return removed;
}

void LootAuditDialog::unlink_itemgroup_from_all_monsters(int itemgroup_index) {
    for (const MonsterRecord* monster : m_store->monsters().sorted_monsters()) {
        if (!monster) {
            continue;
        }
        std::vector<std::pair<int, int>> kept;
        kept.reserve(monster->item_groups.size());
        bool changed = false;
        for (const auto& [ig, attempts] : monster->item_groups) {
            if (ig == itemgroup_index) {
                changed = true;
                continue;
            }
            kept.emplace_back(ig, attempts);
        }
        if (changed) {
            (void) m_store->set_monster_item_groups(monster->index, std::move(kept));
        }
    }
}

void LootAuditDialog::cleanup_selected() {
    const std::vector<int> indices = selected_indices();
    if (indices.empty()) {
        QMessageBox::information(this, "Cleanup", "Select one or more rows to clean up.");
        return;
    }

    if (m_mode == Mode::InvalidItemGroups) {
        const QString prompt =
            QString("Remove missing item definition entries from %1 selected Group(s)?\n\n"
                    "The Group definitions are kept; only invalid item rows are deleted.")
                .arg(static_cast<int>(indices.size()));
        if (QMessageBox::question(this,
                                  "Cleanup Selected",
                                  prompt,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes) {
            return;
        }

        int groups_updated = 0;
        int entries_removed = 0;
        for (int index : indices) {
            const int removed = strip_invalid_items_from_group(index);
            if (removed > 0) {
                ++groups_updated;
                entries_removed += removed;
            }
        }

        populate();
        QMessageBox::information(
            this,
            "Cleanup",
            QString("Updated %1 Group(s), removed %2 invalid item entr(y/ies).")
                .arg(groups_updated)
                .arg(entries_removed));
        return;
    }

    const bool deleting_itemgroups = m_mode == Mode::EmptyItemGroups;
    const QString kind = deleting_itemgroups ? "ItemGroup" : "Group";
    const QString prompt =
        QString("Permanently delete %1 selected %2(s)?\n\n"
                "References will be unlinked first. Save afterward to remove the JSON files.")
            .arg(static_cast<int>(indices.size()))
            .arg(kind);
    if (QMessageBox::question(this,
                              "Cleanup Selected",
                              prompt,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    int removed = 0;
    for (int index : indices) {
        if (deleting_itemgroups) {
            unlink_itemgroup_from_all_monsters(index);
            if (m_store->remove_itemgroup(index)) {
                ++removed;
            }
        } else {
            unlink_group_from_all_itemgroups(index);
            if (m_store->remove_group(index)) {
                ++removed;
            }
        }
    }

    populate();
    QMessageBox::information(this, "Cleanup", QString("Deleted %1 %2(s).").arg(removed).arg(kind));
}
