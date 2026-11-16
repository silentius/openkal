/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/StatusWidget.h"

#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <string>

namespace openkal::client {

    void StatusWidgetData::set_hp(int hp, int max_hp) {
        m_hp = hp;
        m_max_hp = max_hp;
        m_hp_ratio = static_cast<float>(hp) / static_cast<float>(std::max(1, max_hp));
    }

    void StatusWidgetData::set_mp(int mp, int max_mp) {
        m_mp = mp;
        m_max_mp = max_mp;
        m_mp_ratio = static_cast<float>(mp) / static_cast<float>(std::max(1, max_mp));
    }

    bool StatusWidget::contains(
        float mx, float my, float x, float y, float scale, const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const float s = std::max(0.25f, scale);
        const UiVarMap vars{{"hud.class_hovered", "false"}};
        for (const auto& widget : doc.widgets()) {
            if (!widget.visible() || !ui_eval_condition(widget.visible_when(), vars)) {
                continue;
            }
            float sx = 0, sy = 0, sw = 0, sh = 0;
            UiView::widget_rect(
                widget, nullptr, x, y, doc.window_w() * s, doc.window_h() * s, s, sx, sy, sw, sh);
            if (WidgetTools::point_in_rect(mx, my, sx, sy, sw, sh)) {
                return true;
            }
        }
        return false;
    }

    void StatusWidget::draw(WidgetTools& ui,
                            engine::IEngine& engine,
                            const FreetypeFont* font,
                            float x,
                            float y,
                            const StatusWidgetData& data,
                            float mx,
                            float my,
                            bool pointer_enabled,
                            float scale,
                            const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const float s = std::max(0.25f, scale);
        const float bw = (doc.window_w() > 0.0f ? doc.window_w() : 220.0f) * s;
        const float bh = (doc.window_h() > 0.0f ? doc.window_h() : 72.0f) * s;

        class StatusHost : public UiViewHost {
          public:
            explicit StatusHost(const StatusWidgetData& data) : m_data(data) {}

            [[nodiscard]] bool
            resolve_slot(std::string_view bind, int /*index*/, UiSlotIcon& out) const override {
                if (bind != "hud.class") {
                    return false;
                }
                if (!m_data.class_icon() || !m_data.class_icon()[0]) {
                    out.set_empty(true);
                    return true;
                }
                out.set_file(m_data.class_icon());
                out.set_empty(false);
                return true;
            }

            [[nodiscard]] float progress_ratio(std::string_view bind) const override {
                if (bind == "vital.hp_ratio") {
                    return m_data.hp_ratio();
                }
                if (bind == "vital.mp_ratio") {
                    return m_data.mp_ratio();
                }
                return 0.0f;
            }

            void draw_canvas(engine::IEngine& engine,
                             WidgetTools& /*ui*/,
                             const UiWidget& w,
                             float sx,
                             float sy,
                             float sw,
                             float /*sh*/,
                             float scale,
                             const UiVarMap& /*vars*/,
                             const FreetypeFont& body) const override {
                if (w.canvas_kind() != "level_orb" || !body.ready()) {
                    return;
                }
                const std::string txt = std::to_string(m_data.level());
                const float tw = body.text_width(txt, scale);
                const float th = body.line_height() * scale;
                body.draw(engine, sx - tw * 0.5f, sy - th * 0.5f, txt, 1.0f, 1.0f, 1.0f, scale);
                (void) sw;
            }

          private:
            const StatusWidgetData& m_data;
        };

        UiVarMap vars;
        vars["player.name"] = data.name();
        vars["player.level"] = std::to_string(data.level());
        vars["vital.hp_text"] = std::to_string(data.hp()) + "/" + std::to_string(data.max_hp());
        vars["vital.mp_text"] = std::to_string(data.mp()) + "/" + std::to_string(data.max_mp());
        vars["vital.hp_ratio"] = std::to_string(data.hp_ratio());
        vars["vital.mp_ratio"] = std::to_string(data.mp_ratio());
        ui_fill_theme_vars(doc, vars);

        const UiHit hover =
            pointer_enabled ? UiView::hit(doc, &ui, mx, my, x, y, bw, bh, s, vars, nullptr, false)
                            : UiHit{};
        vars["hud.class_hovered"] =
            hover.kind() == UiHitKind::IconSlot && hover.bind() == "hud.class" ? "true" : "false";

        StatusHost host(data);
        const FreetypeFont empty;
        const FreetypeFont& face = (font && font->ready()) ? *font : empty;
        UiView::draw(engine, ui, doc, x, y, bw, bh, s, vars, face, face, &host, false);
    }

}  // namespace openkal::client
