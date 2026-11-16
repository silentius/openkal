/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MenuBarWidget.h"

#include "openkal/client/UiDocument.h"
#include "openkal/client/UiView.h"

#include <algorithm>
#include <array>
#include <string>

namespace openkal::client {
    namespace {
        struct MenuEntry {
            HudMenuAction action;
            const char* name;
            const char* tooltip;
        };

        constexpr std::array<MenuEntry, 12> kEntries{{
            {HudMenuAction::Sinsu, "hud.menu.sinsu", "Shinsu (H)"},
            {HudMenuAction::Riding, "hud.menu.riding", "Riding (E)"},
            {HudMenuAction::Character, "hud.menu.character", "Character (C)"},
            {HudMenuAction::Inventory, "hud.menu.inventory", "Inventory (I)"},
            {HudMenuAction::Skill, "hud.menu.skill", "Skills (K)"},
            {HudMenuAction::Quest, "hud.menu.quest", "Quests (Q)"},
            {HudMenuAction::Party, "hud.menu.party", "Party actions / invite selected player"},
            {HudMenuAction::Community, "hud.menu.community", "Guild (G)"},
            {HudMenuAction::Map, "hud.menu.map", "Map (M)"},
            {HudMenuAction::Messages, "hud.menu.messages", "Messages (L)"},
            {HudMenuAction::TradingPost, "hud.menu.trading_post", "Trading Post"},
            {HudMenuAction::System, "hud.menu.system", "System (Esc)"},
        }};
    }  // namespace

    HudMenuAction MenuBarWidget::action_for(std::string_view action) {
        for (const auto& entry : kEntries) {
            if (action == entry.name) {
                return entry.action;
            }
        }
        return HudMenuAction::None;
    }

    void MenuBarWidget::draw(WidgetTools& ui,
                             engine::IEngine& engine,
                             const FreetypeFont& font,
                             float x,
                             float y,
                             float w,
                             float h,
                             float scale,
                             const MenuBarWidgetData& data,
                             float screen_w,
                             float screen_h,
                             const UiLayout* layout) {
        UiDocument doc;
        UiView::reload(doc, layout);
        const float s = std::max(0.25f, scale);
        UiVarMap vars;
        ui_fill_theme_vars(doc, vars);
        for (const auto& entry : kEntries) {
            vars[std::string(entry.name) + ".tooltip"] = entry.tooltip;
        }

        vars["hud.menu.messages.unread"] = std::to_string(data.unread_messages);
        vars["hud.menu.trading_post.pending"] =
            data.trading_post_collection_pending ? "true" : "false";
        vars["hud.menu.trading_post.badge"] = "!";
        vars["hud.menu.messages.badge"] =
            data.unread_messages > 99 ? "99+" : std::to_string(data.unread_messages);

        const UiHit hover =
            data.pointer_enabled
                ? UiView::hit(
                      doc, &ui, data.mouse_x, data.mouse_y, x, y, w, h, s, vars, nullptr, false)
                : UiHit{};
        std::string tooltip;
        float anchor_x = x;
        float anchor_y = y;
        float anchor_h = h;
        for (auto& widget : doc.widgets()) {
            if (widget.type() != UiWidgetType::Button) {
                continue;
            }
            const HudMenuAction action = action_for(widget.action());
            const bool open = data.open[static_cast<std::size_t>(action)];
            const bool hovered =
                hover.kind() == UiHitKind::Button && hover.widget_id() == widget.id();
            if ((open || (hovered && data.pressed)) && !widget.file_on().empty()) {
                widget.set_file_default(widget.file_on());
            } else if (hovered && !widget.file_over().empty()) {
                widget.set_file_default(widget.file_over());
            }
            if (hovered && !data.pressed) {
                tooltip = ui_interpolate(widget.text(), vars);
                float width = 0;
                UiView::widget_rect(
                    widget, &ui, x, y, w, h, s, anchor_x, anchor_y, width, anchor_h);
                anchor_x += width * 0.5f;
            }
        }
        // HUD bitmap backgrounds sit behind their individual button/image widgets.
        UiView::draw_chrome(ui, doc, x, y, w, h, s, false);
        UiView::draw_widgets(engine, ui, doc, x, y, w, h, s, vars, font, font);

        if (tooltip.empty() || !font.ready()) {
            return;
        }
        const float padding = 7 * s;
        const float tip_w = font.text_width(tooltip, s) + 2 * padding;
        const float tip_h = font.line_height() * s + 2 * padding;
        const float tip_x =
            std::clamp(anchor_x - tip_w * 0.5f, 0.0f, std::max(0.0f, screen_w - tip_w));
        float tip_y = anchor_y + anchor_h + 7 * s;
        if (tip_y + tip_h > screen_h) {
            tip_y = anchor_y - tip_h - 7 * s;
        }
        tip_y = std::clamp(tip_y, 0.0f, std::max(0.0f, screen_h - tip_h));
        engine.draw_ui_rect(tip_x, tip_y, tip_w, tip_h, 0.45f, 0.33f, 0.17f, 1);
        engine.draw_ui_rect(
            tip_x + s, tip_y + s, tip_w - 2 * s, tip_h - 2 * s, 0.055f, 0.055f, 0.05f, 1);
        font.draw(engine, tip_x + padding, tip_y + padding, tooltip, 0.95f, 0.90f, 0.80f, s);
    }

}  // namespace openkal::client
