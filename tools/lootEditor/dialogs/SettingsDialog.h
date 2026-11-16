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

#include "model/LootDataStore.h"

#include <QDialog>

class QLineEdit;

/** Edits loot editor data root paths. */
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /** Creates a settings dialog.
     * @param paths Current paths.
     * @param parent Qt parent.
     */
    SettingsDialog(const LootEditorPaths& paths, QWidget* parent = nullptr);

    /** @return Updated paths when accepted. */
    [[nodiscard]] LootEditorPaths paths() const;

  private:
    /** Current paths, retaining command-line-only overrides. */
    LootEditorPaths m_paths;

    /** Root for the externally maintained loot configuration. */
    QLineEdit* m_loot_config = nullptr;

    /** Client data root field. */
    QLineEdit* m_data = nullptr;
};
