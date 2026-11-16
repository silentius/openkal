/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ItemTooltip.h"

#include "openkal/client/GeonColor.h"
#include "openkal/client/MixVisual.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/DragonFusing.h"
#include "openkal/server/Qigong.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <sstream>

namespace openkal::client {
    namespace {

        /** Tooltip markup colour codes. */
        constexpr const char* kWhite = "#8";
        constexpr const char* kRed = "#1";
        constexpr const char* kYellow = "#3";
        constexpr const char* kGreen = "#4";
        constexpr const char* kBlue = "#6";
        constexpr const char* kOrange = "#2";
        constexpr const char* kPurple = "#7";
        constexpr const char* kDarkPurple = "#b";
        constexpr const char* kDarkGray = "#a";
        constexpr const char* kEndl = "#n";

        /** Dashed separator under talisman stats when Explosive Blow is present. */
        constexpr const char* kEbRule = "----------------------";

        [[nodiscard]] const std::string& tip_description(const ItemDef& def) {
            return def.description();
        }

        [[nodiscard]] std::string comma_int(int value) {
            const bool neg = value < 0;
            std::uint64_t n = neg ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(value))
                                  : static_cast<std::uint64_t>(value);
            std::string digits = std::to_string(n);
            std::string out;
            out.reserve(digits.size() + digits.size() / 3 + (neg ? 1 : 0));
            if (neg) {
                out.push_back('-');
            }
            const std::size_t first = digits.size() % 3;
            if (first != 0) {
                out.append(digits, 0, first);
            }
            for (std::size_t i = first; i < digits.size(); i += 3) {
                if (!out.empty() && !(neg && out.size() == 1)) {
                    out.push_back(',');
                }
                out.append(digits, i, 3);
            }
            return out;
        }

        [[nodiscard]] int charm_bonus_attack(int x) {
            if (x <= 0) {
                return 0;
            }
            return x * (2 * x + 7) / 9;
        }

        [[nodiscard]] int explosive_blow_activity_percent(int upgrade_level) {
            static constexpr int kPer[20] = {10, 11, 11, 12, 12, 13, 13, 14, 14, 15,
                                             15, 16, 16, 17, 17, 18, 18, 19, 19, 20};
            if (upgrade_level <= 0) {
                return 0;
            }
            if (upgrade_level >= 20) {
                return kPer[19];
            }
            return kPer[upgrade_level - 1];
        }

        [[nodiscard]] bool is_wand(const ItemDef& def) {
            return def.class_small() == "wand";
        }

        [[nodiscard]] bool has_charm(const ItemTipExtras& extras) {
            return extras.x_attack > 0 || extras.x_magic > 0 || extras.x_hit > 0 ||
                   extras.x_defense > 0 || extras.x_dodge > 0;
        }

        [[nodiscard]] int charm_sell_cube(int plus) {
            if (plus <= 0) {
                return 0;
            }
            return plus * plus * plus * 100;
        }

        /**
         * Appends the NPC sell line using the shared Geon amount color.
         * Prefix adds percent of base Sell; talisman plus cubes add
         * on weapons. Mixing / Explosive Blow are not part of the formula.
         */
        void append_sell_price(std::ostringstream& s,
                               const ItemDef& def,
                               const ItemDb& catalog,
                               const PrefixDef* prefix,
                               const ItemTipExtras& extras) {
            int n_sell = def.sell();
            if (prefix) {
                n_sell += (def.sell() * prefix->sell()) / 100;
            }
            if (has_charm(extras)) {
                const std::string& small = def.class_small();
                if (small == "sword" || small == "bow" || small == "sword2h" || small == "dagger") {
                    n_sell += charm_sell_cube(extras.x_attack);
                    n_sell += 2 * charm_sell_cube(extras.x_hit);
                    n_sell += charm_sell_cube(extras.x_defense);
                    n_sell += 2 * charm_sell_cube(extras.x_dodge);
                } else if (small == "wand") {
                    n_sell += charm_sell_cube(extras.x_attack);
                    n_sell += 2 * charm_sell_cube(extras.x_hit);
                    n_sell += charm_sell_cube(extras.x_magic);
                    n_sell += 2 * charm_sell_cube(extras.x_dodge);
                    // Defense contributes its own stat value to the sell-price calculation.
                    n_sell += charm_sell_cube(extras.x_defense);
                }
            }
            if (n_sell <= 0) {
                return;
            }
            s << kEndl << geon_color(n_sell).markup << catalog.sys(156) << comma_int(n_sell)
              << kWhite;
        }

        void append_stat_line(std::ostringstream& s, const ItemDb& catalog, int sys_id, int value) {
            if (value == 0) {
                return;
            }
            s << catalog.sys(sys_id) << " " << value << kEndl;
        }

        void
        append_stat_plain(std::ostringstream& s, const ItemDb& catalog, int sys_id, int value) {
            s << catalog.sys(sys_id) << " " << value;
        }

        void append_stat_bonus(std::ostringstream& s,
                               const ItemDb& catalog,
                               int sys_id,
                               int base,
                               int addition,
                               const char* scolor) {
            s << catalog.sys(sys_id) << " " << base;
            if (addition != 0) {
                s << scolor << " (+" << addition << ")" << kWhite;
            }
        }

