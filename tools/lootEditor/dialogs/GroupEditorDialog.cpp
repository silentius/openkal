/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/GroupEditorDialog.h"

#include "dialogs/ItemPickerDialog.h"
#include "dialogs/PrefixPickerDialog.h"
#include "widgets/ItemIconOverlay.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <unordered_set>

namespace {

    enum Col { Type = 0, Item, Prefix, Count, Amount, Chance, CountCols };

    /** Table cell showing an item icon, name, and picker button. */
    class ItemRefWidget : public QWidget {
      public:
        ItemRefWidget(LootDataStore* store,
                      ItemIconCache* icons,
                      int item_index,
                      std::function<std::unordered_set<int>()> occupied_items,
                      QWidget* parent)
            : QWidget(parent), m_store(store), m_icons(icons), m_index(item_index),
              m_occupied_items(std::move(occupied_items)) {
            auto* layout = new QHBoxLayout(this);
            layout->setContentsMargins(2, 2, 2, 2);
            layout->setSpacing(4);

            m_icon = new QLabel(this);
            m_icon->setFixedSize(32, 32);
            m_icon->setScaledContents(true);
            layout->addWidget(m_icon);

            m_name = new QLabel(this);
            m_name->setMinimumWidth(120);
            m_name->setWordWrap(true);
            layout->addWidget(m_name, 1);

            auto* pick = new QPushButton("…", this);
            pick->setFixedWidth(28);
            pick->setToolTip("Choose item from icon grid");
            layout->addWidget(pick);

            connect(pick, &QPushButton::clicked, this, [this]() { open_picker(); });

            setProperty("itemIndex", m_index);
            refresh();
        }

        [[nodiscard]] int index() const {
            return m_index;
        }

        void set_index(int item_index) {
            m_money_mode = false;
            m_index = item_index;
            setProperty("itemIndex", m_index);
            refresh();
        }

        void set_money_display() {
            m_money_mode = true;
            setProperty("itemIndex", 0);
            refresh();
        }

      private:
        void open_picker() {
            std::unordered_set<int> occupied;
            if (m_occupied_items) {
                occupied = m_occupied_items();
            }
            ItemPickerDialog dlg(m_store, m_icons, m_index, std::move(occupied), window());
            if (dlg.exec() != QDialog::Accepted) {
                return;
            }
            if (dlg.selected_index() > 0) {
                set_index(dlg.selected_index());
            }
        }

        void refresh() {
            if (m_money_mode) {
                m_icon->setPixmap(m_icons->money_icon());
                m_name->setText("Money");
                m_icon->setToolTip("Zamogeon drop");
                m_name->setToolTip("Zamogeon drop");
                return;
            }
            if (m_index <= 0) {
                m_icon->setPixmap(m_icons->icon(0));
                m_name->setText("(pick item)");
                m_icon->setToolTip({});
                m_name->setToolTip({});
                return;
            }
            QPixmap px = m_icons->icon(m_index);
            const QStringList overlay_lines = m_store->changeprefix_overlay_lines(m_index, 8);
            if (!overlay_lines.isEmpty()) {
                px = apply_item_text_overlay(px, overlay_lines, 32);
            }
            m_icon->setPixmap(px);
            QString name = m_store->item_name(m_index);
            if (!overlay_lines.isEmpty()) {
                name += "\n" + overlay_lines.join(" · ");
            }
            m_name->setText(name);
            const QString tip = m_store->item_tooltip(m_index);
            m_icon->setToolTip(tip);
            m_name->setToolTip(tip);
        }

        LootDataStore* m_store = nullptr;
        ItemIconCache* m_icons = nullptr;
        int m_index = 0;
        bool m_money_mode = false;
        std::function<std::unordered_set<int>()> m_occupied_items;
        QLabel* m_icon = nullptr;
        QLabel* m_name = nullptr;
    };

    /** Table cell for prefix selection with a searchable picker. */
    class PrefixRefWidget : public QWidget {
      public:
        PrefixRefWidget(LootDataStore* store, int prefix_id, QWidget* parent)
            : QWidget(parent), m_store(store), m_prefix(prefix_id) {
            auto* layout = new QHBoxLayout(this);
            layout->setContentsMargins(2, 2, 2, 2);
            layout->setSpacing(4);

            m_label = new QLabel(this);
            m_label->setMinimumWidth(100);
            layout->addWidget(m_label, 1);

            auto* pick = new QPushButton("…", this);
            pick->setFixedWidth(28);
            pick->setToolTip("Choose prefix / enchant");
            layout->addWidget(pick);

            connect(pick, &QPushButton::clicked, this, [this]() { open_picker(); });

            refresh();
        }

        [[nodiscard]] int prefix() const {
            return m_prefix;
        }

        void set_prefix(int prefix_id) {
            m_prefix = prefix_id;
            refresh();
        }

      private:
        void open_picker() {
            PrefixPickerDialog dlg(m_store, m_prefix, window());
            if (dlg.exec() != QDialog::Accepted) {
                return;
            }
            set_prefix(dlg.selected_prefix());
        }

