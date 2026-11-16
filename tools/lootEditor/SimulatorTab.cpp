/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SimulatorTab.h"

#include "openkal/server/world/DropRoller.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace {

    constexpr int kSimBagSlots = 72;

    struct SimSlot {
        int index = 0;
        int prefix = 0;
        int count = 0;
    };

    [[nodiscard]] bool sim_try_insert(std::array<SimSlot, kSimBagSlots>& bag,
                                      const openkal::client::ItemDb& catalog,
                                      int item_index,
                                      int prefix,
                                      int count,
                                      std::int64_t& money) {
        if (item_index <= 0 || count <= 0) {
            return false;
        }
        if (item_index == openkal::server::kMoneyItemIndex) {
            money += count;
            return true;
        }
        const openkal::client::ItemDef* def = catalog.find(item_index);
        const bool stackable = def && def->is_stackable();
        if (stackable) {
            for (auto& slot : bag) {
                if (slot.index == item_index && slot.prefix == prefix && slot.count > 0) {
                    slot.count += count;
                    return true;
                }
            }
            for (auto& slot : bag) {
                if (slot.index == 0 || slot.count <= 0) {
                    slot.index = item_index;
                    slot.prefix = prefix;
                    slot.count = count;
                    return true;
                }
            }
            return false;
        }
        int free = 0;
        for (const auto& slot : bag) {
            if (slot.index == 0 || slot.count <= 0) {
                ++free;
            }
        }
        if (free < count) {
            return false;
        }
        int remaining = count;
        for (auto& slot : bag) {
            if (remaining <= 0) {
                break;
            }
            if (slot.index == 0 || slot.count <= 0) {
                slot.index = item_index;
                slot.prefix = prefix;
                slot.count = 1;
                --remaining;
            }
        }
        return true;
    }

    [[nodiscard]] int sim_slots_used(const std::array<SimSlot, kSimBagSlots>& bag) {
        int used = 0;
        for (const auto& slot : bag) {
            if (slot.index != 0 && slot.count > 0) {
                ++used;
            }
        }
        return used;
    }

}  // namespace

SimulatorTab::SimulatorTab(LootDataStore* store, QWidget* parent)
    : QWidget(parent), m_store(store) {
    m_icons = std::make_unique<ItemIconCache>(store->items());

    auto* layout = new QVBoxLayout(this);

    auto* controls = new QHBoxLayout();
    controls->addWidget(new QLabel("Simulate kills:", this));
    m_kills = new QSpinBox(this);
    m_kills->setRange(1, 1000000);
    m_kills->setValue(100);
    controls->addWidget(m_kills);
    m_run = new QPushButton("Run Simulation", this);
    connect(m_run, &QPushButton::clicked, this, &SimulatorTab::run_simulation);
    controls->addWidget(m_run);
    controls->addStretch();
    layout->addLayout(controls);

    m_summary = new QLabel("Select a monster and run simulation.", this);
    layout->addWidget(m_summary);

    m_results = new QTableWidget(0, 5, this);
    m_results->setHorizontalHeaderLabels({"Icon", "Item", "Prefix", "Total", "Drop rate"});
    m_results->horizontalHeader()->setStretchLastSection(true);
    m_results->setIconSize(QSize(32, 32));
    layout->addWidget(m_results);

    connect(store, &LootDataStore::selectionChanged, this, [this]() {
        m_results->setRowCount(0);
        m_summary->setText("Selection changed — press Run Simulation.");
    });
}

void SimulatorTab::run_simulation() {
    m_icons = std::make_unique<ItemIconCache>(m_store->items());

    const MonsterRecord* monster = m_store->monsters().find(m_store->selected_monster());
    if (!monster) {
        m_summary->setText("No monster selected.");
        return;
    }

    const int kills = m_kills->value();
    std::array<SimSlot, kSimBagSlots> bag{};
    std::int64_t money = 0;
    int kills_done = 0;
    bool bag_full = false;

    std::map<std::pair<int, int>, DropRow> totals;

    for (int kill = 1; kill <= kills; ++kill) {
        const auto drops = m_store->roller().roll_monster_drops(
            openkal::server::DropRoller::effective_monster_kill_item_groups(monster->item_groups,
                                                                            monster->race),
            0,
            0);
        std::set<std::pair<int, int>> seen_this_kill;
        for (const auto& drop : drops) {
            const int count = std::max(1, drop.count);
            if (!sim_try_insert(
                    bag, m_store->items(), drop.item_index, drop.prefix, count, money)) {
                bag_full = true;
                kills_done = kill - 1;
                break;
            }
            if (drop.item_index != openkal::server::kMoneyItemIndex) {
                const auto key = std::make_pair(drop.item_index, drop.prefix);
                auto& row = totals[key];
                row.item_index = drop.item_index;
                row.prefix = drop.prefix;
                row.total_count += count;
                seen_this_kill.insert(key);
            }
        }
        if (bag_full) {
            break;
        }
        kills_done = kill;
        for (const auto& key : seen_this_kill) {
            totals[key].kill_hits += 1;
        }
    }

    m_results->setRowCount(0);
    std::vector<DropRow> ranked;
    ranked.reserve(totals.size());
    for (auto& [_, row] : totals) {
        ranked.push_back(row);
    }
    std::sort(ranked.begin(), ranked.end(), [](const DropRow& a, const DropRow& b) {
        return a.total_count > b.total_count;
    });

    for (const DropRow& row : ranked) {
        const int r = m_results->rowCount();
        m_results->insertRow(r);

        auto* icon_item = new QTableWidgetItem();
        icon_item->setIcon(QIcon(m_icons->icon(row.item_index)));
        m_results->setItem(r, 0, icon_item);
        m_results->setItem(r, 1, new QTableWidgetItem(m_store->item_name(row.item_index)));
        m_results->setItem(r, 2, new QTableWidgetItem(m_store->prefix_name(row.prefix)));
        m_results->setItem(r, 3, new QTableWidgetItem(QString::number(row.total_count)));
        const double rate =
            kills_done > 0 ? (100.0 * static_cast<double>(row.kill_hits) / kills_done) : 0.0;
        m_results->setItem(r, 4, new QTableWidgetItem(QString("%1%").arg(rate, 0, 'f', 2)));
    }

    QString note = bag_full ? QString("Bag full after %1 kills.").arg(kills_done)
                            : QString("Completed all %1 kills.").arg(kills_done);
    m_summary->setText(QString("%1 — money +%2 — slots %3/%4 — %5")
                           .arg(QString::fromStdString(monster->name))
                           .arg(money)
                           .arg(sim_slots_used(bag))
                           .arg(kSimBagSlots)
                           .arg(note));
}
