/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "widgets/ItemIconOverlay.h"

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <cmath>

QPixmap apply_item_text_overlay(const QPixmap& base, const QStringList& lines, int logical_size) {
    if (lines.isEmpty() || logical_size <= 0) {
        return base;
    }

    const qreal dpr = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    const int pixel = static_cast<int>(std::lround(logical_size * dpr));
    QPixmap canvas(pixel, pixel);
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!base.isNull()) {
        const QPixmap scaled =
            base.scaled(logical_size, logical_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const int ox = (logical_size - scaled.width()) / 2;
        const int oy = (logical_size - scaled.height()) / 2;
        painter.drawPixmap(ox, oy, scaled);
    }

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(logical_size >= 40 ? 6 : 5);
    painter.setFont(font);

    const QFontMetrics metrics(font);
    const int line_h = metrics.height();
    const int pad = 1;
    int y = pad;
    for (const QString& line : lines) {
        if (y + line_h > logical_size) {
            break;
        }
        const QString clipped = metrics.elidedText(line, Qt::ElideRight, logical_size - 2);
        const int text_w = metrics.horizontalAdvance(clipped);
        painter.fillRect(QRect(0, y, text_w + 2, line_h), QColor(20, 20, 20, 170));
        painter.setPen(QColor(20, 20, 20));
        painter.drawText(2, y + metrics.ascent() + 1, clipped);
        painter.setPen(QColor(255, 230, 120));
        painter.drawText(1, y + metrics.ascent(), clipped);
        y += line_h;
    }

    painter.end();
    return canvas;
}
