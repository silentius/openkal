/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ItemViewerWindow.h"

#include "model/ItemIconCache.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/PrefixDb.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QTabBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <map>

namespace {

    constexpr int kIconSize = 48;

    [[nodiscard]] QString as_text(const std::string& value) {
        return QString::fromStdString(value);
    }

    [[nodiscard]] QString item_type(const openkal::client::ItemDef& def) {
        if (!def.class_big().empty() || !def.class_small().empty()) {
            return as_text(def.class_big()) + " / " + as_text(def.class_small());
        }
        return as_text(def.clazz());
    }

    [[nodiscard]] QString class_limit_label(int class_limit) {
        static constexpr const char* kClasses[] = {"Knight", "Mage", "Archer", "Thief"};
        if (class_limit >= 0 && class_limit < 4) {
            return QString::fromUtf8(kClasses[class_limit]);
        }
        return "Any";
    }

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

    void add_prefix_stat(QTreeWidgetItem* parent, const QString& name, int value) {
        if (value == 0) {
            return;
        }
        auto* row = new QTreeWidgetItem(parent);
        row->setText(0, name);
        row->setText(1, QString::number(value));
    }

}  // namespace

ItemViewerWindow::ItemViewerWindow(const openkal::client::ItemDb& catalog,
                                   ItemIconCache& icons,
                                   QWidget* parent)
    : QMainWindow(parent), m_catalog(catalog), m_icons(icons) {
    setWindowTitle("OpenKal Item Viewer");
    resize(1220, 780);

    for (const auto& [index, def] : m_catalog.by_index()) {
        if (index != openkal::server::kMoneyItemIndex && !def.is_coin()) {
            m_indices.push_back(index);
        }
    }
    std::sort(m_indices.begin(), m_indices.end());

    auto* central = new QWidget(this);
    auto* central_layout = new QVBoxLayout(central);
    central_layout->setContentsMargins(8, 8, 8, 8);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);

    auto* left = new QWidget(splitter);
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(0, 0, 6, 0);

    m_search = new QLineEdit(left);
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText("Search by name, index, class, grade, or image…");
    left_layout->addWidget(m_search);

    m_tabs = new QTabBar(left);
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
    left_layout->addWidget(m_tabs);

    m_status = new QLabel(left);
    left_layout->addWidget(m_status);

    m_item_grid = new QListWidget(left);
    m_item_grid->setViewMode(QListWidget::IconMode);
    m_item_grid->setIconSize(QSize(kIconSize, kIconSize));
    m_item_grid->setGridSize(QSize(108, 102));
    m_item_grid->setResizeMode(QListWidget::Adjust);
    m_item_grid->setMovement(QListWidget::Static);
    m_item_grid->setSpacing(6);
    m_item_grid->setUniformItemSizes(true);
    m_item_grid->setWordWrap(true);
    m_item_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    left_layout->addWidget(m_item_grid, 1);

    m_grade_scroll = new QScrollArea(left);
    m_grade_scroll->setWidgetResizable(true);
    m_grade_scroll->setFrameShape(QFrame::NoFrame);
    m_grade_host = new QWidget(m_grade_scroll);
    m_grade_layout = new QVBoxLayout(m_grade_host);
    m_grade_layout->setContentsMargins(0, 0, 0, 0);
    m_grade_layout->setSpacing(10);
    m_grade_scroll->setWidget(m_grade_host);
    m_grade_scroll->setVisible(false);
    left_layout->addWidget(m_grade_scroll, 1);

    auto* right_scroll = new QScrollArea(splitter);
    right_scroll->setWidgetResizable(true);
    right_scroll->setFrameShape(QFrame::StyledPanel);
    auto* right = new QWidget(right_scroll);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(12, 12, 12, 12);

    auto* title_row = new QHBoxLayout();
    m_preview = new QLabel(right);
    m_preview->setFixedSize(72, 72);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    title_row->addWidget(m_preview);

    m_title = new QLabel("Select an item", right);
    QFont title_font = m_title->font();
    title_font.setPointSize(title_font.pointSize() + 5);
    title_font.setBold(true);
    m_title->setFont(title_font);
    m_title->setWordWrap(true);
    title_row->addWidget(m_title, 1);
    right_layout->addLayout(title_row);

    m_details = new QTreeWidget(right);
    m_details->setColumnCount(2);
    m_details->setHeaderLabels({"Field", "Value"});
    m_details->setRootIsDecorated(true);
    m_details->setAlternatingRowColors(true);
    m_details->setWordWrap(true);
    m_details->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_details->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    right_layout->addWidget(m_details, 1);

    m_description_label = new QLabel("Description", right);
    QFont section_font = m_description_label->font();
    section_font.setBold(true);
    m_description_label->setFont(section_font);
    right_layout->addWidget(m_description_label);

    m_description = new QPlainTextEdit(right);
    m_description->setReadOnly(true);
    m_description->setMinimumHeight(130);
    right_layout->addWidget(m_description);

    right_scroll->setWidget(right);
    splitter->addWidget(left);
    splitter->addWidget(right_scroll);
    splitter->setSizes({650, 550});
    central_layout->addWidget(splitter);
    setCentralWidget(central);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { rebuild_item_grid(); });
    connect(m_tabs, &QTabBar::currentChanged, this, [this]() { rebuild_item_grid(); });
    connect(m_item_grid,
            &QListWidget::currentItemChanged,
            this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current) {
                    show_item_details(current->data(Qt::UserRole).toInt());
                }
            });
    connect(
        m_details, &QTreeWidget::itemDoubleClicked, this, [](QTreeWidgetItem* item, int column) {
            if (!item || column != 1 || item->text(1).isEmpty()) {
                return;
            }
            QApplication::clipboard()->setText(item->text(1));
        });

    rebuild_item_grid();
    if (m_item_grid->count() > 0) {
        m_item_grid->setCurrentRow(0);
    }
}

