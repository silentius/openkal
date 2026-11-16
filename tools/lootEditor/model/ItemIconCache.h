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

#include "openkal/client/ItemDb.h"

#include <QPixmap>
#include <unordered_map>
#include <unordered_set>

/** Loads and caches packaged-runtime and client HyperText item icons as QPixmaps. */
class ItemIconCache {
  public:
    /** Constructs an empty cache bound to an item catalog.
     * @param catalog Item catalog (must have loaded HyperText path).
     */
    explicit ItemIconCache(const openkal::client::ItemDb& catalog);

    /** Returns a 32×32 icon for an item index.
     * @param item_index item definition index.
     * @return Icon pixmap, or a placeholder when missing.
     */
    [[nodiscard]] QPixmap icon(int item_index) const;

    /** Returns a money placeholder icon.
     * @return Coin pixmap.
     */
    [[nodiscard]] QPixmap money_icon() const;

    /** Reports whether HyperText icons are available.
     * @return True when the catalog resolved at least one BMP.
     */
    [[nodiscard]] bool hypertext_ready() const {
        return m_hypertext_ready;
    }

    /** Enables verbose icon path logging to stderr (also set OPENKAL_LOOT_ICON_DEBUG=1).
     * @param enabled True to log every load attempt.
     */
    static void set_debug_verbose(bool enabled);

    /** @return True when verbose icon debug logging is active. */
    [[nodiscard]] static bool debug_verbose();

  private:
    /** Logs catalog/HyperText setup once per process.
     * @param catalog Item catalog used for icon resolution.
     */
    static void log_catalog_once(const openkal::client::ItemDb& catalog);

    /** Loads a BMP from the packaged runtime or catalog's HyperText directory.
     * @param item_index item definition index.
     * @return Pixmap, or null when not found.
     */
    [[nodiscard]] QPixmap load_icon(int item_index) const;

    /** Item catalog reference. */
    const openkal::client::ItemDb& m_catalog;

    /** Cached pixmaps by item index. */
    mutable std::unordered_map<int, QPixmap> m_cache;

    /** Shared money placeholder. */
    mutable QPixmap m_money;

    /** True after at least one HyperText BMP loaded successfully. */
    mutable bool m_hypertext_ready = false;

    /** Item indices whose failed load was already logged. */
    mutable std::unordered_set<int> m_logged_failures;

    /** Verbose logging override (env var takes precedence when unset). */
    static bool s_debug_verbose;
};