        void append_minmax(std::ostringstream& s,
                           const ItemDb& catalog,
                           int sys_id,
                           int min_v,
                           int max_v,
                           int addition,
                           const char* scolor) {
            s << catalog.sys(sys_id) << " " << min_v << " ~ " << max_v;
            if (addition != 0) {
                s << scolor << " (+" << addition << ")" << kWhite;
            }
            s << kEndl;
        }

        void append_attr_block(std::ostringstream& s, const ItemDb& catalog, const ItemDef& def) {
            append_stat_line(s, catalog, 33, def.str());
            append_stat_line(s, catalog, 34, def.hth());
            append_stat_line(s, catalog, 35, def.intel());
            append_stat_line(s, catalog, 36, def.wis());
            append_stat_line(s, catalog, 37, def.dex());
            append_stat_line(s, catalog, 47, def.hp());
            append_stat_line(s, catalog, 48, def.mp());
        }

        void append_weapon_stats(std::ostringstream& s,
                                 const ItemDb& catalog,
                                 const ItemDef& def,
                                 const ItemTipExtras& extras,
                                 const char* scolor) {
            const int atk_bonus = charm_bonus_attack(extras.x_attack);
            const int mag_bonus = charm_bonus_attack(extras.x_magic);
            if (def.max_attack() != 0) {
                append_minmax(
                    s, catalog, 86, def.min_attack(), def.max_attack(), atk_bonus, scolor);
            }
            if (def.max_magic() != 0) {
                append_minmax(s, catalog, 87, def.min_magic(), def.max_magic(), mag_bonus, scolor);
            }
            append_attr_block(s, catalog, def);
            append_stat_plain(s, catalog, 88, def.range());
            s << kEndl;
            append_stat_bonus(s, catalog, 89, def.hit(), extras.x_hit, scolor);
            s << kEndl;
        }

        void append_defence_stats(std::ostringstream& s,
                                  const ItemDb& catalog,
                                  const ItemDef& def,
                                  const ItemTipExtras& extras,
                                  const char* scolor) {
            append_attr_block(s, catalog, def);
            append_stat_bonus(s, catalog, 78, def.dodge(), extras.x_dodge, scolor);
            s << kEndl;
            append_stat_bonus(s, catalog, 79, def.defense(), extras.x_defense, scolor);
            s << kEndl;
            append_stat_plain(s, catalog, 80, def.absorb());
            s << kEndl;
            append_stat_plain(s, catalog, 81, def.resist_fire());
            s << " ";
            append_stat_plain(s, catalog, 82, def.resist_ice());
            s << kEndl;
            append_stat_plain(s, catalog, 83, def.resist_lightning());
            s << " ";
            append_stat_plain(s, catalog, 84, def.resist_curse());
            s << kEndl;
            append_stat_plain(s, catalog, 85, def.resist_palsy());
            s << kEndl;
        }

        void append_shield_stats(std::ostringstream& s,
                                 const ItemDb& catalog,
                                 const ItemDef& def,
                                 const ItemTipExtras& extras,
                                 const char* scolor) {
            append_stat_bonus(s, catalog, 78, def.dodge(), extras.x_dodge, scolor);
            s << kEndl;
            append_stat_bonus(s, catalog, 79, def.defense(), extras.x_defense, scolor);
            s << kEndl;
            if (extras.x_defense > 0) {
                s << scolor;
                append_stat_plain(s, catalog, 79, extras.x_defense);
                s << kEndl << kWhite;
            }
            if (def.absorb() > 0) {
                append_stat_plain(s, catalog, 80, def.absorb());
                s << kEndl;
            }
        }

        void append_ornament_stats(std::ostringstream& s,
                                   const ItemDb& catalog,
                                   const ItemDef& def,
                                   const ItemTipExtras& extras,
                                   const char* scolor) {
            const int atk_bonus = charm_bonus_attack(extras.x_attack);
            const int mag_bonus = charm_bonus_attack(extras.x_magic);
            if (def.max_attack() != 0) {
                append_minmax(
                    s, catalog, 86, def.min_attack(), def.max_attack(), atk_bonus, scolor);
            }
            if (def.max_magic() != 0) {
                append_minmax(s, catalog, 87, def.min_magic(), def.max_magic(), mag_bonus, scolor);
            }
            append_attr_block(s, catalog, def);
        }

        [[nodiscard]] int transform_cocoon_stat(int egg_level, int divisor) {
            if (divisor <= 0) {
                return 0;
            }
            return 1 + std::max(1, egg_level) / divisor;
        }

        void append_transform_stats(std::ostringstream& s,
                                    const ItemDb& catalog,
                                    const ItemDef& def,
                                    int player_level,
                                    const ItemTipExtras& extras) {
            const int egg_level = extras.egg_level > 0 ? extras.egg_level : 1;
            const int egg_exp = std::max(0, extras.egg_exp);
            const int need = protocol::need_transform_exp_for_level(egg_level);
            const double pct = protocol::transform_exp_percent(egg_exp, need);
            char exp_buf[16];
            std::snprintf(exp_buf, sizeof(exp_buf), "%.2f%%", pct);

            s << kEndl << kEndl;
            s << (egg_level <= player_level ? kWhite : kRed);
            s << catalog.sys(75) << " " << egg_level << kWhite;
            s << kEndl << catalog.sys(54) << " " << exp_buf << kEndl;

            static constexpr int kCocoonSys[5] = {33, 34, 37, 35, 36};
            for (int i = 0; i < 5; ++i) {
                append_stat_line(s,
                                 catalog,
                                 kCocoonSys[i],
                                 transform_cocoon_stat(egg_level, def.cocoon_ability(i)));
            }
        }

