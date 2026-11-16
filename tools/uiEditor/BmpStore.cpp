/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "BmpStore.h"

#include "openkal/client/WidgetTools.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/ImageRgba.h"

#include <QImage>

void BmpStore::set_search_dirs(std::vector<std::string> dirs) {
    m_dirs = std::move(dirs);
    m_cache.clear();
}

QPixmap BmpStore::pixmap(const std::string& filename) {
    if (filename.empty()) {
        return {};
    }
    const auto it = m_cache.find(filename);
    if (it != m_cache.end()) {
        return it->second;
    }
    const std::string path = resolve(filename);
    QPixmap px;
    if (!path.empty()) {
        openkal::terrain::ImageRgba img;
        if (openkal::client::WidgetTools::load_bmp_rgba(path, img) && img.width > 0 &&
            img.height > 0 && !img.rgba.empty()) {
            QImage q(
                img.rgba.data(), img.width, img.height, img.width * 4, QImage::Format_RGBA8888);
            px = QPixmap::fromImage(q.copy());
        }
    }
    m_cache[filename] = px;
    return px;
}

int BmpStore::width(const std::string& filename) {
    return pixmap(filename).width();
}

int BmpStore::height(const std::string& filename) {
    return pixmap(filename).height();
}

std::string BmpStore::resolve(const std::string& filename) const {
    for (const auto& dir : m_dirs) {
        if (dir.empty()) {
            continue;
        }
        const std::string direct = openkal::platform::join_path(dir, filename);
        if (openkal::platform::file_exists(direct)) {
            return direct;
        }
        const std::string cased = openkal::platform::resolve_path_case(dir, filename);
        if (!cased.empty() && openkal::platform::file_exists(cased)) {
            return cased;
        }
    }
    return {};
}
