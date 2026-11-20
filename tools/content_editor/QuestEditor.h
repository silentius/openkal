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

class QComboBox;
class QJsonArray;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace openkal::content_editor {

    /** Opens a searchable definition picker.
     * @param parent Owning widget.
     * @param catalog Loaded definitions.
     * @param kind Category to choose from.
     * @param current Initially selected identifier.
     * @param clear_label Optional choice for clearing a reference.
     * @return Selected identifier, an empty identifier for the clear choice, or no value on cancel.
     */
    [[nodiscard]] std::optional<QString> pick_definition(QWidget* parent,
                                                         const Catalog& catalog,
                                                         Kind kind,
                                                         const QString& current,
                                                         const QString& clear_label = {});

    /** Opens the typed form for a quest condition or action.
     * @param parent Owning widget.
     * @param catalog Optional loaded definitions for choosing referenced IDs.
     * @param condition True for a condition and false for an action.
     * @param existing Rule to edit, or an empty object to create one.
     * @return Updated rule or no value on cancel.
     */
    [[nodiscard]] std::optional<QJsonObject> edit_quest_rule(QWidget* parent,
                                                             const Catalog* catalog,
                                                             bool condition,
                                                             const QJsonObject& existing = {});

    /** Shows one quest step and its ordered cases as an editable flow graph. */
    class QuestEditor final : public QWidget {
        Q_OBJECT

      public:
        /** @param parent Owning widget. */
        explicit QuestEditor(QWidget* parent = nullptr);

        /** @param catalog Catalog used for labels and for opening related definitions. */
        void set_catalog(const Catalog* catalog);

        /** @param object Current quest step.
         * @param writable Whether the step can be changed.
         * @param reset_scroll Whether to scroll back to the start of the graph.
         */
        void load(const QJsonObject& object, bool writable, bool reset_scroll);

      signals:
        /** @param object Modified quest step. */
        void changed(const QJsonObject& object);

        /** @param path File path of a related definition to open. */
        void open_record_requested(const QString& path);

        /** Requests a new step for the current quest ID. */
        void new_step_requested();

        /** @param path Rule to open in the Fields view. */
        void field_requested(const JsonPath& path);

        /** @param path Teleport rule to edit on the map. */
        void teleport_requested(const JsonPath& path);

      private:
        void add_case(QVBoxLayout* layout, const QJsonArray& cases, int index);
        QWidget* rule_group(QWidget* parent, const JsonPath& path, bool condition);
        void edit_rule(const JsonPath& path, bool condition, int index);
        void move_entry(const JsonPath& path, int from, int to);
        void replace(const JsonPath& path, const QJsonValue& value);

        const Catalog* m_catalog = nullptr;
        QJsonObject m_object;
        bool m_writable = false;
        QComboBox* m_steps;
        QPushButton* m_new_step;
        QScrollArea* m_graph;
    };

}  // namespace openkal::content_editor
