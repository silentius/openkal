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

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace openkal::content_editor {

    /** Runtime JSON catalogs available in the editor. */
    enum class Kind { Npc, Monster, Item, Quest, Dialog, Shop, Task, DropGroup, ItemGroup, Prefix };

    /** Identifies the location of a JSON value, with decimal indices for array elements. */
    using JsonPath = QStringList;

    /** A loose JSON definition and the version read from disk. */
    struct Record {
        Kind kind;
        QString path;
        QJsonObject object;
        QByteArray original;
        bool writable = true;
    };

    /** A reference to a definition in another catalog. */
    struct Reference {
        Kind kind;
        QString id;
        JsonPath path;
        JsonPath flag_path;
    };

    /** @return Catalogs in display order. */
    [[nodiscard]] QVector<Kind> kinds();

    /** @param kind Catalog category.
     * @return Display name of the category.
     */
    [[nodiscard]] QString label(Kind kind);

    /** @param kind Catalog category.
     * @return Directory relative to the loose configuration root.
     */
    [[nodiscard]] QString directory(Kind kind);

    /** @param kind Catalog category.
     * @param object Definition object.
     * @return Stable identifier, including the flag for a quest step, or an empty string.
     */
    [[nodiscard]] QString identity(Kind kind, const QJsonObject& object);

    /** @param record Definition to describe.
     * @return Identifier and any available display name.
     */
    [[nodiscard]] QString title(const Record& record);

    /** @param kind Catalog category.
     * @param id Identifier typed by the user.
     * @param flag Quest step number, when applicable.
     * @return Initial fields for a new definition.
     */
    [[nodiscard]] QJsonObject initial_object(Kind kind, const QString& id, int flag = 0);

    /** @param object JSON definition to format.
     * @return UTF-8 JSON with compact numeric arrays.
     */
    [[nodiscard]] QByteArray serialize(const QJsonObject& object);

    /** @param value JSON object or array to traverse.
     * @param path Nested keys and array indices.
     * @return Value at the path or Undefined.
     */
    [[nodiscard]] QJsonValue value_at(QJsonValue value, const JsonPath& path);

    /** @param value Root value to update.
     * @param path Nested keys and array indices.
     * @param replacement New value, or Undefined to remove the entry.
     * @return Updated root.
     */
    [[nodiscard]] QJsonValue
    replace_at(QJsonValue value, const JsonPath& path, const QJsonValue& replacement);

    /** @param record Definition to inspect.
     * @return Catalog references in supported OpenKal fields.
     */
    [[nodiscard]] QVector<Reference> references(const Record& record);

    /** Reads and updates individual definitions from loose configuration directories. */
    class Catalog {
      public:
        /** Loads editable files and optional read-only definitions.
         * @param root Writable configuration directory.
         * @param base Optional fallback configuration directory.
         * @param archive Optional configuration path inside the mounted runtime archive.
         * @return Parse and duplicate diagnostics.
         */
        [[nodiscard]] QStringList
        load(const QString& root, const QString& base = {}, const QString& archive = {});

        /** @return Writable configuration directory. */
        [[nodiscard]] const QString& root() const;

        /** @return Loaded definitions. */
        [[nodiscard]] const QVector<Record>& records() const;

        /** @param kind Target category.
         * @param id Identifier or quest identifier with optional flag.
         * @return First matching loaded definition, or nullptr.
         */
        [[nodiscard]] const Record* find(Kind kind, const QString& id) const;

        /** @param definition NPC draft, possibly with a relative placement.
         * @return Effective definition, or no value if its anchor is missing or invalid.
         */
        [[nodiscard]] std::optional<QJsonObject> resolve_npc(const QJsonObject& definition) const;

        /** @param kind Category of the new record.
         * @param object Initial definition.
         * @param subdirectory Optional relative subdirectory within the items catalog.
         * @return Draft with its eventual file path.
         */
        [[nodiscard]] Record
        draft(Kind kind, QJsonObject object, const QString& subdirectory = {}) const;

        /** @param record Definition in its proposed state.
         * @return Validation error, or an empty string on success.
         */
        [[nodiscard]] QString validate(const Record& record) const;

        /** @param record Definition in its proposed state.
         * @return Labels for newly introduced references without a loaded target.
         */
        [[nodiscard]] QStringList unresolved(const Record& record) const;

        /** @param record Definition to search for.
         * @return Labels and field paths of loaded definitions referring to the target.
         */
        [[nodiscard]] QStringList usages(const Record& record) const;

        /** Atomically saves one changed definition after checking the on-disk version.
         * @param record Definition to write.
         * @return Error message, or an empty string on success.
         */
        [[nodiscard]] QString save(const Record& record);

        /** Removes one definition after checking references and the on-disk version.
         * @param record Loaded definition to remove.
         * @return Error message, or an empty string on success.
         */
        [[nodiscard]] QString remove(const Record& record);

      private:
        QString m_root;
        QVector<Record> m_records;
    };

}  // namespace openkal::content_editor
