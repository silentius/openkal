/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DragonHud.h"

#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <string>

namespace openkal::client {
    void DragonHud::draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         const FreetypeFont& bold,
                         float x,
                         float y,
                         float scale,
                         const StatusWidgetData& status,
                         const DragonHudData& data,
                         const UiLayout* layout) {
        const auto& doc = UiView::document(layout);
        auto ratio = [](std::int64_t current, std::int64_t maximum) {
            return maximum > 0 ? std::clamp(static_cast<double>(current) / maximum, 0.0, 1.0) : 0.0;
        };
        UiVarMap vars;
        vars["vital.hp"] = std::to_string(status.hp());
        vars["vital.max_hp"] = std::to_string(status.max_hp());
        vars["vital.mp"] = std::to_string(status.mp());
        vars["vital.max_mp"] = std::to_string(status.max_mp());
        vars["vital.hp_ratio"] = std::to_string(ratio(status.hp(), status.max_hp()));
        vars["vital.mp_ratio"] = std::to_string(ratio(status.mp(), status.max_mp()));
        vars["vital.hp_label"] = "HP";
        vars["vital.mp_label"] = "MP";
        vars["skillbar.active_label"] = "ACTIVE";
        vars["skillbar.auto_hp_label"] = "AUTO HP";
        vars["skillbar.auto_hp_percent"] = std::to_string(data.auto_hp_percent());
        vars["skillbar.auto_hp_status"] = "AUTO HP " + std::to_string(data.auto_hp_percent()) + "%";
        for (const int percent : {25, 50, 75, 100}) {
            vars["skillbar.auto_hp_" + std::to_string(percent)] =
                data.auto_hp_percent() == percent ? "true" : "false";
        }
        vars["exp.pct"] = std::to_string(ratio(data.current_exp(), data.total_exp()));
        vars["exp.sinsu_pct"] = std::to_string(ratio(data.rage_current(), data.rage_total()));
        ui_fill_theme_vars(doc, vars);
        UiView::draw(engine,
                     ui,
                     doc,
                     x,
                     y,
                     doc.window_w() * scale,
                     doc.window_h() * scale,
                     scale,
                     vars,
                     font,
                     bold,
                     nullptr,
                     false);
    }
}  // namespace openkal::client
