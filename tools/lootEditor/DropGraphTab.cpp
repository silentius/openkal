/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "DropGraphTab.h"

#include "dialogs/FindItemGroupDialog.h"
#include "dialogs/GroupEditorDialog.h"
#include "dialogs/ItemGroupEditorDialog.h"
#include "dialogs/NewIndexDialog.h"
#include "graph/GroupNodeItem.h"
#include "graph/ItemGroupNodeItem.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QGraphicsView>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <vector>

DropGraphTab::DropGraphTab(LootDataStore* store, QWidget* parent)
    : QWidget(parent), m_store(store) {
    m_icons = std::make_unique<ItemIconCache>(store->items());

    auto* layout = new QVBoxLayout(this);
    m_toolbar = new QToolBar(this);
    m_toolbar->addAction("New Group", this, &DropGraphTab::new_group);
    m_toolbar->addAction("New ItemGroup", this, &DropGraphTab::new_itemgroup);
    m_toolbar->addAction("Delete Selected", this, &DropGraphTab::delete_selected);
    m_toolbar->addAction("Refresh", this, &DropGraphTab::rebuild);
    m_toolbar->addAction("Find Item…", this, &DropGraphTab::find_item_in_groups);
    layout->addWidget(m_toolbar);

    m_scene = new DropGraphScene(store, m_icons.get(), this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    layout->addWidget(m_view);

    connect(store, &LootDataStore::selectionChanged, this, &DropGraphTab::rebuild);
    connect(store, &LootDataStore::dataChanged, this, &DropGraphTab::rebuild);
}

void DropGraphTab::rebuild() {
    m_icons = std::make_unique<ItemIconCache>(m_store->items());
    if (!m_scene) {
        m_scene = new DropGraphScene(m_store, m_icons.get(), this);
        m_view->setScene(m_scene);
    } else {
        m_scene->set_icon_cache(m_icons.get());
    }
    m_scene->rebuild();
}

void DropGraphTab::new_group() {
    std::set<int> used;
    for (const auto& [idx, _] : m_store->drop_db().groups()) {
        (void) _;
        used.insert(idx);
    }
    const int idx = m_store->drop_db().next_free_group_index();
    NewIndexDialog picker("New Group", idx, std::move(used), this);
    if (picker.exec() != QDialog::Accepted) {
        return;
    }
    openkal::server::GroupDef def;
    def.index = picker.index();
    GroupEditorDialog editor(def, m_store, this);
    if (editor.exec() == QDialog::Accepted) {
        m_store->upsert_group(editor.result_def());
        rebuild();
    }
}

void DropGraphTab::new_itemgroup() {
    std::set<int> used;
    for (const auto& [idx, _] : m_store->drop_db().itemgroups()) {
        (void) _;
        used.insert(idx);
    }
    const int idx = m_store->drop_db().next_free_itemgroup_index();
    NewIndexDialog picker("New ItemGroup", idx, std::move(used), this);
    if (picker.exec() != QDialog::Accepted) {
        return;
    }
    openkal::server::ItemGroupDef def;
    def.index = picker.index();
    ItemGroupEditorDialog editor(def, m_store, this);
    if (editor.exec() == QDialog::Accepted) {
        m_store->upsert_itemgroup(editor.result_def());
        rebuild();
    }
}

std::vector<int> DropGraphTab::all_itemgroups_using(int group_index) const {
    std::vector<int> out;
    for (const auto& [idx, def] : m_store->drop_db().itemgroups()) {
        for (const auto& e : def.entries) {
            if (e.group_index == group_index) {
                out.push_back(idx);
                break;
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<int> DropGraphTab::current_itemgroups_using(int group_index) const {
    std::vector<int> out;
    const MonsterRecord* monster = m_store->monsters().find(m_store->selected_monster());
    if (!monster) {
        return out;
    }

    // Prefer an explicitly selected itemgroup on the graph when it references the group.
    if (m_scene) {
        for (QGraphicsItem* item : m_scene->selectedItems()) {
            auto* ig = dynamic_cast<ItemGroupNodeItem*>(item);
            if (!ig) {
                continue;
            }
            const openkal::server::ItemGroupDef* def =
                m_store->drop_db().find_itemgroup(ig->itemgroup_index());
            if (!def) {
                continue;
            }
            for (const auto& e : def->entries) {
                if (e.group_index == group_index) {
                    out.push_back(ig->itemgroup_index());
                    break;
                }
            }
        }
    }
    if (!out.empty()) {
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    for (const auto& [ig, _] : monster->item_groups) {
        const openkal::server::ItemGroupDef* def = m_store->drop_db().find_itemgroup(ig);
        if (!def) {
            continue;
        }
        for (const auto& e : def->entries) {
            if (e.group_index == group_index) {
                out.push_back(ig);
                break;
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool DropGraphTab::unlink_group_from_itemgroup(int itemgroup_index, int group_index) {
    openkal::server::ItemGroupDef* ig = m_store->drop_db().find_itemgroup_mut(itemgroup_index);
    if (!ig) {
        return false;
    }
    openkal::server::ItemGroupDef updated = *ig;
    const auto before = updated.entries.size();
    updated.entries.erase(std::remove_if(updated.entries.begin(),
                                         updated.entries.end(),
                                         [group_index](const openkal::server::ItemGroupEntry& e) {
                                             return e.group_index == group_index;
                                         }),
                          updated.entries.end());
    if (updated.entries.size() == before) {
        return false;
    }
    m_store->upsert_itemgroup(std::move(updated));
    return true;
}

bool DropGraphTab::delete_group_selection(int group_index) {
    const std::vector<int> current = current_itemgroups_using(group_index);
    const std::vector<int> all = all_itemgroups_using(group_index);

    std::set<int> current_set(current.begin(), current.end());
    std::vector<int> elsewhere;
    for (int ig : all) {
        if (current_set.find(ig) == current_set.end()) {
            elsewhere.push_back(ig);
        }
    }

    if (!elsewhere.empty()) {
        if (current.empty()) {
            QStringList others;
            for (int ig : elsewhere) {
                others << QString("#%1").arg(ig);
            }
            QMessageBox::information(
                this,
                "Group still in use",
                QString("Group #%1 is used by other ItemGroup(s): %2.\n"
                        "Select the ItemGroup together with the Group to unlink it, "
                        "or edit that ItemGroup directly.")
                    .arg(group_index)
                    .arg(others.join(", ")));
            return false;
        }

        QStringList targets;
        for (int ig : current) {
            targets << QString("#%1").arg(ig);
        }
        QStringList others;
        for (int ig : elsewhere) {
            others << QString("#%1").arg(ig);
        }
        if (QMessageBox::question(this,
                                  "Unlink Group",
                                  QString("Group #%1 is also used by ItemGroup(s): %2.\n\n"
                                          "Remove it only from the current ItemGroup(s) (%3)?")
                                      .arg(group_index)
                                      .arg(others.join(", "))
                                      .arg(targets.join(", ")),
                                  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
            return false;
        }

        bool changed = false;
        for (int ig : current) {
            changed = unlink_group_from_itemgroup(ig, group_index) || changed;
        }
        return changed;
    }

    // Not used by any other ItemGroup — offer permanent delete.
    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Question);
    prompt.setWindowTitle("Delete Group");
    prompt.setText(QString("Group #%1 is not used by any other ItemGroup.").arg(group_index));
    prompt.setInformativeText(
        "Delete the group permanently, or only unlink it from the current ItemGroup(s)?");
    QPushButton* delete_btn = prompt.addButton("Delete Group", QMessageBox::DestructiveRole);
    QPushButton* unlink_btn = prompt.addButton("Unlink Only", QMessageBox::AcceptRole);
    prompt.addButton(QMessageBox::Cancel);
    prompt.exec();

    if (prompt.clickedButton() == nullptr ||
        prompt.clickedButton() == prompt.button(QMessageBox::Cancel)) {
        return false;
    }

    bool changed = false;
    for (int ig : current) {
        changed = unlink_group_from_itemgroup(ig, group_index) || changed;
    }

    if (prompt.clickedButton() == delete_btn) {
        changed = m_store->remove_group(group_index) || changed;
    } else if (prompt.clickedButton() == unlink_btn && current.empty()) {
        return false;
    }
    (void) unlink_btn;
    return changed;
}

void DropGraphTab::delete_selected() {
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    std::vector<int> group_indices;
    std::vector<int> itemgroup_indices;
    for (QGraphicsItem* item : selected) {
        if (auto* ig = dynamic_cast<ItemGroupNodeItem*>(item)) {
            itemgroup_indices.push_back(ig->itemgroup_index());
        } else if (auto* g = dynamic_cast<GroupNodeItem*>(item)) {
            group_indices.push_back(g->group_index());
        }
    }

    bool changed = false;

    for (int gid : group_indices) {
        changed = delete_group_selection(gid) || changed;
    }

    if (!itemgroup_indices.empty()) {
        if (QMessageBox::question(this,
                                  "Delete ItemGroup",
                                  "Delete the selected ItemGroup node(s) from the drop database?",
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            for (int ig : itemgroup_indices) {
                changed = m_store->remove_itemgroup(ig) || changed;
            }
        }
    }

    if (changed) {
        rebuild();
    }
}

int DropGraphTab::preferred_itemgroup() const {
    if (m_scene) {
        for (QGraphicsItem* item : m_scene->selectedItems()) {
            if (auto* ig = dynamic_cast<ItemGroupNodeItem*>(item)) {
                return ig->itemgroup_index();
            }
        }
    }
    const MonsterRecord* monster = m_store->monsters().find(m_store->selected_monster());
    if (monster && !monster->item_groups.empty()) {
        return monster->item_groups.front().first;
    }
    return -1;
}

void DropGraphTab::find_item_in_groups() {
    FindItemGroupDialog dlg(m_store, preferred_itemgroup(), this);
    dlg.exec();
    if (dlg.did_add()) {
        rebuild();
    }
}
