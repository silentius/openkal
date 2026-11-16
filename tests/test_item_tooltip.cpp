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
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/MixVisual.h"
#include "openkal/platform/Path.h"
#include "test_util.h"

#include <string>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::test::expect;

    openkal::client::ItemDb catalog;
    expect(catalog.load(openkal::platform::join_path(OPENKAL_TEST_RESOURCE_ROOT, "config"),
                        OPENKAL_TEST_RESOURCE_ROOT),
           "item catalog loads");
    expect(catalog.prefixes().find(8) != nullptr, "prefix 8 loaded");
    expect(catalog.prefixes().name(8) == "Slaughterous", "prefix 8 name");
    expect(catalog.prefixes().name(2) == "Unlawful", "prefix 2 name");

    {
        const openkal::client::ItemDef* medicine = catalog.find(447);
        expect(medicine != nullptr, "Speed-up Medicine exists");
        if (medicine) {
            expect(medicine->is_consumable(), "Speed-up Medicine is consumable");
            expect(medicine->buff_type() == 12 && medicine->buff_remain() == 1800 &&
                       medicine->buff_amount() == 65,
                   "Speed-up Medicine keeps the compatible client Haste payload");
        }
    }

    {
        const openkal::client::ItemDef* talisman = catalog.find(280);
        expect(talisman != nullptr, "talisman 280 exists");
        if (talisman) {
            expect(talisman->has_changeprefix(), "talisman has changeprefix");
            expect(talisman->changeprefix_target() == "weapon", "talisman targets weapons");
            expect(talisman->changeprefix_rate0() == 16, "rate0 16");
            expect(talisman->changeprefix_prefix0() == 8, "prefix0 slaughterous");
            expect(talisman->changeprefix_rate1() == 55, "rate1 55 cumulative");
            expect(talisman->changeprefix_prefix1() == 2, "prefix1 unlawful");

            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*talisman, catalog, 1, 0, 1);
            expect(tip.find("Effective on: Weapon") != std::string::npos, "weapon target line");
            expect(tip.find("Slaughterous") != std::string::npos, "prefix A name");
            expect(tip.find("Unlawful") != std::string::npos, "prefix B name");
            expect(tip.find("Percentage of Success: 16 %") != std::string::npos,
                   "prefix A shown rate is rate0");
            expect(tip.find("Percentage of Success: 39 %") != std::string::npos,
                   "prefix B shown rate is rate1-rate0");
            expect(tip.find("Percentage of Success: 55 %") == std::string::npos,
                   "raw cumulative rate1 is not shown");
            expect(tip.find("#4") != std::string::npos, "prefix stats are green");
            expect(tip.find("Strength 8") != std::string::npos, "slaughterous str 8");
            expect(tip.find("Strength 2") != std::string::npos, "unlawful str 2");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(100);
        expect(sword != nullptr, "red hwandoo exists");
        if (sword) {
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1);
            expect(tip.find("Red HwanDoo Big Sword") != std::string::npos, "sword name");
            expect(tip.find("Limitation: Knight Level 1") != std::string::npos, "class limit");
            expect(tip.find("[ Grade 40 ]") != std::string::npos, "grade 40");
            expect(tip.find("Endurance 36 / 36") != std::string::npos, "endurance from def");
            expect(tip.find("STATS") == std::string::npos, "no STATS header");
            expect(tip.find("PREFIX") == std::string::npos, "no PREFIX header without enchant");
            expect(tip.find("Physical Attack") != std::string::npos, "weapon stats present");
            expect(sword->sell() == 52000, "sword sell from json");
            expect(tip.find("#8") != std::string::npos, "sell price is white");
            expect(tip.find("Price when selling : 52,000") != std::string::npos,
                   "sell price at bottom of tip");

            openkal::client::ItemTipExtras extras;
            extras.random_stat_type =
                openkal::protocol::RandomItemStatType::WeaponPhysicalDamagePercent;
            extras.random_stat_value = 5;
            const std::string random_tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1, 8, -1, -1, extras);
            const std::size_t random_pos = random_tip.find("#8Min. and Max. Physical Damage +5%");
            const std::size_t prefix_pos = random_tip.find("#4Strength 8");
            expect(random_pos != std::string::npos, "random weapon stat is white");
            expect(prefix_pos != std::string::npos && random_pos < prefix_pos,
                   "random weapon stat precedes prefix stats");

            extras.random_stat_type = openkal::protocol::RandomItemStatType::ArmorDefensePercent;
            const std::string mismatched_random_tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1, 8, -1, -1, extras);
            expect(mismatched_random_tip.find("Defense +5%") == std::string::npos,
                   "weapons hide armor-only random stats");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(100);
        expect(sword != nullptr, "red hwandoo for Qigong tier colors");
        if (sword) {
            openkal::client::ItemTipExtras tier_one;
            tier_one.qigong_perforated = 1;
            tier_one.qigong_effect = 1;
            const std::string tier_one_tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 1, 0, -1, -1, tier_one);
            expect(tier_one_tip.find("#aBlack Magic: Black Imoogi") != std::string::npos,
                   "Qigong tier one is dark gray");

            openkal::client::ItemTipExtras tier_eight;
            tier_eight.qigong_perforated = 1;
            tier_eight.qigong_effect = 80;
            const std::string tier_eight_tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 1, 0, -1, -1, tier_eight);
            expect(tier_eight_tip.find("#7Black Magic: Black Insanity Dragon") != std::string::npos,
                   "Qigong tier eight is purple");

            openkal::client::ItemTipExtras dragon_tier_one;
            dragon_tier_one.dragon_effect = 1;
            const std::string dragon_tier_one_tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 1, 0, -1, -1, dragon_tier_one);
            expect(dragon_tier_one_tip.find("#aDragon Fusing: Imoogi") != std::string::npos,
                   "Dragon Fusing tier one is dark gray");

            openkal::client::ItemTipExtras dragon_tier_eight;
            dragon_tier_eight.dragon_effect = 237;
            const std::string dragon_tier_eight_tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 1, 0, -1, -1, dragon_tier_eight);
            expect(dragon_tier_eight_tip.find("#7Dragon Fusing: Insanity Dragon") !=
                       std::string::npos,
                   "Dragon Fusing tier eight is purple");
        }
    }

    {
        const openkal::client::ItemDef* ring = catalog.find(207);
        expect(ring != nullptr && ring->is_ornament(), "plain ring is ornament");
        if (ring) {
            expect(ring->endurance() == 0, "ring def has no endurance");
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*ring, catalog, 1, 0, 1, 0, 10, 10);
            expect(tip.find("Endurance") == std::string::npos, "ring tip hides leftover endurance");
            expect(ring->sell() == 0, "plain ring sell is 0");
            expect(tip.find("Price when selling") == std::string::npos,
                   "zero sell is omitted from tip");
        }
        const openkal::client::ItemDef* belt = catalog.find(205);
        expect(belt != nullptr && belt->is_ornament() && belt->class_small() == "trinket",
               "belt/trinket is ornament");
        if (belt) {
            expect(belt->endurance() == 0, "trinket def has no endurance");
            const std::string tip = openkal::client::ItemTooltip::make_tip(*belt, catalog, 1, 0, 1);
            expect(tip.find("Endurance") == std::string::npos, "trinket tip has no endurance");
        }
        const openkal::client::ItemDef* apprentice = catalog.find(203);
        expect(apprentice != nullptr && apprentice->sell() == 3000, "apprentice trinket sell");
        if (apprentice) {
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*apprentice, catalog, 1, 0, 1);
            expect(tip.find("Price when selling : 3,000") != std::string::npos,
                   "ornament sell price at bottom");
        }
        const openkal::client::ItemDef* necklace = catalog.find(1040);
        expect(necklace != nullptr && necklace->is_ornament(), "necklace is ornament");
        if (necklace) {
            expect(necklace->endurance() == 0, "necklace def has no endurance");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(100);
        if (sword) {
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1, 8);
            expect(tip.find("Slaughterous Red HwanDoo Big Sword") != std::string::npos,
                   "instance prefix in name");
            expect(tip.find("#4") != std::string::npos, "enchanted prefix stats are green");
            expect(tip.find("Strength 8") != std::string::npos, "instance prefix stats");
            expect(tip.find("PREFIX") == std::string::npos, "no PREFIX label when enchanted");
            expect(tip.find("Price when selling : 72,800") != std::string::npos,
                   "prefix sell is base plus percent");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(100);
        expect(sword != nullptr, "red hwandoo for charm name");
        if (sword) {
            openkal::client::ItemTipExtras extras;
            extras.x_attack = 12;
            extras.x_hit = 1;
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1, 0, -1, -1, extras);
            expect(tip.find("+12 Strengthened Red HwanDoo Big Sword") != std::string::npos,
                   "charm plus sits in the name");
            expect(tip.find("+12 Attack") == std::string::npos, "no leftover +Attack line");
            expect(tip.find("(+41)") != std::string::npos, "charm attack bonus on physical range");
            expect(tip.find("(+1)") != std::string::npos, "charm OTP bonus");
            expect(tip.find("#3") != std::string::npos, "charmed name without prefix is yellow");
            expect(tip.find("Price when selling : 225,000") != std::string::npos,
                   "charm cubes add to sell");
            expect(tip.find("#4Price when selling : 225,000#8") != std::string::npos,
                   "inventory sell price uses the green Geon tier and resets its text color");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(100);
        if (sword) {
            openkal::client::ItemTipExtras extras;
            extras.x_attack = 12;
            extras.upgrade_level = 11;
            const std::string tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 1, 90, -1, -1, extras);
            expect(tip.find("+12 Strengthened Legendary Red HwanDoo Big Sword[11]") !=
                       std::string::npos,
                   "prefix and EB suffix in the name");
            expect(tip.find("Explosive Blow of Weapon [11]") != std::string::npos, "sys 852");
            expect(tip.find("Percentage of Occurrence 15 %") != std::string::npos,
                   "sys 853 at EB 11");
            expect(tip.find("Damage 237 ~ 509") != std::string::npos, "sys 854 weapon EB range");
            expect(tip.find("----------------------") != std::string::npos, "red EB rule");
            expect(tip.find("#6") != std::string::npos, "EB name and prefix stats are blue");
            expect(tip.find("Explosive Blow 11") == std::string::npos, "no leftover EB line");
        }
    }

    {
        expect(catalog.prefixes().name(92) == "The King, GuhBalHan's", "prefix 92 name");
        const openkal::client::ItemDef* sword = catalog.find(101);
        expect(sword != nullptr, "white hwandoo exists");
        if (sword) {
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*sword, catalog, 1, 0, 1, 92);
            expect(tip.find("The King, GuhBalHan's White HwanDoo Big Sword") != std::string::npos,
                   "king prefix on white hwandoo");
        }
    }

    {
        const openkal::client::ItemDef* sword = catalog.find(778);
        expect(sword != nullptr, "imperial sword 778 exists");
        if (sword) {
            openkal::client::ItemTipExtras extras;
            extras.mix_type = (2 << 4) | 1;  // Stone of Shadow mix level 2
            const std::string tip = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 50, 0, -1, -1, extras);
            expect(tip.find("Mixing : Stone of Shadow") != std::string::npos,
                   "mix header sys 1278+1279");
            expect(tip.find("HP+50/Strength+3/Intelligence+3/Agility+3") != std::string::npos,
                   "shadow mix level 2 stats sys 1302");
            expect(tip.find("Price when selling : 117,000") != std::string::npos,
                   "mixing does not change sell");
            extras.mix_type = (1 << 4) | 1;
            const std::string tip_l1 = openkal::client::ItemTooltip::make_tip(
                *sword, catalog, 1, 0, 50, 0, -1, -1, extras);
            expect(tip_l1.find("HP+20/Strength+2/Intelligence+2/Agility+2") != std::string::npos,
                   "shadow mix level 1 stats sys 1292");
        }
    }

    {
        const openkal::client::ItemDef* money = catalog.find(31);
        expect(money != nullptr && money->is_coin(), "zamogeon is coin");
        expect(money != nullptr && money->rarity() == 0, "zamogeon json rarity is 0");
        if (money) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            money->glow_color(r, g, b);
            expect(r > 0.90f && g > 0.70f && b < 0.30f, "geon drop glow is gold");
        }

        const openkal::client::ItemDef* sword = catalog.find(100);
        expect(sword != nullptr && sword->rarity() == 0, "catalog items default rarity 0");
        if (sword) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            sword->glow_color(r, g, b);
            expect(r > 0.90f && g > 0.90f && b > 0.90f, "rarity 0 drop glow is white");
        }

        openkal::client::ItemDef band;
        band.set_rarity(9);
        expect(band.rarity() == 5, "rarity clamps to 5");
        band.set_rarity(-3);
        expect(band.rarity() == 0, "rarity clamps to 0");
        band.set_rarity(2);
        float r = 0.0f;
        float g = 0.0f;
        float b = 1.0f;
        band.glow_color(r, g, b);
        expect(g > r && g > b, "rarity 2 drop glow is green");
    }

    {
        const openkal::client::ItemDef* bead = catalog.find(1029);
        expect(bead != nullptr && bead->has_bof(), "bead 1029 specialty.bof");
        const openkal::client::ItemDef* bead_alt = catalog.find(1165);
        expect(bead_alt != nullptr && bead_alt->has_bof(), "bead 1165 specialty.bof");
        expect(catalog.bof().ready(), "tooltip catalog loads bof.json");

        const openkal::client::ItemDef* boots = catalog.find(1593);
        expect(boots != nullptr && boots->grade() == 60 && boots->class_limit() == 3,
               "G60 thief boots");
        if (boots) {
            openkal::client::ItemTipExtras extras;
            extras.bof = 1;
            const std::string tip = openkal::client::ItemTooltip::make_tip(
                *boots, catalog, 1, 3, 60, 0, -1, -1, extras);
            expect(tip.find("----------------------") != std::string::npos, "BOF dashed rule");
            expect(tip.find("#2") != std::string::npos, "BOF block is orange");
            expect(tip.find(" Equip : Bead of Fire ") != std::string::npos, "sys 1324");
            expect(tip.find("Strength 4") != std::string::npos, "G60 piece STR");
            expect(tip.find("Health 4") != std::string::npos, "G60 piece HTH");
            expect(tip.find("Agility 4") != std::string::npos, "thief third stat DEX");
            expect(tip.find("Intelligence 4") == std::string::npos, "thief boots are not INT");

            extras.bof = 0;
            const std::string plain =
                openkal::client::ItemTooltip::make_tip(*boots, catalog, 1, 3, 60);
            expect(plain.find(" Equip : Bead of Fire ") == std::string::npos,
                   "no BOF block without flag");
            expect(plain.find("Hit") == std::string::npos, "armor does not show hit");

            openkal::client::ItemTipExtras armor_extras;
            armor_extras.random_stat_type =
                openkal::protocol::RandomItemStatType::WeaponPhysicalDamagePercent;
            armor_extras.random_stat_value = 5;
            const std::string mismatched_random_tip = openkal::client::ItemTooltip::make_tip(
                *boots, catalog, 1, 3, 60, 0, -1, -1, armor_extras);
            expect(mismatched_random_tip.find("Min. and Max. Physical Damage +5%") ==
                       std::string::npos,
                   "armor hides weapon-only random stats");
        }

        const openkal::client::ItemDef* helm = catalog.find(1135);
        expect(helm != nullptr && helm->grade() == 60 && helm->class_limit() == 1,
               "G60 mage helmet");
        if (helm) {
            openkal::client::ItemTipExtras extras;
            extras.bof = 1;
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*helm, catalog, 1, 1, 60, 8, -1, -1, extras);
            expect(tip.find("Slaughterous") != std::string::npos, "prefix coexists with BOF");
            expect(tip.find(" Equip : Bead of Fire ") != std::string::npos, "BOF under prefix");
            expect(tip.find("Intelligence 4") != std::string::npos, "mage third stat INT");
            expect(tip.find("Agility 4") == std::string::npos, "mage helmet is not DEX");
        }
    }

    {
        const openkal::client::ItemDef* tiger = catalog.find(337);
        expect(tiger != nullptr && tiger->is_transform(), "ancient tiger egg");
        if (tiger) {
            expect(tiger->cocoon_ability(0) == 2, "tiger STR A=2");
            expect(tiger->cocoon_ability(1) == 4, "tiger HTH C=4");
            expect(tiger->cocoon_ability(2) == 6, "tiger DEX E=6");
            expect(tiger->cocoon_ability(3) == 0, "tiger INT F hidden");
            expect(tiger->cocoon_ability(4) == 0, "tiger WIS F hidden");
            const std::string tip =
                openkal::client::ItemTooltip::make_tip(*tiger, catalog, 1, 0, 15);
            expect(tip.find("Grade 1") != std::string::npos, "egg grade at level 1");
            expect(tip.find("Exp 0.00%") != std::string::npos, "egg exp percent");
            expect(tip.find("Strength 1") != std::string::npos, "tiger STR at lv1");
            expect(tip.find("Health 1") != std::string::npos, "tiger HTH at lv1");
            expect(tip.find("Agility 1") != std::string::npos, "tiger DEX at lv1");
            expect(tip.find("Intelligence") == std::string::npos, "tiger hides INT");
            expect(tip.find("Wisdom") == std::string::npos, "tiger hides WIS");
            openkal::client::ItemTipExtras extras;
            extras.egg_level = 16;
            extras.egg_exp = 0;
            const std::string grown = openkal::client::ItemTooltip::make_tip(
                *tiger, catalog, 1, 0, 20, 0, -1, -1, extras);
            expect(grown.find("Grade 16") != std::string::npos, "egg grade follows instance");
            expect(grown.find("Strength 9") != std::string::npos, "tiger STR 1+16/2");
            expect(grown.find("Health 5") != std::string::npos, "tiger HTH 1+16/4");
            expect(grown.find("Agility 3") != std::string::npos, "tiger DEX 1+16/6");
            expect(tiger->image() == "item078", "tiger base icon");
            expect(tiger->image_ex(0) == "item078_01", "tiger icon at 11");
            expect(tiger->image_ex(1) == "item078_02", "tiger icon at 21");
            expect(tiger->image_ex(2) == "item078_03", "tiger icon at 31");
            extras.egg_level = 5;
            extras.egg_exp = 1234;
            extras.egg_name = "Ancient Tiger";
            const std::string bag = openkal::client::ItemTooltip::make_tip(
                *tiger, catalog, 1, 0, 15, 0, -1, -1, extras);
            expect(bag.find("Ancient Tiger") != std::string::npos, "unequipped egg keeps name");
            expect(bag.find("Grade 5") != std::string::npos, "unequipped egg keeps level");
            extras.egg_name = "Ancient Baby Tiger";
            extras.egg_level = 11;
            const std::string baby = openkal::client::ItemTooltip::make_tip(
                *tiger, catalog, 1, 0, 15, 0, -1, -1, extras);
            expect(baby.find("Ancient Baby Tiger") != std::string::npos, "egg name at 11");
            extras.egg_name = "Ancient Young Tiger";
            extras.egg_level = 21;
            const std::string young = openkal::client::ItemTooltip::make_tip(
                *tiger, catalog, 1, 0, 21, 0, -1, -1, extras);
            expect(young.find("Ancient Young Tiger") != std::string::npos, "egg name at 21");
            extras.egg_name = "Ancient Perfect Tiger";
            extras.egg_level = 31;
            const std::string adult = openkal::client::ItemTooltip::make_tip(
                *tiger, catalog, 1, 0, 31, 0, -1, -1, extras);
            expect(adult.find("Ancient Perfect Tiger") != std::string::npos, "egg name at 31");
        }
    }

    {
        const openkal::client::ItemDef* blue_dragon = catalog.find(1747);
        expect(blue_dragon != nullptr && blue_dragon->is_pet(), "Blue Dragon is a pet");
        if (blue_dragon) {
            openkal::client::ItemTipExtras extras;
            extras.lifetime_duration_seconds = 7 * 24 * 60 * 60;
            const std::string tip = openkal::client::ItemTooltip::make_tip(
                *blue_dragon, catalog, 1, 0, 1, 0, -1, -1, extras);
            expect(tip.find("Duration: 7d") != std::string::npos,
                   "timed pets show their configured duration");
            expect(tip.find("Duration: Permanent") == std::string::npos,
                   "timed pets are not labelled permanent");

            extras.lifetime_expiration_known = true;
            extras.lifetime_remaining_seconds = 6 * 24 * 60 * 60 + 5 * 60 * 60;
            const std::string remaining_tip = openkal::client::ItemTooltip::make_tip(
                *blue_dragon, catalog, 1, 0, 1, 0, -1, -1, extras);
            expect(remaining_tip.find("Time remaining: 6d 5h") != std::string::npos,
                   "inventory pets show their authoritative remaining lifetime");
        }
    }

    {
        for (const int index : {30010, 30100}) {
            const openkal::client::ItemDef* riding = catalog.find(index);
            expect(riding != nullptr && riding->is_ride(),
                   "regular and battle riding items are riding items");
            if (!riding) {
                continue;
            }
            openkal::client::ItemTipExtras extras;
            extras.lifetime_expiration_known = true;
            extras.lifetime_remaining_seconds = 3 * 24 * 60 * 60 + 7 * 60 * 60;
            const std::string tip = openkal::client::ItemTooltip::make_tip(
                *riding, catalog, 1, 0, 1, 0, -1, -1, extras);
            expect(tip.find("Time remaining: 3d 7h") != std::string::npos,
                   "regular and battle riding items show their remaining lifetime");
        }
    }

    {
        expect(openkal::client::MixVisual::magic_stone(0x11) == 1,
               "encoded level-one mix selects first particle");
        expect(openkal::client::MixVisual::magic_stone(1) == 1,
               "legacy level-one mix selects first particle");
        expect(openkal::client::MixVisual::magic_stone(0x21) == 11,
               "level-two mix selects second particle");
        expect(openkal::client::MixVisual::magic_stone(0x2b) == 1034,
               "level-two blood mix selects ew_1034");
    }

    {
        expect(catalog.sys(80) == "Absorption Rate" && catalog.sys(81) == "Fire Resistance" &&
                   catalog.sys(85) == "Non Elemental Resistance",
               "armor resist labels are not color codes");
        expect(catalog.sys(80).find('#') == std::string::npos, "absorb label has no markup hash");

        openkal::client::ItemDef armor;
        armor.set_name("Test Armor");
        armor.set_class_big("defense");
        armor.set_class_small("upperarmor");
        armor.set_grade(8);
        armor.set_endurance(5);
        armor.set_defense(12);
        armor.set_dodge(1);
        const std::string tip = openkal::client::ItemTooltip::make_tip(armor, catalog, 1, 0, 1);
        expect(tip.find("Defense 12") != std::string::npos, "armor defense");
        expect(tip.find("Absorption Rate 0") != std::string::npos, "zero absorb keeps its name");
        expect(tip.find("Fire Resistance 0 Ice Resistance 0") != std::string::npos,
               "paired fire and ice resists");
        expect(tip.find("Lightning Resistance 0 Curse Resistance 0") != std::string::npos,
               "paired lightning and curse resists");
        expect(tip.find("Non Elemental Resistance 0") != std::string::npos, "palsy resist label");
        const auto lines = openkal::client::ItemTooltip::parse_markup(tip);
        bool garbled = false;
        for (const auto& line : lines) {
            std::string text;
            for (const auto& span : line.spans()) {
                text += span.text();
            }
            if (text == "0 0" || text == "1 0 2 0" || text == "3 0 4 0" || text == "5 0") {
                garbled = true;
            }
        }
        expect(!garbled, "zero resists are not shown as bare id digits");
    }

    if (openkal::test::g_failures != 0) {
        return 1;
    }
    return 0;
}
