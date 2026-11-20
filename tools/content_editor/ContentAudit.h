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

namespace openkal::content_editor {

    /** Groups shown in the content reference audit. */
    enum class ContentFindingKind { PossibleOrphan, NoKnownRoute, MissingTarget };

    /** One finding with a definition and an optional field to open. */
    struct ContentFinding {
        ContentFindingKind kind;
        QString definition;
        QString detail;
        QString record_path;
        JsonPath field;
    };

    /** Structural checks of the loaded content definitions. */
    struct ContentAudit {
        QHash<QString, QString> orphan_reasons;
        QVector<ContentFinding> findings;
    };

    /** @param kind Catalog category.
     * @return Whether the reference audit can check entries in this category for known uses.
     */
    [[nodiscard]] bool supports_orphan_audit(Kind kind);

    /** @param catalog Current saved definitions, including loaded fallback records.
     * @return Potential orphans, definitions without a known entry or loot source, and missing targets.
     */
    [[nodiscard]] ContentAudit audit_content_references(const Catalog& catalog);

}  // namespace openkal::content_editor