void ItemViewerWindow::rebuild_item_grid() {
    const QString search = m_search->text().trimmed();
    const Tab tab = static_cast<Tab>(m_tabs->currentIndex());
    const int selected_index = current_item_index();

    m_item_grid->setVisible(!is_class_tab(tab));
    m_grade_scroll->setVisible(is_class_tab(tab));

    if (is_class_tab(tab)) {
        rebuild_grade_groups(selected_index);
        return;
    }

    m_item_grid->clear();

    int shown = 0;
    for (int index : m_indices) {
        const openkal::client::ItemDef* def = m_catalog.find(index);
        if (!def || !passes_filter(index, search, tab)) {
            continue;
        }
        add_item_tile(m_item_grid, index);
        ++shown;
    }

    m_status->setText(QString("%1 — showing %2 of %3 items")
                          .arg(m_tabs->tabText(m_tabs->currentIndex()))
                          .arg(shown)
                          .arg(m_indices.size()));
    for (int row = 0; row < m_item_grid->count(); ++row) {
        if (m_item_grid->item(row)->data(Qt::UserRole).toInt() == selected_index) {
            m_item_grid->setCurrentRow(row);
            return;
        }
    }
    if (m_item_grid->count() > 0) {
        m_item_grid->setCurrentRow(0);
    }
}

