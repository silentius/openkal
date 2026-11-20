/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "QuestEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <utility>

namespace openkal::content_editor {
    namespace {
        constexpr int kIntMax = std::numeric_limits<int>::max();

        struct Field {
            const char* key;
            const char* caption;
            int maximum = kIntMax;
            std::optional<Kind> target = {};
        };

        struct Rule {
            QString op;
            QString caption;
            QList<Field> fields;
        };

        const QList<Rule>& rules(bool condition) {
            const Field quest{"quest", "Quest ID", 65535, Kind::Quest};
            const Field flag{"flag", "Flag", 255};
            const Field player_class{"class", "Class ID"};
            const Field specialty{"specialty", "Specialty bit", 30};
            const Field state{"gstate", "State bit", 31};
            const Field value{"value", "Value (0 or 1)", 1};
            static const QList<Rule> conditions{
                {"level", "Minimum level", {{"level", "Level"}}},
                {"class", "Player class", {player_class}},
                {"specialty", "Player specialty", {player_class, specialty}},
                {"item", "Required items", {}},
                {"quest", "Quest flag equals", {quest, flag}},
                {"clear", "Quest completed", {quest}},
                {"notclear", "Quest incomplete", {quest}},
                {"gstate", "State bit equals", {state, value}},
            };
            static const QList<Rule> actions{
                {"html", "Show dialog", {{"id", "Dialog ID", kIntMax, Kind::Dialog}}},
                {"link", "Run quest step", {quest, flag}},
                {"save", "Save quest progress", {quest, flag}},
                {"clear", "Complete quest", {quest, flag}},
                {"exp", "Award experience", {{"exp", "Experience"}}},
                {"item", "Give or take items", {}},
                {"itemrand",
                 "Roll item reward",
                 {{"group", "Item group ID", kIntMax, Kind::ItemGroup}}},
                {"supoint", "Award skill points", {{"supoint", "Skill points"}}},
                {"contribute", "Award contribution", {{"contribute", "Contribution"}}},
                {"specialty", "Unlock specialty", {specialty}},
                {"gstate", "Change state bit", {state, value}},
                {"teleport", "Teleport", {{"map", "Map ID"}, {"x", "X"}, {"y", "Y"}}},
                {"guide", "Show guide", {{"guide", "Guide ID"}}},
                {"redistribute", "Redistribute stats", {}},
            };
            return condition ? conditions : actions;
        }

        const Rule* rule_type(bool condition, const QJsonObject& rule) {
            const auto& choices = rules(condition);
            const auto op = rule.value("op").toString();
            const auto it = std::find_if(
                choices.cbegin(), choices.cend(), [&](const Rule& type) { return type.op == op; });
            return it != choices.cend() && !rule.value("raw").isArray() ? &*it : nullptr;
        }

        QString item_name(const Catalog* catalog, int id) {
            if (const auto* record =
                    catalog ? catalog->find(Kind::Item, QString::number(id)) : nullptr) {
                return title(*record);
            }
            return QString("Item %1").arg(id);
        }

        QString item_summary(const Catalog* catalog, const QJsonValue& value, bool reward) {
            const auto row = value.toArray();
            if (row.size() < (reward ? 3 : 2)) {
                return "Invalid item row";
            }
            auto summary = QString("%1 × %2")
                               .arg(row.at(reward ? 2 : 1).toInt())
                               .arg(item_name(catalog, row.at(0).toInt()));
            if (reward && row.at(1).toInt() != 0) {
                summary += QString(" · Prefix %1").arg(row.at(1).toInt());
            }
            if (reward && row.size() > 3 && row.at(3).toInt() == 1) {
                summary += " · Account-bound";
            }
            return summary;
        }

        QString
        rule_description(const Catalog* catalog, const QJsonObject& rule, const Rule& type) {
            QStringList values;
            if (type.op == "item") {
                for (const auto* field : {"items", "in", "out"}) {
                    const QString key(field);
                    const auto rows = rule.value(key).toArray();
                    for (const auto& row : rows) {
                        const auto prefix = key == "in" ? "Give " : key == "out" ? "Take " : "";
                        values.append(prefix + item_summary(catalog, row, key == "in"));
                    }
                }
            } else {
                for (const auto& field : type.fields) {
                    values.append(
                        QString("%1 %2").arg(field.caption).arg(rule.value(field.key).toInt()));
                }
            }
            return values.join(" · ");
        }

