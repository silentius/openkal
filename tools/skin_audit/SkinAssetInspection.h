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

#include "openkal/platform/Json.h"
#include "openkal/terrain/GbModel.h"

#include <string>

namespace openkal::tools {

    /** Inspects decoded geometry, bone references and used material textures.
     * @param model Model to inspect without modifying its geometry or materials.
     * @param path Model path used by the normal texture resolver.
     * @return Inspection object with bounds, fingerprints and technical errors.
     * The attachment-shape fingerprint covers non-effect mesh positions and indices,
     * ignoring UVs, materials, skin weights and mesh order. It applies to rigid
     * attachments and does not establish weapon type alone.
     */
    [[nodiscard]] platform::Json inspect_skin_asset(const terrain::GbModel& model,
                                                    const std::string& path);

}  // namespace openkal::tools