        [[nodiscard]] bool is_stack_tip_class(const ItemDef& def) {
            const std::string& small = def.class_small();
            if (small == "refresh" || small == "etc" || small == "charm" || small == "repair" ||
                small == "monsterspawn" || small == "summonscroll" || small == "hpautorefresh") {
                return true;
            }
            return def.is_quest();
        }

        [[nodiscard]] bool is_gear_tip_class(const ItemDef& def) {
            return def.is_weapon() || def.is_defense() || def.is_ornament();
        }

        [[nodiscard]] std::string format_sys_ints(std::string tmpl,
                                                  std::initializer_list<int> values) {
            int index = 1;
            for (int value : values) {
                const std::string needle = "%" + std::to_string(index) + "!d!";
                const auto pos = tmpl.find(needle);
                if (pos != std::string::npos) {
                    tmpl.replace(pos, needle.size(), std::to_string(value));
                }
                ++index;
            }
            std::size_t pct = 0;
            while ((pct = tmpl.find("%%", pct)) != std::string::npos) {
                tmpl.replace(pct, 2, "%");
                pct += 1;
            }
            return tmpl;
        }

        void
        append_prefix_stats(std::ostringstream& s, const ItemDb& catalog, const PrefixDef& prefix) {
            append_stat_line(s, catalog, 33, prefix.str());
            append_stat_line(s, catalog, 34, prefix.hth());
            append_stat_line(s, catalog, 35, prefix.intel());
            append_stat_line(s, catalog, 36, prefix.wis());
            append_stat_line(s, catalog, 37, prefix.dex());
            append_stat_line(s, catalog, 47, prefix.hp());
            append_stat_line(s, catalog, 48, prefix.mp());
            append_stat_line(s, catalog, 38, prefix.hit());
            append_stat_line(s, catalog, 42, prefix.min_attack());
            append_stat_line(s, catalog, 43, prefix.max_attack());
            append_stat_line(s, catalog, 44, prefix.min_magic());
            append_stat_line(s, catalog, 45, prefix.max_magic());
            append_stat_line(s, catalog, 79, prefix.defense());
        }

        void append_random_stat(std::ostringstream& s,
                                const ItemDb& catalog,
                                const ItemDef& def,
                                protocol::RandomItemStatType type,
                                int value) {
            if (value <= 0 || type == protocol::RandomItemStatType::None) {
                return;
            }
            const bool weapon_stat =
                type >= protocol::RandomItemStatType::WeaponMinPhysicalDamagePercent &&
                type <= protocol::RandomItemStatType::WeaponMagicalDamagePercent;
            if ((weapon_stat && !def.is_weapon()) || (!weapon_stat && !def.is_defense())) {
                return;
            }
            s << kWhite;
            const auto percent_line = [&](const char* label) {
                s << label << " +" << value << "%" << kEndl;
            };
            const auto defense_and_primary = [&](int sys_id) {
                s << catalog.sys(79) << " +" << value << "% / " << catalog.sys(sys_id) << " +"
                  << value << kEndl;
            };
            switch (type) {
            case protocol::RandomItemStatType::WeaponMinPhysicalDamagePercent:
                percent_line("Min. Physical Damage");
                break;
            case protocol::RandomItemStatType::WeaponMinMagicalDamagePercent:
                percent_line("Min. Magical Damage");
                break;
            case protocol::RandomItemStatType::WeaponMaxPhysicalDamagePercent:
                percent_line("Max. Physical Damage");
                break;
            case protocol::RandomItemStatType::WeaponMaxMagicalDamagePercent:
                percent_line("Max. Magical Damage");
                break;
            case protocol::RandomItemStatType::WeaponPhysicalDamagePercent:
                percent_line("Min. and Max. Physical Damage");
                break;
            case protocol::RandomItemStatType::WeaponMagicalDamagePercent:
                percent_line("Min. and Max. Magical Damage");
                break;
            case protocol::RandomItemStatType::ArmorStrength:
                append_stat_line(s, catalog, 33, value);
                break;
            case protocol::RandomItemStatType::ArmorHealth:
                append_stat_line(s, catalog, 34, value);
                break;
            case protocol::RandomItemStatType::ArmorIntelligence:
                append_stat_line(s, catalog, 35, value);
                break;
            case protocol::RandomItemStatType::ArmorWisdom:
                append_stat_line(s, catalog, 36, value);
                break;
            case protocol::RandomItemStatType::ArmorDexterity:
                append_stat_line(s, catalog, 37, value);
                break;
            case protocol::RandomItemStatType::ArmorDefense:
                append_stat_line(s, catalog, 79, value);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercent:
                percent_line("Defense");
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentStrength:
                defense_and_primary(33);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentHealth:
                defense_and_primary(34);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentIntelligence:
                defense_and_primary(35);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentWisdom:
                defense_and_primary(36);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentDexterity:
                defense_and_primary(37);
                break;
            case protocol::RandomItemStatType::ArmorDefensePercentDefense:
                s << catalog.sys(79) << " +" << value << "% / " << catalog.sys(79) << " +" << value
                  << kEndl;
                break;
            case protocol::RandomItemStatType::None:
            default:
                break;
            }
        }

