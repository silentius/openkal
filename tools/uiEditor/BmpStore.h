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

#include <QPixmap>
#include <string>
#include <unordered_map>
#include <vector>

/** Magenta-keyed UI BMP cache for the Qt editor. */
class BmpStore {
  public:
    /**
     * Sets search directories (typically `data/UI` then `data/HyperText`).
     * @param dirs Directories to search, in order.
     */
    void set_search_dirs(std::vector<std::string> dirs);

    /**
     * Loads a UI bitmap with Kal magenta / near-black color keys.
     * @param filename BMP file name.
     * @return Pixmap, or null when missing.
     */
    [[nodiscard]] QPixmap pixmap(const std::string& filename);

    /**
     * @param filename BMP file name.
     * @return Native width, or 0.
     */
    [[nodiscard]] int width(const std::string& filename);

    /**
     * @param filename BMP file name.
     * @return Native height, or 0.
     */
    [[nodiscard]] int height(const std::string& filename);

  private:
    /** Resolves a file name against search dirs.
     * @param filename BMP file name.
     * @return Full path, or empty.
     */
    [[nodiscard]] std::string resolve(const std::string& filename) const;

    /** Search directories. */
    std::vector<std::string> m_dirs;

    /** Loaded pixmaps keyed by file name. */
    std::unordered_map<std::string, QPixmap> m_cache;
};
