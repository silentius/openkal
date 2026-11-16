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

#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <QDebug>
#include <QImage>
#include <QPainter>
#include <cstdlib>

#ifndef OPENKAL_RUNTIME_DATA_ROOT
#define OPENKAL_RUNTIME_DATA_ROOT ""
#endif

namespace {

    bool icon_debug_verbose() {
        if (ItemIconCache::debug_verbose()) {
            return true;
        }
        const char* env = std::getenv("OPENKAL_LOOT_ICON_DEBUG");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }

    void log_line(const char* level, const std::string& msg) {
        qWarning().noquote() << "[ItemIconCache]" << level << QString::fromStdString(msg);
        openkal::log::warn(std::string("ItemIconCache: ") + level + " " + msg);
    }

    [[nodiscard]] QPixmap make_placeholder(int size, const QColor& color, const QString& label) {
        QPixmap px(size, size);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(0, 0, size, size, color);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSize(7);
        p.setFont(f);
        p.drawText(px.rect(), Qt::AlignCenter | Qt::TextWordWrap, label);
        return px;
    }

    [[nodiscard]] std::string sample_hypertext_files(const std::string& hypertext_dir,
                                                     int max_files) {
        if (!openkal::platform::is_directory(hypertext_dir)) {
            return "(directory missing)";
        }
        std::string out;
        int count = 0;
        for (const auto& name : openkal::platform::list_directory(hypertext_dir)) {
            if (name.size() >= 4) {
                const std::string ext = name.substr(name.size() - 4);
                if (ext == ".bmp" || ext == ".BMP") {
                    if (!out.empty()) {
                        out += ", ";
                    }
                    out += name;
                    ++count;
                    if (count >= max_files) {
                        out += ", …";
                        break;
                    }
                }
            }
        }
        if (out.empty()) {
            return "(no .bmp files found)";
        }
        return out;
    }

}  // namespace

bool ItemIconCache::s_debug_verbose = false;

void ItemIconCache::set_debug_verbose(bool enabled) {
    s_debug_verbose = enabled;
}

bool ItemIconCache::debug_verbose() {
    return s_debug_verbose;
}

void ItemIconCache::log_catalog_once(const openkal::client::ItemDb& catalog) {
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    const std::string hypertext = catalog.hypertext_dir();
    const bool dir_ok = openkal::platform::is_directory(hypertext);
    log_line("init",
             "hypertext_dir=" + (hypertext.empty() ? "(empty)" : hypertext) +
                 " exists=" + (dir_ok ? "yes" : "no"));
    if (!hypertext.empty()) {
        log_line("init", "sample bmp: " + sample_hypertext_files(hypertext, 5));
    }
    log_line("init", "set OPENKAL_LOOT_ICON_DEBUG=1 for per-item verbose load traces");
}

ItemIconCache::ItemIconCache(const openkal::client::ItemDb& catalog) : m_catalog(catalog) {
    log_catalog_once(catalog);
}

QPixmap ItemIconCache::load_icon(int item_index) const {
    const openkal::client::ItemDef* def = m_catalog.find(item_index);
    const std::string image_stem = def ? def->image() : std::string{};
    const std::string bmp_name = m_catalog.icon_filename(item_index);
    const std::string runtime_data_root = OPENKAL_RUNTIME_DATA_ROOT;
    const std::string runtime_hypertext =
        runtime_data_root.empty() ? std::string{}
                                  : openkal::platform::join_path(runtime_data_root, "HyperText");
    const std::string client_hypertext = m_catalog.hypertext_dir();

    const auto log_attempt = [&](const char* step, const std::string& detail) {
        if (!icon_debug_verbose()) {
            return;
        }
        log_line("verbose", "item " + std::to_string(item_index) + " [" + step + "] " + detail);
    };

    log_attempt("catalog",
                "name=" + (def ? def->name() : "?") +
                    " image_stem=" + (image_stem.empty() ? "(empty)" : image_stem));

    if (bmp_name.empty()) {
        log_attempt("fail", "icon_filename returned empty (no image stem?)");
        return {};
    }

    if (runtime_hypertext.empty() && client_hypertext.empty()) {
        log_attempt("fail", "no packaged-runtime or client HyperText directory is configured");
        return {};
    }

    for (const std::string& hypertext : {runtime_hypertext, client_hypertext}) {
        if (hypertext.empty()) {
            continue;
        }
        const std::string path = openkal::platform::join_path(hypertext, bmp_name);
        log_attempt("path", "icon_filename=" + bmp_name + " full=" + path);
        if (!openkal::platform::file_exists(path)) {
            const std::string case_fixed =
                openkal::platform::resolve_path_case(hypertext, bmp_name);
            log_attempt("miss",
                        "file missing at " + path +
                            (case_fixed.empty() ? "" : " (resolve_path_case=" + case_fixed + ")"));
            continue;
        }

        QImage img(QString::fromStdString(path));
        if (img.isNull()) {
            log_attempt("fail", "QImage failed to decode " + path);
            continue;
        }

        log_attempt("ok",
                    "loaded " + path + " size=" + std::to_string(img.width()) + "x" +
                        std::to_string(img.height()));
        m_hypertext_ready = true;
        return QPixmap::fromImage(
            img.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    return {};
}

QPixmap ItemIconCache::icon(int item_index) const {
    const auto it = m_cache.find(item_index);
    if (it != m_cache.end()) {
        return it->second;
    }

    QPixmap px = load_icon(item_index);
    if (px.isNull()) {
        if (m_logged_failures.insert(item_index).second) {
            const openkal::client::ItemDef* def = m_catalog.find(item_index);
            const std::string hypertext = m_catalog.hypertext_dir();
            const std::string bmp_name = m_catalog.icon_filename(item_index);
            const std::string path = hypertext.empty()
                                         ? std::string{}
                                         : openkal::platform::join_path(hypertext, bmp_name);
            log_line("miss",
                     "item " + std::to_string(item_index) +
                         " stem=" + (def && !def->image().empty() ? def->image() : "?") +
                         " bmp=" + (bmp_name.empty() ? "(none)" : bmp_name) +
                         " path=" + (path.empty() ? "(none)" : path) +
                         " hypertext=" + (hypertext.empty() ? "(empty)" : hypertext));
        }

        const openkal::client::ItemDef* def = m_catalog.find(item_index);
        QString label = def && !def->name().empty() ? QString::fromStdString(def->name())
                                                    : QString("#%1").arg(item_index);
        if (label.size() > 12) {
            label = label.left(10) + "…";
        }
        px = make_placeholder(32, QColor(60, 60, 80), label);
    }
    m_cache[item_index] = px;
    return px;
}

QPixmap ItemIconCache::money_icon() const {
    if (m_money.isNull()) {
        QPixmap coin = load_icon(openkal::server::kMoneyItemIndex);
        if (coin.isNull()) {
            m_money = make_placeholder(32, QColor(180, 150, 40), "$");
        } else {
            m_money = coin;
        }
    }
    return m_money;
}
