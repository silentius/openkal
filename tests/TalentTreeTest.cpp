/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/TalentTree.h"

#include "fixture_util.h"
#include "openkal/client/BuffSlotBar.h"
#include "openkal/client/Skills.h"
#include "openkal/client/TalentTreeWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/Combat.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sqlite3.h>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;
    void model_checks() {
        const auto all = (std::uint64_t{1} << protocol::kTalentNodeCount) - 1;
        expect(protocol::talent_nodes().size() == 37, "all 37 requested nodes exist");
        expect(protocol::talent_cost(all) == 325, "complete tree costs 325 points");
        expect(protocol::valid_talents(0, 1), "level one owns one unspent point");
        expect(!protocol::valid_talents(protocol::talent_bit(0), 9), "cannot overspend");
        expect(protocol::valid_talents(protocol::talent_bit(0), 10), "exact node cost accepted");
        expect(!protocol::valid_talents(protocol::talent_bit(1), 110), "predecessor required");
        expect(!protocol::valid_talents(std::uint64_t{1} << 60, 110), "unknown node bits rejected");
        expect(!protocol::can_apply_talents(1, 0, 110), "committed nodes cannot be refunded");
        const auto b = protocol::talent_bonuses(all);
        expect(b.physical == 80 && b.magical == 80 && b.max_physical == -15,
               "attack totals are additive, including penalties");
        expect(b.defense == 35 && b.mana == 225 && b.mana_percent == -50,
               "defense and mana penalties aggregate");
        expect(b.explosive_percent == 50 && b.hp_per_pulse == 20 && b.mp_per_pulse == 20,
               "power and regeneration add up");
        expect(b.pickup_count == 6 && b.speed_percent == 25 && b.evasion == -20 && b.beacon,
               "quality and upgrade nodes add up");
        server::combat::Combatant attacker, target;
        attacker.level = target.level = 100;
        attacker.clazz = 0;
        attacker.upgrade_level = 5;
        attacker.char_attack = 100;
        attacker.weapon_min_attack = attacker.weapon_max_attack = 100;
        for (const bool pvp : {false, true}) {
            target.is_player = pvp;
            attacker.talent_explosive_damage_percent = 0;
            const auto base = server::combat::get_palsy_extra(attacker, target, 1, 150);
            attacker.talent_explosive_damage_percent = 50;
            expect(base > 0 && server::combat::get_palsy_extra(attacker, target, 1, 150) ==
                                   base * 150 / 100,
                   "talent Explosive Blow power applies in PvE and PvP");
            expect(server::combat::get_palsy_extra(attacker, target, 100, 150) == 0,
                   "talent power does not change Explosive Blow proc chance");
        }
        for (int i = 0; i < 5; ++i) {
            expect(protocol::talent_nodes()[31 + i].cost == 5 &&
                       protocol::talent_nodes()[31 + i].bonus.speed_percent == 5 &&
                       protocol::talent_nodes()[31 + i].bonus.evasion == -(i + 2),
                   "each Speed node costs five points, adds five percent and retains its evasion "
                   "penalty");
            for (int first : {5, 15}) {
                expect(protocol::talent_nodes()[first + i].cost == 10 &&
                           protocol::talent_nodes()[first + i].bonus.explosive_percent == 5,
                       "each physical and magical Explosive Blow node costs ten points and adds "
                       "five percent");
            }
        }
        const auto ranks = protocol::talent_ranks(all);
        expect(ranks == std::array<int, 5>{10, 10, 5, 6, 5},
               "passive ranks exclude the Beacon buff");
        for (const auto command : {protocol::TalentCommand::Snapshot,
                                   protocol::TalentCommand::Apply,
                                   protocol::TalentCommand::Beacon}) {
            const auto frame = protocol::frame_talent_request(command, 1, 3);
            protocol::TalentCommand parsed;
            std::uint64_t expected = 0, desired = 0;
            expect(protocol::parse_talent_request(
                       frame.data() + 3, frame.size() - 3, parsed, expected, desired) &&
                       parsed == command,
                   "request roundtrip");
            expect(!protocol::parse_talent_request(
                       frame.data() + 3, frame.size() - 4, parsed, expected, desired),
                   "truncated request rejected");
            auto extra = frame;
            extra.push_back(0);
            expect(!protocol::parse_talent_request(
                       extra.data() + 3, extra.size() - 3, parsed, expected, desired),
                   "request trailing bytes rejected");
        }
        protocol::TalentState original;
        original.level = 110;
        original.mask = 3;
        original.beacon_cooldown_ms = 120000;
        const auto frame = protocol::frame_talent_state(original);
        protocol::TalentState decoded;
        expect(protocol::parse_talent_state(frame.data() + 3, frame.size() - 3, decoded) &&
                   decoded.mask == 3 && decoded.level == 110 &&
                   decoded.beacon_cooldown_ms == 120000,
               "authoritative state roundtrip");
        expect(!protocol::parse_talent_state(frame.data() + 3, frame.size() - 4, decoded) &&
                   decoded.mask == 3,
               "invalid state leaves previous state intact");
    }
    void world_checks(const std::string& root) {
        const auto maps = root + "/maps";
        test::mkdir_p(maps);
        expect(test::write_open_area(maps + "/n_000_000.akarea"), "write beacon map");
        server::GameWorld world;
        expect(world.load_maps_around(maps, 800, 800, 1), "load beacon map");
        for (int i = 0; i < 5; ++i) {
            server::MonsterEntity m;
            m.id = i + 1;
            m.x = 400;
            m.y = 800 + i * 16;
            m.target_x = m.x;
            m.target_y = m.y;
            m.map = i == 3 ? 1 : 0;
            m.hp = m.max_hp = 100;
            m.race = static_cast<int>(protocol::MonsterRace::Maguni);
            m.chase_speed = 80;
            m.detect_range = 80;
            m.sight_range = 120;
            m.roam_x0 = 300;
            m.roam_x1 = 1000;
            m.roam_y0 = 600;
            m.roam_y1 = 1000;
            m.idle_left = 1000;
            if (i == 1) {
                m.aggro = true;
                m.aggro_player_id = 2;
                m.hostility_by_player[2] = 50;
            }
            if (i == 2) {
                m.spawn_protection_left = 10;
            }
            if (i == 4) {
                m.x = 100;
            }
            world.monsters().push_back(m);
        }
        server::PlayerPose owner{1, 800, 800, 0};
        owner.beacon_active = true;
        server::PlayerPose other{2, 400, 820, 0};
        std::vector<server::MonsterMoveEvent> moves;
        std::vector<server::MonsterAttackEvent> attacks;
        world.tick(0.05f, {owner, other}, moves, attacks);
        expect(world.monsters()[0].aggro_player_id == 1 && world.monsters()[0].beacon_pursuit,
               "Beacon attracts idle monster outside its normal sight range");
        expect(world.monsters()[1].aggro_player_id == 2, "Beacon never steals an engaged monster");
        expect(world.monsters()[2].aggro_player_id == 0 &&
                   world.monsters()[3].aggro_player_id == 0 &&
                   world.monsters()[4].aggro_player_id == 0,
               "Beacon respects protection, map and radius");
        owner.beacon_active = false;
        world.tick(0.05f, {owner, other}, moves, attacks);
        expect(!world.monsters()[0].beacon_pursuit && world.monsters()[0].aggro_player_id == 0,
               "expiration restores the normal monster leash");
    }
    void persistence_checks(const std::string& root) {
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        const auto dir = root + "/store";
        expect(store->open(dir), "open talent persistence");
        server::CharacterRecord hero;
        hero.name = "Talent Tester";
        hero.level = 100;
        expect(store->ensure_default_player("talents", "talents", hero), "create talent character");
        const auto account = store->find_account("talents");
        const auto rows =
            account ? store->list_characters(account->id) : std::vector<server::CharacterRecord>{};
        expect(!rows.empty(), "new character available");
        if (rows.empty()) {
            return;
        }
        auto saved = rows.front();
        saved.talent_mask = 3;
        saved.beacon_ready_at = 1234567890123;
        expect(store->save_character(saved), "save talent mask and 64-bit cooldown atomically");
        store->close();
        expect(store->open(dir), "reopen talent database");
        auto loaded = store->load_character(saved.id);
        expect(loaded && loaded->talent_mask == 3 &&
                   loaded->beacon_ready_at == saved.beacon_ready_at,
               "talents and cooldown survive reconnect");
        store->close();
        sqlite3* db = nullptr;
        expect(sqlite3_open((dir + "/" + server::kSqliteFileName).c_str(), &db) == SQLITE_OK,
               "open old schema fixture");
        expect(sqlite3_exec(db,
                            "ALTER TABLE characters DROP COLUMN talent_mask; ALTER TABLE "
                            "characters DROP COLUMN beacon_ready_at",
                            nullptr,
                            nullptr,
                            nullptr) == SQLITE_OK,
               "simulate pre-talent schema with retained version marker");
        sqlite3_close(db);
        expect(store->open(dir), "idempotent talent schema migration");
        loaded = store->load_character(saved.id);
        expect(loaded && loaded->talent_mask == 0 && loaded->beacon_ready_at == 0 &&
                   loaded->level == 100,
               "existing character retains level and all level points");
        store->close();
    }
    void beacon_buff_checks() {
        std::vector<client::BuffSlotEntry> slots(1);
        slots.front().bstate = 1;
        slots.front().effect_name = "Existing buff";
        protocol::TalentState state;
        state.level = 110;
        for (int i = 31; i < 37; ++i) {
            state.mask |= protocol::talent_bit(i);
        }
        client::sync_talent_beacon_buff(slots, state, 1000);
        expect(slots.size() == 1, "unlocking Beacon does not add a permanent buff");
        state.beacon_active_ms = 30000;
        client::sync_talent_beacon_buff(slots, state, 1000);
        expect(slots.size() == 2 && slots.back().is_talent_beacon && slots.back().bstate == 0 &&
                   slots.back().source_item_index == 0 &&
                   slots.back().icon_file == "talents/beacon.bmp" &&
                   slots.back().expire_ms == 31000,
               "activation adds a timed Beacon buff to Skill Info without a legacy state bit");
        expect(client::BuffSlotBar::tooltip_text(slots.back(), 1000).find("Beacon") !=
                       std::string::npos &&
                   client::BuffSlotBar::tooltip_text(slots.back(), 1000).find("30Second") !=
                       std::string::npos,
               "Beacon buff tooltip exposes its name, effect and remaining duration");
        state.beacon_active_ms = 20000;
        client::sync_talent_beacon_buff(slots, state, 11000);
        expect(
            slots.size() == 2 && slots.back().expire_ms == 31000 &&
                slots.front().effect_name == "Existing buff",
            "refreshing Beacon neither duplicates nor extends the buff and preserves other buffs");
        expect(client::BuffSlotBar::tooltip_text(slots.back(), 31000).empty(),
               "expired Beacon is hidden automatically");
        state.beacon_active_ms = 0;
        client::sync_talent_beacon_buff(slots, state, 15000);
        expect(slots.size() == 1 && slots.front().effect_name == "Existing buff",
               "server cancellation removes only Beacon");
    }
    void ui_checks(const char* capture) {
        client::Catalog catalog;
        (void) catalog.load("");
        const auto* physical = catalog.find(0, protocol::kTalentSkillFirst);
        const auto* beacon = catalog.find(0, protocol::kBeaconSkill);
        expect(physical && physical->passive() &&
                   physical->format_ability(2).find("+15") != std::string::npos,
               "project-owned talent passives show cumulative rank bonuses without optional data");
        expect(beacon && !beacon->passive() && client::Catalog::is_castable(*beacon) &&
                   beacon->delay_ms() == protocol::kBeaconCooldownMs,
               "Beacon is an equippable self buff with a 120-second cooldown");
        for (int clazz = 0; clazz < 4; ++clazz) {
            const auto skills = catalog.for_class(clazz);
            expect(client::Catalog::cast_kind_for(clazz, protocol::kBeaconSkill) ==
                           client::Catalog::cast_kind_for(2, 7) &&
                       !client::Catalog::skill_bar_sets_active(
                           client::Catalog::cast_kind_for(clazz, protocol::kBeaconSkill)) &&
                       client::Catalog::skill_bar_casts_on_click(clazz, protocol::kBeaconSkill) ==
                           client::Catalog::skill_bar_casts_on_click(2, 7),
                   "Beacon follows Archer Chance Capture routing for every class");
            expect(skills.size() == protocol::kTalentSkillCount + 1 &&
                       std::count_if(skills.begin(),
                                     skills.end(),
                                     [](const client::SkillDef* skill) {
                                         return skill->passive() &&
                                                !client::Catalog::is_castable(*skill);
                                     }) == protocol::kTalentSkillCount,
                   "every class exposes five passive talents and the Beacon self buff");
        }
        client::PlayerSkills player;
        std::array<int, client::PlayerSkills::kHotbarSlots> old_shortcuts;
        old_shortcuts.fill(-1);
        old_shortcuts[0] = protocol::kBeaconSkill;
        player.apply_shortcut(protocol::kBeaconSkill, old_shortcuts);
        expect(player.hotbar_slot(0) == protocol::kBeaconSkill &&
                   player.active_index() == protocol::kBeaconSkill,
               "Beacon hotbar and mouse-action bindings survive shortcut synchronization");
        client::UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/talent_tree.json"),
               "load talent UI document");
        client::UiDocument doc;
        expect(doc.load(layout.root()), "parse universal talent schema");
        client::TalentTreeWindow window;
        window.set_layout(&layout);
        protocol::TalentState state;
        state.level = 110;
        state.mask = 3;
        window.receive(state);
        window.open();
        expect(!window.toggle_node(4), "UI rejects a skipped prerequisite");
        expect(window.toggle_node(2) && window.toggle_node(3), "UI stages nodes sequentially");
        expect(window.committed() == 3 && window.draft() == 15,
               "draft does not affect authoritative bonuses");
        expect(window.toggle_node(2) && window.draft() == 3, "undo removes staged descendants");
        expect(!window.toggle_node(0), "UI never refunds a committed node");
        expect(window.toggle_node(2), "restage a pending node");
        window.request_sent(true);
        expect(!window.toggle_node(3), "pending request blocks further edits");
        state.status = protocol::TalentStatus::SaveFailed;
        window.receive(state);
        expect(window.draft() == 7 && window.committed() == 3,
               "failed save preserves editable draft");
        window.close();
        window.open();
        expect(window.draft() == 3, "Exit discards pending selection");
        expect(!doc.find_widget("description") && !doc.find_widget("description_title"),
               "node details are not rendered in the footer");
        client::WidgetTools ui;
        for (const auto& size :
             {std::pair{1280, 720}, std::pair{1920, 1080}, std::pair{1024, 768}}) {
            engine::FrameInput input;
            input.width = size.first;
            input.height = size.second;
            const float s = std::min(size.first / 1600.0f, size.second / 900.0f);
            const float x = (size.first - 1600 * s) * 0.5f, y = (size.second - 900 * s) * 0.5f;
            const auto* node = doc.find_widget("node_2");
            input.mouse_x = x + (node->x() + 22) * s;
            input.mouse_y = y + (node->y() + 22) * s;
            input.mouse_left_clicked = false;
            (void) window.update(input, ui);
            const auto hovered = window.variables();
            expect(window.draft() == 3 && hovered.at("talent.overlay_visible") == "true" &&
                       hovered.at("talent.detail").find("Min Physical +15") != std::string::npos &&
                       hovered.at("talent.detail").find("Max Physical +15") != std::string::npos &&
                       hovered.at("talent.detail").find("Defense -8") != std::string::npos,
                   "hover shows every node bonus and penalty without changing the draft");
            expect(hovered.at("talent.detail_cost").find("10 talent points") != std::string::npos &&
                       hovered.at("talent.detail_requirement") == "Requires: Physical Power 2",
                   "node overlay includes the exact cost and prerequisite");
            input.mouse_left_clicked = true;
            (void) window.update(input, ui);
            expect(window.draft() == 7, "node hit matches scaled fullscreen layout");
            const auto* apply = doc.find_widget("apply");
            input.mouse_x = x + (apply->x() + 70) * s;
            input.mouse_y = y + (apply->y() + 21) * s;
            expect(window.update(input, ui) == client::TalentTreeWindow::Action::Apply,
                   "Apply action uses lower-right scaled hitbox");
            input.mouse_left_clicked = false;
            input.mouse_x = 5;
            input.mouse_y = 5;
            (void) window.update(input, ui);
            expect(window.variables().at("talent.overlay_visible") == "false",
                   "overlay closes when the pointer leaves the node");
            window.close();
            window.open();
        }
        if (!capture) {
            return;
        }
        auto renderer = std::unique_ptr<engine::IEngine>(engine::create_engine());
        if (!renderer->init("OpenKal talent tree verification", 1600, 900)) {
            expect(false, "initialize talent capture");
            return;
        }
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*renderer, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        (void) window.toggle_node(2);
        for (const auto& size :
             {std::pair{1600, 900}, std::pair{1280, 720}, std::pair{1024, 768}}) {
            for (const int node_index : {2, 19, 30, 36}) {
                const float scale = std::min(size.first / 1600.0f, size.second / 900.0f);
                const auto* node = doc.find_widget("node_" + std::to_string(node_index));
                engine::FrameInput pointer;
                pointer.width = size.first;
                pointer.height = size.second;
                pointer.mouse_x = (size.first - 1600 * scale) * .5f + (node->x() + 22) * scale;
                pointer.mouse_y = (size.second - 900 * scale) * .5f + (node->y() + 22) * scale;
                (void) window.update(pointer, ui);
                expect(renderer->begin_offscreen(size.first, size.second, 0, 0, 0),
                       "begin fullscreen tree capture");
                window.draw(
                    *renderer, ui, static_cast<float>(size.first), static_cast<float>(size.second));
                std::vector<std::uint8_t> rgba;
                expect(renderer->end_offscreen_rgba(rgba), "read fullscreen tree pixels");
                std::ofstream out(std::string(capture) + "-" + std::to_string(size.first) +
                                      "-node-" + std::to_string(node_index) + ".ppm",
                                  std::ios::binary);
                out << "P6\n" << size.first << " " << size.second << "\n255\n";
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
            }
        }
        window.destroy(*renderer);
        ui.destroy();
        renderer->shutdown();
    }
}  // namespace
int main(int argc, char** argv) {
    model_checks();
    beacon_buff_checks();
    const auto root = test::temp_dir(("talents_" + std::to_string(getpid())).c_str());
    world_checks(root);
    persistence_checks(root);
    ui_checks(argc > 1 ? argv[1] : nullptr);
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
