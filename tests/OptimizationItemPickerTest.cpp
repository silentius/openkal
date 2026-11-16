/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dialogs/ItemPickerDialog.h"
#include "test_util.h"

#include <QApplication>
#include <QBrush>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>

namespace {
    using openkal::test::expect;

    void write(const std::string& path, const std::string& contents) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream stream(path);
        stream << contents;
        expect(stream.good(), "write owned picker catalog");
    }

    struct Fixture {
        QTemporaryDir temporary;
        std::string root = temporary.path().toStdString();
        LootDataStore store;
        Fixture() {
            // initial, synthetic catalog and raster fixture; no game data or external assets.
            for (const auto* directory : {"config/items",
                                          "config/groups",
                                          "config/itemgroups",
                                          "config/monsters",
                                          "HyperText"}) {
                std::filesystem::create_directories(root + "/" + directory);
            }
            write(root + "/config/groups/1.json", R"({"index":1,"entries":[]})");
            write(root + "/config/itemgroups/1.json", R"({"index":1,"entries":[]})");
            add(31, "Money", "general", "etc");
            add(32, "Coin", "money", "etc");
            add(33, "Coin", "general", "coin");
            add(100, "Zero", "weapon", "sword", 0, "knight");
            add(101, "Alpha", "weapon", "sword", 3, "knight");
            add(102, "alpha", "weapon", "sword", 3, "knight");
            add(103, "Vest", "defense", "armor", 3, "knight");
            add(104, "Accessory", "general", "etc", 3, "knight");
            add(105, "Wand", "weapon", "wand", 2, "mage");
            add(106, "Bow", "weapon", "bow", 4, "archer");
            add(107, "Dagger", "weapon", "dagger", 5, "thief");
            add(108, "Journal", "quest", "quest");
            add(109, "Charm", "general", "charm");
            add(110, "Medicine", "general", "refresh");
            add(111, "Auto", "general", "hpautorefresh");
            add(112, "Ring", "ornament", "ring");
            add(113, "Companion", "pet", "pet");
            add(114, "Egg", "general", "egg");
            add(115, "Material", "general", "etc");
            add(116, "A Very Long Item Label", "general", "etc");
            add(117,
                "Overlay",
                "general",
                "charm",
                0,
                "",
                R"(,"specialty":{"changeprefix":["weapon", 20, 500, 60, 501]})");
            QImage icon(16, 16, QImage::Format_RGB32);
            icon.fill(QColor(220, 40, 20));
            expect(icon.save(QString::fromStdString(root + "/HyperText/owned.bmp")),
                   "write solid-color BMP fixture");
            reload();
        }
        void add(int index,
                 const char* name,
                 const char* big,
                 const char* small,
                 int grade = 0,
                 const char* clazz = "",
                 const char* extra = "") {
            write(root + "/config/items/" + std::to_string(index) + ".json",
                  "{\"index\":" + std::to_string(index) + ",\"name\":\"" + name +
                      "\",\"class\":[\"" + big + "\",\"" + small +
                      "\"],\"level\":" + std::to_string(grade) + ",\"image\":\"" +
                      (index == 115 ? "missing" : "owned") + "\",\"limit\":{\"class\":\"" + clazz +
                      "\"}" + extra + "}");
        }
        void reload() {
            LootEditorPaths paths;
            paths.repo_root = root;
            paths.loot_config_root = root + "/config";
            paths.data_root = root;
            expect(store.load(paths), "load owned picker tables");
        }
    };

    void show(ItemPickerDialog& dialog) {
        dialog.show();
        QApplication::processEvents();
        expect(QMetaObject::invokeMethod(&dialog, "refresh_list", Qt::DirectConnection),
               "refresh visible picker through Qt slot");
    }

    std::vector<QListView*> visible_lists(ItemPickerDialog& dialog, bool dispatch = true) {
        if (dispatch) {
            QApplication::processEvents();
        }
        std::vector<QListView*> lists;
        for (auto* list : dialog.findChildren<QListView*>()) {
            if (list->isVisible()) {
                lists.push_back(list);
            }
        }
        std::stable_sort(lists.begin(), lists.end(), [&](QListView* a, QListView* b) {
            return a->mapTo(&dialog, QPoint{}).y() < b->mapTo(&dialog, QPoint{}).y();
        });
        return lists;
    }

    std::vector<int> ids(ItemPickerDialog& dialog, bool dispatch = true) {
        std::vector<int> result;
        for (auto* list : visible_lists(dialog, dispatch)) {
            for (int row = 0; row < list->model()->rowCount(); ++row) {
                result.push_back(list->model()->index(row, 0).data(Qt::UserRole).toInt());
            }
        }
        return result;
    }

    QModelIndex item(ItemPickerDialog& dialog, int index) {
        for (auto* list : visible_lists(dialog)) {
            for (int row = 0; row < list->model()->rowCount(); ++row) {
                const QModelIndex candidate = list->model()->index(row, 0);
                if (candidate.data(Qt::UserRole).toInt() == index) {
                    return candidate;
                }
            }
        }
        return {};
    }

    QListView* view_for(ItemPickerDialog& dialog, const QModelIndex& index) {
        for (auto* list : visible_lists(dialog)) {
            if (list->model() == index.model()) {
                return list;
            }
        }
        return nullptr;
    }

    void select_item(ItemPickerDialog& dialog, int index) {
        const QModelIndex row = item(dialog, index);
        auto* list = view_for(dialog, row);
        expect(list && row.isValid(), "find the requested model row for selection");
        if (list && row.isValid()) {
            list->selectionModel()->select(row, QItemSelectionModel::Select);
        }
    }

    void catalog_and_filters(Fixture& fixture) {
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        const std::vector<int> all{100,
                                   116,
                                   111,
                                   109,
                                   113,
                                   114,
                                   108,
                                   115,
                                   110,
                                   117,
                                   112,
                                   105,
                                   101,
                                   102,
                                   103,
                                   104,
                                   106,
                                   107};
        expect(ids(dialog) == all,
               "catalog excludes money and coins and orders grade, kind, case-folded name, ID");
        auto* tabs = dialog.findChild<QTabBar*>();
        const std::vector<std::vector<int>> expected{all,
                                                     {100, 101, 102, 103, 104},
                                                     {105},
                                                     {106},
                                                     {107},
                                                     {108},
                                                     {109, 117},
                                                     {111, 110},
                                                     {112},
                                                     {113, 114},
                                                     {116, 115}};
        for (int tab = 0; tab < tabs->count(); ++tab) {
            tabs->setCurrentIndex(tab);
            expect(ids(dialog) == expected[static_cast<std::size_t>(tab)],
                   "each class and specialized tab has exactly the expected ordered IDs");
        }
        tabs->setCurrentIndex(0);
        auto* search = dialog.findChild<QLineEdit*>();
        const std::vector<std::pair<QString, std::vector<int>>> queries{
            {" aLPHa ", {101, 102}},
            {"105", {105}},
            {"g3", {101, 102, 103, 104}},
            {"weapon sword", {100, 101, 102}},
            {"armor", {103}},
            {"absent", {}},
            {"", all}};
        for (const auto& [query, expectedIds] : queries) {
            search->setText(query);
            expect(ids(dialog, false) == expectedIds,
                   "search filters synchronously without event dispatch and preserves order");
        }
        tabs->setCurrentIndex(1);
        search->setText("alpha");
        expect(ids(dialog) == std::vector<int>({101, 102}),
               "class search preserves ID tie order within a grade");
        search->clear();
        expect(dialog.findChildren<QGroupBox*>().size() == 2,
               "clearing class search recreates zero-grade and positive-grade groups");
        expect(icons.hypertext_ready(), "owned BMP is loaded through the production icon cache");
    }

    void pointer_selection(Fixture& fixture) {
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        dialog.findChild<QTabBar*>()->setCurrentIndex(1);
        const auto click = [&](int index, Qt::KeyboardModifiers modifiers) {
            const QModelIndex row = item(dialog, index);
            auto* list = view_for(dialog, row);
            expect(list && row.isValid(), "find visible tile for pointer selection");
            if (list && row.isValid()) {
                QTest::mouseClick(
                    list->viewport(), Qt::LeftButton, modifiers, list->visualRect(row).center());
            }
        };
        click(100, Qt::NoModifier);
        expect(dialog.selected_indices() == std::vector<int>({100}),
               "plain click selects the pointed tile");
        click(101, Qt::ControlModifier);
        expect(dialog.selected_indices() == std::vector<int>({100, 101}),
               "Ctrl click adds a selection across grade groups");
        click(103, Qt::ShiftModifier);
        expect(dialog.selected_indices() == std::vector<int>({100, 101, 102, 103}),
               "Shift click extends the current grade range and retains other grade selections");
        click(104, Qt::NoModifier);
        expect(dialog.selected_indices() == std::vector<int>({104}),
               "plain click clears selections in every other grade group");
        const QModelIndex row = item(dialog, 104);
        auto* list = view_for(dialog, row);
        QSignalSpy doubleClicked(list, &QListView::doubleClicked);
        QSignalSpy activated(list, &QListView::activated);
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        QTest::mouseDClick(
            list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualRect(row).center());
        expect(doubleClicked.count() == 1 && activated.count() == 1 &&
                   dialog.result() == QDialog::Accepted,
               "double click emits both Qt activation signals and accepts the dialog");
        expect(dialog.selected_indices() == std::vector<int>({104}) && accepted.count() == 1,
               "double-click retains the selected ID and accepts exactly once");
        expect(QMetaObject::invokeMethod(&dialog, "accept_selection", Qt::DirectConnection) &&
                   dialog.selected_indices() == std::vector<int>({104}) && accepted.count() == 1,
               "repeated activation after acceptance leaves the result and selected IDs intact");
        dialog.open();
        QApplication::processEvents();
        click(100, Qt::NoModifier);
        expect(QMetaObject::invokeMethod(&dialog, "accept_selection", Qt::DirectConnection) &&
                   dialog.result() == QDialog::Accepted &&
                   dialog.selected_indices() == std::vector<int>({100}) && accepted.count() == 2,
               "reopening the same dialog permits a new acceptance with a different selection");

        ItemPickerDialog keyboard(&fixture.store, &icons, 104);
        show(keyboard);
        const QModelIndex current = item(keyboard, 104);
        auto* keyboard_list = view_for(keyboard, current);
        keyboard_list->setCurrentIndex(current);
        QTest::keyClick(keyboard_list, Qt::Key_Return);
        expect(keyboard.result() == QDialog::Accepted && keyboard.selected_index() == 104,
               "Return activates a tile once and retains its selected ID");
    }

    void invalid_view_rows(Fixture& fixture) {
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        select_item(dialog, 100);
        select_item(dialog, 100);
        const QModelIndex first = item(dialog, 100);
        auto* list = view_for(dialog, first);
        list->selectionModel()->select(QModelIndex(), QItemSelectionModel::Select);
        expect(dialog.selected_indices() == std::vector<int>({100}),
               "invalid model indexes and repeated selection do not add selected IDs");
        list->clicked(QModelIndex());
        expect(dialog.selected_indices() == std::vector<int>({100}),
               "a missing model row in a click signal leaves existing selection intact");
    }

    void hidden_selection_refresh(Fixture& fixture) {
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        for (int index : {100, 101, 102, 103}) {
            select_item(dialog, index);
        }
        dialog.hide();
        auto* search = dialog.findChild<QLineEdit*>();
        auto* tabs = dialog.findChild<QTabBar*>();
        search->setText("alpha");
        tabs->setCurrentIndex(1);
        expect(dialog.selected_indices() == std::vector<int>({101, 102}),
               "hidden filter and tab changes retain ordered unique selected survivors");
        show(dialog);
        expect(dialog.selected_indices() == std::vector<int>({101, 102}),
               "showing a filtered dialog preserves its hidden multi-selection");
        dialog.hide();
        search->setText("102");
        tabs->setCurrentIndex(0);
        search->clear();
        expect(dialog.selected_indices() == std::vector<int>({102}),
               "hidden narrowing and widening retain only the surviving selected ID");
        show(dialog);
        expect(dialog.selected_indices() == std::vector<int>({102}),
               "showing the flat view does not restore IDs discarded while hidden");
    }

    void selections_and_tiles(Fixture& fixture) {
        ItemIconCache icons(fixture.store.items());
        for (const auto& [index, expectedTab] : std::vector<std::pair<int, int>>{{101, 1},
                                                                                 {105, 2},
                                                                                 {106, 3},
                                                                                 {107, 4},
                                                                                 {108, 5},
                                                                                 {109, 6},
                                                                                 {110, 7},
                                                                                 {111, 7},
                                                                                 {112, 8},
                                                                                 {113, 9},
                                                                                 {114, 9},
                                                                                 {115, 10},
                                                                                 {999, 0}}) {
            ItemPickerDialog dialog(&fixture.store, &icons, index);
            const int expected_selection = index == 999 ? -1 : index;
            expect(dialog.selected_index() == expected_selection,
                   "valid initial selection is retained before the dialog is shown");
            show(dialog);
            expect(dialog.findChild<QTabBar*>()->currentIndex() == expectedTab,
                   "initial selection chooses the matching primary tab");
            expect(dialog.selected_index() == expected_selection,
                   "initial selection survives a visible refresh and missing IDs stay unselected");
        }
        ItemPickerDialog dialog(&fixture.store, &icons, -1, {101, 117});
        show(dialog);
        const QModelIndex alpha = item(dialog, 101);
        expect(alpha.isValid() &&
                   alpha.data(Qt::DisplayRole).toString().startsWith(QString::fromUtf8("● ")) &&
                   alpha.data(Qt::ToolTipRole).toString().endsWith("Already in this group") &&
                   alpha.data(Qt::BackgroundRole).value<QBrush>().color() == QColor(120, 190, 255),
               "occupied tiles retain labels, tooltip and blue background");
        expect(item(dialog, 116).data(Qt::DisplayRole).toString() ==
                   QString::fromUtf8("A Very Lon…"),
               "long names retain the existing truncation contract");
        const auto occupied = alpha.data(Qt::DecorationRole).value<QPixmap>().toImage();
        const auto base = icons.icon(101).toImage();
        expect(!occupied.isNull() && occupied.width() >= 48 && occupied != base,
               "occupied composition differs from the icon");
        expect(item(dialog, 117).data(Qt::DecorationRole).value<QPixmap>().toImage() !=
                   icons.icon(117).toImage(),
               "prefix overlay and occupied border are composed on a tile");
        expect(!item(dialog, 115).data(Qt::DecorationRole).value<QPixmap>().isNull(),
               "missing icon produces a visible placeholder");

        select_item(dialog, 101);
        select_item(dialog, 102);
        expect(dialog.selected_indices() == std::vector<int>({101, 102}),
               "multi-selection has stable catalog order");
        auto* search = dialog.findChild<QLineEdit*>();
        search->setText("alpha");
        expect(dialog.selected_indices() == std::vector<int>({101, 102}),
               "flat filtering preserves each visible selected ID exactly once");
        dialog.findChild<QTabBar*>()->setCurrentIndex(1);
        expect(dialog.selected_indices() == std::vector<int>({101, 102}),
               "class filtering preserves selected IDs across grade-view reconstruction");
        search->setText("102");
        expect(dialog.selected_indices() == std::vector<int>({102}),
               "narrowing retains only visible selected IDs without duplicates");
        search->clear();
        expect(dialog.selected_indices() == std::vector<int>({102}),
               "widening retains surviving selection without restoring filtered-out IDs");
        search->setText("absent");
        expect(dialog.selected_index() == -1, "no matches clears the selection");
        expect(QMetaObject::invokeMethod(&dialog, "accept_selection", Qt::DirectConnection) &&
                   dialog.result() != QDialog::Accepted,
               "OK without a visible selection leaves the dialog open");
        search->clear();
        select_item(dialog, 100);
        select_item(dialog, 103);
        expect(dialog.selected_indices() == std::vector<int>({100, 103}),
               "multi-selection spans grade groups");
        expect(QMetaObject::invokeMethod(&dialog, "accept_selection", Qt::DirectConnection) &&
                   dialog.result() == QDialog::Accepted,
               "acceptance keeps selected IDs across grade groups");

        // A fresh presentation after catalog reload must use current names and occupied state.
        fixture.add(101, "Changed", "weapon", "sword", 3, "knight");
        fixture.add(117, "Overlay removed", "general", "charm");
        QImage changed_icon(16, 16, QImage::Format_RGB32);
        changed_icon.fill(QColor(20, 40, 220));
        expect(changed_icon.save(QString::fromStdString(fixture.root + "/HyperText/owned.bmp")),
               "replace the owned source icon before catalog reload");
        fixture.reload();
        ItemIconCache refreshed(fixture.store.items());
        ItemPickerDialog reloaded(&fixture.store, &refreshed, 101);
        show(reloaded);
        expect(item(reloaded, 101).data(Qt::DisplayRole).toString() == "Changed" &&
                   !item(reloaded, 101)
                        .data(Qt::ToolTipRole)
                        .toString()
                        .contains("Already in this group"),
               "new dialog after reload uses current names and occupied state");
        const auto fresh_tile =
            item(reloaded, 101).data(Qt::DecorationRole).value<QPixmap>().toImage();
        expect(!fresh_tile.isNull() && fresh_tile != occupied &&
                   fresh_tile == refreshed.icon(101).toImage() &&
                   fresh_tile.pixelColor(0, 0).blue() > fresh_tile.pixelColor(0, 0).red(),
               "a fresh dialog composes the replacement icon without the old occupied border");
        reloaded.findChild<QTabBar*>()->setCurrentIndex(0);
        expect(item(reloaded, 117).data(Qt::DecorationRole).value<QPixmap>().toImage() ==
                   refreshed.icon(117).toImage(),
               "a fresh dialog does not retain a removed text overlay");
        std::filesystem::remove(fixture.root + "/config/items/115.json");
        fixture.reload();
        reloaded.findChild<QTabBar*>()->setCurrentIndex(0);
        expect(QMetaObject::invokeMethod(&reloaded, "refresh_list", Qt::DirectConnection),
               "refresh the already active tab after catalog reload");
        expect(!item(reloaded, 115).isValid() && item(reloaded, 101).isValid(),
               "an open dialog safely filters deleted definitions after catalog reload");
    }

    void live_reload_order() {
        Fixture fixture;
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        const auto flat_order = ids(dialog);
        for (int index : {100, 101, 102, 104}) {
            select_item(dialog, index);
        }
        fixture.add(100, "Zero", "weapon", "sword", 5, "knight");
        fixture.add(101, "Zulu", "weapon", "sword", 3, "knight");
        fixture.add(102, "Aaa", "weapon", "sword", 3, "knight");
        fixture.add(104, "Abacus", "weapon", "sword", 3, "knight");
        fixture.reload();
        auto* tabs = dialog.findChild<QTabBar*>();
        tabs->setCurrentIndex(1);
        expect(ids(dialog) == std::vector<int>({102, 104, 101, 103, 100}),
               "an open class tab reorders changed names and kinds and regroups changed grades");
        expect(dialog.selected_indices() == std::vector<int>({102, 104, 101, 100}),
               "restored selection follows current grade and row order after metadata reload");
        tabs->setCurrentIndex(0);
        expect(ids(dialog) == flat_order &&
                   item(dialog, 101).data(Qt::DisplayRole).toString() == "Zulu",
               "an open flat tab retains its catalog order while showing reloaded labels");
        expect(dialog.selected_indices() == std::vector<int>({100, 101, 102, 104}),
               "returning to the flat view orders surviving selection by the fixed catalog");
        std::filesystem::remove(fixture.root + "/config/items/101.json");
        fixture.reload();
        expect(QMetaObject::invokeMethod(&dialog, "refresh_list", Qt::DirectConnection) &&
                   dialog.selected_indices() == std::vector<int>({100, 102, 104}),
               "reload drops a removed selected definition and retains every surviving ID");
    }

    void render_proof(const std::filesystem::path& output) {
        QApplication::setStyle("Fusion");
        QApplication::setPalette(QApplication::style()->standardPalette());
        QApplication::setFont(QFont("DejaVu Sans", 10));
        QApplication::setCursorFlashTime(0);
        std::filesystem::create_directories(output);
        std::ofstream geometry(output / "geometry.txt");
        Fixture fixture;
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1, {101, 117});
        dialog.resize(920, 680);
        show(dialog);
        geometry << "font " << QApplication::font().toString().toStdString() << '\n';
        const auto rectangle = [&](const QRect& rect) {
            geometry << rect.x() << ' ' << rect.y() << ' ' << rect.width() << ' ' << rect.height();
        };
        const auto capture = [&](const std::string& state) {
            if (QWidget* focus = QApplication::focusWidget()) {
                focus->clearFocus();
            }
            QApplication::processEvents();
            const auto lists = visible_lists(dialog, false);
            geometry << "state " << state << " lists " << lists.size() << '\n';
            for (std::size_t ordinal = 0; ordinal < lists.size(); ++ordinal) {
                auto* list = lists[ordinal];
                auto* viewport = list->viewport();
                geometry << "list " << ordinal << " viewport ";
                rectangle(QRect(viewport->mapTo(&dialog, QPoint{}), viewport->size()));
                geometry << " dpr " << viewport->devicePixelRatioF() << '\n';
                const auto image = viewport->grab().toImage();
                expect(
                    image.save(QString::fromStdString(
                        (output / (state + "-view-" + std::to_string(ordinal) + ".png")).string())),
                    "capture actual owned tile viewport pixels");
                for (int row = 0; row < list->model()->rowCount(); ++row) {
                    const QModelIndex index = list->model()->index(row, 0);
                    geometry << "tile " << index.data(Qt::UserRole).toInt() << ' ';
                    rectangle(list->visualRect(index));
                    geometry << '\n';
                }
            }
            auto* scroll = dialog.findChild<QScrollArea*>();
            if (scroll && scroll->isVisible()) {
                geometry << "grade-scroll ";
                rectangle(QRect(scroll->viewport()->mapTo(&dialog, QPoint{}),
                                scroll->viewport()->size()));
                geometry << '\n';
                expect(scroll->viewport()->grab().save(QString::fromStdString(
                           (output / (state + "-grade-viewport.png")).string())),
                       "capture the rendered grade frames and nested tile viewports");
            }
        };
        capture("all");
        select_item(dialog, 101);
        select_item(dialog, 117);
        capture("all-selected");
        dialog.findChild<QTabBar*>()->setCurrentIndex(1);
        select_item(dialog, 100);
        select_item(dialog, 103);
        capture("knight-selected");
        dialog.findChild<QLineEdit*>()->setText("alpha");
        capture("knight-filtered");
        dialog.findChild<QLineEdit*>()->setText("absent");
        capture("empty");
        geometry.flush();
        expect(geometry.good(), "record every owned tile visual rectangle and viewport geometry");
    }

    void generate_catalog(Fixture& fixture, int count) {
        std::filesystem::remove_all(fixture.root + "/config/items");
        for (int ordinal = 0; ordinal < count; ++ordinal) {
            const auto name = QString("Owned %1").arg(ordinal, 6, 10, QChar('0')).toStdString();
            const int kind = ordinal % 4;
            fixture.add(1000 + ordinal,
                        name.c_str(),
                        kind < 2 ? "weapon" : "general",
                        kind < 2    ? "sword"
                        : kind == 2 ? "refresh"
                                    : "etc",
                        ordinal % 8,
                        kind == 0   ? "knight"
                        : kind == 1 ? "mage"
                                    : "",
                        ordinal % 100 == 0
                            ? R"(,"specialty":{"changeprefix":["weapon", 20, 500, 60, 501]})"
                            : "");
        }
        fixture.reload();
        expect(fixture.store.items().by_index().size() == static_cast<std::size_t>(count),
               "the generated benchmark owns exactly the requested number of definitions");
    }

    void scrolled_selection() {
        Fixture fixture;
        generate_catalog(fixture, 1000);
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1);
        show(dialog);
        auto* list = visible_lists(dialog, false).front();
        const QModelIndex tail = list->model()->index(999, 0);
        list->scrollTo(tail, QAbstractItemView::PositionAtBottom);
        QApplication::processEvents();
        const QRect rectangle = list->visualRect(tail);
        expect(tail.data(Qt::UserRole).toInt() == 1999 &&
                   rectangle.intersects(list->viewport()->rect()) &&
                   list->viewport()->rect().contains(rectangle.center()),
               "scrolling a large catalog brings the final tile into the actual viewport");
        expect(tail.data(Qt::DisplayRole).toString() == "Owned 000999" &&
                   tail.data(Qt::ToolTipRole).toString() == fixture.store.item_tooltip(1999) &&
                   !tail.data(Qt::DecorationRole).value<QPixmap>().isNull(),
               "a newly exposed tail tile resolves its exact label, tooltip and icon");
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rectangle.center());
        expect(dialog.selected_indices() == std::vector<int>({1999}),
               "real viewport input selects the newly exposed tail tile");
        auto* search = dialog.findChild<QLineEdit*>();
        search->setText("Owned 000999");
        expect(ids(dialog, false) == std::vector<int>({1999}) &&
                   dialog.selected_indices() == std::vector<int>({1999}),
               "immediate filtering retains the selected tail ID");
        search->clear();
        expect(list->model()->rowCount() == 1000 &&
                   dialog.selected_indices() == std::vector<int>({1999}),
               "widening the large catalog retains the selected ID without duplicating it");
    }

    std::unordered_set<int> occupied_catalog(int count) {
        std::unordered_set<int> result;
        for (int ordinal = 0; ordinal < count; ordinal += 100) {
            result.insert(1000 + ordinal);
        }
        return result;
    }

    void select_sixteen(QListView& list) {
        for (int row = 0; row < std::min(16, list.model()->rowCount()); ++row) {
            list.selectionModel()->select(list.model()->index(row, 0), QItemSelectionModel::Select);
        }
    }

    void work_probe(int count, const QString& operation) {
        Fixture fixture;
        generate_catalog(fixture, count);
        ItemIconCache icons(fixture.store.items());
        ItemPickerDialog dialog(&fixture.store, &icons, -1, occupied_catalog(count));
        dialog.show();
        QApplication::processEvents();
        if (operation == "class") {
            dialog.findChild<QTabBar*>()->setCurrentIndex(1);
        } else if (operation == "search") {
            dialog.findChild<QLineEdit*>()->setText("Owned 000");
        } else if (operation == "restore") {
            select_sixteen(*visible_lists(dialog, false).front());
            dialog.findChild<QLineEdit*>()->setText("Owned");
        } else {
            expect(operation == "open", "known picker work-probe operation");
        }
        std::cout << "picker_work_probe definitions=" << count
                  << " operation=" << operation.toStdString()
                  << " visible_rows=" << ids(dialog, false).size() << '\n';
    }

    void benchmark() {
        using Clock = std::chrono::steady_clock;
        std::cout
            << "picker benchmark: Release comparison; 7 fresh dialogs per catalog; "
               "all work on the GUI thread; no timing threshold\n"
               "definitions,operation,samples,median_us,p95_us,flat_rows_inserted,flat_model_resets\n";
        for (int count : {1000, 10000, 50000}) {
            Fixture fixture;
            generate_catalog(fixture, count);
            std::map<std::string, std::vector<double>> samples;
            std::map<std::string, std::size_t> inserted;
            std::map<std::string, std::size_t> resets;
            for (int sample = 0; sample < 7; ++sample) {
                ItemIconCache icons(fixture.store.items());
                std::size_t rows_inserted = 0;
                std::size_t model_resets = 0;
                const auto begin = Clock::now();
                auto dialog = std::make_unique<ItemPickerDialog>(
                    &fixture.store, &icons, -1, occupied_catalog(count));
                dialog->show();
                QApplication::processEvents();
                samples["open_show_events"].push_back(
                    std::chrono::duration<double, std::micro>(Clock::now() - begin).count());
                auto* tabs = dialog->findChild<QTabBar*>();
                auto* search = dialog->findChild<QLineEdit*>();
                auto* flat = visible_lists(*dialog, false).front();
                QObject::connect(flat->model(),
                                 &QAbstractItemModel::rowsInserted,
                                 dialog.get(),
                                 [&](const QModelIndex&, int first, int last) {
                                     rows_inserted += static_cast<std::size_t>(last - first + 1);
                                 });
                QObject::connect(flat->model(), &QAbstractItemModel::modelReset, dialog.get(), [&] {
                    ++model_resets;
                });
                const auto timed = [&](const std::string& operation,
                                       const auto& action,
                                       bool dispatch = false,
                                       bool flat_view = true) {
                    const auto before = rows_inserted;
                    const auto before_resets = model_resets;
                    const auto start = Clock::now();
                    action();
                    if (dispatch) {
                        QApplication::processEvents();
                    }
                    samples[operation].push_back(
                        std::chrono::duration<double, std::micro>(Clock::now() - start).count());
                    if (flat_view) {
                        inserted[operation] += rows_inserted - before;
                        resets[operation] += model_resets - before_resets;
                    }
                };
                timed("tab_knight_events", [&] { tabs->setCurrentIndex(1); }, true, false);
                timed("tab_mage_events", [&] { tabs->setCurrentIndex(2); }, true, false);
                timed("tab_all_events", [&] { tabs->setCurrentIndex(0); }, true);
                expect(ids(*dialog, false).size() == static_cast<std::size_t>(count),
                       "returning to All synchronously restores every generated definition");
                for (const auto* query : {"Owned 000", "Owned 0000", "Owned 00000"}) {
                    timed(std::string("type_") + query, [&] { search->setText(query); });
                    const int expected = std::min(count,
                                                  query == std::string_view("Owned 000")    ? 1000
                                                  : query == std::string_view("Owned 0000") ? 100
                                                                                            : 10);
                    expect(ids(*dialog, false).size() == static_cast<std::size_t>(expected),
                           "each successive keystroke filters immediately without event dispatch");
                }
                search->clear();
                select_sixteen(*flat);
                timed("search_restore16", [&] { search->setText("Owned"); });
                expect(ids(*dialog, false).size() == static_cast<std::size_t>(count),
                       "restoration observation retains the complete visible catalog");
            }
            for (auto& [operation, values] : samples) {
                std::sort(values.begin(), values.end());
                std::cout << count << ',' << operation << ',' << values.size() << ',' << std::fixed
                          << std::setprecision(3) << values[values.size() / 2] << ','
                          << values[static_cast<std::size_t>(std::ceil(.95 * values.size())) - 1]
                          << ',';
                if (operation == "open_show_events" || operation == "tab_knight_events" ||
                    operation == "tab_mage_events") {
                    std::cout << "not_observed,not_observed";
                } else {
                    std::cout << inserted[operation] / values.size() << ','
                              << resets[operation] / values.size();
                }
                std::cout << '\n';
            }
        }
        std::cout << "rowsInserted and modelReset count flat-model signals after opening only; "
                     "they do not measure item objects or painting work. Opening and grade models "
                     "are measured separately with the gcov work probe\n";
    }
}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc > 2 && QString::fromUtf8(argv[1]) == "--render-proof") {
        render_proof(argv[2]);
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && QString::fromUtf8(argv[1]) == "--benchmark") {
        benchmark();
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    if (argc > 3 && QString::fromUtf8(argv[1]) == "--picker-work-probe") {
        work_probe(QString::fromUtf8(argv[2]).toInt(), QString::fromUtf8(argv[3]));
        return openkal::test::g_failures == 0 ? 0 : 1;
    }
    Fixture fixture;
    catalog_and_filters(fixture);
    pointer_selection(fixture);
    invalid_view_rows(fixture);
    hidden_selection_refresh(fixture);
    selections_and_tiles(fixture);
    live_reload_order();
    scrolled_selection();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
