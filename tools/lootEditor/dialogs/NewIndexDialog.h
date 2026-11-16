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

class QPushButton;
class QSpinBox;

/** Prompts for a new group or itemgroup index. */
class NewIndexDialog : public QDialog {
    Q_OBJECT

  public:
    /** Creates an index picker dialog with a free-range finder.
     * @param title Window title.
     * @param suggested Default index value.
     * @param used Occupied indices for conflict checks and the ID finder.
     * @param parent Qt parent.
     */
    NewIndexDialog(const QString& title,
                   int suggested,
                   std::set<int> used,
                   QWidget* parent = nullptr);

    /** @return Chosen index. */
    [[nodiscard]] int index() const;

  private slots:
    /** Opens the free ID range finder and applies the first free ID. */
    void open_id_finder();

    /** Accepts only when the chosen index is free. */
    void accept_if_free();

  private:
    /** Lowest allowed index. */
    static constexpr int kMinId = 1;

    /** Highest allowed index. */
    static constexpr int kMaxId = 99999;

    /** Occupied indices. */
    std::set<int> m_used;

    /** Index input. */
    QSpinBox* m_index = nullptr;

    /** Opens the free-range finder. */
    QPushButton* m_find = nullptr;
};