        void refresh() {
            if (m_prefix <= 0) {
                m_label->setText("—");
                m_label->setToolTip("No prefix");
                return;
            }
            m_label->setText(m_store->prefix_name(m_prefix));
            m_label->setToolTip(QString("Prefix #%1").arg(m_prefix));
        }

        LootDataStore* m_store = nullptr;
        int m_prefix = 0;
        QLabel* m_label = nullptr;
    };

    [[nodiscard]] ItemRefWidget* item_widget(QTableWidget* table, int row) {
        return dynamic_cast<ItemRefWidget*>(table->cellWidget(row, Item));
    }

    [[nodiscard]] PrefixRefWidget* prefix_widget(QTableWidget* table, int row) {
        return dynamic_cast<PrefixRefWidget*>(table->cellWidget(row, Prefix));
    }

    [[nodiscard]] QComboBox* type_widget(QTableWidget* table, int row) {
        return qobject_cast<QComboBox*>(table->cellWidget(row, Type));
    }

}  // namespace

GroupEditorDialog::GroupEditorDialog(openkal::server::GroupDef def,
                                     LootDataStore* store,
                                     QWidget* parent)
    : QDialog(parent), m_def(std::move(def)), m_store(store) {
    m_icons = std::make_unique<ItemIconCache>(store->items());

    setWindowTitle(QString("Edit Group #%1").arg(m_def.index));
    resize(880, 460);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_name = new QLineEdit(QString::fromStdString(m_def.name), this);
    form->addRow("Name:", m_name);
    layout->addLayout(form);

    m_table = new QTableWidget(0, CountCols, this);
    m_table->setHorizontalHeaderLabels({"Type", "Item", "Prefix", "Count", "Amount", "Chance %"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(Item, QHeaderView::Stretch);
    m_table->setColumnWidth(Prefix, 140);
    m_table->verticalHeader()->setDefaultSectionSize(40);
    layout->addWidget(m_table);
    populate_table();

    auto* row_btns = new QHBoxLayout();
    auto* add_item = new QPushButton("Add Item…", this);
    add_item->setToolTip("Pick an item from the icon grid");
    auto* add_money = new QPushButton("Add Money", this);
    auto* remove = new QPushButton("Remove Selected", this);
    connect(add_item, &QPushButton::clicked, this, [this]() { add_item_row(0); });
    connect(add_money, &QPushButton::clicked, this, &GroupEditorDialog::add_money_row);
    connect(remove, &QPushButton::clicked, this, &GroupEditorDialog::remove_selected);
    row_btns->addWidget(add_item);
    row_btns->addWidget(add_money);
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

void GroupEditorDialog::populate_table() {
    m_table->setRowCount(0);
    for (const auto& e : m_def.entries) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* type = new QComboBox();
        type->addItems({"item", "money"});
        type->setCurrentText(e.kind == openkal::server::GroupEntry::Kind::Money ? "money" : "item");
        m_table->setCellWidget(row, Type, type);
        connect(type, &QComboBox::currentTextChanged, this, [this, row](const QString&) {
            sync_row_type(row);
        });

        auto* item = new ItemRefWidget(
            m_store,
            m_icons.get(),
            e.item_index,
            [this]() { return occupied_item_indices(); },
            this);
        m_table->setCellWidget(row, Item, item);

        auto* prefix = new PrefixRefWidget(m_store, e.prefix, this);
        m_table->setCellWidget(row, Prefix, prefix);

        auto* count = new QSpinBox();
        count->setRange(1, 9999);
        count->setValue(e.count > 0 ? e.count : 1);
        m_table->setCellWidget(row, Count, count);

        auto* amount = new QSpinBox();
        amount->setRange(0, 999999999);
        amount->setValue(e.amount);
        m_table->setCellWidget(row, Amount, amount);

        auto* chance = new QDoubleSpinBox();
        chance->setRange(0.01, 100.0);
        chance->setDecimals(2);
        chance->setValue(e.chance);
        m_table->setCellWidget(row, Chance, chance);

        sync_row_type(row);
    }
}

void GroupEditorDialog::apply_table() {
    m_def.name = m_name->text().toStdString();
    m_def.entries.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* type = type_widget(m_table, row);
        const auto* item = item_widget(m_table, row);
        const auto* prefix = prefix_widget(m_table, row);
        const auto* count = qobject_cast<QSpinBox*>(m_table->cellWidget(row, Count));
        const auto* amount = qobject_cast<QSpinBox*>(m_table->cellWidget(row, Amount));
        const auto* chance = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(row, Chance));
        if (!type || !item || !prefix || !count || !amount || !chance) {
            continue;
        }

        openkal::server::GroupEntry e;
        e.chance = static_cast<float>(chance->value());
        if (type->currentText() == "money") {
            e.kind = openkal::server::GroupEntry::Kind::Money;
            e.amount = amount->value();
            if (e.amount <= 0 || e.chance <= 0) {
                continue;
            }
        } else {
            e.kind = openkal::server::GroupEntry::Kind::Item;
            e.item_index = item->index();
            e.prefix = prefix->prefix();
            e.count = count->value();
            if (e.item_index <= 0 || e.chance <= 0) {
                continue;
            }
        }
        m_def.entries.push_back(e);
    }
}

