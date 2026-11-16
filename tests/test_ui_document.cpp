/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GeonColor.h"
#include "openkal/client/RidingEquipment.h"
#include "openkal/client/UiDocument.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "test_util.h"

#include <array>
#include <cmath>
#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main() {
    using openkal::client::ui_eval_condition;
    using openkal::client::ui_fill_dummy_preview;
    using openkal::client::ui_fill_theme_vars;
    using openkal::client::ui_interpolate;
    using openkal::client::ui_parse_hex_color;
    using openkal::client::ui_parse_variable_keys;
    using openkal::client::ui_primary_index;
    using openkal::client::UiDocument;
    using openkal::client::UiVarMap;
    using openkal::test::expect;

    UiVarMap vars;
    vars["stats.hth"] = "42";
    vars["stats.hth_label"] = "HTH";
    vars["stats.hth_pending"] = "false";
    vars["inventory.money"] = "1234567";
    vars["transform.str"] = "3";
    vars["exp.pct"] = "0.37";
    vars["stats.pu_remaining"] = "5";
    vars["player.name"] = "TestKn8";
    expect(ui_interpolate("[${player.name}] : Thank you, ${player.name}.", vars) ==
               "[TestKn8] : Thank you, TestKn8.",
           "dialogue player token resolves every occurrence before wrapping");
    vars["player.name"] = "${literal}";
    expect(ui_interpolate("[${player.name}]", vars) == "[${literal}]",
           "character names are inserted literally, without recursive interpolation");

    expect(ui_interpolate("${stats.hth_label} ${stats.hth}", vars) == "HTH 42",
           "interpolate label and value");
    expect(ui_interpolate("${missing}", vars).empty(), "missing key is empty");
    expect(ui_interpolate("${inventory.money:comma}", vars) == "1,234,567", "comma suffix");
    {
        struct Sample {
            std::int64_t value;
            const char* color;
            const char* markup;
        };
        const Sample amounts[] = {{0, "#FFFFFF", "#8"},
                                  {99999, "#FFFFFF", "#8"},
                                  {100000, "#00FF00", "#4"},
                                  {999999, "#00FF00", "#4"},
                                  {1000000, "#FFFF00", "#3"},
                                  {9999999, "#FFFF00", "#3"},
                                  {10000000, "#FF8040", "#2"},
                                  {99999999, "#FF8040", "#2"},
                                  {100000000, "#0080FF", "#6"},
                                  {999999999, "#0080FF", "#6"},
                                  {1000000000, "#AA55FF", "#9"},
                                  {2000000000000LL, "#AA55FF", "#9"}};
        std::vector<openkal::client::UiTextColorRun> colors;
        for (const auto& amount : amounts) {
            vars["geons"] = std::to_string(amount.value);
            const auto shown = ui_interpolate("Geons: ${geons:geons}", vars, &colors);
            const auto color = openkal::client::geon_color(amount.value);
            expect(colors.size() == 1 && colors[0].start == 7 &&
                       colors[0].length == shown.size() - 7 && colors[0].color == amount.color &&
                       std::string(color.hex) == amount.color &&
                       std::string(color.markup) == amount.markup,
                   "Geon UI and tooltip colors agree at each threshold and for 64-bit totals");
        }
        vars["range"] = "99,999 - 1000000000 Geons / item";
        const auto range = ui_interpolate("${range:geons}", vars, &colors);
        expect(range == "99,999 - 1,000,000,000 Geons / item" && colors.size() == 2 &&
                   colors[0].color == "#FFFFFF" && colors[1].color == "#AA55FF" &&
                   range.substr(colors[1].start, colors[1].length) == "1,000,000,000",
               "each price range endpoint keeps its own amount color and grouping");
        expect(ui_interpolate("${missing:geons}", vars, &colors).empty() && colors.empty(),
               "empty Geon labels clear old color spans");
        vars["geons"] = "99999999999999999999999999";
        expect(ui_interpolate("${geons:geons}", vars, &colors) == vars["geons"] && colors.empty(),
               "out-of-range currency text is preserved without overflow");
    }
    expect(ui_interpolate("${transform.str:+}", vars) == "+3", "plus suffix");
    expect(ui_interpolate("${exp.pct:pct}", vars) == "37%", "pct suffix");
    expect(ui_eval_condition("", vars), "empty condition is true");
    expect(ui_eval_condition("stats.hth_pending", vars) == false, "false pending");
    vars["stats.hth_pending"] = "true";
    expect(ui_eval_condition("stats.hth_pending", vars), "true pending");
    expect(ui_eval_condition("stats.pu_remaining > 0", vars), "comparison true");
    expect(ui_eval_condition("stats.pu_remaining > 9", vars) == false, "comparison false");
    expect(ui_primary_index("stats.hth") == 1, "primary hth index");
    expect(ui_primary_index("stats.intl") == 2, "primary intl index");

    float r = 0, g = 0, b = 0;
    expect(ui_parse_hex_color("#E8DCC0", r, g, b) && r > 0.8f && g > 0.8f,
           "parse theme label color");

    {
        openkal::platform::Json root;
        std::string err;
        expect(
            openkal::platform::Json::parse(
                R"({"schema":"openkal.ui/v1","id":"close_disabled","kind":"window","chrome":{"close":false}})",
                root,
                &err),
            "parse close-disabled chrome");
        UiDocument doc;
        expect(doc.load(root) && !doc.chrome_close_button(), "chrome close false");
        expect(!doc.close_override(), "default close box remains unmodified");
    }

    const std::string vars_md = "| `stats.hth` | int | `42` | Character Info |\n"
                                "| `player.name` | string | `Preview` | Character Info |\n";
    const auto keys = ui_parse_variable_keys(vars_md);
    expect(keys.size() == 2 && keys[0] == "stats.hth" && keys[1] == "player.name",
           "parse VARIABLES.md keys");

    const std::string repo = OPENKAL_REPO_ROOT;
    const std::string info_path =
        openkal::platform::join_path(repo, "config/ui/character_info.json");
    const auto info_bytes = openkal::platform::read_file(info_path);
    expect(info_bytes.has_value(), "read character_info.json");
    if (info_bytes) {
        const std::string text(info_bytes->begin(), info_bytes->end());
        openkal::platform::Json root;
        std::string err;
        expect(openkal::platform::Json::parse(text, root, &err), "parse character_info.json");
        UiDocument doc;
        expect(doc.load(root) && doc.is_v1(), "load character_info as v1");
        expect(doc.id() == "character_info", "character_info id");
        expect(doc.chrome_preset() == "dragon_dialog", "dragon_dialog preset");
        expect(!doc.chrome_close_button(), "no upper-right close control");
        expect(doc.chrome_edges() == openkal::client::UiChromeEdges::Repeat,
               "dragon rails retain native texture density");
        expect(doc.find_widget("dialog_icon") &&
                   doc.find_widget("dialog_icon")->file() == "dragon_menu/character.bmp",
               "character dialog shares its menu icon");
        expect(!doc.find_widget("apply_tabs"), "no decorative active-effect tabs");
        expect(doc.find_widget("title") != nullptr, "title widget");
        expect(doc.find_widget("hth") != nullptr, "hth widget");
        expect(doc.find_widget("ok") != nullptr, "ok widget");
        const auto* hth = doc.find_widget("hth");
        expect(hth && hth->pending_when() == "stats.hth_pending", "pending_when on hth");

        const std::string round = openkal::platform::Json::stringify(doc.to_json(), 2);
        openkal::platform::Json again;
        expect(openkal::platform::Json::parse(round, again, &err), "round-trip parse");
        UiDocument doc2;
        expect(doc2.load(again) && doc2.widgets().size() == doc.widgets().size(),
               "round-trip widget count");
        expect(doc2.chrome_edges() == openkal::client::UiChromeEdges::Repeat,
               "round-trip repeating rails");
        expect(doc2.find_widget("ok") && doc2.find_widget("ok")->action() == "stats.apply",
               "round-trip ok action");
        const auto* close = doc2.find_widget("exit_bottom");
        expect(close && close->x_from_right() == 16 && close->y_from_bottom() == 0,
               "footer anchors survive round trip");
        expect(close && close->file_over() == "dragon_dialog/close_hover.bmp" &&
                   close->file_on() == "dragon_dialog/close_pressed.bmp",
               "button states survive round trip");
        UiDocument preset;
        ui_apply_chrome_preset(preset, "dragon_dialog");
        expect(preset.pad_left() == 40 && preset.pad_top() == 84 && preset.pad_right() == 40 &&
                   preset.pad_bottom() == 56 && !preset.chrome_close_button(),
               "shared dragon preset defaults");
    }

    auto load_v1 = [&](const char* name, UiDocument& doc) -> bool {
        const std::string path =
            openkal::platform::join_path(repo, std::string("config/ui/") + name + ".json");
        const auto bytes = openkal::platform::read_file(path);
        expect(bytes.has_value(), name);
        if (!bytes) {
            return false;
        }
        const std::string text(bytes->begin(), bytes->end());
        openkal::platform::Json root;
        std::string err;
        expect(openkal::platform::Json::parse(text, root, &err), name);
        expect(doc.load(root) && doc.is_v1(), name);
        expect(doc.id() == name, name);
        return doc.is_v1();
    };

    for (const auto* name : {"play_mode", "multiplayer_login"}) {
        UiDocument doc;
        if (!load_v1(name, doc)) {
            continue;
        }
        UiDocument roundtrip;
        expect(roundtrip.load(doc.to_json()), "multiplayer layouts survive editor roundtrip");
        if (std::string(name) == "play_mode") {
            const auto* multiplayer = roundtrip.find_widget("multi");
            expect(multiplayer && multiplayer->action() == "mode.multi",
                   "mode menu offers multiplayer");
        } else {
            expect(!roundtrip.find_widget("host") && !roundtrip.find_widget("port"),
                   "the client cannot edit the packaged server address or port");
            const auto* registration = roundtrip.find_widget("register");
            UiVarMap disabled{{"online.registration_allowed", "false"}};
            UiVarMap enabled{{"online.registration_allowed", "true"}};
            expect(registration && !ui_eval_condition(registration->visible_when(), disabled) &&
                       ui_eval_condition(registration->visible_when(), enabled),
                   "registration visibility follows server capability");
            expect(roundtrip.find_widget("email") && roundtrip.find_widget("confirmation"),
                   "registration requires email and confirmation inputs");
            const auto* toggle = roundtrip.find_widget("toggle_passwords");
            expect(toggle && toggle->action() == "online.toggle_passwords",
                   "account form offers password visibility toggle");
            expect(roundtrip.find_widget("password_feedback") &&
                       roundtrip.find_widget("confirmation_feedback"),
                   "account form includes live password feedback");
            UiVarMap preview;
            ui_fill_dummy_preview(preview);
            ui_fill_theme_vars(roundtrip, preview);
            for (const auto& widget : roundtrip.widgets()) {
                expect(ui_interpolate(widget.text(), preview).find("${") == std::string::npos,
                       "account editor preview resolves text tokens");
            }
        }
    }

    for (const auto* name : {"startup", "character_select", "character_navigation"}) {
        UiDocument doc;
        if (load_v1(name, doc)) {
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "boot layouts survive editor roundtrip");
            UiVarMap preview;
            ui_fill_dummy_preview(preview);
            ui_fill_theme_vars(roundtrip, preview);
            for (const auto& widget : roundtrip.widgets()) {
                if (widget.type() == openkal::client::UiWidgetType::Text) {
                    expect(!ui_interpolate(widget.text(), preview).empty(),
                           "boot text has populated editor bindings");
                }
            }
            if (std::string(name) == "character_select") {
                expect(!roundtrip.find_widget("select"), "stats panel has no entry button");
                continue;
            }
            const bool startup = std::string(name) == "startup";
            const auto* enter = roundtrip.find_widget(startup ? "single_player" : "select");
            expect(enter &&
                       enter->action() == (startup ? "startup.single_player" : "selection.select"),
                   "boot entry action survives editor roundtrip");
        }
    }

    {
        UiDocument doc;
        if (load_v1("inventory", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "inventory preset");
            const auto* tabs = doc.find_widget("bag_tabs");
            expect(tabs && tabs->tabs().size() == 2 && tabs->tabs()[0].id() == "normal",
                   "inventory document exposes General and Other beside the equipment columns");
            expect(doc.find_widget("bag.0") && doc.find_widget("bag.5") &&
                       doc.find_widget("quest") && doc.find_widget("task"),
                   "inventory document contains every permanent and expanded storage section");
            for (const char* category :
                 {"consumables", "weapons", "armors", "enchantment", "skins_pets", "other"}) {
                const auto* button = doc.find_widget(std::string("filter.") + category);
                const auto* icon = doc.find_widget(std::string("filter_icon.") + category);
                expect(button && icon &&
                           button->action() == std::string("inventory.filter.") + category &&
                           button->visible_when() == "inventory.normal_active" &&
                           icon->y() < doc.find_widget("general_viewport")->y(),
                       "each General filter has an independent icon action above its slots");
            }
            const auto* first_block = doc.find_widget("bag.0");
            for (int block = 0; block < 6; ++block) {
                const auto* grid = doc.find_widget("bag." + std::to_string(block));
                expect(!doc.find_widget("page_label." + std::to_string(block)) && first_block &&
                           grid &&
                           grid->y() == first_block->y() +
                                            block * first_block->rows() * first_block->pitch_y(),
                       "all General blocks omit headings and share normal slot row spacing");
            }
            const auto* task = doc.find_widget("task");
            const auto* quest = doc.find_widget("quest");
            expect(task && quest && doc.find_widget("task_label") &&
                       doc.find_widget("quest_label") &&
                       task->y() > quest->y() + quest->rows() * quest->pitch_y() && !task->drag(),
                   "Other retains Quest and Task headings with the Task section below Quest");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()) && roundtrip.find_widget("frame_handle") &&
                       roundtrip.find_widget("expand") &&
                       roundtrip.find_widget("expand")->action() == "inventory.expand" &&
                       roundtrip.find_widget("bag.5")->visible_when() == "inventory.normal_active",
                   "editor roundtrip preserves Collection handle, expansion action and tab "
                   "visibility");
            expect(roundtrip.find_widget("bag.5")->viewport() == "general_viewport" &&
                       roundtrip.find_widget("bag.5")->scroll_bind() == "inventory.scroll",
                   "viewport and scroll binding survive editor roundtrip");
            UiVarMap scrolling{{"inventory.scroll", "46"}};
            const auto clipped =
                openkal::client::ui_scrolled_widget(*doc.find_widget("bag.0"), doc, scrolling);
            expect(
                clipped.visible() && clipped.first_slot() == 6 && clipped.rows() == 5,
                "scroll clipping removes the first complete row and preserves its source offset");
            const auto hidden =
                openkal::client::ui_scrolled_widget(*doc.find_widget("bag.5"), doc, scrolling);
            expect(!hidden.visible(),
                   "offscreen grid is hidden without painting outside the viewport");
            scrolling["inventory.scroll"] = "1380";
            const auto last =
                openkal::client::ui_scrolled_widget(*doc.find_widget("bag.5"), doc, scrolling);
            expect(last.visible() && last.first_slot() == 0 && last.rows() == 6,
                   "last 36-cell block becomes fully visible at the bottom");
            expect(doc.find_widget("wear.weapon") != nullptr, "inventory wear slot");
            expect(doc.find_widget("wear.face") && doc.find_widget("wear.ride"),
                   "inventory exposes mask and registered mount equipment cells");
        }
    }
    {
        UiDocument doc;
        if (load_v1("skill_window", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "skill Dragon preset");
            expect(doc.find_widget("active_grid") != nullptr, "skill active grid");
            expect(doc.find_widget("tab_common") != nullptr, "skill page buttons");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "skill editor roundtrip");
            UiVarMap preview;
            ui_fill_dummy_preview(preview);
            for (const char* id :
                 {"active_grid", "passive_grid", "active_label", "passive_label"}) {
                const auto* widget = roundtrip.find_widget(id);
                expect(widget && ui_eval_condition(widget->visible_when(), preview),
                       "Common preview shows class grids and captions");
                preview["skill.class_page"] = "false";
                expect(widget && !ui_eval_condition(widget->visible_when(), preview),
                       "Action page hides class grids and captions");
                preview["skill.class_page"] = "true";
            }
            preview["skill.action_page"] = "true";
            const auto* actions = roundtrip.find_widget("action_grid");
            expect(actions && ui_eval_condition(actions->visible_when(), preview),
                   "Action preview shows its continuous grid");
        }
    }
    {
        UiDocument doc;
        if (load_v1("character_delete", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "character delete preset");
            expect(doc.find_widget("deleted_characters") != nullptr,
                   "character delete recovery list");
            expect(doc.find_widget("restore") != nullptr, "character delete restore action");
        }
    }
    {
        UiDocument doc;
        if (load_v1("mixing_item_list", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "material list uses Dragon chrome");
            expect(doc.find_widget("row_8_stones") != nullptr,
                   "material list has nine standard rows");
        }
        if (load_v1("mix_master_window", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "mix Dragon preset");
            expect(doc.find_widget("mix.center") != nullptr, "mix center slot");
            expect(doc.find_widget("mix_start") != nullptr, "mix start button");
        }
    }
    {
        UiDocument doc;
        if (load_v1("shop", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "shop preset");
            expect(doc.chrome_kind() == openkal::client::UiChromeKind::NineSlice,
                   "shop nine-slice chrome");
            expect(doc.find_widget("inventory_panel") != nullptr, "shop inventory panel");
            expect(doc.find_widget("offers_panel") != nullptr, "shop merchant panel");
            expect(doc.find_widget("inventory") != nullptr, "shop player grid");
            expect(doc.find_widget("offers") != nullptr, "shop offer grid");
            expect(doc.find_widget("quantity_confirm") != nullptr, "shop quantity modal");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "shop texture round-trip load");
            const auto* grid = roundtrip.find_widget("offers");
            expect(grid && grid->frame_file() == "dragon_hud/slot_normal.bmp",
                   "editor save preserves merchant slot frames");
            const auto* close = roundtrip.find_widget("close");
            expect(close && close->action() == "window.close", "shop footer closes the window");
        }
    }
    {
        UiDocument doc;
        if (load_v1("storage", doc)) {
            expect(doc.chrome_preset() == "info_menu", "storage preset");
            expect(doc.find_widget("inventory") != nullptr, "storage player grid");
            expect(doc.find_widget("storage_items") != nullptr, "storage account grid");
            expect(doc.find_widget("inventory_registers") != nullptr,
                   "storage inventory registers");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "storage texture round-trip load");
            const auto* texture = roundtrip.find_widget("background");
            expect(texture && texture->repeat() && texture->source().x == 5 &&
                       texture->source().y == 378 && texture->source().w == 232 &&
                       texture->source().h == 36,
                   "editor save preserves texture crop and native tiling");
            const auto* inventory_header = doc.find_widget("inventory_header_tab");
            const auto* storage_header = doc.find_widget("storage_header_tab");
            expect(inventory_header && inventory_header->bind() == "storage.inventory_tab" &&
                       inventory_header->tabs().size() == 1,
                   "storage inventory header tab");
            expect(storage_header && storage_header->bind() == "storage.storage_tab" &&
                       storage_header->tabs().size() == 1,
                   "storage account header tab");
            const auto* registers = doc.find_widget("storage_registers");
            expect(registers && registers->tabs().size() == 6, "storage exposes all six registers");
            expect(registers && registers->tabs()[0].x() == 303.0f &&
                       registers->tabs()[5].x() == 483.0f,
                   "storage registers use right-pane coordinates");
            expect(doc.find_widget("deposit") != nullptr, "storage deposit action");
            expect(doc.find_widget("withdraw") != nullptr, "storage withdraw action");
            expect(doc.find_widget("inventory_hint_background") != nullptr,
                   "storage hides inventory destroy affordance");
            expect(doc.find_widget("storage_hint_background") != nullptr,
                   "storage hides account-storage destroy affordance");
            const auto* quantity = doc.find_widget("quantity");
            expect(quantity && quantity->bind() == "storage.quantity",
                   "storage quantity input binding");
            expect(doc.find_widget("quantity_all") != nullptr, "storage quantity all action");
            expect(doc.find_widget("quantity_confirm") != nullptr,
                   "storage quantity confirm action");
        }
    }
    {
        UiDocument doc;
        if (load_v1("speed_medicine", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "speed medicine preset");
            expect(doc.window_w() == 600.0f, "speed medicine width fits recipe labels");
            expect(doc.window_h() == 440.0f, "speed medicine height fits recipes and requirements");
            const auto* title = doc.find_widget("title");
            expect(title && title->y() == -41.0f, "speed medicine title sits above the frame");
            const auto* common_recipe = doc.find_widget("from_blood_fish_increase");
            expect(common_recipe &&
                       common_recipe->action() == "speed_medicine.from_blood_fish.increase",
                   "speed medicine Sea Bream with Blood conversion controls");
            expect(doc.find_widget("direct_increase") == nullptr,
                   "speed medicine hides redundant direct recipe");
            const auto* make = doc.find_widget("make");
            expect(make && make->action() == "speed_medicine.make", "speed medicine make action");
        }
    }
    {
        UiDocument doc;
        if (load_v1("settings", doc)) {
            const auto* tabs = doc.find_widget("settings_tabs");
            const auto* battle_tab = doc.find_widget("battle_tab");
            const auto* disabled_tab = doc.find_widget("battle_tab_disabled");
            expect(tabs && tabs->bind() == "settings.tab" && tabs->tabs().size() == 4 &&
                       battle_tab && battle_tab->bind() == "settings.tab" &&
                       battle_tab->tabs().size() == 1,
                   "settings uses four global tabs and one character-specific tab");
            expect(battle_tab && battle_tab->visible_when() == "settings.battle_available" &&
                       disabled_tab && disabled_tab->action().empty() &&
                       disabled_tab->visible_when() == "settings.battle_unavailable",
                   "unavailable Battle tab has an inert replacement");
            for (const auto& widget : doc.widgets()) {
                if (widget.action() == "settings.monsters.next") {
                    expect(widget.visible_when() == "settings.page.render" &&
                               widget.y() + widget.h() < doc.window_h(),
                           "monster render row fits its Graphics page");
                }
            }
            bool found_monster_limit_action = false;
            for (const auto& widget : doc.widgets()) {
                if (widget.action() == "settings.monsters.next") {
                    found_monster_limit_action = true;
                    break;
                }
            }
            expect(found_monster_limit_action, "settings monster render limit action");
            for (const auto* action : {"settings.players.next", "settings.player_names.next"}) {
                const auto found =
                    std::find_if(doc.widgets().begin(),
                                 doc.widgets().end(),
                                 [&](const auto& widget) { return widget.action() == action; });
                expect(found != doc.widgets().end() &&
                           found->visible_when() == "settings.page.render" &&
                           found->y() + found->h() < doc.window_h(),
                       "player visibility controls fit the Graphics page");
            }
        }
    }
    {
        UiDocument doc;
        if (load_v1("transform_window", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "transform preset");
            expect(doc.find_widget("egg") != nullptr, "transform egg slot");
            expect(doc.find_widget("stages") != nullptr, "transform stages grid");
        }
    }
    {
        UiDocument doc;
        if (load_v1("status", doc)) {
            expect(doc.kind() == openkal::client::UiKind::Hud, "status kind hud");
            expect(doc.find_widget("class_icon") != nullptr, "status class icon");
            expect(doc.find_widget("level_orb") != nullptr, "status level orb");
        }
    }
    {
        UiDocument doc;
        if (load_v1("chat", doc)) {
            expect(doc.kind() == openkal::client::UiKind::Hud, "chat kind hud");
            expect(doc.find_widget("lines") != nullptr, "chat lines");
            expect(doc.find_widget("input") != nullptr, "chat input");
        }
    }
    {
        UiDocument doc;
        if (load_v1("minimap", doc)) {
            expect(doc.kind() == openkal::client::UiKind::Hud, "minimap kind hud");
            expect(doc.find_widget("map") != nullptr, "minimap canvas");
            expect(doc.find_widget("zoom_plus") != nullptr, "minimap zoom plus");
            expect(doc.draggable(), "round minimap remains movable");
            expect(doc.find_widget("map")->shape() == "circle", "minimap clips to a circle");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "round minimap schema roundtrip");
            expect(roundtrip.find_widget("map") &&
                       roundtrip.find_widget("map")->shape() == "circle",
                   "editor save preserves circular clipping");
        }
    }
    {
        UiDocument doc;
        if (load_v1("menu_bar", doc)) {
            expect(doc.kind() == openkal::client::UiKind::Hud, "menu_bar kind hud");
            expect(doc.draggable(), "menu_bar draggable");
            expect(doc.find_widget("menu_inventory") != nullptr, "menu inventory button");
            const auto* riding = doc.find_widget("menu_riding");
            expect(riding && riding->action() == "hud.menu.riding", "menu riding button");
            const auto* system = doc.find_widget("menu_system");
            expect(system && system->x() + system->w() < doc.window_w(),
                   "menu system button leaves the right tip for dragging");
            UiDocument roundtrip;
            expect(roundtrip.load(doc.to_json()), "menu editor schema roundtrip");
            const auto* inventory = roundtrip.find_widget("menu_inventory");
            expect(inventory && !inventory->file_default().empty() &&
                       !inventory->file_over().empty() && !inventory->file_on().empty() &&
                       inventory->text() == "${hud.menu.inventory.tooltip}",
                   "editor save preserves separate button states and tooltip token");
        }
    }
    {
        UiDocument doc;
        if (load_v1("dragon_hud", doc)) {
            expect(doc.kind() == openkal::client::UiKind::Hud && !doc.draggable(),
                   "anchored dragon HUD");
            expect(doc.window_w() == 792 && doc.window_h() == 180, "native HUD geometry");
            const auto* base = doc.find_widget("hotbar_base");
            const auto* shift = doc.find_widget("hotbar_shift");
            expect(base && shift && base->cols() == 10 && shift->cols() == 10 &&
                       base->y() > shift->y(),
                   "twenty slots with Shift row above base");
            const auto* auto_hp = doc.find_widget("auto_hp");
            expect(auto_hp && auto_hp->bind() == "skillbar.auto_hp" && auto_hp->drag() &&
                       auto_hp->accept() == std::vector<std::string>{"item.auto_hp"},
                   "auto HP accepts only automatic medicine");
            UiDocument round;
            expect(round.load(doc.to_json()), "HUD roundtrip");
            for (const int percent : {25, 50, 75, 100}) {
                const auto* marker =
                    round.find_widget("auto_hp_threshold_" + std::to_string(percent));
                expect(marker &&
                           marker->visible_when() == "skillbar.auto_hp_" + std::to_string(percent),
                       "editor preserves the four auto HP threshold markers");
            }
            for (const char* id : {"hp_fill", "mp_fill", "rage_fill", "exp_fill"}) {
                const auto* w = round.find_widget(id);
                expect(w && w->type() == openkal::client::UiWidgetType::Progress &&
                           w->fill_mode() == "crop" && !w->bind().empty(),
                       "live resource cropping survives editor save");
            }
            expect(round.find_widget("hp_fill")->direction() == "bottom_to_top" &&
                       round.find_widget("exp_fill")->direction() == "left_to_right",
                   "fill directions survive roundtrip");
        }
    }
    {
        UiDocument doc;
        if (load_v1("blacksmith", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "blacksmith Dragon preset");
            expect(doc.find_widget("close") && doc.find_widget("close")->action() == "window.close",
                   "blacksmith anchored footer close");
            expect(doc.find_widget("weapon") != nullptr, "blacksmith weapon");
            expect(doc.find_widget("revise") &&
                       doc.find_widget("revise")->action() == "blacksmith.revise",
                   "blacksmith revise action");
            expect(doc.find_widget("explosive_blow") &&
                       doc.find_widget("explosive_blow")->action() == "blacksmith.explosive_blow",
                   "blacksmith explosive blow action");
        }
    }

    {
        UiDocument doc;
        if (load_v1("character_create", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "creation uses Dragon window chrome");
            expect(doc.find_widget("name") && doc.find_widget("name")->bind() == "creation.name",
                   "creation name input is bound");
            expect(doc.find_widget("face_next") && doc.find_widget("hair_next"),
                   "appearance selectors exist");
            expect(doc.find_widget("create") &&
                       doc.find_widget("create")->action() == "creation.submit",
                   "creation submit action is bound");
        }
    }

    {
        UiDocument doc;
        if (load_v1("game_menu", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "game menu uses Dragon window chrome");
            expect(!doc.chrome_close_button(), "game menu has no bypass close control");
            expect(doc.find_widget("options") &&
                       doc.find_widget("options")->action() == "game_menu.options",
                   "game menu options action is bound");
            expect(doc.find_widget("select_character") &&
                       doc.find_widget("select_character")->action() ==
                           "game_menu.select_character",
                   "game menu character selection action is bound");
            expect(doc.find_widget("close_yes") &&
                       doc.find_widget("close_yes")->action() == "game_menu.close_confirm",
                   "game menu close requires confirmation");
        }
    }

    {
        UiDocument doc;
        if (load_v1("settings", doc)) {
            expect(doc.chrome_preset() == "dragon_dialog", "settings uses Dragon window chrome");
            expect(doc.find_widget("title") != nullptr, "settings title exists");
            bool has_save = false;
            bool has_effect_distance = false;
            for (const auto& widget : doc.widgets()) {
                has_save = has_save || widget.action() == "settings.save";
                has_effect_distance =
                    has_effect_distance || widget.action() == "settings.effect_distance.increase";
            }
            expect(has_save, "settings save action is bound");
            expect(has_effect_distance, "settings effect distance action is bound");
        }
    }

    const std::string tmpl_path = openkal::platform::join_path(repo, "config/ui/_template.json");
    const auto tmpl_bytes = openkal::platform::read_file(tmpl_path);
    expect(tmpl_bytes.has_value(), "read _template.json");
    if (tmpl_bytes) {
        const std::string text(tmpl_bytes->begin(), tmpl_bytes->end());
        openkal::platform::Json root;
        std::string err;
        expect(openkal::platform::Json::parse(text, root, &err), "parse _template.json");
        UiDocument doc;
        expect(doc.load(root) && doc.is_v1(), "load _template as v1");
        UiVarMap dummy;
        ui_fill_theme_vars(doc, dummy);
        ui_fill_dummy_preview(dummy);
        expect(ui_interpolate("${player.name}", dummy) == "Preview", "dummy player name");
        expect(ui_interpolate("${stats.hth}", dummy) == "42", "dummy hth");
    }

    {
        using openkal::client::ui_compute_nine_slice;
        const auto lay = ui_compute_nine_slice(0,
                                               0,
                                               245,
                                               417,
                                               1.0f,
                                               26.5f,
                                               45.1f,
                                               19.1f,
                                               36.1f,
                                               74,
                                               84,
                                               100,
                                               47,
                                               38,
                                               53,
                                               18,
                                               30,
                                               18,
                                               30,
                                               24,
                                               77,
                                               64,
                                               42,
                                               97,
                                               77,
                                               true);
        auto near = [](float a, float b) { return std::fabs(a - b) < 1.25f; };
        expect(near(lay.tl().x(), -26.5f) && near(lay.tl().y(), -45.1f), "tile TL matches pad");
        expect(near(lay.left().x(), -13.0f) && near(lay.left().y(), 38.9f),
               "tile left rail hugs client");
        expect(near(lay.right().x(), 240.0f), "tile right rail hugs client");
        expect(near(lay.tr().x(), 221.1f) && near(lay.tr().y(), -42.0f), "tile TR on top bar");
        expect(near(lay.bl().x(), -19.0f) && near(lay.bl().y(), 376.1f), "tile BL under left rail");
        expect(near(lay.br().x(), 167.1f) && near(lay.br().y(), 376.1f), "tile BR outer corner");
        expect(near(lay.bot().y(), 411.1f), "tile bottom center on outer bottom");
        expect(near(lay.top().x(), 47.5f) && near(lay.top().y(), -42.0f), "tile top bar after TL");
        expect(near(lay.top().w(), 200.0f), "tile top bar two full tiles, not under close");
    }

    {
        UiDocument riding;
        const auto bytes =
            openkal::platform::read_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/riding.json");
        expect(bytes.has_value(), "riding layout exists");
        openkal::platform::Json root;
        std::string error;
        expect(
            openkal::platform::Json::parse(std::string(bytes->begin(), bytes->end()), root, &error),
            "riding layout parses");
        expect(riding.load(root), "riding document loads");
        const auto* duplicate = riding.find_widget("stablemaster_duplicate_mount");
        expect(duplicate && duplicate->bind() == "stablemaster.duplicate_mount" &&
                   duplicate->visible_when() == "stablemaster.open" &&
                   duplicate->frame_file() == "dragon_hud/slot_normal.bmp" &&
                   duplicate->x() + duplicate->w() <= riding.window_w() &&
                   duplicate->y() + duplicate->h() <= riding.window_h(),
               "Stablemaster duplicate drop slot is visible and inside the Riding window");
        const auto* writ = riding.find_widget("stablemaster_writ");
        expect(writ && duplicate && writ->action() == "stablemaster.writ" &&
                   writ->x() + writ->w() < duplicate->x(),
               "Time Writ button and duplicate slot have separate click regions");
        const auto* convert = riding.find_widget("stablemaster_duplicate");
        expect(duplicate && convert && convert->action() == "stablemaster.duplicate" &&
                   duplicate->x() + duplicate->w() < convert->x(),
               "duplicate slot and Convert button have separate click regions");
        UiVarMap riding_preview;
        ui_fill_dummy_preview(riding_preview);
        expect(ui_eval_condition(duplicate ? duplicate->visible_when() : std::string{},
                                 riding_preview),
               "UI editor preview exposes the Stablemaster service section");
        openkal::protocol::RidingState state;
        state.iid = 100;
        state.item_index = 30100;
        state.kind = openkal::protocol::RidingKind::Battle;
        const std::array<std::string_view, 5> ids{"helmet", "armor", "saddle", "cloth", "magnet"};
        for (int grade = 0; grade < 4; ++grade) {
            state.grade = static_cast<openkal::protocol::BattleRidingGrade>(grade);
            for (int item = 30200; item <= 30223; ++item) {
                const int type = (item - 30200) % 6;
                const int expected_slot = type <= 1 ? 0 : type - 1;
                for (int slot = 0; slot < 5; ++slot) {
                    const auto* widget =
                        riding.find_widget(std::string(ids[static_cast<std::size_t>(slot)]));
                    expect(widget != nullptr, "riding equipment slot exists");
                    expect(openkal::client::riding_equipment_accepts(*widget, state, item) ==
                               (grade == (item - 30200) / 6 && slot == expected_slot),
                           "drop respects target slot and mount grade, including both helmets");
                }
            }
        }
        std::array<std::uint32_t, 5> equipment_iids{};
        expect(!openkal::client::riding_equipment_contains_iid(equipment_iids, 0),
               "empty riding slots never mark IID zero as equipped");
        equipment_iids[0] = 701;
        expect(openkal::client::riding_equipment_contains_iid(equipment_iids, 701),
               "slotted helmet gets the inventory overlay");
        expect(!openkal::client::riding_equipment_contains_iid(equipment_iids, 702),
               "another instance of the same helmet remains unmarked");
        equipment_iids[0] = 702;
        expect(!openkal::client::riding_equipment_contains_iid(equipment_iids, 701) &&
                   openkal::client::riding_equipment_contains_iid(equipment_iids, 702),
               "replacing the helmet moves its inventory overlay");
        equipment_iids.fill(0);
        expect(!openkal::client::riding_equipment_contains_iid(equipment_iids, 702),
               "unequipping or unregistering clears the inventory overlay");
        state.grade = openkal::protocol::BattleRidingGrade::Normal;
        auto helmet = *riding.find_widget("helmet");
        expect(!openkal::client::riding_equipment_accepts(helmet, state, 30320),
               "feed is not equipment");
        state.kind = openkal::protocol::RidingKind::Common;
        expect(!openkal::client::riding_equipment_accepts(helmet, state, 30200),
               "common mounts reject armor");
        state.kind = openkal::protocol::RidingKind::Battle;
        state.iid = 0;
        expect(!openkal::client::riding_equipment_accepts(helmet, state, 30200),
               "unregistered mounts reject armor");
        state.iid = 100;
        helmet.set_accept({});
        expect(!openkal::client::riding_equipment_accepts(helmet, state, 30200),
               "layout accept tags control drops");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
