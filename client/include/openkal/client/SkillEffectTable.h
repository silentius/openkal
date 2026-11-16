/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    /** Presentation effect identifiers implemented by the OpenKal client.
     * FX/particle names refer to entries in Effect/Fx or Effect/Sprite containers.
     * See docs/SkillEffects.md for per-skill phases, assets and current limitations.
     */
    enum class SkillEt : std::uint16_t {
        /** No additional effect is dispatched for this entry. */
        None = 0,

        /** Flies Skill/Skill_R_2_1.gb at 500 units/s; Sprite firebolt_projectile trail,
         * firebolt_explosion and three firebolt_smallfire impact emitters. Sounds efs010 at launch
         * and efs009 at impact.
         */
        FireBolt,

        /** Flies Skill/Skill_R_3_1.gb at 500 units/s; Sprite icebolt_projectile trail and
         * icebolt_explosion impact. Sounds efs015 at launch and efs017 at impact.
         */
        IceBolt,

        /** Procedural yellow ribbon using Lightning.GTX, lig01.GTX, star4.GTX and
         * LightningSpark.GTX; caster mesh Skill/Skill_R_4_0.gb; sound efs021.
         */
        Lightning,

        /** Particle healing at TargetOrigin; also draws a procedural target-following ring with
         * Effect/healing.dds and plays efs022 at the target origin.
         */
        Healing,

        /** Acknowledged without a dedicated emitter; character animation and optional JSON
         * preparation models supply the visual.
         */
        HealingPrep,

        /** Uses the yellow lightning ribbon, textures and caster mesh described for Lightning;
         * sound efs021.
         */
        LightningImpact,

        /** Uses the cast-model fallback Skill/Skill_M_3_0.gb at the caster tip, plus sound efs010;
         * JSON may name an additional preparation model.
         */
        LightningImpactPrep,

        /** Spawns two yellow Lightning ribbons with the same texture/caster-mesh family and sound
         * efs021.
         */
        Spark,

        /** Procedural expanding ring using Effect/whiteres.dds at the target origin; sound efs017. */
        IceNova,

        /** Flies Skill/Skill_R_2_1.gb at 500 units/s; Sprite firebolt_projectile trail,
         * firebolt_explosion and three firebolt_smallfire impact emitters. Sounds efs010 at launch
         * and efs009 at impact.
         */
        Fireball,

        /** Flies Skill/Skill_R_3_1.gb at 500 units/s; Sprite icebolt_projectile trail and
         * icebolt_explosion impact. Sounds efs015 at launch and efs017 at impact.
         */
        FrostBlast,

        /** Particle taunt at Target */
        Taunt,

        /** FX effect_ef019 at Caster */
        UpperSmash,

        /** FX effect_ef021 at Caster; FX effect_strike01 at Target */
        InfernalAttack,

        /** FX effect_ef026 at Caster */
        ShieldAttack,

        /** FX effect_ef039 at Caster */
        FullSwing,

        /** FX effect_ef023 at Caster */
        Berserk,

        /** FX effect_ef020 at Caster (cast loop) */
        DashPrep,

        /** FX effect_ef041 at Caster; Visible projectile uses Item/Weapon/M_arrow.gb and the
         * Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent.
         * At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or
         * effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.
         */
        ChargedArrow,

        /** FX effect_ef040 at Caster (cast loop); FX effect_ef040a at Caster (cast loop); FX
         * effect_ef040b at Caster
         */
        ChargedArrowPrep,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. Mysterious Arrow (Archer 21)
         * launches three projectiles with distinct parabolic phases. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        MysticArrow,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        FireArrow,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        PoisonArrow,

        /** Particle explosion at Target; Visible projectile uses Item/Weapon/M_arrow.gb and the
         * Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent.
         * Target dispatch is delayed by distance / 500 units/s. Plays efs009 and efc007 at impact.
         */
        ExplosionArrow,

        /** FX davi_ef131_04 at Target (three-second fuse); Plants FX davi_ef131_04 on the target;
         * after three seconds detaches the fuse and spawns Sprite davi_ef131_05 on target bone 1,
         * using sounds read from the FX entry of the same name. Visible projectile uses
         * Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback
         * shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and
         * resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/
         * then Effect/ GB lookup.
         */
        ExploderArrow,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        StunningArrow,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        NegativeArrow,

        /** Plays Effect/effect_behead.gb at the target for 1.2 seconds. */
        BeheadingCharm,

        /** Acknowledged without a dedicated fire-phase mesh or emitter; the preparation supplies
         * the visual.
         */
        Stoneshield,

        /** Procedural orbiting glow around the caster spine using the lig01.GTX effect texture. */
        StoneshieldPrep,

        /** Plays the haste GB clip at the target origin; resolves Skill/haste.gb first, then
         * Effect/haste.gb. Clip keyframes supply additional cues.
         */
        Haste,

        /** Two procedural orbiting glow polylines around the caster spine, using
         * Effect/GenericGlow.dds for ten seconds.
         */
        Meditation,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        Resurrection,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        ResurrectionPrep,

        /** Particle resurrection_p at Target */
        WildAccuracy,

        /** FX effect_ef024 at TargetOrigin */
        RainforceDefence,

        /** FX effect_ef025 at CasterOrigin */
        ChantOfProtect,

        /** FX Tw_7_H_40_Floor at CasterOrigin; Activation is separate from the buff-duration FX
         * entry davi_ef120, which follows the owner until deactivation and starts/stops its script
         * sounds and model stages with the buff.
         */
        PerfectDefense,

        /** FX Tw_7_A_44 at CasterOrigin */
        ProvocationOfBlow,

        /** FX Tw_7_A_38 at CasterOrigin */
        SpinSlash,

        /** FX effect_ef033 at Target */
        ChantOfHealing,

        /** FX effect_ef027 at Caster; FX effect_ef027a at Target */
        Sacrifice,

        /** FX effect_ef028 at CasterOrigin */
        ChantOfLightning,

        /** FX effect_ef029 at CasterOrigin */
        ChantOfIce,

        /** FX effect_ef030 at CasterOrigin */
        ChantOfFlame,

        /** FX effect_ef035 at Caster */
        ChangeToVitality,

        /** FX effect_ef034 at Caster (cast loop) */
        ChangeToVitalityPrep,

        /** FX effect_ef031 at CasterOrigin */
        ChantOfAccuracy,

        /** FX effect_ef032 at CasterOrigin */
        ChantOfBattle,

        /** FX effect_ef036 at Caster */
        ShieldOfResilience,

        /** FX effect_ef038 at Caster */
        Reincarnate,

        /** FX effect_ef037 at Caster (cast loop) */
        ReincarnatePrep,

        /** FX effect_ef075 at Target; FX effect_ef075a at Target; FX effect_ef075b at Target; FX
         * effect_ef075c at Target
         */
        CallLightning,

        /** FX effect_ef074 at Caster (cast loop); FX effect_ef074_1 at Caster (cast loop) */
        CallLightningPrep,

        /** FX effect_ef081a_ice1 at TargetOrigin; FX effect_ef081a_ice2 at TargetOrigin; FX
         * effect_ef081a_ice3 at TargetOrigin; FX effect_ef081a_ice4 at TargetOrigin
         */
        IceLance,

        /** FX effect_ef080 at Caster (cast loop); FX effect_ef080_1 at Caster (cast loop) */
        IceLancePrep,

        /** FX effect_ef089a at TargetOrigin; FX effect_ef089b at TargetOrigin; FX effect_ef089c at
         * TargetOrigin
         */
        Explosion,

        /** FX effect_ef088 at Caster (cast loop); FX effect_ef088_1 at Caster (cast loop) */
        ExplosionPrep,

        /** FX effect_ef114 at Target */
        Mesmerization,

        /** FX effect_ef113 at Caster (cast loop) */
        MesmerizationPrep,

        /** FX effect_ef083 at Target */
        SnowField,

        /** FX effect_ef082 at Caster (cast loop) */
        SnowFieldPrep,

        /** FX effect_ef095 at Target; FX effect_ef095a at Target */
        EnchantWeapon,

        /** FX effect_ef094 at Caster (cast loop) */
        EnchantWeaponPrep,

        /** FX effect_ef099 at Target; FX effect_ef099a at Target */
        AuraOfShield,

        /** FX effect_ef098 at Caster (cast loop) */
        AuraOfShieldPrep,

        /** Procedural ribbon using Lightning02.GTX, Sprite Player_EF_code_EF106 at the caster and
         * lightning_impact at the target; sound efs021.
         */
        SpiritBlast,

        /** FX effect_ef115 at Caster (cast loop) */
        SpiritBlastPrep,

        /** FX effect_ef077 at Caster (cast loop) */
        ChainLightningPrep,

        /** FX effect_ef077a at Target */
        Thunder,

        /** FX effect_ef078 at Caster (cast loop) */
        ThunderStormPrep,

        /** FX effect_ef079 at Target */
        ThunderStorm,

        /** FX effect_ef085 at Caster; FX effect_ef085a at Target */
        IceRequeim,

        /** FX effect_ef084 at Caster (cast loop) */
        IceRequeimPrep,

        /** FX effect_ef086 at Caster (cast loop) */
        IceStormPrep,

        /** FX effect_ef091 at TargetOrigin */
        ExplosiveBurst,

        /** FX effect_ef090 at Caster (cast loop) */
        ExplosiveBurstPrep,

        /** FX effect_ef092 at Caster (cast loop) */
        FireRainPrep,

        /** FX effect_ef100 at Target; FX effect_ef100b at Target */
        BlessedStrength,

        /** FX effect_ef101 at Target; FX effect_ef101a at Target */
        BlessedHealth,

        /** FX effect_ef102 at Target; FX effect_ef102a at Target */
        BlessedDexterity,

        /** FX effect_ef103 at Target; FX effect_ef103b at Target */
        BlessedIntelligence,

        /** FX effect_ef104 at Target; FX effect_ef104a at Target */
        AngelicDexterity,

        /** FX effect_ef112 at Target; FX effect_ef112a at Target */
        Purge,

        /** FX effect_ef111 at Caster (cast loop) */
        PurgePrep,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        ReinforceSight,

        /** FX effect_ef118 at Target; FX effect_ef118a at Target */
        Amnesia,

        /** FX effect_ef117 at Caster (cast loop) */
        AmnesiaPrep,

        /** Particle healing at TargetOrigin; also draws a procedural target-following ring with
         * Effect/healing.dds and plays efs022 at the target origin.
         */
        LightRemedy,

        /** Particle healing at TargetOrigin; also draws a procedural target-following ring with
         * Effect/healing.dds and plays efs022 at the target origin.
         */
        LightReflection,

        /** Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the
         * client builds a fallback shaft when the mesh is absent. At impact the zero-extra
         * projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a
         * critical result) through Skill/ then Effect/ GB lookup.
         */
        ImmolationArrow,

        /** Particle resurrection_p at Target */
        CriticalChance,

        /** Particle resurrection_p at Target */
        WildStrength,

        /** FX effect_ef051 at Caster; FX effect_ef051a at Target; Adds an invisible return-flight
         * controller from target to caster at 200 units/s; target FX is deferred until the outward
         * travel time and triggers sound efs014.
         */
        VitalityDrain,

        /** FX effect_ef053a at Caster; FX effect_ef053b at Target */
        InflectSuffering,

        /** FX effect_ef052 at Caster (cast loop) */
        InflectSufferingPrep,

        /** FX effect_ef055 at Caster; FX effect_ef055a at Target */
        MeshOfSpeed,

        /** FX effect_ef059 at Caster; FX effect_ef059a at Target */
        DarknessVision,

        /** FX effect_ef058 at Caster (cast loop) */
        DarknessVisionPrep,

        /** FX effect_ef042 at Caster */
        ResistLightning,

        /** FX effect_ef043 at Caster */
        ResistIce,

        /** FX effect_ef044 at Caster */
        ResistFire,

        /** FX effect_ef046a at Target; Visible projectile uses Item/Weapon/M_arrow.gb and the
         * Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent.
         * Target dispatch is delayed by distance / 500 units/s. Uses a dark non-additive trail and
         * suppresses the generic impact clip.
         */
        SilentShot,

        /** FX effect_ef045 at Caster (cast loop) */
        SilentShotPrep,

        /** FX effect_ef047 at Caster */
        ResistCurse,

        /** FX effect_ef048 at Caster */
        ResistSpirit,

        /** FX effect_ef050 at Caster */
        EmergencyEscape,

        /** FX effect_ef054 at Caster; FX effect_ef054a at Target */
        CloudSight,

        /** Uses the red ribbon, flare and impact entries described for LightningRed; sound efs021. */
        ManaBurn,

        /** FX effect_ef056 at Caster (cast loop) */
        ManaBurnPrep,

        /** FX effect_ef060 at Caster; FX effect_ef060a at Target */
        MeshOfChance,

        /** FX effect_ef061 at Caster; FX effect_ef061a at Target */
        MeshOfForce,

        /** FX effect_ef062 at Caster; FX effect_ef062a at Target */
        MeshOfHealth,

        /** FX effect_ef063 at Caster; FX effect_ef063a at Target */
        MeshOfAgility,

        /** FX effect_ef064 at Caster; FX effect_ef064a at Target */
        ConstrictMind,

        /** FX effect_ef065 at Caster; FX effect_ef065a at Target */
        ConstrictWisdom,

        /** FX effect_ef066 at Caster; FX effect_ef066a at Target */
        VanishWall,

        /** FX effect_ef069 at Caster; FX effect_ef069a at Target */
        VanishConfidence,

        /** FX effect_ef071 at Caster; FX effect_ef071a at Target */
        DestroySight,

        /** FX effect_ef070 at Caster (cast loop) */
        DestroySightPrep,

        /** FX effect_ef073 at Caster; FX effect_ef073a at Target */
        BenefitAblation,

        /** FX effect_ef072 at Caster (cast loop) */
        BenefitAblationPrep,

        /** FX effect_ef067 at Caster (cast loop) */
        PoisonCloudPrep,

        /** Procedural red ribbon using LightningRed.GTX, Sprite lightningRed_flare at the caster
         * and lightningRed_impact at the target; sound efs021.
         */
        LightningRed,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        GunchiIceTempest,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        YungsangFatalBlow,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        YungsangDemolition,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        TugukGroundSplinter,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        TugukShadowFang,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        TugukDiamondClaw,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        Gale1,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        Gale2,

        /** No dedicated visual dispatch; currently falls back to sound efs010. */
        Gale3,

    };

    /** Lookup job used while the caster is transformed. */
    inline constexpr int kSkillEffectJobTransform = 255;

    /** One prep/fire row from the skill-effect table. */
    struct SkillEffectEntry {
        /** PC job (0 knight / 1 mage / 2 archer / 255 transform). */
        int job = 0;

        /** 0 = prep (gubun 0), 1 = fire (gubun 1). */
        int phase = 0;

        /** Skill index within the job. */
        int skill = 0;

        /** Primary effect type. */
        SkillEt effect = SkillEt::None;

        /** Optional secondary effect (multi-target). */
        SkillEt effect2 = SkillEt::None;
    };

    /** Asset category used by a skill presentation dispatch. */
    enum class SkillFxAssetKind : std::uint8_t {
        /** Effect/Fx script resolved through SpriteFx. */
        Fx,
        /** Effect/Sprite particle shader resolved through SpriteFx. */
        Particle,
    };

    /** Character endpoint used to place a skill effect. */
    enum class SkillFxAttach : std::uint8_t {
        /** Attach to the skill caster. */
        Caster,

        /** Attach to the caster character origin (feet). */
        CasterOrigin,

        /** Attach to the skill target spine / impact point. */
        Target,

        /** Attach to the target character origin (feet). */
        TargetOrigin,
    };

    /** One data-driven FX dispatch for a skill ET value. */
    struct SkillFxDispatch {
        /** Effect type owning this dispatch. */
        SkillEt effect = SkillEt::None;

        /** FX script or particle shader name. */
        std::string_view asset;

        /** Whether asset names an FX script or a particle shader. */
        SkillFxAssetKind kind = SkillFxAssetKind::Fx;

        /** Character endpoint receiving the effect. */
        SkillFxAttach attach = SkillFxAttach::Caster;

        /** True for prep effects retained until the cast phase ends. */
        bool looping = false;
    };

    /**
     * Perfect Defense / Steel Wall FX retained for the active buff lifetime.
     */
    inline constexpr std::string_view kPerfectDefenseLoopFx = "davi_ef120";

    /** Arrow Explosion FX entry attached during the three-second fuse. */
    inline constexpr std::string_view kArcherExploderFuseFx = "davi_ef131_04";

    /** Arrow Explosion particle entry spawned on target bone 1 when the fuse ends. */
    inline constexpr std::string_view kArcherExploderBurstFx = "davi_ef131_05";

    /** Delay from planting Arrow Explosion to its burst, in seconds. */
    inline constexpr float kArcherExploderFuseSec = 3.0f;

    /** Non-owning contiguous view of skill FX dispatch rows. */
    class SkillFxDispatchList {
      public:
        /** Constructs an empty dispatch list. */
        SkillFxDispatchList();

        /**
         * Constructs a dispatch list view.
         * @param data First dispatch row.
         * @param size Number of rows.
         */
        SkillFxDispatchList(const SkillFxDispatch* data, std::size_t size);

        /** @return True when the list has no dispatches. */
        [[nodiscard]] bool empty() const;

        /** @return Number of dispatch rows. */
        [[nodiscard]] std::size_t size() const;

        /**
         * Returns a dispatch row.
         * @param index Zero-based row index.
         * @return Dispatch at index.
         */
        [[nodiscard]] const SkillFxDispatch& operator[](std::size_t index) const;

        /** @return Iterator to the first dispatch. */
        [[nodiscard]] const SkillFxDispatch* begin() const;

        /** @return Iterator one past the last dispatch. */
        [[nodiscard]] const SkillFxDispatch* end() const;

      private:
        /** First dispatch row. */
        const SkillFxDispatch* m_data = nullptr;

        /** Number of dispatch rows. */
        std::size_t m_size = 0;
    };

    /**
     * Looks up the client presentation effects for a cast phase.
     * Thief (job 3) has no mapped rows — returns nullptr.
     * @param job Class / transform job id.
     * @param phase 0 prep / 1 fire.
     * @param skill Skill index.
     * @return Matching entry, or nullptr.
     */
    [[nodiscard]] const SkillEffectEntry* find_skill_effect(int job, int phase, int skill);

    /**
     * Returns script/particle dispatches for a handled Knight ET value.
     * @param et Effect type.
     * @return Contiguous dispatch list, or an empty list for non-Knight effects.
     */
    [[nodiscard]] SkillFxDispatchList knight_fx_dispatches(SkillEt et);

    /**
     * Returns script/particle dispatches for a handled Archer ET value.
     * @param et Effect type.
     * @return Contiguous dispatch list, or an empty list for non-Archer effects.
     */
    [[nodiscard]] SkillFxDispatchList archer_fx_dispatches(SkillEt et);

    /**
     * Returns script/particle dispatches for a handled Mage ET value.
     * @param et Effect type.
     * @return Contiguous dispatch list, or an empty list for non-Mage effects.
     */
    [[nodiscard]] SkillFxDispatchList mage_fx_dispatches(SkillEt et);

    /**
     * Looping storm prep FX shown at the RangeSkillCast targeting cue.
     * Thunder `effect_ef078`, Ice `effect_ef086`, Fire `effect_ef092`.
     * Explosive Burst uses the same ground cue but has no looping preview FX.
     * @param job Player job (Magician = 1).
     * @param skill_index Skill index (43 / 45 / 47 / 48).
     * @return FX script name, or empty when the skill has no mapped prep FX.
     */
    [[nodiscard]] std::string_view mage_range_preview_fx(int job, int skill_index);

    /**
     * Looping RangeSkillCast preview FX for Mage storms and Archer Arrow Rain.
     * Arrow Rain reuses Fire Storm's `effect_ef092` seal while aiming. Channel
     * rain comes from `Tm_7_a_15` clip FX `davi_ef132` / `davi_ef132_01`.
     * Poison Cloud aiming shows the ground ring only.
     * @param job Player job (Magician = 1, Archer = 2).
     * @param skill_index Skill index.
     * @return FX script name, or empty when the skill has no mapped preview FX.
     */
    [[nodiscard]] std::string_view range_skill_preview_fx(int job, int skill_index);

    /**
     * Debug / UI name for an effect type.
     * @param et Effect type.
     * @return Stable ASCII label.
     */
    [[nodiscard]] const char* skill_et_name(SkillEt et);

    /**
     * Number of procedural lightning ribbons: two for Spark, otherwise one.
     * @param et Effect type.
     * @return Number of lightning ribbons to spawn.
     */
    [[nodiscard]] constexpr int lightning_beam_count(SkillEt et) {
        return et == SkillEt::Spark ? 2 : 1;
    }

    /**
     * Selects the red lightning assets for Mana Burn and transformed lightning.
     * @param et Effect type.
     * @return True for ET_MANA_BURN and ET_LIGHTNING_RED.
     */
    [[nodiscard]] constexpr bool is_red_lightning_et(SkillEt et) {
        return et == SkillEt::LightningRed || et == SkillEt::ManaBurn;
    }

    /** Number of projectiles launched for Mysterious Arrow. */
    inline constexpr int kArcherMysticArrowCount = 3;

    /** projectile initialization parabola height for ET_MYSTIC_ARROW. */
    inline constexpr float kArcherMysticParabolaHeight = 12.0f;

    /**
     * Distinct parabolic phases for the three Mysterious Arrow projectiles.
     * projectile initialization uses `extra * 2π` as the parabola angle.
     */
    inline constexpr float kArcherMysticExtras[kArcherMysticArrowCount] = {
        0.0f, 1.0f / 3.0f, 2.0f / 3.0f};

    /**
     * Selects the fire/ice projectile model and its impact presentation.
     * @param et Effect type.
     * @return True for mage fire/ice projectile ETs.
     */
    [[nodiscard]] constexpr bool is_mage_bolt_et(SkillEt et) {
        switch (et) {
        case SkillEt::FireBolt:
        case SkillEt::Fireball:
        case SkillEt::IceBolt:
        case SkillEt::FrostBlast:
            return true;
        default:
            return false;
        }
    }

    /**
     * Expanding Ice Nova ring radii (`pow(fraction, 0.01) * 64` and `fraction * 64`).
     * @param fraction Life-normalized elapsed time (`elapsed / lifetime`).
     * @param radius1 Out inner/outer ring A (snaps near 64).
     * @param radius2 Out ring B (grows 0 → 64).
     */
    inline void ice_nova_radii(float fraction, float& radius1, float& radius2) {
        const float t = fraction < 0.0f ? 0.0f : fraction;
        radius1 = std::pow(t, 0.01f) * 64.0f;
        radius2 = t * 64.0f;
    }

    /**
     * Selects Fire Magic and Fire Blow for delayed HP-bar interpolation.
     * @param skill_index Mage skill index.
     * @return True for Fire Magic (2) and Fire Blow (25).
     */
    [[nodiscard]] constexpr bool is_fire_delay_damage_skill(int skill_index) {
        return skill_index == 2 || skill_index == 25;
    }

    /**
     * Displayed HP fill during the one-second delayed-damage interpolation.
     * Displayed HP is `cur_hp + saved_damage * (1 - elapsed)` until one second elapses.
     * @param cur_hp True HP after the hit was applied.
     * @param max_hp Maximum HP.
     * @param saved_damage Damage amount retained for the interpolation.
     * @param elapsed_s Seconds elapsed since the skill packet.
     * @return Fill ratio in 0..1.
     */
    [[nodiscard]] float
    fire_delay_damage_hp_ratio(int cur_hp, int max_hp, int saved_damage, float elapsed_s);

}  // namespace openkal::client
