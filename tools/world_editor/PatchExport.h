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

#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplFile.h"

#include <QByteArray>
#include <QPoint>

namespace openkal::world_editor {

    /** Updates the layer and height blocks while retaining the source KCM metadata.
     * @param source Original KCM bytes.
     * @param patch Edited terrain.
     * @return Updated KCM bytes, or empty if the data cannot be encoded.
     */
    [[nodiscard]] QByteArray encode_kcm(const QByteArray& source,
                                        const terrain::HeightPatch& patch);

    /** Updates the placement table, retaining the header and any trailing bytes.
     * @param source Original OPL bytes, empty for a new placement file.
     * @param coordinate Chunk coordinate for a new file.
     * @param instances Placed objects in file order.
     * @return Updated OPL bytes, or empty if the data cannot be encoded.
     */
    [[nodiscard]] QByteArray encode_opl(const QByteArray& source,
                                        QPoint coordinate,
                                        const std::vector<terrain::OplInstance>& instances);

}  // namespace openkal::world_editor