        QSpinBox*
        number(QWidget* parent, const QString& name, int value, int maximum, int minimum = 0) {
            auto* spin = new QSpinBox(parent);
            spin->setAccessibleName(name);
            spin->setRange(std::min(minimum, value), std::max(maximum, value));
            spin->setValue(value);
            return spin;
        }

        QToolButton* menu_button(QWidget* parent, const QString& name) {
            auto* button = new QToolButton(parent);
            button->setText("⋯");
            button->setAccessibleName(name);
            button->setToolTip(name);
            button->setPopupMode(QToolButton::InstantPopup);
            button->setMenu(new QMenu(button));
            return button;
        }

        QDialogButtonBox* dialog_buttons(QDialog& dialog) {
            auto* buttons =
                new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            return buttons;
        }

        void add_reference_picker(QFormLayout& form,
                                  QWidget* parent,
                                  const Catalog* catalog,
                                  Kind kind,
                                  const QString& caption,
                                  QSpinBox* id,
                                  QSpinBox* flag = nullptr) {
            if (!catalog) {
                form.addRow(caption, id);
                return;
            }
            auto* row = new QWidget(parent);
            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(id, 1);
            auto* browse = new QPushButton("Browse…", row);
            browse->setAccessibleName("Choose " + caption);
            layout->addWidget(browse);
            QObject::connect(browse, &QPushButton::clicked, row, [=] {
                auto current = QString::number(id->value());
                if (flag) {
                    current += ':' + QString::number(flag->value());
                }
                if (const auto chosen = pick_definition(row, *catalog, kind, current)) {
                    const auto parts = chosen->split(':');
                    id->setValue(parts.front().toInt());
                    if (flag) {
                        flag->setValue(parts.value(1).toInt());
                    }
                }
            });
            form.addRow(caption, row);
        }

        std::optional<QJsonArray>
        edit_item(QWidget* parent, const Catalog* catalog, QJsonArray row, bool reward) {
            QDialog dialog(parent);
            dialog.setWindowTitle(reward ? "Item reward" : "Item and quantity");
            auto* form = new QFormLayout(&dialog);
            auto* item =
                number(&dialog, "Item ID", row.isEmpty() ? 1 : row.at(0).toInt(), 65535, 1);
            const int count_index = reward ? 2 : 1;
            auto* count = number(&dialog,
                                 "Quantity",
                                 row.size() > count_index ? row.at(count_index).toInt() : 1,
                                 kIntMax,
                                 1);
            add_reference_picker(*form, &dialog, catalog, Kind::Item, "Item ID", item);
            QSpinBox* prefix = nullptr;
            QCheckBox* bound = nullptr;
            if (reward) {
                prefix =
                    number(&dialog, "Prefix ID", row.size() > 1 ? row.at(1).toInt() : 0, kIntMax);
                add_reference_picker(*form, &dialog, catalog, Kind::Prefix, "Prefix ID", prefix);
                bound = new QCheckBox("Account-bound", &dialog);
                bound->setChecked(row.size() > 3 && row.at(3).toInt() == 1);
            }
            form->addRow("Quantity", count);
            if (bound) {
                form->addRow({}, bound);
            }
            form->addRow(dialog_buttons(dialog));
            if (dialog.exec() != QDialog::Accepted) {
                return std::nullopt;
            }

            const auto set = [&](int at, int value) {
                while (row.size() <= at) {
                    row.append(0);
                }
                if (row.at(at).toInt() != value) {
                    row.replace(at, value);
                }
            };
            set(0, item->value());
            if (reward) {
                set(1, prefix->value());
                if (bound->isChecked() || (row.size() > 3 && row.at(3).toInt() == 1)) {
                    set(3, bound->isChecked() ? 1 : 0);
                }
            }
            set(count_index, count->value());
            return row;
        }