        void append_changeprefix_option(std::ostringstream& s,
                                        const ItemDb& catalog,
                                        int prefix_index,
                                        int shown_rate) {
            const PrefixDef* prefix = catalog.prefixes().find(prefix_index);
            const std::string name = prefix ? prefix->name() : std::string{};
            if (!name.empty()) {
                s << kEndl << kWhite << name;
            }
            s << kEndl << kWhite << format_sys_ints(catalog.sys(380), {shown_rate}) << kEndl;
            if (prefix) {
                s << kGreen;
                append_prefix_stats(s, catalog, *prefix);
                s << kWhite;
            }
        }

        void append_changeprefix_block(std::ostringstream& s,
                                       const ItemDef& def,
                                       const ItemDb& catalog) {
            const std::string& target = def.changeprefix_target();
            if (target == "ornament") {
                s << kEndl << catalog.sys(379);
            } else if (target == "weapon") {
                s << kEndl << catalog.sys(378);
            } else if (target == "defense") {
                s << kEndl << catalog.sys(377);
            }

            if (def.changeprefix_prefix0() != 0) {
                append_changeprefix_option(
                    s, catalog, def.changeprefix_prefix0(), def.changeprefix_rate0());
            }
            if (def.changeprefix_prefix1() != 0) {
                const int shown = def.changeprefix_rate1() - def.changeprefix_rate0();
                append_changeprefix_option(s, catalog, def.changeprefix_prefix1(), shown);
            }
        }

        void append_endurance(std::ostringstream& s,
                              const ItemDb& catalog,
                              int cur_endurance,
                              int max_endurance) {
            if (max_endurance <= 0) {
                return;
            }
            if (cur_endurance <= 0) {
                s << "[ " << catalog.sys(77) << " ]" << kEndl;
                return;
            }
            s << catalog.sys(76) << " " << cur_endurance << " / " << max_endurance << kEndl;
        }

        [[nodiscard]] const char* rarity_name_color(int rarity) {
            switch (rarity) {
            case static_cast<int>(ItemRarity::Blue):
                return kBlue;
            case static_cast<int>(ItemRarity::Green):
                return kGreen;
            case static_cast<int>(ItemRarity::Orange):
                return "#2";
            case static_cast<int>(ItemRarity::Red):
                return kRed;
            case static_cast<int>(ItemRarity::Purple):
                return "#7";
            default:
                return kWhite;
            }
        }

        [[nodiscard]] const char*
        tip_name_color(bool enchanted, bool has_prefix, int upgrade_level, int rarity) {
            if (upgrade_level > 0) {
                return kBlue;
            }
            if (enchanted) {
                return has_prefix ? kBlue : kYellow;
            }
            if (has_prefix) {
                return kGreen;
            }
            return rarity_name_color(rarity);
        }

        void append_charm_name(std::ostringstream& s,
                               const ItemDef& def,
                               const ItemDb& catalog,
                               const ItemTipExtras& extras) {
            if (!has_charm(extras)) {
                return;
            }
            if (def.is_weapon()) {
                if (is_wand(def)) {
                    if (extras.x_magic > 0) {
                        s << "+" << extras.x_magic << " ";
                    }
                } else if (extras.x_attack > 0) {
                    s << "+" << extras.x_attack << " ";
                }
                if (extras.x_attack > 0 || extras.x_magic > 0 || extras.x_hit > 0) {
                    s << catalog.sys(302) << " ";
                }
            } else if (def.is_defense()) {
                if (extras.x_defense > 0) {
                    s << "+" << extras.x_defense << " ";
                }
                if (extras.x_dodge > 0) {
                    s << catalog.sys(302) << " ";
                }
            }
        }

        void append_explosive_blow(std::ostringstream& s,
                                   const ItemDef& def,
                                   const ItemDb& catalog,
                                   const ItemTipExtras& extras) {
            if (!def.is_weapon() || extras.upgrade_level <= 0) {
                return;
            }
            s << kRed << kEbRule << kEndl;
            s << format_sys_ints(catalog.sys(852), {extras.upgrade_level}) << kEndl;
            s << format_sys_ints(catalog.sys(853),
                                 {explosive_blow_activity_percent(extras.upgrade_level)})
              << kEndl;
            const bool wand = is_wand(def);
            const int bonus = charm_bonus_attack(wand ? extras.x_magic : extras.x_attack);
            const int wmin = wand ? def.min_magic() : def.min_attack();
            const int wmax = wand ? def.max_magic() : def.max_attack();
            if (wmax > 0) {
                const int dmin = wmin + (wmin * extras.upgrade_level / 20) + (bonus * 3);
                const int dmax = wmax + (wmax * extras.upgrade_level / 10) + (bonus * 3);
                s << format_sys_ints(catalog.sys(854), {dmin, dmax}) << kEndl;
            }
            s << kWhite;
        }

