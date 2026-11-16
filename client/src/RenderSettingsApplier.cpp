/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RenderSettingsApplier.h"

#include "openkal/engine/engine.h"

namespace openkal::client {

    RenderSettingsApplier::RenderSettingsApplier(engine::IEngine& engine) : m_engine(engine) {}

    RenderSettingsCapabilities RenderSettingsApplier::capabilities() const {
        return {true, m_engine.render_capabilities().bloom, true};
    }

    void RenderSettingsApplier::apply(const settings::RenderSettings& settings) {
        float bloom_strength = 0.0f;
        if (capabilities().bloom) {
            switch (settings.bloom) {
            case settings::BloomQuality::Subtle:
                bloom_strength = 0.35f;
                break;
            case settings::BloomQuality::High:
                bloom_strength = 0.7f;
                break;
            case settings::BloomQuality::Off:
            default:
                break;
            }
        }
        m_engine.set_bloom_strength(bloom_strength);
    }

}  // namespace openkal::client
