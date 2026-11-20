/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ContentAudit.h"

#include "openkal/server/world/ItemGroupDb.h"

#include <QJsonArray>
#include <QSet>

namespace openkal::content_editor {
    namespace {
        bool quest_flow(Kind kind) {
            return kind == Kind::Quest || kind == Kind::Dialog;
        }

        QString quest_id(const QString& key) {
            return key.section(':', 0, 0);
        }

        bool active_monster_drop(const Record& monster, const JsonPath& path) {
            const auto row = value_at(monster.object, path.mid(0, path.size() - 1)).toArray();
            if (path.value(1) == "itemGroups") {
                return row.size() >= 2 && row.at(1).toInteger() > 0;
            }
            return row.size() >= 4 && row.at(0).toInteger() > 0 && row.at(2).toInteger() > 0 &&
                   row.at(3).toInteger() > 0;
        }

        bool audit_reference(const Record& record, const Reference& ref, bool quest_action) {
            if (quest_flow(ref.kind)) {
                return true;
            }
            if (ref.id.toLongLong() <= 0) {
                return false;
            }
            switch (record.kind) {
            case Kind::Monster:
                return ref.kind == Kind::ItemGroup;
            case Kind::Quest:
                return quest_action && ref.kind == Kind::ItemGroup;
            case Kind::ItemGroup: {
                const auto row = value_at(record.object, ref.path.mid(0, 2)).toObject();
                return ref.kind == Kind::DropGroup && row.value("chance").toDouble() > 0;
            }
            case Kind::Task:
                return ref.kind == Kind::Npc ||
                       (ref.kind == Kind::Item &&
                        value_at(record.object, ref.path.mid(0, 3)).toArray().at(2).toInteger() >
                            0);
            case Kind::Shop:
                return ref.kind == Kind::Npc ||
                       (ref.kind == Kind::Item && ref.path.value(0) != "excludeItems");
            case Kind::DropGroup: {
                const auto row = value_at(record.object, ref.path.mid(0, 2)).toObject();
                return (ref.kind == Kind::Item || ref.kind == Kind::Prefix) &&
                       row.value("type") != "money" && row.value("chance").toDouble() > 0;
            }
            default:
                return false;
            }
        }
    }  // namespace

    bool supports_orphan_audit(Kind kind) {
        return quest_flow(kind) || kind == Kind::ItemGroup || kind == Kind::DropGroup ||
               kind == Kind::Task || kind == Kind::Shop;
    }

