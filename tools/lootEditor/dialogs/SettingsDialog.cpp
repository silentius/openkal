/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

    QHBoxLayout* dir_row(QWidget* parent, QLineEdit*& field, const QString& initial) {
        field = new QLineEdit(initial, parent);
        auto* layout = new QHBoxLayout();
        layout->addWidget(field);
        auto* browse = new QPushButton("…", parent);
        browse->setFixedWidth(28);
        QObject::connect(browse, &QPushButton::clicked, parent, [field, parent]() {
            const QString dir =
                QFileDialog::getExistingDirectory(parent, "Select Directory", field->text());
            if (!dir.isEmpty()) {
                field->setText(dir);
            }
        });
        layout->addWidget(browse);
        return layout;
    }

}  // namespace

SettingsDialog::SettingsDialog(const LootEditorPaths& paths, QWidget* parent)
    : QDialog(parent), m_paths(paths) {
    setWindowTitle("Loot Editor Settings");
    resize(640, 180);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    form->addRow("Loot config:",
                 dir_row(this, m_loot_config, QString::fromStdString(paths.loot_config_root)));
    form->addRow("Client data:", dir_row(this, m_data, QString::fromStdString(paths.data_root)));

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

LootEditorPaths SettingsDialog::paths() const {
    LootEditorPaths paths = m_paths;
    paths.loot_config_root = m_loot_config->text().toStdString();
    paths.data_root = m_data->text().toStdString();
    return paths;
}