        void
        append_mix(std::ostringstream& s, const ItemDef& def, const ItemDb& catalog, int mix_type) {
            if (!def.is_weapon()) {
                return;
            }
            const int stone = MixVisual::stone_type(mix_type);
            const int level = MixVisual::mix_level(mix_type);
            if (stone < 1 || stone > 11 || level < 1) {
                return;
            }
            // Message ranges: names 1279..1289, level-1 stats 1292..1301, level-2 stats 1302..1314.
            int name_id = 1278 + stone;
            int desc_a = 0;
            int desc_b = 0;
            if (level >= 2) {
                static constexpr int kL2A[11] = {
                    1302, 1303, 1304, 1305, 1306, 1308, 1309, 1310, 1311, 1312, 1313};
                static constexpr int kL2B[11] = {0, 0, 0, 0, 1307, 0, 0, 0, 0, 0, 1314};
                desc_a = kL2A[stone - 1];
                desc_b = kL2B[stone - 1];
            } else {
                static constexpr int kL1A[11] = {
                    1292, 1293, 1294, 1295, 1296, 1298, 1299, 1300, 1301, 1312, 1313};
                static constexpr int kL1B[11] = {0, 0, 0, 0, 1297, 0, 0, 0, 0, 0, 1314};
                desc_a = kL1A[stone - 1];
                desc_b = kL1B[stone - 1];
            }
            s << kWhite << kEbRule << kEndl;
            s << kBlue << catalog.sys(1278) << catalog.sys(name_id) << kEndl;
            s << catalog.sys(desc_a);
            if (desc_b != 0) {
                s << kEndl << catalog.sys(desc_b);
            }
            s << kEndl << kWhite;
        }

        void append_bof(std::ostringstream& s,
                        const ItemDef& def,
                        const ItemDb& catalog,
                        const ItemTipExtras& extras) {
            if (extras.bof == 0 || !def.is_defense()) {
                return;
            }
            const server::BofCatalog& bof = catalog.bof();
            if (!bof.ready() || !bof.is_bof_armor_part(def.class_small())) {
                return;
            }
            const server::BofTier tier = bof.tier_for_grade(def.grade());
            if (tier.piece() <= 0) {
                return;
            }
            s << kWhite << kEbRule << kEndl;
            s << kOrange << catalog.sys(1324) << kEndl;
            s << catalog.sys(33) << " " << tier.piece() << kEndl;
            s << catalog.sys(34) << " " << tier.piece() << kEndl;
            if (server::BofCatalog::third_stat_is_intel(def.class_limit())) {
                s << catalog.sys(35) << " " << tier.piece() << kEndl;
            } else {
                s << catalog.sys(37) << " " << tier.piece() << kEndl;
            }
            s << kWhite;
        }

        void append_qigong(std::ostringstream& s, const ItemDef& def, const ItemTipExtras& extras) {
            if (extras.qigong_perforated == 0) {
                return;
            }
            s << kWhite << kEbRule << kEndl;
            s << kOrange << "Perforation" << kEndl;
            const server::QigongBonus bonus = server::QigongBonus::from_effect(
                extras.qigong_effect, def.is_weapon(), def.class_small());
            if (bonus.tier() == 0) {
                s << kWhite;
                return;
            }
            static constexpr const char* kPrimaryNames[] = {"STR", "HTH", "DEX", "WIS", "INT"};
            static constexpr const char* kTierColors[] = {
                kDarkGray, kWhite, kGreen, kYellow, kOrange, kRed, kBlue, kPurple};
            s << kTierColors[bonus.tier() - 1] << "Black Magic: " << bonus.tier_name() << kEndl;
            if (bonus.attack_percent() != 0) {
                s << "Attack " << bonus.attack_percent() << "%  Magic " << bonus.magic_percent()
                  << "%" << kEndl;
                s << "HP " << bonus.hp() << "  MP " << bonus.mp() << kEndl;
                if (bonus.hit() != 0) {
                    s << "OTP " << bonus.hit() << kEndl;
                }
                if (bonus.dodge() != 0) {
                    s << "Evasion " << bonus.dodge() << kEndl;
                }
            } else {
                s << "Defense " << bonus.defense() << kEndl;
            }
            s << kPrimaryNames[bonus.primary_kind()] << " " << bonus.primary_points() << kEndl;
            s << kWhite;
        }