    ContentAudit audit_content_references(const Catalog& catalog) {
        ContentAudit audit;
        const auto& records = catalog.records();
        const auto quest_names = catalog.quest_names();
        const auto definition = [&](const Record& record) {
            const auto name = record.kind == Kind::Quest
                                  ? quest_names.value(record.object.value("index").toInt())
                                  : QString{};
            return label(record.kind) + " / " + title(record, name);
        };

        QHash<QPair<Kind, QString>, qsizetype> definitions;
        QVector<QString> ids(records.size());
        QSet<QString> known_quest_ids;
        for (qsizetype row = 0; row < records.size(); ++row) {
            const auto& record = records.at(row);
            const auto id = identity(record.kind, record.object);
            const auto key = qMakePair(record.kind, id);
            if (!id.isEmpty() && !definitions.contains(key)) {
                definitions.insert(key, row);
                ids[row] = id;
            }
            if (record.kind == Kind::Quest) {
                known_quest_ids.insert(quest_id(id));
            }
        }

        QVector<QVector<qsizetype>> successors(records.size());
        QVector<qsizetype> pending;
        QSet<QString> assigned_quests;
        QSet<qsizetype> incoming_references;
        const auto global_group =
            qMakePair(Kind::ItemGroup, QString::number(server::kGlobalNormalMonsterItemGroupIndex));
        if (const auto global = definitions.constFind(global_group); global != definitions.cend()) {
            incoming_references.insert(*global);
        }
        for (qsizetype row = 0; row < records.size(); ++row) {
            const auto& record = records.at(row);
            if (ids.at(row).isEmpty() ||
                (!supports_orphan_audit(record.kind) && record.kind != Kind::Npc &&
                 record.kind != Kind::Monster)) {
                continue;
            }
            if (record.kind == Kind::Task) {
                assigned_quests.insert(ids.at(row));
            }
            const auto source_quest =
                record.kind == Kind::Quest ? quest_id(ids.at(row)) : QString{};
            for (const auto& ref : references(record)) {
                const bool quest_action = record.kind == Kind::Quest && ref.path.value(2) == "then";
                if (!audit_reference(record, ref, quest_action) ||
                    (record.kind == Kind::Monster && !active_monster_drop(record, ref.path))) {
                    continue;
                }
                const auto operation = quest_action
                                           ? value_at(record.object, ref.path.mid(0, 4)).toObject()
                                           : QJsonObject{};
                if (operation.value("raw").isArray()) {
                    continue;
                }
                if (ref.kind == Kind::Quest) {
                    if (((record.kind == Kind::Npc || record.kind == Kind::Dialog) &&
                         !ref.id.contains(':')) ||
                        (record.kind == Kind::Npc && ref.id == "0:0")) {
                        continue;
                    }
                    const auto family = quest_id(ref.id);
                    known_quest_ids.insert(family);
                    if (family != source_quest) {
                        assigned_quests.insert(family);
                    }
                    if (record.kind == Kind::Monster) {
                        continue;
                    }
                } else if (ref.id == "0") {
                    continue;
                }
                auto target_id = ref.id;
                if (record.kind == Kind::Quest && quest_flow(ref.kind)) {
                    if (operation.value("op") != "link" && operation.value("op") != "html") {
                        continue;
                    }
                    if (ref.kind == Kind::Quest && !target_id.contains(':')) {
                        target_id += ":0";
                    }
                }
                const auto key = qMakePair(ref.kind, target_id);
                const auto target = definitions.constFind(key);
                if (target == definitions.cend()) {
                    audit.findings.append(
                        {ContentFindingKind::MissingTarget,
                         definition(record),
                         label(ref.kind) + " / " + target_id + " · " + ref.path.join(" / "),
                         record.path,
                         ref.path});
                    continue;
                }
                if (quest_flow(ref.kind) || ref.kind == Kind::DropGroup) {
                    successors[row].append(*target);
                }
                if (record.kind == Kind::Npc) {
                    pending.append(*target);
                }
                if (*target != row) {
                    incoming_references.insert(*target);
                }
            }
        }

        QSet<qsizetype> reachable;
        QSet<QString> reachable_quests;
        while (!pending.isEmpty()) {
            const auto row = pending.takeLast();
            if (reachable.contains(row)) {
                continue;
            }
            reachable.insert(row);
            if (records.at(row).kind == Kind::Quest) {
                reachable_quests.insert(quest_id(ids.at(row)));
            }
            pending.append(successors.at(row));
        }

        QSet<qsizetype> used_drop_groups;
        for (const auto row : incoming_references) {
            if (records.at(row).kind == Kind::ItemGroup) {
                for (const auto group : successors.at(row)) {
                    used_drop_groups.insert(group);
                }
            }
        }

        for (qsizetype row = 0; row < records.size(); ++row) {
            const auto& record = records.at(row);
            const auto& id = ids.at(row);
            if (id.isEmpty() || !supports_orphan_audit(record.kind)) {
                continue;
            }
            if (quest_flow(record.kind) && !reachable.contains(row)) {
                audit.findings.append({ContentFindingKind::NoKnownRoute,
                                       definition(record),
                                       "No path from a known NPC entry",
                                       record.path,
                                       {}});
            } else if (record.kind == Kind::DropGroup && incoming_references.contains(row) &&
                       !used_drop_groups.contains(row)) {
                audit.findings.append({ContentFindingKind::NoKnownRoute,
                                       definition(record),
                                       "Referenced only by item groups without a known loot source",
                                       record.path,
                                       {}});
            }

            QString reason;
            switch (record.kind) {
            case Kind::Quest: {
                const auto family = quest_id(id);
                if (!reachable_quests.contains(family) && !assigned_quests.contains(family)) {
                    reason = "No known NPC route or external reference to this quest.";
                }
                break;
            }
            case Kind::Dialog:
                if (!reachable.contains(row) && !incoming_references.contains(row)) {
                    reason = "No incoming reference from a known definition.";
                }
                break;
            case Kind::ItemGroup:
                if (!incoming_references.contains(row)) {
                    reason = "No known monster drop or quest reward uses this item group.";
                }
                break;
            case Kind::DropGroup:
                if (!incoming_references.contains(row)) {
                    reason = "No item group uses this drop group.";
                }
                break;
            case Kind::Task:
                if (!known_quest_ids.contains(id)) {
                    reason = "No known quest uses this journal key.";
                }
                break;
            case Kind::Shop:
                if (!definitions.contains(qMakePair(Kind::Npc, id))) {
                    reason = "No loaded NPC matches this shop's NPC ID.";
                }
                break;
            default:
                break;
            }
            if (!reason.isEmpty()) {
                audit.orphan_reasons.insert(record.path, reason);
                audit.findings.append({ContentFindingKind::PossibleOrphan,
                                       definition(record),
                                       reason,
                                       record.path,
                                       {}});
            }
        }
        return audit;
    }

}  // namespace openkal::content_editor
