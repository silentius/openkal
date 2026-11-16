/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "model/ItemIconCache.h"
#include "model/ItemPickerListModel.h"
#include "model/LootDataStore.h"
#include "test_util.h"

#include <QApplication>
#include <QBrush>
#include <QImage>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>

namespace {
    using openkal::test::expect;

    struct Fixture {
        QTemporaryDir temporary;
        std::filesystem::path root = temporary.path().toStdString();
        LootDataStore store;

        Fixture() {
            for (const char* directory : {"config/items",
                                          "config/groups",
                                          "config/itemgroups",
                                          "config/monsters",
                                          "HyperText"}) {
                std::filesystem::create_directories(root / directory);
            }
            write("config/groups/1.json", R"({"index":1,"entries":[]})");
            write("config/itemgroups/1.json", R"({"index":1,"entries":[]})");
            item(101, "initial");
            item(102, "Removed");
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(QColor(220, 40, 20));
            expect(image.save(QString::fromStdString((root / "HyperText/owned.bmp").string())),
                   "write the model-test icon");
            reload();
        }

        void write(const std::filesystem::path& relative, const std::string& text) {
            std::ofstream output(root / relative);
            output << text;
            expect(output.good(), "write the owned model catalog");
        }

        void item(int index, const char* name, bool overlay = false) {
            write("config/items/" + std::to_string(index) + ".json",
                  "{\"index\":" + std::to_string(index) + ",\"name\":\"" + name +
                      "\",\"class\":[\"general\",\"charm\"],\"image\":\"owned\"" +
                      (overlay ? ",\"specialty\":{\"changeprefix\":[\"weapon\", 20, 500, 60, 501]}"
                               : "") +
                      "}");
        }

        void reload() {
            LootEditorPaths paths;
            paths.repo_root = root.string();
            paths.loot_config_root = (root / "config").string();
            paths.data_root = root.string();
            expect(store.load(paths), "load the owned model catalog");
        }
    };

    class RoleCountingModel final : public QAbstractListModel {
      public:
        int rowCount(const QModelIndex& parent = {}) const override {
            return parent.isValid() ? 0 : 1;
        }

        QVariant data(const QModelIndex&, int) const override {
            ++m_calls;
            return {};
        }

        int calls() const {
            return m_calls;
        }

      private:
        mutable int m_calls = 0;
    };

    void lazy_roles_and_reload() {
        Fixture fixture;
        ItemIconCache icons(fixture.store.items());
        const std::unordered_set<int> occupied{101};
        ItemPickerListModel model(fixture.store, icons, occupied);
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        model.replace_items({101, 102});
        expect(model.rowCount() == 2 && model.index(0, 0).data(Qt::UserRole).toInt() == 101 &&
                   model.index(1, 0).data(Qt::UserRole).toInt() == 102,
               "model rows retain the supplied ID order without requiring appearance roles");

        // No view or appearance request exists before the underlying catalog is replaced.
        fixture.item(101, "Before access");
        std::filesystem::remove(fixture.root / "config/items/102.json");
        fixture.reload();
        const QModelIndex first = model.index(0, 0);
        const QModelIndex removed = model.index(1, 0);
        const QString label = first.data(Qt::DisplayRole).toString();
        const QString tooltip = first.data(Qt::ToolTipRole).toString();
        const QImage icon = first.data(Qt::DecorationRole).value<QPixmap>().toImage();
        expect(label == QString::fromUtf8("● Before acc…"),
               "first appearance access resolves the current catalog and retains truncation");
        expect(tooltip == fixture.store.item_tooltip(101) + "\n\nAlready in this group" &&
                   !icon.isNull(),
               "lazy tooltip and occupied icon use valid current session data");
        expect(removed.data(Qt::DisplayRole).toString() == fixture.store.item_name(102) &&
                   removed.data(Qt::ToolTipRole).toString() == fixture.store.item_tooltip(102) &&
                   !removed.data(Qt::DecorationRole).value<QPixmap>().isNull(),
               "a definition deleted before first access has safe current-store fallbacks");

        fixture.item(101, "After cache", true);
        fixture.reload();
        expect(first.data(Qt::DisplayRole).toString() == label &&
                   first.data(Qt::ToolTipRole).toString() == tooltip &&
                   first.data(Qt::DecorationRole).value<QPixmap>().toImage() == icon,
               "resolved appearance roles remain stable until the explicit refresh boundary");
        QPersistentModelIndex old_first(first);
        model.replace_items({101});
        const QModelIndex refreshed = model.index(0, 0);
        expect(resets.count() == 2 && !old_first.isValid() && model.rowCount() == 1,
               "replacement resets indexes and removes obsolete model rows");
        expect(refreshed.data(Qt::DisplayRole).toString() == QString::fromUtf8("● After cache") &&
                   refreshed.data(Qt::ToolTipRole).toString() ==
                       fixture.store.item_tooltip(101) + "\n\nAlready in this group" &&
                   refreshed.data(Qt::DecorationRole).value<QPixmap>().toImage() != icon,
               "explicit refresh resolves new labels, tooltips and overlay composition");
        expect(refreshed.data(Qt::SizeHintRole).toSize() == QSize(88, 96) &&
                   refreshed.data(Qt::TextAlignmentRole).toInt() ==
                       static_cast<int>(Qt::AlignHCenter | Qt::AlignTop) &&
                   refreshed.data(Qt::BackgroundRole).value<QBrush>().color() ==
                       QColor(120, 190, 255) &&
                   refreshed.data(Qt::ForegroundRole).value<QBrush>().color() == QColor(10, 40, 90),
               "read-only layout and occupied brush roles retain the established tile contract");
        expect(model.rowCount(refreshed) == 0 &&
                   model.flags(refreshed) == (Qt::ItemIsEnabled | Qt::ItemIsSelectable) &&
                   !model.setData(refreshed, "unexpected", Qt::DisplayRole) &&
                   !refreshed.data(Qt::UserRole + 17).isValid(),
               "the model is a read-only flat selection list with no unsupported roles");

        ItemPickerListModel other(fixture.store, icons, occupied);
        other.replace_items({101});
        expect(!model.data(other.index(0, 0)).isValid() &&
                   model.flags(other.index(0, 0)) == Qt::NoItemFlags,
               "foreign model indexes cannot resolve rows in this model");
        model.replace_items({});
        expect(model.rowCount() == 0 && !model.data(refreshed).isValid() &&
                   !model.data(QModelIndex()).isValid() &&
                   model.flags(refreshed) == Qt::NoItemFlags,
               "empty replacement safely rejects stale and invalid indexes");
    }

    void fixed_sizing_is_lazy() {
        RoleCountingModel model;
        ItemPickerTileDelegate delegate;
        QStyleOptionViewItem options;
        expect(delegate.sizeHint(options, model.index(0, 0)) == QSize(88, 96) && model.calls() == 0,
               "tile layout preserves its fixed dimensions without resolving any model role");
    }
}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    lazy_roles_and_reload();
    fixed_sizing_is_lazy();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
