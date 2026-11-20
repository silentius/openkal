/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "Catalog.h"
#include "ContentWindow.h"
#include "JsonEditor.h"
#include "MapView.h"
#include "PatchExport.h"
#include "PlacementPreview.h"
#include "ContentAudit.h"
#include "QuestEditor.h"
#include "test_util.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineF>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>

#include <cmath>
#include <cstring>

namespace {
    using namespace openkal::content_editor;
    using openkal::test::expect;

    bool write(const QString& path, const QByteArray& data) {
        QFile file(path);
        return QDir().mkpath(QFileInfo(path).absolutePath()) && file.open(QIODevice::WriteOnly) &&
               file.write(data) == data.size();
    }

    QByteArray read(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

    bool make_dungeon(const std::string& root) {
        namespace test = openkal::test;
        std::vector<std::uint8_t> bytes(60, 0);
        bytes[0] = 2;
        bytes[1] = 1;
        const char names[] = "room.map\0floor.gb\0";
        const std::uint32_t size = sizeof(names);
        std::memcpy(bytes.data() + 12, &size, sizeof(size));
        test::append_bytes(bytes, names, size);
        test::append_pod(bytes, std::uint32_t{9});
        test::append_pod(bytes, std::uint32_t{0});
        for (float value : {30.0f, 100.0f, 30.0f, 0.0f, 0.0f, 0.0f}) {
            test::append_pod(bytes, value);
        }
        for (int i = 0; i < 3; ++i) {
            test::append_pod(bytes, std::uint32_t{0});
        }
        return test::write_bytes(root + "/room.map", bytes) &&
               test::write_cls_only_gb_v11(root + "/floor.gb", 30);
    }

    bool action(QWidget& window, const QString& name) {
        for (auto* candidate : window.findChildren<QAction*>()) {
            if (candidate->text() == name && candidate->isEnabled()) {
                candidate->trigger();
                return true;
            }
        }
        return false;
    }

    QPushButton* button(QWidget& window, const QString& text) {
        for (auto* candidate : window.findChildren<QPushButton*>()) {
            if (candidate->text() == text && candidate->isVisible() && candidate->isEnabled()) {
                return candidate;
            }
        }
        return nullptr;
    }

    void mouse(MapView& map,
               QEvent::Type event,
               QPoint server,
               Qt::MouseButton button,
               Qt::MouseButtons buttons) {
        auto* target = map.viewport();
        const auto viewport = map.mapFromScene(server.x(), -server.y());
        QMouseEvent mouse(
            event, viewport, target->mapToGlobal(viewport), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(target, &mouse);
    }

    void test_catalog(const QString& root, const QString& base) {
        expect(write(base + "/items/100.json", R"({"index":100,"name":"Fallback"})"),
               "write fallback item");
        expect(write(base + "/dialogs/000001.json", R"({"id":1,"caption":"Fallback dialog"})") &&
                   write(root + "/dialogs/000001.json", R"({"id":1,"caption":"Editable dialog"})"),
               "write a dialog present in both roots");
        expect(
            write(root + "/groups/0005.json", R"({"index":5,"entries":[{"chance":25,"item":42}]})"),
            "write drop group");
        expect(write(root + "/itemgroups/0006.json",
                     R"({"index":6,"entries":[{"group":5,"chance":100}]})"),
               "write item group");

        expect(write(base + "/npcs/9.json",
                     R"({"index":9,"html":99,"map":1,"shape":11,"xy":[64,96,1000],"dir":[66,96]})"),
               "write a relative NPC anchor");
        Catalog catalog;
        expect(catalog.load(root, QDir::current().relativeFilePath(base)).isEmpty(),
               "load both catalogs without duplicating overridden files");
        const auto* dialog = catalog.find(Kind::Dialog, "1");
        expect(dialog && dialog->writable && dialog->object.value("caption") == "Editable dialog",
               "the editable definition takes precedence");
        const auto relative = catalog.resolve_npc(QJsonObject{
            {"index", 10},
            {"placement", QJsonObject{{"beside_dialog", 99}, {"offset", QJsonArray{10, 20, 30}}}},
        });
        expect(relative && relative->value("shape").toInt() == 11 &&
                   relative->value("map").toInt() == 1 &&
                   relative->value("xy").toArray() == QJsonArray{74, 116, 1030} &&
                   relative->value("dir").toArray() == QJsonArray{76, 116},
               "resolve the preview and map marker of an NPC relative to its anchor");
        const auto* fallback = catalog.find(Kind::Item, "100");
        expect(fallback && !fallback->writable, "fallback record is read-only");
        auto item = catalog.draft(Kind::Item,
                                  QJsonObject{
                                      {"index", 42},
                                      {"name", "Test item"},
                                      {"text", "numbers [1,2]"},
                                      {"code", QJsonArray{4, 8, 0, 0}},
                                      {"unknown", QJsonObject{{"retained", true}}},
                                  },
                                  "general/etc");
        expect(catalog.save(item).isEmpty(), "save a new item");
        const auto bytes = read(item.path);
        expect(bytes.contains("[4, 8, 0, 0]"), "format numeric array on one line");
        expect(QJsonDocument::fromJson(bytes).object() == item.object,
               "leave strings and unknown fields intact");
        item.original = bytes;
        expect(!catalog.remove(item).isEmpty(), "protect referenced items on deletion");

        const Record monster{
            Kind::Monster,
            {},
            QJsonObject{
                {"index", 17},
                {"stats", QJsonObject{{"itemGroups", QJsonArray{QJsonArray{6, 2}}}}},
            },
            {},
            true};
        const auto used = references(monster);
        expect(used.size() == 1 && used.front().id == "6" && used.front().kind == Kind::ItemGroup,
               "resolve nested drop-list references");

        auto quest = catalog.draft(Kind::Quest, initial_object(Kind::Quest, "55", 1));
        quest.object["cases"] =
            QJsonArray{QJsonObject{{"then",
                                    QJsonArray{
                                        QJsonObject{{"op", "html"}, {"id", 900}},
                                    }}}};
        expect(catalog.unresolved(quest).size() == 1, "detect newly missing dialog");
        expect(catalog.save(quest).isEmpty(), "save quest step");
        expect(catalog.find(Kind::Quest, "55:1") != nullptr, "find exact quest step");

        expect(write(item.path, R"({"index":42,"name":"Changed by another process"})"),
               "simulate a concurrent external change");
        item.object["name"] = "Would overwrite";
        expect(!catalog.save(item).isEmpty() && read(item.path).contains("another process"),
               "reject stale writes");
        expect(value_at(replace_at(item.object, {"unknown", "retained"}, QJsonValue::Undefined),
                        {"unknown", "retained"})
                   .isUndefined(),
               "remove nested field without dropping its parent");
    }

    void test_profiles(const QString& root, const QString& client) {
        const auto external = root + "/external/config";
        const auto configured = root + "/profile-config";
        const auto profile = root + "/openkal.profile";
        const auto relative = QDir(OPENKAL_RESOURCE_ROOT).relativeFilePath(configured);
        const QByteArray assets = "data_root=" + client.toUtf8() +
                                  "/data\nmap_root=" + client.toUtf8() +
                                  "/map\nmaps_subdir=Terrain\n";
        expect(write(external + "/monsters/12.json", R"({"index":12,"stats":{"exp":100}})") &&
                   write(configured + "/monsters/19.json", R"({"index":19,"stats":{"exp":50}})") &&
                   write(profile, assets + "configuration_root=" + relative.toUtf8() + '\n'),
               "write loose external catalogs and a profile with a relative configuration root");

        Workspace workspace;
        workspace.external_config = external;
        expect(ContentWindow::read_profile(workspace, profile).isEmpty() &&
                   workspace.config == configured && workspace.data == client + "/data" &&
                   workspace.map == client + "/map" && workspace.maps_subdirectory == "Terrain",
               "resolve the profile configuration and client asset paths");

        expect(write(profile, assets + "configuration_root=\n"),
               "write profile with the runtime's default configuration setting");
        expect(
            ContentWindow::read_profile(workspace, profile).isEmpty() &&
                workspace.config == external && workspace.archived_config.isEmpty(),
            "use the configured external sources for a profile with an empty configuration root");
        Catalog catalog;
        expect(catalog.load(workspace.config).isEmpty(),
               "open the external definitions from the profile");
        const auto* monster = catalog.find(Kind::Monster, "12");
        expect(monster && monster->writable, "profile opens external definitions for editing");
        if (monster) {
            auto updated = *monster;
            updated.object["stats"] = QJsonObject{{"exp", 250}};
            expect(catalog.save(updated).isEmpty() &&
                       read(external + "/monsters/12.json").contains("\"exp\": 250") &&
                       read(configured + "/monsters/19.json").contains("\"exp\":50"),
                   "save changes back to the active profile's external catalog");
        }
        expect(ContentWindow::read_profile(workspace, profile, configured).isEmpty() &&
                   workspace.config == configured,
               "the explicit configuration override takes precedence");
    }

    void test_content_audit(const QString& root, const QString& base) {
        const auto source = root + "/dialogs/5.json";
        const auto orphan = root + "/dialogs/6.json";
        expect(
            write(root + "/npcs/1.json", R"({"index":1,"xy":[10,20,30],"quest":[11,1]})") &&
                write(
                    root + "/quests/11_1.json",
                    R"({"index":11,"flag":1,"cases":[{"if":[{"op":"clear","quest":33}],"then":[{"op":"link","quest":11,"flag":2},{"op":"html","id":4},{"op":"itemrand","group":102},{"op":"save","quest":55,"flag":1}]},{"if":[],"then":[{"op":"html","id":997,"raw":[]}]}]})") &&
                write(root + "/quests/11_2.json", R"({"index":11,"flag":2})") &&
                write(root + "/quests/11_3.json", R"({"index":11,"flag":3})") &&
                write(
                    root + "/quests/12_1.json",
                    R"({"index":12,"flag":1,"cases":[{"then":[{"op":"save","quest":12,"flag":1}]}]})") &&
                write(root + "/quests/22_1.json", R"({"index":22,"flag":1})") &&
                write(root + "/quests/33_1.json", R"({"index":33,"flag":1})") &&
                write(root + "/quests/44_1.json", R"({"index":44,"flag":1})") &&
                write(root + "/tasks/22.json",
                      R"({"key":22,"flags":[{"key":1,"npcIndex":99,"item":[0,702,2]}]})") &&
                write(root + "/tasks/55.json", R"({"key":55})") &&
                write(root + "/tasks/66.json", R"({"key":66,"npcIndex":1})") &&
                write(
                    root + "/monsters/1.json",
                    R"({"index":1,"stats":{"quests":[44,1,104,1],"itemGroups":[[101,2],[105,0],[199,1]]}})") &&
                write(base + "/dialogs/4.json", R"({"id":4,"links":[{"type":"0005"}]})") &&
                write(source,
                      R"({"id":5,"links":[{"type":"quest","quest":[11,2]},{"type":"999"}]})") &&
                write(orphan, R"({"id":6,"caption":"Unused"})") &&
                write(root + "/dialogs/7.json", R"({"id":7,"links":[{"type":"8"}]})") &&
                write(root + "/dialogs/8.json", R"({"id":8,"links":[{"type":"7"}]})"),
            "write a reachable chain, orphan definitions, assignments and an unreachable cycle");
        expect(
            write(
                root + "/itemgroups/101.json",
                R"({"index":101,"entries":[{"group":301,"chance":100},{"group":308,"chance":0},{"group":399,"chance":1}]})") &&
                write(root + "/itemgroups/102.json",
                      R"({"index":102,"entries":[{"group":302,"chance":100}]})") &&
                write(root + "/itemgroups/103.json",
                      R"({"index":103,"entries":[{"group":303,"chance":100}]})") &&
                write(root + "/itemgroups/104.json",
                      R"({"index":104,"entries":[{"group":304,"chance":100}]})") &&
                write(root + "/itemgroups/105.json",
                      R"({"index":105,"entries":[{"group":305,"chance":100}]})") &&
                write(base + "/itemgroups/1000000.json",
                      R"({"index":1000000,"entries":[{"group":306,"chance":100}]})") &&
                write(root + "/groups/307.json",
                      R"({"index":307,"entries":[{"type":"item","item":700,"chance":10}]})") &&
                write(root + "/items/42.json", R"({"index":42})") &&
                write(
                    root + "/server/shops/1.json",
                    R"({"schema":"openkal.shop/v1","npcIndex":1,"items":[42,699],"excludeItems":[703]})") &&
                write(root + "/server/shops/2.json",
                      R"({"schema":"openkal.shop/v1","npcIndex":2,"items":[42]})"),
            "write used, disconnected and broken loot, journal and shop references");
        for (const int id : {301, 302, 303, 304, 305, 306, 308}) {
            expect(write(root + QString("/groups/%1.json").arg(id),
                         serialize(QJsonObject{{"index", id}})),
                   "write a drop group for the audit");
        }
        Catalog catalog;
        expect(catalog.load(root, base).isEmpty(),
               "load the audit fixture and its fallback catalog");
        const auto audit = audit_content_references(catalog);
        const QStringList orphan_paths{"/quests/12_1.json",
                                       "/dialogs/6.json",
                                       "/itemgroups/103.json",
                                       "/itemgroups/105.json",
                                       "/groups/307.json",
                                       "/groups/308.json",
                                       "/tasks/66.json",
                                       "/server/shops/2.json"};
        expect(audit.orphan_reasons.size() == orphan_paths.size() &&
                   std::all_of(orphan_paths.cbegin(),
                               orphan_paths.cend(),
                               [&](const auto& path) {
                                   return audit.orphan_reasons.contains(root + path);
                               }),
               "mark orphans while recognizing configured loot, saved journal keys, and NPC shops");
        const auto finds = [&](ContentFindingKind kind, const QString& ending) {
            return std::any_of(
                audit.findings.cbegin(), audit.findings.cend(), [&](const ContentFinding& finding) {
                    return finding.kind == kind && finding.record_path.endsWith(ending);
                });
        };
        expect(
            !finds(ContentFindingKind::NoKnownRoute, "/5.json") &&
                !finds(ContentFindingKind::NoKnownRoute, "/11_2.json") &&
                finds(ContentFindingKind::NoKnownRoute, "/11_3.json") &&
                finds(ContentFindingKind::NoKnownRoute, "/7.json") &&
                finds(ContentFindingKind::NoKnownRoute, "/8.json"),
            "follow NPC entry points through quest and dialog links and find disconnected cycles");
        expect(finds(ContentFindingKind::MissingTarget, "/5.json") &&
                   !finds(ContentFindingKind::MissingTarget, "/11_1.json"),
               "report a broken dialog link and skip unsupported raw actions");
        expect(finds(ContentFindingKind::NoKnownRoute, "/groups/303.json") &&
                   finds(ContentFindingKind::NoKnownRoute, "/groups/305.json") &&
                   !finds(ContentFindingKind::NoKnownRoute, "/groups/306.json"),
               "trace drop groups to loot sources, including the built-in global item group");
        for (const auto* path : {"/monsters/1.json",
                                 "/itemgroups/101.json",
                                 "/groups/307.json",
                                 "/tasks/22.json",
                                 "/server/shops/1.json",
                                 "/server/shops/2.json"}) {
            expect(finds(ContentFindingKind::MissingTarget, path),
                   "report missing targets in the extended audit");
        }
        expect(std::none_of(audit.findings.cbegin(),
                            audit.findings.cend(),
                            [](const ContentFinding& finding) {
                                return finding.field.value(0) == "excludeItems";
                            }),
               "shop exclusions can refer to items outside the active catalog");

        Workspace workspace;
        workspace.config = root;
        workspace.base = base;
        ContentWindow window(workspace);
        window.show();
        QApplication::processEvents();
        auto* browser = window.findChild<QListWidget*>();
        auto* editor = window.findChild<JsonEditor*>();
        QCheckBox* filter = nullptr;
        for (auto* box : window.findChildren<QCheckBox*>()) {
            if (box->text() == "Possible orphans only") {
                filter = box;
                break;
            }
        }
        QComboBox* category = nullptr;
        for (auto* combo : window.findChildren<QComboBox*>()) {
            if (combo->findText("Monsters") >= 0) {
                category = combo;
                break;
            }
        }
        expect(browser && editor && filter && category, "show audit controls in the editor");
        if (!browser || !editor || !filter || !category) {
            return;
        }
        const auto find_row = [&](const QString& path) -> QListWidgetItem* {
            for (int i = 0; i < browser->count(); ++i) {
                auto* item = browser->item(i);
                if (item->data(Qt::UserRole) == path) {
                    return item;
                }
            }
            return nullptr;
        };
        category->setCurrentIndex(category->findText("Quests"));
        const auto* orphan_step = find_row(root + "/quests/12_1.json");
        expect(orphan_step && orphan_step->background().style() == Qt::SolidPattern,
               "tint a possible orphan in the quest list");
        filter->setChecked(true);
        const QList<QPair<Kind, QString>> additional{
            {Kind::ItemGroup, "/itemgroups/103.json"},
            {Kind::DropGroup, "/groups/307.json"},
            {Kind::Task, "/tasks/66.json"},
            {Kind::Shop, "/server/shops/2.json"},
        };
        for (const auto& [kind, path] : additional) {
            category->setCurrentIndex(category->findText(label(kind)));
            const auto* row = find_row(root + path);
            expect(filter->isVisible() && row && !row->isHidden() &&
                       row->background().style() == Qt::SolidPattern,
                   "tint and filter orphans in the additional catalogs");
        }
        category->setCurrentIndex(category->findText("Dialogs"));
        const auto* complete_dialog = find_row(source);
        const auto* orphan_dialog = find_row(orphan);
        expect(filter->isVisible() && complete_dialog && complete_dialog->isHidden() &&
                   orphan_dialog && !orphan_dialog->isHidden() &&
                   orphan_dialog->background().style() == Qt::SolidPattern &&
                   orphan_dialog->toolTip().contains("Possible orphan"),
               "show reasons and filter the list to tinted dialogs");

        QTimer::singleShot(0, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            auto* results = dialog->findChild<QTreeWidget*>("ContentAuditResults");
            auto* group = dialog->findChild<QComboBox*>();
            if (results && group) {
                group->setCurrentIndex(
                    group->findData(static_cast<int>(ContentFindingKind::MissingTarget)));
                if (auto* finding = results->topLevelItem(0)) {
                    expect(finding->text(1).contains("999"), "show the missing target ID");
                    results->setCurrentItem(finding);
                    if (auto* open = button(*dialog, "Open definition")) {
                        open->click();
                        return;
                    }
                }
            }
            dialog->reject();
        });
        expect(action(window, "Audit content references…") &&
                   editor->selected_path() == JsonPath{"links", "1", "type"} &&
                   browser->currentItem() && browser->currentItem()->data(Qt::UserRole) == source &&
                   !browser->currentItem()->isHidden(),
               "open the broken link's source field even with the orphan filter enabled");
        if (auto draft = editor->object()) {
            auto links = draft->value("links").toArray();
            links.append(QJsonObject{{"type", "6"}});
            draft->insert("links", links);
            editor->apply(*draft);
        }
        expect(action(window, "Save definition"), "save a new link to the orphan dialog");
        const auto* linked = find_row(orphan);
        expect(linked && linked->background().style() == Qt::NoBrush,
               "remove the orphan highlight as soon as its new link is saved");
    }

    void test_quest_graph(const QString& root, const QString& base) {
        const auto npc = root + "/npcs/1.json";
        const auto entry = root + "/quests/0055_01.json";
        const auto next = root + "/quests/0055_03.json";
        expect(
            write(npc, R"({"index":1,"name":"Quest NPC","xy":[10,20,30],"quest":[55,1]})") &&
                write(
                    entry,
                    R"({"index":55,"flag":1,"cases":[{"if":[],"then":[{"op":"html","id":6},{"op":"link","quest":55,"flag":3},{"op":"save","quest":80,"flag":1}],"designerNote":"keep"}]})") &&
                write(next, R"({"index":55,"flag":3,"cases":[]})") &&
                write(root + "/items/42.json", R"({"index":42,"name":"Quest Reward"})") &&
                write(root + "/quests/0080_01.json", R"({"index":80,"flag":1,"cases":[]})") &&
                write(base + "/quests/0070_01.json", R"({"index":70,"flag":1,"cases":[]})") &&
                write(root + "/dialogs/000006.json", R"({"id":6,"links":[{"type":"7"}]})") &&
                write(
                    root + "/dialogs/000007.json",
                    R"({"id":7,"links":[{"type":"6"},{"type":"quest","quest":[70,1]},{"type":"quest","quest":[55,1]}]})") &&
                write(root + "/tasks/55.json", R"({"key":55,"name":"Editor quest"})") &&
                write(root + "/tasks/90.json",
                      R"({"key":90,"name":"Journal task","flags":[{"key":2,"npcIndex":1}]})"),
            "write NPC, quest flow, chained dialogs and journal fixtures");
        bool populated = true;
        for (int i = 2; i <= 80; ++i) {
            populated =
                write(root + "/npcs/" + QString::number(i) + ".json",
                      serialize(QJsonObject{{"index", i}, {"xy", QJsonArray{10, 20, 30}}})) &&
                populated;
        }
        expect(populated, "populate a scrollable catalog");

        Workspace workspace;
        workspace.config = root;
        workspace.base = base;
        ContentWindow window(workspace);
        window.resize(1300, 680);
        window.show();
        QApplication::processEvents();
        auto* browser = window.findChild<QListWidget*>();
        auto* editor = window.findChild<JsonEditor*>();
        auto* links = window.findChild<QTreeWidget*>("NpcQuests");
        auto* graph = window.findChild<QuestEditor*>();
        auto* tabs = editor ? editor->findChild<QTabWidget*>() : nullptr;
        expect(browser && editor && links && graph && tabs, "open editor with quest controls");
        if (!browser || !editor || !links || !graph || !tabs) {
            return;
        }

        browser->setCurrentRow(0);
        QApplication::processEvents();
        const auto* direct = links->topLevelItem(0);
        const auto* linked = links->topLevelItem(1);
        const auto* dialog = links->topLevelItem(2);
        const auto* journal = links->topLevelItem(3);
        expect(links->topLevelItemCount() == 4 && direct && direct->text(0) == "55:1" &&
                   direct->text(1).contains("NPC entry") && direct->text(1).contains("Dialog 7") &&
                   linked && linked->text(0) == "55:3" &&
                   linked->text(1).contains("Linked from 55:1") && dialog &&
                   dialog->text(0) == "70:1" && dialog->text(1).contains("Dialog 7") && journal &&
                   journal->text(0) == "90:2" && journal->text(1).contains("Journal task"),
               "list unique NPC quest steps and their connection without following saved state");

        auto* viewport = browser->viewport();
        const QPoint pointer(70, 170);
        QWheelEvent wheel(pointer,
                          viewport->mapToGlobal(pointer),
                          QPoint{},
                          {0, -840},
                          Qt::NoButton,
                          Qt::NoModifier,
                          Qt::NoScrollPhase,
                          false);
        QApplication::sendEvent(viewport, &wheel);
        QApplication::processEvents();
        const auto* hovered = browser->itemAt(pointer);
        const auto clicked = hovered ? hovered->data(Qt::UserRole).toString() : QString{};
        const auto scroll = browser->verticalScrollBar()->value();
        expect(scroll > 0 && !clicked.isEmpty() && clicked != npc,
               "wheel scroll to a different NPC");
        for (const auto event : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent mouse(event,
                              pointer,
                              viewport->mapToGlobal(pointer),
                              Qt::LeftButton,
                              event == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                              Qt::NoModifier);
            QApplication::sendEvent(viewport, &mouse);
        }
        QApplication::processEvents();
        expect(browser->currentItem() && browser->currentItem()->data(Qt::UserRole) == clicked &&
                   editor->object().value_or(QJsonObject{}).value("index").toInt() ==
                       QFileInfo(clicked).baseName().toInt() &&
                   browser->verticalScrollBar()->value() == scroll && browser->itemAt(pointer) &&
                   browser->itemAt(pointer)->data(Qt::UserRole) == clicked,
               "open the row under the pointer and keep the scrolled list in place");
        browser->setCurrentRow(0);
        QApplication::processEvents();

        links->setCurrentItem(links->topLevelItem(2));
        if (auto* open = button(window, "Open")) {
            open->click();
        }
        expect(tabs->currentWidget() == graph && !button(*graph, "Add case") &&
                   editor->object().value_or(QJsonObject{}).value("index") == 70,
               "open a dialog quest in a read-only graph");

        auto choose = [&](const QString& filename) {
            for (int i = 0; i < browser->count(); ++i) {
                if (QFileInfo(browser->item(i)->data(Qt::UserRole).toString()).fileName() ==
                    filename) {
                    browser->setCurrentRow(i);
                    QApplication::processEvents();
                    break;
                }
            }
        };
        choose("0055_01.json");
        expect(tabs->currentWidget() == graph && button(*graph, "Open step 55:3"),
               "open a quest in the graph and show the executable link");
        expect(browser->currentItem() && browser->currentItem()->text().contains("Editor quest"),
               "show the loaded journal title in the quest browser");
        if (auto* add = button(*graph, "Add condition…")) {
            QTimer::singleShot(0, &window, [] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (dialog) {
                    if (auto* level = dialog->findChild<QSpinBox*>()) {
                        level->setValue(18);
                    }
                    dialog->accept();
                }
            });
            add->click();
        }
        QApplication::processEvents();
        if (auto* add = button(*graph, "Add action…")) {
            QTimer::singleShot(0, &window, [] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (dialog) {
                    if (auto* op = dialog->findChild<QComboBox*>()) {
                        op->setCurrentIndex(op->findData("exp"));
                    }
                    if (auto* amount = dialog->findChild<QSpinBox*>()) {
                        amount->setValue(250);
                    }
                    dialog->accept();
                }
            });
            add->click();
        }
        QApplication::processEvents();
        if (auto* add = button(*graph, "Add case")) {
            add->click();
        }
        QApplication::processEvents();
        const auto branch = editor->object().value_or(QJsonObject{}).value("cases").toArray();
        const auto first_case = branch.at(0).toObject();
        expect(branch.size() == 2 &&
                   first_case.value("if").toArray() ==
                       QJsonArray{QJsonObject{{"op", "level"}, {"level", 18}}} &&
                   first_case.value("then").toArray().last() ==
                       QJsonObject{{"op", "exp"}, {"exp", 250}} &&
                   first_case.value("designerNote") == "keep",
               "graph writes typed conditions and rewards while keeping case metadata");