        class ItemRows final : public QWidget {
          public:
            ItemRows(QWidget* parent, const Catalog* catalog, QJsonArray rows, bool reward)
                : QWidget(parent), m_catalog(catalog), m_rows(std::move(rows)), m_reward(reward) {
                auto* layout = new QVBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                m_list = new QListWidget(this);
                m_list->setMinimumHeight(72);
                m_list->setMaximumHeight(106);
                layout->addWidget(m_list);
                for (const auto& row : m_rows) {
                    m_list->addItem(item_summary(m_catalog, row, m_reward));
                }
                auto* controls = new QHBoxLayout;
                layout->addLayout(controls);
                auto* add = new QPushButton("Add item", this);
                if (m_reward) {
                    add->setAccessibleName("Add reward item");
                }
                auto* edit = new QPushButton("Edit", this);
                auto* remove = new QPushButton("Remove", this);
                controls->addWidget(add);
                controls->addWidget(edit);
                controls->addWidget(remove);
                controls->addStretch();
                edit->setEnabled(false);
                remove->setEnabled(false);
                connect(m_list, &QListWidget::currentRowChanged, this, [=](int row) {
                    edit->setEnabled(row >= 0);
                    remove->setEnabled(row >= 0);
                });
                connect(add, &QPushButton::clicked, this, [this] { edit_row(-1); });
                connect(
                    edit, &QPushButton::clicked, this, [this] { edit_row(m_list->currentRow()); });
                connect(m_list,
                        &QListWidget::itemDoubleClicked,
                        this,
                        [this](QListWidgetItem* item) { edit_row(m_list->row(item)); });
                connect(remove, &QPushButton::clicked, this, [this] {
                    const int row = m_list->currentRow();
                    m_rows.removeAt(row);
                    delete m_list->takeItem(row);
                });
            }

            const QJsonArray& rows() const {
                return m_rows;
            }

          private:
            void edit_row(int row) {
                auto edited = edit_item(
                    this, m_catalog, row < 0 ? QJsonArray{} : m_rows.at(row).toArray(), m_reward);
                if (!edited) {
                    return;
                }
                if (row < 0) {
                    row = static_cast<int>(m_rows.size());
                    m_rows.append(*edited);
                    m_list->addItem(item_summary(m_catalog, *edited, m_reward));
                } else {
                    m_rows.replace(row, *edited);
                    m_list->item(row)->setText(item_summary(m_catalog, *edited, m_reward));
                }
                m_list->setCurrentRow(row);
            }

            const Catalog* m_catalog;
            QJsonArray m_rows;
            bool m_reward;
            QListWidget* m_list;
        };
    }  // namespace

