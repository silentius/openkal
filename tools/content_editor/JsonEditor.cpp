/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "JsonEditor.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace openkal::content_editor {
    namespace {
        QString compact(const QJsonValue& value) {
            const auto encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
            return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
        }

        std::optional<QJsonValue>
        edit_value(QWidget* parent, const QString& title, const QJsonValue& value) {
            QDialog dialog(parent);
            dialog.setWindowTitle(title);
            dialog.resize(540, 290);
            auto* layout = new QVBoxLayout(&dialog);
            const bool text = value.isString();
            auto* description = new QLabel(text ? "Text" : "Enter a JSON value.", &dialog);
            layout->addWidget(description);
            auto* editor = new QPlainTextEdit(text ? value.toString() : compact(value), &dialog);
            if (!text) {
                editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            }
            editor->selectAll();
            layout->addWidget(editor);
            auto* buttons =
                new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            layout->addWidget(buttons);
            std::optional<QJsonValue> edited;
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
                if (text) {
                    edited = editor->toPlainText();
                } else {
                    QJsonParseError error;
                    const auto source = '[' + editor->toPlainText().toUtf8() + ']';
                    const auto parsed = QJsonDocument::fromJson(source, &error).array();
                    if (error.error != QJsonParseError::NoError || parsed.size() != 1) {
                        description->setText("Enter a single JSON value. " + error.errorString());
                        return;
                    }
                    edited = parsed.at(0);
                }
                dialog.accept();
            });
            if (dialog.exec() == QDialog::Accepted) {
                return edited;
            }
            return std::nullopt;
        }

        QJsonValue default_field(Kind kind, const JsonPath& container, const QString& key) {
            if (kind == Kind::Npc && container.isEmpty()) {
                if (key == "quest") {
                    return QJsonArray{0, 0};
                }
                if (key == "dir") {
                    return QJsonArray{0, 0};
                }
                if (key == "teleports") {
                    return QJsonArray();
                }
                if (key == "appearance" || key == "placement") {
                    return QJsonObject();
                }
            }
            return QJsonValue(QJsonValue::Null);
        }

        std::optional<QJsonValue>
        new_array_entry(QWidget* parent, Kind kind, const JsonPath& path) {
            const QString field = path.value(path.size() - 1);
            if (kind == Kind::Npc && field == "teleports") {
                return QJsonArray{0, 0, 0};
            }
            if (kind == Kind::Monster && field == "spawns") {
                return QJsonObject{{"map", 0},
                                   {"area", 0},
                                   {"max", 1},
                                   {"cycle", 30},
                                   {"rect", QJsonArray{0, 0, 0, 0}}};
            }
            if (kind == Kind::Monster && field == "itemGroups") {
                return QJsonArray{0, 1};
            }
            if (kind == Kind::Monster && field == "quests") {
                return QJsonArray{0, 0, 0, 1};
            }
            if (kind == Kind::Quest && field == "cases") {
                return QJsonObject{{"if", QJsonArray()}, {"then", QJsonArray()}};
            }
            if (kind == Kind::Quest && (field == "if" || field == "then")) {
                const QStringList choices = field == "if" ? QStringList{"level",
                                                                        "class",
                                                                        "item",
                                                                        "quest",
                                                                        "clear",
                                                                        "notclear",
                                                                        "specialty",
                                                                        "gstate",
                                                                        "contribute",
                                                                        "party"}
                                                          : QStringList{"html",
                                                                        "item",
                                                                        "exp",
                                                                        "link",
                                                                        "save",
                                                                        "clear",
                                                                        "teleport",
                                                                        "itemrand",
                                                                        "contribute",
                                                                        "supoint",
                                                                        "specialty",
                                                                        "gstate",
                                                                        "guide",
                                                                        "redistribute"};
                bool ok = false;
                const auto op = QInputDialog::getItem(
                    parent, "Quest rule", "Operation", choices, 0, false, &ok);
                if (!ok) {
                    return std::nullopt;
                }
                QJsonObject result{{"op", op}};
                if (op == "item" && field == "if") {
                    result["items"] = QJsonArray();
                } else if (op == "item") {
                    result["in"] = QJsonArray();
                    result["out"] = QJsonArray();
                } else if (op == "html") {
                    result["id"] = 0;
                } else if (op == "teleport") {
                    result["map"] = 0;
                    result["x"] = 0;
                    result["y"] = 0;
                } else if (op == "quest" || op == "link" || op == "save" || op == "clear" ||
                           op == "notclear") {
                    result["quest"] = 0;
                    if (op == "quest" || op == "link" || op == "save" ||
                        (op == "clear" && field == "then")) {
                        result["flag"] = 0;
                    }
                } else if (op == "itemrand") {
                    result["group"] = 0;
                } else if (op != "redistribute") {
                    result[op] = 0;
                    if (op == "gstate") {
                        result["value"] = 0;
                    }
                }
                return result;
            }
            if (kind == Kind::Quest && path.size() >= 5) {
                if (field == "in") {
                    return QJsonArray{0, 0, 1};
                }
                if (field == "out" || field == "items") {
                    return QJsonArray{0, 1};
                }
            }
            if (kind == Kind::Dialog && field == "lines") {
                return edit_value(parent, "Add dialog line", QString{});
            }
            if (kind == Kind::Dialog && field == "links") {
                bool ok = false;
                const auto type = QInputDialog::getItem(parent,
                                                        "Dialog option",
                                                        "Type",
                                                        {"quest", "shop", "close", "custom"},
                                                        0,
                                                        false,
                                                        &ok);
                if (!ok) {
                    return std::nullopt;
                }
                QJsonObject link{{"type", type == "custom" ? QString{} : type}, {"text", ""}};
                if (type == "quest") {
                    link["quest"] = QJsonArray{0, 0};
                }
                return link;
            }
            if (kind == Kind::Shop && (field == "items" || field == "excludeItems")) {
                return 0;
            }
            if (kind == Kind::ItemGroup && field == "entries") {
                return QJsonObject{{"group", 0}, {"chance", 100}};
            }
            if (kind == Kind::DropGroup && field == "entries") {
                bool ok = false;
                const auto type = QInputDialog::getItem(
                    parent, "Drop entry", "Type", {"item", "money"}, 0, false, &ok);
                if (!ok) {
                    return std::nullopt;
                }
                if (type == "money") {
                    return QJsonObject{{"type", type}, {"chance", 100}, {"amount", 1}};
                }
                return QJsonObject{{"type", type}, {"chance", 100}, {"item", 0}, {"count", 1}};
            }
            return edit_value(parent, "Add array entry", QJsonValue::Null);
        }
    }  // namespace

    JsonEditor::JsonEditor(QWidget* parent) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_tabs = new QTabWidget(this);
        layout->addWidget(m_tabs);
        auto* fields = new QWidget(m_tabs);
        auto* fields_layout = new QVBoxLayout(fields);
        auto* buttons = new QHBoxLayout;
        m_edit = new QPushButton("Edit value", fields);
        m_add = new QPushButton("Add", fields);
        m_remove = new QPushButton("Remove", fields);
        m_up = new QPushButton("Move up", fields);
        m_down = new QPushButton("Move down", fields);
        for (auto* button : {m_edit, m_add, m_remove, m_up, m_down}) {
            buttons->addWidget(button);
        }
        buttons->addStretch();
        fields_layout->addLayout(buttons);
        m_tree = new QTreeWidget(fields);
        m_tree->setHeaderLabels({"Field", "Value"});
        m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_tree->setUniformRowHeights(true);
        m_tree->setAlternatingRowColors(true);
        fields_layout->addWidget(m_tree);
        m_tabs->addTab(fields, "Fields");
        m_source = new QPlainTextEdit(m_tabs);
        m_source->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_source->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_tabs->addTab(m_source, "JSON");
        connect(m_tabs, &QTabWidget::currentChanged, this, &JsonEditor::tab_changed);
        connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &JsonEditor::update_actions);
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
            if (item->childCount() == 0) {
                edit();
            }
        });
        connect(m_edit, &QPushButton::clicked, this, &JsonEditor::edit);
        connect(m_add, &QPushButton::clicked, this, &JsonEditor::add);
        connect(m_remove, &QPushButton::clicked, this, &JsonEditor::remove);
        connect(m_up, &QPushButton::clicked, this, [this] { move_array_entry(-1); });
        connect(m_down, &QPushButton::clicked, this, [this] { move_array_entry(1); });
        connect(m_source, &QPlainTextEdit::textChanged, this, &JsonEditor::changed);
        load(Kind::Npc, {}, false);
    }

    void JsonEditor::load(Kind kind, const QJsonObject& object, bool writable) {
        m_kind = kind;
        m_object = object;
        m_writable = writable;
        const QSignalBlocker blocker(m_tabs);
        m_tabs->setCurrentIndex(0);
        m_source->setReadOnly(!writable);
        rebuild({}, true);
    }

    std::optional<QJsonObject> JsonEditor::object(QString* error) const {
        if (m_tabs->currentIndex() == 0) {
            return m_object;
        }
        QJsonParseError parse_error;
        const auto doc = QJsonDocument::fromJson(m_source->toPlainText().toUtf8(), &parse_error);
        if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) {
                *error = "Expected a JSON object. " + parse_error.errorString();
            }
            return std::nullopt;
        }
        return doc.object();
    }

    void JsonEditor::apply(const QJsonObject& object) {
        const auto path = selected_path();
        if (m_tabs->currentIndex() == 1) {
            const QSignalBlocker blocker(m_source);
            m_source->setPlainText(QString::fromUtf8(serialize(object)));
        }
        m_object = object;
        rebuild(path, false);
        emit changed();
    }

    JsonPath JsonEditor::selected_path() const {
        const auto* item = m_tree->currentItem();
        return item ? item->data(0, Qt::UserRole).toStringList() : JsonPath{};
    }

    void JsonEditor::select_path(const JsonPath& path) {
        if (m_tabs->currentIndex() != 0) {
            m_tabs->setCurrentIndex(0);
        }
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            auto* item = *it;
            if (item->data(0, Qt::UserRole).toStringList() == path) {
                for (auto* ancestor = item->parent(); ancestor; ancestor = ancestor->parent()) {
                    ancestor->setExpanded(true);
                }
                m_tree->setCurrentItem(item);
                m_tree->scrollToItem(item);
                return;
            }
        }
    }

    void JsonEditor::rebuild(const JsonPath& selected, bool reset) {
        const QSignalBlocker blocker(m_tree);
        QSet<JsonPath> expanded;
        if (!reset) {
            for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
                if ((*it)->isExpanded()) {
                    expanded.insert((*it)->data(0, Qt::UserRole).toStringList());
                }
            }
        }
        m_tree->clear();
        append(m_tree->invisibleRootItem(), "Definition", {}, m_object);
        auto* root = m_tree->topLevelItem(0);
        root->setExpanded(true);
        m_tree->setCurrentItem(root);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            auto* item = *it;
            const auto path = item->data(0, Qt::UserRole).toStringList();
            if (expanded.contains(path) || (reset && path.size() == 1)) {
                item->setExpanded(true);
            }
            if (path == selected) {
                m_tree->setCurrentItem(item);
            }
        }
        update_actions();
    }

    void JsonEditor::append(QTreeWidgetItem* parent,
                            const QString& key,
                            const JsonPath& path,
                            const QJsonValue& value) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, key);
        item->setData(0, Qt::UserRole, path);
        if (value.isObject()) {
            const auto object = value.toObject();
            item->setText(1, QString("%1 fields").arg(object.size()));
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                append(item, it.key(), path + JsonPath{it.key()}, it.value());
            }
        } else if (value.isArray()) {
            const auto array = value.toArray();
            item->setText(1, QString("%1 entries").arg(array.size()));
            for (int i = 0; i < array.size(); ++i) {
                const auto index = QString::number(i);
                const auto entry = array.at(i);
                const auto summary = entry.toObject().value("op").toString();
                append(item,
                       summary.isEmpty() ? '[' + index + ']' : '[' + index + "]  " + summary,
                       path + JsonPath{index},
                       entry);
            }
        } else {
            const auto content = value.isString() ? value.toString() : compact(value);
            item->setText(1, content.simplified().left(380));
            item->setToolTip(1, content.left(1400));
        }
    }

    void JsonEditor::edit() {
        const auto path = selected_path();
        if (!m_writable || path.isEmpty()) {
            return;
        }
        if (const auto value = edit_value(this, "Edit " + path.back(), value_at(m_object, path))) {
            apply(replace_at(m_object, path, *value).toObject());
        }
    }

    void JsonEditor::add() {
        if (!m_writable) {
            return;
        }
        auto path = selected_path();
        auto container = value_at(m_object, path);
        if (!container.isObject() && !container.isArray()) {
            path.removeLast();
            container = value_at(m_object, path);
        }
        if (container.isArray()) {
            auto array = container.toArray();
            if (const auto entry = new_array_entry(this, m_kind, path)) {
                if (m_kind == Kind::Monster && path == JsonPath{"stats", "quests"} &&
                    !array.isEmpty() && !array.first().isArray()) {
                    array = QJsonArray{array};
                }
                const auto index = QString::number(array.size());
                array.append(*entry);
                apply(replace_at(m_object, path, array).toObject());
                select_path(path + JsonPath{index});
            }
        } else {
            bool ok = false;
            const auto key = QInputDialog::getText(
                                 this, "Add field", "JSON field name", QLineEdit::Normal, {}, &ok)
                                 .trimmed();
            if (!ok || key.isEmpty()) {
                return;
            }
            if (container.toObject().contains(key)) {
                QMessageBox::information(this, "Add field", "That field already exists.");
                return;
            }
            auto value = default_field(m_kind, path, key);
            if (value.isNull()) {
                const auto entered = edit_value(this, "Value of " + key, value);
                if (!entered) {
                    return;
                }
                value = *entered;
            }
            path += key;
            apply(replace_at(m_object, path, value).toObject());
            select_path(path);
        }
    }

    void JsonEditor::remove() {
        auto path = selected_path();
        if (!m_writable || path.isEmpty()) {
            return;
        }
        const auto value = value_at(m_object, path);
        if ((value.isObject() || value.isArray()) &&
            QMessageBox::question(this, "Remove field", "Remove this field and all its entries?") !=
                QMessageBox::Yes) {
            return;
        }
        apply(replace_at(m_object, path, QJsonValue::Undefined).toObject());
        path.removeLast();
        select_path(path);
    }

    void JsonEditor::move_array_entry(int offset) {
        auto path = selected_path();
        if (!m_writable || path.isEmpty()) {
            return;
        }
        const auto index = path.takeLast().toInt();
        auto array = value_at(m_object, path).toArray();
        if (index < 0 || index >= array.size() || index + offset < 0 ||
            index + offset >= array.size()) {
            return;
        }
        const QJsonValue value = array.at(index);
        array.removeAt(index);
        array.insert(index + offset, value);
        apply(replace_at(m_object, path, array).toObject());
        select_path(path + JsonPath{QString::number(index + offset)});
    }

    void JsonEditor::update_actions() {
        auto path = selected_path();
        m_edit->setEnabled(m_writable && !path.isEmpty());
        m_add->setEnabled(m_writable);
        m_remove->setEnabled(m_writable && !path.isEmpty());
        const auto index = path.isEmpty() ? -1 : path.takeLast().toInt();
        const auto parent = value_at(m_object, path);
        m_up->setEnabled(m_writable && parent.isArray() && index > 0);
        m_down->setEnabled(m_writable && parent.isArray() && index >= 0 &&
                           index + 1 < parent.toArray().size());
    }

    void JsonEditor::tab_changed(int index) {
        if (index == 1) {
            const QSignalBlocker blocker(m_source);
            m_source->setPlainText(QString::fromUtf8(serialize(m_object)));
            return;
        }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(m_source->toPlainText().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            const QSignalBlocker blocker(m_tabs);
            m_tabs->setCurrentIndex(1);
            QMessageBox::warning(
                this, "JSON source", "Expected a JSON object. " + error.errorString());
            return;
        }
        m_object = doc.object();
        rebuild({}, false);
    }

}  // namespace openkal::content_editor