        void append_dragon_fusing(std::ostringstream& s,
                                  const ItemDef& def,
                                  const ItemTipExtras& extras) {
            const server::DragonFusingBonus bonus = server::DragonFusingBonus::from_effect(
                extras.dragon_effect, def.is_weapon(), def.class_small());
            if (bonus.tier() == 0) {
                return;
            }
            static constexpr const char* kPrimaryNames[] = {"STR", "HTH", "DEX", "WIS", "INT"};
            static constexpr const char* kTierColors[] = {
                kDarkGray, kWhite, kGreen, kYellow, kOrange, kRed, kBlue, kPurple};
            s << kWhite << kEbRule << kEndl;
            s << kTierColors[bonus.tier() - 1] << "Dragon Fusing: " << bonus.tier_name() << kEndl;
            if (bonus.attack_percent() != 0) {
                s << "Attack " << bonus.attack_percent() << "%  Magic " << bonus.magic_percent()
                  << "%" << kEndl;
                s << "HP " << bonus.hp() << "  MP " << bonus.mp() << kEndl;
                if (bonus.hit() != 0) {
                    s << "OTP " << bonus.hit() << kEndl;
                }
                if (bonus.dodge() != 0) {
                    s << "Evasion " << bonus.dodge() << kEndl;
                }
            } else {
                s << "Defense " << bonus.defense() << kEndl;
            }
            s << kPrimaryNames[bonus.primary_kind()] << " " << bonus.primary_points() << kEndl;
            s << kWhite;
        }

    }  // namespace

    void TipLine::add(std::string text, float r, float g, float b) {
        if (text.empty()) {
            return;
        }
        TipSpan span;
        span.set_text(std::move(text));
        span.set_r(r);
        span.set_g(g);
        span.set_b(b);
        m_spans.push_back(std::move(span));
    }

    void ItemTooltip::color_for_digit(char digit, float& r, float& g, float& b) {
        switch (digit) {
        case '0':
            r = 0.0f;
            g = 0.0f;
            b = 0.0f;
            break;
        case '1':
            r = 1.0f;
            g = 0.0f;
            b = 0.0f;
            break;
        case '2':
            r = 1.0f;
            g = 128.0f / 255.0f;
            b = 64.0f / 255.0f;
            break;
        case '3':
            r = 1.0f;
            g = 1.0f;
            b = 0.0f;
            break;
        case '4':
            r = 0.0f;
            g = 1.0f;
            b = 0.0f;
            break;
        case '5':
            r = 0.0f;
            g = 1.0f;
            b = 1.0f;
            break;
        case '6':
            r = 0.0f;
            g = 128.0f / 255.0f;
            b = 1.0f;
            break;
        case '7':
            r = 1.0f;
            g = 0.0f;
            b = 1.0f;
            break;
        case '9':
            r = 170.0f / 255.0f;
            g = 85.0f / 255.0f;
            b = 1.0f;
            break;
        case 'a':
        case 'A':
            r = 0.35f;
            g = 0.35f;
            b = 0.35f;
            break;
        case 'b':
        case 'B':
            r = 0.38f;
            g = 0.08f;
            b = 0.50f;
            break;
        case '8':
        default:
            r = 1.0f;
            g = 1.0f;
            b = 1.0f;
            break;
        }
    }

    std::vector<TipLine> ItemTooltip::parse_markup(std::string_view markup) {
        std::vector<TipLine> lines;
        TipLine current;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        std::string buf;

        auto flush = [&]() {
            if (!buf.empty()) {
                current.add(std::move(buf), r, g, b);
                buf.clear();
            }
        };

        for (std::size_t i = 0; i < markup.size(); ++i) {
            if (markup[i] == '#' && i + 1 < markup.size()) {
                flush();
                const char code = markup[i + 1];
                ++i;
                if (code == 'n' || code == 'N') {
                    lines.push_back(std::move(current));
                    current = TipLine{};
                    continue;
                }
                if (code == ' ') {
                    continue;
                }
                color_for_digit(code, r, g, b);
                continue;
            }
            buf.push_back(markup[i]);
        }
        flush();
        if (!current.spans().empty() || lines.empty()) {
            lines.push_back(std::move(current));
        }
        return lines;
    }

    std::string ItemTooltip::make_tip(const ItemDef& def,
                                      const ItemDb& catalog,
                                      int count,
                                      int player_clazz,
                                      int player_level,
                                      int instance_prefix,
                                      int cur_endurance,
                                      int max_endurance,
                                      ItemTipExtras extras) {
        std::ostringstream s;

        const PrefixDef* prefix =
            instance_prefix > 0 ? catalog.prefixes().find(instance_prefix) : nullptr;
        const bool enchanted = has_charm(extras);
        const bool has_prefix = prefix != nullptr && !prefix->name().empty();
        const char* scolor =
            tip_name_color(enchanted, has_prefix, extras.upgrade_level, def.rarity());

        s << scolor;
        append_charm_name(s, def, catalog, extras);
        if (has_prefix) {
            s << prefix->name() << " ";
        }
        if (!extras.egg_name.empty()) {
            s << extras.egg_name;
        } else {
            s << def.name();
        }
        if (def.is_weapon() && extras.upgrade_level > 0) {
            s << "[" << extras.upgrade_level << "]";
        }
        if (extras.character_bound) {
            s << kEndl << kDarkPurple << "Character Bound" << kWhite;
        } else if (extras.account_bound) {
            s << kEndl << kDarkPurple << "Account Bound" << kWhite;
        }

        if (extras.enchant_protected) {
            s << kEndl << kWhite << "Enchantment protected: 1 attempt";
        }

        const int limit_level = def.level_limit();
        if (limit_level != 0) {
            const bool ok = (player_level >= limit_level) &&
                            (def.class_limit() < 0 || def.class_limit() == player_clazz);
            s << (ok ? kWhite : kRed);
            if (def.class_limit() >= 0) {
                int class_msg = 23;
                switch (def.class_limit()) {
                case 1:
                    class_msg = 24;
                    break;
                case 2:
                    class_msg = 25;
                    break;
                case 3:
                    class_msg = 2000;
                    break;
                default:
                    class_msg = 23;
                    break;
                }
                // sys 376 "Limitation: %1 Level %2!d!"
                s << kEndl << "Limitation: " << catalog.sys(class_msg) << " Level " << limit_level;
            } else {
                // sys 676 "Limitation : level %1!d! or above"
                s << kEndl << "Limitation : level " << limit_level << " or above";
            }
        }

        s << kWhite;

        if (def.is_coin()) {
            if (count >= 1) {
                s << kEndl << comma_int(count) << " " << catalog.sys(111);
            }
            {
                const std::string desc = tip_description(def);
                if (!desc.empty()) {
                    s << kEndl << kWhite << desc;
                }
            }
            append_sell_price(s, def, catalog, prefix, extras);
            return s.str();
        }

        if (is_stack_tip_class(def) && def.class_small() != "fish") {
            if (def.grade() > 0) {
                s << kEndl << "[ " << catalog.sys(75) << " " << def.grade() << " ]";
            }
            if (count > 1) {
                s << kEndl << comma_int(count);
            }
            if (def.has_changeprefix()) {
                append_changeprefix_block(s, def, catalog);
            }
            {
                const std::string desc = tip_description(def);
                if (!desc.empty()) {
                    s << kEndl << kWhite << desc;
                }
            }
            append_sell_price(s, def, catalog, prefix, extras);
            return s.str();
        }

        if (def.class_big() == "transform") {
            {
                const std::string desc = tip_description(def);
                if (!desc.empty()) {
                    s << kEndl << kWhite << desc;
                }
            }
            append_transform_stats(s, catalog, def, player_level, extras);
            append_sell_price(s, def, catalog, prefix, extras);
            return s.str();
        }

        if (!def.is_quest() && def.class_small() != "fish") {
            if (def.grade() > 0) {
                s << kEndl << "[ " << catalog.sys(75) << " " << def.grade() << " ]" << kEndl;
            }

            if (is_gear_tip_class(def) && !def.is_ornament()) {
                if (def.grade() <= 0) {
                    s << kEndl;
                }
                const int max_end = max_endurance >= 0 ? max_endurance : def.endurance();
                const int cur_end = cur_endurance >= 0 ? cur_endurance : max_end;
                append_endurance(s, catalog, cur_end, max_end);
            } else if (is_gear_tip_class(def) && def.grade() <= 0) {
                s << kEndl;
            }

            const std::string& small = def.class_small();
            if (def.is_weapon()) {
                append_weapon_stats(s, catalog, def, extras, scolor);
            } else if (def.is_defense()) {
                if (small == "shield") {
                    append_shield_stats(s, catalog, def, extras, scolor);
                } else {
                    append_defence_stats(s, catalog, def, extras, scolor);
                }
            } else if (small == "trinket" || small == "ring" || small == "necklace" ||
                       small.find("resistnecklace") != std::string::npos) {
                append_ornament_stats(s, catalog, def, extras, scolor);
            }

            const auto collection = server::collection_def(def.index());
            if (collection.kind != server::CollectionKind::None) {
                if (extras.upgrade_level > 0) {
                    s << kEndl << "Enchantment: +" << extras.upgrade_level;
                }
                const char* ability =
                    server::collection_option_name(static_cast<int>(extras.random_stat_type));
                if (*ability && extras.random_stat_value > 0) {
                    s << kEndl << ability << ": +" << static_cast<int>(extras.random_stat_value);
                }
                if (collection.kind == server::CollectionKind::Taegeuk && extras.mix_type > 0) {
                    s << kEndl << "Trigram absorption: active";
                }
                s << kEndl << "PvP: only movement speed applies.";
                if (collection.kind == server::CollectionKind::YeoijuAttack ||
                    collection.kind == server::CollectionKind::YeoijuDefense) {
                    s << kEndl << "Yeoiju combat bonuses are disabled.";
                }
                if (collection.kind == server::CollectionKind::Trigram) {
                    s << kEndl << "Upgrade: drag the matching raw trigram here.";
                } else if (collection.kind == server::CollectionKind::Essence &&
                           collection.grade < 3) {
                    s << kEndl << "Upgrade: drag a same-grade essence here.";
                } else if (collection.kind == server::CollectionKind::Taegeuk) {
                    s << kEndl << "Select in bag, then click Absorb in the drawer.";
                } else if (collection.kind == server::CollectionKind::YeoijuAttack ||
                           collection.kind == server::CollectionKind::YeoijuDefense) {
                    s << kEndl << "Upgrade: matching Yeoiju + fortification material.";
                }
            }
            if (is_gear_tip_class(def)) {
                append_random_stat(
                    s, catalog, def, extras.random_stat_type, extras.random_stat_value);
            }

            if (is_gear_tip_class(def) && has_prefix) {
                s << (extras.upgrade_level > 0 ? kBlue : kGreen);
                append_prefix_stats(s, catalog, *prefix);
                s << kWhite;
            }
            append_bof(s, def, catalog, extras);
            append_qigong(s, def, extras);
            append_explosive_blow(s, def, catalog, extras);
            append_mix(s, def, catalog, extras.mix_type);
            append_dragon_fusing(s, def, extras);
        }

        if (def.is_pet()) {
            bool has_stats = false;
            const auto append_pet_stat = [&s, &has_stats](const char* label, int value) {
                if (value == 0) {
                    return;
                }
                if (has_stats) {
                    s << ", ";
                } else {
                    s << kEndl << kGreen << "Pet stats: ";
                }
                s << label << (value > 0 ? "+" : "") << value;
                has_stats = true;
            };
            append_pet_stat("ATK ", def.min_attack());
            append_pet_stat("MAG ", def.min_magic());
            append_pet_stat("DEF ", def.defense());
            append_pet_stat("HP ", def.hp());
            append_pet_stat("MP ", def.mp());
            if (extras.pet_heal_per_two_seconds > 0) {
                s << kEndl << kGreen << "Pet ability: Heal " << extras.pet_heal_per_two_seconds
                  << " HP / 2 sec";
            }
            if (extras.pet_auto_pickup) {
                s << kEndl << kGreen << "Pet ability: Auto Pickup";
            }
        }

        if (def.is_pet() || def.is_ride() ||
            server::collection_def(def.index()).kind == server::CollectionKind::Mirror) {
            if (extras.lifetime_expiration_known && extras.lifetime_remaining_seconds > 0) {
                s << kEndl << kYellow
                  << "Time remaining: " << extras.lifetime_remaining_seconds / 86400ull << "d "
                  << (extras.lifetime_remaining_seconds % 86400ull) / 3600ull << "h";
            } else if (extras.lifetime_expiration_known) {
                s << kEndl << kYellow << "Expired";
            } else if (extras.lifetime_duration_seconds > 0) {
                s << kEndl << kYellow << "Duration: " << extras.lifetime_duration_seconds / 86400ull
                  << "d";
            } else {
                s << kEndl << kYellow << "Duration: Permanent";
            }
            s << kWhite;
        }

        {
            const std::string desc = tip_description(def);
            if (!desc.empty()) {
                s << kEndl << kWhite << desc;
            }
        }

        append_sell_price(s, def, catalog, prefix, extras);
        return s.str();
    }

    void ItemTooltip::place_near_cursor(float cursor_x,
                                        float cursor_y,
                                        float box_w,
                                        float box_h,
                                        float screen_w,
                                        float screen_h,
                                        float& tip_x,
                                        float& tip_y,
                                        float scale) {
        // Soft cursor is drawn under the overlay; a small gap keeps the tip readable.
        const float s = std::max(.1f, scale);
        const float cursor_size = 14.0f * s;
        const float gap = 6.0f * s;

        tip_x = cursor_x + gap;
        tip_y = cursor_y + cursor_size + gap;
        if (tip_y + box_h > screen_h) {
            tip_y = cursor_y - box_h - gap;
        }
        if (tip_x + box_w > screen_w) {
            tip_x = cursor_x - box_w - gap;
        }
        tip_x = std::min(std::max(0.0f, tip_x), std::max(0.0f, screen_w - box_w));
        tip_y = std::min(std::max(0.0f, tip_y), std::max(0.0f, screen_h - box_h));
    }

    void ItemTooltip::draw(WidgetTools& ui,
                           engine::IEngine& engine,
                           const FreetypeFont& font,
                           const std::vector<TipLine>& lines,
                           float cursor_x,
                           float cursor_y,
                           float screen_w,
                           float screen_h,
                           float scale) {
        if (!font.ready() || lines.empty()) {
            return;
        }

        const float s = std::max(.1f, scale);
        const float border = WidgetTools::overlay_border_thickness(s);
        const float pad = 5.0f * s + border;
        const float line_offset = 2.0f * s;
        const float line_h = font.line_height() * s + line_offset;

        float max_line_w = 0.0f;
        for (const TipLine& line : lines) {
            float w = 0.0f;
            for (const TipSpan& span : line.spans()) {
                w += font.text_width(span.text(), s);
            }
            max_line_w = std::max(max_line_w, w);
        }

        const float content_w = max_line_w;
        const float box_w = content_w + pad * 2.0f;
        const float box_h = pad * 2.0f + line_h * static_cast<float>(lines.size());

        float tip_x = 0.0f;
        float tip_y = 0.0f;
        place_near_cursor(cursor_x, cursor_y, box_w, box_h, screen_w, screen_h, tip_x, tip_y, s);

        ui.draw_overlay_panel(tip_x, tip_y, box_w, box_h, s);

        float y = tip_y + pad;
        for (const TipLine& line : lines) {
            float line_w = 0.0f;
            for (const TipSpan& span : line.spans()) {
                line_w += font.text_width(span.text(), s);
            }
            float x = tip_x + pad;
            if (line_w < content_w) {
                x += (content_w - line_w) * 0.5f;
            }
            for (const TipSpan& span : line.spans()) {
                font.draw(engine, x, y, span.text(), span.r(), span.g(), span.b(), s);
                x += font.text_width(span.text(), s);
            }
            y += line_h;
        }
    }

}  // namespace openkal::client
