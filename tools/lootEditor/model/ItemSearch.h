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

#include "model/LootDataStore.h"

#include <QString>
#include <algorithm>
#include <vector>

/** Finds item definition indices matching a name substring or exact index text.
 * @param store Session store.
 * @param text Search text.
 * @return Matching indices sorted ascending (may be empty).
 */
[[nodiscard]] inline std::vector<int> find_items_matching_search(const LootDataStore& store,
                                                                 const QString& text) {
    std::vector<int> matches;
    const QString needle = text.trimmed();
    if (needle.isEmpty()) {
        return matches;
    }

    bool ok = false;
    const int as_index = needle.toInt(&ok);
    if (ok && as_index > 0 && store.items().find(as_index) != nullptr) {
        matches.push_back(as_index);
        return matches;
    }

    const QString lower = needle.toLower();
    for (const auto& [index, def] : store.items().by_index()) {
        (void) def;
        const QString name = store.item_name(index).toLower();
        if (name.contains(lower)) {
            matches.push_back(index);
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}