    std::optional<QString> pick_definition(QWidget* parent,
                                           const Catalog& catalog,
                                           Kind kind,
                                           const QString& current,
                                           const QString& clear_label) {
        QDialog dialog(parent);
        dialog.setWindowTitle("Choose from " + label(kind));
        dialog.resize(520, 470);
        auto* layout = new QVBoxLayout(&dialog);
        auto* filter = new QLineEdit(&dialog);
        filter->setPlaceholderText("Filter by name or ID");
        filter->setClearButtonEnabled(true);
        layout->addWidget(filter);
        auto* list = new QListWidget(&dialog);
        list->setAlternatingRowColors(true);
        layout->addWidget(list);
        const auto quest_names =
            kind == Kind::Quest ? catalog.quest_names() : QHash<int, QString>{};
        for (const auto& record : catalog.records()) {
            if (record.kind == kind) {
                const auto name = quest_names.value(record.object.value("index").toInt());
                auto* item = new QListWidgetItem(title(record, name), list);
                item->setData(Qt::UserRole, identity(kind, record.object));
            }
        }
        list->sortItems();
        if (!clear_label.isEmpty()) {
            list->insertItem(0, clear_label);
        }
        auto* buttons = dialog_buttons(dialog);
        layout->addWidget(buttons);
        const auto update_ok = [&] {
            const auto* selected = list->currentItem();
            buttons->button(QDialogButtonBox::Ok)->setEnabled(selected && !selected->isHidden());
        };
        QObject::connect(list, &QListWidget::currentItemChanged, &dialog, update_ok);
        QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
        QObject::connect(filter, &QLineEdit::textChanged, &dialog, [&] {
            const auto words = filter->text().simplified().split(' ', Qt::SkipEmptyParts);
            for (int i = 0; i < list->count(); ++i) {
                auto* item = list->item(i);
                item->setHidden(
                    !std::all_of(words.cbegin(), words.cend(), [&](const QString& word) {
                        return item->text().contains(word, Qt::CaseInsensitive);
                    }));
            }
            if (!list->currentItem() || list->currentItem()->isHidden()) {
                for (int i = 0; i < list->count(); ++i) {
                    if (!list->item(i)->isHidden()) {
                        list->setCurrentRow(i);
                        break;
                    }
                }
            }
            update_ok();
        });
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == current) {
                list->setCurrentRow(i);
                break;
            }
        }
        update_ok();
        filter->setFocus();
        if (dialog.exec() == QDialog::Accepted && list->currentItem() &&
            !list->currentItem()->isHidden()) {
            return list->currentItem()->data(Qt::UserRole).toString();
        }
        return std::nullopt;
    }

    std::optional<QJsonObject> edit_quest_rule(QWidget* parent,
                                               const Catalog* catalog,
                                               bool condition,
                                               const QJsonObject& existing) {
        const auto* type = rule_type(condition, existing);
        if (!existing.isEmpty() && !type) {
            return std::nullopt;
        }

        QDialog dialog(parent);
        dialog.setWindowTitle(condition ? "Quest condition" : "Quest action");
        dialog.setMinimumWidth(430);
        auto* layout = new QVBoxLayout(&dialog);
        auto* operation = new QComboBox(&dialog);
        operation->setAccessibleName("Operation");
        const auto& choices = rules(condition);
        for (const auto& choice : choices) {
            operation->addItem(choice.caption, choice.op);
        }
        if (type) {
            operation->setCurrentIndex(operation->findData(type->op));
            operation->setEnabled(false);
        }
        layout->addWidget(operation);

        QWidget* pane = nullptr;
        QList<QPair<QString, QSpinBox*>> numbers;
        QList<QPair<QString, ItemRows*>> items;
        auto* buttons = dialog_buttons(dialog);
        layout->addWidget(buttons);
        const auto rebuild = [&](int index) {
            delete pane;
            pane = new QWidget(&dialog);
            layout->insertWidget(1, pane);
            auto* form = new QFormLayout(pane);
            numbers.clear();
            items.clear();
            const auto& choice = choices.at(index);
            if (choice.op == "item") {
                const auto add_items = [&](const QString& key,
                                           const QString& caption,
                                           bool reward) {
                    auto* rows = new ItemRows(pane, catalog, existing.value(key).toArray(), reward);
                    rows->setEnabled(!existing.contains(key) || existing.value(key).isArray());
                    form->addRow(caption, rows);
                    items.append({key, rows});
                };
                if (condition) {
                    add_items("items", "Required items", false);
                } else {
                    add_items("out", "Take from player", false);
                    add_items("in", "Give to player", true);
                }
            }
            for (const auto& field : choice.fields) {
                numbers.append(
                    {field.key,
                     number(
                         pane, field.caption, existing.value(field.key).toInt(), field.maximum)});
            }
            for (int i = 0; i < choice.fields.size(); ++i) {
                const auto& field = choice.fields.at(i);
                auto* spin = numbers.at(i).second;
                if (field.target) {
                    auto* flag = field.target == Kind::Quest && numbers.size() > 1
                                     ? numbers.at(1).second
                                     : nullptr;
                    add_reference_picker(
                        *form, pane, catalog, *field.target, field.caption, spin, flag);
                } else {
                    form->addRow(field.caption, spin);
                }
            }
            dialog.adjustSize();
        };
        rebuild(operation->currentIndex());
        QObject::connect(operation, &QComboBox::currentIndexChanged, &dialog, rebuild);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        auto edited = existing;
        edited["op"] = operation->currentData().toString();
        for (const auto& [key, spin] : numbers) {
            if (existing.isEmpty() || existing.value(key).toInt() != spin->value()) {
                edited[key] = spin->value();
            }
        }
        for (const auto& [key, rows] : items) {
            if (existing.isEmpty() || rows->rows() != existing.value(key).toArray()) {
                edited[key] = rows->rows();
            }
        }
        return edited;
    }

    QuestEditor::QuestEditor(QWidget* parent) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        auto* navigation = new QHBoxLayout;
        m_steps = new QComboBox(this);
        m_steps->setAccessibleName("Quest steps");
        m_steps->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_steps->setMinimumContentsLength(13);
        m_steps->setMaximumWidth(430);
        m_new_step = new QPushButton("New step…", this);
        navigation->addWidget(m_steps, 1);
        navigation->addWidget(m_new_step);
        navigation->addStretch();
        layout->addLayout(navigation);
        m_graph = new QScrollArea(this);
        m_graph->setObjectName("QuestGraph");
        m_graph->setWidgetResizable(true);
        layout->addWidget(m_graph, 1);
        connect(m_steps, &QComboBox::activated, this, [this](int index) {
            const auto path = m_steps->itemData(index).toString();
            if (!path.isEmpty()) {
                const int previous = m_steps->findData(QString{});
                m_steps->setCurrentIndex(previous);
                emit open_record_requested(path);
            }
        });
        connect(m_new_step, &QPushButton::clicked, this, &QuestEditor::new_step_requested);
    }

    void QuestEditor::set_catalog(const Catalog* catalog) {
        m_catalog = catalog;
    }

    void QuestEditor::load(const QJsonObject& object, bool writable, bool reset_scroll) {
        m_object = object;
        m_writable = writable;
        const int quest = object.value("index").toInt();
        const int flag = object.value("flag").toInt();
        m_new_step->setEnabled(m_catalog && !m_catalog->root().isEmpty());
        m_steps->clear();

        const auto step_label = [&](const QJsonObject& step) {
            const QString suffix = step.value("linked").toBool() ? " · linked only" : "";
            return QString("Quest %1 · Step %2%3")
                .arg(quest)
                .arg(step.value("flag").toInt())
                .arg(suffix);
        };
        if (m_catalog) {
            QList<const Record*> steps;
            for (const auto& candidate : m_catalog->records()) {
                if (candidate.kind == Kind::Quest &&
                    candidate.object.value("index").toInt() == quest &&
                    candidate.object.value("flag").toInt() != flag) {
                    steps.append(&candidate);
                }
            }
            std::sort(steps.begin(), steps.end(), [](const Record* left, const Record* right) {
                return left->object.value("flag").toInt() < right->object.value("flag").toInt();
            });
            for (const auto* step : steps) {
                const int index = m_steps->count();
                m_steps->addItem(step_label(step->object), step->path);
                m_steps->setItemData(index, step->object.value("flag").toInt(), Qt::UserRole + 1);
            }
        }
        int current = 0;
        while (current < m_steps->count() &&
               m_steps->itemData(current, Qt::UserRole + 1).toInt() < flag) {
            ++current;
        }
        m_steps->insertItem(current, step_label(object), QString{});
        m_steps->setCurrentIndex(current);

        const int scroll = reset_scroll ? 0 : m_graph->verticalScrollBar()->value();
        if (auto* previous = m_graph->takeWidget()) {
            previous->hide();
            previous->deleteLater();
        }
        auto* canvas = new QWidget;
        auto* layout = new QVBoxLayout(canvas);
        layout->setAlignment(Qt::AlignTop);
        canvas->setStyleSheet(
            "QFrame#QuestCard { background: palette(base); border: 1px solid palette(mid); "
            "border-radius: 8px; }"
            "QFrame#QuestRule { background: palette(alternate-base); border-radius: 5px; }");
        auto* entry = new QCheckBox("Linked only (run through a link action)", canvas);
        entry->setChecked(object.value("linked").toBool());
        entry->setEnabled(writable);
        connect(entry, &QCheckBox::toggled, this, [this](bool checked) {
            replace({"linked"}, checked);
        });
        auto* header = new QHBoxLayout;
        header->addWidget(entry);
        header->addStretch();
        auto* add = new QPushButton("Add case", canvas);
        const auto cases_value = object.value("cases");
        const auto cases = cases_value.toArray();
        add->setEnabled(writable && (cases_value.isUndefined() || cases_value.isArray()));
        header->addWidget(add);
        layout->addLayout(header);
        connect(add, &QPushButton::clicked, this, [this, cases] {
            auto updated = cases;
            updated.append(QJsonObject{{"if", QJsonArray{}}, {"then", QJsonArray{}}});
            replace({"cases"}, updated);
        });
        auto* flow = new QLabel(
            "The first case whose conditions all pass runs its actions in order.", canvas);
        flow->setWordWrap(true);
        layout->addWidget(flow);
        for (int i = 0; i < cases.size(); ++i) {
            add_case(layout, cases, i);
        }
        if (cases.isEmpty()) {
            layout->addWidget(new QLabel(cases_value.isUndefined() || cases_value.isArray()
                                             ? "No cases"
                                             : "Open Fields to inspect the cases value.",
                                         canvas));
        }
        m_graph->setWidget(canvas);
        m_graph->verticalScrollBar()->setValue(scroll);
    }

    void QuestEditor::add_case(QVBoxLayout* layout, const QJsonArray& cases, int index) {
        auto* canvas = layout->parentWidget();
        const JsonPath path{"cases", QString::number(index)};
        auto* row = new QWidget(canvas);
        auto* columns = new QHBoxLayout(row);
        columns->setContentsMargins(0, 0, 0, 0);
        if (cases.at(index).isObject()) {
            columns->addWidget(rule_group(row, path + JsonPath{"if"}, true), 1, Qt::AlignTop);
            auto* match = new QLabel("Match →", row);
            match->setAlignment(Qt::AlignCenter);
            match->setContentsMargins(0, 12, 0, 0);
            columns->addWidget(match, 0, Qt::AlignTop);
            columns->addWidget(rule_group(row, path + JsonPath{"then"}, false), 1, Qt::AlignTop);
        } else {
            auto* inspect =
                new QPushButton(QString("Inspect case %1 in Fields").arg(index + 1), row);
            connect(
                inspect, &QPushButton::clicked, this, [this, path] { emit field_requested(path); });
            columns->addWidget(inspect, 1);
        }
        if (m_writable) {
            auto* controls = menu_button(row, QString("Case %1 options").arg(index + 1));
            controls->menu()
                ->addAction("Move case up",
                            this,
                            [this, index] { move_entry({"cases"}, index, index - 1); })
                ->setEnabled(index > 0);
            controls->menu()
                ->addAction("Move case down",
                            this,
                            [this, index] { move_entry({"cases"}, index, index + 1); })
                ->setEnabled(index + 1 < cases.size());
            controls->menu()->addAction(
                "Remove case", this, [this, path] { replace(path, QJsonValue::Undefined); });
            columns->addWidget(controls, 0, Qt::AlignTop);
        }
        layout->addWidget(row);
        const auto conditions = cases.at(index).toObject().value("if").toArray();
        if (!cases.at(index).isObject() || !conditions.isEmpty()) {
            auto* otherwise =
                new QLabel(index + 1 == cases.size() ? "↓  Otherwise: no matching case"
                                                     : "↓  Otherwise: try next case",
                           canvas);
            otherwise->setContentsMargins(16, 0, 0, 0);
            layout->addWidget(otherwise);
        } else if (index + 1 < cases.size()) {
            auto* unreachable =
                new QLabel("This case always matches. Later cases are skipped.", canvas);
            unreachable->setWordWrap(true);
            layout->addWidget(unreachable);
        }
    }

    QWidget* QuestEditor::rule_group(QWidget* parent, const JsonPath& path, bool condition) {
        auto* card = new QFrame(parent);
        card->setObjectName("QuestCard");
        auto* layout = new QVBoxLayout(card);
        const auto caption = condition ? QString("Case %1 · Conditions").arg(path.at(1).toInt() + 1)
                                       : "Actions & rewards";
        auto* heading = new QLabel(caption, card);
        auto font = heading->font();
        font.setBold(true);
        heading->setFont(font);
        layout->addWidget(heading);
        const auto value = value_at(m_object, path);
        const auto entries = value.toArray();
        if (entries.isEmpty()) {
            auto* hint = new QLabel(value.isUndefined() || value.isArray()
                                        ? (condition ? "Always matches" : "No actions")
                                        : "Open Fields to inspect this value.",
                                    card);
            hint->setWordWrap(true);
            layout->addWidget(hint);
        }
        for (int i = 0; i < entries.size(); ++i) {
            const auto rule = entries.at(i).toObject();
            const auto* type = rule_type(condition, rule);
            const auto op = rule.value("op").toString();
            const auto entry_path = path + JsonPath{QString::number(i)};
            auto* node = new QFrame(card);
            node->setObjectName("QuestRule");
            auto* row = new QHBoxLayout(node);
            row->setContentsMargins(8, 5, 8, 5);
            auto* info = new QVBoxLayout;
            info->setSpacing(2);
            row->addLayout(info, 1);
            auto* text =
                new QLabel(type ? type->caption
                                : (op.isEmpty() ? QString("Unknown rule") : "Unsupported · " + op),
                           node);
            text->setTextFormat(Qt::PlainText);
            text->setWordWrap(true);
            info->addWidget(text);
            if (m_writable && type) {
                auto* edit = new QPushButton("Edit", node);
                edit->setAccessibleName("Edit " + type->caption);
                connect(edit, &QPushButton::clicked, this, [this, path, condition, i] {
                    edit_rule(path, condition, i);
                });
                row->addWidget(edit, 0, Qt::AlignTop);
            }
            auto* menu = menu_button(node, text->text() + " options");
            menu->menu()->addAction(
                "Open in Fields", this, [this, entry_path] { emit field_requested(entry_path); });
            if (m_writable) {
                if (!condition && type && op == "teleport") {
                    menu->menu()->addAction("Set target on map", this, [this, entry_path] {
                        emit teleport_requested(entry_path);
                    });
                }
                menu->menu()
                    ->addAction("Move up", this, [this, path, i] { move_entry(path, i, i - 1); })
                    ->setEnabled(i > 0);
                menu->menu()
                    ->addAction("Move down", this, [this, path, i] { move_entry(path, i, i + 1); })
                    ->setEnabled(i + 1 < entries.size());
                menu->menu()->addAction("Remove", this, [this, entry_path] {
                    replace(entry_path, QJsonValue::Undefined);
                });
            }
            row->addWidget(menu, 0, Qt::AlignTop);
            layout->addWidget(node);
            if (type) {
                const auto description = rule_description(m_catalog, rule, *type);
                if (!description.isEmpty()) {
                    auto* summary = new QLabel(description, node);
                    summary->setTextFormat(Qt::PlainText);
                    summary->setWordWrap(true);
                    info->addWidget(summary);
                }
                if (m_catalog && !condition && (op == "html" || op == "link")) {
                    const auto id = op == "html" ? QString::number(rule.value("id").toInt())
                                                 : QString("%1:%2")
                                                       .arg(rule.value("quest").toInt())
                                                       .arg(rule.value("flag").toInt());
                    if (const auto* target =
                            m_catalog->find(op == "html" ? Kind::Dialog : Kind::Quest, id)) {
                        auto* open = new QPushButton(
                            op == "html" ? "Open dialog " + id : "Open step " + id, node);
                        open->setToolTip(title(*target));
                        connect(
                            open, &QPushButton::clicked, this, [this, target_path = target->path] {
                                emit open_record_requested(target_path);
                            });
                        info->addWidget(open, 0, Qt::AlignLeft);
                    } else {
                        auto* missing = new QLabel("Target definition is not loaded.", node);
                        missing->setWordWrap(true);
                        info->addWidget(missing);
                    }
                }
            }
        }
        layout->addStretch();
        if (m_writable && (value.isUndefined() || value.isArray())) {
            auto* add = new QPushButton(condition ? "Add condition…" : "Add action…", card);
            connect(add, &QPushButton::clicked, this, [this, path, condition] {
                edit_rule(path, condition, -1);
            });
            layout->addWidget(add, 0, Qt::AlignLeft);
        }
        return card;
    }

    void QuestEditor::edit_rule(const JsonPath& path, bool condition, int index) {
        auto entries = value_at(m_object, path).toArray();
        if (const auto edited =
                edit_quest_rule(this,
                                m_catalog,
                                condition,
                                index < 0 ? QJsonObject{} : entries.at(index).toObject())) {
            if (index < 0) {
                entries.append(*edited);
            } else {
                entries.replace(index, *edited);
            }
            replace(path, entries);
        }
    }

    void QuestEditor::move_entry(const JsonPath& path, int from, int to) {
        auto entries = value_at(m_object, path).toArray();
        if (from >= 0 && from < entries.size() && to >= 0 && to < entries.size()) {
            const auto entry = entries.takeAt(from);
            entries.insert(to, entry);
            replace(path, entries);
        }
    }

    void QuestEditor::replace(const JsonPath& path, const QJsonValue& value) {
        const auto next = replace_at(m_object, path, value).toObject();
        if (m_writable && next != m_object) {
            emit changed(next);
        }
    }

}  // namespace openkal::content_editor