void ItemViewerWindow::rebuild_grade_groups(int selected_index) {
    while (QLayoutItem* child = m_grade_layout->takeAt(0)) {
        if (QWidget* widget = child->widget()) {
            delete widget;
        }
        delete child;
    }
    m_grade_grids.clear();

    const QString search = m_search->text().trimmed();
    const Tab tab = static_cast<Tab>(m_tabs->currentIndex());
    std::map<int, std::vector<int>> by_grade;
    for (int index : m_indices) {
        if (!passes_filter(index, search, tab)) {
            continue;
        }
        if (const openkal::client::ItemDef* def = m_catalog.find(index)) {
            by_grade[def->grade()].push_back(index);
        }
    }

    int shown = 0;
    bool restored_selection = false;
    for (auto& [grade, indices] : by_grade) {
        std::sort(indices.begin(), indices.end(), [this](int first_index, int second_index) {
            const openkal::client::ItemDef* first = m_catalog.find(first_index);
            const openkal::client::ItemDef* second = m_catalog.find(second_index);
            if (!first || !second) {
                return first_index < second_index;
            }
            const int first_kind = gear_kind_rank(*first);
            const int second_kind = gear_kind_rank(*second);
            if (first_kind != second_kind) {
                return first_kind < second_kind;
            }
            const int name_comparison = QString::compare(
                as_text(first->name()), as_text(second->name()), Qt::CaseInsensitive);
            return name_comparison != 0 ? name_comparison < 0 : first_index < second_index;
        });

        const QString title = grade > 0 ? QString("G%1  (%2)").arg(grade).arg(indices.size())
                                        : QString("No grade  (%1)").arg(indices.size());
        auto* box = new QGroupBox(title, m_grade_host);
        auto* box_layout = new QVBoxLayout(box);
        box_layout->setContentsMargins(8, 8, 8, 8);

        auto* grid = new QListWidget(box);
        grid->setViewMode(QListWidget::IconMode);
        grid->setIconSize(QSize(kIconSize, kIconSize));
        grid->setGridSize(QSize(108, 102));
        grid->setResizeMode(QListWidget::Adjust);
        grid->setMovement(QListWidget::Static);
        grid->setSpacing(6);
        grid->setUniformItemSizes(true);
        grid->setWordWrap(true);
        grid->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(grid,
                &QListWidget::currentItemChanged,
                this,
                [this](QListWidgetItem* current, QListWidgetItem*) {
                    if (current) {
                        show_item_details(current->data(Qt::UserRole).toInt());
                    }
                });

        for (int index : indices) {
            add_item_tile(grid, index);
            ++shown;
        }

        const int rows = std::max(1, (static_cast<int>(indices.size()) + 5) / 6);
        grid->setMinimumHeight(rows * 106);
        grid->setMaximumHeight(rows * 106);
        box_layout->addWidget(grid);
        m_grade_layout->addWidget(box);
        m_grade_grids.push_back(grid);

        for (int row = 0; row < grid->count(); ++row) {
            if (grid->item(row)->data(Qt::UserRole).toInt() == selected_index) {
                grid->setCurrentRow(row);
                restored_selection = true;
                break;
            }
        }
    }
    m_grade_layout->addStretch(1);
    m_status->setText(QString("%1 — %2 items in grade groups")
                          .arg(m_tabs->tabText(m_tabs->currentIndex()))
                          .arg(shown));

    if (!restored_selection) {
        for (QListWidget* grid : m_grade_grids) {
            if (grid->count() > 0) {
                grid->setCurrentRow(0);
                break;
            }
        }
    }
}

void ItemViewerWindow::add_item_tile(QListWidget* grid, int item_index) {
    const openkal::client::ItemDef* def = m_catalog.find(item_index);
    if (!def) {
        return;
    }

    QString label = as_text(def->name());
    if (label.isEmpty()) {
        label = QString("#%1").arg(item_index);
    }
    if (label.size() > 16) {
        label = label.left(15) + QChar(0x2026);
    }

    auto* item = new QListWidgetItem(QIcon(m_icons.icon(item_index)), label);
    item->setData(Qt::UserRole, item_index);
    item->setToolTip(QString("%1\nIndex: %2\nType: %3")
                         .arg(as_text(def->name()))
                         .arg(item_index)
                         .arg(item_type(*def)));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    item->setSizeHint(QSize(108, 102));
    grid->addItem(item);
}

int ItemViewerWindow::current_item_index() const {
    if (m_item_grid->isVisible() && m_item_grid->currentItem()) {
        return m_item_grid->currentItem()->data(Qt::UserRole).toInt();
    }
    for (QListWidget* grid : m_grade_grids) {
        if (grid->currentItem()) {
            return grid->currentItem()->data(Qt::UserRole).toInt();
        }
    }
    return -1;
}

bool ItemViewerWindow::passes_filter(int item_index, const QString& filter, Tab tab) const {
    const openkal::client::ItemDef* def = m_catalog.find(item_index);
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
    const QString searchable = QString("%1 %2 %3 %4 %5 G%6")
                                   .arg(item_index)
                                   .arg(as_text(def->name()))
                                   .arg(as_text(def->clazz()))
                                   .arg(as_text(def->image()))
                                   .arg(item_type(*def))
                                   .arg(def->grade());
    return searchable.contains(filter, Qt::CaseInsensitive);
}