        if (auto* add = button(*graph, "Add action…")) {
            QTimer::singleShot(0, &window, [] {
                if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                    if (auto* op = dialog->findChild<QComboBox*>()) {
                        op->setCurrentIndex(op->findData("item"));
                    }
                    for (auto* product : dialog->findChildren<QPushButton*>()) {
                        if (product->accessibleName() != "Add reward item") {
                            continue;
                        }
                        QTimer::singleShot(0, dialog, [] {
                            if (auto* item =
                                    qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                                for (auto* value : item->findChildren<QSpinBox*>()) {
                                    if (value->accessibleName() == "Item ID") {
                                        value->setValue(42);
                                    } else if (value->accessibleName() == "Quantity") {
                                        value->setValue(3);
                                    }
                                }
                                if (auto* bound = item->findChild<QCheckBox*>()) {
                                    bound->setChecked(true);
                                }
                                item->accept();
                            }
                        });
                        product->click();
                        break;
                    }
                    dialog->accept();
                }
            });
            add->click();
        }
        QApplication::processEvents();

        auto* source = editor->findChild<QPlainTextEdit*>();
        tabs->setCurrentWidget(source);
        expect(source && source->toPlainText().contains("250"), "show graph edits in JSON");
        if (source) {
            source->setPlainText(source->toPlainText().replace("250", "251"));
        }
        tabs->setCurrentWidget(graph);
        expect(action(window, "Save definition") && read(entry).contains("251") &&
                   read(entry).contains("designerNote") && read(entry).contains("[42, 0, 3, 1]"),
               "save graph and JSON changes to the same file");
        if (auto* open = button(*graph, "Open step 55:3")) {
            open->click();
        }
        expect(editor->object().value_or(QJsonObject{}).value("flag") == 3,
               "navigate to an exact linked step from the graph");

        if (auto* add = button(*graph, "New step…")) {
            QTimer::singleShot(0, &window, [] {
                if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
                    expect(dialog->intValue() == 4, "suggest the next free quest step");
                    dialog->accept();
                }
            });
            add->click();
        }
        const auto step_path = root + "/quests/0055_04.json";
        expect(action(window, "Save definition") && QFileInfo::exists(step_path) &&
                   tabs->currentWidget() == graph,
               "create a new step for the current quest in the usual catalog");

        for (auto* combo : window.findChildren<QComboBox*>()) {
            if (const int index = combo->findText("NPCs"); index >= 0) {
                combo->setCurrentIndex(index);
                break;
            }
        }
        choose("1.json");
        if (auto* set = button(window, "Set entry step…")) {
            QTimer::singleShot(0, &window, [] {
                if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                    if (auto* list = dialog->findChild<QListWidget*>()) {
                        for (int i = 0; i < list->count(); ++i) {
                            if (list->item(i)->data(Qt::UserRole) == "55:4") {
                                list->setCurrentRow(i);
                                break;
                            }
                        }
                    }
                    dialog->accept();
                }
            });
            set->click();
        }
        expect(action(window, "Save definition") &&
                   QJsonDocument::fromJson(read(npc)).object().value("quest").toArray() ==
                       QJsonArray{55, 4} &&
                   links->topLevelItem(0) && links->topLevelItem(0)->text(0) == "55:4",
               "assign the new quest step from the NPC and update its quest list");
    }

    void test_map(const QString& root, const QString& client) {
        namespace test = openkal::test;
        const auto data = client + "/data";
        const auto geometry = client + "/map";
        const auto atlas = data + "/HyperText/MiniMap";
        expect(QDir().mkpath(atlas + "/e") && QDir().mkpath(data + "/MAPS") &&
                   QDir().mkpath(geometry),
               "create synthetic asset directories");
        expect(test::write_solid_dds(
                   (atlas + "/e/minimap_n_e_000_000.gtx").toStdString(), 32, 32, 180, 60, 30, 255),
               "write overworld map tile");
        expect(
            test::write_solid_dds(
                (atlas + "/minimap_Dungeon_room_000.gtx").toStdString(), 32, 32, 35, 145, 75, 255),
            "write dungeon map tile");
        expect(test::write_kcm_v7((data + "/MAPS/n_000_000.kcm").toStdString(), 0, 0, 0, 1, 1250),
               "write outdoor height map");
        openkal::terrain::OplInstance building;
        building.name = "OBJECTS/building.gb";
        building.tx = 380.0f / openkal::map::kWorldSize;
        building.ty = 360.0f / openkal::map::kWorldSize;
        building.tz = 1640;
        building.qx = building.qw = std::sqrt(0.5f);
        building.sx = building.sy = building.sz = 8;
        const auto objects = data + "/OBJECTS";
        expect(
            QDir().mkpath(objects) &&
                test::write_lm_static_gb_v10((objects + "/building.gb").toStdString(),
                                             "roof.dds") &&
                test::write_solid_dds((objects + "/roof.dds").toStdString(), 8, 8, 24, 216, 48) &&
                write(data + "/MAPS/n_000_000.opl",
                      openkal::world_editor::encode_opl({}, {0, 0}, {building})),
            "write a textured outdoor building and its placement");
        expect(make_dungeon(geometry.toStdString()), "write walkable dungeon floor");
        const auto npc_clothes = data + "/Npc/Clothes";
        const auto models = QFileInfo(root).absolutePath() + "/RuntimeData/models";
        expect(QDir().mkpath(npc_clothes) && QDir().mkpath(models) &&
                   test::write_solid_dds(
                       (npc_clothes + "/npc.dds").toStdString(), 8, 8, 190, 60, 25) &&
                   test::write_solid_dds(
                       (models + "/monster.dds").toStdString(), 8, 8, 60, 160, 215) &&
                   test::write_lm_static_gb_v10((npc_clothes + "/N011_B1.gb").toStdString(),
                                                "npc.dds") &&
                   test::write_lm_static_gb_v10((models + "/editor-fixture.gb").toStdString(),
                                                "monster.dds"),
               "write owned fixtures for NPC and monster appearances");
        expect(
            write(
                root + "/mapinfo.json",
                R"({"maps":[{"index":0,"kind":1,"x":0,"y":0},{"index":1,"kind":1,"x":2,"y":2,"mapname":"Map\\room"}]})"),
            "write map catalog");
        expect(
            write(
                root + "/npcs/300.json",
                R"({"index":300,"name":"Editor test NPC","shape":11,"xy":[100, 100, 12490],"dir":[110, 110]})"),
            "write NPC with existing facing");
        expect(write(root + "/npcs/301.json",
                     R"({"index":301,"map":1,"shape":11,"xy":[16420,16430,0]})"),
               "write a dungeon NPC to test automatic map selection");
        expect(
            write(
                root + "/monsters/400.json",
                R"({"index":400,"name":"Editor test monster","stats":{"hp":150},"appearance":{"owned_model":"RuntimeData/models/editor-fixture.gb"},"spawns":[]})"),
            "write monster");
        expect(
            write(root + "/monsters/401.json",
                  R"({"index":401,"spawns":[{"map":6,"rect":[13,13,14,14],"max":1,"cycle":30}]})"),
            "write a second monster with an old field map ID");
        Workspace workspace;
        workspace.config = root;
        workspace.data = data;
        workspace.map = geometry;
        ContentWindow window(workspace);
        window.resize(1380, 760);
        window.show();
        QApplication::processEvents();
        auto* map = window.findChild<MapView*>();
        auto* browser = window.findChild<QListWidget*>();
        auto* editor = window.findChild<JsonEditor*>();
        auto* preview = window.findChild<PlacementPreview*>();
        expect(map && browser && editor && preview, "open Qt editor workspace");
        if (!map || !browser || !editor || !preview) {
            return;
        }
        const bool check_gl = qEnvironmentVariableIsSet("OPENKAL_TEST_PREVIEW_GL");
        const WId initial_window = check_gl ? window.winId() : WId{};
        auto* initial_view = preview->findChild<QOpenGLWidget*>();
        if (check_gl) {
            expect(initial_view && initial_view->isValid() && initial_view->isVisible(),
                   "initialize the visible OpenGL viewport before selecting a record");
        }
        const auto focused_on = [&](int map_id, QPoint point) {
            const auto* viewport = map->viewport();
            return map->map_id() == map_id &&
                   QLineF(map->mapFromScene(point.x(), -point.y()), viewport->rect().center())
                           .length() <= 2.0 &&
                   std::min(viewport->width(), viewport->height()) / map->transform().m11() < 480;
        };
        const auto preview_contains = [&](const QString& detail) {
            const auto labels = preview->findChildren<QLabel*>();
            return std::any_of(labels.begin(), labels.end(), [&](const auto* label) {
                return label->text().contains(detail);
            });
        };
        const auto verify_frame = [&](const char* description) {
            if (check_gl) {
                auto* view = dynamic_cast<openkal::world_editor::WorldViewport*>(
                    preview->findChild<QOpenGLWidget*>());
                expect(view && !view->grabFramebuffer().isNull() && view->preview_model_available(),
                       description);
            }
        };

        expect(map->maps().value(1) == "D2 (map 1)",
               "use the client dungeon name for the server map ID");
        expect(map->spawn_map(1, {16386, 16386}) == 1 && map->spawn_map(6, {9000, 650}) == 6,
               "keep configured spawn maps for dungeon cells and cells absent from the catalog");
        expect(map->elevations({500, 650}) == QVector<int>{12500}, "sample outdoor KCM server Z");
        expect(map->elevations({9000, 650}).isEmpty(), "missing KCM does not invent a height");
        map->show_map(1, 0);
        expect(map->elevations({16386, 16386}) == QVector<int>{0}, "sample indoor GB floor");
        map->focus({17000, 27000});
        auto dungeon = map->viewport()->grab().toImage();
        const auto dungeon_color = dungeon.pixelColor(dungeon.width() / 2, dungeon.height() / 2);
        expect(dungeon_color.green() > dungeon_color.red() + 50,
               "draw correctly oriented dungeon atlas");
        map->show_map(0, 0);
        map->focus({500, 650});
        auto world = map->viewport()->grab().toImage();
        const auto world_color = world.pixelColor(world.width() / 2, world.height() / 2);
        expect(world_color.red() > world_color.green() + 70, "draw the client's overworld tile");

        if (qEnvironmentVariableIsSet("OPENKAL_TEST_PREVIEW_GL")) {
            const auto has_roof = [&](QPoint point) {
                const auto pixmap = map->viewport()->grab();
                const QPoint screen =
                    map->mapFromScene(point.x(), -point.y()) * pixmap.devicePixelRatio();
                const auto frame = pixmap.toImage();
                if (!frame.rect().contains(screen)) {
                    return false;
                }
                const auto color = frame.pixelColor(screen);
                return color.green() > color.red() + 60 && color.green() > color.blue() + 60;
            };
            QElapsedTimer timer;
            timer.start();
            bool found = false;
            do {
                QApplication::processEvents();
                found = has_roof({400, 380});
                QThread::msleep(10);
            } while (!found && timer.elapsed() < 2000);
            expect(found && !has_roof({280, 380}) && !has_roof({400, 260}),
                   "render the textured roof at the correct X/Y in the high-detail map");
            map->fit();
            expect(!has_roof({400, 380}), "use the client atlas when zoomed out");
            map->focus({500, 650});
        }

        const auto zoom_at = [&](QPoint pointer, int delta) {
            const auto anchor = map->viewportTransform().inverted().map(QPointF(pointer));
            const auto old_scale = map->transform().m11();
            QWheelEvent wheel(pointer,
                              map->viewport()->mapToGlobal(pointer),
                              {},
                              {0, delta},
                              Qt::NoButton,
                              Qt::NoModifier,
                              Qt::NoScrollPhase,
                              false);
            QApplication::sendEvent(map->viewport(), &wheel);
            return (delta > 0 ? map->transform().m11() > old_scale
                              : map->transform().m11() < old_scale) &&
                   QLineF(map->viewportTransform().map(anchor), QPointF(pointer)).length() <= 1.0;
        };
        expect(zoom_at({map->viewport()->width() * 3 / 4, map->viewport()->height() / 4}, 120),
               "zoom in around the wheel event's off-center pointer");
        expect(zoom_at({map->viewport()->width() / 3, map->viewport()->height() * 2 / 3}, -120),
               "zoom out around an updated pointer position");

        browser->setCurrentRow(0);
        QApplication::processEvents();
        expect(focused_on(0, {100, 100}), "center and zoom when selecting an NPC");
        if (check_gl) {
            expect(window.winId() == initial_window &&
                       preview->findChild<QOpenGLWidget*>() == initial_view,
                   "keep the same native window and OpenGL viewport on the first selection");
        }
        expect(preview->isVisible() && preview_contains("X 100  Y 100  Z 12490"),
               "preview the selected NPC at its configured position");
        verify_frame("render the NPC asset in an OpenGL framebuffer");
        expect(action(window, "Locate on map"), "locate selected NPC");
        expect(action(window, "Place / move NPC"), "begin map placement");
        const QPoint target{200, 250};
        mouse(*map, QEvent::MouseButtonPress, target, Qt::LeftButton, Qt::LeftButton);
        mouse(*map, QEvent::MouseButtonRelease, target, Qt::LeftButton, Qt::NoButton);
        const auto placed = editor->object().value_or(QJsonObject{});
        const auto xy = placed.value("xy").toArray();
        const auto dir = placed.value("dir").toArray();
        expect(std::abs(xy.at(0).toInt() - target.x()) <= 2 &&
                   std::abs(xy.at(1).toInt() - target.y()) <= 2 && xy.at(2).toInt() == 12500,
               "map clicks store XYZ in server coordinates");
        expect(dir.at(0).toInt() == xy.at(0).toInt() + 10 &&
                   dir.at(1).toInt() == xy.at(1).toInt() + 10,
               "moving an NPC preserves its facing");
        expect(preview_contains(
                   QString("X %1  Y %2  Z 12500").arg(xy.at(0).toInt()).arg(xy.at(1).toInt())),
               "follow an unsaved NPC movement in the preview");
        expect(action(window, "Save definition"), "save NPC from the window");
        expect(QJsonDocument::fromJson(read(root + "/npcs/300.json")).object().value("xy") == xy,
               "persist the NPC position in loose config");

        for (auto* combo : window.findChildren<QComboBox*>()) {
            const int index = combo->findText("Monsters");
            if (index >= 0) {
                combo->setCurrentIndex(index);
                break;
            }
        }
        QString picked_spawn;
        QObject::connect(
            map, &MapView::marker_selected, &window, [&](const QString& path, int spawn) {
                if (spawn >= 0) {
                    picked_spawn = QFileInfo(path).fileName();
                }
            });
        const auto pick_spawn = [&](QPoint point) {
            picked_spawn.clear();
            mouse(*map, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
            mouse(*map, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
            return picked_spawn;
        };
        const QPoint second_spawn{430, 430};
        map->focus({320, 320});
        expect(pick_spawn(second_spawn).isEmpty(), "hide spawns until a monster is selected");

        browser->setCurrentRow(0);
        QApplication::processEvents();
        map->focus({320, 320});
        expect(preview->isVisible() && preview_contains("3D preview · 400"),
               "open a model preview for a monster without spawns");
        verify_frame("render the owned monster model in an OpenGL framebuffer");
        expect(action(window, "New spawn"), "begin drawing a spawn");
        mouse(*map, QEvent::MouseButtonPress, {51, 83}, Qt::LeftButton, Qt::LeftButton);
        mouse(*map, QEvent::MouseMove, {276, 340}, Qt::NoButton, Qt::LeftButton);
        mouse(*map, QEvent::MouseButtonRelease, {276, 340}, Qt::LeftButton, Qt::NoButton);
        expect(action(window, "Save definition"), "save monster from the window");
        const auto result = QJsonDocument::fromJson(read(root + "/monsters/400.json")).object();
        const auto spawn = result.value("spawns").toArray().at(0).toObject();
        expect(spawn.value("rect").toArray() == QJsonArray{1, 2, 8, 10},
               "persist spawn rectangle in server tile coordinates");
        expect(result.value("stats").toObject().value("hp").toInt() == 150,
               "placement preserves monster stats");
        expect(preview_contains("X 144  Y 192  Z 12500"),
               "preview the new monster spawn at its rectangle center and sampled height");
        const QPoint first_spawn{150, 170};
        expect(pick_spawn(first_spawn) == "400.json" && pick_spawn(second_spawn).isEmpty(),
               "show the selected monster's spawn and hide other monsters' spawns");
        browser->setCurrentRow(1);
        QApplication::processEvents();
        expect(focused_on(0, {432, 432}),
               "center and zoom on an old field-map spawn on the overworld");
        expect(pick_spawn(first_spawn).isEmpty() && pick_spawn(second_spawn) == "401.json",
               "switch spawn markers when the selected monster changes");
        expect(preview_contains("Overworld") && editor->object()
                                                        .value_or(QJsonObject{})
                                                        .value("spawns")
                                                        .toArray()
                                                        .at(0)
                                                        .toObject()
                                                        .value("map") == 6,
               "preview the effective overworld location while preserving the configured map");
        for (const auto* combo : window.findChildren<QComboBox*>()) {
            if (combo->accessibleName() == "Monster spawn") {
                expect(combo->currentText().contains("Overworld (configured map 6)"),
                       "explain the map assignment in the spawn selector");
            }
        }
        for (auto* combo : window.findChildren<QComboBox*>()) {
            const int index = combo->findText("NPCs");
            if (index >= 0) {
                combo->setCurrentIndex(index);
                break;
            }
        }
        browser->setCurrentRow(1);
        QApplication::processEvents();
        expect(focused_on(1, {16420, 16430}),
               "switch to the NPC's dungeon and zoom to its position");
    }
}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir workspace;
    expect(workspace.isValid(), "create an isolated fixture directory");
    if (workspace.isValid()) {
        const auto root = workspace.filePath("config");
        test_catalog(root, workspace.filePath("base"));
        test_profiles(workspace.filePath("profiles"), workspace.filePath("client"));
        test_map(root, workspace.filePath("client"));
        test_quest_graph(workspace.filePath("quests"), workspace.filePath("quest-base"));
        test_content_audit(workspace.filePath("audit"), workspace.filePath("audit-base"));
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
