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
#include "test_util.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineF>
#include <QListWidget>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QThread>
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
                  R"({"index":401,"spawns":[{"map":0,"rect":[13,13,14,14],"max":1,"cycle":30}]})"),
            "write a second monster with a spawn");
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
        expect(focused_on(0, {432, 432}), "center and zoom on the selected monster spawn");
        expect(pick_spawn(first_spawn).isEmpty() && pick_spawn(second_spawn) == "401.json",
               "switch spawn markers when the selected monster changes");
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
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
