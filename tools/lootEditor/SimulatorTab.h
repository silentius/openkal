/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "model/ItemIconCache.h"
#include "model/LootDataStore.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/** Tab 3: monster kill drop simulator. */
class SimulatorTab : public QWidget {
    Q_OBJECT

  public:
    /** Creates the simulator tab.
     * @param store Shared session data.
     * @param parent Qt parent.
     */
    explicit SimulatorTab(LootDataStore* store, QWidget* parent = nullptr);

  public slots:
    /** Runs the simulation for the selected monster. */
    void run_simulation();

  private:
    /** Aggregated drop row for the results table. */
    struct DropRow {
        int item_index = 0;
        int prefix = 0;
        int total_count = 0;
        int kill_hits = 0;
    };

    /** Session store. */
    LootDataStore* m_store = nullptr;

    /** Item icon cache. */
    std::unique_ptr<ItemIconCache> m_icons;

    /** Kill count input. */
    QSpinBox* m_kills = nullptr;

    /** Run button. */
    QPushButton* m_run = nullptr;

    /** Summary label. */
    QLabel* m_summary = nullptr;

    /** Results table. */
    QTableWidget* m_results = nullptr;
};
