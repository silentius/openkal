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

#include <QDialog>
#include <set>
#include <vector>

class QLabel;
class QListWidget;
class QListWidgetItem;

/** Lists contiguous free ID ranges and returns the first free ID of a pick. */
class IdRangeFinderDialog : public QDialog {
    Q_OBJECT

  public:
    /** One inclusive free range [first, last]. */
    struct Range {
        /** First free ID in the range. */
        int first = 0;

        /** Last free ID in the range. */
        int last = 0;
    };

    /** Creates the free-range picker.
     * @param title Window title.
     * @param used Occupied indices.
     * @param min_id Lowest allowed ID (inclusive).
     * @param max_id Highest allowed ID (inclusive).
     * @param parent Qt parent.
     */
    IdRangeFinderDialog(const QString& title,
                        const std::set<int>& used,
                        int min_id,
                        int max_id,
                        QWidget* parent = nullptr);

    /** @return First free ID of the selected range, or 0 when cancelled. */
    [[nodiscard]] int selected_id() const {
        return m_selected_id;
    }

    /** Builds free ranges from occupied IDs.
     * @param used Occupied indices.
     * @param min_id Lowest allowed ID (inclusive).
     * @param max_id Highest allowed ID (inclusive).
     * @return Contiguous free ranges sorted ascending.
     */
    [[nodiscard]] static std::vector<Range>
    compute_ranges(const std::set<int>& used, int min_id, int max_id);

  private slots:
    /** Accepts the currently highlighted range. */
    void accept_selection();

    /** Handles double-click on a range row.
     * @param item Clicked list item.
     */
    void on_item_double_clicked(QListWidgetItem* item);

  private:
    /** Formats one range for the list.
     * @param range Free range.
     * @return Display label.
     */
    [[nodiscard]] static QString range_label(const Range& range);

    /** Chosen first free ID. */
    int m_selected_id = 0;

    /** Status line. */
    QLabel* m_status = nullptr;

    /** Range list. */
    QListWidget* m_list = nullptr;
};