bool ItemViewerWindow::is_class_tab(Tab tab) {
    return tab == Tab::Knight || tab == Tab::Mage || tab == Tab::Archer || tab == Tab::Thief;
}

QTreeWidgetItem* ItemViewerWindow::add_field(const QString& name, const QString& value) {
    auto* row = new QTreeWidgetItem(m_details);
    row->setText(0, name);
    row->setText(1, value);
    return row;
}

QString ItemViewerWindow::prefix_label(int prefix_id) const {
    if (prefix_id <= 0) {
        return "None";
    }
    const openkal::client::PrefixDef* prefix = m_catalog.prefixes().find(prefix_id);
    if (!prefix || prefix->name().empty()) {
        return QString("%1 (unresolved)").arg(prefix_id);
    }
    return QString("%1 — %2").arg(prefix_id).arg(as_text(prefix->name()));
}

void ItemViewerWindow::add_prefix_details(const QString& label, int prefix_id) {
    if (prefix_id <= 0) {
        return;
    }
    auto* group = new QTreeWidgetItem(m_details);
    group->setText(0, label);
    group->setText(1, prefix_label(prefix_id));

    const openkal::client::PrefixDef* prefix = m_catalog.prefixes().find(prefix_id);
    if (!prefix) {
        return;
    }
    add_prefix_stat(group, "Level", prefix->level());
    add_prefix_stat(group, "Sell", prefix->sell());
    add_prefix_stat(group, "STR", prefix->str());
    add_prefix_stat(group, "HTH", prefix->hth());
    add_prefix_stat(group, "INT", prefix->intel());
    add_prefix_stat(group, "WIS", prefix->wis());
    add_prefix_stat(group, "DEX", prefix->dex());
    add_prefix_stat(group, "Attack", prefix->min_attack());
    add_prefix_stat(group, "Attack max", prefix->max_attack());
    add_prefix_stat(group, "Magic", prefix->min_magic());
    add_prefix_stat(group, "Magic max", prefix->max_magic());
    add_prefix_stat(group, "HP", prefix->hp());
    add_prefix_stat(group, "MP", prefix->mp());
    add_prefix_stat(group, "Defense", prefix->defense());
    add_prefix_stat(group, "Hit", prefix->hit());
    add_prefix_stat(group, "Resist fire", prefix->resist_fire());
    add_prefix_stat(group, "Resist ice", prefix->resist_ice());
    add_prefix_stat(group, "Resist lightning", prefix->resist_lightning());
    add_prefix_stat(group, "Resist curse", prefix->resist_curse());
    add_prefix_stat(group, "Resist palsy", prefix->resist_palsy());
    group->setExpanded(true);
}

QString ItemViewerWindow::yes_no(bool value) {
    return value ? "Yes" : "No";
}

