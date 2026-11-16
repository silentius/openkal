/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/ItemPickerDialog.h"

#include "openkal/server/world/ItemGroupDb.h"
#include "model/ItemPickerListModel.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QTabBar>
#include <QVBoxLayout>
#include <algorithm>
#include <map>
#include <memory>
#include <unordered_set>

namespace {

    constexpr int kPickerIconSize = 48;

    [[nodiscard]] bool is_quest_item(const openkal::client::ItemDef& def) {
        return def.class_big() == "quest";
    }

    [[nodiscard]] bool is_charm_item(const openkal::client::ItemDef& def) {
        return def.class_small() == "charm";
    }

    [[nodiscard]] bool is_consumable_item(const openkal::client::ItemDef& def) {
        return def.class_small() == "refresh" || def.class_small() == "hpautorefresh";
    }

    [[nodiscard]] bool is_pet_item(const openkal::client::ItemDef& def) {
        return def.class_big() == "pet" || def.class_small() == "egg";
    }

    [[nodiscard]] bool is_specialized_general(const openkal::client::ItemDef& def) {
        return is_quest_item(def) || is_charm_item(def) || is_consumable_item(def) ||
               def.is_ornament() || is_pet_item(def);
    }

    /** Weapon=0, defense=1, other=2 — keeps swords before armor inside a grade. */
    [[nodiscard]] int gear_kind_rank(const openkal::client::ItemDef& def) {
        if (def.is_weapon()) {
            return 0;
        }
        if (def.is_defense()) {
            return 1;
        }
        return 2;
    }

}  // namespace

ItemPickerDialog::ItemPickerDialog(LootDataStore* store,
                                   ItemIconCache* icons,
                                   int initial_index,
                                   std::unordered_set<int> occupied_items,
                                   QWidget* parent)
    : QDialog(parent), m_store(store), m_icons(icons), m_occupied_items(std::move(occupied_items)) {
    if (initial_index > 0) {
        m_selected = {initial_index};
    }

    setWindowTitle("Pick Item");
    resize(920, 680);

    m_catalog = build_catalog();

    auto* layout = new QVBoxLayout(this);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search by name or index…");
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_tabs = new QTabBar(this);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(true);
    for (const char* title : {"All",
                              "Knight",
                              "Mage",
                              "Archer",
                              "Thief",
                              "Quest",
                              "Charm",
                              "Consumable",
                              "Ornament",
                              "Pet",
                              "Other"}) {
        m_tabs->addTab(QString::fromUtf8(title));
    }
    layout->addWidget(m_tabs);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    m_list = make_icon_list(this);
    wire_list(m_list);
    layout->addWidget(m_list, 1);

    m_grade_scroll = new QScrollArea(this);
    m_grade_scroll->setWidgetResizable(true);
    m_grade_scroll->setFrameShape(QFrame::NoFrame);
    m_grade_host = new QWidget(m_grade_scroll);
    m_grade_layout = new QVBoxLayout(m_grade_host);
    m_grade_layout->setContentsMargins(0, 0, 0, 0);
    m_grade_layout->setSpacing(10);
    m_grade_layout->addStretch(1);
    m_grade_scroll->setWidget(m_grade_host);
    m_grade_scroll->setVisible(false);
    layout->addWidget(m_grade_scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &ItemPickerDialog::refresh_list);
    connect(m_tabs, &QTabBar::currentChanged, this, &ItemPickerDialog::refresh_list);
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemPickerDialog::accept_selection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (initial_index > 0) {
        m_tabs->setCurrentIndex(static_cast<int>(primary_tab_for(initial_index)));
    }

    refresh_list();
    if (initial_index > 0) {
        select_item_in_view(initial_index);
    }

    m_search->setFocus();
}

int ItemPickerDialog::selected_index() const {
    return m_selected.empty() ? -1 : m_selected.front();
}

QListView* ItemPickerDialog::make_icon_list(QWidget* parent) {
    auto* list = new QListView(parent);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(kPickerIconSize, kPickerIconSize));
    list->setGridSize(QSize(88, 96));
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setSpacing(6);
    list->setUniformItemSizes(true);
    list->setWordWrap(true);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setModel(new ItemPickerListModel(*m_store, *m_icons, m_occupied_items, list));
    list->setItemDelegate(new ItemPickerTileDelegate(list));
    return list;
}

