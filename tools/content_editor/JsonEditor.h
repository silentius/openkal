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

#include "Catalog.h"

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace openkal::content_editor {

    /** Structured editing of all JSON fields, with a source view for bulk changes. */
    class JsonEditor : public QWidget {
        Q_OBJECT

      public:
        /** Constructs a definition editor.
         * @param parent Owning widget.
         */
        explicit JsonEditor(QWidget* parent = nullptr);

        /** Replaces the editor contents for a selected definition.
         * @param kind Definition category.
         * @param object Loaded JSON object.
         * @param writable True when edits are permitted.
         */
        void load(Kind kind, const QJsonObject& object, bool writable);

        /** @param error Receives a parse error if the source is invalid.
         * @return Current object, or no value when the source is invalid.
         */
        [[nodiscard]] std::optional<QJsonObject> object(QString* error = nullptr) const;

        /** Applies a change created by a map or reference picker.
         * @param object Updated definition.
         */
        void apply(const QJsonObject& object);

        /** @return The selected field's nested path. */
        [[nodiscard]] JsonPath selected_path() const;

        /** Focuses a field in the structured view.
         * @param path Nested keys and indices.
         */
        void select_path(const JsonPath& path);

      signals:
        /** The current definition has changed. */
        void changed();

      private:
        void rebuild(const JsonPath& selected, bool reset);
        void append(QTreeWidgetItem* parent,
                    const QString& key,
                    const JsonPath& path,
                    const QJsonValue& value);
        void edit();
        void add();
        void remove();
        void move_array_entry(int offset);
        void update_actions();
        void tab_changed(int index);

        Kind m_kind = Kind::Npc;
        QJsonObject m_object;
        bool m_writable = false;
        QTabWidget* m_tabs;
        QTreeWidget* m_tree;
        QPlainTextEdit* m_source;
        QPushButton* m_edit;
        QPushButton* m_add;
        QPushButton* m_remove;
        QPushButton* m_up;
        QPushButton* m_down;
    };

}  // namespace openkal::content_editor
