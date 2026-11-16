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
#include <QStringList>

/** Draws compact multi-line text onto an item icon pixmap.
 * @param base Source icon.
 * @param lines Overlay lines (top-left, yellow badge style).
 * @param logical_size Logical width/height in device-independent pixels.
 * @return Composite pixmap with overlay, or base when lines are empty.
 */
[[nodiscard]] QPixmap
apply_item_text_overlay(const QPixmap& base, const QStringList& lines, int logical_size);
