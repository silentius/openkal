/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ItemDb.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "openkal/server/world/QuestDb.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace openkal::client {
    namespace {

        std::string to_lower(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        std::string resolve_icon_bmp(const std::string& hypertext_dir, const std::string& stem) {
            if (stem.empty()) {
                return {};
            }
            static std::unordered_map<std::string, std::string> cache;
            const auto cached = cache.find(stem);
            if (cached != cache.end()) {
                return cached->second;
            }

            auto remember = [&](std::string name) -> std::string {
                cache.emplace(stem, name);
                return name;
            };

            const std::string exact = platform::join_path(hypertext_dir, stem + ".bmp");
            if (platform::file_exists(exact)) {
                return remember(stem + ".bmp");
            }
            const std::string exact_u = platform::join_path(hypertext_dir, stem + ".BMP");
            if (platform::file_exists(exact_u)) {
                return remember(stem + ".BMP");
            }
            const std::string lower = to_lower(stem);
            std::string titled = lower;
            if (!titled.empty()) {
                titled[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(titled[0])));
            }
            for (const std::string& v : {stem, lower, titled}) {
                for (const char* ext : {".bmp", ".BMP"}) {
                    const std::string name = v + ext;
                    if (platform::file_exists(platform::join_path(hypertext_dir, name))) {
                        return remember(name);
                    }
                }
            }
            return remember(stem + ".bmp");
        }

        int class_limit_from_name(const std::string& lc) {
            if (lc == "knight") {
                return 0;
            }
            if (lc == "mage" || lc == "magician") {
                return 1;
            }
            if (lc == "archer") {
                return 2;
            }
            if (lc == "thief") {
                return 3;
            }
            return -1;
        }

        [[nodiscard]] int cocoon_letter_divisor(std::string_view letter) {
            if (letter.empty()) {
                return 0;
            }
            switch (letter[0]) {
            case 'A':
            case 'a':
                return 2;
            case 'B':
            case 'b':
                return 3;
            case 'C':
            case 'c':
                return 4;
            case 'D':
            case 'd':
                return 5;
            case 'E':
            case 'e':
                return 6;
            default:
                return 0;
            }
        }

        [[nodiscard]] int read_int_or_first(const platform::Json& node) {
            if (node.is_array() && !node.array().empty()) {
                return node.array()[0].as_int(0);
            }
            return node.as_int(0);
        }

        void read_minmax(const platform::Json& node, int& min_v, int& max_v) {
            if (node.is_object()) {
                min_v = node.at("min").as_int(0);
                max_v = node.at("max").as_int(0);
            } else if (node.is_array() && node.array().size() >= 2) {
                min_v = node.array()[0].as_int(0);
                max_v = node.array()[1].as_int(0);
            } else if (node.is_number()) {
                min_v = node.as_int(0);
                max_v = min_v;
            }
        }

        ItemDef item_from_json(const platform::Json& root) {
            ItemDef def;
            def.set_index(root.at("index").as_int(0));
            def.set_name(root.at("name").as_string());
            def.set_image(root.at("image").as_string());
            def.set_wearable(root.at("wear").as_int(0) != 0);
            def.set_assassin_mask(root.at("assassin_mask").boolean());
            def.set_skin_costume_mesh(root.at("skin").at("costume_mesh").boolean());
            def.set_skin_keep_hair(root.at("skin").at("keep_hair").boolean());
            def.set_skin_replace_face(root.at("skin").at("replace_face").boolean());
            def.set_skin_weapon_type(root.at("skin").at("weapon_type").as_string());
            def.set_stackable(root.at("stackable").as_int(0) != 0);
            def.set_range(root.at("range").as_int(0));
            // Top-level "level" is item definition grade (shown as [ Grade N ]), not the equip
            // limit.
            def.set_grade(root.at("level").as_int(0));
            def.set_rarity(root.at("rarity").as_int(0));
            def.set_endurance(root.at("endurance").as_int(0));
            if (root.get("buy")) {
                def.set_buy(root.at("buy").as_int(0));
            }
            if (root.get("sell")) {
                def.set_sell(root.at("sell").as_int(0));
            }
            if (const platform::Json* description = root.get("description");
                description && description->is_string()) {
                def.set_description(description->as_string());
            }

            const platform::Json& clazz = root.at("class");
            if (clazz.is_array() && !clazz.array().empty()) {
                def.set_class_big(to_lower(clazz.array()[0].as_string()));
                if (clazz.array().size() > 1) {
                    def.set_class_small(to_lower(clazz.array()[1].as_string()));
                }
                def.set_clazz(def.class_big());
                if (!def.class_small().empty()) {
                    def.set_clazz(def.clazz() + " " + def.class_small());
                }
            } else if (clazz.is_string()) {
                def.set_clazz(to_lower(clazz.as_string()));
                const auto sp = def.clazz().find(' ');
                if (sp == std::string::npos) {
                    def.set_class_big(def.clazz());
                } else {
                    def.set_class_big(def.clazz().substr(0, sp));
                    def.set_class_small(def.clazz().substr(sp + 1));
                }
            }
            // compatibility behavior never copies item definition endurance.
            if (def.is_ornament()) {
                def.set_endurance(0);
            }

            const platform::Json& action = root.at("action");
            if (action.is_array() && action.array().size() >= 2) {
                def.set_action_class(action.array()[0].as_int(0));
                def.set_action_index(action.array()[1].as_int(0));
            }

            const platform::Json& limit = root.at("limit");
            if (limit.is_object()) {
                def.set_class_limit(class_limit_from_name(to_lower(limit.at("class").as_string())));
                def.set_level_limit(limit.at("level").as_int(0));
            }

            def.set_usable(root.at("use").as_int(0) != 0);
            def.set_effect(root.at("effect").as_int(0));

            const platform::Json& cooltime = root.at("cooltime");
            if (cooltime.is_array() && !cooltime.array().empty()) {
                def.set_cooltime_ms(cooltime.array()[0].as_int(0));
            } else if (cooltime.is_number()) {
                def.set_cooltime_ms(cooltime.as_int(0));
            }

            const platform::Json& specialty = root.at("specialty");
            if (specialty.is_object()) {
                const auto& coupon = specialty.at("honor_reward_points");
                def.set_honor_coupon(def.usable() && def.class_big() == "general" &&
                                     def.class_small() == "etc" && coupon.is_number() &&
                                     coupon.number() >= 1 &&
                                     coupon.number() <= std::numeric_limits<int>::max() &&
                                     coupon.number() == coupon.as_int(0));
                def.set_hit(specialty.at("hit").as_int(0));
                def.set_attack_speed_ms(specialty.at("aspeed").as_int(0));
                def.set_dodge(specialty.at("dodge").as_int(0));
                def.set_absorb(specialty.at("absorb").as_int(0));
                def.set_defense(read_int_or_first(specialty.at("defense")));
                def.set_str(specialty.at("str").as_int(0));
                def.set_hth(specialty.at("hth").as_int(0));
                def.set_intel(specialty.at("int").as_int(0));
                def.set_wis(specialty.at("wis").as_int(0));
                def.set_dex(specialty.at("dex").as_int(0));
                def.set_hp(specialty.at("hp").as_int(0));
                def.set_mp(specialty.at("mp").as_int(0));
                def.set_resist_fire(specialty.at("resistfire").as_int(0));
                def.set_resist_ice(specialty.at("resistice").as_int(0));
                def.set_resist_lightning(specialty.at("resistlitning").as_int(0));
                def.set_resist_curse(specialty.at("resistcurse").as_int(0));
                def.set_resist_palsy(specialty.at("resistpalsy").as_int(0));

                int amin = 0;
                int amax = 0;
                read_minmax(specialty.at("attack"), amin, amax);
                def.set_min_attack(amin);
                def.set_max_attack(amax);

                int mmin = 0;
                int mmax = 0;
                read_minmax(specialty.at("magic"), mmin, mmax);
                def.set_min_magic(mmin);
                def.set_max_magic(mmax);

                const platform::Json& teleport = specialty.at("teleport");
                if (teleport.is_object()) {
                    def.set_teleport_loc(teleport.at("loc").as_int(-1));
                }
                const platform::Json& refresh = specialty.at("refresh");
                if (refresh.is_object()) {
                    def.set_auto_hp(refresh.at("auto_hp").boolean());
                    const std::string stat = to_lower(refresh.at("stat").as_string());
                    const int amount = refresh.at("amount").as_int(0);
                    if (stat == "hp") {
                        def.set_refresh_hp(amount);
                    } else if (stat == "mp") {
                        def.set_refresh_mp(amount);
                    }
                }
                const platform::Json& buff = specialty.at("buff");
                if (buff.is_array() && buff.array().size() >= 2) {
                    def.set_buff_type(buff.array()[0].as_int(-1));
                    def.set_buff_remain(buff.array()[1].as_int(0));
                    if (buff.array().size() >= 3) {
                        def.set_buff_amount(buff.array()[2].as_int(0));
                    }
                }
                const platform::Json& revival = specialty.at("revival");
                if (revival.is_number()) {
                    def.set_revival_extra_cut(revival.as_int(-1));
                }
                const platform::Json& changeprefix = specialty.at("changeprefix");
                if (changeprefix.is_array() && changeprefix.array().size() >= 5) {
                    def.set_has_changeprefix(true);
                    def.set_changeprefix_target(to_lower(changeprefix.array()[0].as_string()));
                    def.set_changeprefix_rate0(changeprefix.array()[1].as_int(0));
                    def.set_changeprefix_prefix0(changeprefix.array()[2].as_int(0));
                    def.set_changeprefix_rate1(changeprefix.array()[3].as_int(0));
                    def.set_changeprefix_prefix1(changeprefix.array()[4].as_int(0));
                }
                const platform::Json& repair = specialty.at("repair");
                if (repair.is_array() && repair.array().size() >= 2) {
                    def.set_has_repair(true);
                    def.set_repair_mode(repair.array()[0].as_int(0));
                    def.set_repair_amount(repair.array()[1].as_int(0));
                }
                const std::string charming = to_lower(specialty.at("charming").as_string());
                if (!charming.empty()) {
                    def.set_charming(charming);
                }
                const platform::Json& bof = specialty.at("bof");
                if (bof.type() == platform::Json::Type::Bool) {
                    def.set_bof(bof.boolean());
                } else if (bof.is_number()) {
                    def.set_bof(bof.as_int(0) != 0);
                }
            }

            {
                const platform::Json* extra = root.get("extra");
                if (extra && extra->is_object()) {
                    const platform::Json* race = extra->get("race");
                    if (race && race->is_number()) {
                        def.set_transform_race(race->as_int(-1));
                    }
                    const platform::Json* cocoon = extra->get("cocoon");
                    if (cocoon && cocoon->is_array()) {
                        const auto& letters = cocoon->array();
                        const std::size_t n = std::min<std::size_t>(letters.size(), 5);
                        for (std::size_t i = 0; i < n; ++i) {
                            def.set_cocoon_ability(static_cast<int>(i),
                                                   cocoon_letter_divisor(letters[i].as_string()));
                        }
                    }
                    const platform::Json* imageex = extra->get("imageex");
                    if (imageex && imageex->is_array()) {
                        const auto& stems = imageex->array();
                        const std::size_t n = std::min<std::size_t>(stems.size(), 3);
                        for (std::size_t i = 0; i < n; ++i) {
                            def.set_image_ex(static_cast<int>(i), stems[i].as_string());
                        }
                    }
                }
            }

            if (def.name().empty()) {
                if (!def.image().empty()) {
                    def.set_name(def.image());
                } else {
                    def.set_name("Item" + std::to_string(def.index()));
                }
            }
            return def;
        }

        bool load_items_json_dir(const std::string& dir,
                                 std::unordered_map<std::int32_t, ItemDef>& out,
                                 std::unordered_map<std::int32_t, std::string>& source_paths) {
            if (!platform::is_directory(dir)) {
                return false;
            }
            out.clear();
            source_paths.clear();
            for (const auto& rel : platform::list_files_recursive(dir)) {
                if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0) {
                    continue;
                }
                const auto slash = rel.find_last_of('/');
                const std::string base = slash == std::string::npos ? rel : rel.substr(slash + 1);
                if (base == "manifest.json") {
                    continue;
                }
                const std::string path = platform::join_path(dir, rel);
                const auto bytes = platform::read_file(path);
                if (!bytes) {
                    continue;
                }
                platform::Json root;
                if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                    !root.is_object()) {
                    continue;
                }
                ItemDef def = item_from_json(root);
                if (def.index() > 0) {
                    const std::int32_t index = def.index();
                    out[index] = std::move(def);
                    source_paths[index] = rel;
                }
            }
            log::info("items json loaded: " + std::to_string(out.size()) + " from " + dir);
            return !out.empty();
        }

        /**
         * Interface labels used by item tips and buff names.
         * @param id Label id.
         * @return Label text, or nullptr when the id has no label.
         */
        [[nodiscard]] const char* ui_label(int id) {
            switch (id) {
            case 23:
                return "Knight";
            case 24:
                return "Magician";
            case 25:
                return "Archer";
            case 33:
                return "Strength";
            case 34:
                return "Health";
            case 35:
                return "Intelligence";
            case 36:
                return "Wisdom";
            case 37:
                return "Agility";
            case 38:
                return "On Target Point";
            case 42:
                return "Min. Physical Attack";
            case 43:
                return "Max. Physical Attack";
            case 44:
                return "Min. Magical Attack";
            case 45:
                return "Max. Magical Attack";
            case 47:
                return "Health Point";
            case 48:
                return "Magic Point";
            case 54:
                return "Exp";
            case 75:
                return "Grade";
            case 76:
                return "Endurance";
            case 77:
                return "Destroyed";
            case 78:
                return "Evasion Point";
            case 79:
                return "Defense";
            case 80:
                return "Absorption Rate";
            case 81:
                return "Fire Resistance";
            case 82:
                return "Ice Resistance";
            case 83:
                return "Lightning Resistance";
            case 84:
                return "Curse Resistance";
            case 85:
                return "Non Elemental Resistance";
            case 86:
                return "Physical Attack";
            case 87:
                return "Magical Attack";
            case 88:
                return "Effective Range";
            case 89:
                return "On Target Point";
            case 111:
                return "Geon(s)";
            case 140:
                return "Evaded the enemy's attack";
            case 156:
                return "Price when selling : ";
            case 302:
                return "Strengthened";
            case 377:
                return "Effective on: Armor";
            case 378:
                return "Effective on: Weapon";
            case 379:
                return "Effective on: Accessory";
            case 380:
                return "Percentage of Success: %1!d! %%";
            case 671:
                return "Recovering Health";
            case 672:
                return "Recovering Energy";
            case 852:
                return "Explosive Blow of Weapon [%1!d!]";
            case 853:
                return "Percentage of Occurrence %1!d! %%";
            case 854:
                return "Damage %1!d! ~ %2!d!";
            case 919:
                return "Final Striking Power[Physical/Magical] is increased by 10%";
            case 920:
                return "Final damage from the monster is decreased by 10%";
            case 921:
                return "Activity of Explosive Blow percentage is increased by 3%";
            case 922:
                return "Activity of Explosive Blow percentage is increased by 5%";
            case 923:
                return "Final On Target Point is increased by 10";
            case 924:
                return "Final Evasion Point is increased by 5";
            case 925:
                return "Absorbing the enemy's attack with 10% activity percentage";
            case 931:
                return "Draining the enemy's HP with 10% activity percentage";
            case 1278:
                return "Mixing : ";
            case 1279:
                return "Stone of Shadow";
            case 1280:
                return "Stone of Holy";
            case 1281:
                return "Stone of Flame";
            case 1282:
                return "Stone of Ice";
            case 1283:
                return "Stone of Lightening";
            case 1284:
                return "Stone of Poison";
            case 1285:
                return "Stone of Paralysis";
            case 1286:
                return "Stone of Strength";
            case 1287:
                return "Stone of Skill";
            case 1288:
                return "Stone of Mystery";
            case 1289:
                return "Stone of Demon's Blood";
            case 1292:
                return "HP+20/Strength+2/Intelligence+2/Agility+2";
            case 1293:
                return "MP+20/Wisdom+2/Health+2";
            case 1294:
                return "Flame damage with 5% probability";
            case 1295:
                return "Ice damage with 5% probability";
            case 1296:
                return "Lightening damage with 5% probability,";
            case 1297:
                return "Reduce the target's defense by 10";
            case 1298:
                return "Poison damage with 5% probability";
            case 1299:
                return "Paralysis with 5% probability";
            case 1300:
                return "HP + 10%, MP - 5%";
            case 1301:
                return "HP - 5%, MP + 10%";
            case 1302:
                return "HP+50/Strength+3/Intelligence+3/Agility+3";
            case 1303:
                return "MP+50/Wisdom+5/Health+5";
            case 1304:
                return "Flame damage with 10% probability";
            case 1305:
                return "Ice damage with 10% probability";
            case 1306:
                return "Lightening damage with 10% probability,";
            case 1307:
                return "Reduce the target's defense by 10";
            case 1308:
                return "Poison damage with 10% probability";
            case 1309:
                return "Paralysis with 10% probability";
            case 1310:
                return "HP + 20%, MP - 10%";
            case 1311:
                return "HP - 10%, MP + 20%";
            case 1312:
                return "Min/Max Attack + 50";
            case 1313:
                return "HP+50/MP+50/Strength+6,";
            case 1314:
                return "/Intelligence+6/Agility+6/Wisdom+10/Health+10";
            case 1324:
                return " Equip : Bead of Fire ";
            case 2000:
                return "Thief";
            default:
                return nullptr;
            }
        }

    }  // namespace

    void ItemDef::set_rarity(int v) {
        m_rarity = std::clamp(v, kItemRarityMin, kItemRarityMax);
    }

    void ItemDef::glow_color(float& r, float& g, float& b) const {
        if (is_coin()) {
            r = 1.00f;
            g = 0.82f;
            b = 0.18f;
            return;
        }
        switch (m_rarity) {
        case static_cast<int>(ItemRarity::Blue):
            r = 0.25f;
            g = 0.45f;
            b = 1.00f;
            break;
        case static_cast<int>(ItemRarity::Green):
            r = 0.22f;
            g = 0.95f;
            b = 0.28f;
            break;
        case static_cast<int>(ItemRarity::Orange):
            r = 1.00f;
            g = 0.55f;
            b = 0.12f;
            break;
        case static_cast<int>(ItemRarity::Red):
            r = 1.00f;
            g = 0.18f;
            b = 0.12f;
            break;
        case static_cast<int>(ItemRarity::Purple):
            r = 0.78f;
            g = 0.28f;
            b = 0.95f;
            break;
        default:
            r = 0.95f;
            g = 0.95f;
            b = 0.95f;
            break;
        }
    }

    bool ItemDef::is_skin() const {
        return m_class_big == "skin" && skin_slot() != protocol::SkinSlot::None;
    }

    protocol::SkinSlot ItemDef::skin_slot() const {
        return m_class_big == "skin" ? protocol::skin_slot_for_part(m_class_small)
                                     : protocol::SkinSlot::None;
    }

    bool ItemDef::full_body_skin() const {
        return is_skin() && m_class_small == "body";
    }

    bool ItemDef::uses_costume_mesh() const {
        return is_skin() && (full_body_skin() || m_skin_costume_mesh);
    }

    bool ItemDef::skin_keeps_hair() const {
        return full_body_skin() && m_skin_keep_hair;
    }

    bool ItemDef::skin_replaces_face() const {
        return full_body_skin() && m_skin_replace_face;
    }

    bool ItemDef::skin_matches_weapon(const ItemDef& weapon) const {
        const auto type = protocol::resolve_weapon_skin_type(m_skin_weapon_type, class_limit());
        return skin_slot() == protocol::SkinSlot::Weapon && weapon.is_weapon() &&
               type != protocol::WeaponSkinType::None &&
               type == protocol::weapon_skin_type_for_name(weapon.class_small());
    }

    bool ItemDef::is_weapon() const {
        return m_class_big == "weapon";
    }

    bool ItemDef::is_defense() const {
        return m_class_big == "defense";
    }

    bool ItemDef::is_ornament() const {
        return m_class_big == "ornament";
    }

    bool ItemDef::is_coin() const {
        return m_class_big == "money" || m_class_small == "coin" || m_index == 31;
    }

    bool ItemDef::is_transform() const {
        return m_class_big == "transform";
    }

    bool ItemDef::is_pet() const {
        return m_class_big == "pet" || m_class_big == "limitedpet";
    }

    bool ItemDef::is_ride() const {
        return m_class_big == "ride";
    }

    bool ItemDef::is_quest() const {
        return m_class_big == "quest";
    }

    bool ItemDef::can_equip() const {
        return m_wearable &&
               ((m_class_big == "yinyang" || m_class_big == "gather" || m_class_big == "yeoiju") ||
                is_weapon() || is_defense() || is_ornament() || is_transform() || is_pet() ||
                is_ride() || is_skin());
    }

    bool ItemDef::can_equip_by(int clazz, int level) const {
        if (!can_equip()) {
            return false;
        }
        if (m_level_limit > 0 && level < m_level_limit) {
            return false;
        }
        if (m_class_limit >= 0 && m_class_limit != clazz) {
            return false;
        }
        return true;
    }

    const ItemDef* ItemDb::find(std::int32_t index) const {
        auto it = m_by_index.find(index);
        return it == m_by_index.end() ? nullptr : &it->second;
    }

    std::string ItemDb::source_path(std::int32_t index) const {
        const auto it = m_source_paths.find(index);
        if (it == m_source_paths.end()) {
            return {};
        }
        return m_source_path_root.empty() ? it->second
                                          : platform::join_path(m_source_path_root, it->second);
    }

    std::string ItemDb::sys(int id) const {
        if (const char* label = ui_label(id)) {
            return label;
        }
        // A leading '#' is a tooltip color code, so unknown ids must not use it.
        return "[" + std::to_string(id) + "]";
    }

    bool ItemDb::load(const std::string& configuration_root,
                      const std::string& data_root,
                      const std::string& items_dir) {
        m_hypertext_dir = platform::join_path(data_root, "HyperText");
        if (!m_prefixes.load(platform::join_path(configuration_root, "prefixes"))) {
            log::warn("prefixes json: none loaded from config/prefixes");
        }
        if (!m_bof.load_file(platform::join_path(configuration_root, "bof.json"))) {
            log::warn("bof json: none loaded from config/bof.json");
        }

        bool ok = false;
        m_by_index.clear();
        m_source_paths.clear();
        m_source_path_root.clear();
        const std::string preferred = platform::join_path(configuration_root, "items");
        if (!items_dir.empty() && load_items_json_dir(items_dir, m_by_index, m_source_paths)) {
            ok = true;
            m_source_path_root = items_dir == preferred ? "config/items" : items_dir;
        } else if (items_dir != preferred &&
                   load_items_json_dir(preferred, m_by_index, m_source_paths)) {
            ok = true;
            m_source_path_root = "config/items";
        }
        if (!ok) {
            log::warn("items json: no definitions loaded from " + preferred +
                      (items_dir.empty() || items_dir == preferred ? "" : " or " + items_dir));
        }

        server::QuestDb quests;
        if (quests.load_dir(platform::join_path(configuration_root, "quests"))) {
            server::ItemGroupDb drops;
            (void) drops.load_groups_dir(platform::join_path(configuration_root, "groups"));
            const auto task_indices = quests.task_item_indices(drops);
            for (auto& entry : m_by_index) {
                entry.second.set_task(entry.second.is_quest() &&
                                      task_indices.count(entry.first) != 0);
            }
        }
        return ok;
    }

    std::string ItemDb::icon_filename(std::int32_t index, int egg_level) const {
        const ItemDef* d = find(index);
        if (!d) {
            return {};
        }
        std::string stem = d->image();
        if (d->is_transform()) {
            const int ex = protocol::transform_egg_image_ex_index(egg_level);
            if (ex >= 0) {
                const std::string alt = d->image_ex(ex);
                if (!alt.empty()) {
                    stem = alt;
                }
            }
        }
        if (stem.empty()) {
            return {};
        }
        return resolve_icon_bmp(m_hypertext_dir, stem);
    }

}  // namespace openkal::client