void ItemPickerDialog::wire_list(QListView* list) {
    connect(list, &QListView::doubleClicked, this, &ItemPickerDialog::accept_selection);
    connect(list, &QListView::activated, this, &ItemPickerDialog::accept_selection);
    connect(list, &QListView::clicked, this, [this, list](const QModelIndex& item) {
        on_item_clicked(list, item);
    });
    connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        if (m_restoring_selection) {
            return;
        }
        sync_selected_from_views();
        update_status(-1);
    });
}

void ItemPickerDialog::on_item_clicked(QListView* list, const QModelIndex& item) {
    if (!list || !item.isValid()) {
        return;
    }
    const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if (!(mods & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
        clear_selection_except(list);
    }
    sync_selected_from_views();
    update_status(-1);
}

void ItemPickerDialog::clear_selection_except(QListView* keep) {
    auto clear_if = [keep](QListView* list) {
        if (list && list != keep) {
            list->clearSelection();
        }
    };
    clear_if(m_list);
    for (const auto& [grade, group] : m_grade_groups) {
        clear_if(group.list);
    }
}

void ItemPickerDialog::sync_selected_from_views() {
    std::vector<int> picked;
    std::unordered_set<int> seen;

    auto collect = [&](QListView* list) {
        if (!list) {
            return;
        }
        auto selected = list->selectionModel()->selectedIndexes();
        std::sort(selected.begin(), selected.end(), [](const QModelIndex& a, const QModelIndex& b) {
            return a.row() < b.row();
        });
        for (const QModelIndex& item : selected) {
            const int idx = item.data(Qt::UserRole).toInt();
            if (idx <= 0 || !seen.insert(idx).second) {
                continue;
            }
            picked.push_back(idx);
        }
    };

    if (is_class_tab(static_cast<Tab>(m_tabs->currentIndex()))) {
        for (const auto& [grade, group] : m_grade_groups) {
            collect(group.list);
        }
    } else {
        collect(m_list);
    }
    m_selected = std::move(picked);
}

std::vector<int> ItemPickerDialog::build_catalog() const {
    std::vector<int> indices;
    indices.reserve(m_store->items().by_index().size());
    for (const auto& [index, def] : m_store->items().by_index()) {
        if (index == openkal::server::kMoneyItemIndex || def.is_coin()) {
            continue;
        }
        indices.push_back(index);
    }
    std::sort(indices.begin(), indices.end(), [this](int a, int b) { return gear_less(a, b); });
    return indices;
}

bool ItemPickerDialog::is_class_tab(Tab tab) {
    return tab == Tab::Knight || tab == Tab::Mage || tab == Tab::Archer || tab == Tab::Thief;
}

bool ItemPickerDialog::gear_less(int a, int b) const {
    const openkal::client::ItemDef* da = m_store->items().find(a);
    const openkal::client::ItemDef* db = m_store->items().find(b);
    if (!da || !db) {
        return a < b;
    }

    const int ga = da->grade();
    const int gb = db->grade();
    if (ga != gb) {
        return ga < gb;
    }

    const int ka = gear_kind_rank(*da);
    const int kb = gear_kind_rank(*db);
    if (ka != kb) {
        return ka < kb;
    }

    const int cmp =
        QString::compare(m_store->item_name(a), m_store->item_name(b), Qt::CaseInsensitive);
    if (cmp != 0) {
        return cmp < 0;
    }
    return a < b;
}

ItemPickerDialog::Tab ItemPickerDialog::primary_tab_for(int item_index) const {
    const openkal::client::ItemDef* def = m_store->items().find(item_index);
    if (!def) {
        return Tab::All;
    }
    if (def->class_limit() == 0) {
        return Tab::Knight;
    }
    if (def->class_limit() == 1) {
        return Tab::Mage;
    }
    if (def->class_limit() == 2) {
        return Tab::Archer;
    }
    if (def->class_limit() == 3) {
        return Tab::Thief;
    }
    if (is_quest_item(*def)) {
        return Tab::Quest;
    }
    if (is_charm_item(*def)) {
        return Tab::Charm;
    }
    if (is_consumable_item(*def)) {
        return Tab::Consumable;
    }
    if (def->is_ornament()) {
        return Tab::Ornament;
    }
    if (is_pet_item(*def)) {
        return Tab::Pet;
    }
    return Tab::Other;
}

bool ItemPickerDialog::passes_filter(int item_index, const QString& filter, Tab tab) const {
    const openkal::client::ItemDef* def = m_store->items().find(item_index);
    if (!def) {
        return false;
    }

    switch (tab) {
    case Tab::All:
        break;
    case Tab::Knight:
        if (def->class_limit() != 0) {
            return false;
        }
        break;
    case Tab::Mage:
        if (def->class_limit() != 1) {
            return false;
        }
        break;
    case Tab::Archer:
        if (def->class_limit() != 2) {
            return false;
        }
        break;
    case Tab::Thief:
        if (def->class_limit() != 3) {
            return false;
        }
        break;
    case Tab::Quest:
        if (!is_quest_item(*def)) {
            return false;
        }
        break;
    case Tab::Charm:
        if (!is_charm_item(*def)) {
            return false;
        }
        break;
    case Tab::Consumable:
        if (!is_consumable_item(*def)) {
            return false;
        }
        break;
    case Tab::Ornament:
        if (!def->is_ornament()) {
            return false;
        }
        break;
    case Tab::Pet:
        if (!is_pet_item(*def)) {
            return false;
        }
        break;
    case Tab::Other:
        if (def->class_limit() >= 0 || is_specialized_general(*def)) {
            return false;
        }
        break;
    }

    if (filter.isEmpty()) {
        return true;
    }

    const QString name = m_store->item_name(item_index);
    if (name.contains(filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (QString::number(item_index).contains(filter)) {
        return true;
    }
    if (QString("G%1").arg(def->grade()).contains(filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (QString::fromStdString(def->clazz()).contains(filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (QString::fromStdString(def->class_small()).contains(filter, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

int ItemPickerDialog::rebuild_flat_list(const QString& filter, Tab tab) {
    std::vector<int> indices;
    indices.reserve(m_catalog.size());
    for (int item_index : m_catalog) {
        if (!passes_filter(item_index, filter, tab)) {
            continue;
        }
        indices.push_back(item_index);
    }
    const int shown = static_cast<int>(indices.size());
    static_cast<ItemPickerListModel*>(m_list->model())->replace_items(std::move(indices));
    return shown;
}

int ItemPickerDialog::rebuild_grade_groups(const QString& filter, Tab tab) {
    while (QLayoutItem* child = m_grade_layout->takeAt(0)) {
        const std::unique_ptr<QLayoutItem> removed(child);
    }

    std::map<int, std::vector<int>> by_grade;
    for (int item_index : m_catalog) {
        if (!passes_filter(item_index, filter, tab)) {
            continue;
        }
        const openkal::client::ItemDef* def = m_store->items().find(item_index);
        const int grade = def ? def->grade() : 0;
        by_grade[grade].push_back(item_index);
    }

    for (auto group = m_grade_groups.begin(); group != m_grade_groups.end();) {
        if (by_grade.count(group->first) == 0) {
            const std::unique_ptr<QGroupBox> removed(group->second.box);
            group = m_grade_groups.erase(group);
        } else {
            ++group;
        }
    }

    int shown = 0;
    for (auto& [grade, indices] : by_grade) {
        const auto less = [this](int a, int b) { return gear_less(a, b); };
        // Filtering preserves catalog order; an open dialog may observe changed keys after reload.
        if (!std::is_sorted(indices.begin(), indices.end(), less)) {
            std::sort(indices.begin(), indices.end(), less);
        }

        const QString title =
            grade > 0 ? QString("G%1  (%2)").arg(grade).arg(static_cast<int>(indices.size()))
                      : QString("No grade  (%1)").arg(static_cast<int>(indices.size()));
        auto& group = m_grade_groups[grade];
        if (!group.box) {
            group.box = new QGroupBox(m_grade_host);
            auto* box_layout = new QVBoxLayout(group.box);
            box_layout->setContentsMargins(8, 8, 8, 8);
            group.list = make_icon_list(group.box);
            group.list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            group.list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            wire_list(group.list);
            box_layout->addWidget(group.list);
        }
        group.box->setTitle(title);

        const int rows = std::max(1, (static_cast<int>(indices.size()) + 7) / 8);
        group.list->setMinimumHeight(rows * 100);
        group.list->setMaximumHeight(rows * 100);
        shown += static_cast<int>(indices.size());
        static_cast<ItemPickerListModel*>(group.list->model())->replace_items(std::move(indices));
        m_grade_layout->addWidget(group.box);
        group.box->show();
    }

    m_grade_layout->addStretch(1);
    return shown;
}

void ItemPickerDialog::select_item_in_view(int item_index) {
    auto select_in = [item_index](QListView* list) -> bool {
        for (int row = 0; row < list->model()->rowCount(); ++row) {
            const QModelIndex index = list->model()->index(row, 0);
            if (index.data(Qt::UserRole).toInt() == item_index) {
                list->setCurrentIndex(index);
                list->selectionModel()->select(index, QItemSelectionModel::Select);
                list->scrollTo(index);
                return true;
            }
        }
        return false;
    };

    clear_selection_except(nullptr);
    if (!is_class_tab(static_cast<Tab>(m_tabs->currentIndex()))) {
        if (select_in(m_list)) {
            m_selected = {item_index};
        }
        return;
    }
    for (const auto& [grade, group] : m_grade_groups) {
        if (select_in(group.list)) {
            m_selected = {item_index};
            return;
        }
    }
}

void ItemPickerDialog::update_status(int shown) {
    static int last_shown = 0;
    if (shown >= 0) {
        last_shown = shown;
    }

    const QString tab_name = m_tabs->tabText(m_tabs->currentIndex());
    const Tab tab = static_cast<Tab>(m_tabs->currentIndex());
    const bool class_view = is_class_tab(tab);

    QString occupied_note;
    if (!m_occupied_items.empty()) {
        occupied_note = QString(" — %1 already in group (blue ●)")
                            .arg(static_cast<int>(m_occupied_items.size()));
    }

    const QString selected_note =
        m_selected.empty() ? QString()
                           : QString(" — %1 selected").arg(static_cast<int>(m_selected.size()));

    if (class_view) {
        m_status->setText(QString("%1 — %2 items in grade groups%3%4 — Ctrl/Shift multi-select")
                              .arg(tab_name)
                              .arg(last_shown)
                              .arg(occupied_note)
                              .arg(selected_note));
    } else {
        m_status->setText(QString("%1 — showing %2 of %3 items%4%5 — Ctrl/Shift multi-select")
                              .arg(tab_name)
                              .arg(last_shown)
                              .arg(static_cast<int>(m_catalog.size()))
                              .arg(occupied_note)
                              .arg(selected_note));
    }
}

void ItemPickerDialog::refresh_list() {
    const QString filter = m_search->text().trimmed();
    const Tab tab = static_cast<Tab>(m_tabs->currentIndex());
    const bool class_view = is_class_tab(tab);
    const std::unordered_set<int> keep(m_selected.begin(), m_selected.end());

    int shown = 0;
    {
        const QScopedValueRollback<bool> restoring(m_restoring_selection, true);
        m_list->setVisible(!class_view);
        m_grade_scroll->setVisible(class_view);

        shown = class_view ? rebuild_grade_groups(filter, tab) : rebuild_flat_list(filter, tab);

        // Selection signals from rebuilding must not replace the saved IDs mid-restore.
        clear_selection_except(nullptr);
        const auto restore = [&keep](QListView* list) {
            for (int row = 0; row < list->model()->rowCount(); ++row) {
                const QModelIndex index = list->model()->index(row, 0);
                if (keep.count(index.data(Qt::UserRole).toInt()) != 0) {
                    list->selectionModel()->select(index, QItemSelectionModel::Select);
                }
            }
        };
        if (!keep.empty()) {
            if (!class_view) {
                restore(m_list);
            } else {
                for (const auto& [grade, group] : m_grade_groups) {
                    restore(group.list);
                }
            }
        }
    }

    sync_selected_from_views();
    update_status(shown);
}

void ItemPickerDialog::accept_selection() {
    if (result() == QDialog::Accepted) {
        return;
    }
    sync_selected_from_views();
    if (m_selected.empty()) {
        return;
    }
    accept();
}
