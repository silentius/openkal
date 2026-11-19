/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Catalog.h"

#include "openkal/map/Constants.h"
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/NpcDefinitions.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace openkal::content_editor {
    namespace {
        struct CatalogType {
            const char* label;
            const char* folder;
            const char* key;
        };

        constexpr std::array<CatalogType, 10> kTypes{{
            {"NPCs", "npcs", "index"},
            {"Monsters", "monsters", "index"},
            {"Items", "items", "index"},
            {"Quests", "quests", "index"},
            {"Dialogs", "dialogs", "id"},
            {"Shops", "server/shops", "npcIndex"},
            {"Tasks", "tasks", "key"},
            {"Drop groups", "groups", "index"},
            {"Item groups", "itemgroups", "index"},
            {"Prefixes", "prefixes", "index"},
        }};

        const CatalogType& type(Kind kind) {
            return kTypes[static_cast<std::size_t>(kind)];
        }

        QString id_string(const QJsonValue& value) {
            const auto number = value.toInteger(-1);
            if (number >= 0) {
                return QString::number(number);
            }
            return value.toString().trimmed();
        }

        QString padded(const QString& id, int width) {
            return id.rightJustified(width, '0');
        }

        void add_ref(QVector<Reference>& refs,
                     const Record& record,
                     Kind kind,
                     JsonPath path,
                     const JsonPath& flag = {}) {
            const QString id = id_string(value_at(record.object, path));
            if (!id.isEmpty()) {
                const auto step =
                    flag.isEmpty() ? QString{} : id_string(value_at(record.object, flag));
                refs.append({kind, step.isEmpty() ? id : id + ':' + step, std::move(path), flag});
            }
        }

        void array_refs(QVector<Reference>& refs,
                        const Record& record,
                        Kind kind,
                        const JsonPath& path,
                        const QStringList& members = {}) {
            const int count = static_cast<int>(value_at(record.object, path).toArray().size());
            for (int i = 0; i < count; ++i) {
                auto key = path;
                key += QString::number(i);
                key += members;
                add_ref(refs, record, kind, key);
            }
        }

        bool matches(Kind kind, const QString& key, const QString& query) {
            return key == query ||
                   (kind == Kind::Quest && !query.contains(':') && key.startsWith(query + ':'));
        }

        bool positive_int(const QJsonValue& value) {
            const auto number = value.toInteger(0);
            return number > 0 && number <= std::numeric_limits<int>::max();
        }

        bool valid_drop_rows(const QJsonValue& value) {
            if (!value.isArray()) {
                return false;
            }
            const auto rows = value.toArray();
            return std::all_of(rows.begin(), rows.end(), [](const QJsonValue& entry) {
                const auto row = entry.toArray();
                return row.size() >= 2 && positive_int(row.at(0)) && positive_int(row.at(1));
            });
        }

        QString changed_on_disk(const Record& record) {
            const QFileInfo info(record.path);
            if (record.original.isEmpty()) {
                return (info.exists() || info.isSymLink())
                           ? QString("The target file already exists. Reload the catalogs first.")
                           : QString{};
            }
            QFile file(record.path);
            if (info.isSymLink() || !file.open(QIODevice::ReadOnly) ||
                file.readAll() != record.original) {
                return "The file changed on disk. Reload the catalogs before editing it.";
            }
            return {};
        }
    }  // namespace

    QVector<Kind> kinds() {
        return {Kind::Npc,
                Kind::Monster,
                Kind::Item,
                Kind::Quest,
                Kind::Dialog,
                Kind::Shop,
                Kind::Task,
                Kind::DropGroup,
                Kind::ItemGroup,
                Kind::Prefix};
    }

    QString label(Kind kind) {
        return QString::fromLatin1(type(kind).label);
    }

    QString directory(Kind kind) {
        return QString::fromLatin1(type(kind).folder);
    }

    QString identity(Kind kind, const QJsonObject& object) {
        const auto value = object.value(type(kind).key);
        const QString id = value.toInteger(-1) < 0 ? QString{} : id_string(value);
        if (kind == Kind::Quest && !id.isEmpty()) {
            const auto flag = object.value("flag").toInteger(-1);
            return flag < 0 ? QString{} : id + ':' + QString::number(flag);
        }
        return id;
    }

    QString title(const Record& record) {
        const auto name =
            record.object.value(record.kind == Kind::Dialog ? "caption" : "name").toString();
        const auto id = identity(record.kind, record.object);
        return name.isEmpty() ? id : id + "  " + name;
    }

    QJsonObject initial_object(Kind kind, const QString& id, int flag) {
        QJsonObject object;
        object[type(kind).key] = id.toInt();
        switch (kind) {
        case Kind::Npc:
            object["name"] = "";
            object["kind"] = 3;
            object["shape"] = 0;
            object["map"] = 0;
            object["html"] = 0;
            object["teleports"] = QJsonArray();
            break;
        case Kind::Monster:
            object["name"] = "";
            object["stats"] = QJsonObject{{"level", 1},
                                          {"hp", 1},
                                          {"exp", 0},
                                          {"itemGroups", QJsonArray()},
                                          {"quests", QJsonArray()}};
            object["spawns"] = QJsonArray();
            break;
        case Kind::Item:
            object["name"] = "";
            object["description"] = "";
            object["image"] = "";
            object["class"] = QJsonArray{"general", "etc"};
            object["stackable"] = 1;
            break;
        case Kind::Quest:
            object["flag"] = flag;
            object["linked"] = false;
            object["cases"] = QJsonArray{QJsonObject{{"if", QJsonArray()}, {"then", QJsonArray()}}};
            break;
        case Kind::Dialog:
            object["caption"] = "";
            object["lines"] = QJsonArray();
            object["links"] = QJsonArray();
            break;
        case Kind::Shop:
            object["schema"] = "openkal.shop/v1";
            object["items"] = QJsonArray();
            break;
        case Kind::Task:
            object["name"] = "";
            object["desc"] = "";
            object["flags"] = QJsonArray();
            object["npcIndex"] = 0;
            break;
        case Kind::DropGroup:
        case Kind::ItemGroup:
            object["name"] = "";
            object["entries"] = QJsonArray();
            break;
        case Kind::Prefix:
            object["name"] = "";
            object["level"] = 1;
            object["sell"] = 0;
            break;
        }
        return object;
    }

    QByteArray serialize(const QJsonObject& object) {
        static const QRegularExpression numeric_array(
            R"(\[\r?\n\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?(?:\s*,\s*-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)*?)\s*\])");
        static const QRegularExpression comma(R"(\s*,\s*)");
        QString formatted = QString::fromUtf8(QJsonDocument(object).toJson());
        auto matches = numeric_array.globalMatch(formatted);
        int end = 0;
        QString result;
        while (matches.hasNext()) {
            const auto match = matches.next();
            result += formatted.mid(end, match.capturedStart() - end);
            result += '[' + match.captured(1).replace(comma, ", ") + ']';
            end = static_cast<int>(match.capturedEnd());
        }
        result += formatted.mid(end);
        return result.toUtf8();
    }

    QJsonValue value_at(QJsonValue value, const JsonPath& path) {
        for (const auto& part : path) {
            if (value.isArray()) {
                bool valid = false;
                const int index = part.toInt(&valid);
                value = valid ? value.toArray().at(index) : QJsonValue(QJsonValue::Undefined);
            } else {
                value = value.toObject().value(part);
            }
        }
        return value;
    }

    QJsonValue replace_at(QJsonValue value, const JsonPath& path, const QJsonValue& replacement) {
        if (path.isEmpty()) {
            return replacement;
        }
        const auto key = path.front();
        const auto tail = path.mid(1);
        if (value.isArray()) {
            auto array = value.toArray();
            bool valid = false;
            const int row = key.toInt(&valid);
            if (!valid || row < 0 || row >= array.size()) {
                return value;
            }
            const auto updated = replace_at(array.at(row), tail, replacement);
            if (updated.isUndefined()) {
                array.removeAt(row);
            } else {
                array.replace(row, updated);
            }
            return array;
        }
        auto object = value.toObject();
        object.insert(key, replace_at(object.value(key), tail, replacement));
        return object;
    }

    QVector<Reference> references(const Record& record) {
        QVector<Reference> result;
        const auto add = [&](Kind kind, const JsonPath& path, const JsonPath& flag = {}) {
            add_ref(result, record, kind, path, flag);
        };
        const auto rows = [&](Kind kind, const JsonPath& path, const QStringList& members = {}) {
            array_refs(result, record, kind, path, members);
        };
        switch (record.kind) {
        case Kind::Npc:
            add(Kind::Dialog, {"html"});
            add(Kind::Dialog, {"placement", "beside_dialog"});
            add(Kind::Quest, {"quest", "0"}, {"quest", "1"});
            add(Kind::Item, {"scenario", "1"});
            break;
        case Kind::Monster: {
            rows(Kind::ItemGroup, {"stats", "itemGroups"}, {"0"});
            const auto drops = value_at(record.object, {"stats", "quests"}).toArray();
            for (int i = 0; i < drops.size(); ++i) {
                auto path = JsonPath{"stats", "quests"};
                const bool nested = drops.at(i).isArray();
                if (nested) {
                    path += QString::number(i);
                }
                add(Kind::Quest, path + JsonPath{"0"}, path + JsonPath{"1"});
                add(Kind::ItemGroup, path + JsonPath{"2"});
                if (!nested) {
                    break;
                }
            }
            break;
        }
        case Kind::Quest: {
            const auto cases = record.object.value("cases").toArray();
            for (int i = 0; i < cases.size(); ++i) {
                for (const auto* branch : {"if", "then"}) {
                    auto path = JsonPath{"cases", QString::number(i), branch};
                    const auto ops = value_at(record.object, path).toArray();
                    for (int j = 0; j < ops.size(); ++j) {
                        const auto op = ops.at(j).toObject().value("op").toString();
                        const auto entry = path + JsonPath{QString::number(j)};
                        if (op == "quest" || op == "link" || op == "save" || op == "clear" ||
                            op == "notclear") {
                            add(Kind::Quest, entry + JsonPath{"quest"}, entry + JsonPath{"flag"});
                        } else if (op == "html") {
                            add(Kind::Dialog, entry + JsonPath{"id"});
                        } else if (op == "itemrand") {
                            add(Kind::ItemGroup, entry + JsonPath{"group"});
                        } else if (op == "item") {
                            for (const auto* field : {"items", "in", "out"}) {
                                rows(Kind::Item, entry + JsonPath{field}, {"0"});
                            }
                            rows(Kind::Prefix, entry + JsonPath{"in"}, {"1"});
                        }
                    }
                }
            }
            break;
        }
        case Kind::Shop:
            add(Kind::Npc, {"npcIndex"});
            add(Kind::Item, {"currencyItem"});
            rows(Kind::Item, {"items"});
            rows(Kind::Item, {"excludeItems"});
            break;
        case Kind::Task:
            add(Kind::Npc, {"npcIndex"});
            rows(Kind::Npc, {"flags"}, {"npcIndex"});
            rows(Kind::Item, {"flags"}, {"item", "1"});
            break;
        case Kind::DropGroup:
            rows(Kind::Item, {"entries"}, {"item"});
            rows(Kind::Prefix, {"entries"}, {"prefix"});
            break;
        case Kind::ItemGroup:
            rows(Kind::DropGroup, {"entries"}, {"group"});
            break;
        case Kind::Dialog: {
            const auto links = record.object.value("links").toArray();
            for (int i = 0; i < links.size(); ++i) {
                const JsonPath path{"links", QString::number(i), "quest"};
                add(Kind::Quest, path + JsonPath{"0"}, path + JsonPath{"1"});
            }
            break;
        }
        case Kind::Item:
        case Kind::Prefix:
            break;
        }
        return result;
    }

    QStringList Catalog::load(const QString& root, const QString& base, const QString& archive) {
        m_root = root.isEmpty() ? QString{} : QDir(root).absolutePath();
        m_records.clear();
        QStringList diagnostics;
        QSet<QString> paths;
        QSet<QString> identities;
        const auto base_root = base.isEmpty() ? QString{} : QDir(base).absolutePath();
        const auto archive_root = archive.isEmpty() ? QString{} : QDir(archive).absolutePath();
        for (const auto& current :
             {m_root, archive_root, base_root == m_root ? QString{} : base_root}) {
            if (current.isEmpty()) {
                continue;
            }
            const bool packed = current == archive_root;
            const bool writable = !packed && current == m_root;
            for (const auto kind : kinds()) {
                const QDir folder(QDir(current).filePath(directory(kind)));
                const bool recursive = kind == Kind::Item || kind == Kind::Shop;
                QStringList files;
                if (packed) {
                    const auto path = folder.absolutePath().toStdString();
                    const auto names = recursive
                                           ? platform::ConfigArchive::list_files_recursive(path)
                                           : platform::ConfigArchive::list_directory(path);
                    for (const auto& name : names) {
                        const auto filename = QString::fromStdString(name);
                        if (filename.endsWith(".json")) {
                            files.append(folder.filePath(filename));
                        }
                    }
                } else {
                    QDirIterator iterator(folder.absolutePath(),
                                          {"*.json"},
                                          QDir::Files | QDir::NoSymLinks,
                                          recursive ? QDirIterator::Subdirectories
                                                    : QDirIterator::NoIteratorFlags);
                    while (iterator.hasNext()) {
                        files.append(iterator.next());
                    }
                }
                for (const auto& path : files) {
                    const auto relative = QDir(current).relativeFilePath(path);
                    if (paths.contains(relative)) {
                        continue;
                    }
                    paths.insert(relative);
                    QByteArray contents;
                    if (packed) {
                        std::string error;
                        const auto bytes =
                            platform::ConfigArchive::read(path.toStdString(), &error);
                        if (!bytes) {
                            diagnostics.append(path + ": " + QString::fromStdString(error));
                            continue;
                        }
                        contents = QByteArray(reinterpret_cast<const char*>(bytes->data()),
                                              static_cast<qsizetype>(bytes->size()));
                    } else {
                        QFile file(path);
                        if (!file.open(QIODevice::ReadOnly)) {
                            diagnostics.append(path + ": " + file.errorString());
                            continue;
                        }
                        contents = file.readAll();
                    }
                    QJsonParseError error;
                    const auto doc = QJsonDocument::fromJson(contents, &error);
                    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                        diagnostics.append(path + ": Expected a JSON object (" +
                                           error.errorString() + ')');
                        continue;
                    }
                    const auto id = identity(kind, doc.object());
                    if (id.isEmpty()) {
                        continue;
                    }
                    const auto key = QString::number(static_cast<int>(kind)) + '/' + id;
                    if (identities.contains(key)) {
                        diagnostics.append("Duplicate " + label(kind) + " ID " + id + ": " + path);
                    }
                    identities.insert(key);
                    m_records.append({kind, path, doc.object(), contents, writable});
                }
            }
        }
        return diagnostics;
    }

    const QString& Catalog::root() const {
        return m_root;
    }

    const QVector<Record>& Catalog::records() const {
        return m_records;
    }

    const Record* Catalog::find(Kind kind, const QString& id) const {
        const auto result =
            std::find_if(m_records.begin(), m_records.end(), [&](const Record& record) {
                return record.kind == kind && matches(kind, identity(kind, record.object), id);
            });
        return result == m_records.end() ? nullptr : &*result;
    }

    std::optional<QJsonObject> Catalog::resolve_npc(const QJsonObject& definition) const {
        const auto placement = definition.value("placement");
        if (!placement.isObject()) {
            return definition;
        }
        const auto dialog = placement.toObject().value("beside_dialog").toInt();
        const Record* anchor = nullptr;
        int first = std::numeric_limits<int>::max();
        for (const auto& entry : m_records) {
            if (entry.kind == Kind::Npc && !entry.object.value("placement").isObject() &&
                entry.object.value("html").toInt() == dialog) {
                const int index = entry.object.value("index").toInt();
                if (index > 0 && index < first) {
                    first = index;
                    anchor = &entry;
                }
            }
        }
        if (!anchor) {
            return std::nullopt;
        }
        const auto anchor_json =
            platform::Json::parse(QJsonDocument(anchor->object).toJson().toStdString());
        const auto source_json =
            platform::Json::parse(QJsonDocument(definition).toJson().toStdString());
        const auto resolved = platform::resolve_npc_placement(source_json, &anchor_json);
        if (!resolved) {
            return std::nullopt;
        }
        return QJsonDocument::fromJson(
                   QByteArray::fromStdString(platform::Json::stringify(*resolved)))
            .object();
    }

    Record Catalog::draft(Kind kind, QJsonObject object, const QString& subdirectory) const {
        const auto id = identity(kind, object);
        QString filename = id;
        if (kind == Kind::Quest) {
            const auto parts = id.split(':');
            filename = padded(parts.value(0), 4) + '_' + padded(parts.value(1), 2);
        } else if (kind == Kind::Dialog) {
            filename = padded(id, 6);
        } else if (kind == Kind::DropGroup || kind == Kind::ItemGroup) {
            filename = padded(id, 4);
        }
        auto path = QDir(m_root).filePath(directory(kind));
        if (kind == Kind::Item && !subdirectory.isEmpty()) {
            path = QDir(path).filePath(subdirectory);
        }
        return {kind, QDir(path).filePath(filename + ".json"), std::move(object), {}, true};
    }

    QString Catalog::validate(const Record& record) const {
        const QString id = identity(record.kind, record.object);
        const auto index = record.object.value(type(record.kind).key).toInteger(-1);
        const int upper =
            record.kind == Kind::Quest || record.kind == Kind::Task || record.kind == Kind::Item
                ? 65535
                : 2147483647;
        if (id.isEmpty() || index < (record.kind == Kind::Quest ? 0 : 1) || index > upper ||
            (record.kind == Kind::Quest && record.object.value("flag").toInteger(-1) > 255)) {
            return "Enter a valid identifier.";
        }
        const auto existing = QJsonDocument::fromJson(record.original).object();
        if (!record.original.isEmpty() && id != identity(record.kind, existing)) {
            return "An existing definition's identifier cannot be renamed. Create a new definition and update its references.";
        }
        if (record.original.isEmpty() && find(record.kind, id)) {
            return "A definition with this identifier already exists.";
        }
        if (record.kind == Kind::Npc) {
            const auto xy = record.object.value("xy").toArray();
            if (xy.size() < 2 ||
                (record.object.value("xy") != existing.value("xy") &&
                 (xy.size() < 3 || xy.at(0).toInteger(-1) < 0 || xy.at(1).toInteger(-1) < 0 ||
                  xy.at(2).toInteger(std::numeric_limits<qint64>::lowest()) <
                      std::numeric_limits<int>::lowest() ||
                  xy.at(2).toInteger() > std::numeric_limits<int>::max()))) {
                return "Set an NPC position with X, Y and Z before saving.";
            }
        }
        if (record.kind == Kind::Monster) {
            const JsonPath path{"stats", "itemGroups"};
            const auto item_groups = value_at(record.object, path);
            if (!item_groups.isUndefined() && item_groups != value_at(existing, path) &&
                !valid_drop_rows(item_groups)) {
                return "Each monster drop row needs an item-group ID and a positive number of attempts.";
            }
        }
        if (record.kind == Kind::Monster &&
            record.object.value("spawns") != existing.value("spawns")) {
            if (!record.object.value("spawns").isArray()) {
                return "Monster spawns must be an array.";
            }
            for (const auto& entry : record.object.value("spawns").toArray()) {
                if (existing.value("spawns").toArray().contains(entry)) {
                    continue;
                }
                const auto spawn = entry.toObject();
                const auto rect = spawn.value("rect").toArray();
                if (rect.size() != 4 || spawn.value("map").toInteger(0) < 0 ||
                    spawn.value("max").toInteger(0) <= 0 ||
                    spawn.value("cycle").toInteger(0) <= 0) {
                    return "Each changed spawn needs a map, four rectangle coordinates, a positive count and a positive respawn time.";
                }
                for (const auto& coordinate : rect) {
                    if (coordinate.toInteger(-1) < 0 ||
                        coordinate.toInteger() >= map::kPatchGrid * map::kTileCount) {
                        return "Spawn coordinates must be tile indices inside the world.";
                    }
                }
            }
        }
        return {};
    }

    QStringList Catalog::unresolved(const Record& record) const {
        QStringList result;
        auto previous = record;
        previous.object = QJsonDocument::fromJson(record.original).object();
        const auto old = references(previous);
        for (const auto& ref : references(record)) {
            if (ref.id == "0" || ref.id.startsWith("0:")) {
                continue;
            }
            const bool existed = std::any_of(old.begin(), old.end(), [&](const Reference& entry) {
                return entry.kind == ref.kind && entry.id == ref.id;
            });
            if (!existed && !find(ref.kind, ref.id) &&
                !(record.kind == ref.kind &&
                  matches(ref.kind, identity(record.kind, record.object), ref.id))) {
                result.append(label(ref.kind) + " " + ref.id + " (" + ref.path.join(" / ") + ')');
            }
        }
        result.removeDuplicates();
        return result;
    }

    QStringList Catalog::usages(const Record& record) const {
        QStringList result;
        const auto key = identity(record.kind, record.object);
        for (const auto& candidate : m_records) {
            if (candidate.path == record.path) {
                continue;
            }
            for (const auto& ref : references(candidate)) {
                if (ref.kind != record.kind || !matches(record.kind, key, ref.id)) {
                    continue;
                }
                if (record.kind == Kind::Quest && !ref.id.contains(':') &&
                    std::any_of(m_records.begin(), m_records.end(), [&](const Record& step) {
                        return step.kind == Kind::Quest && step.path != record.path &&
                               matches(Kind::Quest, identity(step.kind, step.object), ref.id);
                    })) {
                    continue;
                }
                result.append(label(candidate.kind) + " / " + title(candidate) + " / " +
                              ref.path.join(" / "));
            }
        }
        return result;
    }

    QString Catalog::save(const Record& record) {
        if (m_root.isEmpty()) {
            return "Choose a loose configuration directory before saving.";
        }
        if (!record.writable) {
            return "This definition belongs to the read-only configuration.";
        }
        if (const auto error = validate(record); !error.isEmpty()) {
            return error;
        }
        if (const auto error = changed_on_disk(record); !error.isEmpty()) {
            return error;
        }
        const auto parent = QFileInfo(record.path).absolutePath();
        const auto relative = QDir(m_root).relativeFilePath(parent);
        if (relative.startsWith("../") || QDir::isAbsolutePath(relative)) {
            return "The file location is outside the editable configuration.";
        }
        QDir directory(m_root);
        for (const auto& part : relative.split('/', Qt::SkipEmptyParts)) {
            if (QFileInfo(directory.filePath(part)).isSymLink()) {
                return "Choose a catalog directory without symbolic links.";
            }
            directory.setPath(directory.filePath(part));
        }
        if (!QDir().mkpath(parent)) {
            return "Could not create the catalog directory.";
        }
        const auto data = serialize(record.object);
        QSaveFile file(record.path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            return file.errorString();
        }
        auto it = std::find_if(m_records.begin(), m_records.end(), [&](const Record& entry) {
            return entry.path == record.path;
        });
        auto updated = record;
        updated.original = data;
        if (it == m_records.end()) {
            m_records.append(std::move(updated));
        } else {
            *it = std::move(updated);
        }
        return {};
    }

    QString Catalog::remove(const Record& record) {
        if (!record.writable || record.original.isEmpty()) {
            return "Select an existing definition in the editable configuration.";
        }
        if (const auto refs = usages(record); !refs.isEmpty()) {
            return "Update these references before deleting the definition:\n" + refs.join('\n');
        }
        if (const auto error = changed_on_disk(record); !error.isEmpty()) {
            return error;
        }
        const auto canonical = QFileInfo(record.path).canonicalFilePath();
        const auto root = QFileInfo(m_root).canonicalFilePath();
        if (root.isEmpty() || !canonical.startsWith(root + '/')) {
            return "The file location is outside the editable configuration.";
        }
        QFile file(record.path);
        if (!file.remove()) {
            return file.errorString();
        }
        m_records.erase(
            std::remove_if(m_records.begin(),
                           m_records.end(),
                           [&](const Record& item) { return item.path == record.path; }),
            m_records.end());
        return {};
    }

}  // namespace openkal::content_editor
