/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/UiDocument.h"

#include "openkal/client/GeonColor.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace openkal::client {
    namespace {

        void set_string(platform::Json& obj, const char* key, const std::string& value) {
            auto& node = obj.object()[key];
            node.set_type(platform::Json::Type::String);
            node.string() = value;
        }

        void set_number(platform::Json& obj, const char* key, double value) {
            auto& node = obj.object()[key];
            node.set_type(platform::Json::Type::Number);
            node.set_number(value);
        }

        void set_bool(platform::Json& obj, const char* key, bool value) {
            auto& node = obj.object()[key];
            node.set_type(platform::Json::Type::Bool);
            node.set_boolean(value);
        }

        [[nodiscard]] std::string
        json_s(const platform::Json& node, const char* key, std::string fallback = {}) {
            const auto* child = node.get(key);
            return child ? child->as_string(std::move(fallback)) : fallback;
        }

        [[nodiscard]] float json_f(const platform::Json& node, const char* key, float fallback) {
            const auto* child = node.get(key);
            return child ? child->as_float(fallback) : fallback;
        }

        [[nodiscard]] int json_i(const platform::Json& node, const char* key, int fallback) {
            const auto* child = node.get(key);
            return child ? child->as_int(fallback) : fallback;
        }

        [[nodiscard]] bool json_b(const platform::Json& node, const char* key, bool fallback) {
            const auto* child = node.get(key);
            if (!child) {
                return fallback;
            }
            if (child->type() == platform::Json::Type::Bool) {
                return child->boolean();
            }
            if (child->is_number()) {
                return child->as_int(fallback ? 1 : 0) != 0;
            }
            const std::string s = child->as_string();
            return !(s == "0" || s == "false" || s == "False" || s.empty());
        }

        void load_widget(UiWidget& w, const platform::Json& j) {
            w.set_type(ui_widget_type_from_name(json_s(j, "type")));
            w.set_id(json_s(j, "id"));
            w.set_doc_note(json_s(j, "_doc"));
            w.set_x(json_f(j, "x", 0.0f));
            w.set_y(json_f(j, "y", 0.0f));
            w.set_w(json_f(j, "w", 0.0f));
            w.set_h(json_f(j, "h", 0.0f));
            w.set_visible(json_b(j, "visible", true));
            w.set_visible_when(json_s(j, "visible_when"));
            w.set_text(json_s(j, "text"));
            w.set_multiline(json_b(j, "multiline", false));
            w.set_color(json_s(j, "color"));
            w.set_file(json_s(j, "file"));
            w.set_fill_mode(json_s(j, "fill_mode", "lcr"));
            w.set_direction(json_s(j, "direction", "left_to_right"));
            if (const auto* source = j.get("source")) {
                w.set_source({json_f(*source, "x", 0),
                              json_f(*source, "y", 0),
                              json_f(*source, "w", 0),
                              json_f(*source, "h", 0)});
            }
            w.set_repeat(json_b(j, "repeat", false));
            w.set_bind(json_s(j, "bind"));
            w.set_action(json_s(j, "action"));
            w.set_label(json_s(j, "label"));
            w.set_label_ox(json_f(j, "label_ox", 0.0f));
            w.set_label_oy(json_f(j, "label_oy", 0.0f));
            w.set_file_default(json_s(j, "file_default"));
            w.set_file_over(json_s(j, "file_over"));
            w.set_file_on(json_s(j, "file_on"));
            w.set_file_off(json_s(j, "file_off"));
            w.set_page(json_s(j, "page"));
            w.set_filter(json_s(j, "filter"));
            w.set_viewport(json_s(j, "viewport"));
            w.set_scroll_bind(json_s(j, "scroll_bind"));
            w.set_tooltip(json_b(j, "tooltip", false));
            w.set_drag(json_b(j, "drag", false));
            w.set_pixel_size(json_i(j, "pixel_size", 0));
            w.set_bold(json_b(j, "bold", false));
            w.set_align(json_s(j, "align", "left"));
            w.set_pending_when(json_s(j, "pending_when"));
            w.set_alpha(json_f(j, "alpha", 1.0f));
            w.set_canvas_kind(json_s(j, "kind"));
            w.set_shape(json_s(j, "shape", "rect"));
            w.set_cx(json_f(j, "cx", 0.0f));
            w.set_cy(json_f(j, "cy", 0.0f));
            w.set_inner(json_f(j, "inner", 0.0f));
            w.set_color_on(json_s(j, "color_on"));
            w.set_color_off(json_s(j, "color_off"));
            w.set_cols(json_i(j, "cols", 1));
            w.set_rows(json_i(j, "rows", 1));
            w.set_select_color(json_s(j, "select_color"));
            w.set_line_height(json_f(j, "line_height", 14.0f));
            w.set_thickness(json_f(j, "thickness", 0.0f));
            w.set_border_top(json_s(j, "top"));
            w.set_border_bottom(json_s(j, "bottom"));
            w.set_border_left(json_s(j, "left"));
            w.set_border_right(json_s(j, "right"));
            w.set_x_from_right(json_f(j, "x_from_right", -1.0f));
            if (j.get("y_from_bottom")) {
                w.set_y_from_bottom(json_f(j, "y_from_bottom", -1.0f));
            }
            if (const auto* cell = j.get("cell"); cell && cell->is_object()) {
                w.set_cell_w(json_f(*cell, "w", 32.0f));
                w.set_cell_h(json_f(*cell, "h", 32.0f));
            }
            if (const auto* pitch = j.get("pitch"); pitch && pitch->is_object()) {
                w.set_pitch_x(json_f(*pitch, "x", w.cell_w()));
                w.set_pitch_y(json_f(*pitch, "y", w.cell_h()));
            } else {
                w.set_pitch_x(w.cell_w());
                w.set_pitch_y(w.cell_h());
            }
            if (const auto* empty = j.get("empty"); empty && empty->is_object()) {
                w.set_empty_file(json_s(*empty, "file"));
            }
            if (const auto* badge = j.get("badge"); badge && badge->is_object()) {
                w.set_badge_text(json_s(*badge, "text"));
                w.set_badge_visible_when(json_s(*badge, "visible_when"));
            }
            if (const auto* frame = j.get("frame"); frame && frame->is_object()) {
                w.set_frame_file(json_s(*frame, "file"));
                w.set_frame_w(json_f(*frame, "w", 0.0f));
                w.set_frame_h(json_f(*frame, "h", 0.0f));
            }
            if (const auto* acc = j.get("accept"); acc && acc->is_array()) {
                std::vector<std::string> tags;
                for (const auto& t : acc->array()) {
                    const std::string s = t.as_string();
                    if (!s.empty()) {
                        tags.push_back(s);
                    }
                }
                w.set_accept(std::move(tags));
            }
            if (const auto* items = j.get("items"); items && items->is_array()) {
                for (const auto& it : items->array()) {
                    if (!it.is_object()) {
                        continue;
                    }
                    UiTabItem tab;
                    tab.set_id(json_s(it, "id"));
                    tab.set_label(json_s(it, "label"));
                    tab.set_file_on(json_s(it, "file_on"));
                    tab.set_file_off(json_s(it, "file_off"));
                    tab.set_x(json_f(it, "x", 0.0f));
                    tab.set_w(json_f(it, "w", 0.0f));
                    w.tabs().push_back(std::move(tab));
                }
            }
        }

        [[nodiscard]] std::string format_value(std::string raw, std::string_view suffix) {
            if (suffix.empty()) {
                return raw;
            }
            if (suffix == "comma") {
                std::string digits;
                bool neg = false;
                if (!raw.empty() && raw[0] == '-') {
                    neg = true;
                    raw = raw.substr(1);
                }
                for (char c : raw) {
                    if (std::isdigit(static_cast<unsigned char>(c))) {
                        digits.push_back(c);
                    }
                }
                if (digits.empty()) {
                    return raw;
                }
                std::string out;
                int n = 0;
                for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
                    if (n > 0 && n % 3 == 0) {
                        out.push_back(',');
                    }
                    out.push_back(digits[static_cast<std::size_t>(i)]);
                    ++n;
                }
                std::reverse(out.begin(), out.end());
                return neg ? "-" + out : out;
            }
            if (suffix == "+") {
                if (raw.empty() || raw == "0") {
                    return {};
                }
                if (raw[0] == '-' || raw[0] == '+') {
                    return raw;
                }
                return "+" + raw;
            }
            if (suffix == "pct") {
                char buf[32];
                const float v = std::strtof(raw.c_str(), nullptr);
                std::snprintf(buf, sizeof(buf), "%.0f%%", v * 100.0f);
                return buf;
            }
            return raw;
        }

        [[nodiscard]] std::string lookup(const UiVarMap& vars, std::string_view key) {
            const auto it = vars.find(std::string(key));
            return it == vars.end() ? std::string{} : it->second;
        }

        [[nodiscard]] bool truthy(std::string_view v) {
            if (v.empty() || v == "0" || v == "false" || v == "False") {
                return false;
            }
            return true;
        }

    }  // namespace

    UiChromeFiles ui_chrome_preset_files(std::string_view preset) {
        UiChromeFiles f;
        if (preset == "dragon_dialog") {
            f.set_upleft("dragon_dialog/frame_top_left.bmp");
            f.set_upcenter("dragon_dialog/frame_top.bmp");
            f.set_upright("dragon_dialog/frame_top_right.bmp");
            f.set_left("dragon_dialog/frame_left.bmp");
            f.set_right("dragon_dialog/frame_right.bmp");
            f.set_downleft("dragon_dialog/frame_bottom_left.bmp");
            f.set_downcenter("dragon_dialog/frame_bottom.bmp");
            f.set_downright("dragon_dialog/frame_bottom_right.bmp");
        } else if (preset == "dragon_compact") {
            f.set_upleft("dragon_dialog/compact_top_left.bmp");
            f.set_upcenter("dragon_dialog/compact_top.bmp");
            f.set_upright("dragon_dialog/compact_top_right.bmp");
            f.set_left("dragon_dialog/compact_left.bmp");
            f.set_right("dragon_dialog/compact_right.bmp");
            f.set_downleft("dragon_dialog/compact_bottom_left.bmp");
            f.set_downcenter("dragon_dialog/compact_bottom.bmp");
            f.set_downright("dragon_dialog/compact_bottom_right.bmp");
        } else if (preset == "info_character") {
            f.set_upleft("info-top left(character).bmp");
            f.set_downleft("info-bottom right_2.bmp");
            f.set_downcenter("info-bottom center.bmp");
            f.set_downright("info-bottom right.bmp");
        } else if (preset == "info_inventory") {
            f.set_upleft("info-top left(inven).bmp");
            f.set_downleft("info-bottom left.bmp");
            f.set_downcenter("info-bottom center.bmp");
            f.set_downright("info-bottom right.bmp");
        } else if (preset == "info_mix") {
            f.set_upleft("info-top left(DragonBall).bmp");
            f.set_left("info-left center.bmp");
            f.set_right("info-right center.bmp");
            f.set_downleft("info-bottom right_2.bmp");
            f.set_downcenter("info-bottom center.bmp");
            f.set_downright("info-bottom right.bmp");
        }
        return f;
    }

    void ui_apply_chrome_preset(UiDocument& doc, std::string_view preset) {
        doc.set_chrome_kind(UiChromeKind::NineSlice);
        doc.set_chrome_preset(std::string(preset));
        doc.set_chrome_files(ui_chrome_preset_files(preset));
        doc.set_chrome_close_button(preset != "dragon_dialog" && preset != "dragon_compact");
        if (preset == "dragon_dialog") {
            doc.set_chrome_edges(UiChromeEdges::Repeat);
            doc.set_pad_left(40.0f);
            doc.set_pad_top(84.0f);
            doc.set_pad_right(40.0f);
            doc.set_pad_bottom(56.0f);
        } else if (preset == "dragon_compact") {
            doc.set_chrome_edges(UiChromeEdges::Repeat);
            doc.set_pad_left(5.0f);
            doc.set_pad_top(5.0f);
            doc.set_pad_right(5.0f);
            doc.set_pad_bottom(5.0f);
        } else if (preset == "info_menu") {
            doc.set_chrome_edges(UiChromeEdges::Stretch);
            doc.set_pad_left(10.0f);
            doc.set_pad_top(40.0f);
            doc.set_pad_right(7.0f);
            doc.set_pad_bottom(15.0f);
        } else {
            doc.set_chrome_edges(UiChromeEdges::Tile);
            doc.set_pad_left(26.5f);
            doc.set_pad_top(45.1f);
            doc.set_pad_right(19.1f);
            doc.set_pad_bottom(36.1f);
        }
    }

    const char* ui_widget_type_name(UiWidgetType t) {
        switch (t) {
        case UiWidgetType::Image:
            return "image";
        case UiWidgetType::Text:
            return "text";
        case UiWidgetType::Button:
            return "button";
        case UiWidgetType::Progress:
            return "progress";
        case UiWidgetType::IconSlot:
            return "icon_slot";
        case UiWidgetType::IconGrid:
            return "icon_grid";
        case UiWidgetType::Tabs:
            return "tabs";
        case UiWidgetType::Fill:
            return "fill";
        case UiWidgetType::Border:
            return "border";
        case UiWidgetType::Input:
            return "input";
        case UiWidgetType::List:
            return "list";
        case UiWidgetType::Canvas:
            return "canvas";
        default:
            return "unknown";
        }
    }

    UiWidgetType ui_widget_type_from_name(std::string_view name) {
        if (name == "image") {
            return UiWidgetType::Image;
        }
        if (name == "text") {
            return UiWidgetType::Text;
        }
        if (name == "button") {
            return UiWidgetType::Button;
        }
        if (name == "progress") {
            return UiWidgetType::Progress;
        }
        if (name == "icon_slot") {
            return UiWidgetType::IconSlot;
        }
        if (name == "icon_grid") {
            return UiWidgetType::IconGrid;
        }
        if (name == "tabs") {
            return UiWidgetType::Tabs;
        }
        if (name == "fill") {
            return UiWidgetType::Fill;
        }
        if (name == "border") {
            return UiWidgetType::Border;
        }
        if (name == "input") {
            return UiWidgetType::Input;
        }
        if (name == "list") {
            return UiWidgetType::List;
        }
        if (name == "canvas") {
            return UiWidgetType::Canvas;
        }
        return UiWidgetType::Unknown;
    }

    std::string ui_interpolate(std::string_view templ,
                               const UiVarMap& vars,
                               std::vector<UiTextColorRun>* colors) {
        if (colors) {
            colors->clear();
        }
        std::string out;
        out.reserve(templ.size());
        for (std::size_t i = 0; i < templ.size();) {
            if (templ[i] == '$' && i + 1 < templ.size() && templ[i + 1] == '{') {
                const std::size_t end = templ.find('}', i + 2);
                if (end == std::string_view::npos) {
                    out.append(templ.substr(i));
                    break;
                }
                const std::string_view inner = templ.substr(i + 2, end - (i + 2));
                const std::size_t colon = inner.find(':');
                std::string_view key = inner;
                std::string_view suffix;
                if (colon != std::string_view::npos) {
                    key = inner.substr(0, colon);
                    suffix = inner.substr(colon + 1);
                }
                const auto value = lookup(vars, key);
                if (suffix == "geons") {
                    for (std::size_t n = 0; n < value.size();) {
                        if (value[n] < '0' || value[n] > '9') {
                            out.push_back(value[n++]);
                            continue;
                        }
                        const auto begin = n;
                        std::string digits;
                        do {
                            if (value[n] != ',') {
                                digits.push_back(value[n]);
                            }
                            ++n;
                        } while (n < value.size() &&
                                 ((value[n] >= '0' && value[n] <= '9') ||
                                  (value[n] == ',' && n + 1 < value.size() && value[n + 1] >= '0' &&
                                   value[n + 1] <= '9')));
                        std::int64_t amount = 0;
                        const auto parsed =
                            std::from_chars(digits.data(), digits.data() + digits.size(), amount);
                        if (parsed.ec != std::errc{}) {
                            out.append(value, begin, n - begin);
                            continue;
                        }
                        const auto formatted = format_value(digits, "comma");
                        if (colors) {
                            colors->push_back(
                                {out.size(), formatted.size(), geon_color(amount).hex});
                        }
                        out += formatted;
                    }
                } else {
                    out += format_value(value, suffix);
                }
                i = end + 1;
                continue;
            }
            out.push_back(templ[i]);
            ++i;
        }
        return out;
    }

    bool ui_eval_condition(std::string_view expr, const UiVarMap& vars) {
        if (expr.empty()) {
            return true;
        }
        const std::size_t gt = expr.find('>');
        if (gt != std::string_view::npos) {
            std::string left(expr.substr(0, gt));
            std::string right(expr.substr(gt + 1));
            auto trim = [](std::string& s) {
                while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                    s.erase(s.begin());
                }
                while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                    s.pop_back();
                }
            };
            trim(left);
            trim(right);
            if (!left.empty() && left.front() == '$') {
                left = ui_interpolate(left, vars);
            } else {
                left = lookup(vars, left);
            }
            const float lv = std::strtof(left.c_str(), nullptr);
            const float rv = std::strtof(right.c_str(), nullptr);
            return lv > rv;
        }
        return truthy(lookup(vars, expr));
    }

    bool ui_parse_hex_color(std::string_view text, float& r, float& g, float& b) {
        std::string_view t = text;
        if (!t.empty() && t.front() == '#') {
            t.remove_prefix(1);
        }
        if (t.size() != 6) {
            return false;
        }
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            return -1;
        };
        int bytes[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            const int hi = hex(t[static_cast<std::size_t>(i * 2)]);
            const int lo = hex(t[static_cast<std::size_t>(i * 2 + 1)]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            bytes[i] = hi * 16 + lo;
        }
        r = static_cast<float>(bytes[0]) / 255.0f;
        g = static_cast<float>(bytes[1]) / 255.0f;
        b = static_cast<float>(bytes[2]) / 255.0f;
        return true;
    }

    void ui_fill_theme_vars(const UiDocument& doc, UiVarMap& vars) {
        vars["theme.colors.label"] = doc.color_label();
        vars["theme.colors.value"] = doc.color_value();
        vars["theme.colors.pending"] = doc.color_pending();
        vars["theme.colors.title"] = doc.color_title();
        vars["theme.font_pixel_size"] = std::to_string(doc.font_pixel_size());
    }

    void ui_fill_dummy_preview(UiVarMap& vars) {
        vars["trading_post.buy"] = "false";
        vars["trading_post.filters"] = "true";
        vars["trading_post.collection"] = "false";
        vars["trading_post.own"] = "false";
        vars["trading_post.collection_help"] = "Items and Geons are held safely.";
        vars["trading_post.collection_help2"] = "Collect at a Trading Post Agent.";
        vars["trading_post.collect_all"] = "Collect All";
        vars["trading_post.collect_selected"] = "Collect selected";
        vars["trading_post.own_help"] = "Select an offer to withdraw it.";
        for (const auto* tab : {"buy", "sell", "own", "collection"}) {
            vars[std::string("trading_post.") + tab + "_color"] =
                "sell" == std::string_view(tab) ? "#F5D68A" : "#BDAE92";
        }
        const std::pair<const char*, const char*> market_labels[] = {
            {"title", "Trading Post"},
            {"tab_buy", "Buy"},
            {"tab_sell", "Sell"},
            {"tab_own", "My Offers"},
            {"tab_collection", "Collection"},
            {"class_label", "Class"},
            {"type_label", "Type"},
            {"prefix_label", "Prefix"},
            {"grade_label", "Grade"},
            {"order_label", "Order"},
            {"class", "Any  v"},
            {"type", "Weapon  v"},
            {"prefix", "Any  v"},
            {"grade", "Grade 50  v"},
            {"order", "Price: lowest first  v"},
            {"sell", "true"},
            {"price_label", "Unit price (Geons)"},
            {"price", "100,000"},
            {"price_color", "#00FF00"},
            {"selected_color", "#EAE3D4"},
            {"details_color", "#00FF00"},
            {"inspect", "false"},
            {"selected", "Small Medicine"},
            {"details", "500,000 Geons total"},
            {"act", "List selected"},
            {"page", "1"},
            {"status1", "Select an item to inspect or trade. Offers are anonymous."},
            {"status2", ""},
            {"quantity_title", "Offer quantity"},
            {"quantity_prompt", "Quantity (1 - 50)"},
            {"quantity", "5"},
            {"quantity_unit_price", "Unit price: 100,000 Geons"},
            {"quantity_total", "500,000 Geons total"},
            {"option", "Knight"}};
        for (const auto& entry : market_labels) {
            vars[std::string("trading_post.") + entry.first] = entry.second;
        }
        for (int row = 0; row < 6; ++row) {
            vars["trading_post.range" + std::to_string(row)] =
                row == 0 ? "100,000 - 3,000,000 Geons / item" : "3,000,000 Geons / item";
        }
        vars["hud.menu.trading_post.tooltip"] = "Trading Post";
        vars["hud.menu.trading_post.pending"] = "true";
        vars["hud.menu.trading_post.badge"] = "!";

        const std::pair<const char*, const char*> skin_shop_labels[] = {
            {"title", "Skin Artisan"},
            {"weapons", "Weapons"},
            {"full", "Full skins"},
            {"parts", "Skin pieces"},
            {"rotate_left", "<"},
            {"rotate_right", ">"},
            {"rotate_hint", "Rotate preview"},
            {"tickets", "Tickets available: 8"},
            {"cost", "Cost: 4 tickets"},
            {"selected", "Selected cosmetic skin"},
            {"redeem", "Redeem"},
            {"close", "Close"},
            {"hint", "Preview is free. Choose your look."},
            {"category", "Full-body skins"}};
        for (const auto& entry : skin_shop_labels) {
            vars[std::string("skin_shop.") + entry.first] = entry.second;
        }
        vars["guild.title"] = "Guild";
        vars["guild.membership"] = "0";
        vars["guild.membership_leader"] = "0";
        vars["guild.membership_hint"] =
            "Membership changes are exceptional. Leave guild requires a second click to confirm.";
        vars["guild.announcement_editing"] = "0";
        vars["guild.announcement_can_edit"] = "1";
        vars["guild.announcement_hint"] = "Announcement — visible to all guild members.";
        vars["guild.announcement_text"] = "Guild meeting at the village.\n\nBring your supplies "
                                          "and meet at 20:00.\nAll guild members are welcome.";
        vars["guild.announcement_scroll"] = "0";
        vars["guild_ranks.title"] = "Change rank";
        vars["guild_ranks.open"] = "1";
        vars["guild_ranks.edit"] = "0";
        vars["guild_ranks.assign"] = "1";
        vars["guild_ranks.context"] = "Member: GuildMember";
        vars["guild_ranks.hint"] = "Choose the new rank, review its permissions, then confirm.";
        vars["guild_ranks.selected"] = "Selected: Sub-Leader";
        vars["guild_ranks.availability"] = "";
        vars["guild_ranks.input"] = "Sub-Leader";
        vars["guild_ranks.name_label"] = "Rank title";
        vars["guild_ranks.permissions_label"] = "Permissions for this rank";
        vars["guild_ranks.save_title"] = "Save title";
        vars["guild_ranks.save_permissions"] = "Save permissions";
        vars["guild_ranks.confirm"] = "Confirm rank";
        vars["guild_ranks.cancel"] = "Close";
        vars["guild_ranks.rank_heading"] = "Rank";
        vars["guild_ranks.availability_heading"] = "Availability";
        vars["guild_ranks.status"] = "";
        vars["guild_ranks.status_more"] = "";
        const char* rank_titles[] = {
            "Leader", "Sub-Leader", "HeadChief", "Chief", "OfficialMember"};
        const char* permission_headings[] = {"Recruit", "Expel", "Announce", "Ranks", "Titles"};
        const char* permission_labels[] = {
            "Accept / invitation", "Expel", "Announcement", "Change rank", "Rename title"};
        for (int bit = 0; bit < 5; ++bit) {
            vars["guild_ranks.heading." + std::to_string(bit)] = permission_headings[bit];
            vars["guild_ranks.option." + std::to_string(bit)] =
                std::string("[ ] ") + permission_labels[bit];
            vars["guild_ranks.option." + std::to_string(bit) + ".label"] = permission_labels[bit];
            vars["guild_ranks.option." + std::to_string(bit) + ".checked"] = "0";
            vars["guild_ranks.option." + std::to_string(bit) + ".color"] = "#EAE3D4";
        }
        for (int rank = 1; rank <= 5; ++rank) {
            const auto key = "guild_ranks.row." + std::to_string(rank);
            vars[key + ".title"] = rank_titles[rank - 1];
            vars[key + ".selected"] = rank == 2 ? "1" : "0";
            vars[key + ".availability"] = rank == 1   ? "Protected rank"
                                          : rank == 5 ? "Current rank"
                                                      : "Available";
            vars[key + ".color"] = rank == 1 || rank == 5 ? "#999999" : "#EAE3D4";
            for (int bit = 0; bit < 5; ++bit) {
                vars[key + ".permission." + std::to_string(bit)] = rank == 1 ? "Yes" : "—";
            }
        }
        vars["guild.tab"] = "0";
        vars["guild.member"] = "1";
        vars["guild.non_member"] = "0";
        vars["guild.body_visible"] = "0";
        vars["guild.table.visible"] = "1";
        vars["guild.table.simple"] = "1";
        vars["guild.table.members"] = "0";
        vars["guild.table.empty"] = "0";
        vars["guild.table.empty_label"] = "No entries on this page.";
        vars["guild.column.0"] = "Guild";
        vars["guild.column.1"] = "Grade";
        vars["guild.column.2"] = "Points";
        vars["guild.option.0"] = "[ ] Accept / invitation";
        vars["guild.option.1"] = "[ ] Expel";
        vars["guild.option.2"] = "[ ] Announcement";
        vars["guild.option.3"] = "[ ] Change rank";
        vars["guild.option.4"] = "[ ] Rename title";
        vars["guild.own"] = "Your guild: OpenKal / Leader";
        vars["guild.summary"] = "OpenKal  |  Grade 4  |  2 members, 1 online  |  120 points";
        vars["guild.requirements"] =
            "Create: level 30, 2 founders, 100000 Zamogeon, party required";
        vars["guild.fees"] = "Apply: 0 / Admit: 0 Zamogeon; rejoin lock 86400 s";
        vars["guild.body"] = "Welcome! Read the invitation and apply to join.";
        vars["guild.input"] = "OpenKal";
        vars["guild.management_input"] = "0";
        vars["guild.browsing"] = "1";
        vars["guild.rank"] = "Rank 2 / options 0";
        vars["guild.pages"] = "1 / 1";
        vars["guild.status"] = "Server rules control every requirement.";
        vars["guild.status_more"] = "";
        vars["player_options.title"] = "Player options";
        vars["player_options.target"] = "GuildMember";
        vars["player_options.pm_available"] = "1";
        vars["player_options.pm_label"] = "Send PM";
        vars["player_options.empty"] = "0";
        vars["player_options.empty_label"] = "No party or guild actions are available.";
        vars["player_options.status"] = "Choose an available action.";
        vars["player_options.status_more"] = "";
        vars["player_options.action_label"] = "Request to join OpenKal";
        const char* guild_tabs[] = {"Ranking",
                                    "Search / Create",
                                    "Members",
                                    "Applications",
                                    "My requests",
                                    "Invitation",
                                    "Announcement",
                                    "Membership",
                                    "Rank options"};
        for (int i = 0; i < 9; ++i) {
            vars["guild.tab." + std::to_string(i)] = guild_tabs[i];
            vars["guild.page." + std::to_string(i)] = i == 0 ? "1" : "0";
            vars["guild.tab." + std::to_string(i) + ".active"] = i == 0 ? "1" : "0";
            vars["guild.tab." + std::to_string(i) + ".member_active"] = i == 0 ? "1" : "0";
            vars["guild.tab." + std::to_string(i) + ".non_member_active"] = "0";
        }
        for (int i = 0; i < 10; ++i) {
            const auto key = "guild.row." + std::to_string(i);
            vars[key + ".visible"] = i < 2 ? "1" : "0";
            vars[key + ".table_visible"] = vars[key + ".visible"];
            vars[key + ".simple_visible"] = vars[key + ".visible"];
            vars[key + ".members_visible"] = "0";
            vars[key + ".body_visible"] = "0";
            vars[key + ".selected"] = "0";
            vars[key + ".cell.0"] = i == 0 ? "OpenKal" : "Test Guild";
            vars[key + ".cell.1"] = i == 0 ? "4" : "2";
            vars[key + ".cell.2"] = i == 0 ? "120" : "50";
            vars[key + ".text"] =
                i == 0 ? "OpenKal    Grade 4    120 points" : "Test Guild    Grade 2    50 points";
            vars[key + ".color"] = i == 0 ? "#FFFFFF" : "#999999";
        }
        vars["guild.formation_accept_label"] = "Formation accept";
        vars["guild.formation_decline_label"] = "Formation decline";
        vars["guild.search_label"] = "Search";
        vars["guild.create_label"] = "Create";
        vars["guild.details_label"] = "Details";
        vars["guild.apply_label"] = "Apply";
        vars["guild.withdraw_label"] = "Withdraw";
        vars["guild.accept_label"] = "Accept";
        vars["guild.temporary_label"] = "Temporary";
        vars["guild.reject_label"] = "Reject";
        vars["guild.expel_label"] = "Expel";
        vars["guild.set_rank_label"] = "Set rank";
        vars["guild.set_invitation_label"] = "Set invitation";
        vars["guild.set_announcement_label"] = "Set announcement";
        vars["guild.transfer_label"] = "Transfer";
        vars["guild.set_title_label"] = "Set title";
        vars["guild.set_permissions_label"] = "Set permissions";
        vars["guild.rank_label"] = "Rank";
        vars["guild.refresh_label"] = "Refresh";
        vars["guild.previous_label"] = "Previous";
        vars["guild.next_label"] = "Next";
        vars["guild.leave_label"] = "Leave";
        vars["guild.daily_label"] = "Daily";

        vars["quest.snitch_label"] = "Snitch";
        vars["snitch.title"] = "Snitch";
        vars["snitch.target"] = "Monster: Training Beast";
        vars["snitch.target_more"] = "";
        vars["snitch.position"] = "Map 0 / X 258000 / Y 258000";
        vars["snitch.page"] = "1 / 3";
        vars["snitch.multiple"] = "1";
        vars["snitch.previous"] = "<";
        vars["snitch.next"] = ">";
        vars["snitch.legend"] = "Yellow: NPC / monster spawn";
        vars["powerbook.title"] = "Powerbook";
        vars["powerbook.selection"] = "Training Beast  /  Lv. 12";
        vars["powerbook.monsters_label"] = "Monsters / Level";
        vars["powerbook.drops_label"] = "Possible drops";
        vars["powerbook.spawns_label"] = "Spawn areas";
        vars["powerbook.previous"] = "< Previous";
        vars["powerbook.next"] = "Next >";
        vars["powerbook.up"] = "^";
        vars["powerbook.down"] = "v";
        vars["powerbook.monster_range"] = "1 - 16 / 120";
        vars["powerbook.drop_status"] = "77 possible items";
        vars["powerbook.spawn_visible"] = "true";
        vars["powerbook.spawn_multiple"] = "true";
        vars["powerbook.spawn_status"] = "Area 1  /  Map 0  /  1 of 3";
        vars["powerbook.legend"] = "Yellow: spawn area   /   Q: requires an eligible quest";
        vars["powerbook.controls"] =
            "Scroll over either list or drag its scrollbar. Select a "
            "monster; hover an item slot for its effects and requirements.";
        vars["powerbook.blank"] = "";
        vars["powerbook.empty"] = "";
        for (int i = 0; i < 16; ++i) {
            const auto monster = "powerbook.monster." + std::to_string(i);
            vars[monster + ".visible"] = "true";
            vars[monster + ".name"] =
                i == 0 ? "Training Beast" : "Practice Monster " + std::to_string(i);
            vars[monster + ".level"] = std::to_string(12 + i);
            vars[monster + ".color"] = i == 0 ? "#4C3B22" : (i % 2 ? "#151410" : "#0D0D0C");
        }
        for (int i = 0; i < 60; ++i) {
            const auto drop = "powerbook.drop." + std::to_string(i);
            vars[drop + ".visible"] = "true";
            vars[drop + ".name"] = "Example item " + std::to_string(i + 1);
            vars[drop + ".quest"] = i == 2 ? "Q" : "";
            vars[drop + ".color"] = i == 0 ? "#D9BA72" : "#554A37";
        }
        vars["talent.title"] = "Talent Tree";
        vars["talent.points"] = "Available: 40 | Spent: 20 | Pending: 10";
        vars["talent.physical_label"] = "Offensive / Physical";
        vars["talent.magical_label"] = "Offensive / Magical";
        vars["talent.defensive_label"] = "Defensive";
        vars["talent.qol_label"] = "Quality of Life";
        vars["talent.upgrades_label"] = "Upgrades";
        vars["talent.class_name"] = "Knight";
        vars["talent.future_label"] = "Class Updates";
        vars["talent.future_hint"] = "TBD / Unavailable";
        vars["talent.apply"] = "Apply";
        vars["talent.exit"] = "Exit";
        vars["talent.beacon"] = "Activate Beacon";
        vars["talent.detail_title"] = "Physical Power 3 / 10 points";
        vars["talent.detail"] = "Min Physical +15\nMax Physical +15\nDefense -8";
        vars["talent.detail_cost"] = "10 talent points / 40 available";
        vars["talent.detail_requirement"] = "Requires: Physical Power 2";
        vars["talent.overlay_visible"] = "true";
        vars["talent.status"] = "";
        vars["talent.detail_extra"] = "Pending / press Apply to activate";
        vars["skill.talent_tree_label"] = "Talent Tree";
        vars["ui.tab_talents"] = "Talents";
        for (int i = 0; i < 37; ++i) {
            const auto prefix = "talent.node." + std::to_string(i);
            vars[prefix + ".color"] = i < 2 ? "#76AB8B" : i == 2 ? "#E3BC6D" : "#3E454F";
            vars[prefix + ".line"] = i < 2 ? "#527760" : i == 2 ? "#A68A50" : "#343B46";
            vars[prefix + ".cost"] = i >= 25 && i < 29 ? "5 pt" : "10 pt";
        }
        vars["hud.menu.messages.tooltip"] = "Messages (L)";
        vars["hud.menu.messages.unread"] = "3";
        vars["hud.menu.messages.badge"] = "3";
        vars["mail.title"] = "Messages";
        vars["mail.inbox_label"] = "Inbox";
        vars["mail.details_label"] = "Message";
        vars["mail.summary"] = "3 messages / 2 unread";
        vars["mail.date_label"] = "Date";
        vars["mail.subject_label"] = "Subject";
        vars["mail.status_label"] = "Status";
        vars["mail.read_label"] = "Read";
        vars["mail.refresh_label"] = "Refresh";
        vars["mail.claim_label"] = "Receive attachment";
        vars["mail.delete_label"] = "Delete";
        vars["mail.previous_label"] = "<";
        vars["mail.next_label"] = ">";
        vars["mail.body_up_label"] = "Up";
        vars["mail.body_down_label"] = "Down";
        vars["mail.confirm_label"] = "Confirm";
        vars["mail.cancel_label"] = "Cancel";
        vars["mail.empty_label"] = "Your inbox is empty.";
        vars["mail.empty"] = "0";
        vars["mail.viewing"] = "1";
        vars["mail.not_viewing"] = "0";
        vars["mail.select_hint"] = "Select a message, then Read or double-click.";
        vars["mail.status"] = "";
        vars["mail.page"] = "1 / 1";
        vars["mail.confirming"] = "0";
        vars["mail.confirmation"] = "Receive this attachment?";
        vars["mail.subject"] = "Level Up Reward";
        vars["mail.sender"] = "From: System";
        vars["mail.date"] = "Sent: 2026-10-11 18:00";
        vars["mail.attachment_name"] = "Medicine";
        vars["mail.attachment_count"] = "Quantity: 3";
        vars["mail.attachment_status"] = "Attachment available";
        vars["mail.delete_hint"] = "Receive the attachment before deleting.";
        vars["mail.body_page"] = "";
        for (int row = 0; row < 8; ++row) {
            const auto key = "mail.row." + std::to_string(row) + ".";
            vars[key + "visible"] = row < 3 ? "1" : "0";
            vars[key + "selected"] = row == 0 ? "1" : "0";
            vars[key + "has_attachment"] = row < 2 ? "1" : "0";
            vars[key + "subject"] = row == 0 ? "Level Up Reward" : "A gift for your journey";
            vars[key + "date"] = "2026-10-11 18:00";
            vars[key + "status"] = row == 0 ? "Read" : "Unread";
            vars[key + "color"] = row == 0 ? "#BDAE92" : "#E5CF9E";
        }
        for (int row = 0; row < 9; ++row) {
            vars["mail.body." + std::to_string(row)] =
                row == 0 ? "Congratulations on your new level!" : "";
        }
        vars["startup.title"] = "OpenKal";
        vars["startup.hint"] = "Your journey begins here";
        vars["startup.single_player"] = "Single Player";
        vars["startup.multiplayer"] = "Multiplayer";
        vars["mode.title"] = "OpenKal";
        vars["mode.single"] = "Single Player";
        vars["mode.multi"] = "Multiplayer";
        vars["mode.quit"] = "Quit";
        vars["online.title"] = "Create account";
        vars["online.username_label"] = "Username";
        vars["online.email_label"] = "Email address";
        vars["online.password_label"] = "Password (12 to 128 bytes)";
        vars["online.confirmation_label"] = "Repeat password";
        vars["online.username"] = "ExampleUser";
        vars["online.email"] = "user@example.test";
        vars["online.password"] = "************";
        vars["online.confirmation"] = "************";
        vars["online.connect"] = "Connect";
        vars["online.login"] = "Sign in";
        vars["online.register"] = "Create account";
        vars["online.back"] = "Back";
        vars["online.controls"] = "Tab: next field. Ctrl+A/C/X/V: select/copy/cut/paste.";
        vars["online.toggle_passwords"] = "Show passwords";
        vars["online.password_feedback"] = "12 / 128 bytes - valid password";
        vars["online.password_color"] = "#8DCE9C";
        vars["online.confirmation_feedback"] = "Passwords match.";
        vars["online.confirmation_color"] = "#8DCE9C";
        for (const auto* field : {"username", "email", "password", "confirmation"}) {
            vars[std::string("online.") + field + "_background"] = "#0B100F";
        }
        vars["online.connect_visible"] = "false";
        vars["online.account_fields"] = "true";
        vars["online.registering"] = "true";
        vars["online.registration_allowed"] = "false";
        vars["online.login_visible"] = "false";
        vars["online.create_visible"] = "true";
        for (int i = 0; i < 3; ++i) {
            vars["mode.status_line" + std::to_string(i)] = i == 0 ? "Choose a play mode." : "";
            vars["online.status_line" + std::to_string(i)] =
                i == 0 ? "Create an account on the selected server." : "";
        }
        vars["startup.options"] = "Options";
        vars["startup.close"] = "Close";
        vars["selection.title"] = "Character";
        vars["selection.name"] = "TestKn8";
        vars["selection.class"] = "Knight";
        vars["selection.level"] = "Level 42";
        vars["selection.count"] = "2 / 8";
        vars["selection.attributes"] = "Attributes";
        vars["selection.previous"] = "<";
        vars["selection.next"] = ">";
        vars["selection.enter"] = "Enter World";
        vars["game_menu.title"] = "Menu";
        vars["game_menu.cancel"] = "Cancel";
        vars["game_menu.options"] = "Options";
        vars["game_menu.select_character"] = "Select Character";
        vars["game_menu.close"] = "Close";
        vars["game_menu.close_question"] = "Are you sure?";
        vars["game_menu.yes"] = "Yes";
        vars["game_menu.no"] = "No";
        vars["game_menu.main"] = "true";
        vars["game_menu.confirming"] = "false";
        vars["character_delete.title"] = "Delete Character";
        vars["character_delete.question"] = "Delete TestKn8?";
        vars["character_delete.recovery_note"] = "The character can be restored for 30 days.";
        vars["character_delete.yes"] = "Delete";
        vars["character_delete.no"] = "Cancel";
        vars["character_delete.columns"] = "Level / Name / Class / Days left";
        vars["character_delete.empty"] = "No recoverable characters.";
        vars["character_delete.restore"] = "Restore";
        vars["character_delete.close"] = "Close";
        vars["character_delete.confirming"] = "true";
        vars["character_delete.listing"] = "false";
        vars["character_delete.empty_visible"] = "false";
        vars["creation.class"] = "Knight";
        vars["creation.name"] = "NewKnight_";
        vars["creation.name_label"] = "Name";
        vars["creation.appearance_label"] = "Appearance";
        vars["creation.face_label"] = "Face";
        vars["creation.hair_label"] = "Hair";
        vars["creation.face"] = "1";
        vars["creation.hair"] = "2";
        vars["creation.stats_label"] = "Starting attributes";
        vars["creation.points"] = "5 points left";
        vars["creation.previous"] = "<";
        vars["creation.next"] = ">";
        vars["creation.minus"] = "-";
        vars["creation.plus"] = "+";
        vars["creation.back"] = "Back";
        vars["creation.create"] = "Create";
        vars["creation.status"] = "Spend all starting points.";
        vars["selection.str_label"] = "Strength";
        vars["selection.str"] = "18";
        vars["creation.str_label"] = "Strength";
        vars["creation.str"] = "18";
        vars["selection.hth_label"] = "Health";
        vars["selection.hth"] = "18";
        vars["creation.hth_label"] = "Health";
        vars["creation.hth"] = "18";
        vars["selection.intl_label"] = "Intelligence";
        vars["selection.intl"] = "18";
        vars["creation.intl_label"] = "Intelligence";
        vars["creation.intl"] = "18";
        vars["selection.wis_label"] = "Wisdom";
        vars["selection.wis"] = "18";
        vars["creation.wis_label"] = "Wisdom";
        vars["creation.wis"] = "18";
        vars["selection.dex_label"] = "Dexterity";
        vars["selection.dex"] = "18";
        vars["creation.dex_label"] = "Dexterity";
        vars["creation.dex"] = "18";
        vars["trade.title"] = "Trade with Companion";
        vars["trade.status"] = "Both players must accept. Reject cancels the trade.";
        vars["trade.invitation"] = "Would you like to trade with this player?";
        vars["trade.inventory_label"] = "Inventory";
        vars["trade.other_label"] = "Companion offers";
        vars["trade.own_label"] = "Your offer";
        vars["trade.money"] = "1250";
        vars["trade.partner_money"] = "500";
        vars["trade.own_status"] = "Not accepted";
        vars["trade.other_status"] = "Accepted";
        vars["trade.inventory_scroll"] = "0";
        vars["trade.scroll"] = "1 / 5";
        vars["trade.quantity_title"] = "Item quantity";
        vars["trade.quantity_prompt"] = "Amount (1 - 20)";
        vars["trade.quantity"] = "5|";
        vars["party.title"] = "Party";
        vars["party.invitation_title"] = "Party invitation";
        vars["party.inviter"] = "Companion";
        vars["party.invitation"] = "would like you to join their party.";
        vars["party.is_leader"] = "true";
        vars["party.role"] = "Party leader";
        vars["party.count"] = "3 / 12 members";
        vars["party.leave"] = "Leave party";
        vars["party.remove"] = "Remove";
        vars["party.invite_label"] = "Invite: Companion";
        vars["party.accept"] = "Accept";
        vars["party.decline"] = "Decline";
        vars["party.member.name"] = "* Companion";
        vars["party.member.hp_ratio"] = "0.85";
        vars["clock.title"] = "Clock";
        vars["clock.server_label"] = "Server";
        vars["clock.online_label"] = "Online";
        vars["clock.server_time"] = "20:15:42";
        vars["clock.online_time"] = "01:23:45";
        vars["clock.selected_event"] = "No event  v";
        vars["clock.countdown"] = "00:00:00:00";
        vars["clock.previous"] = "<";
        vars["clock.next"] = ">";
        vars["clock.page"] = "1/1";
        vars["clock.paging"] = "false";
        for (int row = 0; row < 6; ++row) {
            const auto key = "clock.option." + std::to_string(row);
            vars[key] = row == 0 ? "No event" : "Preview event " + std::to_string(row);
            vars[key + ".visible"] = row < 3 ? "true" : "false";
        }
        vars["settings.clock_label"] = "Show server clock";
        vars["settings.clock_checked"] = "X";
        vars["settings.misc_heading"] = "Misc";
        vars["settings.ignore_party_label"] = "Ignore party invitations";
        vars["settings.ignore_party"] = "Off";
        vars["settings.page.misc"] = "false";
        vars["settings.title"] = "Options";
        vars["settings.tab"] = "battle";
        vars["settings.display_heading"] = "Display";
        vars["settings.world_tab"] = "World";
        vars["settings.render_tab"] = "Graphics";
        vars["settings.audio_heading"] = "Audio";
        vars["settings.battle_heading"] = "Battle messages";
        vars["settings.battle_note"] =
            "Choose which events appear in the feed. Saved for this character.";
        vars["settings.page.battle"] = "true";
        for (const auto* tab : {"display", "world", "render", "audio"}) {
            vars[std::string("settings.page.") + tab] = "false";
        }
        const char* battle_labels[] = {"Damage",
                                       "Skill",
                                       "HP",
                                       "MP",
                                       "Rage",
                                       "EXP",
                                       "Animal EXP",
                                       "Item use",
                                       "Item get",
                                       "Geon"};
        for (std::size_t i = 0; i < 10; ++i) {
            const auto key = "settings.battle." + std::to_string(i);
            vars[key] = battle_labels[i];
            vars[key + ".active"] = i % 2 == 0 ? "true" : "false";
            vars[key + ".inactive"] = i % 2 == 0 ? "false" : "true";
        }
        vars["settings.battle.system"] = "System messages";
        vars["settings.battle.system.active"] = "true";
        vars["settings.battle.system.inactive"] = "false";
        vars["settings.battle_available"] = "true";
        vars["settings.battle_unavailable"] = "false";

        vars["settings.save"] = "Save";
        vars["settings.cancel"] = "Cancel";
        vars["hud.class_hovered"] = "true";
        vars["minimap.area_name"] = "Village of Narootuh";
        vars["minimap.coordinates"] = "8040, 8085";
        vars["minimap.north"] = "N";
        vars["minimap.zoom_in_label"] = "+";
        vars["minimap.zoom_out_label"] = "−";
        vars["ui.title"] = "Info";
        vars["ui.ok"] = "OK";
        vars["ui.close"] = "Close";
        vars["ui.honor"] = "Honor";
        vars["ui.ancient_animal"] = "Ancient animal";
        vars["ui.riding_title"] = "Riding animals";
        vars["riding.status_label"] = "Status";
        vars["riding.duration_label"] = "Duration";
        vars["riding.grade_label"] = "Grade";
        vars["riding.satiety_label"] = "Satiety";
        vars["riding.mount_label"] = "Riding pet";
        vars["riding.helmet_label"] = "Helmet";
        vars["riding.armor_label"] = "Armor";
        vars["riding.saddle_label"] = "Saddle";
        vars["riding.cloth_label"] = "Saddle cloth";
        vars["riding.magnet_label"] = "Magnet";
        vars["riding.feed_label"] = "Feed";
        vars["riding.combine_label"] = "Combine";
        vars["riding.ability_label"] = "Ability +";
        vars["riding.reroll_label"] = "Reroll";
        vars["riding.enchant_label"] = "Enchant first gear";
        vars["riding.upgrade_label"] = "Upgrade +10 gear";
        vars["riding.registered"] = "true";
        vars["riding.empty"] = "false";
        vars["riding.common"] = "false";
        vars["riding.battle"] = "true";
        vars["riding.name"] = "Battle Riding Horse";
        vars["riding.status"] = "Registered";
        vars["riding.remaining"] = "Permanent";
        vars["riding.grade"] = "Rare";
        vars["riding.rarity"] = "Green";
        vars["riding.grade_line"] = "Rare (Green)";
        vars["riding.satiety"] = "83 / 100";
        vars["riding.satiety_ratio"] = "0.83";
        vars["riding.equipment_hint"] = "Drag gear in/out. Right-click to remove.";
        vars["riding.hint"] = "Press R to mount or dismount.";
        vars["stablemaster.open"] = "true";
        vars["stablemaster.title"] = "Stablemaster services";
        vars["stablemaster.seals"] = "Riding Seals: 12";
        vars["stablemaster.exchange_label"] = "Horse (2 Seals)";
        vars["stablemaster.promote_label"] = "Promote (6 Seals)";
        vars["stablemaster.grade_label"] = "Grade (10 Seals)";
        vars["stablemaster.writ_label"] = "Time Writ (+7d)";
        vars["stablemaster.duplicate_label"] = "Convert (+2 Seals)";
        vars["stablemaster.duplicate_selection"] = "Drag Pet";
        vars["ui.point"] = "Point";
        vars["player.name"] = "Preview";
        vars["player.name_label"] = "Name";
        vars["player.job"] = "Magician";
        vars["player.job_label"] = "Job";
        vars["player.guild"] = "-";
        vars["player.guild_label"] = "Guild";
        vars["player.guild_pos"] = "-";
        vars["player.guild_pos_label"] = "Pos";
        vars["player.gov_pos"] = "-";
        vars["player.gov_pos_label"] = "G.P";
        vars["player.awaken"] = "Awaken -";
        vars["player.level"] = "50";
        vars["player.level_label"] = "Level";
        vars["player.ctb"] = "0";
        vars["player.ctb_label"] = "CTB";
        vars["stats.str"] = "20";
        vars["stats.str_label"] = "STR";
        vars["stats.hth"] = "42";
        vars["stats.hth_label"] = "HTH";
        vars["stats.intl"] = "40";
        vars["stats.intl_label"] = "INT";
        vars["stats.wis"] = "22";
        vars["stats.wis_label"] = "WIS";
        vars["stats.dex"] = "15";
        vars["stats.dex_label"] = "AGI";
        vars["stats.pu_remaining"] = "5";
        vars["stats.pu_point_label"] = "Point";
        vars["vital.hp"] = "6000";
        vars["vital.max_hp"] = "12000";
        vars["vital.mp"] = "2500";
        vars["vital.max_mp"] = "5000";
        vars["vital.hp_ratio"] = "0.5";
        vars["vital.mp_ratio"] = "0.5";
        vars["skillbar.active_label"] = "ACTIVE";
        vars["skillbar.auto_hp_label"] = "AUTO HP";
        vars["skillbar.auto_hp_percent"] = "50";
        vars["skillbar.auto_hp_status"] = "AUTO HP 50%";
        for (const int percent : {25, 50, 75, 100}) {
            vars["skillbar.auto_hp_" + std::to_string(percent)] = percent == 50 ? "true" : "false";
        }
        vars["exp.pct"] = "0.42";
        vars["vital.hp_label"] = "HP";
        vars["vital.mp_label"] = "MP";
        vars["vital.hp_text"] = "800/800";
        vars["vital.mp_text"] = "400/400";
        vars["combat.attack"] = "40~55";
        vars["combat.attack_label"] = "ATK";
        vars["combat.mattack"] = "60~80";
        vars["combat.mattack_label"] = "Mg.ATK";
        vars["combat.hit"] = "30";
        vars["combat.hit_label"] = "OTP";
        vars["combat.evasion"] = "12";
        vars["combat.evasion_label"] = "Eva";
        vars["combat.defense"] = "20";
        vars["combat.defense_label"] = "DEF";
        vars["combat.absorb"] = "5";
        vars["combat.absorb_label"] = "Absorb.(%)";
        vars["resist.fire"] = "0";
        vars["resist.fire_label"] = "Fire";
        vars["resist.ice"] = "0";
        vars["resist.ice_label"] = "Ice";
        vars["resist.lightning"] = "0";
        vars["resist.lightning_label"] = "Lightning";
        vars["resist.curse"] = "0";
        vars["resist.curse_label"] = "Curse";
        vars["resist.palsy"] = "0";
        vars["resist.palsy_label"] = "Non Elemental";
        vars["inventory.count_text"] = "3/72";
        vars["collection.title"] = "Collection";
        vars["collection.absorb"] = "Absorb";
        vars["collection.absorb_help"] = "Select a bag Taegeuk, then Absorb.";
        vars["collection.taegeuk"] = "Taegeuk";
        vars["collection.hanin"] = "Hanin's Mirror";
        vars["collection.hanin_help"] = "Mirror + five essences. Drag to equip.";
        vars["collection.yeoiju"] = "Yeoiju";
        vars["collection.attack"] = "Attack";
        vars["collection.defense"] = "Defense";
        vars["collection.pvp_notice"] = "PvP: only movement speed applies.";
        constexpr const char* collection_names[] = {
            "Mirror",  "Geon",   "Tae", "I", "Jin", "Son", "Gam", "Gan", "Gon",
            "Taegeuk", "Mirror", "1",   "2", "3",   "4",   "5",   "1",   "2",
            "3",       "4",      "5",   "1", "2",   "3",   "4",   "5"};
        for (int i = 0; i < 26; ++i) {
            vars["collection.empty." + std::to_string(i)] = collection_names[i];
        }
        for (int page = 1; page <= 6; ++page) {
            vars["inventory.page" + std::to_string(page) + "_shaded"] = page > 2 ? "true" : "false";
        }
        vars["inventory.bag_active"] = "true";
        vars["inventory.expand_label"] = "Expand +36";
        vars["inventory.destroy_label"] = "Destroy";
        vars["inventory.general_capacity"] = "General: 72 / 216 slots unlocked";
        vars["inventory.general_hint"] = "72 permanent slots + four timed expansions";
        vars["inventory.other_capacity"] = "Other: 72 permanent slots";
        for (const char* category :
             {"consumables", "weapons", "armors", "enchantment", "skins_pets", "other"}) {
            const std::string key = std::string("inventory.filter.") + category;
            vars[key] = "false";
            vars[key + "_visible"] = "false";
        }
        vars["inventory.quest_label"] = "Quest Items  8 / 36";
        vars["inventory.task_label"] = "Task Items  12 / 36";
        constexpr const char* wear_ids[] = {"helmet",
                                            "upper",
                                            "gauntlet",
                                            "pants",
                                            "boots",
                                            "weapon",
                                            "shield",
                                            "ring",
                                            "necklace",
                                            "accessory",
                                            "face",
                                            "sinsu",
                                            "pet",
                                            "ride"};
        constexpr const char* wear_labels[] = {"Helmet",
                                               "Armor",
                                               "Gloves",
                                               "Pants",
                                               "Boots",
                                               "Weapon",
                                               "Shield",
                                               "Ring",
                                               "Necklace",
                                               "Accessory I",
                                               "Mask",
                                               "Sin Su",
                                               "Pet",
                                               "Mount"};
        for (int i = 0; i < 14; ++i) {
            vars[std::string("inventory.wear_label.") + wear_ids[i]] = wear_labels[i];
        }
        for (int page = 0; page < 6; ++page) {
            vars["inventory.page" + std::to_string(page + 1) + "_label"] =
                std::to_string(page * 36 + 1) + "-" + std::to_string((page + 1) * 36) +
                (page < 2 ? "  Permanent" : "  Locked");
        }
        vars["inventory.skin_label"] = "Skins";
        vars["inventory.skin_locked_label"] = "Locked";
        for (const char* part :
             {"helmet", "upper", "pants", "boots", "gauntlet", "face", "weapon", "shield"}) {
            vars[std::string("inventory.skin_locked.") + part] = "false";
        }
        vars["inventory.equipment_label"] = "Equipment";
        vars["inventory.sort_label"] = "Sort inventory";
        vars["inventory.money_label"] = "Geon";
        for (int page = 1; page <= 6; ++page) {
            vars["inventory.page" + std::to_string(page) + "_locked"] = page > 2 ? "true" : "false";
        }
        vars["inventory.money"] = "12345";
        vars["inventory.tab"] = "normal";
        vars["inventory.normal_active"] = "true";
        vars["inventory.quest_active"] = "false";
        vars["inventory.register"] = "0";
        vars["inventory.scroll"] = "0";
        vars["inventory.scroll_up_label"] = "^";
        vars["inventory.scroll_down_label"] = "v";
        vars["character.attributes"] = "Attributes";
        vars["character.combat"] = "Combat";
        vars["character.resistances"] = "Resistances";
        vars["character.active_effects"] = "Active Effects";
        vars["stats.str_pending"] = "false";
        vars["stats.hth_pending"] = "false";
        vars["stats.intl_pending"] = "false";
        vars["stats.wis_pending"] = "false";
        vars["stats.dex_pending"] = "false";
        vars["vital.hp_pending"] = "false";
        vars["vital.mp_pending"] = "false";
        vars["combat.attack_pending"] = "false";
        vars["combat.mattack_pending"] = "false";
        vars["combat.hit_pending"] = "false";
        vars["combat.evasion_pending"] = "false";
        vars["ui.tab_normal"] = "General";
        vars["ui.tab_task"] = "Other";
        vars["skill.title"] = "Skills";
        vars["skill.tab"] = "common";
        vars["skill.action_page"] = "false";
        vars["skill.class_page"] = "true";
        vars["ui.tab_common"] = "Common";
        vars["ui.tab_job1"] = "1st";
        vars["ui.tab_job2"] = "2nd";
        vars["ui.tab_job3"] = "3rd";
        vars["ui.tab_awaken"] = "Awaken";
        vars["ui.tab_action"] = "Action";
        vars["ui.active"] = "Active";
        vars["ui.passive"] = "Passive";
        vars["ui.skill_chart"] = "Chart";
        vars["ui.message"] = "Message";
        vars["ui.private_shop"] = "Private shop";
        vars["ui.start_mixing"] = "Start Mixing";
        vars["mix.can_start_label"] = vars["ui.start_mixing"];
        vars["mix.title"] = "Mix Master";
        vars["mix.mode"] = "Elemental Mixing";
        vars["mix.placement_hint"] = "Place a weapon in the center and materials around it.";
        vars["mix.weapon_label"] = "Weapon";
        vars["mix.remove_hint"] = "Click a placed item to remove it.";
        vars["mix.status"] = "Place the weapon you want to mix in the center.";
        vars["mix.can_start"] = "false";
        vars["mix_list.title"] = "Mixing Item List";
        vars["mix_list.upgrade_label"] = "Upgrade Weapon";
        vars["mix_list.create_label"] = "Create Weapon";
        vars["mix_list.upgrade_color"] = "#E5CF9E";
        vars["mix_list.create_color"] = "#BDAE92";
        vars["mix_list.description"] = "Materials for upgrading a weapon's mix grade";
        vars["mix_list.weapon_label"] = "Main weapon";
        vars["mix_list.extra_label"] = "Extra weapon";
        vars["mix_list.armor_label"] = "Armor";
        vars["mix_list.stones_label"] = "Stones";
        vars["mix_list.gold_label"] = "Gold";
        vars["mix_list.previous_label"] = "Previous";
        vars["mix_list.next_label"] = "Next";
        vars["mix_list.page"] = "1 / 2";
        vars["mix_list.empty"] = "false";
        vars["mix_list.empty_label"] = "No mixing recipes are available.";
        vars["mix_list.note"] =
            "G3: use a mix Grade 2 weapon and two Mystery or Demon's Blood stones.";
        vars["mix_list.stone_note"] =
            "* Use two stones of the same type for G3; ordinary upgrades use one elemental stone.";
        vars["mix_list.warning"] =
            "Extra weapon, armor and stones are consumed even if mixing fails.";
        const int mix_preview_grades[][3] = {{40, 40, 42},
                                             {45, 45, 42},
                                             {48, 48, 46},
                                             {50, 50, 46},
                                             {53, 50, 50},
                                             {56, 53, 50},
                                             {59, 53, 50},
                                             {62, 56, 50},
                                             {65, 59, 50}};
        for (int i = 0; i < 9; ++i) {
            const auto& grades = mix_preview_grades[i];
            const std::string prefix = "mix_list.row." + std::to_string(i) + ".";
            vars[prefix + "visible"] = "true";
            vars[prefix + "weapon"] = "Grade " + std::to_string(grades[0]);
            vars[prefix + "extra"] = "Grade " + std::to_string(grades[1]);
            vars[prefix + "armor"] = "Grade " + std::to_string(grades[2]) +
                                     (i == 1 || i == 3 || i >= 7 ? " chest" : " armor");
            vars[prefix + "stones"] = "1 elemental";
            vars[prefix + "gold"] = "0";
        }
        vars["ui.transform"] = "Transform";
        vars["ui.animal_skills"] = "Animal skills";
        vars["ui.equip_egg_hint"] = "Equip an egg to transform.";
        vars["ui.blacksmith"] = "Blacksmith";
        vars["ui.revise"] = "Revise";
        vars["ui.explosive_blow"] = "Explosive Blow";
        vars["transform.equipped"] = "true";
        vars["transform.empty"] = "false";
        vars["transform.name"] = "White Tiger";
        vars["transform.level_line"] = "Lv. 1";
        vars["transform.str"] = "3";
        vars["transform.hth"] = "2";
        vars["transform.dex"] = "1";
        vars["transform.intl"] = "0";
        vars["transform.wis"] = "0";
        vars["chat.mode"] = "Normal";
        vars["chat.mode_color"] = "#EBEBE0";
        vars["chat.tab"] = "normal";
        vars["chat.tab_normal"] = "Normal";
        vars["chat.tab_party"] = "Party";
        vars["chat.tab_guild"] = "Guild";
        vars["chat.tab_alliance"] = "Alliance";
        vars["chat.tab_squad"] = "Squad";
        vars["chat.tab_shout"] = "Shout";
        vars["chat.focused"] = "false";
        vars["chat.history_open"] = "false";
        vars["chat.history_hint"] = "Wheel: history | Enter: command | Esc: close";
        vars["exp.player_pct"] = "0.42";
        vars["exp.sinsu_pct"] = "0.18";
        vars["npc.name"] = "Guard";
        vars["npc.closable"] = "true";
        vars["npc.paged"] = "true";
        vars["npc.page"] = "1 / 2";
        vars["npc.previous_label"] = "Previous";
        vars["npc.next_label"] = "Next";
        vars["npc.body"] = "Welcome, traveler.";
        vars["npc.link_label"] = "Quest";
        vars["crafting.title"] = "Medicine Crafting";
        vars["crafting.make_label"] = "Make";
        vars["crafting.close_label"] = "Close";
        vars["crafting.hint"] =
            "Each craft makes one item. Ingredients are consumed only on success.";
        vars["crafting.empty"] = "false";
        vars["crafting.empty_label"] = "Recipes for this family will be added later.";
        for (int row = 0; row < 3; ++row) {
            const std::string prefix = "crafting.row." + std::to_string(row) + ".";
            const char* sizes[] = {"Small", "Medium", "Large", "Huge"};
            vars[prefix + "visible"] = "true";
            vars[prefix + "title"] = std::string("1x ") + sizes[row + 1] + " Medicine";
            vars[prefix + "materials"] =
                std::string("2x ") + sizes[row] + " Medicine + 1x Aloe Vera";
            vars[prefix + "owned"] =
                std::string("Owned: ") + sizes[row] + " Medicine: 8   |   Aloe Vera: 3";
            vars[prefix + "color"] = "#B5D991";
        }
        vars["speed_medicine.title"] = "Chemist of Speed-up Medicine";
        vars["speed_medicine.from_blood_fish_label"] =
            "Speed-up Medicine (use Sea Bream with Blood)";
        vars["speed_medicine.blood_fish_label"] = "Sea Bream with Blood (use raw materials)";
        vars["speed_medicine.from_blood_fish"] = "1";
        vars["speed_medicine.blood_fish"] = "0";
        vars["speed_medicine.requirements_label"] = "Requirements";
        vars["speed_medicine.owned_label"] = "Owned";
        vars["speed_medicine.required_label"] = "Required";
        vars["speed_medicine.requirements.demon_blood.label"] = "Blood";
        vars["speed_medicine.requirements.demon_blood.required"] = "0";
        vars["speed_medicine.requirements.demon_blood.owned"] = "12";
        vars["speed_medicine.requirements.sea_bream.label"] = "Sea Bream";
        vars["speed_medicine.requirements.sea_bream.required"] = "0";
        vars["speed_medicine.requirements.sea_bream.owned"] = "3";
        vars["speed_medicine.requirements.bamboo_keg.label"] = "Keg";
        vars["speed_medicine.requirements.bamboo_keg.required"] = "0";
        vars["speed_medicine.requirements.bamboo_keg.owned"] = "2";
        vars["speed_medicine.requirements.blood_fish.label"] = "Sea Bream with Blood";
        vars["speed_medicine.requirements.blood_fish.required"] = "1";
        vars["speed_medicine.requirements.blood_fish.owned"] = "1";
        vars["speed_medicine.requirements.geon.label"] = "Geon";
        vars["speed_medicine.requirements.geon.required"] = "500";
        vars["speed_medicine.requirements.geon.owned"] = "12000";
        vars["speed_medicine.materials"] = "Need: Blood 0/12   Sea Bream 0/3   Keg 0/2";
        vars["speed_medicine.fee"] = "Sea Bream with Blood 1/1   Geon 500/12000";
        vars["speed_medicine.hint"] = "Production takes 10 seconds. Esc cancels it.";
        vars["speed_medicine.make_label"] = "Make";
        vars["quest.title"] = "Quest Journal";
        vars["quest.active_label"] = "Active quests";
        vars["quest.details_label"] = "Quest details";
        vars["quest.count"] = "3";
        vars["quest.selected_name"] = "Nou-Woong's Money";
        vars["quest.selected_body"] = "Collect the requested items.";
        vars["quest.item_progress"] = "Nou-Woong's Money 0 / 1";
        vars["quest.have"] = "0";
        vars["quest.need"] = "1";
        vars["blacksmith.title"] = "Explosive Blow";
        vars["blacksmith.can_revise"] = "true";
        vars["blacksmith.can_eb"] = "true";
        vars["blacksmith.weapon_name"] = "Training Sword";
        vars["blacksmith.weapon_hint"] = "Grade 50. Click the icon to remove.";
        vars["blacksmith.placement_hint"] = "Drag a weapon from your inventory into the slot.";
        vars["blacksmith.level_label"] = "Level / maximum";
        vars["blacksmith.level"] = "3";
        vars["blacksmith.cap"] = "5";
        vars["blacksmith.chance_label"] = "Success chance";
        vars["blacksmith.chance"] = "75%";
        vars["blacksmith.stones_label"] = "Stones (owned / needed)";
        vars["blacksmith.stones_owned"] = "24";
        vars["blacksmith.stones_required"] = "4";
        vars["blacksmith.fee_label"] = "Geon (owned / fee)";
        vars["blacksmith.geon_owned"] = "12000";
        vars["blacksmith.fee"] = "6000";
        vars["blacksmith.status"] = "Use Stones of Revision to improve the chance.";
        vars["blacksmith.warning"] = "Failed upgrades can destroy the weapon.";
        vars["blacksmith.revise_label"] = "Revise chance";
        vars["blacksmith.upgrade_label"] = "Apply Explosive Blow";
        vars["shop.title"] = "Merchant Shop";
        vars["shop.inventory_label"] = "Inventory";
        vars["shop.offers_label"] = "Merchant";
        vars["shop.money"] = "12345";
        vars["shop.money_label"] = "Geon";
        vars["shop.selected_name"] = "Medium Medicine";
        vars["shop.selected_price"] = "Buy: 70";
        vars["shop.offers_page"] = "1/1";
        vars["shop.inventory_register"] = "0";
        vars["shop.inventory_count"] = "3";
        vars["shop.selection_hint"] = "CTRL: select multiple items";
        vars["shop.quantity_open"] = "false";
        vars["shop.buy_quantity_open"] = "false";
        vars["shop.quantity_prompt"] = "Buy quantity";
        vars["shop.quantity"] = "1";
        vars["storage.title"] = "Account Storage";
        vars["storage.inventory_label"] = "Inventory";
        vars["storage.storage_label"] = "Storage";
        vars["storage.sort_label"] = "Sort";
        vars["storage.inventory.scroll"] = "0";
        vars["storage.items.scroll"] = "0";
        vars["storage.money"] = "12345";
        vars["storage.account_money"] = "5000";
        vars["storage.fee"] = "Item deposit: 60 Geon | Ctrl-click to mark more";
        vars["storage.count_text"] = "12/216";
        vars["storage.selected_name"] = "Select an item";
        vars["storage.quantity_open"] = "false";
        vars["storage.quantity_prompt"] = "Deposit quantity";
        vars["storage.quantity_hint"] = "Type an amount, then press Enter";
        vars["storage.quantity"] = "1";
    }

    std::vector<std::string> ui_parse_variable_keys(std::string_view markdown) {
        std::vector<std::string> keys;
        std::string line;
        std::istringstream in{std::string(markdown)};
        while (std::getline(in, line)) {
            const std::size_t tick = line.find("`");
            if (tick == std::string::npos) {
                continue;
            }
            const std::size_t tick2 = line.find("`", tick + 1);
            if (tick2 == std::string::npos) {
                continue;
            }
            std::string key = line.substr(tick + 1, tick2 - tick - 1);
            if (key.find('.') == std::string::npos) {
                continue;
            }
            if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
                keys.push_back(std::move(key));
            }
        }
        return keys;
    }

    int ui_primary_index(std::string_view bind) {
        if (bind == "stats.str") {
            return 0;
        }
        if (bind == "stats.hth") {
            return 1;
        }
        if (bind == "stats.intl") {
            return 2;
        }
        if (bind == "stats.wis") {
            return 3;
        }
        if (bind == "stats.dex") {
            return 4;
        }
        return -1;
    }

    bool UiDocument::load(const platform::Json& root) {
        if (!root.is_object()) {
            return false;
        }
        m_schema = json_s(root, "schema");
        if (m_schema != "openkal.ui/v1") {
            return false;
        }
        m_chrome_close_button = true;
        m_close_override = false;
        m_doc_note = json_s(root, "_doc");
        m_id = json_s(root, "id");
        const std::string kind = json_s(root, "kind", "window");
        m_kind = kind == "hud" ? UiKind::Hud : UiKind::Window;
        if (const auto* win = root.get("window"); win && win->is_object()) {
            m_window_w = json_f(*win, "w", m_window_w);
            m_window_h = json_f(*win, "h", m_window_h);
            m_draggable = json_b(*win, "draggable", true);
            m_title_drag_h = json_f(*win, "title_drag_h", m_title_drag_h);
        }
        if (const auto* theme = root.get("theme"); theme && theme->is_object()) {
            m_font_pixel_size = json_i(*theme, "font_pixel_size", m_font_pixel_size);
            if (const auto* colors = theme->get("colors"); colors && colors->is_object()) {
                m_color_label = json_s(*colors, "label", m_color_label);
                m_color_value = json_s(*colors, "value", m_color_value);
                m_color_pending = json_s(*colors, "pending", m_color_pending);
                m_color_title = json_s(*colors, "title", m_color_title);
            }
        }
        if (const auto* chrome = root.get("chrome"); chrome && chrome->is_object()) {
            const std::string ck = json_s(*chrome, "kind", "nine_slice");
            if (ck == "bitmap") {
                m_chrome_kind = UiChromeKind::Bitmap;
            } else if (ck == "pieces") {
                m_chrome_kind = UiChromeKind::Pieces;
            } else {
                m_chrome_kind = UiChromeKind::NineSlice;
            }
            m_chrome_preset = json_s(*chrome, "preset", "info_menu");
            const bool dragon = m_chrome_preset == "dragon_dialog";
            if (m_chrome_preset == "dragon_compact") {
                m_pad_left = m_pad_right = m_pad_top = m_pad_bottom = 5.0f;
                m_chrome_close_button = false;
            }
            if (dragon) {
                m_pad_left = m_pad_right = 40.0f;
                m_pad_top = 84.0f;
                m_pad_bottom = 56.0f;
                m_chrome_close_button = false;
            }
            const std::string edges =
                json_s(*chrome,
                       "edges",
                       (dragon || m_chrome_preset == "dragon_compact") ? "repeat" : "tile");
            m_chrome_edges = edges == "repeat"    ? UiChromeEdges::Repeat
                             : edges == "stretch" ? UiChromeEdges::Stretch
                                                  : UiChromeEdges::Tile;
            m_pad_left = json_f(*chrome, "pad_left", m_pad_left);
            m_pad_top = json_f(*chrome, "pad_top", m_pad_top);
            m_pad_right = json_f(*chrome, "pad_right", m_pad_right);
            m_pad_bottom = json_f(*chrome, "pad_bottom", m_pad_bottom);
            m_chrome_close_button = json_b(*chrome, "close", m_chrome_close_button);
            m_chrome_file = json_s(*chrome, "file");
            m_fallback_w = json_f(*chrome, "fallback_w", m_fallback_w);
            m_fallback_h = json_f(*chrome, "fallback_h", m_fallback_h);
            m_chrome_files = ui_chrome_preset_files(m_chrome_preset);
            if (const auto* files = chrome->get("files"); files && files->is_object()) {
                const std::string ul = json_s(*files, "upleft");
                if (!ul.empty()) {
                    m_chrome_files.set_upleft(ul);
                }
                const std::string uc = json_s(*files, "upcenter");
                if (!uc.empty()) {
                    m_chrome_files.set_upcenter(uc);
                }
                const std::string ur = json_s(*files, "upright");
                if (!ur.empty()) {
                    m_chrome_files.set_upright(ur);
                }
                const std::string lf = json_s(*files, "left");
                if (!lf.empty()) {
                    m_chrome_files.set_left(lf);
                }
                const std::string rf = json_s(*files, "right");
                if (!rf.empty()) {
                    m_chrome_files.set_right(rf);
                }
                const std::string dl = json_s(*files, "downleft");
                if (!dl.empty()) {
                    m_chrome_files.set_downleft(dl);
                }
                const std::string dc = json_s(*files, "downcenter");
                if (!dc.empty()) {
                    m_chrome_files.set_downcenter(dc);
                }
                const std::string dr = json_s(*files, "downright");
                if (!dr.empty()) {
                    m_chrome_files.set_downright(dr);
                }
            }
        }
        if (const auto* close = root.get("close"); close && close->is_object()) {
            m_close_override = true;
            m_close_ox = json_f(*close, "ox", m_close_ox);
            m_close_oy = json_f(*close, "oy", m_close_oy);
            m_close_w = json_f(*close, "w", m_close_w);
            m_close_h = json_f(*close, "h", m_close_h);
        }
        m_widgets.clear();
        if (const auto* widgets = root.get("widgets"); widgets && widgets->is_array()) {
            for (const auto& j : widgets->array()) {
                if (!j.is_object()) {
                    continue;
                }
                UiWidget w;
                load_widget(w, j);
                m_widgets.push_back(std::move(w));
            }
        }
        return true;
    }

    platform::Json UiDocument::to_json() const {
        platform::Json root;
        root.set_type(platform::Json::Type::Object);
        if (!m_doc_note.empty()) {
            set_string(root, "_doc", m_doc_note);
        }
        set_string(root, "schema", "openkal.ui/v1");
        set_string(root, "id", m_id);
        set_string(root, "kind", m_kind == UiKind::Hud ? "hud" : "window");

        platform::Json win;
        win.set_type(platform::Json::Type::Object);
        set_number(win, "w", m_window_w);
        set_number(win, "h", m_window_h);
        set_bool(win, "draggable", m_draggable);
        set_number(win, "title_drag_h", m_title_drag_h);
        root.object()["window"] = std::move(win);

        platform::Json colors;
        colors.set_type(platform::Json::Type::Object);
        set_string(colors, "label", m_color_label);
        set_string(colors, "value", m_color_value);
        set_string(colors, "pending", m_color_pending);
        set_string(colors, "title", m_color_title);
        platform::Json theme;
        theme.set_type(platform::Json::Type::Object);
        set_number(theme, "font_pixel_size", m_font_pixel_size);
        theme.object()["colors"] = std::move(colors);
        root.object()["theme"] = std::move(theme);

        platform::Json chrome;
        chrome.set_type(platform::Json::Type::Object);
        const char* ck = "nine_slice";
        if (m_chrome_kind == UiChromeKind::Bitmap) {
            ck = "bitmap";
        } else if (m_chrome_kind == UiChromeKind::Pieces) {
            ck = "pieces";
        }
        set_string(chrome, "kind", ck);
        set_string(chrome, "preset", m_chrome_preset);
        set_string(chrome,
                   "edges",
                   m_chrome_edges == UiChromeEdges::Repeat    ? "repeat"
                   : m_chrome_edges == UiChromeEdges::Stretch ? "stretch"
                                                              : "tile");
        set_number(chrome, "pad_left", m_pad_left);
        set_number(chrome, "pad_top", m_pad_top);
        set_number(chrome, "pad_right", m_pad_right);
        set_number(chrome, "pad_bottom", m_pad_bottom);
        set_bool(chrome, "close", m_chrome_close_button);
        if (!m_chrome_file.empty()) {
            set_string(chrome, "file", m_chrome_file);
        }
        if (m_fallback_w > 0.0f) {
            set_number(chrome, "fallback_w", m_fallback_w);
        }
        if (m_fallback_h > 0.0f) {
            set_number(chrome, "fallback_h", m_fallback_h);
        }
        root.object()["chrome"] = std::move(chrome);

        if (m_close_override) {
            platform::Json close;
            close.set_type(platform::Json::Type::Object);
            set_number(close, "ox", m_close_ox);
            set_number(close, "oy", m_close_oy);
            set_number(close, "w", m_close_w);
            set_number(close, "h", m_close_h);
            root.object()["close"] = std::move(close);
        }

        platform::Json arr;
        arr.set_type(platform::Json::Type::Array);
        for (const auto& w : m_widgets) {
            platform::Json j;
            j.set_type(platform::Json::Type::Object);
            set_string(j, "type", ui_widget_type_name(w.type()));
            if (!w.doc_note().empty()) {
                set_string(j, "_doc", w.doc_note());
            }
            if (!w.id().empty()) {
                set_string(j, "id", w.id());
            }
            set_number(j, "x", w.x());
            set_number(j, "y", w.y());
            if (w.w() != 0.0f) {
                set_number(j, "w", w.w());
            }
            if (w.h() != 0.0f) {
                set_number(j, "h", w.h());
            }
            if (!w.visible()) {
                set_bool(j, "visible", false);
            }
            if (!w.text().empty()) {
                set_string(j, "text", w.text());
            }
            if (!w.color().empty()) {
                set_string(j, "color", w.color());
            }
            if (!w.file().empty()) {
                set_string(j, "file", w.file());
            }
            if (w.fill_mode() != "lcr") {
                set_string(j, "fill_mode", w.fill_mode());
            }
            if (w.direction() != "left_to_right") {
                set_string(j, "direction", w.direction());
            }
            if (w.source().w > 0 && w.source().h > 0) {
                auto& source = j.object()["source"];
                source.set_type(platform::Json::Type::Object);
                set_number(source, "x", w.source().x);
                set_number(source, "y", w.source().y);
                set_number(source, "w", w.source().w);
                set_number(source, "h", w.source().h);
            }
            if (w.repeat()) {
                set_bool(j, "repeat", true);
            }
            if (!w.bind().empty()) {
                set_string(j, "bind", w.bind());
            }
            if (!w.action().empty()) {
                set_string(j, "action", w.action());
            }
            if (!w.label().empty()) {
                set_string(j, "label", w.label());
            }
            if (!w.file_default().empty()) {
                set_string(j, "file_default", w.file_default());
            }
            if (!w.file_over().empty()) {
                set_string(j, "file_over", w.file_over());
            }
            if (!w.file_on().empty()) {
                set_string(j, "file_on", w.file_on());
            }
            if (!w.file_off().empty()) {
                set_string(j, "file_off", w.file_off());
            }
            if (!w.page().empty()) {
                set_string(j, "page", w.page());
            }
            if (!w.filter().empty()) {
                set_string(j, "filter", w.filter());
            }
            if (!w.viewport().empty()) {
                set_string(j, "viewport", w.viewport());
                set_string(j, "scroll_bind", w.scroll_bind());
            }
            if (w.tooltip()) {
                set_bool(j, "tooltip", true);
            }
            if (w.drag()) {
                set_bool(j, "drag", true);
            }
            if (w.x_from_right() >= 0.0f) {
                set_number(j, "x_from_right", w.x_from_right());
            }
            if (w.y_from_bottom() >= 0.0f) {
                set_number(j, "y_from_bottom", w.y_from_bottom());
            }
            if (w.thickness() > 0.0f) {
                set_number(j, "thickness", w.thickness());
            }
            if (!w.border_top().empty()) {
                set_string(j, "top", w.border_top());
            }
            if (!w.border_bottom().empty()) {
                set_string(j, "bottom", w.border_bottom());
            }
            if (!w.border_left().empty()) {
                set_string(j, "left", w.border_left());
            }
            if (!w.border_right().empty()) {
                set_string(j, "right", w.border_right());
            }
            if (!w.select_color().empty()) {
                set_string(j, "select_color", w.select_color());
            }
            if (w.multiline()) {
                set_bool(j, "multiline", true);
            }
            if ((w.type() == UiWidgetType::List || w.multiline()) && w.line_height() != 14.0f) {
                set_number(j, "line_height", w.line_height());
            }
            if (w.cx() != 0.0f) {
                set_number(j, "cx", w.cx());
            }
            if (w.cy() != 0.0f) {
                set_number(j, "cy", w.cy());
            }
            if (w.inner() != 0.0f) {
                set_number(j, "inner", w.inner());
            }
            if (w.pixel_size() > 0) {
                set_number(j, "pixel_size", w.pixel_size());
            }
            if (w.bold()) {
                set_bool(j, "bold", true);
            }
            if (w.align() != "left" && !w.align().empty()) {
                set_string(j, "align", w.align());
            }
            if (w.alpha() != 1.0f) {
                set_number(j, "alpha", w.alpha());
            }
            if (!w.canvas_kind().empty()) {
                set_string(j, "kind", w.canvas_kind());
            }
            if (w.shape() != "rect") {
                set_string(j, "shape", w.shape());
            }
            if (!w.color_on().empty()) {
                set_string(j, "color_on", w.color_on());
            }
            if (!w.color_off().empty()) {
                set_string(j, "color_off", w.color_off());
            }
            if (!w.pending_when().empty()) {
                set_string(j, "pending_when", w.pending_when());
            }
            if (!w.visible_when().empty()) {
                set_string(j, "visible_when", w.visible_when());
            }
            if (w.type() == UiWidgetType::IconGrid) {
                set_number(j, "cols", w.cols());
                set_number(j, "rows", w.rows());
                platform::Json cell;
                cell.set_type(platform::Json::Type::Object);
                set_number(cell, "w", w.cell_w());
                set_number(cell, "h", w.cell_h());
                j.object()["cell"] = std::move(cell);
                platform::Json pitch;
                pitch.set_type(platform::Json::Type::Object);
                set_number(pitch, "x", w.pitch_x());
                set_number(pitch, "y", w.pitch_y());
                j.object()["pitch"] = std::move(pitch);
            }
            if (!w.empty_file().empty()) {
                platform::Json empty;
                empty.set_type(platform::Json::Type::Object);
                set_string(empty, "file", w.empty_file());
                j.object()["empty"] = std::move(empty);
            }
            if (!w.badge_text().empty()) {
                platform::Json badge;
                badge.set_type(platform::Json::Type::Object);
                set_string(badge, "text", w.badge_text());
                if (!w.badge_visible_when().empty()) {
                    set_string(badge, "visible_when", w.badge_visible_when());
                }
                j.object()["badge"] = std::move(badge);
            }
            if (!w.frame_file().empty()) {
                platform::Json frame;
                frame.set_type(platform::Json::Type::Object);
                set_string(frame, "file", w.frame_file());
                set_number(frame, "w", w.frame_w());
                set_number(frame, "h", w.frame_h());
                j.object()["frame"] = std::move(frame);
            }
            if (!w.accept().empty()) {
                platform::Json acc;
                acc.set_type(platform::Json::Type::Array);
                for (const auto& tag : w.accept()) {
                    platform::Json t;
                    t.set_type(platform::Json::Type::String);
                    t.string() = tag;
                    acc.array().push_back(std::move(t));
                }
                j.object()["accept"] = std::move(acc);
            }
            if (!w.tabs().empty()) {
                platform::Json items;
                items.set_type(platform::Json::Type::Array);
                for (const auto& tab : w.tabs()) {
                    platform::Json it;
                    it.set_type(platform::Json::Type::Object);
                    set_string(it, "id", tab.id());
                    set_string(it, "label", tab.label());
                    set_string(it, "file_on", tab.file_on());
                    set_string(it, "file_off", tab.file_off());
                    set_number(it, "x", tab.x());
                    set_number(it, "w", tab.w());
                    items.array().push_back(std::move(it));
                }
                j.object()["items"] = std::move(items);
            }
            if (w.label_ox() != 0.0f) {
                set_number(j, "label_ox", w.label_ox());
            }
            if (w.label_oy() != 0.0f) {
                set_number(j, "label_oy", w.label_oy());
            }
            arr.array().push_back(std::move(j));
        }
        root.object()["widgets"] = std::move(arr);
        return root;
    }

    const UiWidget* UiDocument::find_widget(std::string_view id) const {
        for (const auto& w : m_widgets) {
            if (w.id() == id) {
                return &w;
            }
        }
        return nullptr;
    }

    UiWidget* UiDocument::find_widget(std::string_view id) {
        for (auto& w : m_widgets) {
            if (w.id() == id) {
                return &w;
            }
        }
        return nullptr;
    }

    UiNineSliceLayout ui_compute_nine_slice(float client_x,
                                            float client_y,
                                            float client_w,
                                            float client_h,
                                            float scale,
                                            float pad_l,
                                            float pad_t,
                                            float pad_r,
                                            float pad_b,
                                            float tl_nw,
                                            float tl_nh,
                                            float tc_nw,
                                            float tc_nh,
                                            float tr_nw,
                                            float tr_nh,
                                            float l_nw,
                                            float l_nh,
                                            float r_nw,
                                            float r_nh,
                                            float bl_nw,
                                            float bl_nh,
                                            float bc_nw,
                                            float bc_nh,
                                            float br_nw,
                                            float br_nh,
                                            bool tile_edges) {
        const float s = std::max(0.25f, scale);
        const float tl_w = tl_nw * s;
        const float tl_h = tl_nh * s;
        const float tr_w = tr_nw * s;
        const float tr_h = tr_nh * s;
        const float bl_w = bl_nw * s;
        const float bl_h = bl_nh * s;
        const float br_w = br_nw * s;
        const float br_h = br_nh * s;
        const float side_l_w = l_nw * s;
        const float side_r_w = r_nw * s;
        const float top_h = tc_nh * s;
        const float bot_h = bc_nh * s;
        const float ox = client_x - pad_l * s;
        const float oy = client_y - pad_t * s;
        const float ow = client_w + (pad_l + pad_r) * s;
        const float oh = client_h + (pad_t + pad_b) * s;

        UiNineSliceLayout out;
        out.set_outer(UiChromeRect(ox, oy, ow, oh));
        out.set_top_tile_w(tc_nw * s);
        out.set_bot_tile_w(bc_nw * s);
        out.set_left_tile_h(l_nh * s);
        out.set_right_tile_h(r_nh * s);

        if (!tile_edges) {
            const float mid_y = oy + top_h;
            const float mid_h = std::max(0.0f, oh - top_h - bot_h);
            out.set_tl(UiChromeRect(ox, oy, tl_w, tl_h));
            out.set_tr(UiChromeRect(ox + ow - tr_w, oy, tr_w, tr_h));
            out.set_bl(UiChromeRect(ox, oy + oh - bl_h, bl_w, bl_h));
            out.set_br(UiChromeRect(ox + ow - br_w, oy + oh - br_h, br_w, br_h));
            out.set_top(UiChromeRect(ox + tl_w, oy, std::max(0.0f, ow - tl_w - tr_w), top_h));
            out.set_bot(
                UiChromeRect(ox + bl_w, oy + oh - bot_h, std::max(0.0f, ow - bl_w - br_w), bot_h));
            out.set_left(UiChromeRect(ox, mid_y, side_l_w, mid_h));
            out.set_right(UiChromeRect(ox + ow - side_r_w, mid_y, side_r_w, mid_h));
            return out;
        }

        // Renewal Info chrome: rails hug the client (5 px overlap), corners sit on
        // the outer pad, and side rails run between the top/bottom corners of that
        // side — not between the shorter center strips.
        const float overlap = 5.0f * s;
        const float top_y = client_y - top_h + overlap;
        const float left_x = client_x - side_l_w + overlap;
        const float right_x = client_x + client_w - overlap;
        const float tr_x = client_x + client_w + pad_r * s - tr_w - overlap;
        const float bl_x = left_x + side_l_w - bl_w;
        const float bl_y = client_y + client_h + pad_b * s - bl_h;
        const float br_x = ox + ow - br_w;
        const float br_y = client_y + client_h + pad_b * s - br_h;
        const float left_y = oy + tl_h;
        const float right_y = top_y + tr_h;
        const float bot_y = client_y + client_h + pad_b * s - bot_h;

        out.set_tl(UiChromeRect(ox, oy, tl_w, tl_h));
        out.set_tr(UiChromeRect(tr_x, top_y, tr_w, tr_h));
        out.set_bl(UiChromeRect(bl_x, bl_y, bl_w, bl_h));
        out.set_br(UiChromeRect(br_x, br_y, br_w, br_h));
        const float top_start = ox + tl_w;
        float top_w = std::max(0.0f, (tr_x + tr_w) - top_start);
        // A leftover top-center stub that starts under TR shows through the
        // transparent [X] in info-menu-top right.bmp. Layouts placed
        // whole 100px tiles only (two on a 245-wide Info window).
        const float top_tile = tc_nw * s;
        if (top_tile > 1.0f) {
            const float last_start = top_start + std::floor(top_w / top_tile) * top_tile;
            if (last_start + 0.5f >= tr_x) {
                top_w = std::max(0.0f, last_start - top_start);
            }
        }
        out.set_top(UiChromeRect(top_start, top_y, top_w, top_h));
        out.set_bot(UiChromeRect(client_x, bot_y, std::max(0.0f, br_x - client_x), bot_h));
        out.set_left(UiChromeRect(left_x, left_y, side_l_w, std::max(0.0f, bl_y - left_y)));
        out.set_right(UiChromeRect(right_x, right_y, side_r_w, std::max(0.0f, br_y - right_y)));
        return out;
    }

    UiWidget
    ui_scrolled_widget(const UiWidget& source, const UiDocument& document, const UiVarMap& vars) {
        UiWidget widget = source;
        if (source.viewport().empty()) {
            return widget;
        }
        const auto* viewport = document.find_widget(source.viewport());
        if (!viewport || viewport->h() <= 0) {
            widget.set_visible(false);
            return widget;
        }
        const auto value = vars.find(source.scroll_bind());
        float offset = value == vars.end() ? 0 : std::strtof(value->second.c_str(), nullptr);
        if (!std::isfinite(offset)) {
            offset = 0;
        }
        widget.set_y(source.y() - std::max(0.0f, offset));
        const float top = viewport->y();
        const float bottom = top + viewport->h();
        if (source.type() == UiWidgetType::IconGrid && source.pitch_y() > 0) {
            const float inset = std::max(0.0f, (source.frame_h() - source.cell_h()) * .5f);
            int first = 0;
            while (first < source.rows() && widget.y() + first * source.pitch_y() - inset < top) {
                ++first;
            }
            int end = first;
            while (end < source.rows() &&
                   widget.y() + end * source.pitch_y() + source.cell_h() + inset <= bottom) {
                ++end;
            }
            widget.set_y(widget.y() + first * source.pitch_y());
            widget.set_rows(end - first);
            widget.set_first_slot(first * source.cols());
            widget.set_visible(source.visible() && end > first);
        } else {
            const float height =
                source.h() > 0
                    ? source.h()
                    : static_cast<float>(source.pixel_size() > 0 ? source.pixel_size()
                                                                 : document.font_pixel_size()) +
                          4;
            widget.set_visible(source.visible() && widget.y() >= top &&
                               widget.y() + height <= bottom);
        }
        return widget;
    }

}  // namespace openkal::client