void ItemViewerWindow::show_item_details(int item_index) {
    const openkal::client::ItemDef* def = m_catalog.find(item_index);
    if (!def) {
        return;
    }

    m_preview->setPixmap(
        m_icons.icon(item_index).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_title->setText(QString("%1  [#%2]").arg(as_text(def->name())).arg(item_index));
    m_details->clear();

    const auto add_if = [this](bool present, const QString& name, const QString& value) {
        if (present) {
            add_field(name, value);
        }
    };
    const auto add_number = [&add_if](const QString& name, int value) {
        add_if(value != 0, name, QString::number(value));
    };

    add_field("Index", QString::number(def->index()));
    add_if(!m_catalog.source_path(item_index).empty(),
           "File",
           as_text(m_catalog.source_path(item_index)));
    add_if(!def->name().empty(), "Name", as_text(def->name()));
    add_if(!def->image().empty(), "Image", as_text(def->image()));
    add_if(!def->clazz().empty(), "Class", as_text(def->clazz()));
    add_if(!def->class_big().empty(), "Class big", as_text(def->class_big()));
    add_if(!def->class_small().empty(), "Class small", as_text(def->class_small()));
    add_number("Action class", def->action_class());
    add_number("Action index", def->action_index());
    add_if(def->wearable(), "Wearable", yes_no(true));
    add_if(def->is_stackable(), "Stackable", yes_no(true));
    add_if(def->usable(), "Usable", yes_no(true));
    add_if(def->is_quest(), "Quest item", yes_no(true));
    add_if(def->is_transform(), "Transform item", yes_no(true));
    add_number("Range", def->range());
    add_number("Grade", def->grade());
    add_number("Rarity", def->rarity());
    add_number("Level limit", def->level_limit());
    add_if(def->class_limit() >= 0, "Class limit", class_limit_label(def->class_limit()));
    add_if(def->min_attack() != 0 || def->max_attack() != 0,
           "Attack",
           QString("%1 – %2").arg(def->min_attack()).arg(def->max_attack()));
    add_if(def->min_magic() != 0 || def->max_magic() != 0,
           "Magic",
           QString("%1 – %2").arg(def->min_magic()).arg(def->max_magic()));
    add_number("Hit", def->hit());
    add_number("Defense", def->defense());
    add_number("Dodge", def->dodge());
    add_number("Absorb", def->absorb());
    add_number("STR", def->str());
    add_number("HTH", def->hth());
    add_number("INT", def->intel());
    add_number("WIS", def->wis());
    add_number("DEX", def->dex());
    add_number("HP", def->hp());
    add_number("MP", def->mp());
    add_number("Resist fire", def->resist_fire());
    add_number("Resist ice", def->resist_ice());
    add_number("Resist lightning", def->resist_lightning());
    add_number("Resist curse", def->resist_curse());
    add_number("Resist palsy", def->resist_palsy());
    add_number("Endurance", def->endurance());
    add_number("Sell", def->sell());
    add_number("Attack speed (ms)", def->attack_speed_ms());
    add_if(def->teleport_loc() >= 0, "Teleport location", QString::number(def->teleport_loc()));
    add_number("Refresh HP", def->refresh_hp());
    add_number("Refresh MP", def->refresh_mp());
    add_number("Cooldown (ms)", def->cooltime_ms());
    add_number("Effect", def->effect());
    if (def->buff_type() >= 0) {
        add_field("Buff type", QString::number(def->buff_type()));
        add_field("Buff remaining", QString::number(def->buff_remain()));
        add_field("Buff amount", QString::number(def->buff_amount()));
    }
    add_if(def->is_transform() && def->transform_race() >= 0,
           "Transform race",
           QString::number(def->transform_race()));
    add_number("Cocoon STR", def->cocoon_ability(0));
    add_number("Cocoon HTH", def->cocoon_ability(1));
    add_number("Cocoon DEX", def->cocoon_ability(2));
    add_number("Cocoon INT", def->cocoon_ability(3));
    add_number("Cocoon WIS", def->cocoon_ability(4));
    add_if(!def->image_ex(0).empty(), "Extra image 11–20", as_text(def->image_ex(0)));
    add_if(!def->image_ex(1).empty(), "Extra image 21–30", as_text(def->image_ex(1)));
    add_if(!def->image_ex(2).empty(), "Extra image 31+", as_text(def->image_ex(2)));
    add_if(def->revival_extra_cut() >= 0,
           "Revival extra cut",
           QString::number(def->revival_extra_cut()));
    if (def->has_changeprefix()) {
        add_if(!def->changeprefix_target().empty(),
               "Changeprefix target",
               as_text(def->changeprefix_target()));
        add_field("Prefix roll A rate", QString("%1%").arg(def->changeprefix_rate0()));
        add_prefix_details("Prefix roll A", def->changeprefix_prefix0());
        add_field("Prefix roll B rate", QString("%1%").arg(def->changeprefix_rate1()));
        add_prefix_details("Prefix roll B", def->changeprefix_prefix1());
    }
    if (def->has_repair()) {
        add_field("Repair mode", QString::number(def->repair_mode()));
        add_field("Repair amount", QString::number(def->repair_amount()));
    }
    add_if(def->has_charming(), "Charming type", as_text(def->charming()));
    add_if(def->has_bof(), "Bead of Fire", yes_no(true));

    QString description = as_text(def->description());
    description.replace("#n", "\n");
    description.replace('#', '\n');
    m_description->setPlainText(description);
    m_description_label->setVisible(!description.isEmpty());
    m_description->setVisible(!description.isEmpty());
}
