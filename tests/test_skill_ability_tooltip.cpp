/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/Skills.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/MageSkill.h"
#include "test_util.h"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

int main() {
    using openkal::client::SkillDef;
    using openkal::client::SkillExpr;
    using openkal::client::SkillParameter;
    using openkal::test::expect;

    expect(openkal::client::Catalog::cast_kind_for(3, 6) == openkal::client::CastKind::SelfHeal,
           "Thief Hiding is a self cast");
    expect(openkal::client::Catalog::cast_kind_for(3, 19) == openkal::client::CastKind::SelfArea,
           "Spin Attack is centered on the caster");
    expect(openkal::client::Catalog::cast_kind_for(3, 29) == openkal::client::CastKind::Party,
           "Wind Cry is sent as a party cast");
    expect(openkal::client::Catalog::cast_kind_for(3, 32) == openkal::client::CastKind::TargetArea,
           "Shadow Slash uses a hostile target area");
    expect(openkal::client::Catalog::cast_kind_for(3, 90) == openkal::client::CastKind::None,
           "Awaken Thief skills are not routed through the legacy handler");

    SkillDef mastery;
    mastery.set_name("Weapon Mastery");
    mastery.set_ability("#nOn-target Poin + %1!d! Min/Max Attack + %2!d!");
    {
        // P1: skill_level/2 + skill_level/21
        SkillParameter p1;
        p1.id = 1;
        p1.expr = SkillExpr::make_call(
            "plus",
            {SkillExpr::make_call(
                 "div", {SkillExpr::make_symbol("skill_level"), SkillExpr::make_number(2)}),
             SkillExpr::make_call(
                 "div", {SkillExpr::make_symbol("skill_level"), SkillExpr::make_number(21)})});
        // P2: 1 + skill_level/2 + skill_level/21
        SkillParameter p2;
        p2.id = 2;
        p2.expr = SkillExpr::make_call(
            "plus",
            {SkillExpr::make_call(
                 "plus",
                 {SkillExpr::make_number(1),
                  SkillExpr::make_call(
                      "div", {SkillExpr::make_symbol("skill_level"), SkillExpr::make_number(2)})}),
             SkillExpr::make_call(
                 "div", {SkillExpr::make_symbol("skill_level"), SkillExpr::make_number(21)})});
        mastery.set_parameters({p1, p2});
    }

    expect(mastery.eval_parameter(1, 10).value_or(-1) == 5, "P1 at lv10");
    expect(mastery.eval_parameter(2, 10).value_or(-1) == 6, "P2 at lv10");
    const std::string tip = mastery.format_ability(10);
    expect(tip.find("On-target Poin + 5") != std::string::npos, "ability P1 filled");
    expect(tip.find("Min/Max Attack + 6") != std::string::npos, "ability P2 filled");
    expect(tip.find("%") == std::string::npos, "no leftover placeholders");

    SkillDef char_dep;
    char_dep.set_ability("Bonus + %1!d!");
    SkillParameter pc;
    pc.id = 1;
    pc.expr = SkillExpr::make_symbol("char_str");
    char_dep.set_parameters({pc});
    expect(char_dep.format_ability(1).find('?') != std::string::npos, "unknown symbol becomes ?");

    using openkal::client::SkillAbilityVars;
    SkillAbilityVars vars;
    vars.has_stats = true;
    vars.char_str = 42;
    expect(char_dep.eval_parameter(1, 1, vars).value_or(-1) == 42, "char_str fills from vars");
    expect(char_dep.format_ability(1, vars).find("Bonus + 42") != std::string::npos,
           "ability tooltip uses character stats");

    SkillDef ice;
    ice.set_ability("Magic Attack Point + %1!d!");
    SkillParameter pi;
    pi.id = 1;
    pi.expr = SkillExpr::make_call(
        "div",
        {SkillExpr::make_call(
             "plus",
             {SkillExpr::make_call(
                  "div",
                  {SkillExpr::make_call(
                       "mul", {SkillExpr::make_number(3), SkillExpr::make_symbol("char_level")}),
                   SkillExpr::make_number(2)}),
              SkillExpr::make_call(
                  "div",
                  {SkillExpr::make_call(
                       "mul",
                       {SkillExpr::make_symbol("char_int"),
                        SkillExpr::make_call(
                            "mul",
                            {SkillExpr::make_symbol("skill_level"), SkillExpr::make_number(3)})}),
                   SkillExpr::make_number(13)})}),
         SkillExpr::make_number(2)});
    ice.set_parameters({pi});
    SkillAbilityVars ice_vars;
    ice_vars.has_stats = true;
    ice_vars.char_level = 10;
    ice_vars.char_int = 18;
    expect(ice.eval_parameter(1, 1, ice_vars).value_or(-1) == 9, "Ice Magic tooltip at lv10 int18");
    expect(ice.format_ability(1, ice_vars).find('?') == std::string::npos,
           "Ice Magic tooltip has no ? when stats are set");

    SkillDef blessing;
    blessing.set_ability("#nPower is increased by 8 %%");
    expect(blessing.format_ability(1).find("8 %") != std::string::npos &&
               blessing.format_ability(1).find("% %") == std::string::npos &&
               blessing.format_ability(1).find("%%") == std::string::npos,
           "FormatMessage %% becomes a single percent");

    SkillDef shrewd;
    shrewd.set_ability("#nEvasion Point : %1!d!");
    SkillParameter ps;
    ps.id = 1;
    ps.expr = SkillExpr::make_call(
        "div",
        {SkillExpr::make_call(
             "mul",
             {SkillExpr::make_symbol("char_org_dodge"),
              SkillExpr::make_call(
                  "div", {SkillExpr::make_symbol("char_level"), SkillExpr::make_number(2)})}),
         SkillExpr::make_number(400)});
    shrewd.set_parameters({ps});
    SkillAbilityVars dodge_vars;
    dodge_vars.has_stats = true;
    dodge_vars.char_level = 70;
    dodge_vars.char_org_dodge = 200;
    expect(shrewd.eval_parameter(1, 1, dodge_vars).value_or(-1) == 17,
           "Shrewdness tooltip uses org dodge * (CL/2) / 400");
    expect(shrewd.format_ability(1, dodge_vars).find("Evasion Point : 17") != std::string::npos,
           "Shrewdness ability shows evasion points");

    using openkal::client::CastKind;
    using openkal::client::Catalog;
    expect(Catalog::cast_kind_for(1, 38) == CastKind::Player,
           "refining weapon uses player targeting");
    expect(Catalog::cast_kind_for(1, 16) == CastKind::DeadPlayer,
           "revival uses dead-player targeting");
    expect(Catalog::cast_kind_for(1, 28) == CastKind::Party, "group cure uses party targeting");
    expect(Catalog::cast_kind_for(1, 25) == CastKind::SelfArea,
           "fire blow uses caster-centered area");
    expect(Catalog::cast_kind_for(1, 41) == CastKind::TargetArea,
           "chain lightning uses target-centered area");
    expect(Catalog::cast_kind_for(1, 47) == CastKind::GroundPosition,
           "explosive burst uses ground coordinates");
    expect(Catalog::cast_kind_for(1, 12) == CastKind::None, "lightning mastery cannot be cast");

    expect(Catalog::cast_kind_for(0, 40) == CastKind::Party,
           "Knight Perfect Defense uses server-selected party targeting");
    {
        const auto* pd = openkal::client::find_skill_effect(0, 1, 40);
        expect(pd != nullptr && pd->effect == openkal::client::SkillEt::PerfectDefense,
               "Knight Perfect Defense fire maps to PerfectDefense");
        expect(openkal::client::kPerfectDefenseLoopFx == "davi_ef120",
               "Perfect Defense duration sphere uses Steel Wall FX davi_ef120");
        const auto* pob = openkal::client::find_skill_effect(0, 1, 42);
        expect(pob != nullptr && pob->effect == openkal::client::SkillEt::ProvocationOfBlow,
               "Knight Provocation of Blow fire maps to ProvocationOfBlow");
        const auto* spin = openkal::client::find_skill_effect(0, 1, 38);
        expect(spin != nullptr && spin->effect == openkal::client::SkillEt::SpinSlash,
               "Knight Spin Slash fire maps to SpinSlash");
        const auto* defense_chant = openkal::client::find_skill_effect(0, 1, 19);
        expect(defense_chant != nullptr &&
                   defense_chant->effect == openkal::client::SkillEt::ChantOfProtect,
               "Knight Call of Defense fire maps to ChantOfProtect");
        const auto* demon_fire = openkal::client::find_skill_effect(-2, 1, 33);
        expect(demon_fire != nullptr && demon_fire->effect == openkal::client::SkillEt::Fireball,
               "Demon Worker Explosive Fire uses the monster Fireball dispatch");
        const auto* demon_thunder = openkal::client::find_skill_effect(-2, 1, 29);
        expect(demon_thunder != nullptr &&
                   demon_thunder->effect == openkal::client::SkillEt::Lightning,
               "Demon Worker Thunder Strike dispatches the Lightning beam");
        const auto* demon_spark = openkal::client::find_skill_effect(-2, 1, 30);
        expect(demon_spark != nullptr && demon_spark->effect == openkal::client::SkillEt::Spark,
               "Demon Worker Spark dispatches both lightning beams");
        const auto* monster_fire = openkal::client::find_skill_effect(-2, 1, 31);
        expect(monster_fire != nullptr &&
                   monster_fire->effect == openkal::client::SkillEt::FireBolt,
               "Monster fire skill dispatches the fire bolt");
        const auto* monster_shock = openkal::client::find_skill_effect(-2, 1, 32);
        expect(monster_shock != nullptr &&
                   monster_shock->effect == openkal::client::SkillEt::LightningImpact,
               "Monster shock skill dispatches the lightning impact");
        const auto* monster_wide_fire = openkal::client::find_skill_effect(-2, 1, 33);
        expect(monster_wide_fire != nullptr &&
                   monster_wide_fire->effect == openkal::client::SkillEt::Fireball,
               "Monster wide fire dispatches the fireball");
        const auto* monster_flower = openkal::client::find_skill_effect(-2, 1, 34);
        expect(monster_flower != nullptr &&
                   monster_flower->effect == openkal::client::SkillEt::Explosion,
               "Monster fire-flower skill dispatches the explosion");
    }
    expect(Catalog::cast_kind_for(0, 6) == CastKind::None, "Knight Defense passive cannot be cast");
    expect(Catalog::cast_kind_for(0, 15) == CastKind::Combat,
           "Knight shield attack targets a monster");
    expect(Catalog::cast_kind_for(0, 14) == CastKind::Player,
           "Knight protect uses player targeting");
    expect(Catalog::cast_kind_for(0, 19) == CastKind::Party,
           "Knight chants use server-selected party targeting");
    expect(Catalog::cast_kind_for(0, 22) == CastKind::Combat,
           "Knight Shield Offense uses hostile targeting");
    expect(Catalog::cast_kind_for(0, 35) == CastKind::DeadPlayer,
           "Knight revival uses dead-player targeting");
    expect(Catalog::cast_kind_for(0, 42) == CastKind::Combat,
           "Knight Provocation of Blow uses hostile targeting");
    expect(Catalog::cast_kind_for(0, 88) == CastKind::SelfHeal,
           "Knight Walk on the Air is a self toggle");
    expect(Catalog::cast_kind_for(0, 91) == CastKind::SelfArea,
           "Knight Boom of Earth fires around the caster");
    expect(Catalog::cast_kind_for(0, 93) == CastKind::SelfArea,
           "Knight Wave of Earth fires once around the caster");
    expect(Catalog::cast_kind_for(0, 2) == CastKind::None, "Knight passive skill cannot be cast");
    expect(Catalog::cast_kind_for(0, 106) == CastKind::SelfHeal, "Rage is a self animal skill");
    expect(Catalog::cast_kind_for(0, 101) == CastKind::Combat,
           "animal attack skills need a target");
    expect(Catalog::skill_bar_casts_on_click(0, 106), "animal skills click-cast from the bar");
    expect(Catalog::is_knight_combat_skill(17),
           "Knight infernal skill participates in auto-combat");
    expect(Catalog::is_knight_combat_skill(42),
           "Knight Provocation of Blow participates in auto-combat");
    expect(!Catalog::is_knight_combat_skill(22),
           "Knight party skill does not participate in monster auto-combat");

    SkillDef infernal;
    infernal.set_clazz(0);
    infernal.set_index(17);
    infernal.set_mp(999);
    expect(Catalog::skill_spend_mp(0, &infernal, 4, 60) == 24,
           "Knight MP uses the shared rank formula");

    SkillDef level_cost;
    level_cost.set_clazz(0);
    level_cost.set_index(40);
    expect(Catalog::skill_spend_mp(0, &level_cost, 3, 60) == 338,
           "Knight MP receives the character level");

    SkillDef knight_cooldown;
    knight_cooldown.set_compare_cooldown_ms(700);
    expect(Catalog::ui_cooldown_ms(knight_cooldown, 1200) == 700,
           "Knight literal icon cooldown is preserved");
    knight_cooldown.set_compare_cooldown_weapon_speed(true);
    knight_cooldown.set_compare_cooldown_ms(0);
    expect(Catalog::ui_cooldown_ms(knight_cooldown, 1200) == 1200,
           "Knight weapon-speed cooldown uses current attack speed");
    knight_cooldown.set_compare_cooldown_ms(4600);
    expect(Catalog::ui_cooldown_ms(knight_cooldown, 1200) == 5800,
           "plus(weapon_speed, 4600) icon cooldown is aspeed plus delay");

    using openkal::client::knight_fx_dispatches;
    using openkal::client::SkillEt;
    using openkal::client::SkillFxAssetKind;
    using openkal::client::SkillFxAttach;
    struct ExpectedKnightFx {
        SkillEt effect;
        std::string_view asset;
        SkillFxAssetKind kind;
        SkillFxAttach attach;
        bool looping;
    };
    constexpr std::array<ExpectedKnightFx, 27> expected_knight_fx{{
        {SkillEt::UpperSmash, "effect_ef019", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::DashPrep, "effect_ef020", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::InfernalAttack,
         "effect_ef021",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         false},
        {SkillEt::InfernalAttack,
         "effect_strike01",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::Berserk, "effect_ef023", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::RainforceDefence,
         "effect_ef024",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::ChantOfProtect,
         "effect_ef025",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::PerfectDefense,
         "Tw_7_H_40_Floor",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ProvocationOfBlow,
         "Tw_7_A_44",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::SpinSlash, "Tw_7_A_38", SkillFxAssetKind::Fx, SkillFxAttach::CasterOrigin, false},
        {SkillEt::ShieldAttack, "effect_ef026", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::Sacrifice, "effect_ef027", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::Sacrifice, "effect_ef027a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::ChantOfLightning,
         "effect_ef028",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ChantOfIce,
         "effect_ef029",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ChantOfFlame,
         "effect_ef030",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ChantOfAccuracy,
         "effect_ef031",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ChantOfBattle,
         "effect_ef032",
         SkillFxAssetKind::Fx,
         SkillFxAttach::CasterOrigin,
         false},
        {SkillEt::ChantOfHealing,
         "effect_ef033",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::ChangeToVitalityPrep,
         "effect_ef034",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::ChangeToVitality,
         "effect_ef035",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         false},
        {SkillEt::ShieldOfResilience,
         "effect_ef036",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         false},
        {SkillEt::ReincarnatePrep,
         "effect_ef037",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::Reincarnate, "effect_ef038", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::FullSwing, "effect_ef039", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::Taunt, "taunt", SkillFxAssetKind::Particle, SkillFxAttach::Target, false},
        {SkillEt::WildAccuracy,
         "resurrection_p",
         SkillFxAssetKind::Particle,
         SkillFxAttach::Target,
         false},
    }};
    for (const auto& expected : expected_knight_fx) {
        bool found = false;
        for (const auto& actual : knight_fx_dispatches(expected.effect)) {
            found =
                found || (actual.asset == expected.asset && actual.kind == expected.kind &&
                          actual.attach == expected.attach && actual.looping == expected.looping);
        }
        const std::string message = std::string("Knight FX mapping ") + std::string(expected.asset);
        expect(found, message.c_str());
    }
    constexpr std::array<SkillEt, 25> handled_knight_effects{{
        SkillEt::UpperSmash,       SkillEt::DashPrep,           SkillEt::InfernalAttack,
        SkillEt::Berserk,          SkillEt::RainforceDefence,   SkillEt::ChantOfProtect,
        SkillEt::PerfectDefense,   SkillEt::ProvocationOfBlow,  SkillEt::SpinSlash,
        SkillEt::ShieldAttack,     SkillEt::Sacrifice,          SkillEt::ChantOfLightning,
        SkillEt::ChantOfIce,       SkillEt::ChantOfFlame,       SkillEt::ChantOfAccuracy,
        SkillEt::ChantOfBattle,    SkillEt::ChantOfHealing,     SkillEt::ChangeToVitalityPrep,
        SkillEt::ChangeToVitality, SkillEt::ShieldOfResilience, SkillEt::ReincarnatePrep,
        SkillEt::Reincarnate,      SkillEt::FullSwing,          SkillEt::Taunt,
        SkillEt::WildAccuracy,
    }};
    for (const SkillEt effect : handled_knight_effects) {
        expect(!knight_fx_dispatches(effect).empty(), "handled Knight ET has explicit FX mapping");
    }
    expect(knight_fx_dispatches(SkillEt::FireBolt).empty(),
           "Mage FX remains outside Knight mapping");

    using openkal::client::mage_fx_dispatches;
    struct ExpectedMageFx {
        SkillEt effect;
        std::string_view asset;
        SkillFxAssetKind kind;
        SkillFxAttach attach;
        bool looping;
    };
    constexpr std::array<ExpectedMageFx, 54> expected_mage_fx{{
        {SkillEt::MesmerizationPrep,
         "effect_ef113",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::Mesmerization,
         "effect_ef114",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::SpiritBlastPrep,
         "effect_ef115",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::EnchantWeaponPrep,
         "effect_ef094",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::EnchantWeapon,
         "effect_ef095",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::EnchantWeapon,
         "effect_ef095a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::AuraOfShieldPrep,
         "effect_ef098",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::AuraOfShield, "effect_ef099", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::AuraOfShield,
         "effect_ef099a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedStrength,
         "effect_ef100",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedStrength,
         "effect_ef100b",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedHealth,
         "effect_ef101",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedHealth,
         "effect_ef101a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedDexterity,
         "effect_ef102",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedDexterity,
         "effect_ef102a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedIntelligence,
         "effect_ef103",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::BlessedIntelligence,
         "effect_ef103b",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::AngelicDexterity,
         "effect_ef104",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::AngelicDexterity,
         "effect_ef104a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::PurgePrep, "effect_ef111", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::Purge, "effect_ef112", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::Purge, "effect_ef112a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::AmnesiaPrep, "effect_ef117", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::Amnesia, "effect_ef118", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::Amnesia, "effect_ef118a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::Healing,
         "healing",
         SkillFxAssetKind::Particle,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::LightRemedy,
         "healing",
         SkillFxAssetKind::Particle,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::LightReflection,
         "healing",
         SkillFxAssetKind::Particle,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::CallLightningPrep,
         "effect_ef074",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::CallLightningPrep,
         "effect_ef074_1",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::CallLightning,
         "effect_ef075",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::CallLightning,
         "effect_ef075a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::CallLightning,
         "effect_ef075b",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::CallLightning,
         "effect_ef075c",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Target,
         false},
        {SkillEt::IceLancePrep, "effect_ef080", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::IceLancePrep,
         "effect_ef080_1",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::IceLance,
         "effect_ef081a_ice1",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::IceLance,
         "effect_ef081a_ice2",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::IceLance,
         "effect_ef081a_ice3",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::IceLance,
         "effect_ef081a_ice4",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::SnowFieldPrep, "effect_ef082", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::SnowField, "effect_ef083", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::ChainLightningPrep,
         "effect_ef077",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::Thunder, "effect_ef077a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::IceRequeimPrep,
         "effect_ef084",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::IceRequeim, "effect_ef085", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
        {SkillEt::IceRequeim, "effect_ef085a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
        {SkillEt::ExplosionPrep, "effect_ef088", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
        {SkillEt::ExplosionPrep,
         "effect_ef088_1",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::Explosion,
         "effect_ef089a",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::Explosion,
         "effect_ef089b",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::Explosion,
         "effect_ef089c",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
        {SkillEt::ExplosiveBurstPrep,
         "effect_ef090",
         SkillFxAssetKind::Fx,
         SkillFxAttach::Caster,
         true},
        {SkillEt::ExplosiveBurst,
         "effect_ef091",
         SkillFxAssetKind::Fx,
         SkillFxAttach::TargetOrigin,
         false},
    }};
    for (const auto& expected : expected_mage_fx) {
        bool found = false;
        for (const auto& actual : mage_fx_dispatches(expected.effect)) {
            found =
                found || (actual.asset == expected.asset && actual.kind == expected.kind &&
                          actual.attach == expected.attach && actual.looping == expected.looping);
        }
        const std::string message = std::string("Mage FX mapping ") + std::string(expected.asset);
        expect(found, message.c_str());
    }
    constexpr std::array<SkillEt, 33> handled_mage_effects{{
        SkillEt::MesmerizationPrep,
        SkillEt::Mesmerization,
        SkillEt::SpiritBlastPrep,
        SkillEt::EnchantWeaponPrep,
        SkillEt::EnchantWeapon,
        SkillEt::AuraOfShieldPrep,
        SkillEt::AuraOfShield,
        SkillEt::BlessedStrength,
        SkillEt::BlessedHealth,
        SkillEt::BlessedDexterity,
        SkillEt::BlessedIntelligence,
        SkillEt::AngelicDexterity,
        SkillEt::PurgePrep,
        SkillEt::Purge,
        SkillEt::AmnesiaPrep,
        SkillEt::Amnesia,
        SkillEt::Healing,
        SkillEt::LightRemedy,
        SkillEt::LightReflection,
        SkillEt::CallLightningPrep,
        SkillEt::CallLightning,
        SkillEt::IceLancePrep,
        SkillEt::IceLance,
        SkillEt::SnowFieldPrep,
        SkillEt::SnowField,
        SkillEt::ChainLightningPrep,
        SkillEt::Thunder,
        SkillEt::IceRequeimPrep,
        SkillEt::IceRequeim,
        SkillEt::ExplosionPrep,
        SkillEt::Explosion,
        SkillEt::ExplosiveBurstPrep,
        SkillEt::ExplosiveBurst,
    }};
    for (const SkillEt effect : handled_mage_effects) {
        expect(!mage_fx_dispatches(effect).empty(), "handled Mage ET has explicit FX mapping");
    }
    expect(mage_fx_dispatches(SkillEt::FireBolt).empty(),
           "Mage combat bolts remain outside Mage SpriteFx mapping");
    expect(mage_fx_dispatches(SkillEt::Lightning).empty(),
           "Chain Lightning fire stays CLightning, not a SpriteFx mapping");
    expect(mage_fx_dispatches(SkillEt::SpiritBlast).empty(),
           "Spirit Blast fire stays CLightning, not a SpriteFx mapping");
    expect(mage_fx_dispatches(SkillEt::HealingPrep).empty(),
           "HealingPrep has no FX class; animation only");
    using openkal::client::find_skill_effect;
    const auto* bless = find_skill_effect(1, 1, 49);
    expect(bless != nullptr && bless->effect == SkillEt::BlessedStrength,
           "Blessing of Strength fire maps to BlessedStrength");
    const auto* bless_prep = find_skill_effect(1, 0, 49);
    expect(bless_prep != nullptr && bless_prep->effect == SkillEt::AuraOfShieldPrep,
           "Blessing of Strength prep reuses AuraOfShieldPrep");
    const auto* haste = find_skill_effect(1, 1, 26);
    expect(haste != nullptr && haste->effect == SkillEt::Haste,
           "Speed Up fire maps to the compatible client Haste model effect");
    SkillDef speed_up;
    speed_up.set_clazz(1);
    speed_up.set_index(26);
    for (int phase = 0; phase < 3; ++phase) {
        speed_up.set_sound(phase, "test_sound");
        expect(speed_up.sound(phase).empty(), "Speed Up suppresses configured skill sounds");
    }
    speed_up.set_clazz(2);
    for (int phase = 0; phase < 3; ++phase) {
        expect(speed_up.sound(phase) == "test_sound", "Other classes keep configured sounds");
    }

    using openkal::client::mage_range_preview_fx;
    using openkal::client::range_skill_preview_fx;
    expect(mage_range_preview_fx(1, 43) == "effect_ef078", "thunder storm targeting FX");
    expect(mage_range_preview_fx(1, 45) == "effect_ef086", "ice storm targeting FX");
    expect(mage_range_preview_fx(1, 48) == "effect_ef092", "fire storm targeting FX");
    expect(mage_range_preview_fx(1, 47).empty(),
           "explosive burst targeting shows the ground ring only");
    expect(range_skill_preview_fx(1, 48) == "effect_ef092", "range preview reuses mage storms");
    expect(range_skill_preview_fx(2, 47) == "effect_ef092",
           "Arrow Rain targeting reuses fire-storm preview FX");
    expect(range_skill_preview_fx(2, 42).empty(),
           "Poison Cloud targeting shows the ground ring only");

    const auto* summons_prep = find_skill_effect(1, 0, 31);
    expect(summons_prep != nullptr && summons_prep->effect == SkillEt::CallLightningPrep,
           "Lightning Summons prep maps to CallLightningPrep");
    const auto* summons_fire = find_skill_effect(1, 1, 31);
    expect(summons_fire != nullptr && summons_fire->effect == SkillEt::CallLightning,
           "Lightning Summons fire maps to CallLightning");
    const auto* hail_prep = find_skill_effect(1, 0, 32);
    expect(hail_prep != nullptr && hail_prep->effect == SkillEt::IceLancePrep,
           "Hail prep maps to IceLancePrep");
    const auto* hail_fire = find_skill_effect(1, 1, 32);
    expect(hail_fire != nullptr && hail_fire->effect == SkillEt::IceLance,
           "Hail fire maps to IceLance");
    const auto* cold_prep = find_skill_effect(1, 0, 37);
    expect(cold_prep != nullptr && cold_prep->effect == SkillEt::SnowFieldPrep,
           "Cold Binding prep maps to SnowFieldPrep");
    const auto* cold_fire = find_skill_effect(1, 1, 37);
    expect(cold_fire != nullptr && cold_fire->effect == SkillEt::SnowField,
           "Cold Binding fire maps to SnowField");
    const auto* chain_prep = find_skill_effect(1, 0, 41);
    expect(chain_prep != nullptr && chain_prep->effect == SkillEt::ChainLightningPrep,
           "Chain Lightning prep maps to ChainLightningPrep");
    const auto* chain_fire = find_skill_effect(1, 1, 41);
    expect(chain_fire != nullptr && chain_fire->effect == SkillEt::Lightning &&
               chain_fire->effect2 == SkillEt::Lightning,
           "Chain Lightning fire stays ET_LIGHTNING (CLightning)");
    const auto* thunder_prep = find_skill_effect(1, 0, 42);
    expect(thunder_prep != nullptr && thunder_prep->effect == SkillEt::CallLightningPrep,
           "Thunder prep reuses CallLightningPrep");
    const auto* thunder_fire = find_skill_effect(1, 1, 42);
    expect(thunder_fire != nullptr && thunder_fire->effect == SkillEt::Thunder,
           "Thunder fire maps to Thunder");
    const auto* ice_r_prep = find_skill_effect(1, 0, 44);
    expect(ice_r_prep != nullptr && ice_r_prep->effect == SkillEt::IceRequeimPrep,
           "Ice Restriction prep maps to IceRequeimPrep");
    const auto* ice_r_fire = find_skill_effect(1, 1, 44);
    expect(ice_r_fire != nullptr && ice_r_fire->effect == SkillEt::IceRequeim,
           "Ice Restriction fire maps to IceRequeim");
    const auto* explosion_prep = find_skill_effect(1, 0, 46);
    expect(explosion_prep != nullptr && explosion_prep->effect == SkillEt::ExplosionPrep,
           "Explosion prep maps to ExplosionPrep");
    const auto* explosion_fire = find_skill_effect(1, 1, 46);
    expect(explosion_fire != nullptr && explosion_fire->effect == SkillEt::Explosion,
           "Explosion fire maps to Explosion");
    const auto* burst_prep = find_skill_effect(1, 0, 47);
    expect(burst_prep != nullptr && burst_prep->effect == SkillEt::ExplosiveBurstPrep,
           "Explosive Burst prep maps to ExplosiveBurstPrep");
    const auto* burst_fire = find_skill_effect(1, 1, 47);
    expect(burst_fire != nullptr && burst_fire->effect == SkillEt::ExplosiveBurst,
           "Explosive Burst fire maps to ExplosiveBurst");

    expect(openkal::client::lightning_beam_count(SkillEt::Lightning) == 1,
           "Lightning Magic uses one CLightning polyline");
    expect(openkal::client::lightning_beam_count(SkillEt::Spark) == 2,
           "Lightning Blow uses two CLightning polylines");
    expect(openkal::client::is_mage_bolt_et(SkillEt::FireBolt) &&
               openkal::client::is_mage_bolt_et(SkillEt::IceBolt) &&
               openkal::client::is_mage_bolt_et(SkillEt::Fireball) &&
               openkal::client::is_mage_bolt_et(SkillEt::FrostBlast) &&
               !openkal::client::is_mage_bolt_et(SkillEt::Lightning),
           "Fire/Ice Magic fly bolts; Lightning does not");
    expect(!openkal::client::is_mage_bolt_et(SkillEt::Explosion) &&
               !openkal::client::is_mage_bolt_et(SkillEt::ExplosiveBurst) &&
               !openkal::client::is_mage_bolt_et(SkillEt::IceLance) &&
               !openkal::client::is_mage_bolt_et(SkillEt::IceRequeim) &&
               !openkal::client::is_mage_bolt_et(SkillEt::SnowField) &&
               !openkal::client::is_mage_bolt_et(SkillEt::CallLightning) &&
               !openkal::client::is_mage_bolt_et(SkillEt::Thunder),
           "Hermit CGeneralEffect skills must not wait for a bolt crash");
    expect(openkal::client::Catalog::is_mage_combat_skill(4), "Lightning Magic is a combat skill");
    expect(openkal::client::Catalog::is_mage_combat_skill(32), "Hail is a combat skill");
    expect(openkal::client::Catalog::is_mage_combat_skill(41), "Chain Lightning is a combat skill");
    expect(openkal::client::Catalog::is_mage_combat_skill(42), "Thunder is a combat skill");
    expect(openkal::client::Catalog::is_mage_combat_skill(44), "Ice Restriction is a combat skill");
    expect(openkal::client::Catalog::is_mage_combat_skill(46), "Explosion is a combat skill");
    expect(!openkal::client::Catalog::is_mage_combat_skill(47),
           "Explosive Burst is ground-targeted, not auto-combat");
    expect(!openkal::client::Catalog::is_mage_combat_skill(43),
           "Thunder Storm is ground-targeted, not auto-combat");
    const auto* splashy = find_skill_effect(1, 1, 24);
    expect(splashy != nullptr && splashy->effect == SkillEt::IceNova,
           "Splashy Ice fire maps to IceNova");
    expect(Catalog::cast_kind_for(1, 24) == CastKind::SelfArea,
           "Splashy Ice uses caster-centered area");
    expect(mage_fx_dispatches(SkillEt::IceNova).empty(),
           "IceNova is DrawDonut, not a Mage SpriteFx mapping");
    expect(!openkal::client::is_mage_bolt_et(SkillEt::IceNova),
           "Splashy Ice must not wait for an ice bolt crash");
    {
        float r1 = -1.0f;
        float r2 = -1.0f;
        openkal::client::ice_nova_radii(0.0f, r1, r2);
        expect(r1 == 0.0f && r2 == 0.0f, "IceNova radii start at zero");
        openkal::client::ice_nova_radii(1.0f, r1, r2);
        expect(r1 == 64.0f && r2 == 64.0f, "IceNova radii end at 64");
        openkal::client::ice_nova_radii(0.5f, r1, r2);
        expect(r2 == 32.0f, "IceNova outer radius is linear");
        expect(r1 > 63.0f && r1 < 64.0f, "IceNova inner radius snaps near 64");
    }
    expect(openkal::client::is_fire_delay_damage_skill(2) &&
               openkal::client::is_fire_delay_damage_skill(25) &&
               !openkal::client::is_fire_delay_damage_skill(3),
           "Fire Magic and Fire Blow drain dest-info HP; Ice Magic does not");
    expect(openkal::client::fire_delay_damage_hp_ratio(400, 500, 100, 0.0f) > 0.99f,
           "fire dest-info starts at pre-hit HP");
    expect(openkal::client::fire_delay_damage_hp_ratio(400, 500, 100, 0.5f) > 0.89f &&
               openkal::client::fire_delay_damage_hp_ratio(400, 500, 100, 0.5f) < 0.91f,
           "fire dest-info is halfway at 500 ms");
    expect(openkal::client::fire_delay_damage_hp_ratio(400, 500, 100, 1.0f) > 0.79f &&
               openkal::client::fire_delay_damage_hp_ratio(400, 500, 100, 1.0f) < 0.81f,
           "fire dest-info reaches true HP at 1 s");

    {
        openkal::client::Catalog catalog;
        const std::string skills_dir = std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/skills";
        expect(catalog.load_json_dir(skills_dir), "load skill catalog for stone shield icons");
        {
            const std::unordered_map<int, int> mage_starting_skills{{1, 1}, {4, 1}};
            openkal::client::PlayerSkills new_mage;
            new_mage.apply_server_info(mage_starting_skills, catalog, 1);
            expect(new_mage.hotbar_slot(0) == 1 && new_mage.hotbar_slot(1) == 4 &&
                       new_mage.active_index() == 4,
                   "new Mage starts with Beheading and active Lightning Magic");

            openkal::client::PlayerSkills existing_mage;
            std::array<int, openkal::client::PlayerSkills::kHotbarSlots> saved_slots{};
            saved_slots.fill(-1);
            saved_slots[0] = 1;
            existing_mage.apply_shortcut(1, saved_slots);
            existing_mage.apply_server_info(mage_starting_skills, catalog, 1);
            expect(existing_mage.hotbar_slot(1) == -1 && existing_mage.active_index() == 1,
                   "existing Mage hotbar remains unchanged");
        }
        std::set<int> configured_monster_skills;
        const std::string monsters_dir =
            std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/monsters";
        for (const std::string& name : openkal::platform::list_directory(monsters_dir)) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
                continue;
            }
            const auto bytes =
                openkal::platform::read_file(openkal::platform::join_path(monsters_dir, name));
            openkal::platform::Json root;
            if (!bytes ||
                !openkal::platform::json_parse(std::string(bytes->begin(), bytes->end()), root)) {
                continue;
            }
            const auto& slots = root.at("stats").at("skill");
            if (!slots.is_array() || slots.array().empty()) {
                continue;
            }
            const auto add_slot = [&](const openkal::platform::Json& slot) {
                if (!slot.is_array() || slot.array().empty()) {
                    return;
                }
                const int skill_index = slot.array().front().as_int(-1);
                if (skill_index >= 0) {
                    configured_monster_skills.insert(skill_index);
                }
            };
            if (slots.array().front().is_array()) {
                for (const auto& slot : slots.array()) {
                    add_slot(slot);
                }
            } else {
                add_slot(slots);
            }
        }
        expect(configured_monster_skills.size() == 64,
               "every configured monster skill is included in the visual audit");
        std::set<int> configured_motion_variants;
        for (const int skill_index : configured_monster_skills) {
            const auto* monster_skill = catalog.find(-2, skill_index);
            expect(monster_skill != nullptr,
                   "every configured monster skill has a monster catalog entry");
            if (monster_skill == nullptr) {
                continue;
            }
            configured_motion_variants.insert(std::max(1, monster_skill->motion_prep()));
            configured_motion_variants.insert(std::max(1, monster_skill->motion_fire()));
        }
        for (int motion = 1; motion <= 21; ++motion) {
            expect(configured_motion_variants.count(motion) != 0,
                   "configured monster motion variants are collected for Action 7 loading");
        }
        const auto* walk_air = catalog.find(0, 88);
        const auto* boom_earth = catalog.find(0, 91);
        const auto* wave_earth = catalog.find(0, 93);
        const auto* perfect_defense = catalog.find(0, 40);
        expect(walk_air && walk_air->awaken(), "Walk on the Air is an Awaken skill");
        expect(boom_earth && boom_earth->awaken(), "Boom of Earth is an Awaken skill");
        expect(wave_earth && wave_earth->awaken(), "Wave of Earth is an Awaken skill");
        expect(perfect_defense && !perfect_defense->awaken(),
               "Perfect Defense stays on the 3rd Job tab");
        expect(catalog.buff_icon_for_skill(1, 17) == "active015.bmp", "DI1 icon");
        expect(catalog.buff_icon_for_skill(1, 18) == "active019.bmp", "DI2 icon");
        expect(catalog.buff_icon_for_skill(1, 19) == "active020.bmp", "DI3 icon");
        expect(catalog.buff_icon_for_skill(1, 20) == "active021.bmp", "DI4 icon");
        expect(catalog.buff_icon_for_skill(1, 21) == "active022.bmp", "DI5 icon");
        expect(catalog.buff_icon_for_skill(1, 63) == "active124.bmp", "DI6 icon");
        const auto* splashy = catalog.find(1, 24);
        expect(splashy != nullptr && splashy->delay_pre_ms() == 800 &&
                   splashy->compare_preskill_ms() == 900 && Catalog::preskill_ms(*splashy) == 900 &&
                   Catalog::ui_cooldown_ms(*splashy, 1200) == 600,
               "Splashy Ice PRESKILL is compare[2] 900 ms; icon is 600 ms");
        const auto* rush = catalog.find(0, 21);
        expect(rush && Catalog::skill_cast_range(0, rush, 1, 32) == 80 &&
                   Catalog::skill_cast_range(0, rush, 2, 32) == 100,
               "Rush approach range follows learned rank rather than the first rank");
        expect(rush && Catalog::ui_cooldown_ms(*rush, 1200, 1) == 12000 &&
                   Catalog::ui_cooldown_ms(*rush, 1200, 2) == 10000,
               "Rush cooldown follows learned rank instead of a constant fallback");
        const auto* rain = catalog.find(2, 47);
        expect(rain && Catalog::ui_cooldown_ms(*rain, 1200, 1) == 300000 &&
                   Catalog::ui_cooldown_ms(*rain, 1200, 6) == 200000,
               "Arrow Rain reads every cooldown array entry");
        const auto* hypnotism = catalog.find(1, 36);
        expect(hypnotism && Catalog::preskill_ms(*hypnotism, 1) == 2400 &&
                   Catalog::preskill_ms(*hypnotism, 5) == 2000,
               "Hypnotism preparation follows learned rank");
        const auto* fire_magic = catalog.find(1, 2);
        expect(fire_magic && Catalog::preskill_ms(*fire_magic, 1, 0) == 1300 &&
                   Catalog::preskill_ms(*fire_magic, 1, 5) == 975,
               "Fire Magic preparation uses learned Fire Mastery");
        const auto* upper_smash = catalog.find(0, 16);
        const auto* soul_destruction = catalog.find(1, 40);
        const auto* eruption = catalog.find(1, 46);
        SkillAbilityVars damage_vars;
        damage_vars.has_stats = true;
        damage_vars.char_level = 60;
        damage_vars.char_int = 100;
        damage_vars.char_wis = 1;
        expect(soul_destruction && soul_destruction->eval_parameter(1, 1, damage_vars) == 385 &&
                   soul_destruction->eval_parameter(2, 2, damage_vars) == 28,
               "Soul Destruction tooltip uses Intelligence and the MP cost");
        expect(eruption && eruption->eval_parameter(1, 1, damage_vars) == 858,
               "Explosion tooltip groups the division and adds the initial 580 bonus");
        damage_vars.char_int = 1000;
        expect(soul_destruction && soul_destruction->eval_parameter(1, 2, damage_vars) == 1000 &&
                   eruption && eruption->eval_parameter(1, 2, damage_vars) == 1200,
               "damage tooltips apply their rank-dependent caps");
        expect(upper_smash != nullptr && !upper_smash->attack_auto() &&
                   upper_smash->compare_cooldown_weapon_speed() &&
                   upper_smash->compare_cooldown_ms() == 4600 &&
                   Catalog::ui_cooldown_ms(*upper_smash, 1200) == 5800,
               "Powerful Upward Slash is a one-shot with aspeed plus 4600 ms cooldown");
        const auto* spin_slash = catalog.find(0, 38);
        expect(spin_slash != nullptr && spin_slash->cast_range() == 400 &&
                   Catalog::skill_cast_range(0, spin_slash, 1, 80) == 400,
               "Spin Slash compare[2] range is 400 like Provocation of Blow");
        const auto* lightning_slash = catalog.find(0, 3);
        expect(lightning_slash != nullptr && lightning_slash->attack_auto(),
               "Lightning Slash motion[2] attackauto recasts on RMB");
        const auto* ice_magic = catalog.find(1, 3);
        expect(ice_magic != nullptr && Catalog::preskill_ms(*ice_magic) == 800,
               "Ice Magic PRESKILL matches delay[0]");
        const auto* fire_blow = catalog.find(1, 25);
        expect(fire_blow != nullptr && Catalog::preskill_ms(*fire_blow) == 2100,
               "Fire Blow PRESKILL matches delay[0]");
        const auto* meditation = catalog.find(1, 15);
        expect(meditation != nullptr && meditation->once() && Catalog::is_castable(*meditation),
               "Meditation is an active once skill and can go on the bar");
        expect(Catalog::cast_kind_for(1, 15) == CastKind::SelfHeal,
               "Meditation is a self click-cast, not a combat skill");
        const auto* flamy_arrow = catalog.find(2, 6);
        expect(flamy_arrow != nullptr && flamy_arrow->effect(0) == "Skill_M_6_0" &&
                   flamy_arrow->motion_prep() == 2 && flamy_arrow->motion_fire() == 3,
               "Flamy Arrow prep GB is Skill_M_6_0, fire clip is motion 3");
        const auto* blow_up = catalog.find(2, 14);
        expect(blow_up != nullptr && blow_up->effect(0).empty() && blow_up->motion_prep() == 2 &&
                   blow_up->motion_fire() == 4,
               "Blow-up Arrow has no prep GB and fires motion 4, not Staggering Blow 3");
        const auto* arrow_explosion = catalog.find(2, 49);
        expect(arrow_explosion != nullptr && arrow_explosion->motion_prep() == 16 &&
                   arrow_explosion->motion_fire() == 13 &&
                   Catalog::cast_kind_for(2, 49) == CastKind::Combat &&
                   Catalog::is_castable(*arrow_explosion) &&
                   Catalog::preskill_ms(*arrow_explosion) == 1800,
               "Arrow Explosion is a hostile bow shot with 1800 ms PRESKILL");
        const auto* shrewdness = catalog.find(2, 48);
        expect(shrewdness != nullptr && shrewdness->passive() &&
                   shrewdness->format_ability(1).find('?') != std::string::npos,
               "Shrewdness needs char_org_dodge for evasion points");
        SkillAbilityVars shrewd_vars;
        shrewd_vars.has_stats = true;
        shrewd_vars.char_level = 70;
        shrewd_vars.char_org_dodge = 200;
        expect(shrewdness->format_ability(1, shrewd_vars).find("17") != std::string::npos,
               "Shrewdness catalog formula fills evasion points");
        const auto* archer_blessing = catalog.find(2, 70);
        expect(archer_blessing != nullptr &&
                   archer_blessing->format_ability(1).find("8 %") != std::string::npos &&
                   archer_blessing->format_ability(1).find("% %") == std::string::npos,
               "Blessing of Son of the Sky shows a single percent");
        const auto* archer_blessing_71 = catalog.find(2, 71);
        expect(archer_blessing_71 != nullptr &&
                   archer_blessing_71->format_ability(1).find("8 %") != std::string::npos &&
                   archer_blessing_71->format_ability(1).find("% %") == std::string::npos,
               "Blessing of Son of the Sky (71) shows a single percent");
        const auto* staggering = catalog.find(2, 3);
        expect(staggering != nullptr && staggering->effect(0) == "Skill_M_3_0" &&
                   staggering->motion_fire() == 3,
               "Staggering Blow keeps Skill_M_3_0 prep GB");
        const auto* focus_shot = catalog.find(2, 16);
        expect(focus_shot != nullptr && focus_shot->once() && Catalog::is_castable(*focus_shot) &&
                   Catalog::cast_kind_for(2, 16) == CastKind::Combat &&
                   Catalog::skill_bar_sets_active(CastKind::Combat) &&
                   Catalog::skill_bar_casts_on_click(2, 16) &&
                   !Catalog::skill_bar_casts_on_click(2, 6) &&
                   Catalog::skill_cast_range(2, focus_shot, 1, 100) == 160 &&
                   Catalog::preskill_ms(*focus_shot) == 3300,
               "Focus Shot charges from the icon without occupying the combat slot");
        const auto* splashy_ice = catalog.find(1, 24);
        expect(splashy_ice != nullptr && splashy_ice->once() &&
                   Catalog::cast_kind_for(1, 24) == CastKind::SelfArea &&
                   Catalog::skill_bar_casts_on_click(1, 24) &&
                   Catalog::skill_bar_casts_on_click(1, 43) &&
                   Catalog::skill_bar_casts_on_click(1, 45) &&
                   Catalog::skill_bar_casts_on_click(1, 48) &&
                   Catalog::skill_bar_casts_on_click(2, 42) &&
                   Catalog::skill_bar_casts_on_click(2, 47) &&
                   Catalog::uses_range_skill_selector(2, 47) &&
                   Catalog::uses_range_skill_selector(2, 42) &&
                   !Catalog::uses_range_skill_selector(2, 16) &&
                   !Catalog::skill_bar_casts_on_click(1, 2) &&
                   !Catalog::skill_bar_sets_active(CastKind::SelfArea) &&
                   !Catalog::skill_bar_sets_active(CastKind::GroundPosition),
               "Splashy Ice, storms, and Archer Arrow Rain fire from the icon without occupying "
               "the combat slot");
        expect(Catalog::skill_bar_casts_on_click(0, 200) &&
                   Catalog::skill_bar_casts_on_click(2, 201) &&
                   Catalog::skill_bar_casts_on_click(1, 202) &&
                   Catalog::skill_bar_casts_on_click(0, 101) &&
                   Catalog::skill_bar_casts_on_click(0, 110) &&
                   Catalog::skill_bar_casts_on_click(2, 111),
               "Baby/Young/Adult transform and Gale hiding fire from the icon without occupying "
               "the combat slot");
        const auto* baby = catalog.find(-1, 200);
        const auto* lightning_wave = catalog.find(16, 101);
        expect(baby != nullptr && baby->motion_fire() == 64 && baby->motion_prep() == 64 &&
                   lightning_wave != nullptr && lightning_wave->motion_prep() == 2 &&
                   lightning_wave->motion_fire() == 3,
               "Transform PRE uses GO_SKILL 64; Gunchi Lightning Wave uses 2/3");
        const auto* archer_behead = catalog.find(2, 1);
        const auto* mage_behead = catalog.find(1, 1);
        const auto* knight_behead = catalog.find(0, 1);
        const auto* thief_behead = catalog.find(3, 1);
        expect(archer_behead != nullptr && archer_behead->grade_mask() == 1 &&
                   mage_behead != nullptr && mage_behead->grade_mask() == 1 &&
                   knight_behead != nullptr && knight_behead->grade_mask() == 1 &&
                   thief_behead != nullptr && thief_behead->grade_mask() == 1 &&
                   Catalog::cast_kind_for(2, 1) == CastKind::Behead &&
                   Catalog::cast_kind_for(3, 1) == CastKind::Behead &&
                   Catalog::skill_bar_sets_active(CastKind::Behead) &&
                   !Catalog::skill_bar_casts_on_click(2, 1) &&
                   !Catalog::skill_bar_casts_on_click(1, 1) &&
                   !Catalog::skill_bar_casts_on_click(0, 1) &&
                   !Catalog::skill_bar_casts_on_click(3, 1),
               "Beheading is a Common-tab combat skill for Knight, Mage, Archer, and Thief");
        const auto* life_abs = catalog.find(2, 18);
        expect(life_abs != nullptr && Catalog::cast_kind_for(2, 18) == CastKind::Combat &&
                   Catalog::skill_cast_range(2, life_abs, 1, 200) == 160,
               "Life Absorption uses compare range 160, not weapon range");
        int imperial_hostile = 0;
        for (const auto* def : catalog.for_class(2)) {
            if (!def || def->grade_mask() != 11 || def->role() != "archer") {
                continue;
            }
            const int idx = def->index();
            const auto shape = openkal::protocol::archer_target_shape(idx);
            if (shape != openkal::protocol::ArcherTargetShape::HostileCharacter) {
                continue;
            }
            ++imperial_hostile;
            const bool player_only = idx == 34;
            expect(Catalog::cast_kind_for(2, idx) ==
                       (player_only ? CastKind::Player : CastKind::Combat),
                   "Imperial Commander hostile skills share Combat/Player routing");
            expect(!Catalog::skill_bar_casts_on_click(2, idx) && !def->attack_auto(),
                   "Imperial Commander hostiles are one-shots, not attackauto");
            if (def->cast_range() > 0) {
                expect(Catalog::skill_cast_range(2, def, 1, 300) == def->cast_range(),
                       "Imperial Commander hostiles use compare range, not bow range");
            }
            if (def->compare_preskill_ms() > 0) {
                expect(Catalog::preskill_ms(*def) == def->compare_preskill_ms(),
                       "Imperial Commander PRESKILL follows compare id 3");
            }
        }
        expect(imperial_hostile == 16, "Imperial Commander has 16 HostileCharacter actives");
        const auto* pain = catalog.find(2, 19);
        expect(pain != nullptr && Catalog::cast_kind_for(2, 19) == CastKind::Combat &&
                   Catalog::skill_cast_range(2, pain, 1, 250) == 180 &&
                   Catalog::preskill_ms(*pain) == 1500 && !pain->attack_auto(),
               "Pain uses compare range 180 and 1500 ms PRESKILL, not weapon range");
        const auto* confidence = catalog.find(2, 43);
        expect(confidence != nullptr && Catalog::skill_cast_range(2, confidence, 1, 250) == 90,
               "Loss of Confidence uses compare range 90, not weapon range");
        expect(openkal::protocol::mage_target_shape(32) ==
                       openkal::protocol::MageTargetShape::Monster &&
                   openkal::protocol::mage_target_shape(44) ==
                       openkal::protocol::MageTargetShape::TargetArea &&
                   openkal::protocol::mage_target_shape(37) ==
                       openkal::protocol::MageTargetShape::TargetArea,
               "Hail is single-target IceLance; Ice Restriction and Cold Binding are area");
        const auto* hail_et = find_skill_effect(1, 1, 32);
        const auto* ice_req_et = find_skill_effect(1, 1, 44);
        expect(hail_et != nullptr && hail_et->effect == SkillEt::IceLance &&
                   ice_req_et != nullptr && ice_req_et->effect == SkillEt::IceRequeim,
               "Hail fire is IceLance, not Ice Restriction");
        const auto* meditation_et = find_skill_effect(1, 1, 15);
        expect(meditation_et != nullptr && meditation_et->effect == SkillEt::Meditation &&
                   mage_fx_dispatches(SkillEt::Meditation).empty(),
               "Meditation uses the procedural glow renderer");
        expect(Catalog::cast_kind_for(1, 44) == CastKind::TargetArea &&
                   Catalog::skill_bar_sets_active(CastKind::TargetArea) &&
                   Catalog::skill_bar_sets_active(CastKind::Combat) &&
                   !Catalog::skill_bar_sets_active(CastKind::SelfHeal),
               "Ice Restriction LMB selects like Hail; Meditation still click-casts");
    }
    {
        using openkal::client::CastKind;
        using openkal::client::Catalog;
        using openkal::client::find_skill_effect;
        using openkal::client::SkillEt;
        expect(Catalog::cast_kind_for(2, 15) == CastKind::Player,
               "Arrow of Silence is player-only");
        expect(Catalog::cast_kind_for(2, 27) == CastKind::Player,
               "Powerful Arrow of Silence is player-only");
        expect(Catalog::cast_kind_for(2, 34) == CastKind::Player, "Mana Burn is player-only");
        expect(Catalog::cast_kind_for(2, 6) == CastKind::Combat, "Flamy Arrow is combat");
        expect(find_skill_effect(2, 1, 21) == nullptr,
               "Mysterious Arrow is dispatched outside the phase table");
        const auto* vitality = find_skill_effect(2, 1, 18);
        expect(vitality != nullptr && vitality->effect == SkillEt::VitalityDrain,
               "Life Absorption fire maps to VitalityDrain");
        const auto* exploder = find_skill_effect(2, 1, 49);
        expect(exploder != nullptr && exploder->effect == SkillEt::ExploderArrow,
               "Arrow Explosion fire maps to the delayed ExploderArrow fuse");
        expect(Catalog::cast_kind_for(2, 18) == CastKind::Combat &&
                   openkal::protocol::archer_target_shape(18) ==
                       openkal::protocol::ArcherTargetShape::HostileCharacter,
               "Life Absorption is a hostile combat shot");
        expect(openkal::client::kArcherMysticArrowCount == 3,
               "Mysterious Arrow client volley is 3");
        expect(openkal::client::is_red_lightning_et(SkillEt::ManaBurn) ||
                   openkal::client::is_red_lightning_et(SkillEt::LightningRed),
               "Mana Burn uses the red lightning ET");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