void GroupEditorDialog::add_item_row(int item_index) {
    std::vector<int> to_add;
    if (item_index > 0) {
        to_add.push_back(item_index);
    } else {
        ItemPickerDialog picker(m_store, m_icons.get(), 0, occupied_item_indices(), this);
        if (picker.exec() != QDialog::Accepted) {
            return;
        }
        to_add = picker.selected_indices();
        if (to_add.empty() && picker.selected_index() > 0) {
            to_add.push_back(picker.selected_index());
        }
    }

    int last_row = -1;
    for (int picked : to_add) {
        if (picked <= 0) {
            continue;
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* type = new QComboBox();
        type->addItems({"item", "money"});
        type->setCurrentText("item");
        m_table->setCellWidget(row, Type, type);
        connect(type, &QComboBox::currentTextChanged, this, [this, row](const QString&) {
            sync_row_type(row);
        });

        auto* item = new ItemRefWidget(
            m_store, m_icons.get(), picked, [this]() { return occupied_item_indices(); }, this);
        m_table->setCellWidget(row, Item, item);

        auto* prefix = new PrefixRefWidget(m_store, 0, this);
        m_table->setCellWidget(row, Prefix, prefix);

        auto* count = new QSpinBox();
        count->setRange(1, 9999);
        count->setValue(1);
        m_table->setCellWidget(row, Count, count);

        auto* amount = new QSpinBox();
        amount->setRange(0, 999999999);
        m_table->setCellWidget(row, Amount, amount);

        auto* chance = new QDoubleSpinBox();
        chance->setRange(0.01, 100.0);
        chance->setDecimals(2);
        chance->setValue(1.0);
        m_table->setCellWidget(row, Chance, chance);

        sync_row_type(row);
        last_row = row;
    }

    if (last_row >= 0) {
        m_table->selectRow(last_row);
    }
}

void GroupEditorDialog::add_money_row() {
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* type = new QComboBox();
    type->addItems({"item", "money"});
    type->setCurrentText("money");
    m_table->setCellWidget(row, Type, type);
    connect(type, &QComboBox::currentTextChanged, this, [this, row](const QString&) {
        sync_row_type(row);
    });

    auto* item = new ItemRefWidget(
        m_store, m_icons.get(), 0, [this]() { return occupied_item_indices(); }, this);
    m_table->setCellWidget(row, Item, item);

    auto* prefix = new PrefixRefWidget(m_store, 0, this);
    m_table->setCellWidget(row, Prefix, prefix);

    auto* count = new QSpinBox();
    count->setRange(1, 9999);
    count->setValue(1);
    m_table->setCellWidget(row, Count, count);

    auto* amount = new QSpinBox();
    amount->setRange(0, 999999999);
    amount->setValue(100);
    m_table->setCellWidget(row, Amount, amount);

    auto* chance = new QDoubleSpinBox();
    chance->setRange(0.01, 100.0);
    chance->setDecimals(2);
    chance->setValue(1.0);
    m_table->setCellWidget(row, Chance, chance);

    sync_row_type(row);
    m_table->selectRow(row);
}

void GroupEditorDialog::sync_row_type(int row) {
    const QComboBox* type = type_widget(m_table, row);
    ItemRefWidget* item = item_widget(m_table, row);
    PrefixRefWidget* prefix = prefix_widget(m_table, row);
    QWidget* count = m_table->cellWidget(row, Count);
    QWidget* amount = m_table->cellWidget(row, Amount);
    if (!type || !item || !prefix || !count || !amount) {
        return;
    }

    const bool is_money = type->currentText() == "money";
    if (is_money) {
        item->set_money_display();
    }
    item->setEnabled(!is_money);
    prefix->setEnabled(!is_money);
    count->setEnabled(!is_money);
    amount->setEnabled(is_money);
}

std::unordered_set<int> GroupEditorDialog::occupied_item_indices() const {
    std::unordered_set<int> occupied;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QWidget* cell = m_table->cellWidget(row, Item);
        if (!cell) {
            continue;
        }
        // Prefer Qt property — avoids fragile dynamic_cast across anonymous types.
        const QVariant prop = cell->property("itemIndex");
        if (prop.isValid()) {
            const int idx = prop.toInt();
            if (idx > 0) {
                occupied.insert(idx);
            }
            continue;
        }
        if (const ItemRefWidget* item = item_widget(m_table, row)) {
            if (item->index() > 0) {
                occupied.insert(item->index());
            }
        }
    }
    // Fallback to the last applied definition if the table has no readable widgets yet.
    if (occupied.empty()) {
        for (const auto& e : m_def.entries) {
            if (e.kind == openkal::server::GroupEntry::Kind::Item && e.item_index > 0) {
                occupied.insert(e.item_index);
            }
        }
    }
    return occupied;
}

void GroupEditorDialog::remove_selected() {
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
